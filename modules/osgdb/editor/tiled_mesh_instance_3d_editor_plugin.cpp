/**************************************************************************/
/*  tiled_mesh_instance_3d_editor_plugin.cpp                              */
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

#include "tiled_mesh_instance_3d_editor_plugin.h"

#include "../tiled_mesh_instance_3d.h"

#include "core/object/callable_mp.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/main/scene_tree.h"

namespace {

static TiledMeshInstance3D *get_tiled_target(ObjectID p_id) {
	return ObjectDB::get_instance<TiledMeshInstance3D>(p_id);
}

} // namespace

void TiledMeshInstance3DInspectorControl::_open_reload() {
	if (TiledMeshInstance3D *target = get_tiled_target(target_id)) {
		if (target->is_open()) {
			target->reload();
		} else {
			target->open();
		}
		_update_status();
	}
}

void TiledMeshInstance3DInspectorControl::_toggle_pause() {
	if (TiledMeshInstance3D *target = get_tiled_target(target_id)) {
		target->set_streaming_paused(!target->is_streaming_paused());
		_update_status();
	}
}

void TiledMeshInstance3DInspectorControl::_unload() {
	if (TiledMeshInstance3D *target = get_tiled_target(target_id)) {
		target->close();
		_update_status();
	}
}

void TiledMeshInstance3DInspectorControl::_frame_dataset() {
	Node3DEditorViewport *viewport = Node3DEditor::get_singleton() ? Node3DEditor::get_singleton()->get_last_used_viewport() : nullptr;
	if (viewport) {
		viewport->focus_selection();
	}
}

void TiledMeshInstance3DInspectorControl::_toggle_bounds() {
	if (TiledMeshInstance3D *target = get_tiled_target(target_id)) {
		target->set_editor_preview_show_bounds(!target->is_editor_preview_showing_bounds());
		_update_status();
	}
}

void TiledMeshInstance3DInspectorControl::_update_status() {
	TiledMeshInstance3D *target = get_tiled_target(target_id);
	if (!target) {
		set_process(false);
		return;
	}
	const Dictionary stats = target->get_streaming_stats();
	open_reload_button->set_text(target->is_open() ? TTR("Reload") : TTR("Open"));
	pause_button->set_text(target->is_streaming_paused() ? TTR("Resume Preview") : TTR("Pause Preview"));
	pause_button->set_disabled(!target->is_open());
	unload_button->set_disabled(!target->is_open());
	bounds_button->set_pressed(target->is_editor_preview_showing_bounds());
	statistics->set_text(vformat(
			TTR("State: %s\nResident: %d  Requested: %d  CPU ready: %d  Failed: %d\nRender RIDs: %d/%d visible  Inline LOD: %d/%d visible\nFallback: %d/%d hidden  Textures pending: %d\nMemory: %s"),
			String(stats["root_state"]), int(stats["resident_tiles"]), int(stats["requested_tiles"]), int(stats["cpu_ready_tiles"]), int(stats["failed_tiles"]),
			int(stats["visible_render_instances"]), int(stats["resident_render_instances"]), int(stats["visible_inline_lod_render_instances"]), int(stats["inline_lod_render_instances"]),
			int(stats["hidden_fallback_surfaces"]), int(stats["fallback_surfaces"]), int(stats["texture_requests"]), String::humanize_size(uint64_t(stats["estimated_resident_bytes"]))));
}

void TiledMeshInstance3DInspectorControl::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		status_refresh_elapsed += get_process_delta_time();
		if (status_refresh_elapsed < 0.25) {
			return;
		}
		status_refresh_elapsed = 0.0;
		_update_status();
	}
}

TiledMeshInstance3DInspectorControl::TiledMeshInstance3DInspectorControl(ObjectID p_target) :
		target_id(p_target) {
	HBoxContainer *primary_row = memnew(HBoxContainer);
	add_child(primary_row);
	open_reload_button = memnew(Button);
	open_reload_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	open_reload_button->connect(SceneStringName(pressed), callable_mp(this, &TiledMeshInstance3DInspectorControl::_open_reload));
	primary_row->add_child(open_reload_button);
	pause_button = memnew(Button(TTR("Pause Preview")));
	pause_button->connect(SceneStringName(pressed), callable_mp(this, &TiledMeshInstance3DInspectorControl::_toggle_pause));
	primary_row->add_child(pause_button);
	unload_button = memnew(Button(TTR("Unload")));
	unload_button->connect(SceneStringName(pressed), callable_mp(this, &TiledMeshInstance3DInspectorControl::_unload));
	primary_row->add_child(unload_button);

	HBoxContainer *secondary_row = memnew(HBoxContainer);
	add_child(secondary_row);
	Button *frame_button = memnew(Button(TTR("Frame Dataset")));
	frame_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	frame_button->connect(SceneStringName(pressed), callable_mp(this, &TiledMeshInstance3DInspectorControl::_frame_dataset));
	secondary_row->add_child(frame_button);
	bounds_button = memnew(Button(TTR("Show Bounds")));
	bounds_button->set_toggle_mode(true);
	bounds_button->connect(SceneStringName(pressed), callable_mp(this, &TiledMeshInstance3DInspectorControl::_toggle_bounds));
	secondary_row->add_child(bounds_button);

	statistics = memnew(Label);
	statistics->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	add_child(statistics);
	set_process(true);
	_update_status();
}

