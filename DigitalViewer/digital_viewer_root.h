/**************************************************************************/
/*  digital_viewer_root.h                                                 */
/**************************************************************************/
/*                         This is part of DigitalViewer fork app code.    */
/**************************************************************************/

#pragma once

#include "core/input/input_event.h"
#include "scene/gui/control.h"

class Camera3D;
class FileDialog;
class HSplitContainer;
class Label;
class Node3D;
class PanelContainer;
class SubViewport;
class TabContainer;
class Tree;
class TreeItem;

// Full-window UI: title strip, left structure dock (tab), center 3D viewport, right properties dock, status bar.
class DigitalViewerRoot : public Control {
	GDCLASS(DigitalViewerRoot, Control);

	enum ImportMenuItem {
		MENU_IMPORT_GLTF = 1,
	};

protected:
	static void _bind_methods();
	void _notification(int p_what);

	void _on_import_menu_id_pressed(int p_id);
	void _on_file_selected(const String &p_path);

	void _on_nonclient_title_input(const Ref<InputEvent> &p_event);
	void _on_title_drag_region_gui_input(const Ref<InputEvent> &p_event);
	void _process_title_bar_drag(const Ref<InputEvent> &p_event);
	void _sync_title_merge_layout();

	void _title_win_minimize();
	void _title_win_toggle_maximize();
	void _title_win_close();

	void _refresh_structure_tree();
	void _on_structure_tree_item_selected();
	void _structure_tree_fill_branch(TreeItem *p_parent, Node *p_node);
	void _update_properties_panel(Node *p_node);

	PanelContainer *title_bar = nullptr;
	Control *title_drag_region = nullptr;
	bool title_drag_moving = false;
	Point2i title_drag_click_rel;

#ifndef _3D_DISABLED
	void _clear_model_holder();
	void _frame_camera_to_contents();
#endif

	Label *status_label = nullptr;
	FileDialog *file_dialog = nullptr;

	Tree *structure_tree = nullptr;
	Label *prop_name_label = nullptr;
	Label *prop_class_label = nullptr;
	Label *prop_path_label = nullptr;

#ifndef _3D_DISABLED
	SubViewport *model_viewport = nullptr;
	Node3D *model_holder = nullptr;
	Camera3D *view_camera = nullptr;
#endif

public:
	DigitalViewerRoot();
};
