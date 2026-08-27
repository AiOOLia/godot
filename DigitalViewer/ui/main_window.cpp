/**************************************************************************/
/*  main_window.cpp                                                       */
/**************************************************************************/
/*                         This is part of DigitalViewer fork app code.    */
/**************************************************************************/

#include "DigitalViewer/ui/main_window.h"

#include "DigitalViewer/ui/theme.h"

#include "core/extension/gdextension_manager.h"
#include "core/io/file_access.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "scene/main/node.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/tree.h"
#include "scene/gui/button.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/scene_string_names.h"
#include "servers/display/display_server.h"
#include "servers/display/display_server_enums.h"

namespace {

constexpr const char *DIGITAL_VIEW_GDEXTENSION_RES_PATH = "res://extensions/DigitalView/godotwindowextension.gdextension";

String get_digital_view_gdextension_path() {
	const String res_path = DIGITAL_VIEW_GDEXTENSION_RES_PATH;
	if (FileAccess::exists(res_path)) {
		return res_path;
	}

	const String exe_path = OS::get_singleton()->get_executable_path();
	if (!exe_path.is_empty()) {
		const String bin_path = exe_path.get_base_dir().path_join("godotwindowextension.gdextension");
		if (FileAccess::exists(bin_path)) {
			return bin_path;
		}
	}

	return res_path;
}

} // namespace

namespace dw {

void MainWindow::_bind_methods() {
}

bool MainWindow::_ensure_digital_view_extension_loaded() {
	if (ClassDB::class_exists(SNAME("DigitalView"))) {
		return true;
	}

	GDExtensionManager *extension_manager = GDExtensionManager::get_singleton();
	if (!extension_manager) {
		if (status_label) {
			status_label->set_text(String::utf8(u8"状态 - 无法加载 DigitalView: GDExtensionManager 不可用"));
		}
		return false;
	}

	const String extension_path = get_digital_view_gdextension_path();
	if (!FileAccess::exists(extension_path)) {
		if (status_label) {
			status_label->set_text(String::utf8(u8"状态 - 未找到 DigitalView 扩展: ") + extension_path);
		}
		return false;
	}

	const GDExtensionManager::LoadStatus load_status = extension_manager->load_extension(extension_path);
	if (load_status != GDExtensionManager::LOAD_STATUS_OK && load_status != GDExtensionManager::LOAD_STATUS_ALREADY_LOADED) {
		if (status_label) {
			status_label->set_text(String::utf8(u8"状态 - DigitalView 扩展加载失败: ") + extension_path);
		}
		return false;
	}

	if (!ClassDB::class_exists(SNAME("DigitalView"))) {
		if (status_label) {
			status_label->set_text(String::utf8(u8"状态 - DigitalView 扩展已加载，但未注册 DigitalView 类"));
		}
		return false;
	}

	return true;
}

void MainWindow::_sync_digital_view_layout() {
	if (!digital_view) {
		return;
	}
	if (digital_view->has_method(SNAME("sync_layout"))) {
		digital_view->call(SNAME("sync_layout"));
	}
}

void MainWindow::_refresh_structure_tree() {
	ERR_FAIL_NULL(structure_tree);
	structure_tree->clear();
	structure_tree->set_hide_root(true);

	TreeItem *hint = structure_tree->create_item();
	hint->set_text(0, String::utf8(u8"(DigitalView 场景)"));
	hint->set_selectable(0, false);
	_update_properties_panel(nullptr);
}

void MainWindow::_on_structure_tree_item_selected() {
	if (!structure_tree) {
		return;
	}
	TreeItem *sel = structure_tree->get_selected();
	if (!sel) {
		_update_properties_panel(nullptr);
		return;
	}
	const Variant meta = sel->get_metadata(0);
	if (meta.get_type() == Variant::NIL) {
		_update_properties_panel(nullptr);
		return;
	}
	const ObjectID oid((uint64_t)meta.operator int64_t());
	if (!oid.is_valid()) {
		_update_properties_panel(nullptr);
		return;
	}
	Object *obj = ObjectDB::get_instance(oid);
	Node *n = Object::cast_to<Node>(obj);
	_update_properties_panel(n);
}

void MainWindow::_update_properties_panel(Node *p_node) {
	if (!prop_name_label || !prop_class_label || !prop_path_label) {
		return;
	}
	if (!p_node) {
		const String dash(String::utf8(u8"—"));
		prop_name_label->set_text(dash);
		prop_class_label->set_text(dash);
		prop_path_label->set_text(dash);
		return;
	}
	prop_name_label->set_text(p_node->get_name());
	prop_class_label->set_text(p_node->get_class());
	prop_path_label->set_text(String(p_node->get_path()));
}

void MainWindow::_sync_title_merge_layout() {
	if (!title_bar) {
		return;
	}
	Window *w = get_window();
	if (!w || !DisplayServer::get_singleton()) {
		return;
	}
	if (DisplayServer::get_singleton()->has_feature(DisplayServerEnums::FEATURE_EXTEND_TO_TITLE)) {
		const Rect2 gr = title_bar->get_global_rect();
		w->set_nonclient_area(Rect2i(gr.position, gr.size));
	}
}

void MainWindow::_process_title_bar_drag(const Ref<InputEvent> &p_event) {
	Window *w = get_window();
	ERR_FAIL_NULL(w);

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && title_drag_moving) {
		if (mm->get_button_mask().has_flag(MouseButtonMask::LEFT)) {
			if (DisplayServer::get_singleton()->has_feature(DisplayServerEnums::FEATURE_WINDOW_DRAG)) {
				DisplayServer::get_singleton()->window_start_drag(w->get_window_id());
			} else {
				const Point2 mouse = DisplayServer::get_singleton()->mouse_get_position();
				w->set_position(Point2i(mouse) - title_drag_click_rel);
			}
		} else {
			title_drag_moving = false;
		}
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->is_pressed()) {
			if (DisplayServer::get_singleton()->has_feature(DisplayServerEnums::FEATURE_WINDOW_DRAG)) {
				DisplayServer::get_singleton()->window_start_drag(w->get_window_id());
			} else {
				title_drag_click_rel = Point2i(DisplayServer::get_singleton()->mouse_get_position()) - w->get_position();
				title_drag_moving = true;
			}
		} else {
			title_drag_moving = false;
		}
		if (mb->is_double_click() && mb->is_pressed()) {
			if (DisplayServer::get_singleton()->window_maximize_on_title_dbl_click()) {
				_title_win_toggle_maximize();
			}
			title_drag_moving = false;
		}
	}
}

