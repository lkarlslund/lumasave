// SPDX-License-Identifier: MIT
#pragma once

#include <KCModule>

class QCheckBox;
class QComboBox;
class QSpinBox;

class LumaSaveKcm final : public KCModule
{
    Q_OBJECT
public:
    LumaSaveKcm(QObject *parent, const KPluginMetaData &data);
    void load() override;
    void save() override;
    void defaults() override;
private:
    void launchCalibration();
    void clearCalibration();
    bool powerDevilDimmingEnabled() const;
    void offerToDisablePowerDevilDimming();
    void settingsChanged();
    QComboBox *m_mode;
    QCheckBox *m_batteryOnly;
    QSpinBox *m_idleSeconds;
    QSpinBox *m_maxReduction;
    QSpinBox *m_sampleInterval;
};
