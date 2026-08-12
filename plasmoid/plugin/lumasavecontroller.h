// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>
#include <QDBusServiceWatcher>

class LumaSaveController final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY settingsChanged)
    Q_PROPERTY(int maximumReduction READ maximumReduction NOTIFY settingsChanged)
    Q_PROPERTY(bool batteryOnly READ batteryOnly NOTIFY settingsChanged)
    Q_PROPERTY(bool hdrConfirmationRequired READ hdrConfirmationRequired NOTIFY hdrConfirmationRequiredChanged)
    Q_PROPERTY(bool dimmingConfirmationRequired READ dimmingConfirmationRequired NOTIFY dimmingConfirmationRequiredChanged)
public:
    explicit LumaSaveController(QObject *parent = nullptr);
    QVariantMap status() const { return m_status; }
    QString mode() const;
    int maximumReduction() const;
    bool batteryOnly() const;
    bool hdrConfirmationRequired() const { return m_hdrConfirmationRequired; }
    bool dimmingConfirmationRequired() const { return m_dimmingConfirmationRequired; }

public Q_SLOTS:
    void refresh();

public:
    Q_INVOKABLE void requestMode(const QString &mode);
    Q_INVOKABLE void confirmDisableHdr();
    Q_INVOKABLE void cancelDisableHdr();
    Q_INVOKABLE void confirmDisableDimming();
    Q_INVOKABLE void keepDimming();
    Q_INVOKABLE void setMaximumReduction(int value);
    Q_INVOKABLE void setBatteryOnly(bool value);

Q_SIGNALS:
    void statusChanged();
    void settingsChanged();
    void hdrConfirmationRequiredChanged();
    void dimmingConfirmationRequiredChanged();
    void error(const QString &message);

private:
    bool anyHdrEnabled() const;
    bool disableHdr();
    bool powerDevilDimmingEnabled() const;
    void disablePowerDevilDimming();
    void continueActivation();
    void applyMode(const QString &mode);
    QVariantMap m_status;
    QDBusServiceWatcher m_serviceWatcher;
    QString m_pendingMode;
    bool m_hdrConfirmationRequired = false;
    bool m_dimmingConfirmationRequired = false;
};
