from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import numpy as np
import OpenEXR
from PIL import Image


class ImageComparisonError(ValueError):
    """The images cannot be compared safely."""


def read_exr_rgb(path: Path) -> np.ndarray:
    path = path.resolve()
    if not path.is_file():
        raise FileNotFoundError(f"EXR image not found: {path}")
    try:
        channels = OpenEXR.File(str(path)).channels()
    except Exception as error:
        raise OSError(f"Failed to read EXR image '{path}': {error}") from error

    if "RGB" in channels:
        rgb = channels["RGB"].pixels
    elif "RGBA" in channels:
        rgb = channels["RGBA"].pixels[..., :3]
    elif all(name in channels for name in ("R", "G", "B")):
        rgb = np.stack([channels[name].pixels for name in ("R", "G", "B")], axis=-1)
    else:
        raise ImageComparisonError(
            f"EXR image '{path}' must contain named R, G, and B channels; found {sorted(channels)}."
        )

    result = np.asarray(rgb, dtype=np.float32)
    if result.ndim != 3 or result.shape[2] != 3:
        raise ImageComparisonError(
            f"EXR image '{path}' has invalid RGB shape {result.shape}."
        )
    return result


def write_exr_rgb(path: Path, rgb: np.ndarray) -> None:
    image = np.asarray(rgb, dtype=np.float32)
    if image.ndim != 3 or image.shape[2] != 3:
        raise ValueError(f"Expected HxWx3 RGB image, got {image.shape}.")
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        OpenEXR.File(
            {"compression": OpenEXR.ZIP_COMPRESSION},
            {"RGB": np.ascontiguousarray(image)},
        ).write(str(path))
    except Exception as error:
        raise OSError(f"Failed to write EXR image '{path}': {error}") from error


def finite_counts(image: np.ndarray) -> dict[str, int]:
    return {
        "nan": int(np.isnan(image).sum()),
        "inf": int(np.isinf(image).sum()),
    }


def validate_comparison_inputs(actual: np.ndarray, reference: np.ndarray) -> None:
    if actual.shape != reference.shape:
        raise ImageComparisonError(
            f"Image shape mismatch: actual {actual.shape}, reference {reference.shape}."
        )
    if actual.ndim != 3 or actual.shape[2] != 3:
        raise ImageComparisonError(
            f"Images must be HxWx3 RGB arrays, got {actual.shape}."
        )

    actual_finite = finite_counts(actual)
    reference_finite = finite_counts(reference)
    if actual_finite["nan"] or actual_finite["inf"]:
        raise ImageComparisonError(
            f"Actual image contains NaN={actual_finite['nan']}, Inf={actual_finite['inf']}."
        )
    if reference_finite["nan"] or reference_finite["inf"]:
        raise ImageComparisonError(
            f"Reference image contains NaN={reference_finite['nan']}, Inf={reference_finite['inf']}."
        )


