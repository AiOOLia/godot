#!/usr/bin/env python3
"""Strip TOOLS_ENABLED / editor-only preprocessor regions from main/DigitalViewerMain.cpp."""

import re
import sys
from pathlib import Path


def should_skip_block_open(line: str) -> bool:
    s = line.strip()
    if s.startswith("#ifdef TOOLS_ENABLED"):
        return True
    if s == "#if TOOLS_ENABLED":
        return True
    if re.match(r"#if\s+defined\(TOOLS_ENABLED\)\s*$", s):
        return True
    if "defined(TOOLS_ENABLED)" in s and s.startswith("#if "):
        # Keep blocks that only add DEBUG when tools are off; strip all other TOOLS-only #ifs
        # (including e.g. `#if defined(TOOLS_ENABLED) && (WIN || LINUX)` where `||` is unrelated).
        if re.search(r"defined\s*\(\s*DEBUG_ENABLED\s*\)\s*\|\|", s):
            return False
        if s.startswith("#ifndef"):
            return False
        return True
    return False


def rewrite_line(line: str) -> str:
    line2 = re.sub(
        r"#if\s+defined\(DEBUG_ENABLED\)\s*\|\|\s*defined\(TOOLS_ENABLED\)",
        "#if defined(DEBUG_ENABLED)",
        line,
    )
    line2 = re.sub(
        r"#if\s+!defined\(OVERRIDE_PATH_ENABLED\)\s*&&\s*!defined\(TOOLS_ENABLED\)",
        "#if !defined(OVERRIDE_PATH_ENABLED)",
        line2,
    )
    line2 = re.sub(
        r"#if\s+!defined\(TOOLS_ENABLED\)\s*&&\s*defined\(WEB_ENABLED\)",
        "#if defined(WEB_ENABLED)",
        line2,
    )
    line2 = re.sub(
        r"#endif\s*//\s*defined\(DEBUG_ENABLED\)\s*\|\|\s*defined\s*\(TOOLS_ENABLED\)",
        "#endif // defined(DEBUG_ENABLED)",
        line2,
    )
    line2 = re.sub(
        r"#endif\s*//\s*defined\(DEBUG_ENABLED\)\s*\|\|\s*defined\s+\(TOOLS_ENABLED\)",
        "#endif // defined(DEBUG_ENABLED)",
        line2,
    )
    return line2


def strip_file(path: Path) -> None:
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    out = []
    i = 0
    skip = 0

    while i < len(lines):
        line = lines[i]
        s = line.strip()

        if skip > 0:
            if s.startswith("#if") or s.startswith("#ifdef") or s.startswith("#ifndef"):
                skip += 1
            elif s.startswith("#endif"):
                skip -= 1
            i += 1
            continue

        line = rewrite_line(line)
        s = line.strip()

        if s.startswith("#ifndef TOOLS_ENABLED"):
            depth = 1
            i += 1
            while i < len(lines) and depth > 0:
                s2 = lines[i].strip()
                if s2.startswith("#if") or s2.startswith("#ifdef") or s2.startswith("#ifndef"):
                    depth += 1
                elif s2.startswith("#endif"):
                    depth -= 1
                    if depth == 0:
                        i += 1
                        break
                else:
                    if depth == 1:
                        out.append(lines[i])
                i += 1
            continue

        if should_skip_block_open(line):
            skip = 1
            i += 1
            continue

        out.append(line)
        i += 1

    path.write_text("".join(out), encoding="utf-8", newline="\n")


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    target = root / "main" / "DigitalViewerMain.cpp"
    if not target.is_file():
        print(f"Missing {target}", file=sys.stderr)
        sys.exit(1)
    strip_file(target)
    print(f"Updated {target}")


if __name__ == "__main__":
    main()
