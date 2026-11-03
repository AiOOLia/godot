def can_build(env, platform):
    env.module_add_dependencies("3dtiles", ["tiled_mesh_3d"])
    env.module_add_dependencies("3dtiles", ["draco"], True)
    return not env["disable_3d"] and platform in ["windows", "linuxbsd", "macos", "android", "ios", "web"]


def configure(env):
    pass


def get_doc_classes():
    return ["Tiles3DOptions"]


def get_doc_path():
    return "doc_classes"
