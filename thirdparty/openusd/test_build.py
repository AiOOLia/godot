import argparse
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


ROOT = Path(__file__).resolve().parent
build = load_module("openusd_build", ROOT / "build.py")
config = load_module("usd_config", ROOT.parents[1] / "modules" / "usd" / "config.py")


class Environment(dict):
    msvc = True


class BuildTests(unittest.TestCase):
    def test_default_paths_and_pin(self):
        self.assertEqual(build.OPENUSD_COMMIT, config.OPENUSD_COMMIT)
        self.assertEqual(build.OPENUSD_VERSION, config.OPENUSD_VERSION)
        for target in ("windows", "linuxbsd", "macos"):
            self.assertEqual(build.default_prefix(target), config.default_sdk_path(target))
        self.assertEqual(build.parser().parse_args(["build"]).prefix, build.default_prefix(build.host_platform()))

    def test_upstream_command(self):
        command = build.build_command(Path("source"), Path("install"), 3)
        self.assertEqual(command[1:3], [str(Path("source/build_scripts/build_usd.py")), "install"])
        self.assertEqual(command[3:], [
            "--build-monolithic", "--no-python", "--no-imaging", "--no-usdview",
            "--no-tests", "--no-examples", "--no-tutorials", "--build-variant", "release", "--jobs", "3",
        ])

    def test_positive_limits(self):
        for value in ("0", "-1"):
            with self.assertRaises(argparse.ArgumentTypeError):
                build.positive_int(value)
        self.assertEqual(build.positive_int("2"), 2)

    def test_missing_source(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, "build.py fetch"):
                build.verify_source(Path(directory))

    def test_source_verification(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / ".git").mkdir()
            with patch.object(build, "git", return_value="wrong"):
                with self.assertRaisesRegex(RuntimeError, "commit does not match"):
                    build.verify_source(source)
            with patch.object(build, "git", side_effect=[build.OPENUSD_COMMIT, " M modified"]):
                with self.assertRaisesRegex(RuntimeError, "modified"):
                    build.verify_source(source)
            with patch.object(build, "git", side_effect=[build.OPENUSD_COMMIT, ""]):
                with self.assertRaisesRegex(RuntimeError, "missing build_scripts"):
                    build.verify_source(source)

    def test_fetch_pin_and_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(build, "git", side_effect=["", "", build.OPENUSD_COMMIT, ""]) as git:
                with patch.object(build, "verify_source") as verify:
                    build.fetch_source(Path(directory), 17)
            fetch = git.call_args_list[1]
            self.assertIn(build.OPENUSD_COMMIT, fetch.args)
            self.assertIn("--depth=1", fetch.args)
            self.assertEqual(fetch.kwargs["timeout"], 17)
            verify.assert_called_once_with(Path(directory))

    def test_fetch_rejects_mismatched_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(build, "git", side_effect=["", "", "wrong"]) as git:
                with self.assertRaisesRegex(RuntimeError, "Fetched OpenUSD commit"):
                    build.fetch_source(Path(directory), 17)
            self.assertEqual(git.call_count, 3)

    def test_fetch_preserves_nonempty_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / "keep").touch()
            with self.assertRaisesRegex(RuntimeError, "nonempty"):
                build.fetch_source(Path(directory), 17)

    def test_option_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / "build_scripts").mkdir()
            script = source / "build_scripts/build_usd.py"
            script.write_text('parser.add_argument("--build-monolithic")\n', encoding="utf-8")
            build.validate_upstream_options(source, ["python", "script", "prefix", "--build-monolithic"])
            with self.assertRaisesRegex(RuntimeError, "lacks options: --no-python"):
                build.validate_upstream_options(source, ["python", "script", "prefix", "--no-python"])

    def test_build_and_dry_run(self):
        for dry_run in (False, True):
            with patch.object(build, "verify_source"), patch.object(build, "validate_upstream_options"):
                with patch.object(build.subprocess, "run") as run, contextlib.redirect_stdout(io.StringIO()):
                    args = ["build", "--build-timeout", "23"] + (["--dry-run"] if dry_run else [])
                    self.assertEqual(build.main(args), 0)
                self.assertEqual(run.call_count, 0 if dry_run else 1)
                if not dry_run:
                    self.assertEqual(run.call_args.kwargs, {"check": True, "timeout": 23})

    def test_git_is_bounded_and_noninteractive(self):
        with patch.object(build.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "sha\n")) as run:
            self.assertEqual(build.git(Path("source"), "rev-parse", "HEAD"), "sha")
        self.assertEqual(run.call_args.kwargs["timeout"], 60)
        self.assertEqual(run.call_args.kwargs["env"]["GIT_TERMINAL_PROMPT"], "0")


class ConfigTests(unittest.TestCase):
    def make_sdk(self, sdk, platform, folder="lib"):
        for header in ("pxr/pxr.h", "pxr/usd/usd/stage.h", "pxr/usd/usdGeom/mesh.h", "pxr/usd/usdGeom/primvarsAPI.h"):
            path = sdk / "include" / header
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        library = {"windows": "usd_ms.lib", "linuxbsd": "libusd_ms.so", "macos": "libusd_ms.dylib"}[platform]
        (sdk / folder).mkdir(parents=True, exist_ok=True)
        (sdk / folder / library).touch()
        if platform == "windows":
            (sdk / "bin").mkdir()
            (sdk / "bin/usd_ms.dll").touch()
        return sdk / folder / library

    def test_default_and_external_sdk(self):
        for platform in ("windows", "linuxbsd", "macos"):
            for external in (False, True):
                with tempfile.TemporaryDirectory() as directory:
                    sdk = Path(directory)
                    library = self.make_sdk(sdk, platform, "lib64")
                    env = Environment(platform=platform, use_static_cpp=False, debug_crt=False)
                    if external:
                        env["usd_sdk_path"] = str(sdk)
                    with patch.object(config, "default_sdk_path", return_value=sdk) as default:
                        config.configure(env)
                    self.assertEqual(default.call_count, 0 if external else 1)
                    self.assertEqual(env["USD_LIBRARY"], str(library))
                    self.assertEqual(env["USD_INCLUDE_PATH"], str(sdk / "include"))

    def test_missing_artifacts_have_prebuild_commands(self):
        for missing in ("header", "library", "dll"):
            with tempfile.TemporaryDirectory() as directory:
                sdk = Path(directory)
                library = self.make_sdk(sdk, "windows")
                path = {"header": sdk / "include/pxr/pxr.h", "library": library, "dll": sdk / "bin/usd_ms.dll"}[missing]
                path.unlink()
                env = Environment(platform="windows", use_static_cpp=False, debug_crt=False, usd_sdk_path=str(sdk))
                with self.assertRaises(RuntimeError) as raised:
                    config.configure(env)
                for text in ("build.py fetch", "build.py build", "SCons is offline"):
                    self.assertIn(text, str(raised.exception))

    def test_windows_crt_guard(self):
        env = Environment(platform="windows", use_static_cpp=True, debug_crt=False)
        with self.assertRaisesRegex(RuntimeError, "matching /MD SDK"):
            config.configure(env)

    def test_web_stays_disabled(self):
        self.assertFalse(config.is_enabled())
        with self.assertRaisesRegex(RuntimeError, "does not enable Godot Web"):
            config.can_build({"threads": True, "disable_3d": False}, "web")
        self.assertTrue(config.can_build({"threads": True, "disable_3d": False}, "linuxbsd"))


if __name__ == "__main__":
    unittest.main()
