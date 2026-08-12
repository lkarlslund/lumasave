// SPDX-License-Identifier: MIT
#include "calibrationcontroller.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

static constexpr auto service = "org.kde.org_kde_powerdevil";
static constexpr auto path = "/org/kde/ScreenBrightness/display0";
static constexpr auto interface = "org.kde.ScreenBrightness.Display";

CalibrationController::CalibrationController(QObject *parent)
    : QObject(parent)
    , m_baseline(readProperty("Brightness"))
    , m_maximum(readProperty("MaxBrightness"))
{
}

CalibrationController::~CalibrationController()
{
    restore();
}

int CalibrationController::readProperty(const char *name) const
{
    QDBusInterface display(QString::fromLatin1(service), QString::fromLatin1(path), QString::fromLatin1(interface));
    const QVariant value = display.property(name);
    return value.isValid() ? value.toInt() : -1;
}

void CalibrationController::setBrightness(int value)
{
    QDBusInterface display(QString::fromLatin1(service), QString::fromLatin1(path), QString::fromLatin1(interface));
    // SuppressIndicator keeps calibration changes out of Plasma's OSD.
    display.call(QStringLiteral("SetBrightnessWithContext"),
                 std::clamp(value, 0, m_maximum), uint(1), QStringLiteral("lumasave-calibration"));
}

void CalibrationController::setCompensated(bool enabled, double reduction)
{
    if (!available()) return;
    reduction = std::clamp(reduction, 0.0, 0.60);
    setBrightness(enabled ? int(std::lround(m_baseline * (1.0 - reduction))) : m_baseline);
    if (m_compensated != enabled) {
        m_compensated = enabled;
        Q_EMIT compensatedChanged();
    }
}

void CalibrationController::restore()
{
    if (available()) setBrightness(m_baseline);
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
