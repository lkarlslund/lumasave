//! Display-independent policy and tone mapping for content-adaptive backlights.

use serde::{Deserialize, Serialize};

pub const HISTOGRAM_BINS: usize = 64;

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
#[serde(default)]
pub struct PolicyConfig {
    /// Input inactivity required before the compositor may sample the scene.
    pub idle_seconds: u32,
    /// Do not activate while clients inhibit idle (for example video players).
    pub respect_idle_inhibitors: bool,
    /// Maximum output repaint rate considered a stable, idle image.
    pub max_idle_frames_per_second: f32,
    /// Apply only while discharging unless explicitly overridden by the UI.
    pub battery_only: bool,
    /// Linear-light luminance at or below which pixels are never boosted.
    pub black_preservation_threshold: f32,
    /// Largest fraction by which LumaSave may reduce the user's backlight.
    pub max_backlight_reduction: f32,
    /// Histogram-weighted RMS luminance error budget in linear light.
    pub max_rms_error: f32,
    /// Maximum absolute error allowed for 99% of pixels.
    pub max_p99_error: f32,
    /// Smallest useful change; prevents imperceptible oscillation.
    pub hysteresis: f32,
    /// Exponential transition time constant in milliseconds.
    pub transition_ms: u32,
    /// Disable adaptation above this amount of frame-to-frame histogram motion.
    pub motion_guard: f32,
}

impl Default for PolicyConfig {
    fn default() -> Self {
        Self {
            idle_seconds: 15,
            respect_idle_inhibitors: true,
            max_idle_frames_per_second: 2.0,
            battery_only: true,
            black_preservation_threshold: 0.01,
            max_backlight_reduction: 0.35,
            max_rms_error: 0.035,
            max_p99_error: 0.12,
            hysteresis: 0.015,
            transition_ms: 2500,
            motion_guard: 0.35,
        }
    }
}

impl PolicyConfig {
    /// User-facing maximum reduction as a percentage in the range 0..=75.
    pub fn max_backlight_reduction_percent(&self) -> f32 {
        self.max_backlight_reduction * 100.0
    }

    /// Set the user-facing maximum reduction percentage.
    pub fn set_max_backlight_reduction_percent(
        &mut self,
        percent: f32,
    ) -> Result<(), &'static str> {
        if !percent.is_finite() || !(0.0..=75.0).contains(&percent) {
            return Err("maximum backlight reduction must be between 0% and 75%");
        }
        self.max_backlight_reduction = percent / 100.0;
        Ok(())
    }

    pub fn validate(&self) -> Result<(), &'static str> {
        if !(0.0..=0.75).contains(&self.max_backlight_reduction) {
            return Err("max_backlight_reduction must be between 0 and 0.75");
        }
        if !(0.0..=0.25).contains(&self.max_rms_error) {
            return Err("max_rms_error must be between 0 and 0.25");
        }
        if !(0.0..=0.5).contains(&self.max_p99_error) {
            return Err("max_p99_error must be between 0 and 0.5");
        }
        if !(0.0..=0.2).contains(&self.hysteresis) {
            return Err("hysteresis must be between 0 and 0.2");
        }
        if !(0.0..=0.1).contains(&self.black_preservation_threshold) {
            return Err("black_preservation_threshold must be between 0 and 0.1");
        }
        if self.idle_seconds > 3600 {
            return Err("idle_seconds must not exceed 3600");
        }
        if !(0.0..=60.0).contains(&self.max_idle_frames_per_second) {
            return Err("max_idle_frames_per_second must be between 0 and 60");
        }
        Ok(())
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum RuntimeState {
    #[default]
    Awake,
    WaitingForStableFrame,
    Active,
}

#[derive(Clone, Copy, Debug, Default)]
pub struct Eligibility {
    pub idle_ms: u64,
    pub idle_inhibited: bool,
    pub frames_per_second: f32,
    pub on_battery: bool,
    pub internal_display: bool,
    pub hdr: bool,
    pub screen_capture_active: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RuntimeAction {
    None,
    /// Downsample the final frame once, then call `sample_applied`.
    AnalyzeOnce,
    /// Atomically remove compensation and restore the baseline backlight.
    Deactivate,
}

#[derive(Clone, Debug, Default)]
pub struct RuntimeController {
    state: RuntimeState,
}

impl RuntimeController {
    pub fn state(&self) -> RuntimeState {
        self.state
    }

