#pragma once

#include "core/object/ref_counted.h"
#include "scene/3d/node_3d.h"

class USDDocument : public RefCounted {
	GDCLASS(USDDocument, RefCounted);

protected:
	static void _bind_methods();

public:
	// Static snapshot at the stage start time. External files must be accessible to USD's resolver.
	static Node3D *import_scene(const String &p_path);
	static Node3D *import_scene_internal(const String &p_path, bool p_importer_mesh, Error *r_error = nullptr);
};
