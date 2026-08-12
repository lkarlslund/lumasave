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
{
    writeSetting(QStringLiteral("CalibrationMode"), true);
    writeSetting(QStringLiteral("CalibrationActive"), false);
    reconfigure();
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
    QDBusInterface effects(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects"));
    effects.call(QStringLiteral("reconfigureEffect"), m_effectName);
}

void CalibrationController::setCompensated(bool enabled, double reduction,
                                           int perceivedBrightness, int shadowDetail,
                                           int highlightProtection, int colorIntensity)
{
    if (!available()) return;
    reduction = std::clamp(reduction, 0.0, 0.60);
    writeSetting(QStringLiteral("CalibrationActive"), enabled);
    writeSetting(QStringLiteral("CalibrationReductionPercent"), int(std::lround(reduction * 100.0)));
    writeSetting(QStringLiteral("CalibrationPerceivedBrightnessPercent"), std::clamp(perceivedBrightness, 80, 120));
    writeSetting(QStringLiteral("CalibrationShadowDetailPercent"), std::clamp(shadowDetail, 0, 100));
    writeSetting(QStringLiteral("CalibrationHighlightProtectionPercent"), std::clamp(highlightProtection, 0, 100));
    writeSetting(QStringLiteral("CalibrationColorIntensityPercent"), std::clamp(colorIntensity, 80, 120));
    reconfigure();
    if (m_compensated != enabled) {
        m_compensated = enabled;
        Q_EMIT compensatedChanged();
    }
}

void CalibrationController::restore()
{
    if (available()) {
        writeSetting(QStringLiteral("CalibrationActive"), false);
        reconfigure();
    }
    if (m_compensated) {
        m_compensated = false;
        Q_EMIT compensatedChanged();
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
