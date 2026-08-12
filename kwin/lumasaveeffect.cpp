// SPDX-License-Identifier: MIT
#include "lumasaveeffect.h"
#include "lumasave_core.h"

#include "core/rendertarget.h"
#include "core/backendoutput.h"
#include "brightnessdevice_compat.h"
#include "effect/effecthandler.h"
#include "idledetector.h"
#include "main.h"
#include "core/outputbackend.h"
#include "core/colorspace.h"
#include "opengl/glframebuffer.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/gltexture.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <array>

namespace KWin
{
static constexpr QSize sampleSize(64, 40);
// Plasma panels normally occupy an output edge. Excluding this narrow band
// prevents LumaSave's own status widget (and other panel indicators) from
// feeding back into the content decision.
static constexpr int sampleBorder = 3;
static constexpr int transitionDurationMs = 900;

LumaSaveEffect::LumaSaveEffect()
{
    m_shader = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture, QString(), QStringLiteral(":/lumasave/shaders/lumasave.frag"));
    connect(effects, &EffectsHandler::windowAdded, this, &LumaSaveEffect::redirectWindow);
    connect(effects, &EffectsHandler::windowDeleted, this, &LumaSaveEffect::forgetWindow);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.LumaSave"));
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/LumaSave"), this,
        QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    m_accountingClock.start();
    m_statisticsTimer.setInterval(60000);
    connect(&m_statisticsTimer, &QTimer::timeout, this, [this] {
        persistStatistics();
        Q_EMIT statusChanged();
    });
    m_statisticsTimer.start();
    connect(&m_sampleTimer, &QTimer::timeout, this, &LumaSaveEffect::periodicAnalysis);
    m_transitionTimer.setInterval(16);
    connect(&m_transitionTimer, &QTimer::timeout, this, &LumaSaveEffect::advanceTransition);
    for (BackendOutput *output : kwinApp()->outputBackend()->outputs()) {
        connect(output, &BackendOutput::colorDescriptionChanged, this, &LumaSaveEffect::enforceOutputCompatibility);
    }
    connect(kwinApp()->outputBackend(), &OutputBackend::outputAdded, this, [this](BackendOutput *output) {
        connect(output, &BackendOutput::colorDescriptionChanged, this, &LumaSaveEffect::enforceOutputCompatibility);
        enforceOutputCompatibility();
    });
    readConfig();
    enforceOutputCompatibility();
    if (m_calibrationMode) {
        QTimer::singleShot(0, this, &LumaSaveEffect::applyCalibrationMode);
    } else {
        configureAutomaticMode();
    }
}

LumaSaveEffect::~LumaSaveEffect()
{
    persistStatistics();
    deactivate();
}

bool LumaSaveEffect::supported()
{
    return effects->isOpenGLCompositing();
}

bool LumaSaveEffect::isActive() const
{
    return m_active || m_analysisPending;
}

int LumaSaveEffect::requestedEffectChainPosition() const
{
    return 98;
}

