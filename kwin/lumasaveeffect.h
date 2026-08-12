// SPDX-License-Identifier: MIT
#pragma once

#include "effect/offscreeneffect.h"

#include <QElapsedTimer>
#include <QPointer>
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
public:
    LumaSaveEffect();
    ~LumaSaveEffect() override;

    static bool supported();
    bool isActive() const override;
    int requestedEffectChainPosition() const override;
    void reconfigure(ReconfigureFlags flags) override;
    void paintScreen(const RenderTarget &target, const RenderViewport &viewport, int mask,
                     const Region &region, LogicalOutput *screen) override;

private:
    void readConfig();
    void armIdleDetector();
    void requestAnalysis();
    void analyze(const RenderTarget &target, const RenderViewport &viewport, LogicalOutput *screen);
    void activate(float reduction);
    void deactivate();
    void redirectWindow(EffectWindow *window);
    void forgetWindow(EffectWindow *window);
    void setBacklightScale(float scale);
    bool onBattery() const;
    bool attachInternalOutput();
    void applyCalibrationMode();

    std::unique_ptr<IdleDetector> m_idleDetector;
    std::unique_ptr<GLShader> m_shader;
    std::unordered_set<EffectWindow *> m_windows;
    bool m_enabled = false;
    bool m_batteryOnly = true;
    bool m_calibrationMode = false;
    bool m_calibrationActive = false;
    int m_calibrationReductionPercent = 10;
    bool m_analysisPending = false;
    bool m_active = false;
    int m_idleSeconds = 15;
    int m_maxReductionPercent = 35;
    float m_perceivedBrightness = 1.0f;
    float m_shadowDetail = 0.5f;
    float m_highlightProtection = 0.7f;
    float m_colorIntensity = 1.0f;
    QPointer<BackendOutput> m_output;
    float m_backlightScale = 1.0f;
    double m_userBrightness = 1.0;
};
}
