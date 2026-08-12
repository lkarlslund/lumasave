// SPDX-License-Identifier: MIT
#include "lumasaveeffect.h"

#include "core/rendertarget.h"
#include "core/backendoutput.h"
#include "brightnessdevice_compat.h"
#include "effect/effecthandler.h"
#include "idledetector.h"
#include "main.h"
#include "core/outputbackend.h"
#include "opengl/glframebuffer.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/gltexture.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QImage>
#include <QLoggingCategory>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace KWin
{
static constexpr QSize sampleSize(64, 40);

LumaSaveEffect::LumaSaveEffect()
{
    m_shader = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture, QString(), QStringLiteral(":/lumasave/shaders/lumasave.frag"));
    connect(effects, &EffectsHandler::windowAdded, this, &LumaSaveEffect::redirectWindow);
    connect(effects, &EffectsHandler::windowDeleted, this, &LumaSaveEffect::forgetWindow);
    readConfig();
    armIdleDetector();
}

LumaSaveEffect::~LumaSaveEffect()
{
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
    m_idleSeconds = std::clamp(group.readEntry("IdleSeconds", 15), 3, 300);
    m_maxReductionPercent = std::clamp(group.readEntry("MaxBacklightReductionPercent", 35), 0, 75);
}

void LumaSaveEffect::reconfigure(ReconfigureFlags)
{
    const int previousMaximum = m_maxReductionPercent;
    readConfig();
    armIdleDetector();
    if (!m_enabled || m_maxReductionPercent == 0) {
        deactivate();
    } else if (m_active && previousMaximum != m_maxReductionPercent) {
        deactivate();
        requestAnalysis();
    }
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
    if (!m_enabled || m_maxReductionPercent == 0 || m_active) {
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
    m_output = output;
    m_userBrightness = output->brightnessSetting();
    connect(output, &BackendOutput::brightnessChanged, this, [this] {
        // A hotkey or Plasma slider changes brightnessSetting(). Keep that as
        // the user's new baseline and reapply only our invisible multiplier.
        if (m_active && m_output
            && !qFuzzyCompare(m_userBrightness, m_output->brightnessSetting())) {
            m_userBrightness = m_output->brightnessSetting();
            setBacklightScale(m_backlightScale);
        }
    });
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
    double luminance = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            auto linear = [](double value) {
                value /= 255.0;
                return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
            };
            luminance += 0.2126 * linear(qRed(pixel)) + 0.7152 * linear(qGreen(pixel)) + 0.0722 * linear(qBlue(pixel));
        }
    }
    luminance /= image.width() * image.height();
    // Dark scenes can safely use more of the user's selected maximum. White scenes use none.
    const float contentFactor = std::clamp(1.0 - std::sqrt(luminance), 0.0, 1.0);
    const float reduction = (m_maxReductionPercent / 100.0f) * contentFactor;
    qInfo() << "LumaSave sampled mean" << luminance << "reduction" << reduction;
    if (reduction >= 0.01f) {
        activate(reduction);
    }
}

void LumaSaveEffect::activate(float reduction)
{
    if (!m_shader) {
        return;
    }
    const float scale = std::clamp(1.0f - reduction, 0.25f, 1.0f);
    {
        ShaderBinder binder(m_shader.get());
        m_shader->setUniform("backlightScale", scale);
        m_shader->setUniform("blackThreshold", 0.01f);
    }
    m_active = true;
    for (EffectWindow *window : effects->stackingOrder()) {
        redirectWindow(window);
    }
    m_backlightScale = scale;
    qInfo() << "LumaSave activating" << reduction << "user baseline" << m_output->brightnessSetting();
    effects->addRepaintFull();
    QTimer::singleShot(80, this, [this, scale] { if (m_active) setBacklightScale(scale); });
}

void LumaSaveEffect::deactivate()
{
    m_analysisPending = false;
    if (!m_active) {
        return;
    }
    m_active = false;
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
}

#include "moc_lumasaveeffect.cpp"