void LumaSaveEffect::readConfig()
{
    const KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-lumasave"));
    m_enabled = group.readEntry("Enabled", false);
    m_operatingMode = group.readEntry("OperatingMode", m_enabled ? QStringLiteral("on") : QStringLiteral("off"));
    if (m_operatingMode == QLatin1String("on")) m_operatingMode = QStringLiteral("idle");
    m_idleSeconds = std::clamp(group.readEntry("IdleSeconds", 15), 3, 300);
    m_sampleIntervalSeconds = std::clamp(group.readEntry("SampleIntervalSeconds", 15), 5, 300);
    m_maxReductionPercent = std::clamp(group.readEntry("MaxBacklightReductionPercent", 35), 0, 75);
    m_batteryOnly = group.readEntry("BatteryOnly", true);
    m_perceivedBrightness = std::clamp(group.readEntry("PerceivedBrightnessPercent", 100) / 100.0f, 0.8f, 1.2f);
    m_shadowDetail = std::clamp(group.readEntry("ShadowDetailPercent", 50) / 100.0f, 0.0f, 1.0f);
    m_highlightProtection = std::clamp(group.readEntry("HighlightProtectionPercent", 70) / 100.0f, 0.0f, 1.0f);
    m_colorIntensity = std::clamp(group.readEntry("ColorIntensityPercent", 100) / 100.0f, 0.8f, 1.2f);
    m_hasCalibrationProfiles = group.readEntry("HasCalibrationProfiles", false);
    constexpr std::array<int, 3> levels{25, 50, 100};
    for (std::size_t i = 0; i < levels.size(); ++i) {
        const QString prefix = QStringLiteral("Profile%1").arg(levels[i]);
        m_profilePerceived[i] = std::clamp(group.readEntry(prefix + QStringLiteral("PerceivedBrightnessPercent"), 100) / 100.0f, 0.8f, 1.2f);
        m_profileShadow[i] = std::clamp(group.readEntry(prefix + QStringLiteral("ShadowDetailPercent"), 50) / 100.0f, 0.0f, 1.0f);
        m_profileHighlight[i] = std::clamp(group.readEntry(prefix + QStringLiteral("HighlightProtectionPercent"), 70) / 100.0f, 0.0f, 1.0f);
        m_profileColor[i] = std::clamp(group.readEntry(prefix + QStringLiteral("ColorIntensityPercent"), 100) / 100.0f, 0.8f, 1.2f);
    }
    m_calibrationMode = m_operatingMode == QLatin1String("calibrating");
    m_calibrationActive = group.readEntry("CalibrationActive", false);
    m_calibrationReductionPercent = std::clamp(group.readEntry("CalibrationReductionPercent", 10), 0, 60);
    if (m_calibrationMode) {
        m_perceivedBrightness = std::clamp(group.readEntry("CalibrationPerceivedBrightnessPercent", 100) / 100.0f, 0.8f, 1.2f);
        m_shadowDetail = std::clamp(group.readEntry("CalibrationShadowDetailPercent", 50) / 100.0f, 0.0f, 1.0f);
        m_highlightProtection = std::clamp(group.readEntry("CalibrationHighlightProtectionPercent", 70) / 100.0f, 0.0f, 1.0f);
        m_colorIntensity = std::clamp(group.readEntry("CalibrationColorIntensityPercent", 100) / 100.0f, 0.8f, 1.2f);
    }
}

void LumaSaveEffect::reconfigure(ReconfigureFlags)
{
    const int previousMaximum = m_maxReductionPercent;
    readConfig();
    if (m_calibrationMode) {
        m_sampleTimer.stop();
        m_idleDetector.reset();
        deactivate();
        applyCalibrationMode();
        return;
    }
    configureAutomaticMode();
    if (!m_enabled || m_operatingMode == QLatin1String("off") || m_maxReductionPercent == 0) {
        deactivate();
    } else if (m_active && previousMaximum != m_maxReductionPercent) {
        deactivate();
        requestAnalysis();
    }
}

void LumaSaveEffect::configureAutomaticMode()
{
    m_idleDetector.reset();
    m_sampleTimer.stop();
    if (!m_enabled) return;
    if (m_operatingMode == QLatin1String("idle")) {
        armIdleDetector();
    } else if (m_operatingMode == QLatin1String("always")) {
        m_sampleTimer.setInterval(m_sampleIntervalSeconds * 1000);
        m_sampleTimer.start();
        QTimer::singleShot(0, this, &LumaSaveEffect::periodicAnalysis);
    }
}

void LumaSaveEffect::periodicAnalysis()
{
    if (m_operatingMode != QLatin1String("always") || m_calibrationMode) return;
    requestAnalysis();
}

void LumaSaveEffect::armIdleDetector()
{
    m_idleDetector = std::make_unique<IdleDetector>(std::chrono::seconds(m_idleSeconds),
        IdleDetector::OperatingMode::FollowsInhibitors, this);
    connect(m_idleDetector.get(), &IdleDetector::idle, this, &LumaSaveEffect::requestAnalysis);
    connect(m_idleDetector.get(), &IdleDetector::resumed, this, &LumaSaveEffect::deactivate);
}

