// SPDX-License-Identifier: MIT
#include "lumasavecontroller.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KScreen/Config>
#include <KScreen/GetConfigOperation>
#include <KScreen/Output>
#include <KScreen/SetConfigOperation>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

static KConfigGroup effectConfig()
{
    return KConfigGroup(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-lumasave"));
}

LumaSaveController::LumaSaveController(QObject *parent) : QObject(parent)
{
    m_serviceWatcher.setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher.setWatchedServices({QStringLiteral("org.kde.LumaSave")});
    m_serviceWatcher.setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    connect(&m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &LumaSaveController::refresh);
    QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.LumaSave"), QStringLiteral("/LumaSave"),
        QStringLiteral("org.kde.LumaSave.Status"), QStringLiteral("statusChanged"), this, SLOT(refresh()));
    refresh();
}

QString LumaSaveController::mode() const
{
    QString value = effectConfig().readEntry("OperatingMode", QStringLiteral("off"));
    return value == QLatin1String("on") ? QStringLiteral("idle") : value;
}
int LumaSaveController::maximumReduction() const { return effectConfig().readEntry("MaxBacklightReductionPercent", 35); }
bool LumaSaveController::batteryOnly() const { return effectConfig().readEntry("BatteryOnly", true); }

void LumaSaveController::refresh()
{
    QDBusInterface api(QStringLiteral("org.kde.LumaSave"), QStringLiteral("/LumaSave"), QStringLiteral("org.kde.LumaSave.Status"));
    const QDBusReply<QString> reply = api.call(QStringLiteral("statusJson"));
    QVariantMap next;
    if (reply.isValid()) next = QJsonDocument::fromJson(reply.value().toUtf8()).object().toVariantMap();
    if (next.isEmpty()) next = {{QStringLiteral("mode"), mode()}, {QStringLiteral("state"), QStringLiteral("unavailable")}};
    if (next != m_status) { m_status = next; Q_EMIT statusChanged(); }
    Q_EMIT settingsChanged();
}

bool LumaSaveController::anyHdrEnabled() const
{
    KScreen::GetConfigOperation get;
    if (!get.exec()) return true;
    for (const auto &output : get.config()->outputs()) if (output->isEnabled() && output->isHdrEnabled()) return true;
    return false;
}

bool LumaSaveController::disableHdr()
{
    KScreen::GetConfigOperation get;
    if (!get.exec()) return false;
    const auto config = get.config();
    for (const auto &output : config->outputs()) if (output->isEnabled()) output->setHdrEnabled(false);
    KScreen::SetConfigOperation set(config);
    return set.exec() && !anyHdrEnabled();
}

void LumaSaveController::requestMode(const QString &requested)
{
    if (requested != QLatin1String("off") && requested != QLatin1String("always") && requested != QLatin1String("idle")) return;
    if (requested != QLatin1String("off") && anyHdrEnabled()) {
        m_pendingMode = requested;
        m_hdrConfirmationRequired = true;
        Q_EMIT hdrConfirmationRequiredChanged();
        return;
    }
    m_pendingMode = requested;
    continueActivation();
}

void LumaSaveController::confirmDisableHdr()
{
    if (!m_hdrConfirmationRequired) return;
    const QString requested = m_pendingMode;
    cancelDisableHdr();
    if (!disableHdr()) { Q_EMIT error(tr("HDR could not be disabled.")); return; }
    m_pendingMode = requested;
    continueActivation();
}

void LumaSaveController::cancelDisableHdr()
{
    m_pendingMode.clear();
    m_hdrConfirmationRequired = false;
    Q_EMIT hdrConfirmationRequiredChanged();
}

bool LumaSaveController::powerDevilDimmingEnabled() const
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("powerdevilrc"));
    for (const QString &profile : {QStringLiteral("AC"), QStringLiteral("Battery"), QStringLiteral("LowBattery")}) {
        const KConfigGroup profileGroup(config, profile);
        const KConfigGroup display(&profileGroup, QStringLiteral("Display"));
        if (display.readEntry("DimDisplayWhenIdle", true)) return true;
    }
    return false;
}

void LumaSaveController::disablePowerDevilDimming()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("powerdevilrc"));
    for (const QString &profile : {QStringLiteral("AC"), QStringLiteral("Battery"), QStringLiteral("LowBattery")}) {
        KConfigGroup profileGroup(config, profile);
        KConfigGroup display(&profileGroup, QStringLiteral("Display"));
        display.writeEntry("DimDisplayWhenIdle", false);
    }
    config->sync();
    QDBusInterface(QStringLiteral("org.kde.Solid.PowerManagement"),
                   QStringLiteral("/org/kde/Solid/PowerManagement"),
                   QStringLiteral("org.kde.Solid.PowerManagement")).call(QStringLiteral("reparseConfiguration"));
}

void LumaSaveController::continueActivation()
{
    if (!m_pendingMode.isEmpty() && m_pendingMode != QLatin1String("off") && powerDevilDimmingEnabled()) {
        m_dimmingConfirmationRequired = true;
        Q_EMIT dimmingConfirmationRequiredChanged();
        return;
    }
    const QString requested = m_pendingMode;
    m_pendingMode.clear();
    applyMode(requested);
}

void LumaSaveController::confirmDisableDimming()
{
    if (!m_dimmingConfirmationRequired) return;
    m_dimmingConfirmationRequired = false;
    Q_EMIT dimmingConfirmationRequiredChanged();
    disablePowerDevilDimming();
    continueActivation();
}

void LumaSaveController::keepDimming()
{
    if (!m_dimmingConfirmationRequired) return;
    m_dimmingConfirmationRequired = false;
    Q_EMIT dimmingConfirmationRequiredChanged();
    const QString requested = m_pendingMode;
    m_pendingMode.clear();
    applyMode(requested);
}

void LumaSaveController::applyMode(const QString &requested)
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup effect(config, QStringLiteral("Effect-lumasave"));
    const bool enabled = requested != QLatin1String("off");
    effect.writeEntry("OperatingMode", requested); effect.writeEntry("Enabled", enabled);
    KConfigGroup plugins(config, QStringLiteral("Plugins")); plugins.writeEntry("lumasaveEnabled", enabled); config->sync();
    QDBusInterface effects(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects"));
    if (enabled) effects.call(QStringLiteral("loadEffect"), QStringLiteral("lumasave"));
    effects.call(QStringLiteral("reconfigureEffect"), QStringLiteral("lumasave"));
    if (!enabled) effects.call(QStringLiteral("unloadEffect"), QStringLiteral("lumasave"));
    refresh();
}

void LumaSaveController::setMaximumReduction(int value)
{
    auto group = effectConfig(); group.writeEntry("MaxBacklightReductionPercent", std::clamp(value, 0, 75)); group.sync();
    QDBusInterface(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects")).call(QStringLiteral("reconfigureEffect"), QStringLiteral("lumasave"));
    refresh();
}
void LumaSaveController::setBatteryOnly(bool value)
{
    auto group = effectConfig(); group.writeEntry("BatteryOnly", value); group.sync();
    QDBusInterface(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects")).call(QStringLiteral("reconfigureEffect"), QStringLiteral("lumasave"));
    refresh();
}
