// SPDX-License-Identifier: MIT
#include "calibrationcontroller.h"

#include <QDBusInterface>
#include <QCoreApplication>
#include <QDir>
#include <QProcessEnvironment>
#include <QSettings>
#include <algorithm>
#include <cmath>

CalibrationController::CalibrationController(QObject *parent)
    : QObject(parent)
    , m_effectName(QProcessEnvironment::systemEnvironment().value(QStringLiteral("LUMASAVE_CALIBRATION_EFFECT")))
    , m_previousOperatingMode(QProcessEnvironment::systemEnvironment().value(QStringLiteral("LUMASAVE_PREVIOUS_MODE")))
    , m_originalBrightness(readBrightnessProperty("Brightness"))
    , m_maximumBrightness(readBrightnessProperty("MaxBrightness"))
{
    writeSetting(QStringLiteral("OperatingMode"), QStringLiteral("calibrating"));
    writeSetting(QStringLiteral("CalibrationActive"), false);
    reconfigure();
}

int CalibrationController::readBrightnessProperty(const char *name) const
{
    QDBusInterface display(QStringLiteral("org.kde.org_kde_powerdevil"),
                           QStringLiteral("/org/kde/ScreenBrightness/display0"),
                           QStringLiteral("org.kde.ScreenBrightness.Display"));
    const QVariant value = display.property(name);
    return value.isValid() ? value.toInt() : -1;
}

void CalibrationController::setBrightness(int value)
{
    if (m_maximumBrightness <= 0) return;
    QDBusInterface display(QStringLiteral("org.kde.org_kde_powerdevil"),
                           QStringLiteral("/org/kde/ScreenBrightness/display0"),
                           QStringLiteral("org.kde.ScreenBrightness.Display"));
    display.call(QStringLiteral("SetBrightnessWithContext"),
                 std::clamp(value, 0, m_maximumBrightness), uint(1),
                 QStringLiteral("lumasave-calibration"));
}

CalibrationController::~CalibrationController()
{
    restore();
}

void CalibrationController::writeSetting(const QString &key, const QVariant &value)
{
    QSettings settings(QDir::homePath() + QStringLiteral("/.config/kwinrc"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Effect-lumasave"));
    settings.setValue(key, value);
    settings.sync();
}

void CalibrationController::reconfigure()
{
    if (m_effectName.isEmpty()) return;
    // The dialog owns brightness and shader state for its entire lifetime.
    // Reassert the mode before every preview update in case another settings
    // writer touched kwinrc while calibration was open.
    if (!m_restored) writeSetting(QStringLiteral("OperatingMode"), QStringLiteral("calibrating"));
    QDBusInterface effects(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects"));
    effects.call(QStringLiteral("reconfigureEffect"), m_effectName);
}

void CalibrationController::setCompensated(bool enabled, double reduction,
                                           int perceivedBrightness, int shadowDetail,
                                           int highlightProtection, int colorIntensity)
{
    setPreviewMode(enabled ? 2 : 0, reduction, perceivedBrightness, shadowDetail,
                   highlightProtection, colorIntensity);
}

void CalibrationController::setPreviewMode(int mode, double reduction,
                                           int perceivedBrightness, int shadowDetail,
                                           int highlightProtection, int colorIntensity)
{
    if (!available()) return;
    mode = std::clamp(mode, 0, 2);
    reduction = std::clamp(reduction, 0.0, 0.60);
    // Always return to the selected logical baseline first. This prevents
    // transitions through mode B from becoming the baseline for mode C.
    writeSetting(QStringLiteral("CalibrationActive"), false);
    reconfigure();
    if (m_levelBrightness >= 0) setBrightness(m_levelBrightness);
    if (mode == 1 && m_levelBrightness >= 0) {
        setBrightness(int(std::lround(m_levelBrightness * (1.0 - reduction))));
    }
    writeSetting(QStringLiteral("CalibrationActive"), mode == 2);
    writeSetting(QStringLiteral("CalibrationReductionPercent"), int(std::lround(reduction * 100.0)));
    writeSetting(QStringLiteral("CalibrationPerceivedBrightnessPercent"), std::clamp(perceivedBrightness, 80, 120));
    writeSetting(QStringLiteral("CalibrationShadowDetailPercent"), std::clamp(shadowDetail, 0, 100));
    writeSetting(QStringLiteral("CalibrationHighlightProtectionPercent"), std::clamp(highlightProtection, 0, 100));
    writeSetting(QStringLiteral("CalibrationColorIntensityPercent"), std::clamp(colorIntensity, 80, 120));
    reconfigure();
    if (m_previewMode != mode) {
        m_previewMode = mode;
        Q_EMIT previewModeChanged();
    }
}

void CalibrationController::setCalibrationLevel(int percent)
{
    if (m_maximumBrightness <= 0) return;
    writeSetting(QStringLiteral("CalibrationActive"), false);
    reconfigure();
    m_previewMode = 0;
    Q_EMIT previewModeChanged();
    m_levelBrightness = int(std::lround(m_maximumBrightness * std::clamp(percent, 1, 100) / 100.0));
    setBrightness(m_levelBrightness);
}

void CalibrationController::restore()
{
    if (m_restored) return;
    m_restored = true;
    if (available()) {
        writeSetting(QStringLiteral("CalibrationActive"), false);
        QString restoreMode = m_previousOperatingMode;
        if (restoreMode == QLatin1String("on")) restoreMode = QStringLiteral("idle");
        if (restoreMode == QLatin1String("always") || restoreMode == QLatin1String("idle")
            || restoreMode == QLatin1String("off")) {
            writeSetting(QStringLiteral("OperatingMode"), restoreMode);
        }
        reconfigure();
    }
    if (m_originalBrightness >= 0) setBrightness(m_originalBrightness);
    if (m_previewMode != 0) {
        m_previewMode = 0;
        Q_EMIT previewModeChanged();
    }
}

void CalibrationController::saveAndQuit()
{
    restore();
    QCoreApplication::exit(0);
}

void CalibrationController::cancelAndQuit()
{
    restore();
    QCoreApplication::exit(2);
}
