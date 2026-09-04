def can_build(env, platform):
    return env["threads"] and not env["disable_3d"] and platform in ["windows", "linuxbsd", "web"]


def configure(env):
    pass


def get_doc_classes():
    return ["TiledMeshInstance3D", "TiledOriginController3D"]


def get_doc_path():
    return "doc_classes"
