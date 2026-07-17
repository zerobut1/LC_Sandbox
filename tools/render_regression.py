from __future__ import annotations

import argparse
import json
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

import numpy as np

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
if str(REPOSITORY_ROOT) not in sys.path:
    sys.path.insert(0, str(REPOSITORY_ROOT))

from tools.exr_utils import (  # noqa: E402
    ImageComparisonError,
    compare_images,
    read_exr_rgb,
    validate_comparison_inputs,
    write_difference_images,
    write_exr_rgb,
    write_json,
)


class RegressionError(RuntimeError):
    """Regression configuration or execution failed."""


def _positive_uint(value: Any, description: str, maximum: int = 0xFFFFFFFF) -> int:
    if (
        isinstance(value, bool)
        or not isinstance(value, int)
        or value <= 0
        or value > maximum
    ):
        raise RegressionError(f"{description} must be an integer in [1, {maximum}].")
    return value


def _seed(value: Any, description: str) -> int:
    if (
        isinstance(value, bool)
        or not isinstance(value, int)
        or value < 0
        or value > 0xFFFFFFFF
    ):
        raise RegressionError(f"{description} must be an integer in [0, 4294967295].")
    return value


def load_manifest(scene_dir: Path, profile_name: str) -> dict[str, Any]:
    scene_dir = scene_dir.resolve()
    manifest_path = scene_dir / "regression.json"
    if not manifest_path.is_file():
        raise RegressionError(f"Regression manifest not found: {manifest_path}")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RegressionError(f"Failed to read '{manifest_path}': {error}") from error
    if not isinstance(manifest, dict):
        raise RegressionError("Regression manifest root must be an object.")

    if manifest.get("scene") != "scene-yutrel.pbrt":
        raise RegressionError("Manifest 'scene' must be 'scene-yutrel.pbrt'.")
    scene_path = (scene_dir / manifest["scene"]).resolve()
    if not scene_path.is_file() or not scene_path.is_relative_to(scene_dir):
        raise RegressionError(
            f"Scene file is missing or escapes its directory: {scene_path}"
        )

    reference = manifest.get("reference")
    if not isinstance(reference, dict):
        raise RegressionError("Manifest 'reference' must be an object.")
    if reference.get("image") != "reference.exr":
        raise RegressionError("Reference image must be named 'reference.exr'.")
    for key in ("renderer", "version", "color_space"):
        if not isinstance(reference.get(key), str) or not reference[key].strip():
            raise RegressionError(
                f"Reference metadata '{key}' must be a non-empty string."
            )
    if reference["color_space"] != "linear-sRGB":
        raise RegressionError("Reference color_space must be exactly 'linear-sRGB'.")
    _positive_uint(reference.get("spp"), "reference.spp")
    reference_path = (scene_dir / reference["image"]).resolve()
    if not reference_path.is_file() or not reference_path.is_relative_to(scene_dir):
        raise RegressionError(
            f"Reference image is missing or escapes its directory: {reference_path}"
        )

    profiles = manifest.get("profiles")
    if not isinstance(profiles, dict) or profile_name not in profiles:
        raise RegressionError(
            f"Profile '{profile_name}' is not defined in the manifest."
        )
    profile = profiles[profile_name]
    if not isinstance(profile, dict):
        raise RegressionError(f"Profile '{profile_name}' must be an object.")
    _positive_uint(profile.get("spp"), f"profiles.{profile_name}.spp")
    seeds = profile.get("seeds")
    if not isinstance(seeds, list) or not seeds:
        raise RegressionError(
            f"profiles.{profile_name}.seeds must be a non-empty array."
        )
    validated_seeds = [
        _seed(value, f"profiles.{profile_name}.seeds") for value in seeds
    ]
    if len(validated_seeds) != len(set(validated_seeds)):
        raise RegressionError(
            f"profiles.{profile_name}.seeds must not contain duplicates."
        )

    limits = profile.get("limits")
    if not isinstance(limits, dict):
        raise RegressionError(f"profiles.{profile_name}.limits must be an object.")
    for key in ("nrmse", "mean_rgb_bias"):
        value = limits.get(key)
        if (
            isinstance(value, bool)
            or not isinstance(value, (int, float))
            or value < 0.0
        ):
            raise RegressionError(
                f"profiles.{profile_name}.limits.{key} must be non-negative."
            )

    return {
        "manifest_path": manifest_path,
        "scene_path": scene_path,
        "reference_path": reference_path,
        "reference": reference,
        "profile": {
            "name": profile_name,
            "spp": profile["spp"],
            "seeds": validated_seeds,
            "limits": {key: float(limits[key]) for key in ("nrmse", "mean_rgb_bias")},
        },
    }


