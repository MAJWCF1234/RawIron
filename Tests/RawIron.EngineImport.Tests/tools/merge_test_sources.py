#!/usr/bin/env python3
"""Merge src/Test*.cpp into a single EngineImportTests.cpp (unique helper namespaces per file)."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def detail_namespace_from_filename(stem: str) -> str:
    if stem.startswith("Test"):
        return "detail_" + stem[4:]
    return "detail_" + stem


def extract_includes_and_body(text: str) -> tuple[list[str], str]:
    lines = text.splitlines(keepends=True)
    include_lines: list[str] = []
    rest_start = 0
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("#include"):
            include_lines.append(line.rstrip("\n"))
            rest_start = i + 1
        elif stripped == "" or stripped.startswith("//"):
            if not include_lines:
                rest_start = i + 1
            continue
        else:
            break
    body = "".join(lines[rest_start:])
    return include_lines, body


def transform_file_body(body: str, detail_ns: str, stem: str) -> str:
    """Rename first anonymous namespace to detail_ns; inject using-namespace into primary Test<Stem>() only."""
    idx = body.find("namespace {")
    if idx < 0:
        return body

    before = body[:idx]
    after_open = body[idx + len("namespace {") :]

    depth = 1
    pos = 0
    close_idx = -1
    while pos < len(after_open):
        c = after_open[pos]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                close_idx = pos
                break
        pos += 1
    if close_idx < 0:
        raise RuntimeError("unbalanced braces in anonymous namespace")

    inner = after_open[:close_idx]
    tail = after_open[close_idx + 1 :].lstrip("\n")
    if tail.startswith("// namespace"):
        tail = tail.split("\n", 1)[-1] if "\n" in tail else ""

    rebuilt = before + f"namespace {detail_ns} {{" + inner + "}\n\n" + tail

    primary = stem  # e.g. TestHeadlessModuleVerifier
    primary_pattern = re.compile(
        rf"(void\s+{re.escape(primary)}\s*\([^)]*\)\s*\{{\s*\n)",
        re.MULTILINE,
    )

    def add_using(m: re.Match[str]) -> str:
        block = m.group(1)
        if f"using namespace {detail_ns}" in block:
            return block
        return block + f"    using namespace {detail_ns};\n"

    rebuilt, count = primary_pattern.subn(add_using, rebuilt, count=1)
    if count != 1:
        raise RuntimeError(f"could not find primary entry void {primary}() in merged section")

    return rebuilt


def merge_sources(src_dir: Path) -> str:
    files = sorted(src_dir.glob("Test*.cpp"))
    all_includes: list[str] = []
    seen_include: set[str] = set()
    chunks: list[str] = []

    for path in files:
        text = path.read_text(encoding="utf-8")
        includes, body = extract_includes_and_body(text)
        for inc in includes:
            if inc not in seen_include:
                seen_include.add(inc)
                all_includes.append(inc)

        stem = path.stem
        detail_ns = detail_namespace_from_filename(stem)
        chunks.append(f"// --- merged from {path.name} ---\n")
        chunks.append(transform_file_body(body.strip() + "\n", detail_ns, stem))
        chunks.append("\n")

    header = (
        "// Merged from former Test*.cpp sources; each section uses `detail_*` for private helpers.\n\n"
        + "\n".join(all_includes)
        + "\n\n"
    )
    return header + "".join(chunks)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    src_dir = root / "src"
    out_path = src_dir / "EngineImportTests.cpp"
    merged = merge_sources(src_dir)
    out_path.write_text(merged, encoding="utf-8")
    print(f"Wrote {out_path} ({len(merged)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
