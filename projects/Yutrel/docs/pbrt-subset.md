# PBRT v4 Subset for Yutrel

This document defines the first PBRT v4 subset supported by Yutrel. The scope is intentionally narrow: it only needs to load the current Cornell Box scene at `projects/Yutrel/scene/cornell-box/scene-v4.pbrt` and convert it into the existing `Scene::CreateInfo -> Scene::create -> Renderer` flow.

The parser should reject unsupported PBRT syntax with a clear error that includes the source file and line number when possible.

## Goals

- Load the Cornell Box scene from `scene-v4.pbrt`.
- Keep `Scene::CreateInfo` as the runtime construction boundary.
- Parse PBRT into temporary scene state first; do not create renderer or runtime objects directly from the parser.
- Keep each implementation step small enough that the project can stay buildable.

## Supported Directives

The first version supports only the directives present in the Cornell Box file.

| Directive | Supported form | Notes |
| --- | --- | --- |
| `Integrator` | `Integrator "path"` | `integer maxdepth` may be recorded. Initial renderer mapping can keep existing defaults if needed. |
| `Transform` | `Transform [ 16 floats ]` | Used before `WorldBegin` for camera transform. Later world-space transforms should be applied to shapes if they appear. |
| `Sampler` | `Sampler "sobol"` | `integer pixelsamples` maps to camera/sample count settings. |
| `PixelFilter` | `PixelFilter "triangle"` | `float xradius` and `float yradius` are recorded. Mapping may fallback to the closest existing filter. |
| `Film` | `Film "rgb"` | Supports `string filename`, `integer xresolution`, and `integer yresolution`. |
| `Camera` | `Camera "perspective"` | Supports `float fov`. |
| `WorldBegin` | `WorldBegin` | Separates global render options from world contents. |
| `AttributeBegin` | `AttributeBegin` | Pushes current attribute state. |
| `AttributeEnd` | `AttributeEnd` | Restores current attribute state. |
| `MakeNamedMaterial` | `MakeNamedMaterial "Name"` | Only diffuse named materials are supported. |
| `NamedMaterial` | `NamedMaterial "Name"` | Sets the current material reference for subsequent shapes. |
| `AreaLightSource` | `AreaLightSource "diffuse"` | Applies to subsequent shapes in the current attribute scope. |
| `Shape` | `Shape "trianglemesh"` | Inline triangle meshes only. |

## Supported Parameter Types

The first version supports the parameter types used by `scene-v4.pbrt`.

| PBRT parameter type | Example | Notes |
| --- | --- | --- |
| `integer` | `"integer pixelsamples" [ 64 ]` | Scalars and arrays are supported. |
| `float` | `"float fov" [ 19.5 ]` | Scalars and arrays are supported. |
| `string` | `"string filename" [ "cornell-box.png" ]` | Quoted string values only. |
| `rgb` | `"rgb reflectance" [ 0.63 0.065 0.05 ]` | Stored as 3 floats; mapped to constant texture values with alpha `1.0`. |
| `point2` | `"point2 uv" [ ... ]` | Stored as float arrays with length divisible by 2. |
| `point3` | `"point3 P" [ ... ]` | Stored as float arrays with length divisible by 3. |
| `normal` | `"normal N" [ ... ]` | The Cornell Box file uses `normal`, not `normal3`. Store as float arrays with length divisible by 3. |

`"integer indices"` is used by `Shape "trianglemesh"` and must be preserved as an integer array whose length is divisible by 3.

Single-value parameters may omit brackets if future supported scenes need that PBRT form, but the Cornell Box file uses bracketed values.

## Cornell Box Scene Contents

The current Cornell Box scene contains:

- 1 `Integrator "path"` block.
- 1 camera `Transform [ 16 floats ]` before `WorldBegin`.
- 1 `Sampler "sobol"` block with `pixelsamples = 64`.
- 1 `PixelFilter "triangle"` block with `xradius = 1` and `yradius = 1`.
- 1 `Film "rgb"` block with `xresolution = 1024`, `yresolution = 1024`, and `filename = "cornell-box.png"`.
- 1 `Camera "perspective"` block with `fov = 19.5`.
- 8 named diffuse materials:
  - `LeftWall`
  - `RightWall`
  - `Floor`
  - `Ceiling`
  - `BackWall`
  - `ShortBox`
  - `TallBox`
  - `Light`
- 8 inline `Shape "trianglemesh"` entries:
  - floor
  - ceiling
  - back wall
  - right wall
  - left wall
  - short box
  - tall box
  - light
- 1 scoped `AreaLightSource "diffuse"` inside `AttributeBegin` / `AttributeEnd`.

The light shape uses `AreaLightSource "diffuse"` with `"rgb L" [ 17 12 4 ]` and `NamedMaterial "Light"`. The area light state should only apply inside that attribute scope.

## Material Mapping

Only PBRT diffuse named materials are supported.

```pbrt
MakeNamedMaterial "LeftWall"
    "string type" [ "diffuse" ]
    "rgb reflectance" [ 0.63 0.065 0.05 ]
```

Mapping rules:

- `"string type" [ "diffuse" ]` maps to Yutrel's diffuse surface.
- `"rgb reflectance"` maps to a constant texture value.
- Alpha should be set to `1.0`.
- Unknown material names are errors.
- Non-diffuse material types are unsupported errors.

## Light Mapping

Only diffuse area lights are supported.

```pbrt
AreaLightSource "diffuse"
    "rgb L" [ 17 12 4 ]
```

Mapping rules:

- `AreaLightSource "diffuse"` maps to Yutrel's diffuse light.
- `"rgb L"` maps to constant emission.
- `"float scale"` may be supported if present in future files.
- `"bool twosided"` may be supported if present in future files.
- Shapes outside the active area-light attribute scope must not inherit emission.

## Shape Mapping

Only inline triangle meshes are supported.

```pbrt
Shape "trianglemesh"
    "point2 uv" [ ... ]
    "normal N" [ ... ]
    "point3 P" [ ... ]
    "integer indices" [ ... ]
```

Required parameters:

- `"point3 P"`: vertex positions.
- `"integer indices"`: triangle indices.

Optional parameters:

- `"normal N"`: vertex normals.
- `"point2 uv"`: texture coordinates.

The initial implementation may convert each triangle mesh to a stable temporary OBJ path and reuse the existing mesh loading path. A later implementation can replace this with in-memory mesh creation.

## Explicitly Unsupported

The first version must reject these features with clear unsupported errors:

- `Include`
- `Import`
- textures and texture references
- volumes and media
- object instances
- animated transforms
- materials other than diffuse named materials
- shapes other than `Shape "trianglemesh"`
- lights other than `AreaLightSource "diffuse"`
- cameras other than `Camera "perspective"`
- samplers other than `Sampler "sobol"`
- films other than `Film "rgb"`
- pixel filters other than `PixelFilter "triangle"`
- transform directives beyond the first supported transform subset, unless they are explicitly implemented later

## Why This Is Enough for Cornell Box

`scene-v4.pbrt` uses only global render settings, a single perspective camera transform, diffuse named materials, one scoped diffuse area light, and inline triangle meshes. The supported subset above covers every directive and parameter type used by that file while avoiding unrelated PBRT features. This keeps the first loader focused on converting the Cornell Box into `Scene::CreateInfo` without committing Yutrel to full PBRT compatibility.