void LumaSaveEffect::requestAnalysis()
{
    enforceOutputCompatibility();
    if (!m_blockedReason.isEmpty()) return;
    const bool always = m_operatingMode == QLatin1String("always");
    const bool idle = m_operatingMode == QLatin1String("idle");
    if (m_batteryOnly && !onBattery()) {
        if (m_active) deactivate();
        return;
    }
    if (screenChangeInhibited()) {
        if (m_active) deactivate();
        return;
    }
    if (m_calibrationMode || (!always && !idle) || !m_enabled || m_maxReductionPercent == 0 || (!always && m_active)
        || effects->isEffectActive(QStringLiteral("screenshot"))) {
        qInfo() << "LumaSave idle ignored" << m_enabled << m_maxReductionPercent << m_active;
        return;
    }
    qInfo() << "LumaSave idle reached; requesting one-shot analysis";
    m_analysisPending = true;
    effects->addRepaintFull();
}

void LumaSaveEffect::paintScreen(const RenderTarget &target, const RenderViewport &viewport,
                                 int mask, const Region &region, LogicalOutput *screen)
{
    effects->paintScreen(target, viewport, mask, region, screen);
    if (m_analysisPending && screen && target.framebuffer()) {
        qInfo() << "LumaSave analyzing idle frame";
        m_analysisPending = false;
        analyze(target, viewport, screen);
    }
}

void LumaSaveEffect::analyze(const RenderTarget &target, const RenderViewport &viewport, LogicalOutput *screen)
{
    auto *output = qobject_cast<BackendOutput *>(screen);
    if (!output) {
        for (BackendOutput *backend : kwinApp()->outputBackend()->outputs()) {
            if (backend->isInternal()) {
                output = backend;
                break;
            }
        }
    }
    if (!output || !output->isInternal() || !output->brightnessDevice()) {
        qInfo() << "LumaSave rejected output" << output
                << (output ? output->isInternal() : false)
                << (output ? output->brightnessDevice() : nullptr);
        return;
    }
    if (outputIsHdr(output)) {
        m_blockedReason = QStringLiteral("hdr");
        deactivate();
        Q_EMIT statusChanged();
        return;
    }
    m_blockedReason.clear();
    auto texture = GLTexture::allocate(GL_RGBA8, sampleSize);
    if (!texture) {
        qInfo() << "LumaSave sample texture allocation failed";
        return;
    }
    GLFramebuffer framebuffer(texture.get());
    if (!framebuffer.valid() || !framebuffer.blitFromRenderTarget(target, viewport,
            target.transformedRect(), Rect(QPoint(), sampleSize))) {
        qInfo() << "LumaSave frame downsample failed";
        return;
    }
    const QImage image = texture->toImage().convertToFormat(QImage::Format_RGBA8888);
    if (image.isNull()) {
        qInfo() << "LumaSave sample readback failed";
        return;
    }
    m_output = output;
    m_userBrightness = output->brightnessSetting();
    disconnect(output, &BackendOutput::brightnessChanged, this, nullptr);
    connect(output, &BackendOutput::brightnessChanged, this, [this] {
        // Physical observations trigger the same signal. React only if the
        // logical user setting actually changed, avoiding a feedback loop.
        if (m_active && m_output
            && !qFuzzyCompare(m_userBrightness, m_output->brightnessSetting())) {
            m_userBrightness = m_output->brightnessSetting();
            setBacklightScale(m_backlightScale);
        }
    });
    std::array<std::uint64_t, 64> histogram{};
    for (int y = sampleBorder; y < image.height() - sampleBorder; ++y) {
        for (int x = sampleBorder; x < image.width() - sampleBorder; ++x) {
            const QRgb pixel = image.pixel(x, y);
            auto linear = [](double value) {
                value /= 255.0;
                return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
            };
            double luminance = 0.2126 * linear(qRed(pixel)) + 0.7152 * linear(qGreen(pixel)) + 0.0722 * linear(qBlue(pixel));
            // The composited target contains our correction while active.
            // Numerically invert the exact monotonic shader luminance curve,
            // so policy decisions always use the original scene luminance
            // and can never feed back on LumaSave's own compensation.
            if (m_active) luminance = uncompensatedLuminance(luminance);
            const std::size_t bin = std::min<std::size_t>(luminance * histogram.size(), histogram.size() - 1);
            histogram[bin]++;
        }
    }
    const float scale = lumasave_decide_backlight_scale(
        histogram.data(), histogram.size(), m_maxReductionPercent / 100.0f,
        0.01f, 0.035f, 0.12f);
    const float reduction = 1.0f - scale;
    m_lastAnalysis = QDateTime::currentDateTime();
    qInfo() << "LumaSave Rust policy selected reduction" << reduction;
    if (m_operatingMode == QLatin1String("always") && std::abs(reduction - m_lastChosenReduction) < 0.02f) {
        ++m_stableSamples;
        m_sampleTimer.setInterval(std::min(60, m_sampleIntervalSeconds * (1 + m_stableSamples / 2)) * 1000);
        Q_EMIT statusChanged();
        return;
    }
    m_stableSamples = 0;
    m_sampleTimer.setInterval(m_sampleIntervalSeconds * 1000);
    m_lastChosenReduction = reduction;
    if (reduction >= 0.01f) {
        if (!m_output && !attachInternalOutput()) return;
        if (m_active) transitionTo(reduction);
        else activate(reduction);
    } else {
        if (m_active) transitionTo(0.0f);
    }
    Q_EMIT statusChanged();
}

