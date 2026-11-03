def can_build(env, platform):
    env.module_add_dependencies("osgdb", ["tiled_mesh_3d"])
    return env["threads"] and not env["disable_3d"] and platform in ["windows", "linuxbsd", "web"]


def configure(env):
    pass


def get_doc_classes():
    return ["OsgTiledMeshOptions"]


def get_doc_path():
    return "doc_classes"
