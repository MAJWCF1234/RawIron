#!/usr/bin/env python3
"""Export readable Unity Mesh .asset YAML files to OBJ for RawIron Forest Ruins."""

from __future__ import annotations

import json
import os
import re
import struct
import sys
from pathlib import Path


def workspace_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_mesh_source() -> Path:
    return workspace_root() / "Assets" / "Source" / "Forest Scene Assets" / "UnityOnly" / "Assets" / "Conifers [BOTD]" / "Sources" / "Meshes"


def default_output() -> Path:
    return workspace_root() / "Games" / "WildernessRuins" / "Assets" / "Generated" / "ForestScene" / "Meshes"


def default_texture_sources() -> list[Path]:
    root = workspace_root()
    return [
        root / "Assets" / "Source" / "Forest Scene" / "Assets" / "Assets" / "Conifers [BOTD]" / "Sources" / "Shared Textures",
        root / "Assets" / "Source" / "Forest Scene Assets" / "Textures" / "Assets" / "Conifers [BOTD]" / "Sources" / "Shared Textures",
        root / "Assets" / "Source" / "Forest Scene Assets" / "UnityOnly" / "Assets" / "Conifers [BOTD]" / "Sources" / "Shared Textures",
    ]


def parse_channels(text: str) -> tuple[int, int, int]:
    """Return vertex stride, position offset, uv offset."""
    channels: list[tuple[int, int]] = []
    for match in re.finditer(
        r"- stream: \d+\s+offset: (\d+)\s+format: \d+\s+dimension: (\d+)",
        text,
    ):
        offset = int(match.group(1))
        dimension = int(match.group(2))
        if dimension > 0:
            channels.append((offset, dimension))

    if not channels:
        raise ValueError("Mesh has no vertex channels.")

    stride = max(offset + dimension * 4 for offset, dimension in channels)
    position_offset = 0
    uv_offset = 56
    for offset, dimension in channels:
        if dimension == 3 and offset == 0:
            position_offset = offset
        if dimension == 2 and offset >= 56:
            uv_offset = offset
            break
    return stride, position_offset, uv_offset


def parse_submeshes(text: str) -> list[dict[str, int]]:
    blocks = re.findall(
        r"- serializedVersion: 2\s+firstByte: (\d+)\s+indexCount: (\d+)\s+topology: \d+\s+baseVertex: (\d+)\s+firstVertex: (\d+)\s+vertexCount: (\d+)",
        text,
    )
    return [
        {
            "first_byte": int(first_byte),
            "index_count": int(index_count),
            "base_vertex": int(base_vertex),
            "first_vertex": int(first_vertex),
            "vertex_count": int(vertex_count),
        }
        for first_byte, index_count, base_vertex, first_vertex, vertex_count in blocks
    ]


