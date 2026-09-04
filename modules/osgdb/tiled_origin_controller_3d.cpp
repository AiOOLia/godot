/**************************************************************************/
/*  tiled_origin_controller_3d.cpp                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "tiled_origin_controller_3d.h"

#include "tiled_mesh_instance_3d.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"

Camera3D *TiledOriginController3D::_resolve_camera() const {
	if (!camera_path.is_empty()) {
		return Object::cast_to<Camera3D>(get_node_or_null(camera_path));
	}
	Viewport *viewport = get_viewport();
	return viewport ? viewport->get_camera_3d() : nullptr;
}

void TiledOriginController3D::_try_rebase() {
	if (Engine::get_singleton()->is_editor_hint() || !enabled || !is_inside_tree()) {
		return;
	}
	Camera3D *camera = _resolve_camera();
	if (!camera) {
		return;
	}
	const Vector3 camera_position = camera->get_global_position();
	if (camera_position.length() < rebase_threshold_m) {
		return;
	}
	const Vector3 delta = camera_position;
	Node3D *world_root = Object::cast_to<Node3D>(get_node_or_null(managed_world_root));
	if (world_root) {
		world_root->set_global_position(world_root->get_global_position() - delta);
	}

	PackedFloat64Array new_origin;
	new_origin.resize(3);
	Vector<Node *> tiled_nodes = get_tree()->get_nodes_in_group(StringName("_tiled_mesh_instances_3d"));
	for (Node *node : tiled_nodes) {
		TiledMeshInstance3D *tiled = Object::cast_to<TiledMeshInstance3D>(node);
		if (!tiled) {
			continue;
		}
		// `delta` is expressed in Godot world axes. Convert it back to the
		// source coordinate system before advancing the double-precision origin.
		const double scale = tiled->get_unit_scale();
		double source_dx = delta.x / scale;
		double source_dy = delta.y / scale;
		double source_dz = delta.z / scale;
		if (tiled->get_axis_mode() == TiledMeshInstance3D::AXIS_MODE_OSG_Z_UP) {
			source_dy = -delta.z / scale;
			source_dz = delta.y / scale;
		}
		tiled->set_world_origin(
				tiled->get_world_origin_x() + source_dx,
				tiled->get_world_origin_y() + source_dy,
				tiled->get_world_origin_z() + source_dz);
		new_origin.set(0, tiled->get_world_origin_x());
		new_origin.set(1, tiled->get_world_origin_y());
		new_origin.set(2, tiled->get_world_origin_z());
	}
	emit_signal(SNAME("origin_shifted"), delta, new_origin);
}

void TiledOriginController3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
		_try_rebase();
	}
}

void TiledOriginController3D::set_enabled(bool p_enabled) { enabled = p_enabled; }
bool TiledOriginController3D::is_enabled() const { return enabled; }
void TiledOriginController3D::set_camera_path(const NodePath &p_path) { camera_path = p_path; }
NodePath TiledOriginController3D::get_camera_path() const { return camera_path; }
void TiledOriginController3D::set_managed_world_root(const NodePath &p_path) { managed_world_root = p_path; }
NodePath TiledOriginController3D::get_managed_world_root() const { return managed_world_root; }
void TiledOriginController3D::set_rebase_threshold_m(double p_value) { rebase_threshold_m = MAX(p_value, 1.0); }
double TiledOriginController3D::get_rebase_threshold_m() const { return rebase_threshold_m; }

void TiledOriginController3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &TiledOriginController3D::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &TiledOriginController3D::is_enabled);
	ClassDB::bind_method(D_METHOD("set_camera_path", "path"), &TiledOriginController3D::set_camera_path);
	ClassDB::bind_method(D_METHOD("get_camera_path"), &TiledOriginController3D::get_camera_path);
	ClassDB::bind_method(D_METHOD("set_managed_world_root", "path"), &TiledOriginController3D::set_managed_world_root);
	ClassDB::bind_method(D_METHOD("get_managed_world_root"), &TiledOriginController3D::get_managed_world_root);
	ClassDB::bind_method(D_METHOD("set_rebase_threshold_m", "value"), &TiledOriginController3D::set_rebase_threshold_m);
	ClassDB::bind_method(D_METHOD("get_rebase_threshold_m"), &TiledOriginController3D::get_rebase_threshold_m);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "camera_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Camera3D"), "set_camera_path", "get_camera_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "managed_world_root", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_managed_world_root", "get_managed_world_root");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rebase_threshold_m", PROPERTY_HINT_RANGE, "1,1000000,1,or_greater"), "set_rebase_threshold_m", "get_rebase_threshold_m");
	ADD_SIGNAL(MethodInfo("origin_shifted", PropertyInfo(Variant::VECTOR3, "delta"), PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "new_origin")));
}

TiledOriginController3D::TiledOriginController3D() {
	set_process_internal(true);
}