def compute_metrics(actual: np.ndarray, reference: np.ndarray) -> dict[str, Any]:
    validate_comparison_inputs(actual, reference)
    actual64 = actual.astype(np.float64, copy=False)
    reference64 = reference.astype(np.float64, copy=False)
    difference = actual64 - reference64
    absolute = np.abs(difference)

    squared = difference * difference
    squared_sum = float(squared.sum())
    reference_energy = float((reference64 * reference64).sum())
    if reference_energy == 0.0:
        if squared_sum != 0.0:
            raise ImageComparisonError(
                "NRMSE is undefined for a zero-energy reference and nonzero actual image."
            )
        nrmse = 0.0
    else:
        nrmse = float(np.sqrt(squared_sum / reference_energy))

    axes = (0, 1)
    actual_mean = actual64.mean(axis=axes)
    reference_mean = reference64.mean(axis=axes)
    mean_bias = (actual_mean - reference_mean) / np.maximum(
        np.abs(reference_mean), 1e-6
    )
    mae_channels = absolute.mean(axis=axes)
    mse_channels = squared.mean(axis=axes)

    with np.errstate(divide="ignore", invalid="ignore"):
        relative_squared = squared / np.square(reference64 + 0.01)
    relative_squared[~np.isfinite(relative_squared)] = 0.0
    mrse_channels = relative_squared.mean(axis=axes)

    luminance_weights = np.asarray([0.2126, 0.7152, 0.0722], dtype=np.float64)
    actual_luminance = float(actual_mean @ luminance_weights)
    reference_luminance = float(reference_mean @ luminance_weights)
    luminance_bias = (actual_luminance - reference_luminance) / max(
        abs(reference_luminance), 1e-6
    )

    height, width, _ = actual.shape
    return {
        "resolution": {"width": width, "height": height},
        "finite": {
            "actual": finite_counts(actual),
            "reference": finite_counts(reference),
        },
        "nrmse": nrmse,
        "mean_rgb": {
            "actual": actual_mean.tolist(),
            "reference": reference_mean.tolist(),
        },
        "mean_rgb_bias": {
            "channels": mean_bias.tolist(),
            "max_abs": float(np.max(np.abs(mean_bias))),
        },
        "mean_luminance": {
            "actual": actual_luminance,
            "reference": reference_luminance,
            "relative_bias": float(luminance_bias),
        },
        "mae": {
            "channels": mae_channels.tolist(),
            "mean": float(mae_channels.mean()),
        },
        "mse": {
            "channels": mse_channels.tolist(),
            "mean": float(mse_channels.mean()),
        },
        "mrse": {
            "channels": mrse_channels.tolist(),
            "mean": float(mrse_channels.mean()),
        },
        "absolute_error": {
            "p95": float(np.percentile(absolute, 95.0)),
            "p99": float(np.percentile(absolute, 99.0)),
        },
        "range": {
            "actual_min": float(actual64.min()),
            "actual_max": float(actual64.max()),
            "reference_min": float(reference64.min()),
            "reference_max": float(reference64.max()),
        },
        "negative_values": {
            "actual": int((actual64 < 0.0).sum()),
            "reference": int((reference64 < 0.0).sum()),
        },
    }


def compare_images(
    actual: np.ndarray,
    reference: np.ndarray,
    limits: dict[str, float] | None = None,
) -> dict[str, Any]:
    metrics = compute_metrics(actual, reference)
    limits = limits or {}
    failures: list[str] = []
    if "nrmse" in limits and metrics["nrmse"] > limits["nrmse"]:
        failures.append(f"NRMSE {metrics['nrmse']:.6g} exceeds {limits['nrmse']:.6g}")
    mean_rgb_bias = metrics["mean_rgb_bias"]["max_abs"]
    if "mean_rgb_bias" in limits and mean_rgb_bias > limits["mean_rgb_bias"]:
        failures.append(
            f"mean RGB bias {mean_rgb_bias:.6g} exceeds {limits['mean_rgb_bias']:.6g}"
        )
    return {
        "passed": not failures,
        "limits": limits,
        "failures": failures,
        "metrics": metrics,
    }


def write_difference_images(
    actual: np.ndarray,
    reference: np.ndarray,
    exr_path: Path | None,
    png_path: Path | None,
) -> None:
    validate_comparison_inputs(actual, reference)
    difference = np.abs(
        actual.astype(np.float64) - reference.astype(np.float64)
    ).astype(np.float32)
    if exr_path is not None:
        write_exr_rgb(exr_path, difference)
    if png_path is not None:
        scale = float(np.percentile(difference, 99.0))
        normalized = (
            np.zeros_like(difference)
            if scale == 0.0
            else np.clip(difference / scale, 0.0, 1.0)
        )
        display = np.round(np.power(normalized, 1.0 / 2.2) * 255.0).astype(np.uint8)
        png_path = png_path.resolve()
        png_path.parent.mkdir(parents=True, exist_ok=True)
        Image.fromarray(display, mode="RGB").save(png_path)


def write_json(path: Path, value: dict[str, Any]) -> None:
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
    )
