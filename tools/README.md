# Yutrel regression tools

Run a fast Cornell Box regression:

```text
uv run python tools/render_regression.py projects/Yutrel/scene/cornell-box --profile quick
```

Run the four-seed quality profile:

```text
uv run python tools/render_regression.py projects/Yutrel/scene/cornell-box --profile quality
```

Compare two linear RGB EXR files directly:

```text
uv run python tools/compare_exr.py --actual actual.exr --reference reference.exr --nrmse-limit 0.03 --mean-rgb-bias-limit 0.05
```

The tools never generate or update `reference.exr`. Regression outputs and JSON reports are
written below `build/Yutrel/regression/`.
