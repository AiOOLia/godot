/**************************************************************************/
/*  tiled_mesh_instance_3d_editor_plugin.h                                */
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

#pragma once

#include "editor/plugins/editor_plugin.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/scene/3d/node_3d_editor_gizmos.h"
#include "scene/gui/box_container.h"

class Button;
class Label;

class TiledMeshInstance3DInspectorControl : public VBoxContainer {
	GDCLASS(TiledMeshInstance3DInspectorControl, VBoxContainer);

	ObjectID target_id;
	Button *open_reload_button = nullptr;
	Button *pause_button = nullptr;
	Button *unload_button = nullptr;
	Button *bounds_button = nullptr;
	Label *statistics = nullptr;
	double status_refresh_elapsed = 0.0;

	void _open_reload();
	void _toggle_pause();
	void _unload();
	void _frame_dataset();
	void _toggle_bounds();
	void _update_status();

protected:
	void _notification(int p_what);

public:
	explicit TiledMeshInstance3DInspectorControl(ObjectID p_target);
};

class TiledMeshInstance3DInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(TiledMeshInstance3DInspectorPlugin, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class TiledMeshInstance3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(TiledMeshInstance3DGizmoPlugin, EditorNode3DGizmoPlugin);

public:
	virtual bool has_gizmo(Node3D *p_node) override;
	virtual String get_gizmo_name() const override;
	virtual int get_priority() const override;
	virtual void redraw(EditorNode3DGizmo *p_gizmo) override;
};

class TiledMeshInstance3DEditorPlugin : public EditorPlugin {
	GDCLASS(TiledMeshInstance3DEditorPlugin, EditorPlugin);

	Ref<TiledMeshInstance3DGizmoPlugin> gizmo_plugin;
	Ref<TiledMeshInstance3DInspectorPlugin> inspector_plugin;

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "TiledMeshInstance3D"; }
	virtual bool handles(Object *p_object) const override;

	TiledMeshInstance3DEditorPlugin();
	~TiledMeshInstance3DEditorPlugin();
};
