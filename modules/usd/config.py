from pathlib import Path


OPENUSD_VERSION = "v24.08"
OPENUSD_COMMIT = "59992d2178afcebd89273759f2bddfe730e59aa8"
OPENUSD_ROOT = Path(__file__).resolve().parents[2] / "thirdparty" / "openusd"
PREBUILD_HELP = (
    "Prebuild explicitly: python thirdparty/openusd/build.py fetch, then "
    "python thirdparty/openusd/build.py build. SCons is offline and does not build dependencies. "
    "Alternatively set usd_sdk_path to a matching OpenUSD v24.08 shared monolithic SDK."
)


def default_sdk_path(platform):
    return OPENUSD_ROOT / "install" / platform


def is_enabled():
    return False


def get_opts(platform):
    return [("usd_sdk_path", "External OpenUSD v24.08 SDK override (default: bundled prebuild install/<platform>)", "")]


def can_build(env, platform):
    if platform == "web":
        raise RuntimeError(
            "USD Web builds are not supported: OpenUSD v24.08 has no verified Emscripten SDK in this module; "
            "the independent thirdparty/openusd/web_probe does not enable Godot Web"
        )
    if platform not in ("windows", "linuxbsd", "macos") or env["disable_3d"] or not env["threads"]:
        raise RuntimeError("USD requires desktop, threads=yes and disable_3d=no")
    return True


def configure(env):
    if env["platform"] == "windows":
        if not env.msvc or env["use_static_cpp"] or env["debug_crt"]:
            raise RuntimeError("USD requires MSVC with use_static_cpp=no debug_crt=no and a matching /MD SDK")
    prefix = str(env.get("usd_sdk_path", ""))
    sdk = Path(prefix).expanduser().resolve() if prefix else default_sdk_path(env["platform"])
    for header in ("pxr/pxr.h", "pxr/usd/usd/stage.h", "pxr/usd/usdGeom/mesh.h", "pxr/usd/usdGeom/primvarsAPI.h"):
        if not (sdk / "include" / header).is_file():
            raise RuntimeError("USD SDK header missing: " + str(sdk / "include" / header) + ". " + PREBUILD_HELP)
    library_name = {"windows": "usd_ms.lib", "linuxbsd": "libusd_ms.so", "macos": "libusd_ms.dylib"}[env["platform"]]
    library = next((sdk / folder / library_name for folder in ("lib", "lib64") if (sdk / folder / library_name).is_file()), None)
    if library is None:
        raise RuntimeError("USD requires the shared monolithic library " + library_name + " in SDK lib/ or lib64/ (static/split libraries unsupported). " + PREBUILD_HELP)
    if env["platform"] == "windows" and not any((sdk / folder / "usd_ms.dll").is_file() for folder in ("bin", "lib")):
        raise RuntimeError("USD SDK requires bin/usd_ms.dll or lib/usd_ms.dll. " + PREBUILD_HELP)
    env["USD_INCLUDE_PATH"] = str(sdk / "include")
    env["USD_LIBRARY"] = str(library)
