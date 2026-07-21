"""Convert a static PMX model to per-material PLY meshes and a PBRT-v4 scene."""

from __future__ import annotations

import argparse
import json
import math
import os
import struct
import sys
from array import array
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Vertex:
    position: tuple[float, float, float]
    normal: tuple[float, float, float]
    uv: tuple[float, float]


@dataclass(frozen=True)
class Material:
    name: str
    english_name: str
    diffuse: tuple[float, float, float, float]
    texture_index: int
    surface_count: int


@dataclass(frozen=True)
class PmxModel:
    name: str
    english_name: str
    version: float
    vertices: list[Vertex]
    indices: list[int]
    textures: list[str]
    materials: list[Material]


class PmxReader:
    def __init__(self, data: bytes) -> None:
        self.data = memoryview(data)
        self.offset = 0
        self.encoding = "utf-8"

    def read(self, size: int) -> bytes:
        end = self.offset + size
        if size < 0 or end > len(self.data):
            raise ValueError(f"Unexpected end of PMX at byte {self.offset}")
        result = self.data[self.offset : end].tobytes()
        self.offset = end
        return result

    def unpack(self, fmt: str):
        size = struct.calcsize(fmt)
        result = struct.unpack_from(fmt, self.data, self.offset)
        self.offset += size
        return result

    def u8(self) -> int:
        return self.unpack("<B")[0]

    def i32(self) -> int:
        return self.unpack("<i")[0]

    def text(self) -> str:
        size = self.i32()
        if size < 0:
            raise ValueError(f"Invalid PMX string size {size}")
        return self.read(size).decode(self.encoding, errors="replace")

    def index(self, size: int, *, signed: bool = True) -> int:
        formats = {
            (1, False): "<B",
            (2, False): "<H",
            (4, False): "<I",
            (1, True): "<b",
            (2, True): "<h",
            (4, True): "<i",
        }
        try:
            return self.unpack(formats[(size, signed)])[0]
        except KeyError as error:
            raise ValueError(f"Unsupported PMX index size {size}") from error

    def indices(self, count: int, size: int) -> list[int]:
        typecodes = {1: "B", 2: "H", 4: "I"}
        try:
            values = array(typecodes[size])
        except KeyError as error:
            raise ValueError(f"Unsupported PMX vertex index size {size}") from error
        values.frombytes(self.read(count * size))
        if sys.byteorder != "little" and size > 1:
            values.byteswap()
        return values.tolist()


