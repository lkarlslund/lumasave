// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>

class CalibrationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool compensated READ compensated NOTIFY compensatedChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
public:
    explicit CalibrationController(QObject *parent = nullptr);
    ~CalibrationController() override;
    bool compensated() const { return m_compensated; }
    bool available() const { return !m_effectName.isEmpty(); }

    Q_INVOKABLE void setCompensated(bool enabled, double reduction,
                                    int perceivedBrightness, int shadowDetail,
                                    int highlightProtection, int colorIntensity);
    Q_INVOKABLE void restore();
    Q_INVOKABLE void saveAndQuit();
    Q_INVOKABLE void cancelAndQuit();
Q_SIGNALS:
    void compensatedChanged();
private:
    void writeSetting(const QString &key, const QVariant &value);
    void reconfigure();
    QString m_effectName;
    bool m_compensated = false;
};