    pub fn evaluate(&mut self, input: Eligibility, config: &PolicyConfig) -> RuntimeAction {
        let eligible = input.idle_ms >= u64::from(config.idle_seconds) * 1000
            && (!config.respect_idle_inhibitors || !input.idle_inhibited)
            && input.frames_per_second <= config.max_idle_frames_per_second
            && (!config.battery_only || input.on_battery)
            && input.internal_display
            && !input.hdr
            && !input.screen_capture_active;

        if !eligible {
            let was_active = self.state == RuntimeState::Active;
            self.state = RuntimeState::Awake;
            return if was_active {
                RuntimeAction::Deactivate
            } else {
                RuntimeAction::None
            };
        }

        match self.state {
            RuntimeState::Awake => {
                self.state = RuntimeState::WaitingForStableFrame;
                RuntimeAction::AnalyzeOnce
            }
            RuntimeState::WaitingForStableFrame | RuntimeState::Active => RuntimeAction::None,
        }
    }

    pub fn sample_applied(&mut self) {
        if self.state == RuntimeState::WaitingForStableFrame {
            self.state = RuntimeState::Active;
        }
    }

    pub fn sample_rejected(&mut self) {
        self.state = RuntimeState::Awake;
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Histogram {
    pub bins: [u64; HISTOGRAM_BINS],
    pub samples: u64,
}

impl Default for Histogram {
    fn default() -> Self {
        Self {
            bins: [0; HISTOGRAM_BINS],
            samples: 0,
        }
    }
}

impl Histogram {
    pub fn add_linear_luminance(&mut self, luminance: f32) {
        let value = luminance.clamp(0.0, 1.0);
        let index = ((value * HISTOGRAM_BINS as f32) as usize).min(HISTOGRAM_BINS - 1);
        self.bins[index] += 1;
        self.samples += 1;
    }

    pub fn from_linear_luminances(values: impl IntoIterator<Item = f32>) -> Self {
        let mut result = Self::default();
        for value in values {
            result.add_linear_luminance(value);
        }
        result
    }

    pub fn distance(&self, other: &Self) -> f32 {
        if self.samples == 0 || other.samples == 0 {
            return 1.0;
        }
        self.bins
            .iter()
            .zip(other.bins.iter())
            .map(|(&a, &b)| {
                (a as f64 / self.samples as f64 - b as f64 / other.samples as f64).abs()
            })
            .sum::<f64>() as f32
            * 0.5
    }
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct Decision {
    /// Physical backlight multiplier relative to the user-selected brightness.
    pub backlight_scale: f32,
    pub rms_error: f32,
    pub p99_error: f32,
    pub estimated_backlight_saving: f32,
}

impl Default for Decision {
    fn default() -> Self {
        Self {
            backlight_scale: 1.0,
            rms_error: 0.0,
            p99_error: 0.0,
            estimated_backlight_saving: 0.0,
        }
    }
}

/// Monotonic normalized Reinhard curve. Its slope near black is `1 / scale`
/// and it maps white to white, avoiding hard highlight clipping.
pub fn compensated_luminance(input: f32, backlight_scale: f32) -> f32 {
    compensated_luminance_preserving_black(input, backlight_scale, 0.0)
}

pub fn compensated_luminance_preserving_black(
    input: f32,
    backlight_scale: f32,
    black_threshold: f32,
) -> f32 {
    let x = input.clamp(0.0, 1.0);
    let scale = backlight_scale.clamp(0.05, 1.0);
    if scale >= 0.9999 || x == 0.0 {
        return x;
    }
    let shoulder = scale / (1.0 - scale);
    let mapped = (x * (1.0 + shoulder) / (x + shoulder)).clamp(0.0, 1.0);
    let black = black_threshold.clamp(0.0, 0.1);
    if black == 0.0 || x >= black * 2.0 {
        mapped
    } else if x <= black {
        x
    } else {
        let position = (x - black) / black;
        let blend = position * position * (3.0 - 2.0 * position);
        x + (mapped - x) * blend
    }
}

fn error_for(input: f32, scale: f32, black_threshold: f32) -> f32 {
    (input - scale * compensated_luminance_preserving_black(input, scale, black_threshold)).abs()
}

pub fn decide(histogram: &Histogram, config: &PolicyConfig) -> Decision {
    if histogram.samples == 0 || config.validate().is_err() {
        return Decision::default();
    }

    let minimum = 1.0 - config.max_backlight_reduction;
    let mut best = Decision::default();
    // Search from most conservative to most aggressive in 0.5% increments.
    let steps = ((1.0 - minimum) / 0.005).round() as usize;
    for step in 1..=steps {
        let scale = 1.0 - step as f32 * 0.005;
        let mut squared = 0.0f64;
        let mut weighted_errors = Vec::with_capacity(HISTOGRAM_BINS);
        for (index, &count) in histogram.bins.iter().enumerate() {
            if count == 0 {
                continue;
            }
            let luminance = (index as f32 + 0.5) / HISTOGRAM_BINS as f32;
            let error = error_for(luminance, scale, config.black_preservation_threshold);
            squared += error as f64 * error as f64 * count as f64;
            weighted_errors.push((error, count));
        }
        let rms = (squared / histogram.samples as f64).sqrt() as f32;
        weighted_errors.sort_by(|a, b| a.0.total_cmp(&b.0));
        let cutoff = (histogram.samples as f64 * 0.99).ceil() as u64;
        let mut seen = 0u64;
        let mut p99 = 0.0;
        for (error, count) in weighted_errors {
            seen += count;
            p99 = error;
            if seen >= cutoff {
                break;
            }
        }
        if rms <= config.max_rms_error && p99 <= config.max_p99_error {
            best = Decision {
                backlight_scale: scale,
                rms_error: rms,
                p99_error: p99,
                estimated_backlight_saving: 1.0 - scale,
            };
        } else {
            break;
        }
    }
    best
}

#[derive(Clone, Debug)]
pub struct Smoother {
    current: f32,
}

impl Default for Smoother {
    fn default() -> Self {
        Self { current: 1.0 }
    }
}

impl Smoother {
    pub fn current(&self) -> f32 {
        self.current
    }

    pub fn update(&mut self, target: f32, elapsed_ms: u32, config: &PolicyConfig) -> f32 {
        if (target - self.current).abs() < config.hysteresis {
            return self.current;
        }
        let tau = config.transition_ms.max(1) as f32;
        let alpha = 1.0 - (-(elapsed_ms as f32) / tau).exp();
        self.current += (target.clamp(0.05, 1.0) - self.current) * alpha;
        self.current
    }
}

pub fn srgb_to_linear(value: f32) -> f32 {
    if value <= 0.04045 {
        value / 12.92
    } else {
        ((value + 0.055) / 1.055).powf(2.4)
    }
}

pub fn linear_to_srgb(value: f32) -> f32 {
    let value = value.clamp(0.0, 1.0);
    if value <= 0.003_130_8 {
        12.92 * value
    } else {
        1.055 * value.powf(1.0 / 2.4) - 0.055
    }
}

pub fn linear_luminance(rgb: [f32; 3]) -> f32 {
    0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]
}

/// Apply the luminance curve while approximately retaining chromaticity.
/// Returns linear RGB and whether gamut compression was required.
pub fn compensate_rgb(linear_rgb: [f32; 3], scale: f32, black_threshold: f32) -> ([f32; 3], bool) {
    let luminance = linear_luminance(linear_rgb);
    if luminance <= f32::EPSILON {
        return (linear_rgb, false);
    }
    let target = compensated_luminance_preserving_black(luminance, scale, black_threshold);
    let gain = target / luminance;
    let mut output = linear_rgb.map(|channel| channel * gain);
    let maximum = output.into_iter().fold(0.0f32, f32::max);
    let compressed = maximum > 1.0;
    if compressed {
        output = output.map(|channel| channel / maximum);
    }
    (output, compressed)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn curve_preserves_endpoints_and_is_monotonic() {
        for scale in [0.5, 0.7, 0.9, 1.0] {
            assert_eq!(compensated_luminance(0.0, scale), 0.0);
            assert!((compensated_luminance(1.0, scale) - 1.0).abs() < 1e-6);
            let mut previous = 0.0;
            for index in 1..=1000 {
                let current = compensated_luminance(index as f32 / 1000.0, scale);
                assert!(current >= previous);
                previous = current;
            }
        }
    }

    #[test]
    fn configured_black_band_is_never_lifted() {
        for value in [0.0, 0.001, 0.005, 0.01] {
            assert_eq!(
                compensated_luminance_preserving_black(value, 0.65, 0.01),
                value
            );
        }
        assert!(compensated_luminance_preserving_black(0.03, 0.65, 0.01) > 0.03);
    }

    #[test]
    fn dark_content_permits_more_saving_than_white_content() {
        let dark = Histogram::from_linear_luminances(std::iter::repeat_n(0.02, 10_000));
        let white = Histogram::from_linear_luminances(std::iter::repeat_n(0.98, 10_000));
        let config = PolicyConfig::default();
        assert!(decide(&dark, &config).backlight_scale < decide(&white, &config).backlight_scale);
    }

    #[test]
    fn reduction_percentage_is_user_facing_and_enforced() {
        let histogram = Histogram::from_linear_luminances(std::iter::repeat_n(0.02, 10_000));
        let mut config = PolicyConfig::default();
        config.set_max_backlight_reduction_percent(12.0).unwrap();
        assert!((config.max_backlight_reduction_percent() - 12.0).abs() < f32::EPSILON);
        assert!(decide(&histogram, &config).estimated_backlight_saving <= 0.120_001);
        assert!(config.set_max_backlight_reduction_percent(75.1).is_err());
        assert!(
            config
                .set_max_backlight_reduction_percent(f32::NAN)
                .is_err()
        );
    }

    #[test]
    fn histogram_distance_is_bounded() {
        let black = Histogram::from_linear_luminances([0.0; 10]);
        let white = Histogram::from_linear_luminances([1.0; 10]);
        assert!((black.distance(&white) - 1.0).abs() < 1e-6);
        assert_eq!(black.distance(&black), 0.0);
    }

    #[test]
    fn idle_controller_never_samples_video_or_inhibited_sessions() {
        let config = PolicyConfig::default();
        let base = Eligibility {
            idle_ms: u64::from(config.idle_seconds) * 1000,
            on_battery: true,
            internal_display: true,
            ..Eligibility::default()
        };
        let mut controller = RuntimeController::default();
        assert_eq!(
            controller.evaluate(base, &config),
            RuntimeAction::AnalyzeOnce
        );
        controller.sample_rejected();
        assert_eq!(
            controller.evaluate(
                Eligibility {
                    idle_inhibited: true,
                    ..base
                },
                &config
            ),
            RuntimeAction::None
        );
        assert_eq!(
            controller.evaluate(
                Eligibility {
                    frames_per_second: 30.0,
                    ..base
                },
                &config
            ),
            RuntimeAction::None
        );
    }

    #[test]
    fn input_deactivates_without_sampling() {
        let config = PolicyConfig::default();
        let idle = Eligibility {
            idle_ms: u64::from(config.idle_seconds) * 1000,
            on_battery: true,
            internal_display: true,
            ..Eligibility::default()
        };
        let mut controller = RuntimeController::default();
        assert_eq!(
            controller.evaluate(idle, &config),
            RuntimeAction::AnalyzeOnce
        );
        controller.sample_applied();
        assert_eq!(controller.state(), RuntimeState::Active);
        assert_eq!(
            controller.evaluate(Eligibility { idle_ms: 0, ..idle }, &config),
            RuntimeAction::Deactivate
        );
    }
}