void MainWindow::_on_nonclient_title_input(const Ref<InputEvent> &p_event) {
	_process_title_bar_drag(p_event);
}

void MainWindow::_on_title_drag_region_gui_input(const Ref<InputEvent> &p_event) {
	_process_title_bar_drag(p_event);
}

void MainWindow::_title_win_minimize() {
	Window *w = get_window();
	if (w) {
		w->set_mode(Window::MODE_MINIMIZED);
	}
}

void MainWindow::_title_win_toggle_maximize() {
	Window *w = get_window();
	if (!w) {
		return;
	}
	if (w->get_mode() == Window::MODE_MAXIMIZED) {
		w->set_mode(Window::MODE_WINDOWED);
	} else if (w->get_mode() == Window::MODE_WINDOWED) {
		w->set_mode(Window::MODE_MAXIMIZED);
	}
	_update_title_win_maximize_icon();
}

void MainWindow::_update_title_win_maximize_icon() {
	if (!title_win_maximize_button) {
		return;
	}
	Window *w = get_window();
	if (w && w->get_mode() == Window::MODE_MAXIMIZED) {
		title_win_maximize_button->set_button_icon(make_window_restore_icon());
	} else {
		title_win_maximize_button->set_button_icon(make_window_maximize_icon());
	}
}

void MainWindow::_title_win_close() {
	if (get_tree()) {
		get_tree()->quit();
	}
}

void MainWindow::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
#ifdef MACOS_ENABLED
			if (DisplayServer::get_singleton()->has_feature(DisplayServerEnums::FEATURE_EXTEND_TO_TITLE)) {
				Window *rw = get_tree()->get_root();
				if (rw) {
					rw->connect(SNAME("nonclient_window_input"), callable_mp(this, &MainWindow::_on_nonclient_title_input));
				}
				callable_mp(this, &MainWindow::_sync_title_merge_layout).call_deferred();
			}
