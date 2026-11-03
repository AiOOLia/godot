import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location("usd_config", Path(__file__).resolve().parents[1] / "config.py")
config = importlib.util.module_from_spec(spec)
spec.loader.exec_module(config)


class Environment(dict):
    msvc = True


class USDConfigTests(unittest.TestCase):
    def env(self, **kwargs):
        result = Environment(platform="windows", threads=True, disable_3d=False, use_static_cpp=False, debug_crt=False, usd_sdk_path="")
        result.update(kwargs)
        return result

    def test_disabled_by_default(self):
        self.assertFalse(config.is_enabled())
        self.assertEqual(config.get_opts("windows")[0][0], "usd_sdk_path")

    def test_platform_constraints(self):
        with self.assertRaisesRegex(RuntimeError, "Web builds are not supported"):
            config.can_build(self.env(), "web")
        for platform in ("android", "ios"):
            with self.assertRaisesRegex(RuntimeError, "desktop"):
                config.can_build(self.env(), platform)
        for key in ("disable_3d", "threads"):
            with self.assertRaises(RuntimeError):
                config.can_build(self.env(**{key: key == "disable_3d"}), "windows")

    def test_crt_constraints(self):
        for key in ("use_static_cpp", "debug_crt"):
            with self.assertRaisesRegex(RuntimeError, "matching /MD"):
                config.configure(self.env(**{key: True}))
        env = self.env()
        env.msvc = False
        with self.assertRaisesRegex(RuntimeError, "MSVC"):
            config.configure(env)

    def test_missing_prefix(self):
        with tempfile.TemporaryDirectory() as empty_dir:
            with patch.object(config, "OPENUSD_ROOT", Path(empty_dir)):
                with self.assertRaisesRegex(RuntimeError, "header missing") as error:
                    config.configure(self.env())
        self.assertIn("Prebuild explicitly", str(error.exception))

    def test_missing_header(self):
        with patch.object(Path, "is_file", return_value=False):
            with self.assertRaisesRegex(RuntimeError, "header missing"):
                config.configure(self.env(usd_sdk_path="missing-sdk"))

    def test_missing_library(self):
        with patch.object(Path, "is_file", lambda path: path.suffix == ".h"):
            with self.assertRaisesRegex(RuntimeError, "shared monolithic"):
                config.configure(self.env(usd_sdk_path="missing-sdk"))

    def test_missing_dll(self):
        with patch.object(Path, "is_file", lambda path: path.suffix in (".h", ".lib")):
            with self.assertRaisesRegex(RuntimeError, "usd_ms.dll"):
                config.configure(self.env(usd_sdk_path="missing-sdk"))

    def test_shared_library_selection(self):
        # Tests only configuration path selection, not SDK validity or linking.
        for platform, name in (("windows", "usd_ms.lib"), ("linuxbsd", "libusd_ms.so"), ("macos", "libusd_ms.dylib")):
            with patch.object(Path, "is_file", return_value=True):
                env = self.env(platform=platform, usd_sdk_path="sdk")
                config.configure(env)
                self.assertEqual(Path(env["USD_LIBRARY"]).name, name)
                self.assertEqual(Path(env["USD_INCLUDE_PATH"]).name, "include")


if __name__ == "__main__":
    unittest.main()
