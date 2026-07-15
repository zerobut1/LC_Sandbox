from __future__ import annotations

from types import SimpleNamespace

import numpy as np
import OpenEXR
import pytest

from tools.exr_utils import (
    ImageComparisonError,
    compare_images,
    compute_metrics,
    read_exr_rgb,
    write_difference_images,
    write_exr_rgb,
)
from tools.render_regression import (
    RegressionError,
    average_images,
    ensure_not_reference,
    load_manifest,
    run_checked,
)


def test_exr_round_trip(tmp_path):
    source = np.arange(36, dtype=np.float32).reshape(3, 4, 3) / 10.0
    path = tmp_path / "image.exr"
    write_exr_rgb(path, source)
    np.testing.assert_allclose(read_exr_rgb(path), source)


def test_rejects_missing_rgb_channels(tmp_path):
    path = tmp_path / "luminance.exr"
    OpenEXR.File({}, {"Y": np.zeros((2, 2), dtype=np.float32)}).write(str(path))
    with pytest.raises(ImageComparisonError, match="R, G, and B"):
        read_exr_rgb(path)


def test_known_metrics_and_limits():
    reference = np.ones((2, 3, 3), dtype=np.float32)
    actual = reference * 1.1
    metrics = compute_metrics(actual, reference)
    assert metrics["nrmse"] == pytest.approx(0.1)
    assert metrics["mean_rgb_bias"]["max_abs"] == pytest.approx(0.1)
    assert metrics["mae"]["mean"] == pytest.approx(0.1)
    assert metrics["mrse"]["mean"] == pytest.approx(0.01 / (1.01**2))
    assert compare_images(actual, reference, {"nrmse": 0.11, "mean_rgb_bias": 0.11})[
        "passed"
    ]
    assert not compare_images(
        actual, reference, {"nrmse": 0.09, "mean_rgb_bias": 0.09}
    )["passed"]


def test_shape_and_finite_values_are_hard_failures():
    reference = np.ones((2, 2, 3), dtype=np.float32)
    with pytest.raises(ImageComparisonError, match="shape mismatch"):
        compute_metrics(np.ones((1, 2, 3), dtype=np.float32), reference)
    actual = reference.copy()
    actual[0, 0, 0] = np.nan
    actual[0, 0, 1] = np.inf
    with pytest.raises(ImageComparisonError, match="NaN=1, Inf=1"):
        compute_metrics(actual, reference)


def test_difference_outputs(tmp_path):
    reference = np.zeros((2, 2, 3), dtype=np.float32)
    actual = np.ones_like(reference)
    exr_path = tmp_path / "difference.exr"
    png_path = tmp_path / "difference.png"
    write_difference_images(actual, reference, exr_path, png_path)
    assert exr_path.is_file()
    assert png_path.is_file()
    np.testing.assert_allclose(read_exr_rgb(exr_path), 1.0)


def test_average_images():
    images = [
        np.zeros((2, 2, 3), dtype=np.float32),
        np.full((2, 2, 3), 2.0, dtype=np.float32),
    ]
    np.testing.assert_allclose(average_images(images), 1.0)
    with pytest.raises(RegressionError, match="empty"):
        average_images([])


def test_reference_overwrite_is_rejected(tmp_path):
    reference = tmp_path / "reference.exr"
    with pytest.raises(RegressionError, match="overwrite"):
        ensure_not_reference(reference, reference)
    ensure_not_reference(tmp_path / "actual.exr", reference)


def test_command_failure_is_reported(monkeypatch, tmp_path):
    monkeypatch.setattr(
        "tools.render_regression.subprocess.run",
        lambda *args, **kwargs: SimpleNamespace(returncode=7),
    )
    with pytest.raises(RegressionError, match="exit code 7"):
        run_checked(["fake-command"], tmp_path)


def test_manifest_validation(tmp_path):
    (tmp_path / "scene-yutrel.pbrt").write_text(
        "WorldBegin\nWorldEnd\n", encoding="utf-8"
    )
    write_exr_rgb(tmp_path / "reference.exr", np.ones((2, 2, 3), dtype=np.float32))
    (tmp_path / "regression.json").write_text(
        """
{
  "scene": "scene-yutrel.pbrt",
  "reference": {
    "image": "reference.exr",
    "renderer": "pbrt-v4",
    "version": "test-commit",
    "spp": 16384,
    "color_space": "linear-sRGB"
  },
  "profiles": {
    "quick": {
      "spp": 32,
      "seeds": [0],
      "limits": {"nrmse": 0.15, "mean_rgb_bias": 0.10}
    }
  }
}
""".strip(),
        encoding="utf-8",
    )
    configuration = load_manifest(tmp_path, "quick")
    assert configuration["profile"]["spp"] == 32
    assert configuration["reference"]["color_space"] == "linear-sRGB"
