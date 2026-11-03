#!/usr/bin/env python3
"""Read-only path coverage for the local engine inventory; no build or Git writes."""

import argparse
import json
from pathlib import Path
import subprocess
import sys


def git(root, *arguments):
    result = subprocess.run(["git", "-C", str(root), *arguments], capture_output=True, check=False)
    if result.returncode:
        raise ValueError(result.stderr.decode("utf-8", errors="replace").strip())
    return result.stdout


def validate_path(path):
    if not isinstance(path, str) or not path or path.startswith("/") or "\\" in path:
        raise ValueError(f"Invalid inventory path: {path!r}")
    if ":" in path or any(part in (".", "..", "") for part in path.rstrip("/").split("/")):
        raise ValueError(f"Invalid inventory path: {path!r}")
    if any(char in path for char in "*?["):
        raise ValueError("Use exact files or directory prefixes ending in '/', not globs")


def matches(path, scope):
    return path.startswith(scope) if scope.endswith("/") else path == scope


def inspect(root, manifest, base_override=None):
    if manifest.get("schema_version") != 1:
        raise ValueError("Unsupported inventory schema")
    entries = manifest["entries"]
    identifiers = [entry["id"] for entry in entries]
    if len(identifiers) != len(set(identifiers)) or not entries:
        raise ValueError("Inventory IDs must be unique and nonempty")
    for entry in entries:
        if not entry["paths"]:
            raise ValueError(f"No paths for {entry['id']}")
        for path in entry["paths"]:
            validate_path(path)
    exclusions = manifest.get("excluded", [])
    for exclusion in exclusions:
        validate_path(exclusion["path"])
        if not exclusion["reason"]:
            raise ValueError("Every exclusion needs a reason")
    base = base_override or manifest["comparison_base"]
    base_commit = git(root, "rev-parse", "--verify", "--end-of-options", base + "^{commit}").decode().strip()
    # One endpoint means compare the full working tree against the named commit,
    # including staged changes. Untracked files require a separate query.
    tracked = git(root, "diff", "--name-only", "--no-renames", "-z", base_commit, "--")
    untracked = git(root, "ls-files", "--others", "--exclude-standard", "-z")
    paths = sorted({raw.decode("utf-8", errors="surrogateescape") for raw in (tracked + untracked).split(b"\0") if raw})
    covered, ignored, uncovered = [], [], []
    for path in paths:
        owners = [entry["id"] for entry in entries if any(matches(path, scope) for scope in entry["paths"])]
        if owners:
            covered.append({"path": path, "entries": owners})
            continue
        exclusions_for_path = [rule["reason"] for rule in exclusions if matches(path, rule["path"])]
        if exclusions_for_path:
            ignored.append({"path": path, "reasons": exclusions_for_path})
        else:
            uncovered.append(path)
    return {"base": base_commit, "head": git(root, "rev-parse", "HEAD").decode().strip(),
            "covered": covered, "excluded": ignored, "unregistered": uncovered}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", help="Explicit comparison commit for migration review; does not edit the inventory")
    parser.add_argument("--json", action="store_true", help="Print complete path-to-entry mapping")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    try:
        manifest = json.loads((root / "GODOT_SOURCE_CHANGES.json").read_text(encoding="utf-8"))
        report = inspect(root, manifest, args.base)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Inventory check failed: {error}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(report, ensure_ascii=True, indent=2))
    else:
        print(f"Comparison base: {report['base']}")
        print(f"Registered paths: {len(report['covered'])}; excluded paths: {len(report['excluded'])}")
        for entry in manifest["entries"]:
            count = sum(entry["id"] in row["entries"] for row in report["covered"])
            print(f"  {entry['id']}: {count} paths")
        for path in report["unregistered"]:
            print(f"UNREGISTERED: {path}")
        print("FAIL: register the paths above." if report["unregistered"] else "PASS: all compared paths are registered or explicitly excluded.")
        print("Path coverage only; existing-file edits still require a manual description and test review.")
    return 1 if report["unregistered"] else 0


if __name__ == "__main__":
    sys.exit(main())
