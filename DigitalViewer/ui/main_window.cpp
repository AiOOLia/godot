/**************************************************************************/
/*  main_window.cpp                                                       */
/**************************************************************************/
/*                         This is part of DigitalViewer fork app code.    */
/**************************************************************************/

#include "DigitalViewer/ui/main_window.h"

#include "DigitalViewer/builtin_fonts.gen.h"

#include "modules/modules_enabled.gen.h"

#include "core/math/math_defs.h"
#include "core/object/callable_mp.h"
#include "core/object/object.h"
#include "core/string/string_name.h"
#include "scene/main/node.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/center_container.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/environment.h"
#include "scene/resources/font.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/theme.h"

#include "core/io/image.h"
#include "core/io/image_loader.h"
#include "scene/gui/subviewport_container.h"
#include "scene/gui/button.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/scene_string_names.h"
#include "servers/display/display_server.h"
#include "servers/display/display_server_enums.h"

#ifdef MODULE_GLTF_ENABLED
#include "modules/gltf/gltf_document.h"
#include "modules/gltf/gltf_state.h"
#endif

static Ref<StyleBoxFlat> _dv_style_bg(const Color &p_color) {
	Ref<StyleBoxFlat> s;
	s.instantiate();
	s->set_bg_color(p_color);
	s->set_border_width_all(0);
	return s;
}


static Ref<ImageTexture> _dv_split_grabber_icon() {
	Ref<Image> img;
	img.instantiate();
	if (ImageLoader::load_image("res://icons/GuiHsplitter.svg", img) != OK) {
		img->initialize_data(2, 64, false, Image::FORMAT_RGBA8);
		img->fill(Color(0, 0, 0, 0));
		for (int y = 2; y < 62; y++) {
			img->set_pixel(0, y, Color(1, 1, 1, 0.4f));
			img->set_pixel(1, y, Color(1, 1, 1, 0.4f));
		}
	}
	return ImageTexture::create_from_image(img);
}

static Ref<FontFile> _dv_load_internal_font(const uint8_t *p_data, size_t p_size, TypedArray<Font> *r_fallbacks = nullptr) {
	Ref<FontFile> font;
	font.instantiate();
	font->set_data_ptr(p_data, p_size);
	font->set_antialiasing(TextServer::FONT_ANTIALIASING_GRAY);
	font->set_hinting(TextServer::HINTING_LIGHT);
	font->set_force_autohinter(true);
	font->set_subpixel_positioning(TextServer::SUBPIXEL_POSITIONING_AUTO);
	font->set_disable_embedded_bitmaps(true);
	if (r_fallbacks) {
		r_fallbacks->push_back(font);
	}
	return font;
}

static float _dv_get_editor_auto_display_scale() {
	DisplayServer *ds = DisplayServer::get_singleton();
	ERR_FAIL_NULL_V(ds, 1.0f);

#if defined(MACOS_ENABLED) || defined(ANDROID_ENABLED)
	return ds->screen_get_max_scale();
#else
	const int screen = ds->window_get_current_screen();
	if (ds->screen_get_size(screen) == Vector2i()) {
		return 1.0f;
	}

#if defined(WINDOWS_ENABLED)
	return ds->screen_get_dpi(screen) / 96.0f;
#else
	const int smallest_dimension = MIN(ds->screen_get_size(screen).x, ds->screen_get_size(screen).y);
	if (ds->screen_get_dpi(screen) >= 192 && smallest_dimension >= 1400) {
		return 2.0f;
	} else if (smallest_dimension >= 1700) {
		return 1.5f;
	} else if (smallest_dimension <= 800) {
		return 0.75f;
	}
	return 1.0f;
#endif
#endif
}

