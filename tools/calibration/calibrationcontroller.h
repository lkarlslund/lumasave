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
    bool available() const { return m_baseline >= 0 && m_maximum > 0; }

    Q_INVOKABLE void setCompensated(bool enabled, double reduction);
    Q_INVOKABLE void restore();
    Q_INVOKABLE void saveAndQuit();
    Q_INVOKABLE void cancelAndQuit();
Q_SIGNALS:
    void compensatedChanged();
private:
    int readProperty(const char *name) const;
    void setBrightness(int value);
    int m_baseline = -1;
    int m_maximum = -1;
    bool m_compensated = false;
};
