from __future__ import annotations

from tools.pmx_to_pbrt import PmxModel, Vertex, write_scene


def test_write_scene_uses_explicit_encodings_and_structured_indentation(tmp_path):
    model = PmxModel(
        name="fixture",
        english_name="fixture",
        version=2.0,
        vertices=[
            Vertex((-1.0, 0.0, -1.0), (0.0, 1.0, 0.0), (0.0, 0.0)),
            Vertex((1.0, 2.0, 1.0), (0.0, 1.0, 0.0), (1.0, 1.0)),
        ],
        indices=[],
        textures=[],
        materials=[],
    )
    records = [
        {
            "index": 0,
            "name": "fixture",
            "diffuse": [1.0, 1.0, 1.0, 0.75],
            "texture_index": 3,
            "texture_path": "tex/color.png",
            "alpha_path": "alpha/mask.png",
            "ply_path": "meshes/material_000.ply",
        }
    ]
    scene_path = tmp_path / "scene.pbrt"

    write_scene(
        scene_path,
        tmp_path / "fixture.pmx",
        model,
        records,
        samples=16,
        x_resolution=320,
        y_resolution=450,
        camera_side="negative-z",
    )

    text = scene_path.read_text(encoding="utf-8")
    assert 'Sampler "sobol"\n' in text
    assert 'PixelFilter "triangle"\n' in text
    assert 'Sampler "halton"' not in text
    assert 'PixelFilter "gaussian"' not in text
    assert (
        'Texture "pmx_texture_003" "spectrum" "imagemap"\n'
        '    "string filename" [ "tex/color.png" ]\n'
        '    "string filter" [ "bilinear" ]\n'
        '    "string encoding" [ "sRGB" ]\n'
    ) in text
    assert (
        "AttributeBegin\n"
        '    Texture "pmx_alpha_000" "float" "imagemap"\n'
        '        "string filename" [ "alpha/mask.png" ]\n'
        '        "string filter" [ "bilinear" ]\n'
        '        "string encoding" [ "sRGB" ]\n'
        '        "float scale" [ 0.75 ]\n'
        '    NamedMaterial "pmx_material_000"\n'
        '    Shape "plymesh"\n'
        '        "string filename" [ "meshes/material_000.ply" ]\n'
        '        "texture alpha" [ "pmx_alpha_000" ]\n'
        "AttributeEnd\n"
    ) in text
    assert '"string sensor"' not in text
    assert '"float iso"' not in text
    assert '"ewa"' not in text
    assert "\t" not in text
    assert all(line == line.rstrip() for line in text.splitlines())
    assert text.endswith("\n") and not text.endswith("\n\n")