bool LumaSaveEffect::attachInternalOutput()
{
    BackendOutput *output = nullptr;
    for (BackendOutput *candidate : kwinApp()->outputBackend()->outputs()) {
        if (candidate->isInternal() && candidate->brightnessDevice()) {
            output = candidate;
            break;
        }
    }
    if (!output) return false;
    m_output = output;
    m_userBrightness = output->brightnessSetting();
    disconnect(output, &BackendOutput::brightnessChanged, this, nullptr);
    connect(output, &BackendOutput::brightnessChanged, this, [this] {
        if (m_active && m_output
            && !qFuzzyCompare(m_userBrightness, m_output->brightnessSetting())) {
            m_userBrightness = m_output->brightnessSetting();
            setBacklightScale(m_backlightScale);
        }
    });
    return true;
}

void LumaSaveEffect::applyCalibrationMode()
{
    enforceOutputCompatibility();
    if (!m_blockedReason.isEmpty()) return;
    if (m_calibrationMode && m_calibrationActive && m_calibrationReductionPercent > 0
        && attachInternalOutput()) {
        activate(m_calibrationReductionPercent / 100.0f);
    }
}

void LumaSaveEffect::activate(float reduction)
{
    if (!m_shader) {
        return;
    }
    accountUsage();
    m_active = true;
    m_currentReduction = 0.0f;
    for (EffectWindow *window : effects->stackingOrder()) redirectWindow(window);
    applyReduction(0.0f);
    qInfo() << "LumaSave activating" << reduction << "user baseline" << m_output->brightnessSetting();
    transitionTo(reduction);
}

void LumaSaveEffect::calibrationParameters(float &perceived, float &shadow, float &highlight, float &colorIntensity) const
{
    perceived = m_perceivedBrightness;
    shadow = m_shadowDetail;
    highlight = m_highlightProtection;
    colorIntensity = m_colorIntensity;
    if (m_hasCalibrationProfiles && !m_calibrationMode) {
        const float brightness = std::clamp(float(m_userBrightness), 0.0f, 1.0f);
        const int lower = brightness <= 0.5f ? 0 : 1;
        const int upper = lower + 1;
        const float lowPoint = lower == 0 ? 0.25f : 0.5f;
        const float highPoint = lower == 0 ? 0.5f : 1.0f;
        const float amount = std::clamp((brightness - lowPoint) / (highPoint - lowPoint), 0.0f, 1.0f);
        auto interpolate = [lower, upper, amount](const std::array<float, 3> &values) {
            return std::lerp(values[lower], values[upper], amount);
        };
        perceived = interpolate(m_profilePerceived);
        shadow = interpolate(m_profileShadow);
        highlight = interpolate(m_profileHighlight);
        colorIntensity = interpolate(m_profileColor);
    }
}

