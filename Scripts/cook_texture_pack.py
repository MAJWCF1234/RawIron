#!/usr/bin/env python3
"""Cook a loose PNG tree into a deterministic, deduplicated Raw Iron .ripak."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import zipfile

from PIL import Image


ZIP_EPOCH = (1980, 1, 1, 0, 0, 0)
FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211


def fnv1a64(data: bytes) -> str:
    value = FNV_OFFSET
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return f"fnv1a64:{value:016x}"


def write_json(path: Path, value: object, *, compact: bool = False) -> bytes:
    encoded = (
        json.dumps(
            value,
            ensure_ascii=False,
            indent=None if compact else 2,
            separators=(",", ":") if compact else None,
        )
        + "\n"
    ).encode("utf-8")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)
    return encoded


def safe_relative(path: Path, root: Path) -> str:
    relative = path.relative_to(root).as_posix()
    if not relative or relative.startswith("/") or any(part in ("", ".", "..") for part in relative.split("/")):
        raise RuntimeError(f"unsafe package-relative path: {relative!r}")
    return relative


def zip_info(name: str) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, ZIP_EPOCH)
    info.compress_type = zipfile.ZIP_STORED
    info.create_system = 3
    info.external_attr = 0o100644 << 16
    return info


def cook(source: Path, output: Path, inventory_output: Path, display_id: str, force: bool = False) -> None:
    source = source.resolve(strict=True)
    output = output.resolve(strict=False)
    inventory_output = inventory_output.resolve(strict=False)
    if not source.is_dir():
        raise RuntimeError(f"source is not a directory: {source}")
    if output.suffix.lower() != ".ripak":
        raise RuntimeError("output must use the .ripak extension")
    if not force and (output.exists() or inventory_output.exists()):
        raise RuntimeError("refusing to overwrite an existing pack or inventory")

    files = sorted(path for path in source.rglob("*") if path.is_file())
    if not files:
        raise RuntimeError("refusing to cook an empty texture directory")
    for path in files:
        if path.is_symlink():
            raise RuntimeError(f"source links are not allowed: {path}")
        if path.suffix.lower() != ".png":
            raise RuntimeError(f"RAWIRONX32 accepts PNG sources only: {path}")

    package_id = "".join(character.lower() if character.isalnum() else "_" for character in display_id).strip("_")
    if not package_id:
        raise RuntimeError("package id is empty after normalization")

    with tempfile.TemporaryDirectory(prefix="rawiron-texture-cook-") as temp_name:
        staging = Path(temp_name)
        blobs: dict[str, dict[str, object]] = {}
        source_entries: list[dict[str, object]] = []
        source_bytes = 0

        for path in files:
            relative = safe_relative(path, source)
            data = path.read_bytes()
            digest = hashlib.sha256(data).hexdigest()
            blob_relative = f"content/textures/{digest[:2]}/{digest}.png"
            source_bytes += len(data)

            with Image.open(path) as image:
                width, height = image.size
                mode = image.mode
                image.verify()

            if digest not in blobs:
                blob_path = staging / blob_relative
                blob_path.parent.mkdir(parents=True, exist_ok=True)
                blob_path.write_bytes(data)
                blobs[digest] = {
                    "path": blob_relative,
                    "sizeBytes": len(data),
                    "signature": fnv1a64(data),
                }

            source_entries.append(
                {
                    "source": relative,
                    "blob": blob_relative,
                    "sha256": digest,
                    "sizeBytes": len(data),
                    "width": width,
                    "height": height,
                    "mode": mode,
                }
            )

        index = {
            "formatVersion": 1,
            "packageId": package_id,
            "mandatoryCooked": True,
            "sourceFileCount": len(source_entries),
            "uniqueBlobCount": len(blobs),
            "sourceBytes": source_bytes,
            "uniqueBlobBytes": sum(int(blob["sizeBytes"]) for blob in blobs.values()),
            "entries": source_entries,
        }
        index_relative = f"indexes/{display_id}.index.json"
        # This is a runtime index, not authoring text. Compact JSON avoids permanently carrying
        # indentation for thousands of rows while remaining inspectable and deterministic.
        write_json(staging / index_relative, index, compact=True)

        catalog = {
            "formatVersion": 1,
            "id": f"{package_id}.catalog",
            "type": "texture-library",
            "displayName": f"{display_id} texture catalog",
            "sourcePath": index_relative,
            "references": [
                {
                    "kind": "cooked-index",
                    "id": f"{package_id}.index",
                    "path": index_relative,
                }
            ],
            "payload": {
                "mandatoryCooked": True,
                "sourceFileCount": len(source_entries),
                "uniqueBlobCount": len(blobs),
            },
        }
        catalog_relative = f"assets/{package_id}.catalog.ri_asset.json"
        catalog_bytes = write_json(staging / catalog_relative, catalog)

        manifest = {
            "formatVersion": 2,
            "packageId": package_id,
            "displayName": display_id,
            "packageKind": "content",
            "packageVersion": "1.0.0",
            "author": "",
            "description": "Mandatory cooked Raw Iron texture library.",
            "installScope": "mounted",
            "mountPoint": f"Packages/{package_id}",
            "sourceRoot": source.as_posix(),
            "generatedAtUtc": "1980-01-01T00:00:00Z",
            "engineApiRequirement": "*",
            "supportedPlatforms": [],
            "tags": ["cooked", "textures", "mandatory", "deduplicated"],
            "providesCapabilities": ["assets.textures"],
            "requiredCapabilities": [],
            "permissions": [],
            "dependencies": [],
            "conflicts": [],
            "runtime": {"executionMode": "data", "entryPoint": "", "abiVersion": 0},
            "assets": [
                {
                    "id": catalog["id"],
                    "type": catalog["type"],
                    "path": catalog_relative,
                    "installPath": "",
                    "sourcePath": index_relative,
                    "sizeBytes": len(catalog_bytes),
                    "signature": fnv1a64(catalog_bytes),
                }
            ],
        }
        write_json(staging / "package.ri_package.json", manifest)

        archive_paths = sorted(
            safe_relative(path, staging)
            for path in staging.rglob("*")
            if path.is_file()
        )
        inventory_name = f"{display_id}.files.txt"
        inventory_lines = [
            "# RawIron cooked texture pack inventory v1",
            f"package={display_id}",
            f"package_id={package_id}",
            f"source={source.as_posix()}",
            f"source_files={len(source_entries)}",
            f"unique_blobs={len(blobs)}",
            f"source_bytes={source_bytes}",
            f"unique_blob_bytes={index['uniqueBlobBytes']}",
            f"archive_files={len(archive_paths)}",
            "",
            "# Every file stored in the archive",
            *archive_paths,
            "",
            "# Original source path -> cooked blob",
            *(f"{entry['source']} -> {entry['blob']}" for entry in source_entries),
            "",
        ]
        inventory_bytes = "\n".join(inventory_lines).encode("utf-8")
        output.parent.mkdir(parents=True, exist_ok=True)
        inventory_output.parent.mkdir(parents=True, exist_ok=True)
        output_work = output.with_name(f".{output.name}.cooking")
        inventory_work = inventory_output.with_name(f".{inventory_output.name}.cooking")
        if output_work.exists() or inventory_work.exists():
            raise RuntimeError("stale cooker publish file exists; refusing an ambiguous replacement")
        try:
            with zipfile.ZipFile(output_work, "x", allowZip64=False) as archive:
                for relative in sorted(archive_paths):
                    archive.writestr(zip_info(relative), (staging / relative).read_bytes())

            with zipfile.ZipFile(output_work, "r") as archive:
                if archive.testzip() is not None:
                    raise RuntimeError("post-build ZIP CRC validation failed")
                if sorted(archive.namelist()) != sorted(archive_paths):
                    raise RuntimeError("post-build archive inventory mismatch")

            inventory_work.write_bytes(inventory_bytes)
            inventory_work.replace(inventory_output)
            output_work.replace(output)
        finally:
            output_work.unlink(missing_ok=True)
            inventory_work.unlink(missing_ok=True)

        print(f"package={display_id}")
        print(f"source_files={len(source_entries)}")
        print(f"unique_blobs={len(blobs)}")
        print(f"deduplicated_files={len(source_entries) - len(blobs)}")
        print(f"source_bytes={source_bytes}")
        print(f"unique_blob_bytes={index['uniqueBlobBytes']}")
        print(f"archive_bytes={output.stat().st_size}")
        print(f"archive={output}")
        print(f"inventory={inventory_output}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--inventory", required=True, type=Path)
    parser.add_argument("--package-id", required=True)
    parser.add_argument("--force", action="store_true", help="atomically replace generated outputs")
    args = parser.parse_args()
    cook(args.source, args.output, args.inventory, args.package_id, args.force)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
