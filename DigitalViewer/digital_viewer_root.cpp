/**************************************************************************/
/*  digital_viewer_root.cpp                                               */
/**************************************************************************/
/*                         This is part of DigitalViewer fork app code.    */
/**************************************************************************/

#include "digital_viewer_root.h"

#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/split_container.h"
#include "scene/gui/text_edit.h"
#include "scene/resources/style_box_flat.h"

static Ref<StyleBoxFlat> _dv_style_bg(const Color &p_color) {
	Ref<StyleBoxFlat> s;
	s.instantiate();
	s->set_bg_color(p_color);
	s->set_border_width_all(0);
	return s;
}

void DigitalViewerRoot::_bind_methods() {
}

void DigitalViewerRoot::_notification(int p_what) {
	if (p_what != NOTIFICATION_READY) {
		return;
	}

	set_anchors_preset(PRESET_FULL_RECT);

	// C string literals must use String::utf8(u8"..."): String(const char*) treats bytes as Latin-1, not UTF-8.
	// Dark palette loosely inspired by VS Code / modern IDEs.
	const Color col_side(0.16f, 0.16f, 0.17f, 1.0f);
	const Color col_activity(0.10f, 0.10f, 0.11f, 1.0f);
	const Color col_editor(0.14f, 0.14f, 0.15f, 1.0f);
	const Color col_status(0.00f, 0.49f, 0.80f, 1.0f); // Accent strip

	HSplitContainer *root_split = memnew(HSplitContainer);
	root_split->set_anchors_preset(PRESET_FULL_RECT);
	add_child(root_split);

	// Left: activity bar + explorer.
	HSplitContainer *left_split = memnew(HSplitContainer);
	left_split->set_custom_minimum_size(Size2(260, 0));

	PanelContainer *activity = memnew(PanelContainer);
	activity->set_custom_minimum_size(Size2(48, 0));
	activity->add_theme_style_override("panel", _dv_style_bg(col_activity));
	VBoxContainer *activity_inner = memnew(VBoxContainer);
	Label *activity_hint = memnew(Label);
	activity_hint->set_text("DV");
	activity_hint->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	activity_inner->add_child(activity_hint);
	activity->add_child(activity_inner);

	PanelContainer *explorer = memnew(PanelContainer);
	explorer->add_theme_style_override("panel", _dv_style_bg(col_side));
	VBoxContainer *ex_col = memnew(VBoxContainer);
	Label *ex_title = memnew(Label);
	ex_title->set_text(String::utf8(u8"资源管理器"));
	ex_col->add_child(ex_title);
	Label *ex_hint = memnew(Label);
	ex_hint->set_text(String::utf8(u8"（占位）在此挂载文件树、搜索等"));
	ex_hint->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	ex_col->add_child(ex_hint);
	explorer->add_child(ex_col);

	left_split->add_child(activity);
	left_split->add_child(explorer);
	left_split->set_split_offset(48);

	root_split->add_child(left_split);

	// Right: main column (tab strip + editor + status).
	VBoxContainer *main_col = memnew(VBoxContainer);
	main_col->set_h_size_flags(SIZE_EXPAND_FILL);

	PanelContainer *tab_strip = memnew(PanelContainer);
	tab_strip->set_custom_minimum_size(Size2(0, 36));
	tab_strip->add_theme_style_override("panel", _dv_style_bg(col_side));
	Label *tab_lbl = memnew(Label);
	tab_lbl->set_text(String::utf8(u8"  DigitalViewer — 主窗口"));
	tab_strip->add_child(tab_lbl);

	PanelContainer *editor_panel = memnew(PanelContainer);
	editor_panel->set_v_size_flags(SIZE_EXPAND_FILL);
	editor_panel->add_theme_style_override("panel", _dv_style_bg(col_editor));
	TextEdit *code = memnew(TextEdit);
	code->set_anchors_preset(PRESET_FULL_RECT);
	code->set_placeholder(
			String::utf8(u8"// 类似 VS Code 中央编辑区 — 可换为你的查看器 / 脚本编辑 / 日志等。\n"
						   u8"// 本程序可无 project.godot 运行；res:// 对应可执行文件所在目录。\n"));
	editor_panel->add_child(code);

	PanelContainer *status = memnew(PanelContainer);
	status->set_custom_minimum_size(Size2(0, 24));
	status->add_theme_style_override("panel", _dv_style_bg(col_status));
	Label *status_lbl = memnew(Label);
	status_lbl->set_text(String::utf8(u8"  就绪  —  DigitalViewer"));
	status->add_child(status_lbl);

	main_col->add_child(tab_strip);
	main_col->add_child(editor_panel);
	main_col->add_child(status);

	root_split->add_child(main_col);
	root_split->set_split_offset(268);
}

DigitalViewerRoot::DigitalViewerRoot() {
}
