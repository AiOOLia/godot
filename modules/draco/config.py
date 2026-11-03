def can_build(env, platform):
    return not env["disable_3d"]


def is_enabled():
    return True


def configure(env):
    pass