def parse_pmx(path: Path) -> PmxModel:
    reader = PmxReader(path.read_bytes())
    if reader.read(4) != b"PMX ":
        raise ValueError(f"{path} is not a PMX file")

    version = reader.unpack("<f")[0]
    header_size = reader.u8()
    header = reader.read(header_size)
    if len(header) < 8:
        raise ValueError("PMX global header is shorter than 8 bytes")

    reader.encoding = "utf-16-le" if header[0] == 0 else "utf-8"
    additional_uv_count = header[1]
    vertex_index_size = header[2]
    texture_index_size = header[3]
    bone_index_size = header[5]

    name = reader.text()
    english_name = reader.text()
    reader.text()  # comment
    reader.text()  # English comment

    vertex_count = reader.i32()
    if vertex_count < 0:
        raise ValueError(f"Invalid PMX vertex count {vertex_count}")

    vertices: list[Vertex] = []
    for _ in range(vertex_count):
        values = reader.unpack("<8f")
        vertices.append(
            Vertex(
                position=(values[0], values[1], values[2]),
                normal=(values[3], values[4], values[5]),
                # PMX uses a top-left texture origin; PBRT UVs use bottom-left.
                uv=(values[6], 1.0 - values[7]),
            )
        )
        reader.read(16 * additional_uv_count)
        deform = reader.u8()
        if deform == 0:  # BDEF1
            reader.read(bone_index_size)
        elif deform == 1:  # BDEF2
            reader.read(2 * bone_index_size + 4)
        elif deform in (2, 4):  # BDEF4 / QDEF
            reader.read(4 * bone_index_size + 16)
        elif deform == 3:  # SDEF
            reader.read(2 * bone_index_size + 40)
        else:
            raise ValueError(f"Unsupported PMX deform type {deform}")
        reader.read(4)  # edge scale

    index_count = reader.i32()
    if index_count < 0 or index_count % 3 != 0:
        raise ValueError(f"Invalid PMX surface index count {index_count}")
    indices = reader.indices(index_count, vertex_index_size)
    if indices and max(indices) >= vertex_count:
        raise ValueError("PMX surface references an out-of-range vertex")

    texture_count = reader.i32()
    if texture_count < 0:
        raise ValueError(f"Invalid PMX texture count {texture_count}")
    textures = [reader.text().replace("\\", "/") for _ in range(texture_count)]

    material_count = reader.i32()
    if material_count < 0:
        raise ValueError(f"Invalid PMX material count {material_count}")
    materials: list[Material] = []
    for _ in range(material_count):
        material_name = reader.text()
        material_english_name = reader.text()
        diffuse = reader.unpack("<4f")
        reader.read(12)  # specular color
        reader.read(4)  # specular strength
        reader.read(12)  # ambient color
        reader.read(1)  # draw flags
        reader.read(16)  # edge color
        reader.read(4)  # edge size
        texture_index = reader.index(texture_index_size)
        reader.index(texture_index_size)  # sphere texture
        reader.read(1)  # sphere mode
        shared_toon = reader.u8()
        if shared_toon == 0:
            reader.index(texture_index_size)
        else:
            reader.read(1)
        reader.text()  # memo
        surface_count = reader.i32()
        if surface_count < 0 or surface_count % 3 != 0:
            raise ValueError(
                f"Material {material_name!r} has invalid surface count {surface_count}"
            )
        materials.append(
            Material(
                name=material_name,
                english_name=material_english_name,
                diffuse=diffuse,
                texture_index=texture_index,
                surface_count=surface_count,
            )
        )

    assigned_index_count = sum(material.surface_count for material in materials)
    if assigned_index_count != index_count:
        raise ValueError(
            "PMX material ranges cover "
            f"{assigned_index_count} indices, but the mesh has {index_count}"
        )

    return PmxModel(
        name=name,
        english_name=english_name,
        version=version,
        vertices=vertices,
        indices=indices,
        textures=textures,
        materials=materials,
    )