void LumaSaveEffect::applyReduction(float reduction)
{
    if (!m_shader) return;
    accountUsage();
    m_currentReduction = std::clamp(reduction, 0.0f, 0.75f);
    const float scale = 1.0f - m_currentReduction;
    float perceived, shadow, highlight, colorIntensity;
    calibrationParameters(perceived, shadow, highlight, colorIntensity);
    {
        ShaderBinder binder(m_shader.get());
        m_shader->setUniform("backlightScale", scale);
        m_shader->setUniform("blackThreshold", 0.01f);
        m_shader->setUniform("perceivedBrightness", perceived);
        m_shader->setUniform("shadowDetail", shadow);
        m_shader->setUniform("highlightProtection", highlight);
        m_shader->setUniform("colorIntensity", colorIntensity);
    }
    m_backlightScale = scale;
    setBacklightScale(scale);
    effects->addRepaintFull();
}

void LumaSaveEffect::transitionTo(float reduction)
{
    m_transitionStartReduction = m_currentReduction;
    m_transitionTargetReduction = std::clamp(reduction, 0.0f, 0.75f);
    m_transitionClock.restart();
    m_transitionTimer.start();
    Q_EMIT statusChanged();
}

void LumaSaveEffect::advanceTransition()
{
    const float position = std::clamp(float(m_transitionClock.elapsed()) / transitionDurationMs, 0.0f, 1.0f);
    const float eased = 1.0f - std::pow(1.0f - position, 3.0f);
    applyReduction(std::lerp(m_transitionStartReduction, m_transitionTargetReduction, eased));
    if (position >= 1.0f) {
        m_transitionTimer.stop();
        if (m_transitionTargetReduction < 0.01f) deactivate();
        else Q_EMIT statusChanged();
    }
}