static Ref<Theme> _dv_make_editor_like_theme() {
	TypedArray<Font> fallbacks;
	_dv_load_internal_font(_font_DroidSansFallback, _font_DroidSansFallback_size, &fallbacks);
	Ref<FontFile> main_font = _dv_load_internal_font(_font_Inter_Regular, _font_Inter_Regular_size);
	main_font->set_fallbacks(fallbacks);

	Ref<Theme> theme;
	theme.instantiate();
	theme->set_default_font(main_font);
	const int default_font_size = MAX(1, (int)(14 * _dv_get_editor_auto_display_scale()));
	theme->set_default_font_size(default_font_size);
	return theme;
}

static void _dv_label_on_dark(Label *p_label) {
	ERR_FAIL_NULL(p_label);
	p_label->add_theme_color_override(SceneStringName(font_color), Color(0.94f, 0.94f, 0.94f, 1.0f));
}

static void _dv_label_on_light(Label *p_label, const Color &p_fg) {
	ERR_FAIL_NULL(p_label);
	p_label->add_theme_color_override(SceneStringName(font_color), p_fg);
}

#ifndef _3D_DISABLED
static void _dv_accum_visual_aabb(Node *p_node, AABB &r_aabb, bool &r_first) {
	VisualInstance3D *vi = Object::cast_to<VisualInstance3D>(p_node);
	if (vi) {
		const AABB aabb = vi->get_global_transform().xform(vi->get_aabb());
		if (aabb.has_surface()) {
			if (r_first) {
				r_aabb = aabb;
				r_first = false;
			} else {
				r_aabb.merge_with(aabb);
			}
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_dv_accum_visual_aabb(p_node->get_child(i), r_aabb, r_first);
	}
}
#endif

namespace dw {

void MainWindow::_bind_methods() {
}

void MainWindow::_on_import_menu_id_pressed(int p_id) {
	if (p_id == MENU_IMPORT_GLTF && file_dialog) {
		file_dialog->popup_centered_ratio(0.55);
	}
}

void MainWindow::_on_file_selected(const String &p_path) {
	ERR_FAIL_NULL_MSG(status_label, "MainWindow: status_label not built.");

#ifndef _3D_DISABLED
	ERR_FAIL_NULL_MSG(model_holder, "MainWindow: model_holder not built.");
	ERR_FAIL_NULL_MSG(view_camera, "MainWindow: view_camera not built.");
#endif

	if (status_label) {
		status_label->set_text(String::utf8(u8"状态 — 正在加载… ") + p_path.get_file());
	}

#ifndef _3D_DISABLED

#ifdef MODULE_GLTF_ENABLED
	_clear_model_holder();

	Ref<GLTFDocument> doc;
	doc.instantiate();
	Ref<GLTFState> state;
	state.instantiate();

	const Error err = doc->append_from_file(p_path, state);
	if (err != OK) {
		status_label->set_text(String::utf8(u8"状态 — 加载失败: ") + p_path.get_file());
		_refresh_structure_tree();
		return;
	}

	Node *imported = doc->generate_scene(state);
	if (!imported) {
		status_label->set_text(String::utf8(u8"状态 — 无法生成场景: ") + p_path.get_file());
		_refresh_structure_tree();
		return;
	}

	model_holder->add_child(imported);
	_frame_camera_to_contents();
	_refresh_structure_tree();

	status_label->set_text(String::utf8(u8"状态 — 已加载 ") + p_path.get_file());
#else
	status_label->set_text(String::utf8(u8"状态 — 构建未包含 glTF 模块，请开启 module_gltf_enabled 后重新编译"));
#endif

#else
	status_label->set_text(String::utf8(u8"状态 — 此构建禁用了 3D (disable_3d)，无法显示模型"));
#endif
}

#ifndef _3D_DISABLED
void MainWindow::_clear_model_holder() {
	ERR_FAIL_NULL(model_holder);
	while (model_holder->get_child_count() > 0) {
		Node *c = model_holder->get_child(0);
		model_holder->remove_child(c);
		memdelete(c);
	}
}

void MainWindow::_frame_camera_to_contents() {
	ERR_FAIL_NULL(view_camera);
	ERR_FAIL_NULL(model_holder);

	AABB bounds;
	bool first = true;
	_dv_accum_visual_aabb(model_holder, bounds, first);

	if (first || !bounds.has_surface()) {
		view_camera->set_position(Vector3(2.0, 1.5, 4.0));
		view_camera->look_at(Vector3(0, 0, 0), Vector3(0, 1, 0));
		return;
	}

	const Vector3 center = bounds.get_center();
	real_t size = bounds.get_longest_axis_size();
	if (size < CMP_EPSILON) {
		size = 1.0;
	}
	const real_t dist = size * 1.35;
	const Vector3 dir = Vector3(-1.0, 0.45, 1.0).normalized();
	view_camera->set_position(center + dir * dist);
	view_camera->look_at(center, Vector3(0, 1, 0));
}
#endif

void MainWindow::_structure_tree_fill_branch(TreeItem *p_parent, Node *p_node) {
	ERR_FAIL_NULL(structure_tree);
	ERR_FAIL_NULL(p_node);
	TreeItem *item = structure_tree->create_item(p_parent);
	item->set_text(0, p_node->get_name());
	item->set_tooltip_text(0, p_node->get_class());
	item->set_metadata(0, p_node->get_instance_id());
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_structure_tree_fill_branch(item, p_node->get_child(i));
	}
}

void MainWindow::_refresh_structure_tree() {
	ERR_FAIL_NULL(structure_tree);
	structure_tree->clear();
	structure_tree->set_hide_root(true);

#ifndef _3D_DISABLED
	if (!model_holder || model_holder->get_child_count() == 0) {
		TreeItem *hint = structure_tree->create_item();
		hint->set_text(0, String::utf8(u8"（未加载模型）"));
		hint->set_selectable(0, false);
		_update_properties_panel(nullptr);
		return;
	}
	for (int i = 0; i < model_holder->get_child_count(); i++) {
		_structure_tree_fill_branch(nullptr, model_holder->get_child(i));
	}
#else
	{
		TreeItem *hint = structure_tree->create_item();
		hint->set_text(0, String::utf8(u8"（此构建未启用 3D）"));
		hint->set_selectable(0, false);
	}
#endif
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
		} break;

		case NOTIFICATION_READY: {
			set_anchors_preset(PRESET_FULL_RECT);
			set_theme(_dv_make_editor_like_theme());

			// IDE-style merged title strip + rest of app (dark framed viewport).
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
			title_bar->add_theme_style_override("panel", _dv_style_bg(col_title_bg));

			HBoxContainer *top_bar = memnew(HBoxContainer);
			top_bar->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

			PanelContainer *dao_badge = memnew(PanelContainer);
			dao_badge->set_custom_minimum_size(Size2(36, 28));
			dao_badge->add_theme_style_override("panel", _dv_style_bg(Color(0.12f, 0.12f, 0.135f, 1.0f)));
			Label *dao_lbl = memnew(Label);
			dao_lbl->set_text(String::utf8(u8"DAO"));
			dao_lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			dao_lbl->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
			_dv_label_on_dark(dao_lbl);
			dao_badge->add_child(dao_lbl);

			MenuButton *file_menu = memnew(MenuButton);
			file_menu->set_text(String::utf8(u8"文件(F)"));
			file_menu->set_flat(true);
			file_menu->add_theme_color_override(SceneStringName(font_color), col_title_fg);
			file_menu->add_theme_color_override("font_hover_color", col_title_fg);
			file_menu->add_theme_color_override("font_pressed_color", col_title_fg);
			PopupMenu *popup = file_menu->get_popup();
			popup->add_item(String::utf8(u8"导入GLTF"), MENU_IMPORT_GLTF);
			popup->connect(SNAME("id_pressed"), callable_mp(this, &MainWindow::_on_import_menu_id_pressed));

			top_bar->add_child(dao_badge);
			top_bar->add_child(file_menu);

			{
				Label *l = memnew(Label);
				l->set_mouse_filter(MOUSE_FILTER_IGNORE);
				l->set_text(String::utf8(u8"编辑(E)"));
				_dv_label_on_light(l, col_title_fg);
				top_bar->add_child(l);
			}
			{
				Label *l = memnew(Label);
				l->set_mouse_filter(MOUSE_FILTER_IGNORE);
				l->set_text(String::utf8(u8"选择(S)"));
				_dv_label_on_light(l, col_title_fg);
				top_bar->add_child(l);
			}
			{
				Label *l = memnew(Label);
				l->set_mouse_filter(MOUSE_FILTER_IGNORE);
				l->set_text(String::utf8(u8"查看(V)"));
				_dv_label_on_light(l, col_title_fg);
				top_bar->add_child(l);
			}

			title_drag_region = memnew(Control);
			title_drag_region->set_h_size_flags(SIZE_EXPAND_FILL);
			title_drag_region->set_mouse_filter(MOUSE_FILTER_STOP);
			title_drag_region->connect(SNAME("gui_input"), callable_mp(this, &MainWindow::_on_title_drag_region_gui_input));
			top_bar->add_child(title_drag_region);

#ifdef WINDOWS_ENABLED
			HBoxContainer *win_btns = memnew(HBoxContainer);
			auto add_win_btn = [&](const String &p_text, const Callable &p_cb) {
				Button *b = memnew(Button);
				b->set_flat(true);
				b->set_text(p_text);
				b->set_custom_minimum_size(Size2(40, 28));
				b->add_theme_color_override(SceneStringName(font_color), col_title_fg);
				//b->add_theme_font_size_override(SceneStringName(font_size), 6);
				b->connect(SNAME("pressed"), p_cb);
				win_btns->add_child(b);
			};
			add_win_btn(String::utf8(u8"—"), callable_mp(this, &MainWindow::_title_win_minimize));
			add_win_btn(String::utf8(u8"\u25A1"), callable_mp(this, &MainWindow::_title_win_toggle_maximize));
			add_win_btn(String::utf8(u8"\u2715"), callable_mp(this, &MainWindow::_title_win_close));
			top_bar->add_child(win_btns);
#endif

			title_bar->add_child(top_bar);

			PanelContainer *body = memnew(PanelContainer);
			body->set_h_size_flags(SIZE_EXPAND_FILL);
			body->set_v_size_flags(SIZE_EXPAND_FILL);
			body->add_theme_style_override("panel", _dv_style_bg(col_margin));

			const Color col_dock_bg(0.14f, 0.14f, 0.145f, 1.0f);
			Ref<ImageTexture> split_grabber_icon = _dv_split_grabber_icon();
			auto configure_hsplit_strip = [&](HSplitContainer *p_split) {
				p_split->set_dragger_visibility(SplitContainer::DRAGGER_VISIBLE);
				p_split->add_theme_constant_override("separation", 3);
				p_split->add_theme_constant_override("autohide", 1);
				p_split->add_theme_constant_override("minimum_grab_thickness", 6);
				p_split->add_theme_icon_override(SNAME("grabber"), split_grabber_icon);
			};

			HSplitContainer *main_hsplit = memnew(HSplitContainer);
			main_hsplit->set_h_size_flags(SIZE_EXPAND_FILL);
			main_hsplit->set_v_size_flags(SIZE_EXPAND_FILL);
			configure_hsplit_strip(main_hsplit);

			PanelContainer *left_dock = memnew(PanelContainer);
			left_dock->set_custom_minimum_size(Size2(200, 0));
			left_dock->set_h_size_flags(SIZE_SHRINK_BEGIN | SIZE_FILL);
			left_dock->set_v_size_flags(SIZE_EXPAND_FILL);
			left_dock->add_theme_style_override("panel", _dv_style_bg(col_dock_bg));
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
			center_wrap->add_theme_style_override("panel", _dv_style_bg(col_margin));

			VBoxContainer *viewport_stack = memnew(VBoxContainer);
			viewport_stack->set_h_size_flags(SIZE_EXPAND_FILL);
			viewport_stack->set_v_size_flags(SIZE_EXPAND_FILL);
			viewport_stack->add_theme_constant_override("separation", 0);

			PanelContainer *green_frame = memnew(PanelContainer);
			green_frame->set_h_size_flags(SIZE_EXPAND_FILL);
			green_frame->set_v_size_flags(SIZE_EXPAND_FILL);
			green_frame->add_theme_style_override("panel", _dv_style_bg(col_viewport_frame));

			SubViewportContainer *svc = memnew(SubViewportContainer);
			svc->set_h_size_flags(SIZE_EXPAND_FILL);
			svc->set_v_size_flags(SIZE_EXPAND_FILL);
			svc->set_stretch(true);

			SubViewport *sv = memnew(SubViewport);
			sv->set_handle_input_locally(true);
			sv->set_update_mode(SubViewport::UPDATE_WHEN_VISIBLE);

#ifndef _3D_DISABLED
			model_viewport = sv;

			Ref<Environment> env;
			env.instantiate();
			env->set_background(Environment::BG_COLOR);
			env->set_bg_color(col_viewport_frame);

			Ref<World3D> w3d;
			w3d.instantiate();
			w3d->set_environment(env);
			sv->set_world_3d(w3d);

			Node3D *world = memnew(Node3D);

			model_holder = memnew(Node3D);
			world->add_child(model_holder);

			DirectionalLight3D *sun = memnew(DirectionalLight3D);
			sun->set_rotation_degrees(Vector3(-42.0, 58.0, 0));
			sun->set_shadow(true);
			world->add_child(sun);

			Camera3D *cam = memnew(Camera3D);
			cam->set_current(true);
			cam->set_position(Vector3(2.0, 1.5, 4.0));
			cam->look_at(Vector3(0, 0, 0), Vector3(0, 1, 0));
			view_camera = cam;
			world->add_child(cam);

			sv->add_child(world);
#endif

			svc->add_child(sv);
			green_frame->add_child(svc);
			viewport_stack->add_child(green_frame);
			center_wrap->add_child(viewport_stack);

			PanelContainer *right_dock = memnew(PanelContainer);
			right_dock->set_custom_minimum_size(Size2(220, 0));
			right_dock->set_h_size_flags(SIZE_SHRINK_BEGIN | SIZE_FILL);
			right_dock->set_v_size_flags(SIZE_EXPAND_FILL);
			right_dock->add_theme_style_override("panel", _dv_style_bg(col_dock_bg));
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
				_dv_label_on_dark(cap);
				props_vbox->add_child(cap);
				Label *val = memnew(Label);
				val->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
				_dv_label_on_dark(val);
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
			status->add_theme_style_override("panel", _dv_style_bg(col_bar));
			status_label = memnew(Label);
			status_label->set_text(String::utf8(u8"状态 — 就绪"));
			status_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
			_dv_label_on_dark(status_label);
			status->add_child(status_label);

			root_col->add_child(title_bar);
			root_col->add_child(body);
			root_col->add_child(status);

			file_dialog = memnew(FileDialog);
			file_dialog->set_title(String::utf8(u8"导入 glTF / glb"));
			file_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
			file_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
			file_dialog->clear_filters();
			file_dialog->add_filter("*.gltf", "glTF");
			file_dialog->add_filter("*.glb", "glTF Binary");
			file_dialog->connect(SNAME("file_selected"), callable_mp(this, &MainWindow::_on_file_selected));
			file_dialog->set_force_native(true);
			add_child(file_dialog);

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