def winding_needs_flip(model: PmxModel) -> bool:
    triangle_count = len(model.indices) // 3
    stride = max(1, triangle_count // 20_000)
    score = 0.0
    samples = 0
    for triangle in range(0, triangle_count, stride):
        i0, i1, i2 = model.indices[3 * triangle : 3 * triangle + 3]
        v0, v1, v2 = (
            model.vertices[i0],
            model.vertices[i1],
            model.vertices[i2],
        )
        e1 = tuple(v1.position[i] - v0.position[i] for i in range(3))
        e2 = tuple(v2.position[i] - v0.position[i] for i in range(3))
        cross = (
            e1[1] * e2[2] - e1[2] * e2[1],
            e1[2] * e2[0] - e1[0] * e2[2],
            e1[0] * e2[1] - e1[1] * e2[0],
        )
        normal = tuple(v0.normal[i] + v1.normal[i] + v2.normal[i] for i in range(3))
        cross_length = math.sqrt(sum(value * value for value in cross))
        normal_length = math.sqrt(sum(value * value for value in normal))
        if cross_length > 1e-12 and normal_length > 1e-12:
            score += sum(cross[i] * normal[i] for i in range(3)) / (
                cross_length * normal_length
            )
            samples += 1
    return samples > 0 and score < 0


def write_binary_ply(
    path: Path,
    model: PmxModel,
    global_indices: list[int],
    *,
    flip_winding: bool,
) -> tuple[int, int]:
    remap: dict[int, int] = {}
    used_vertices: list[int] = []
    local_indices: list[int] = []
    for global_index in global_indices:
        local_index = remap.get(global_index)
        if local_index is None:
            local_index = len(used_vertices)
            remap[global_index] = local_index
            used_vertices.append(global_index)
        local_indices.append(local_index)

    if flip_winding:
        for index in range(0, len(local_indices), 3):
            local_indices[index + 1], local_indices[index + 2] = (
                local_indices[index + 2],
                local_indices[index + 1],
            )

    header = "\n".join(
        [
            "ply",
            "format binary_little_endian 1.0",
            "comment generated by tools/pmx_to_pbrt.py",
            f"element vertex {len(used_vertices)}",
            "property float x",
            "property float y",
            "property float z",
            "property float nx",
            "property float ny",
            "property float nz",
            "property float u",
            "property float v",
            f"element face {len(local_indices) // 3}",
            "property list uchar int vertex_indices",
            "end_header",
            "",
        ]
    ).encode("ascii")

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as output:
        output.write(header)
        for global_index in used_vertices:
            vertex = model.vertices[global_index]
            output.write(
                struct.pack("<8f", *vertex.position, *vertex.normal, *vertex.uv)
            )
        for index in range(0, len(local_indices), 3):
            output.write(struct.pack("<B3i", 3, *local_indices[index : index + 3]))
    return len(used_vertices), len(local_indices) // 3


def pbrt_path(path: Path) -> str:
    return path.as_posix().replace('"', '\\"')


def relative_pbrt_path(path: Path, base: Path) -> str:
    return pbrt_path(Path(os.path.relpath(path, base)))


def emit(
    lines: list[str], command: str, params: tuple[str, ...] = (), *, indent: int = 0
) -> None:
    prefix = "    " * indent
    lines.append(prefix + command)
    lines.extend(f"{prefix}    {param}" for param in params)


def create_alpha_mask(texture: Path, target: Path) -> bool:
    try:
        from PIL import Image
    except ImportError:
        return False

    with Image.open(texture) as image:
        if "A" not in image.getbands() and image.mode != "P":
            return False
        alpha = image.convert("RGBA").getchannel("A")
        minimum, maximum = alpha.getextrema()
        if minimum == maximum == 255:
            return False
        target.parent.mkdir(parents=True, exist_ok=True)
        alpha.save(target)
        return True


def model_bounds(
    model: PmxModel,
) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    minimum = tuple(
        min(vertex.position[i] for vertex in model.vertices) for i in range(3)
    )
    maximum = tuple(
        max(vertex.position[i] for vertex in model.vertices) for i in range(3)
    )
    return minimum, maximum


def write_scene(
    scene_path: Path,
    pmx_path: Path,
    model: PmxModel,
    records: list[dict],
    *,
    samples: int,
    x_resolution: int,
    y_resolution: int,
    camera_side: str,
) -> None:
    minimum, maximum = model_bounds(model)
    height = maximum[1] - minimum[1]
    if height <= 0:
        raise ValueError("Model has zero height")
    center = tuple((minimum[i] + maximum[i]) * 0.5 for i in range(3))
    target = (center[0], minimum[1] + 0.52 * height, center[2])
    camera_sign = -1.0 if camera_side == "negative-z" else 1.0
    eye = (
        center[0],
        minimum[1] + 0.54 * height,
        center[2] + camera_sign * 1.9 * height,
    )
    ground_size = max(maximum[0] - minimum[0], height) * 1.2
    ground_y = minimum[1] - 0.002 * height

    lines = [
        "# Generated from PMX by tools/pmx_to_pbrt.py",
        f"# Model: {model.name}",
    ]
    emit(lines, 'Integrator "path"', ('"integer maxdepth" [ 8 ]',))
    emit(lines, 'Sampler "sobol"', (f'"integer pixelsamples" [ {samples} ]',))
    emit(
        lines,
        'PixelFilter "triangle"',
        ('"float xradius" [ 1.5 ]', '"float yradius" [ 1.5 ]'),
    )
    emit(
        lines,
        'Film "rgb"',
        (
            f'"string filename" [ "{pmx_path.stem}.exr" ]',
            f'"integer xresolution" [ {x_resolution} ]',
            f'"integer yresolution" [ {y_resolution} ]',
        ),
    )
    emit(lines, "LookAt " + " ".join(f"{value:.8g}" for value in eye))
    emit(lines, " ".join(f"{value:.8g}" for value in target), indent=1)
    emit(lines, "0 1 0", indent=1)
    emit(lines, 'Camera "perspective"', ('"float fov" [ 30 ]',))
    lines.extend(["", "WorldBegin", ""])

    emit(
        lines,
        'LightSource "infinite"',
        ('"rgb L" [ 0.8 0.9 1.0 ]', '"float illuminance" [ 0.05 ]'),
    )
    emit(
        lines,
        'LightSource "distant"',
        (
            '"rgb L" [ 1.0 0.88 0.74 ]',
            (
                f'"point3 from" [ {center[0] - height:.8g} {maximum[1] + height:.8g} '
                f"{center[2] + camera_sign * height:.8g} ]"
            ),
            f'"point3 to" [ {target[0]:.8g} {target[1]:.8g} {target[2]:.8g} ]',
            '"float illuminance" [ 5 ]',
        ),
    )
    emit(
        lines,
        'LightSource "distant"',
        (
            '"rgb L" [ 0.62 0.76 1.0 ]',
            (
                f'"point3 from" [ {center[0] + height:.8g} {target[1]:.8g} '
                f"{center[2] + camera_sign * 0.5 * height:.8g} ]"
            ),
            f'"point3 to" [ {target[0]:.8g} {target[1]:.8g} {target[2]:.8g} ]',
            '"float illuminance" [ 1 ]',
        ),
    )
    lines.append("")

    texture_names: dict[int, str] = {}
    for record in records:
        texture_index = record["texture_index"]
        texture_path = record.get("texture_path")
        if texture_path and texture_index not in texture_names:
            texture_name = f"pmx_texture_{texture_index:03d}"
            texture_names[texture_index] = texture_name
            emit(
                lines,
                f'Texture "{texture_name}" "spectrum" "imagemap"',
                (
                    f'"string filename" [ "{texture_path}" ]',
                    '"string filter" [ "bilinear" ]',
                    '"string encoding" [ "sRGB" ]',
                ),
            )
            lines.append("")

    for record in records:
        index = record["index"]
        diffuse = record["diffuse"]
        material_name = f"pmx_material_{index:03d}"
        lines.append(f"# PMX material {index}: {record['name']}")
        params = ['"string type" [ "diffuse" ]']
        texture_name = texture_names.get(record["texture_index"])
        if texture_name:
            params.append(f'"texture reflectance" [ "{texture_name}" ]')
        else:
            rgb = " ".join(f"{max(0.0, min(1.0, value)):.6g}" for value in diffuse[:3])
            params.append(f'"rgb reflectance" [ {rgb} ]')
        emit(lines, f'MakeNamedMaterial "{material_name}"', tuple(params))
        lines.append("")

    for record in records:
        index = record["index"]
        lines.append("AttributeBegin")
        alpha_path = record.get("alpha_path")
        if alpha_path:
            alpha_name = f"pmx_alpha_{index:03d}"
            # PBRT GPU enables sRGB decoding for PNG float textures as well.
            emit(
                lines,
                f'Texture "{alpha_name}" "float" "imagemap"',
                (
                    f'"string filename" [ "{alpha_path}" ]',
                    '"string filter" [ "bilinear" ]',
                    '"string encoding" [ "sRGB" ]',
                    f'"float scale" [ {diffuse_alpha(record):.8g} ]',
                ),
                indent=1,
            )
        emit(lines, f'NamedMaterial "pmx_material_{index:03d}"', indent=1)
        shape_params = [f'"string filename" [ "{record["ply_path"]}" ]']
        if alpha_path:
            shape_params.append(f'"texture alpha" [ "{alpha_name}" ]')
        elif diffuse_alpha(record) < 0.999:
            shape_params.append(f'"float alpha" [ {diffuse_alpha(record):.8g} ]')
        emit(lines, 'Shape "plymesh"', tuple(shape_params), indent=1)
        lines.extend(["AttributeEnd", ""])

    x0, x1 = center[0] - ground_size, center[0] + ground_size
    z0, z1 = center[2] - ground_size, center[2] + ground_size
    lines.append("AttributeBegin")
    emit(
        lines,
        'Material "diffuse"',
        ('"rgb reflectance" [ 0.16 0.18 0.22 ]',),
        indent=1,
    )
    emit(
        lines,
        'Shape "trianglemesh"',
        (
            (
                f'"point3 P" [ {x0:.8g} {ground_y:.8g} {z0:.8g}  '
                f"{x1:.8g} {ground_y:.8g} {z0:.8g}  {x1:.8g} {ground_y:.8g} {z1:.8g}  "
                f"{x0:.8g} {ground_y:.8g} {z1:.8g} ]"
            ),
            '"integer indices" [ 0 2 1  0 3 2 ]',
        ),
        indent=1,
    )
    lines.append("AttributeEnd")

    scene_path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def diffuse_alpha(record: dict) -> float:
    return max(0.0, min(1.0, float(record["diffuse"][3])))


def convert(args: argparse.Namespace) -> None:
    pmx_path = args.pmx.resolve()
    output_dir = (args.output or pmx_path.parent / "pbrt").resolve()
    mesh_dir = output_dir / "meshes"
    alpha_dir = output_dir / "alpha"
    scene_path = output_dir / f"{pmx_path.stem}.pbrt"
    manifest_path = output_dir / "materials.json"
    output_dir.mkdir(parents=True, exist_ok=True)

    model = parse_pmx(pmx_path)
    flip_winding = winding_needs_flip(model)
    records: list[dict] = []
    surface_offset = 0
    for material_index, material in enumerate(model.materials):
        surface_end = surface_offset + material.surface_count
        material_indices = model.indices[surface_offset:surface_end]
        surface_offset = surface_end
        if not material_indices:
            continue

        ply_file = mesh_dir / f"material_{material_index:03d}.ply"
        vertex_count, triangle_count = write_binary_ply(
            ply_file,
            model,
            material_indices,
            flip_winding=flip_winding,
        )

        texture_path: Path | None = None
        texture_reference: str | None = None
        alpha_reference: str | None = None
        if 0 <= material.texture_index < len(model.textures):
            texture_path = pmx_path.parent / Path(
                model.textures[material.texture_index]
            )
            if texture_path.is_file():
                texture_reference = relative_pbrt_path(texture_path, output_dir)
                alpha_file = alpha_dir / f"texture_{material.texture_index:03d}.png"
                if create_alpha_mask(texture_path, alpha_file):
                    alpha_reference = relative_pbrt_path(alpha_file, output_dir)
            else:
                print(f"warning: missing texture: {texture_path}", file=sys.stderr)

        records.append(
            {
                "index": material_index,
                "name": material.name,
                "english_name": material.english_name,
                "diffuse": list(material.diffuse),
                "texture_index": material.texture_index,
                "texture": (
                    model.textures[material.texture_index]
                    if 0 <= material.texture_index < len(model.textures)
                    else None
                ),
                "texture_path": texture_reference,
                "alpha_path": alpha_reference,
                "ply_path": pbrt_path(ply_file.relative_to(output_dir)),
                "vertices": vertex_count,
                "triangles": triangle_count,
            }
        )

    manifest = {
        "source": str(pmx_path),
        "pmx_version": model.version,
        "model_name": model.name,
        "model_english_name": model.english_name,
        "vertex_count": len(model.vertices),
        "triangle_count": len(model.indices) // 3,
        "winding_flipped": flip_winding,
        "materials": records,
    }
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    write_scene(
        scene_path,
        pmx_path,
        model,
        records,
        samples=args.samples,
        x_resolution=args.x_resolution,
        y_resolution=args.y_resolution,
        camera_side=args.camera_side,
    )

    print(
        f"PMX: {model.name} ({len(model.vertices)} vertices, {len(model.indices) // 3} triangles)"
    )
    print(f"PLY meshes: {len(records)} -> {mesh_dir}")
    print(f"Manifest: {manifest_path}")
    print(f"PBRT scene: {scene_path}")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Split a static PMX model by material and create a PBRT-v4 scene."
    )
    parser.add_argument("pmx", type=Path, help="input .pmx file")
    parser.add_argument(
        "-o", "--output", type=Path, help="output directory (default: PMX/pbrt)"
    )
    parser.add_argument(
        "--samples", type=int, default=64, help="PBRT samples per pixel"
    )
    parser.add_argument("--x-resolution", type=int, default=640)
    parser.add_argument("--y-resolution", type=int, default=900)
    parser.add_argument(
        "--camera-side",
        choices=("negative-z", "positive-z"),
        default="negative-z",
        help="side of the model used for the generated camera",
    )
    return parser.parse_args()


if __name__ == "__main__":
    convert(parse_arguments())