bool TiledMeshInstance3DInspectorPlugin::can_handle(Object *p_object) {
	return Object::cast_to<TiledMeshInstance3D>(p_object) != nullptr;
}

void TiledMeshInstance3DInspectorPlugin::parse_begin(Object *p_object) {
	TiledMeshInstance3D *target = Object::cast_to<TiledMeshInstance3D>(p_object);
	if (target) {
		add_custom_control(memnew(TiledMeshInstance3DInspectorControl(target->get_instance_id())));
	}
}

bool TiledMeshInstance3DGizmoPlugin::has_gizmo(Node3D *p_node) {
	return Object::cast_to<TiledMeshInstance3D>(p_node) != nullptr;
}

String TiledMeshInstance3DGizmoPlugin::get_gizmo_name() const {
	return "TiledMeshInstance3DDatasetBounds";
}

int TiledMeshInstance3DGizmoPlugin::get_priority() const {
	return -1;
}

void TiledMeshInstance3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	TiledMeshInstance3D *tiled = Object::cast_to<TiledMeshInstance3D>(p_gizmo->get_node_3d());
	if (!tiled) {
		return;
	}
	p_gizmo->clear();
	const AABB bounds = tiled->get_dataset_aabb();
	if ((!p_gizmo->is_selected() && !tiled->is_editor_preview_showing_bounds()) || bounds == AABB()) {
		return;
	}

	Vector<Vector3> lines;
	for (int i = 0; i < 12; i++) {
		Vector3 a;
		Vector3 b;
		bounds.get_edge(i, a, b);
		lines.push_back(a);
		lines.push_back(b);
	}

	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	material->set_albedo(EDITOR_GET("editors/3d_gizmos/gizmo_colors/aabb"));
	material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	p_gizmo->add_lines(lines, material);
	p_gizmo->add_collision_segments(lines);
}

void TiledMeshInstance3DEditorPlugin::_notification(int p_what) {
	if (p_what != NOTIFICATION_PROCESS || !Node3DEditor::get_singleton() || !Node3DEditor::get_singleton()->is_visible() || !get_tree()) {
		return;
	}

	Vector<Camera3D *> cameras;
	for (uint32_t i = 0; i < Node3DEditor::VIEWPORTS_COUNT; i++) {
		Node3DEditorViewport *editor_viewport = Node3DEditor::get_singleton()->get_editor_viewport(i);
		if (editor_viewport && editor_viewport->is_visible_in_tree() && editor_viewport->get_camera_3d()) {
			cameras.push_back(editor_viewport->get_camera_3d());
		}
	}
	if (cameras.is_empty()) {
		return;
	}
	Node3DEditorViewport *active_viewport = Node3DEditor::get_singleton()->get_last_used_viewport();
	Camera3D *active_camera = active_viewport ? active_viewport->get_camera_3d() : cameras[0];

	const Vector<Node *> nodes = get_tree()->get_nodes_in_group(StringName("_tiled_mesh_instances_3d"));
	for (Node *node : nodes) {
		TiledMeshInstance3D *tiled = Object::cast_to<TiledMeshInstance3D>(node);
		if (tiled && tiled->is_inside_tree()) {
			tiled->editor_update_cameras(cameras, active_camera);
		}
	}
}

bool TiledMeshInstance3DEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<TiledMeshInstance3D>(p_object) != nullptr;
}

TiledMeshInstance3DEditorPlugin::TiledMeshInstance3DEditorPlugin() {
	gizmo_plugin.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(gizmo_plugin);
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
	set_process(true);
}

TiledMeshInstance3DEditorPlugin::~TiledMeshInstance3DEditorPlugin() {
	if (inspector_plugin.is_valid()) {
		remove_inspector_plugin(inspector_plugin);
	}
	if (Node3DEditor::get_singleton() && gizmo_plugin.is_valid()) {
		Node3DEditor::get_singleton()->remove_gizmo_plugin(gizmo_plugin);
	}
}
