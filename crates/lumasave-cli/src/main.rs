use std::{fs, path::PathBuf};

use anyhow::{Context, Result, bail};
use clap::{Parser, Subcommand};
use image::{ImageBuffer, Rgb, RgbImage};
use lumasave_core::{
    Histogram, PolicyConfig, compensate_rgb, decide, linear_luminance, linear_to_srgb,
    srgb_to_linear,
};
use serde::Serialize;

#[derive(Parser)]
#[command(
    version,
    about = "Simulate content-adaptive LCD backlight power saving"
)]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    Analyze {
        input: PathBuf,
        #[arg(short, long)]
        config: Option<PathBuf>,
        #[arg(short, long)]
        output: Option<PathBuf>,
        #[arg(long)]
        json: bool,
        /// Maximum physical backlight reduction, as a percentage.
        #[arg(long, value_name = "PERCENT", value_parser = clap::value_parser!(f32))]
        max_reduction: Option<f32>,
    },
    Curve {
        #[arg(long, default_value_t = 0.75)]
        scale: f32,
        #[arg(long, default_value_t = 256)]
        samples: usize,
    },
}

#[derive(Serialize)]
struct Report {
    width: u32,
    height: u32,
    mean_linear_luminance: f64,
    backlight_scale: f32,
    estimated_backlight_saving_percent: f32,
    rms_luminance_error: f32,
    p99_luminance_error: f32,
    gamut_compressed_pixels_percent: f64,
}

fn load_config(path: Option<PathBuf>) -> Result<PolicyConfig> {
    let config = if let Some(path) = path {
        toml::from_str(&fs::read_to_string(&path).with_context(|| format!("read {path:?}"))?)
            .with_context(|| format!("parse {path:?}"))?
    } else {
        PolicyConfig::default()
    };
    config.validate().map_err(anyhow::Error::msg)?;
    Ok(config)
}

fn analyze(
    input: PathBuf,
    config: Option<PathBuf>,
    output: Option<PathBuf>,
    json: bool,
    max_reduction: Option<f32>,
) -> Result<()> {
    let mut config = load_config(config)?;
    if let Some(percent) = max_reduction {
        config
            .set_max_backlight_reduction_percent(percent)
            .map_err(anyhow::Error::msg)?;
    }
    let source = image::open(&input)
        .with_context(|| format!("open {input:?}"))?
        .into_rgb8();
    let mut histogram = Histogram::default();
    let mut luminance_sum = 0.0f64;
    for pixel in source.pixels() {
        let linear = pixel
            .0
            .map(|channel| srgb_to_linear(channel as f32 / 255.0));
        let luminance = linear_luminance(linear);
        histogram.add_linear_luminance(luminance);
        luminance_sum += luminance as f64;
    }
    let decision = decide(&histogram, &config);
    let mut compressed = 0u64;
    let mut compensated: RgbImage = ImageBuffer::new(source.width(), source.height());
    for (destination, pixel) in compensated.pixels_mut().zip(source.pixels()) {
        let linear = pixel
            .0
            .map(|channel| srgb_to_linear(channel as f32 / 255.0));
        let (mapped, was_compressed) = compensate_rgb(
            linear,
            decision.backlight_scale,
            config.black_preservation_threshold,
        );
        compressed += u64::from(was_compressed);
        *destination =
            Rgb(mapped
                .map(|channel| (linear_to_srgb(channel) * 255.0).round().clamp(0.0, 255.0) as u8));
    }
    if let Some(path) = output {
        compensated
            .save(&path)
            .with_context(|| format!("write {path:?}"))?;
    }
    let pixels = source.width() as u64 * source.height() as u64;
    let report = Report {
        width: source.width(),
        height: source.height(),
        mean_linear_luminance: luminance_sum / pixels as f64,
        backlight_scale: decision.backlight_scale,
        estimated_backlight_saving_percent: decision.estimated_backlight_saving * 100.0,
        rms_luminance_error: decision.rms_error,
        p99_luminance_error: decision.p99_error,
        gamut_compressed_pixels_percent: 100.0 * compressed as f64 / pixels as f64,
    };
    if json {
        println!("{}", serde_json::to_string_pretty(&report)?);
    } else {
        println!("Image: {}x{}", report.width, report.height);
        println!("Mean linear luminance: {:.4}", report.mean_linear_luminance);
        println!(
            "Recommended backlight scale: {:.1}%",
            100.0 * report.backlight_scale
        );
        println!(
            "Estimated backlight saving: {:.1}%",
            report.estimated_backlight_saving_percent
        );
        println!("RMS luminance error: {:.4}", report.rms_luminance_error);
        println!("P99 luminance error: {:.4}", report.p99_luminance_error);
        println!(
            "Gamut-compressed pixels: {:.3}%",
            report.gamut_compressed_pixels_percent
        );
    }
    Ok(())
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.command {
        Command::Analyze {
            input,
            config,
            output,
            json,
            max_reduction,
        } => analyze(input, config, output, json, max_reduction),
        Command::Curve { scale, samples } => {
            if !(0.05..=1.0).contains(&scale) || samples < 2 {
                bail!("scale must be 0.05..=1 and samples at least 2");
            }
            for index in 0..samples {
                let input = index as f32 / (samples - 1) as f32;
                println!(
                    "{input:.6},{:.6}",
                    lumasave_core::compensated_luminance(input, scale)
                );
            }
            Ok(())
        }
    }
}