float LumaSaveEffect::uncompensatedLuminance(float displayedLuminance) const
{
    float perceived, shadow, highlight, colorIntensity;
    calibrationParameters(perceived, shadow, highlight, colorIntensity);
    Q_UNUSED(colorIntensity); // Saturation adjustment preserves luminance.
    const float scale = std::clamp(m_backlightScale, 0.25f, 1.0f);
    const float calibratedBlack = 0.01f * std::lerp(3.0f, 0.2f, shadow);
    auto smoothstep = [](float edge0, float edge1, float value) {
        const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    auto forward = [&](float y) {
        if (y <= calibratedBlack || scale >= 0.9999f) return y;
        const float shoulder = scale / (1.0f - scale);
        const float mapped = std::clamp(y * (1.0f + shoulder) / (y + shoulder), 0.0f, 1.0f);
        const float blend = smoothstep(calibratedBlack, calibratedBlack * 2.0f, y);
        float target = std::lerp(y, mapped, blend) * perceived;
        target = std::lerp(target, y / scale, highlight * smoothstep(0.5f, 1.0f, y));
        return std::clamp(target, 0.0f, 1.0f);
    };
    float low = 0.0f;
    float high = 1.0f;
    for (int i = 0; i < 14; ++i) {
        const float middle = (low + high) * 0.5f;
        if (forward(middle) < displayedLuminance) low = middle;
        else high = middle;
    }
    return (low + high) * 0.5f;
}

void LumaSaveEffect::deactivate()
{
    m_analysisPending = false;
    m_transitionTimer.stop();
    if (!m_active) {
        return;
    }
    accountUsage();
    m_active = false;
    m_currentReduction = 0.0f;
    for (EffectWindow *window : m_windows) {
        unredirect(window);
    }
    m_windows.clear();
    effects->addRepaintFull();
    qInfo() << "LumaSave deactivating at user baseline"
            << (m_output ? m_output->brightnessSetting() : -1.0);
    if (m_output && m_output->brightnessDevice()) {
        disconnect(m_output, &BackendOutput::brightnessChanged, this, nullptr);
        m_output->brightnessDevice()->setBrightness(m_output->brightnessSetting());
    }
    m_output.clear();
    m_backlightScale = 1.0f;
    m_userBrightness = 1.0;
    Q_EMIT statusChanged();
}

void LumaSaveEffect::accountUsage()
{
    if (!m_accountingClock.isValid()) { m_accountingClock.start(); return; }
    const double seconds = m_accountingClock.restart() / 1000.0;
    m_sessionSeconds += seconds;
    if (m_active && !m_calibrationMode) {
        m_activeSeconds += seconds;
        m_reductionSeconds += seconds * lumasave_full_scale_backlight_saving(
            float(m_userBrightness), m_currentReduction);
    }
}

void LumaSaveEffect::persistStatistics()
{
    accountUsage();
    // V2 stores full-scale exposure saved. V1 stored reduction relative to
    // the requested brightness and cannot be converted without historical
    // brightness data, so it is deliberately kept separate and ignored.
    QSettings stats(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("LumaSave"), QStringLiteral("StatisticsV2"));
    const QString day = QDate::currentDate().toString(Qt::ISODate);
    stats.beginGroup(day);
    stats.setValue(QStringLiteral("SessionSeconds"), stats.value(QStringLiteral("SessionSeconds")).toDouble() + m_sessionSeconds - m_persistedSessionSeconds);
    stats.setValue(QStringLiteral("ActiveSeconds"), stats.value(QStringLiteral("ActiveSeconds")).toDouble() + m_activeSeconds - m_persistedActiveSeconds);
    stats.setValue(QStringLiteral("ReductionSeconds"), stats.value(QStringLiteral("ReductionSeconds")).toDouble() + m_reductionSeconds - m_persistedReductionSeconds);
    stats.endGroup();
    stats.sync();
    m_persistedSessionSeconds = m_sessionSeconds;
    m_persistedActiveSeconds = m_activeSeconds;
    m_persistedReductionSeconds = m_reductionSeconds;
}

QString LumaSaveEffect::runtimeState() const
{
    if (!m_blockedReason.isEmpty()) return QStringLiteral("blocked");
    if (m_calibrationMode) return QStringLiteral("calibrating");
    if (m_analysisPending) return QStringLiteral("analyzing");
    if (m_active) return QStringLiteral("saving");
    if (!m_enabled || m_operatingMode == QLatin1String("off")) return QStringLiteral("off");
    return QStringLiteral("waiting");
}

int LumaSaveEffect::currentReductionPercent() const { return qRound(m_currentReduction * 100.0f); }

QString LumaSaveEffect::statusJson()
{
    accountUsage();
    double userBrightness = m_userBrightness;
    if (!m_active) {
        for (BackendOutput *output : kwinApp()->outputBackend()->outputs()) {
            if (output->isInternal() && output->brightnessDevice()) {
                userBrightness = output->brightnessSetting();
                break;
            }
        }
    }
    const double average = m_sessionSeconds > 0.0 ? 100.0 * m_reductionSeconds / m_sessionSeconds : 0.0;
    QSettings stats(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("LumaSave"), QStringLiteral("StatisticsV2"));
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    double todaySession = 0.0;
    double todayActive = 0.0;
    double todayReduction = 0.0;
    double allSession = 0.0;
    double allActive = 0.0;
    double allReduction = 0.0;
    for (const QString &day : stats.childGroups()) {
        stats.beginGroup(day);
        const double session = stats.value(QStringLiteral("SessionSeconds")).toDouble();
        const double active = stats.value(QStringLiteral("ActiveSeconds")).toDouble();
        const double reduction = stats.value(QStringLiteral("ReductionSeconds")).toDouble();
        stats.endGroup();
        allSession += session;
        allActive += active;
        allReduction += reduction;
        if (day == today) {
            todaySession = session;
            todayActive = active;
            todayReduction = reduction;
        }
    }
    const double pendingSession = m_sessionSeconds - m_persistedSessionSeconds;
    const double pendingActive = m_activeSeconds - m_persistedActiveSeconds;
    const double pendingReduction = m_reductionSeconds - m_persistedReductionSeconds;
    todaySession += pendingSession;
    todayActive += pendingActive;
    todayReduction += pendingReduction;
    allSession += pendingSession;
    allActive += pendingActive;
    allReduction += pendingReduction;
    const double fullScaleReduction = userBrightness * m_currentReduction;
    QJsonObject status{{QStringLiteral("schemaVersion"), 2},
                       {QStringLiteral("mode"), m_operatingMode},
                       {QStringLiteral("state"), runtimeState()},
                       {QStringLiteral("blockedReason"), m_blockedReason},
                       {QStringLiteral("reductionPercent"), currentReductionPercent()},
                       {QStringLiteral("relativeReductionPercent"), 100.0 * m_currentReduction},
                       {QStringLiteral("fullScaleReductionPercent"), 100.0 * fullScaleReduction},
                       {QStringLiteral("userBrightnessPercent"), qRound(userBrightness * 100.0)},
                       {QStringLiteral("effectiveBrightnessPercent"), qRound(userBrightness * m_backlightScale * 100.0)},
                       {QStringLiteral("sessionSeconds"), m_sessionSeconds},
                       {QStringLiteral("activeSeconds"), m_activeSeconds},
                       {QStringLiteral("averageReductionPercent"), average},
                       {QStringLiteral("equivalentFullReductionSeconds"), m_reductionSeconds},
                       {QStringLiteral("savedBacklightSeconds"), m_reductionSeconds},
                       {QStringLiteral("todaySessionSeconds"), todaySession},
                       {QStringLiteral("todayActiveSeconds"), todayActive},
                       {QStringLiteral("todayAverageReductionPercent"), todaySession > 0.0 ? 100.0 * todayReduction / todaySession : 0.0},
                       {QStringLiteral("todayEquivalentFullReductionSeconds"), todayReduction},
                       {QStringLiteral("todaySavedBacklightSeconds"), todayReduction},
                       {QStringLiteral("allTimeSessionSeconds"), allSession},
                       {QStringLiteral("allTimeActiveSeconds"), allActive},
                       {QStringLiteral("allTimeAverageReductionPercent"), allSession > 0.0 ? 100.0 * allReduction / allSession : 0.0},
                       {QStringLiteral("allTimeEquivalentFullReductionSeconds"), allReduction},
                       {QStringLiteral("allTimeSavedBacklightSeconds"), allReduction},
                       {QStringLiteral("lastAnalysis"), m_lastAnalysis.toString(Qt::ISODate)}};
    return QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact));
}

