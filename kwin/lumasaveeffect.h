// SPDX-License-Identifier: MIT
#pragma once

#include "effect/offscreeneffect.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QDateTime>
#include <QTimer>
#include <array>
#include <memory>
#include <unordered_set>

namespace KWin
{
class GLShader;
class IdleDetector;
class BackendOutput;

class LumaSaveEffect final : public OffscreenEffect
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.LumaSave.Status")
    Q_PROPERTY(QString operatingMode READ operatingMode NOTIFY statusChanged)
    Q_PROPERTY(QString runtimeState READ runtimeState NOTIFY statusChanged)
    Q_PROPERTY(int currentReductionPercent READ currentReductionPercent NOTIFY statusChanged)
public:
    LumaSaveEffect();
    ~LumaSaveEffect() override;

    static bool supported();
    bool isActive() const override;
    int requestedEffectChainPosition() const override;
    void reconfigure(ReconfigureFlags flags) override;
    void paintScreen(const RenderTarget &target, const RenderViewport &viewport, int mask,
                     const Region &region, LogicalOutput *screen) override;
    QString operatingMode() const { return m_operatingMode; }
    QString runtimeState() const;
    int currentReductionPercent() const;

public Q_SLOTS:
    QString statusJson();

Q_SIGNALS:
    void statusChanged();

private:
    void readConfig();
    void armIdleDetector();
    void requestAnalysis();
    void analyze(const RenderTarget &target, const RenderViewport &viewport, LogicalOutput *screen);
    void activate(float reduction);
    void transitionTo(float reduction);
    void advanceTransition();
    void applyReduction(float reduction);
    float uncompensatedLuminance(float displayedLuminance) const;
    void calibrationParameters(float &perceived, float &shadow, float &highlight, float &colorIntensity) const;
    void deactivate();
    void redirectWindow(EffectWindow *window);
    void forgetWindow(EffectWindow *window);
    void setBacklightScale(float scale);
    bool onBattery() const;
    bool screenChangeInhibited() const;
    bool attachInternalOutput();
    void applyCalibrationMode();
    void configureAutomaticMode();
    void periodicAnalysis();
    void accountUsage();
    void persistStatistics();

    std::unique_ptr<IdleDetector> m_idleDetector;
    std::unique_ptr<GLShader> m_shader;
    std::unordered_set<EffectWindow *> m_windows;
    bool m_enabled = false;
    QString m_operatingMode = QStringLiteral("off");
    bool m_batteryOnly = true;
    bool m_calibrationMode = false;
    bool m_calibrationActive = false;
    int m_calibrationReductionPercent = 10;
    bool m_analysisPending = false;
    bool m_active = false;
    int m_idleSeconds = 15;
    int m_sampleIntervalSeconds = 15;
    int m_maxReductionPercent = 35;
    float m_perceivedBrightness = 1.0f;
    float m_shadowDetail = 0.5f;
    float m_highlightProtection = 0.7f;
    float m_colorIntensity = 1.0f;
    bool m_hasCalibrationProfiles = false;
    std::array<float, 3> m_profilePerceived{1.0f, 1.0f, 1.0f};
    std::array<float, 3> m_profileShadow{0.5f, 0.5f, 0.5f};
    std::array<float, 3> m_profileHighlight{0.7f, 0.7f, 0.7f};
    std::array<float, 3> m_profileColor{1.0f, 1.0f, 1.0f};
    QPointer<BackendOutput> m_output;
    float m_backlightScale = 1.0f;
    double m_userBrightness = 1.0;
    QTimer m_sampleTimer;
    QTimer m_statisticsTimer;
    QTimer m_transitionTimer;
    QElapsedTimer m_transitionClock;
    QElapsedTimer m_accountingClock;
    QDateTime m_lastAnalysis;
    double m_sessionSeconds = 0.0;
    double m_activeSeconds = 0.0;
    double m_reductionSeconds = 0.0;
    double m_persistedSessionSeconds = 0.0;
    double m_persistedActiveSeconds = 0.0;
    double m_persistedReductionSeconds = 0.0;
    float m_currentReduction = 0.0f;
    float m_transitionStartReduction = 0.0f;
    float m_transitionTargetReduction = 0.0f;
    float m_lastChosenReduction = 0.0f;
    int m_stableSamples = 0;
};
}
