/**************************************************************************/

/*  main_window.h                                                         */

/**************************************************************************/

/*                         This is part of DigitalViewer fork app code.    */

/**************************************************************************/



#pragma once



#include "core/input/input_event.h"

#include "scene/gui/control.h"



class Button;

class Label;

class PanelContainer;

class Tree;

class TreeItem;



namespace dw {



/** Full-window main UI shell: caption strip, structure dock, DigitalView viewport, properties dock, status bar. */

class MainWindow : public Control {

	GDCLASS(MainWindow, Control);



protected:

	static void _bind_methods();

	void _notification(int p_what);



	void _on_nonclient_title_input(const Ref<InputEvent> &p_event);

	void _on_title_drag_region_gui_input(const Ref<InputEvent> &p_event);

	void _process_title_bar_drag(const Ref<InputEvent> &p_event);

	void _sync_title_merge_layout();



	void _title_win_minimize();

	void _title_win_toggle_maximize();

	void _update_title_win_maximize_icon();

	void _title_win_close();



	void _refresh_structure_tree();

	void _on_structure_tree_item_selected();

	void _update_properties_panel(Node *p_node);



	bool _ensure_digital_view_extension_loaded();
	void _sync_digital_view_layout();



	PanelContainer *title_bar = nullptr;

	Control *title_drag_region = nullptr;

	Button *title_win_maximize_button = nullptr;

	bool title_drag_moving = false;

	Point2i title_drag_click_rel;



	Label *status_label = nullptr;

	Control *digital_view = nullptr;



	Tree *structure_tree = nullptr;

	Label *prop_name_label = nullptr;

	Label *prop_class_label = nullptr;

	Label *prop_path_label = nullptr;



public:

	MainWindow();

};



} // namespace dw

