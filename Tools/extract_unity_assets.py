import argparse
import hashlib
import json
import os
import shutil
from dataclasses import dataclass
from pathlib import Path
import re


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


@dataclass(frozen=True)
class Rule:
    category: str
    convert_to_png: bool = False


def classify(path: Path) -> Rule:
    ext = path.suffix.lower()
    if ext in {".fbx", ".obj", ".dae", ".blend", ".gltf", ".glb"}:
        return Rule("Models")
    if ext in {".png", ".jpg", ".jpeg", ".tga", ".tif", ".tiff", ".bmp", ".gif"}:
        return Rule("Textures", convert_to_png=ext in {".tga", ".tif", ".tiff", ".bmp", ".gif"})
    if ext in {".wav", ".mp3", ".ogg", ".flac", ".aiff", ".aif"}:
        return Rule("Audio")
    if ext in {".shader", ".cginc", ".hlsl", ".glsl"}:
        return Rule("Shaders")
    if ext in {".mat", ".prefab", ".unity", ".asset", ".controller", ".anim", ".physicmaterial"}:
        return Rule("UnityOnly")
    return Rule("Other")


def safe_relpath(path: Path, root: Path) -> Path:
    rel = path.relative_to(root)
    # Avoid weird things like ".." from sneaking in (shouldn't with relative_to).
    return Path(*[p for p in rel.parts if p not in {"", ".", ".."}])


def copy_file(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def convert_texture_to_png(src: Path, dst_png: Path) -> bool:
    try:
        from PIL import Image  # type: ignore
    except Exception:
        return False

    try:
        dst_png.parent.mkdir(parents=True, exist_ok=True)
        with Image.open(src) as im:
            # Preserve alpha when present; avoid palette pitfalls.
            if im.mode in {"P"}:
                im = im.convert("RGBA")
            elif im.mode in {"LA"}:
                im = im.convert("RGBA")
            elif im.mode not in {"RGB", "RGBA"}:
                im = im.convert("RGBA")
            im.save(dst_png, format="PNG", optimize=True)
        return True
    except Exception:
        return False


def iter_files(root: Path):
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        # Skip Unity metadata and build artifacts.
        if path.suffix.lower() in {".csproj", ".sln", ".user", ".cache"}:
            continue
        yield path


_GUID_RE = re.compile(r"^\s*guid:\s*([0-9a-fA-F]{32})\s*$")
_REF_GUID_RE = re.compile(r"guid:\s*([0-9a-fA-F]{32})")


def try_parse_meta_guid(meta_path: Path) -> str | None:
    try:
        for line in meta_path.read_text(encoding="utf-8", errors="ignore").splitlines():
            m = _GUID_RE.match(line)
            if m:
                return m.group(1).lower()
    except Exception:
        return None
    return None


def extract_referenced_guids(text_path: Path) -> set[str]:
    try:
        text = text_path.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        return set()
    return {m.group(1).lower() for m in _REF_GUID_RE.finditer(text)}


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Extract usable source assets from a Unity project into an engine-agnostic folder."
    )
    parser.add_argument("--project", required=True, help="Path to Unity project root (folder containing Assets/).")
    parser.add_argument("--out", required=True, help="Output folder to write extracted assets.")
    parser.add_argument(
        "--copy-unity-only",
        action="store_true",
        help="Also copy Unity-only files (.prefab/.mat/.asset/.unity) into UnityOnly/ for reference.",
    )
    parser.add_argument(
        "--include-meta",
        action="store_true",
        help="Also copy Unity .meta files and build a guid_index.json for resolving prefab/material references.",
    )
    parser.add_argument(
        "--write-hashes",
        action="store_true",
        help="Compute SHA-256 for each copied file (slower, but helps dedupe/verification).",
    )
    args = parser.parse_args()

    project_root = Path(args.project).resolve()
    assets_root = project_root / "Assets"
    out_root = Path(args.out).resolve()

    if not assets_root.exists():
        raise SystemExit(f"Assets folder not found: {assets_root}")

    out_root.mkdir(parents=True, exist_ok=True)

    manifest = {
        "project_root": str(project_root),
        "assets_root": str(assets_root),
        "output_root": str(out_root),
        "files": [],
        "notes": [
            "Unity-only assets are copied only when --copy-unity-only is set.",
            "Unity .meta files are copied only when --include-meta is set.",
            "Textures in TGA/TIF/TIFF/BMP/GIF are additionally converted to PNG when possible.",
        ],
    }

    guid_index: dict[str, str] = {}
    # Build GUID index first (so we can validate references during copy).
    if args.include_meta:
        for meta in assets_root.rglob("*.meta"):
            guid = try_parse_meta_guid(meta)
            if not guid:
                continue
            # meta file corresponds to asset path without ".meta"
            asset_path = meta.with_suffix("")
            if asset_path.exists():
                rel_asset = safe_relpath(asset_path, assets_root).as_posix()
                guid_index[guid] = rel_asset

    missing_guid_refs: dict[str, list[str]] = {}

    for src in iter_files(assets_root):
        rule = classify(src)
        rel = safe_relpath(src, assets_root)

        if src.suffix.lower() == ".meta":
            if not args.include_meta:
                continue
            # Copy .meta next to its corresponding category bucket as reference-only.
            dst = out_root / "Meta" / rel
            copy_file(src, dst)
            entry = {"category": "Meta", "src": str(src), "dst": str(dst)}
            if args.write_hashes:
                entry["sha256"] = sha256_file(dst)
            manifest["files"].append(entry)
            continue

        if rule.category == "UnityOnly" and not args.copy_unity_only:
            continue

        dst = out_root / rule.category / rel
        copy_file(src, dst)

        entry = {
            "category": rule.category,
            "src": str(src),
            "dst": str(dst),
        }
        if args.write_hashes:
            entry["sha256"] = sha256_file(dst)

        if rule.category == "Textures" and rule.convert_to_png:
            dst_png = (out_root / "TexturesPNG" / rel).with_suffix(".png")
            converted = convert_texture_to_png(src, dst_png)
            entry["png"] = str(dst_png) if converted else None
            if converted and args.write_hashes:
                entry["png_sha256"] = sha256_file(dst_png)

        manifest["files"].append(entry)

        # Validate referenced GUIDs in Unity YAML-ish files.
        if args.include_meta and rule.category == "UnityOnly" and src.suffix.lower() in {".prefab", ".unity", ".mat", ".asset", ".controller", ".anim"}:
            refs = extract_referenced_guids(src)
            if refs:
                unresolved = sorted([g for g in refs if g not in guid_index])
                if unresolved:
                    missing_guid_refs[str(rel)] = unresolved[:200]

    manifest_path = out_root / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    if args.include_meta:
        (out_root / "guid_index.json").write_text(json.dumps(guid_index, indent=2), encoding="utf-8")
        (out_root / "missing_guid_refs.json").write_text(json.dumps(missing_guid_refs, indent=2), encoding="utf-8")
    print(f"Wrote: {manifest_path}")
    print(f"Files: {len(manifest['files'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
