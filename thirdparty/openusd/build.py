#!/usr/bin/env python3
"""Explicit OpenUSD source acquisition and desktop SDK prebuild (never run by SCons)."""

import argparse
import ast
import os
from pathlib import Path
import platform
import subprocess
import sys

OPENUSD_VERSION = "v24.08"
OPENUSD_COMMIT = "59992d2178afcebd89273759f2bddfe730e59aa8"
ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "source"
REPOSITORY = "https://github.com/PixarAnimationStudios/OpenUSD.git"


def host_platform():
    return {"Windows": "windows", "Linux": "linuxbsd", "Darwin": "macos"}[platform.system()]


def default_prefix(target):
    return ROOT / "install" / target


def git(source, *args, timeout=60):
    env = dict(os.environ, GIT_TERMINAL_PROMPT="0")
    return subprocess.run(
        ["git", "-C", str(source), *args], check=True, timeout=timeout,
        env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    ).stdout.strip()


def verify_source(source):
    if not (source / ".git").is_dir():
        raise RuntimeError("OpenUSD source missing; run: python thirdparty/openusd/build.py fetch")
    if git(source, "rev-parse", "HEAD") != OPENUSD_COMMIT:
        raise RuntimeError("OpenUSD source commit does not match " + OPENUSD_COMMIT)
    if git(source, "status", "--porcelain", "--untracked-files=normal"):
        raise RuntimeError("OpenUSD source is modified; use a clean pinned checkout")
    if not (source / "build_scripts" / "build_usd.py").is_file():
        raise RuntimeError("Pinned checkout is missing build_scripts/build_usd.py")


def fetch_source(source, timeout):
    if (source / ".git").exists():
        try:
            git(source, "rev-parse", "--verify", "HEAD")
        except subprocess.CalledProcessError:
            pass  # A previous fetch may have timed out before checkout.
        else:
            verify_source(source)
            return
    elif source.exists() and any(source.iterdir()):
        raise RuntimeError("Refusing to replace nonempty source directory: " + str(source))
    source.mkdir(parents=True, exist_ok=True)
    git(source, "init")
    git(source, "-c", "http.lowSpeedLimit=1024", "-c", "http.lowSpeedTime=20",
        "fetch", "--depth=1", "--no-tags", REPOSITORY, OPENUSD_COMMIT, timeout=timeout)
    if git(source, "rev-parse", "FETCH_HEAD") != OPENUSD_COMMIT:
        raise RuntimeError("Fetched OpenUSD commit does not match pinned commit")
    git(source, "checkout", "--detach", OPENUSD_COMMIT)
    verify_source(source)


def build_command(source, prefix, jobs):
    return [
        sys.executable, str(source / "build_scripts" / "build_usd.py"), str(prefix),
        "--build-monolithic", "--no-python", "--no-imaging", "--no-usdview",
        "--no-tests", "--no-examples", "--no-tutorials", "--build-variant", "release",
        "--jobs", str(jobs),
    ]


def validate_upstream_options(source, command):
    # Inspect the actual pinned script without executing it (including its preflight).
    tree = ast.parse((source / "build_scripts" / "build_usd.py").read_text(encoding="utf-8"))
    options = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr == "add_argument":
            options.update(arg.value for arg in node.args if isinstance(arg, ast.Constant) and isinstance(arg.value, str))
    missing = [arg for arg in command[3:] if arg.startswith("--") and arg not in options]
    if missing:
        raise RuntimeError("Pinned upstream script lacks options: " + ", ".join(missing))


def positive_int(value):
    result = int(value)
    if result <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return result


def parser():
    result = argparse.ArgumentParser(description=__doc__, epilog=(
        "First run fetch, then build. Build invokes upstream build_usd.py, which downloads/builds "
        "dependencies including TBB. Requires CMake and a desktop C++ toolchain (MSVC developer "
        "shell on Windows). This does not build an Emscripten SDK. Keep the complete install "
        "tree for runtime libraries and USD plugin resources. SCons never downloads or builds it."
    ))
    result.add_argument("action", choices=("fetch", "build"))
    result.add_argument("--source", type=Path, default=SOURCE)
    result.add_argument("--prefix", type=Path, default=default_prefix(host_platform()))
    result.add_argument("--jobs", type=positive_int, default=min(os.cpu_count() or 1, 8))
    result.add_argument("--timeout", type=positive_int, default=120, help="source fetch timeout in seconds")
    result.add_argument("--build-timeout", type=positive_int, default=7200, help="upstream build timeout in seconds")
    result.add_argument("--dry-run", action="store_true", help="print build command without invoking upstream")
    return result


def main(argv=None):
    args = parser().parse_args(argv)
    source = args.source.expanduser().resolve()
    try:
        if args.action == "fetch":
            if args.dry_run:
                raise RuntimeError("--dry-run is only supported for build")
            fetch_source(source, args.timeout)
            print("Verified OpenUSD " + OPENUSD_COMMIT)
        else:
            verify_source(source)
            command = build_command(source, args.prefix.expanduser().resolve(), args.jobs)
            validate_upstream_options(source, command)
            print(subprocess.list2cmdline(command), flush=True)
            if not args.dry_run:
                subprocess.run(command, check=True, timeout=args.build_timeout)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print("OpenUSD: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