void LumaSaveEffect::redirectWindow(EffectWindow *window)
{
    if (!m_active || !window || m_windows.contains(window)) return;
    redirect(window);
    setShader(window, m_shader.get());
    m_windows.insert(window);
}

void LumaSaveEffect::forgetWindow(EffectWindow *window)
{
    m_windows.erase(window);
}

void LumaSaveEffect::setBacklightScale(float scale)
{
    if (!m_output || !m_output->brightnessDevice()) return;
    m_output->brightnessDevice()->setBrightness(m_userBrightness * scale);
}

bool LumaSaveEffect::onBattery() const
{
    const QDir supplies(QStringLiteral("/sys/class/power_supply"));
    for (const QString &name : supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile type(supplies.filePath(name + QStringLiteral("/type")));
        QFile online(supplies.filePath(name + QStringLiteral("/online")));
        if (type.open(QIODevice::ReadOnly) && online.open(QIODevice::ReadOnly)) {
            const QByteArray kind = type.readAll().trimmed();
            if ((kind == "Mains" || kind == "USB" || kind == "USB_C")
                && online.readAll().trimmed() == "1") {
                return false;
            }
        }
    }
    return true;
}

bool LumaSaveEffect::screenChangeInhibited() const
{
    // PowerDevil's ChangeScreenSettings policy (bit 4) is asserted by video
    // players, presentations, and other clients that keep the display awake.
    QDBusInterface policy(QStringLiteral("org.kde.Solid.PowerManagement"),
                          QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                          QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"));
    const QDBusReply<bool> reply = policy.call(QStringLiteral("HasInhibition"), uint(4));
    return reply.isValid() && reply.value();
}

bool LumaSaveEffect::outputIsHdr(BackendOutput *output) const
{
    return output && output->colorDescription()
        && output->colorDescription()->transferFunction().type == TransferFunction::PerceptualQuantizer;
}

void LumaSaveEffect::enforceOutputCompatibility()
{
    for (BackendOutput *output : kwinApp()->outputBackend()->outputs()) {
        if (output->isInternal() && outputIsHdr(output)) {
            m_blockedReason = QStringLiteral("hdr");
            deactivate();
            Q_EMIT statusChanged();
            return;
        }
    }
    if (m_blockedReason == QLatin1String("hdr")) {
        m_blockedReason.clear();
        Q_EMIT statusChanged();
    }
}
}

#include "moc_lumasaveeffect.cpp"