def parse_mesh_asset(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace")
    name_match = re.search(r"m_Name: (.+)", text)
    if not name_match:
        raise ValueError(f"Missing m_Name in {path}")
    vertex_count_match = re.search(r"m_VertexCount: (\d+)", text)
    if not vertex_count_match:
        raise ValueError(f"Missing m_VertexCount in {path}")
    index_match = re.search(r"m_IndexBuffer: ([0-9a-fA-F]+)", text)
    data_match = re.search(r"_typelessdata: ([0-9a-fA-F]+)", text)
    if not index_match or not data_match:
        raise ValueError(f"Missing index or vertex data in {path}")

    stride, position_offset, uv_offset = parse_channels(text)
    submeshes = parse_submeshes(text)
    if not submeshes:
        raise ValueError(f"Missing submesh table in {path}")

    return {
        "name": name_match.group(1).strip(),
        "vertex_count": int(vertex_count_match.group(1)),
        "stride": stride,
        "position_offset": position_offset,
        "uv_offset": uv_offset,
        "index_buffer": bytes.fromhex(index_match.group(1)),
        "vertex_data": bytes.fromhex(data_match.group(1)),
        "submeshes": submeshes,
    }


def read_float(data: bytes, offset: int) -> float:
    return struct.unpack_from("<f", data, offset)[0]


def find_texture_in_dir(texture_dir: Path, needles: list[str]) -> Path | None:
    if not texture_dir.is_dir():
        return None
    for entry in texture_dir.iterdir():
        if not entry.is_file():
            continue
        lower = entry.name.lower()
        if all(needle in lower for needle in needles):
            return entry
    return None


def write_mtl(mtl_path: Path, trunk_texture: Path | None, branch_texture: Path | None) -> None:
    lines = ["# RawIron BOTD YAML mesh export"]
    if trunk_texture is not None:
        rel = os.path.relpath(trunk_texture.resolve(), mtl_path.parent.resolve()).replace("\\", "/")
        lines.append("newmtl Trunk")
        lines.append(f"map_Kd {rel}")
    if branch_texture is not None:
        rel = os.path.relpath(branch_texture.resolve(), mtl_path.parent.resolve()).replace("\\", "/")
        lines.append("newmtl Branches")
        lines.append(f"map_Kd {rel}")
        normal = find_texture_in_dir(branch_texture.parent, ["branch", "normal"])
        if normal is not None:
            normal_rel = os.path.relpath(normal.resolve(), mtl_path.parent.resolve()).replace("\\", "/")
            lines.append(f"map_Bump {normal_rel}")
    mtl_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def submesh_material_name(submesh_index: int, submesh_count: int) -> str:
    if submesh_count <= 1:
        return "Branches"
    return "Trunk" if submesh_index == 0 else "Branches"


def export_obj(mesh: dict, output_path: Path, texture_dir: Path) -> None:
    stride = mesh["stride"]
    position_offset = mesh["position_offset"]
    uv_offset = mesh["uv_offset"]
    vertex_data = mesh["vertex_data"]
    vertex_count = mesh["vertex_count"]

    positions: list[tuple[float, float, float]] = []
    uvs: list[tuple[float, float]] = []
    for vertex_index in range(vertex_count):
        base = vertex_index * stride
        if base + position_offset + 12 > len(vertex_data):
            break
        x = read_float(vertex_data, base + position_offset)
        y = read_float(vertex_data, base + position_offset + 4)
        z = read_float(vertex_data, base + position_offset + 8)
        positions.append((x, y, z))
        if base + uv_offset + 8 <= len(vertex_data):
            u = read_float(vertex_data, base + uv_offset)
            v = read_float(vertex_data, base + uv_offset + 4)
            uvs.append((u, v))
        else:
            uvs.append((0.0, 0.0))

    indices = [
        int.from_bytes(mesh["index_buffer"][index : index + 2], "little")
        for index in range(0, len(mesh["index_buffer"]) - 1, 2)
    ]

    mtl_name = output_path.stem + ".mtl"
    trunk_texture = find_texture_in_dir(texture_dir, ["trunk"])
    branch_texture = find_texture_in_dir(texture_dir, ["branch", "albedo"])
    if branch_texture is None:
        branch_texture = find_texture_in_dir(texture_dir, ["branch"])
    write_mtl(output_path.with_suffix(".mtl"), trunk_texture, branch_texture)

    lines: list[str] = [
        "# RawIron BOTD YAML mesh export",
        f"mtllib {mtl_name}",
        f"o {mesh['name']}",
    ]
    for x, y, z in positions:
        lines.append(f"v {x:.9g} {y:.9g} {z:.9g}")
    for u, v in uvs:
        lines.append(f"vt {u:.9g} {v:.9g}")

    submesh_count = len(mesh["submeshes"])
    for submesh_index, submesh in enumerate(mesh["submeshes"]):
        lines.append(f"usemtl {submesh_material_name(submesh_index, submesh_count)}")
        start = submesh["first_byte"] // 2
        end = start + submesh["index_count"]
        sub_indices = indices[start:end]
        for triangle in range(0, len(sub_indices) - 2, 3):
            a = sub_indices[triangle] + 1
            b = sub_indices[triangle + 1] + 1
            c = sub_indices[triangle + 2] + 1
            lines.append(f"f {a}/{a} {b}/{b} {c}/{c}")

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def sanitize_name(value: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9._-]+", "_", value.strip())
    return cleaned or "mesh"


def copy_texture_assets(output_root: Path) -> list[str]:
    texture_dir = output_root.parent / "Textures"
    texture_dir.mkdir(parents=True, exist_ok=True)
    copied: list[str] = []
    for source_dir in default_texture_sources():
        if not source_dir.is_dir():
            continue
        for entry in source_dir.iterdir():
            if not entry.is_file():
                continue
            lower = entry.name.lower()
            if not (lower.endswith(".tif") or lower.endswith(".tga") or lower.endswith(".png")):
                continue
            if "albedo" not in lower and "normal" not in lower and "bark" not in lower and "branch" not in lower:
                continue
            target = texture_dir / entry.name
            if target.exists():
                continue
            target.write_bytes(entry.read_bytes())
            copied.append(str(target))
    return copied


def ensure_png_preview_assets(texture_dir: Path) -> list[str]:
    """RawIron preview loads PNG/JPEG via stb_image; convert BOTD TIF/TGA siblings on export."""
    try:
        from PIL import Image
    except ImportError:
        print("Pillow not installed; skipping TIF/TGA -> PNG conversion.")
        return []

    converted: list[str] = []
    for entry in sorted(texture_dir.iterdir()):
        if not entry.is_file():
            continue
        lower_suffix = entry.suffix.lower()
        if lower_suffix not in {".tif", ".tga"}:
            continue
        png_path = entry.with_suffix(".png")
        if png_path.exists():
            continue
        Image.open(entry).convert("RGBA").save(png_path, "PNG")
        converted.append(str(png_path))
        print(f"Converted {entry.name} -> {png_path.name}")
    return converted


def main() -> int:
    source_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else default_mesh_source()
    output_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else default_output()
    output_dir.mkdir(parents=True, exist_ok=True)

    exports: list[dict[str, str]] = []
    asset_files = sorted(source_dir.glob("*.asset"))
    if not asset_files:
        print(f"No .asset meshes found under {source_dir}")
        return 1

    for asset_path in asset_files:
        try:
            mesh = parse_mesh_asset(asset_path)
            obj_name = sanitize_name(mesh["name"]) + ".obj"
            obj_path = output_dir / obj_name
            texture_dir = output_dir.parent / "Textures"
            export_obj(mesh, obj_path, texture_dir)
            exports.append(
                {
                    "label": mesh["name"],
                    "kind": "mesh-asset-yaml",
                    "source": str(asset_path).replace("\\", "/"),
                    "obj": str(obj_path).replace("\\", "/"),
                }
            )
            print(f"Exported {mesh['name']} -> {obj_path}")
        except Exception as exc:  # noqa: BLE001
            print(f"Skipped {asset_path.name}: {exc}")

    copied_textures = copy_texture_assets(output_dir)
    texture_dir = output_dir.parent / "Textures"
    png_previews = ensure_png_preview_assets(texture_dir) if texture_dir.is_dir() else []
    manifest = {
        "sourceProject": "Forest Scene Assets",
        "exporter": "export_botd_unity_yaml_meshes.py",
        "exports": exports,
        "textures": copied_textures,
        "pngPreviews": png_previews,
    }
    manifest_path = output_dir / "export-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(exports)} OBJ file(s) to {output_dir}")
    print(f"Copied {len(copied_textures)} texture file(s)")
    print(f"Manifest: {manifest_path}")
    return 0 if exports else 1


if __name__ == "__main__":
    raise SystemExit(main())