def ensure_not_reference(output_path: Path, reference_path: Path) -> None:
    if output_path.resolve() == reference_path.resolve():
        raise RegressionError(
            f"Refusing to overwrite reference image: {reference_path}"
        )


def run_checked(command: list[str], cwd: Path) -> float:
    start = time.perf_counter()
    result = subprocess.run(command, cwd=cwd, check=False)
    duration = time.perf_counter() - start
    if result.returncode != 0:
        raise RegressionError(
            f"Command failed with exit code {result.returncode}: {' '.join(command)}"
        )
    return duration


def average_images(images: list[np.ndarray]) -> np.ndarray:
    if not images:
        raise RegressionError("Cannot average an empty image list.")
    first = images[0]
    for image in images[1:]:
        validate_comparison_inputs(image, first)
    return np.mean(np.stack(images, axis=0), axis=0, dtype=np.float64).astype(
        np.float32
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run a Yutrel image regression profile."
    )
    parser.add_argument("scene_dir", type=Path)
    parser.add_argument("--profile", choices=("quick", "quality"), default="quick")
    parser.add_argument("--backend", default="dx")
    parser.add_argument("--skip-build", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        configuration = load_manifest(args.scene_dir, args.profile)
        reference_path: Path = configuration["reference_path"]
        reference = read_exr_rgb(reference_path)
        validate_comparison_inputs(reference, reference)
        height, width, _ = reference.shape

        profile = configuration["profile"]
        output_dir = (
            REPOSITORY_ROOT
            / "build"
            / "Yutrel"
            / "regression"
            / args.scene_dir.resolve().name
            / args.profile
        ).resolve()
        output_dir.mkdir(parents=True, exist_ok=True)
        ensure_not_reference(output_dir / "actual.exr", reference_path)

        build_seconds = 0.0
        if not args.skip_build:
            build_seconds = run_checked(["xmake", "build", "Yutrel"], REPOSITORY_ROOT)

        seed_images: list[np.ndarray] = []
        seed_reports: list[dict[str, Any]] = []
        render_seconds = 0.0
        for seed in profile["seeds"]:
            output_path = output_dir / f"seed-{seed}.exr"
            ensure_not_reference(output_path, reference_path)
            output_path.unlink(missing_ok=True)
            command = [
                "xmake",
                "run",
                "Yutrel",
                args.backend,
                str(configuration["scene_path"]),
                "--headless",
                "--spp",
                str(profile["spp"]),
                "--seed",
                str(seed),
                "--resolution",
                f"{width}x{height}",
                "--output",
                str(output_path),
            ]
            duration = run_checked(command, REPOSITORY_ROOT)
            render_seconds += duration
            if not output_path.is_file():
                raise RegressionError(
                    f"Renderer did not create expected output: {output_path}"
                )
            image = read_exr_rgb(output_path)
            seed_report = compare_images(image, reference)
            seed_reports.append(
                {
                    "seed": seed,
                    "seconds": duration,
                    "output": str(output_path),
                    "metrics": seed_report["metrics"],
                }
            )
            seed_images.append(image)

        actual = average_images(seed_images)
        actual_path = output_dir / "actual.exr"
        write_exr_rgb(actual_path, actual)
        comparison = compare_images(actual, reference, profile["limits"])
        write_difference_images(
            actual,
            reference,
            output_dir / "absolute-difference.exr",
            output_dir / "difference.png",
        )

        seed_nrmse = [report["metrics"]["nrmse"] for report in seed_reports]
        report = {
            "passed": comparison["passed"],
            "scene": str(configuration["scene_path"]),
            "manifest": str(configuration["manifest_path"]),
            "reference": {
                **configuration["reference"],
                "path": str(reference_path),
            },
            "profile": profile,
            "backend": args.backend,
            "timing_seconds": {
                "build": build_seconds,
                "render_total": render_seconds,
            },
            "seed_nrmse_stddev": statistics.pstdev(seed_nrmse)
            if len(seed_nrmse) > 1
            else 0.0,
            "seeds": seed_reports,
            "comparison": comparison,
        }
        write_json(output_dir / "report.json", report)

        metrics = comparison["metrics"]
        print(
            f"Yutrel {args.profile}: {'PASSED' if comparison['passed'] else 'FAILED'}; "
            f"NRMSE={metrics['nrmse']:.6f}, "
            f"mean RGB bias={metrics['mean_rgb_bias']['max_abs']:.6f}, "
            f"render={render_seconds:.2f}s"
        )
        for failure in comparison["failures"]:
            print(f"  {failure}", file=sys.stderr)
        print(f"Report: {output_dir / 'report.json'}")
        return 0 if comparison["passed"] else 1
    except ImageComparisonError as error:
        print(f"regression failed: {error}", file=sys.stderr)
        return 1
    except (RegressionError, OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
