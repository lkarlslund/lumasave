// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>

class CalibrationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int previewMode READ previewMode NOTIFY previewModeChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
public:
    explicit CalibrationController(QObject *parent = nullptr);
    ~CalibrationController() override;
    int previewMode() const { return m_previewMode; }
    bool available() const { return !m_effectName.isEmpty() && m_maximumBrightness > 0; }

    Q_INVOKABLE void setCompensated(bool enabled, double reduction,
                                    int perceivedBrightness, int shadowDetail,
                                    int highlightProtection, int colorIntensity);
    Q_INVOKABLE void setPreviewMode(int mode, double reduction,
                                    int perceivedBrightness, int shadowDetail,
                                    int highlightProtection, int colorIntensity);
    Q_INVOKABLE void setCalibrationLevel(int percent);
    Q_INVOKABLE void restore();
    Q_INVOKABLE void saveAndQuit();
    Q_INVOKABLE void cancelAndQuit();
Q_SIGNALS:
    void previewModeChanged();
private:
    void writeSetting(const QString &key, const QVariant &value);
    void reconfigure();
    int readBrightnessProperty(const char *name) const;
    void setBrightness(int value);
    QString m_effectName;
    int m_originalBrightness = -1;
    int m_maximumBrightness = -1;
    int m_levelBrightness = -1;
    bool m_restored = false;
    int m_previewMode = 0;
};