#endif
		} break;

		case NOTIFICATION_EXIT_TREE: {
#ifdef MACOS_ENABLED
			if (DisplayServer::get_singleton()->has_feature(DisplayServerEnums::FEATURE_EXTEND_TO_TITLE)) {
				Window *rw = get_tree()->get_root();
				if (rw) {
					const Callable cb = callable_mp(this, &MainWindow::_on_nonclient_title_input);
					if (rw->is_connected(SNAME("nonclient_window_input"), cb)) {
						rw->disconnect(SNAME("nonclient_window_input"), cb);
					}
				}
				Window *w = get_window();
				if (w) {
					w->set_nonclient_area(Rect2i());
				}
			}
#endif
		} break;

		case NOTIFICATION_RESIZED: {
			_sync_title_merge_layout();
			_update_title_win_maximize_icon();
			_sync_digital_view_layout();
		} break;

		case NOTIFICATION_READY: {
			set_anchors_preset(PRESET_FULL_RECT);
			set_theme(make_editor_like_theme());

			const Color col_title_bg(0.08f, 0.08f, 0.085f, 1.0f);
			const Color col_title_fg(1.0f, 1.0f, 1.0f, 0.75f);
			const Color col_bar(0.302f, 0.302f, 0.302f, 1.0f);
			const Color col_viewport_frame(0.80f, 1.0f, 0.55f, 1.0f);
			const Color col_margin(0.22f, 0.22f, 0.22f, 1.0f);

			VBoxContainer *root_col = memnew(VBoxContainer);
			root_col->set_anchors_preset(PRESET_FULL_RECT);
			root_col->add_theme_constant_override("separation", 0);
			add_child(root_col);

			title_bar = memnew(PanelContainer);
			title_bar->set_custom_minimum_size(Size2(0, 34));
			title_bar->add_theme_style_override("panel", make_flat_stylebox(col_title_bg));

			HBoxContainer *top_bar = memnew(HBoxContainer);
			top_bar->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

			PanelContainer *dao_badge = memnew(PanelContainer);
			dao_badge->set_custom_minimum_size(Size2(36, 28));
			dao_badge->add_theme_style_override("panel", make_flat_stylebox(Color(0.12f, 0.12f, 0.135f, 1.0f)));
			Label *dao_lbl = memnew(Label);
			dao_lbl->set_text(String::utf8(u8"DAO"));
			dao_lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			dao_lbl->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
			label_on_dark(dao_lbl);
			dao_badge->add_child(dao_lbl);

			top_bar->add_child(dao_badge);

			{
				Label *l = memnew(Label);
				l->set_mouse_filter(MOUSE_FILTER_IGNORE);
				l->set_text(String::utf8(u8"编辑(E)"));
				label_on_light(l, col_title_fg);
				top_bar->add_child(l);
			}
			{
				Label *l = memnew(Label);
				l->set_mouse_filter(MOUSE_FILTER_IGNORE);
				l->set_text(String::utf8(u8"选择(S)"));
				label_on_light(l, col_title_fg);
				top_bar->add_child(l);
			}
			{
				Label *l = memnew(Label);
				l->set_mouse_filter(MOUSE_FILTER_IGNORE);
				l->set_text(String::utf8(u8"查看(V)"));
				label_on_light(l, col_title_fg);
				top_bar->add_child(l);
			}

			title_drag_region = memnew(Control);
			title_drag_region->set_h_size_flags(SIZE_EXPAND_FILL);
			title_drag_region->set_mouse_filter(MOUSE_FILTER_STOP);
			title_drag_region->connect(SNAME("gui_input"), callable_mp(this, &MainWindow::_on_title_drag_region_gui_input));
			top_bar->add_child(title_drag_region);

#ifdef WINDOWS_ENABLED
			HBoxContainer *win_btns = memnew(HBoxContainer);
			auto add_win_btn = [&](const Ref<Texture2D> &p_icon, const Callable &p_cb) {
				Button *b = memnew(Button);
				configure_title_window_button(b);
				b->set_button_icon(p_icon);
				b->connect(SNAME("pressed"), p_cb);
				win_btns->add_child(b);
			};
			add_win_btn(make_window_minimize_icon(), callable_mp(this, &MainWindow::_title_win_minimize));
			title_win_maximize_button = memnew(Button);
			configure_title_window_button(title_win_maximize_button);
			title_win_maximize_button->connect(SNAME("pressed"), callable_mp(this, &MainWindow::_title_win_toggle_maximize));
			win_btns->add_child(title_win_maximize_button);
			_update_title_win_maximize_icon();
			add_win_btn(make_window_close_icon(), callable_mp(this, &MainWindow::_title_win_close));
			top_bar->add_child(win_btns);
#endif

			title_bar->add_child(top_bar);

			PanelContainer *body = memnew(PanelContainer);
			body->set_h_size_flags(SIZE_EXPAND_FILL);
			body->set_v_size_flags(SIZE_EXPAND_FILL);
			body->add_theme_style_override("panel", make_flat_stylebox(col_margin));

			const Color col_dock_bg(0.14f, 0.14f, 0.145f, 1.0f);
			Ref<ImageTexture> split_grabber_icon = make_split_grabber_icon();
			auto configure_hsplit_strip = [&](HSplitContainer *p_split) {
				p_split->set_dragger_visibility(SplitContainer::DRAGGER_VISIBLE);
				p_split->add_theme_constant_override("separation", 3);
				p_split->add_theme_constant_override("autohide", 1);
				p_split->add_theme_constant_override("minimum_grab_thickness", 6);
				p_split->add_theme_icon_override(SNAME("grabber"), split_grabber_icon);
				p_split->add_theme_style_override(SNAME("split_bar_background"), make_flat_stylebox(col_margin));
			};

			HSplitContainer *main_hsplit = memnew(HSplitContainer);
			main_hsplit->set_h_size_flags(SIZE_EXPAND_FILL);
			main_hsplit->set_v_size_flags(SIZE_EXPAND_FILL);
			configure_hsplit_strip(main_hsplit);

			PanelContainer *left_dock = memnew(PanelContainer);
			left_dock->set_custom_minimum_size(Size2(200, 0));
			left_dock->set_h_size_flags(SIZE_SHRINK_BEGIN | SIZE_FILL);
			left_dock->set_v_size_flags(SIZE_EXPAND_FILL);
			left_dock->add_theme_style_override("panel", make_flat_stylebox(col_dock_bg));
			TabContainer *left_tabs = memnew(TabContainer);
			left_tabs->set_h_size_flags(SIZE_EXPAND_FILL);
			left_tabs->set_v_size_flags(SIZE_EXPAND_FILL);
			MarginContainer *left_tree_margin = memnew(MarginContainer);
			left_tree_margin->add_theme_constant_override("margin_left", 4);
			left_tree_margin->add_theme_constant_override("margin_top", 4);
			left_tree_margin->add_theme_constant_override("margin_right", 4);
			left_tree_margin->add_theme_constant_override("margin_bottom", 4);
			structure_tree = memnew(Tree);
			structure_tree->set_h_size_flags(SIZE_EXPAND_FILL);
			structure_tree->set_v_size_flags(SIZE_EXPAND_FILL);
			structure_tree->set_columns(1);
			structure_tree->connect(SNAME("item_selected"), callable_mp(this, &MainWindow::_on_structure_tree_item_selected));
			left_tree_margin->add_child(structure_tree);
			left_tabs->add_child(left_tree_margin);
			left_dock->add_child(left_tabs);
			left_tabs->set_tab_title(0, String::utf8(u8"结构"));

			HSplitContainer *center_right = memnew(HSplitContainer);
			center_right->set_h_size_flags(SIZE_EXPAND_FILL);
			center_right->set_v_size_flags(SIZE_EXPAND_FILL);
			configure_hsplit_strip(center_right);

			PanelContainer *center_wrap = memnew(PanelContainer);
			center_wrap->set_h_size_flags(SIZE_EXPAND_FILL);
			center_wrap->set_v_size_flags(SIZE_EXPAND_FILL);
			center_wrap->set_clip_contents(true);
			center_wrap->add_theme_style_override("panel", make_flat_stylebox(col_margin));

			PanelContainer *viewport_frame = memnew(PanelContainer);
			viewport_frame->set_h_size_flags(SIZE_EXPAND_FILL);
			viewport_frame->set_v_size_flags(SIZE_EXPAND_FILL);
			viewport_frame->set_clip_contents(true);
			viewport_frame->add_theme_style_override("panel", make_bordered_stylebox(col_margin, col_viewport_frame));

			Control *digital_view_host = memnew(Control);
			digital_view_host->set_h_size_flags(SIZE_EXPAND_FILL);
			digital_view_host->set_v_size_flags(SIZE_EXPAND_FILL);
			digital_view_host->set_clip_contents(true);
			digital_view_host->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
			viewport_frame->add_child(digital_view_host);

			center_wrap->add_child(viewport_frame);

			PanelContainer *right_dock = memnew(PanelContainer);
			right_dock->set_custom_minimum_size(Size2(220, 0));
			right_dock->set_h_size_flags(SIZE_SHRINK_BEGIN | SIZE_FILL);
			right_dock->set_v_size_flags(SIZE_EXPAND_FILL);
			right_dock->add_theme_style_override("panel", make_flat_stylebox(col_dock_bg));
			TabContainer *right_tabs = memnew(TabContainer);
			right_tabs->set_h_size_flags(SIZE_EXPAND_FILL);
			right_tabs->set_v_size_flags(SIZE_EXPAND_FILL);
			ScrollContainer *right_scroll = memnew(ScrollContainer);
			right_scroll->set_h_size_flags(SIZE_EXPAND_FILL);
			right_scroll->set_v_size_flags(SIZE_EXPAND_FILL);
			VBoxContainer *props_vbox = memnew(VBoxContainer);
			props_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
			auto add_prop_block = [&](const String &p_caption, Label **r_value) {
				Label *cap = memnew(Label);
				cap->set_text(p_caption);
				label_on_dark(cap);
				props_vbox->add_child(cap);
				Label *val = memnew(Label);
				val->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
				label_on_dark(val);
				props_vbox->add_child(val);
				*r_value = val;
			};
			add_prop_block(String::utf8(u8"名称"), &prop_name_label);
			add_prop_block(String::utf8(u8"类型"), &prop_class_label);
			add_prop_block(String::utf8(u8"节点路径"), &prop_path_label);
			right_scroll->add_child(props_vbox);
			right_tabs->add_child(right_scroll);
			right_dock->add_child(right_tabs);
			right_tabs->set_tab_title(0, String::utf8(u8"属性"));

			main_hsplit->add_child(left_dock);
			main_hsplit->add_child(center_right);
			center_right->add_child(center_wrap);
			center_right->add_child(right_dock);

			body->add_child(main_hsplit);

			PanelContainer *status = memnew(PanelContainer);
			status->set_custom_minimum_size(Size2(0, 28));
			status->add_theme_style_override("panel", make_flat_stylebox(col_bar));
			status_label = memnew(Label);
			status_label->set_text(String::utf8(u8"状态 - 就绪"));
			status_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
			label_on_dark(status_label);
			status->add_child(status_label);

			if (_ensure_digital_view_extension_loaded()) {
				Object *digital_view_object = ClassDB::instantiate(SNAME("DigitalView"));
				digital_view = Object::cast_to<Control>(digital_view_object);
				if (digital_view) {
					digital_view->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
					digital_view_host->add_child(digital_view);
				} else if (digital_view_object) {
					memdelete(digital_view_object);
					ERR_PRINT("DigitalView extension registered a class that is not a Control.");
				}
			}

			const Callable sync_digital_view_cb = callable_mp(this, &MainWindow::_sync_digital_view_layout);
			main_hsplit->connect(SNAME("resized"), sync_digital_view_cb);
			center_right->connect(SNAME("resized"), sync_digital_view_cb);
			center_wrap->connect(SNAME("resized"), sync_digital_view_cb);
			viewport_frame->connect(SNAME("resized"), sync_digital_view_cb);
			digital_view_host->connect(SNAME("resized"), sync_digital_view_cb);

			root_col->add_child(title_bar);
			root_col->add_child(body);
			root_col->add_child(status);

			_refresh_structure_tree();

			callable_mp(this, &MainWindow::_sync_title_merge_layout).call_deferred();
		} break;

		default: {
			break;
		}
	}
}

MainWindow::MainWindow() = default;

} // namespace dw
