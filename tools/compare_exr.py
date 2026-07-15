from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
if str(REPOSITORY_ROOT) not in sys.path:
    sys.path.insert(0, str(REPOSITORY_ROOT))

from tools.exr_utils import (  # noqa: E402
    ImageComparisonError,
    compare_images,
    read_exr_rgb,
    write_difference_images,
    write_json,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare two linear RGB EXR images.")
    parser.add_argument("--actual", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--nrmse-limit", type=float)
    parser.add_argument("--mean-rgb-bias-limit", type=float)
    parser.add_argument("--json", type=Path, dest="json_path")
    parser.add_argument("--diff-exr", type=Path)
    parser.add_argument("--diff-png", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    limits = {}
    if args.nrmse_limit is not None:
        if args.nrmse_limit < 0.0:
            print("error: --nrmse-limit must be non-negative", file=sys.stderr)
            return 2
        limits["nrmse"] = args.nrmse_limit
    if args.mean_rgb_bias_limit is not None:
        if args.mean_rgb_bias_limit < 0.0:
            print("error: --mean-rgb-bias-limit must be non-negative", file=sys.stderr)
            return 2
        limits["mean_rgb_bias"] = args.mean_rgb_bias_limit

    try:
        actual = read_exr_rgb(args.actual)
        reference = read_exr_rgb(args.reference)
        report = compare_images(actual, reference, limits)
        write_difference_images(actual, reference, args.diff_exr, args.diff_png)
        if args.json_path is not None:
            write_json(args.json_path, report)
    except ImageComparisonError as error:
        print(f"comparison failed: {error}", file=sys.stderr)
        return 1
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    print(json.dumps(report, indent=2, sort_keys=True, allow_nan=False))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
