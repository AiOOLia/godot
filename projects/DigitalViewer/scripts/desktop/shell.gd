extends Control
const Pane = preload("res://scripts/desktop/pane.gd")
const Strings = preload("res://scripts/desktop/strings.gd")
const Workbench = preload("res://scripts/scripting/script_workbench.gd")
const Bridge = preload("res://scripts/desktop/mcp_server.gd")
const PluginBridge = preload("res://scripts/desktop/plugin_bridge.gd")
const Dpi = preload("res://scripts/desktop/dpi_scaling.gd")
const Ui = preload("res://scripts/desktop/ui_widgets.gd")
const Profile = preload("res://scripts/desktop/build_profile.gd")
const Sources = preload("res://scripts/desktop/source_resolver.gd")
var panes: Dictionary = {}
var _workspace: Control
var _full: Control
var _layout_root: Control
var _properties: PanelContainer
var _property_fields: VBoxContainer
var _property_scroll: ScrollContainer
var _script: Control
var _center: VSplitContainer
var _status: Label
var _fps: Label
var _windows: Dictionary = {}
var _slots: Dictionary = {}
var _full_id := ""
var _add: Control
var _add_target: Label
var _add_source_buttons: Array[Button] = []
var _add_browse: Button
var _add_folder: Button
var _add_script_hint: Label
var _add_zip_label: Label
var _add_source_row: HBoxContainer
var _pending_box: VBoxContainer
var _pending_scroll: ScrollContainer
var _pending_summary: Label
var _pending_row: HBoxContainer
var _pending_sources: Array[String] = []
var _last_add_directory := ""
var _source: LineEdit
var _kind: OptionButton
var _zip_entry: LineEdit
var _ion_id: SpinBox
var _ion_token: LineEdit
var _ion_fields: VBoxContainer
var _http_fields: VBoxContainer
var _http_headers: TextEdit
var _axis: OptionButton
var _file: FileDialog
var _target_id := ""
var _confirm: ConfirmationDialog
var _pending: Callable
var _diagnostics: AcceptDialog
var _report: TextEdit
var _properties_dirty := false
var _brep_inspector: Node
var _elapsed := 0.0
var _automation_mode := "mcp"
var _panel_buttons: Dictionary = {}
var _property_title: Label
var _slider_grabber: Texture2D
var _slider_grabber_highlight: Texture2D
var _settings_mode := false
var _recording_id := ""
var _recording_dir := ""
var _recording_frames := 0
var _record_timer: Timer
var _split_h_grabber: ImageTexture
var _split_v_grabber: ImageTexture

func _ready() -> void:
	if not OS.has_feature("editor"):
		var mismatch := Profile.template_error()
		if not mismatch.is_empty():
			push_error(mismatch)
			AppState.shutdown()
			get_tree().quit(1)
			return
	if "--dv-check-template" in OS.get_cmdline_user_args():
		print("DIGITALVIEWER_TEMPLATE_OK " + Profile.id())
		AppState.shutdown()
		get_tree().quit()
		return
	Strings.install()
	theme = load(Profile.value("theme")) as Theme
	get_window().title = Profile.value("product_name")
	Engine.max_fps = 60
	get_tree().auto_accept_quit = false
	get_viewport().gui_embed_subwindows = false
	_apply_window_icon()
	add_child(Dpi.new())
	_build()
	_build_dialogs()
	_brep_inspector = preload("res://scripts/desktop/brep_inspector.gd").new()
	add_child(_brep_inspector)
	_brep_inspector.completed.connect(_state_changed)
	AppState.changed.connect(_state_changed)
	AppState.selection_changed.connect(func():
		_settings_mode = false
		_refresh_properties())
	AppState.status_changed.connect(func(message: String): _status.text = message)
	AppState.draft_received.connect(_open_draft)
	AppState.scripts.opened.connect(_open_script)
	AppState.workbench = _script
	AppState.runner = _script.runner
	_automation_mode = "plugin" if "--dv-plugin-check" in OS.get_cmdline_user_args() else "mcp"
	AppState.bridge = _make_bridge(_automation_mode)
	if ("--dv-smoke-test" in OS.get_cmdline_user_args() or "--dv-mcp-check" in OS.get_cmdline_user_args() or "--dv-plugin-check" in OS.get_cmdline_user_args()) and ResourceLoader.exists("res://tests/desktop_smoke.gd"):
		var test_directory := OS.get_environment("DV_TEST_DIRECTORY")
		if test_directory.is_empty(): test_directory = "res://.godot/desktop-zip-test"
		AppState.bridge.token_path = test_directory.path_join("mcp/bearer_token")
		AppState.bridge.port = 18732 if AppState.bridge.is_plugin() else 18731
	add_child(AppState.bridge)
	AppState.bridge.start()
	get_window().files_dropped.connect(func(files: PackedStringArray):
		for file in files: AppState.add_source(AppState.active_id, file))
	_refresh_properties()
	if "--dv-brep-test" in OS.get_cmdline_user_args():
		var brep_test: Node = load("res://tests/brep_smoke.gd").new()
		add_child(brep_test)
		brep_test.run.call_deferred(self)
	if "--dv-profile-test" in OS.get_cmdline_user_args():
		if not ResourceLoader.exists("res://tests/desktop_profile.gd"):
			push_error("Use the Windows Validation export preset for profile tests.")
			get_tree().quit(1)
			return
		var profile_test: Node = load("res://tests/desktop_profile.gd").new()
		add_child(profile_test)
		profile_test.run.call_deferred(self)
	if "--dv-smoke-test" in OS.get_cmdline_user_args():
		if not ResourceLoader.exists("res://tests/desktop_smoke.gd"):
			push_error("Use the Windows Validation export preset for packaged tests.")
			get_tree().quit(1)
			return
		var test: Node = load("res://tests/desktop_smoke.gd").new()
		add_child(test)
		test.run.call_deferred(self)

func _apply_window_icon() -> void:
	if DisplayServer.get_name() == "headless":
		return
	var svg := FileAccess.get_file_as_string(Profile.value("icon"))
	if svg.is_empty():
		return
	var image := Image.new()
	if image.load_svg_from_string(svg, 1.0) != OK:
		return
	DisplayServer.set_icon(image)

func _build() -> void:
	var background := ColorRect.new()
	background.color = Ui.BG
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background)
	var root := VBoxContainer.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_theme_constant_override("separation", 3)
	add_child(root)
	var top_panel := PanelContainer.new()
	top_panel.custom_minimum_size.y = 30
	top_panel.add_theme_stylebox_override("panel", Ui.box(Ui.RAISED, Ui.LINE, 0, 0, Vector4(10, 0, 10, 0)))
	root.add_child(top_panel)
	var top := HBoxContainer.new()
	top.add_theme_constant_override("separation", 6)
	top.alignment = BoxContainer.ALIGNMENT_BEGIN
	top_panel.add_child(top)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(spacer)
	var props_button := Ui.text_icon_button(top, "properties", tr("Properties"), func(): _show_panel("properties"), true)
	var script_button := Ui.text_icon_button(top, "script", tr("Scripts"), func(): _show_panel("script"))
	_panel_buttons = {"properties": props_button, "script": script_button}
	for button in _panel_buttons.values():
		button.custom_minimum_size.y = 26
		Ui.style_button(button, button == props_button, true)
	_fps = Label.new()
	_fps.text = "60 FPS"
	_fps.add_theme_color_override("font_color", Ui.ACCENT)
	_fps.add_theme_font_size_override("font_size", 11)
	_fps.custom_minimum_size = Vector2(52, 20)
	_fps.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_fps.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	var fps_pill := PanelContainer.new()
	fps_pill.custom_minimum_size = Vector2(68, 24)
	fps_pill.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	fps_pill.add_theme_stylebox_override("panel", Ui.box(Ui.ACCENT_SOFT, Color(Ui.ACCENT, 0.25), 99, 1, Vector4(7, 1, 7, 1)))
	var fps_row := HBoxContainer.new()
	fps_row.add_theme_constant_override("separation", 5)
	fps_row.alignment = BoxContainer.ALIGNMENT_CENTER
	var fps_dot := PanelContainer.new()
	fps_dot.add_theme_stylebox_override("panel", Ui.box(Ui.ACCENT, Color.TRANSPARENT, 3, 0, Vector4.ZERO))
	fps_dot.custom_minimum_size = Vector2(6, 6)
	fps_dot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	fps_row.add_child(fps_dot)
	fps_row.add_child(_fps)
	fps_pill.add_child(fps_row)
	top.add_child(fps_pill)
	Ui.icon_button(top, "settings", tr("Settings"), _toggle_settings, false, 26, 16)
	var outer := HSplitContainer.new()
	_style_split(outer)
	outer.size_flags_vertical = Control.SIZE_EXPAND_FILL
	outer.split_offset = -320
	root.add_child(outer)
	_center = VSplitContainer.new()
	_style_split(_center)
	_center.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_center.split_offset = -340
	outer.add_child(_center)
	_workspace = Control.new()
	_workspace.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_center.add_child(_workspace)
	_layout_root = _new_pane(AppState.active_id)
	_workspace.add_child(_layout_root)
	_layout_root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_script = Workbench.new()
	_script.docked = true
	_script.custom_minimum_size.y = 220
	_center.add_child(_script)
	_script.detach_requested.connect(func(): _float_panel("script"))
	_script.closed.connect(_close_panel.bind("script"))
	_properties = PanelContainer.new()
	_properties.custom_minimum_size.x = 320
	_properties.add_theme_stylebox_override("panel", Ui.box(Ui.SURFACE, Ui.LINE, 0, 1, Vector4(12, 10, 2, 10)))
	outer.add_child(_properties)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 8)
	_properties.add_child(box)
	var header := HBoxContainer.new()
	box.add_child(header)
	var prop_title := _label(header, tr("Properties"))
	_property_title = prop_title
	prop_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	prop_title.add_theme_font_size_override("font_size", 13)
	prop_title.add_theme_color_override("font_color", Ui.TEXT)
	if not OS.has_feature("web"): Ui.icon_button(header, "popout", tr("Detach / dock"), func(): _float_panel("properties"), false, 26, 14)
	Ui.icon_button(header, "close", tr("Close"), func(): _close_panel("properties"), false, 26, 14)
	var scroll := ScrollContainer.new()
	_property_scroll = scroll
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	box.add_child(scroll)
	var content_margin := MarginContainer.new()
	content_margin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	content_margin.add_theme_constant_override("margin_right", 3)
	scroll.add_child(content_margin)
	_property_fields = VBoxContainer.new()
	_property_fields.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_property_fields.add_theme_constant_override("separation", 10)
	content_margin.add_child(_property_fields)
	scroll.get_v_scroll_bar().custom_minimum_size.x = 6
	var bottom_panel := PanelContainer.new()
	bottom_panel.custom_minimum_size.y = 28
	bottom_panel.add_theme_stylebox_override("panel", Ui.box(Ui.SURFACE, Ui.LINE, 0, 0, Vector4(10, 0, 10, 0)))
	root.add_child(bottom_panel)
	var bottom := HBoxContainer.new()
	bottom.add_theme_constant_override("separation", 7)
	bottom_panel.add_child(bottom)
	var status_dot := PanelContainer.new()
	status_dot.add_theme_stylebox_override("panel", Ui.box(Ui.ACCENT, Color.TRANSPARENT, 3, 0, Vector4.ZERO))
	status_dot.custom_minimum_size = Vector2(6, 6)
	status_dot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	bottom.add_child(status_dot)
	_status = _label(bottom, "Session only · no history saved")
	_status.add_theme_font_size_override("font_size", 11)
	_status.add_theme_color_override("font_color", Ui.TEXT_3)
	_status.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_status.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var diagnostics_button := Ui.text_icon_button(bottom, "settings", tr("Diagnostics"), _show_diagnostics)
	diagnostics_button.custom_minimum_size = Vector2(0, 24)
	diagnostics_button.add_theme_font_size_override("font_size", 11)
	_full = Control.new()
	_full.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_full.hide()
	add_child(_full)

func _style_split(split: SplitContainer) -> void:
	# Keep a real 4 px separator, but draw only a compact hover/drag grip.
	if _split_h_grabber == null:
		_split_h_grabber = _make_grabber(Vector2i(4, 18), false)
		_split_v_grabber = _make_grabber(Vector2i(18, 4), true)
	split.add_theme_icon_override("h_grabber", _split_h_grabber)
	split.add_theme_icon_override("v_grabber", _split_v_grabber)
	split.add_theme_constant_override("autohide", 1)
	split.dragger_visibility = SplitContainer.DRAGGER_VISIBLE
	split.touch_dragger_enabled = false
	split.add_theme_constant_override("separation", 4)

func _make_grabber(size: Vector2i, horizontal: bool) -> ImageTexture:
	var image := Image.create(size.x, size.y, false, Image.FORMAT_RGBA8)
	image.fill(Color.TRANSPARENT)
	var color := Ui.TEXT_3
	if horizontal:
		for x in range(4, size.x - 4):
			image.set_pixel(x, 1, color)
			image.set_pixel(x, 2, color)
	else:
		for y in range(4, size.y - 4):
			image.set_pixel(1, y, color)
			image.set_pixel(2, y, color)
	return ImageTexture.create_from_image(image)

func _new_pane(id: String) -> Control:
	var pane := Pane.new()
	pane.view_id = id
	pane.action.connect(_pane_action)
	panes[id] = pane
	return pane

func split_view(id: String, vertical: bool) -> String:
	if not panes.has(id): return ""
	if _full_id != "": toggle_fullscreen(_full_id)
	if _windows.has(id): dock_view(id)
	var pane: Control = panes[id]
	var parent: Node = pane.get_parent()
	var index: int = pane.get_index()
	var child_id: String = AppState.create_view()
	var sibling: Control = _new_pane(child_id)
	var split: SplitContainer = VSplitContainer.new() if vertical else HSplitContainer.new()
	_style_split(split)
	split.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.set_meta("recursive_split", true)
	parent.remove_child(pane)
	parent.add_child(split)
	parent.move_child(split, index)
	split.add_child(pane)
	split.add_child(sibling)
	if parent == _workspace:
		_layout_root = split
		split.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	AppState.activate(child_id)
	return child_id

func close_view(id: String) -> bool:
	if AppState.views.size() <= 1: return false
	if _full_id != "": toggle_fullscreen(_full_id)
	if _windows.has(id): dock_view(id)
	var pane: Control = panes[id]
	var split: Node = pane.get_parent()
	if not split is SplitContainer: return false
	var sibling: Control = split.get_child(1 if pane.get_index() == 0 else 0)
	var parent := split.get_parent()
	var index := split.get_index()
	split.remove_child(sibling)
	parent.remove_child(split)
	parent.add_child(sibling)
	parent.move_child(sibling, index)
	if parent == _workspace:
		_layout_root = sibling
		sibling.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	panes.erase(id)
	split.queue_free()
	return AppState.close_view(id)

func toggle_fullscreen(id: String) -> void:
	if _full_id != "":
		var old_id := _full_id
		_full_id = ""
		_restore_slot(old_id)
		_full.hide()
		if old_id == id: return
	if _windows.has(id): dock_view(id)
	_save_slot(id, panes[id])
	panes[id].reparent(_full)
	panes[id].set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_full_id = id
	_full.show()
	AppState.activate(id)

func _save_slot(key: String, control: Control) -> void:
	var parent := control.get_parent()
	var index := control.get_index()
	var placeholder := Control.new()
	placeholder.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	placeholder.size_flags_vertical = Control.SIZE_EXPAND_FILL
	parent.add_child(placeholder)
	parent.move_child(placeholder, index)
	_slots[key] = {"placeholder": placeholder, "control": control}

func _restore_slot(key: String) -> void:
	if not _slots.has(key): return
	var slot: Dictionary = _slots[key]
	var parent: Node = slot.placeholder.get_parent()
	var index: int = slot.placeholder.get_index()
	slot.control.reparent(parent)
	parent.move_child(slot.control, index)
	parent.remove_child(slot.placeholder)
	slot.placeholder.queue_free()
	if parent == _workspace: slot.control.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_slots.erase(key)

func detach_view(id: String) -> void:
	if OS.has_feature("web") or _windows.has(id): return
	if _full_id != "": toggle_fullscreen(_full_id)
	_save_slot(id, panes[id])
	var slot: Control = _slots[id].placeholder
	var dock := Button.new()
	dock.text = id + "\n" + tr("Dock")
	dock.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	dock.pressed.connect(dock_view.bind(id))
	slot.add_child(dock)
	var window := _window(id)
	panes[id].reparent(window)
	panes[id].set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	window.close_requested.connect(dock_view.bind(id))
	window.show()

func dock_view(id: String) -> void:
	if not _windows.has(id): return
	_restore_slot(id)
	var window: Window = _windows[id]
	_windows.erase(id)
	window.queue_free()

func _window(key: String) -> Window:
	var window := Window.new()
	window.hide()
	window.title = Profile.value("product_name") + " · " + key
	window.initial_position = Window.WINDOW_INITIAL_POSITION_CENTER_MAIN_WINDOW_SCREEN
	window.theme = theme
	add_child(window)
	window.size = Dpi._fit_size(window, Vector2i(Vector2(900, 600) * window.content_scale_factor))
	_windows[key] = window
	return window

func _float_panel(key: String) -> void:
	if OS.has_feature("web"): return
	if _windows.has(key):
		dock_view(key)
		return
	var panel: Control = _script if key == "script" else _properties
	_save_slot(key, panel)
	_slots[key].placeholder.hide()
	var window := _window(key)
	panel.reparent(window)
	panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	window.close_requested.connect(func():
		if key == "script": _script.close_workbench()
		else: _close_panel(key))
	panel.show()
	window.show()

func _close_panel(key: String) -> void:
	dock_view(key)
	var panel: Control = _script if key == "script" else _properties
	panel.hide()
	if key == "properties": _settings_mode = false
	_sync_panel_buttons()

func _show_panel(key: String) -> void:
	if key == "properties": _settings_mode = false
	var panel: Control = _script if key == "script" else _properties
	if _windows.has(key): _windows[key].show()
	panel.show()
	if key == "script": _script.refresh_tree()
	if key == "properties": _refresh_properties()
	_sync_panel_buttons()

func _toggle_settings() -> void:
	if _settings_mode:
		_settings_mode = false
		_refresh_properties()
	else:
		_show_settings_panel()

func _show_settings_panel() -> void:
	_settings_mode = true
	_properties.show()
	if is_instance_valid(_property_title): _property_title.text = tr("Settings")
	_refresh_properties()
	_sync_panel_buttons()

func _sync_panel_buttons() -> void:
	for key in _panel_buttons:
		var button: Button = _panel_buttons[key]
		if is_instance_valid(button):
			var panel: Control = _script if key == "script" else _properties
			Ui.style_button(button, panel.visible, true)

func _pane_action(id: String, command: String) -> void:
	AppState.activate(id)
	match command:
		"split_h": split_view(id, false)
		"split_v": split_view(id, true)
		"add": _open_add(id)
		"fullscreen": toggle_fullscreen(id)
		"detach":
			if _windows.has(id): dock_view(id)
			else: detach_view(id)
		"close":
			if AppState.views[id].layers.is_empty(): close_view(id)
			else: _ask("Close this viewport and release its layers?", func(): close_view(id))
		"properties": _show_panel("properties")
		"camera": AppState.activate(id, "camera")
		"grid": AppState.views[id].set_grid(not AppState.views[id].grid_visible)
		"frame":
			AppState.store.frame_all(id)
		"fly": _fly_dialog(id)
		"viewpoints": _viewpoints(id)
		"display": _display_dialog(id)
		"screenshot": _save_screenshot(id)
		"record": _toggle_recording(id)

func _save_screenshot(id: String) -> void:
	if DisplayServer.get_name() == "headless" or not AppState.views.has(id):
		AppState.status_changed.emit(tr("Rendering is unavailable in headless mode"))
		return
	var image: Image = AppState.views[id].viewport.get_texture().get_image()
	if image == null or image.is_empty():
		AppState.status_changed.emit(tr("No rendered frame"))
		return
	var directory := "user://screenshots"
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(directory))
	var stamp := Time.get_datetime_string_from_system().replace(":", "-").replace("T", "_")
	var path := directory.path_join("%s_%s.png" % [id, stamp])
	var error := image.save_png(ProjectSettings.globalize_path(path))
	if error == OK:
		AppState.status_changed.emit(tr("Screenshot saved") + ": " + ProjectSettings.globalize_path(path))
	else:
		AppState.status_changed.emit(tr("Screenshot failed"))

func _toggle_recording(id: String) -> void:
	if not _recording_id.is_empty():
		_stop_recording()
		return
	if DisplayServer.get_name() == "headless" or not AppState.views.has(id):
		AppState.status_changed.emit(tr("Rendering is unavailable in headless mode"))
		return
	var stamp := Time.get_datetime_string_from_system().replace(":", "-").replace("T", "_")
	_recording_id = id
	_recording_frames = 0
	_recording_dir = "user://recordings/%s_%s" % [id, stamp]
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(_recording_dir))
	_record_timer = Timer.new()
	_record_timer.wait_time = 0.2
	_record_timer.timeout.connect(_capture_recording_frame)
	add_child(_record_timer)
	_record_timer.start()
	AppState.status_changed.emit(tr("Recording") + ": " + ProjectSettings.globalize_path(_recording_dir))

func _capture_recording_frame() -> void:
	if _recording_id.is_empty() or not AppState.views.has(_recording_id):
		_stop_recording()
		return
	var image: Image = AppState.views[_recording_id].viewport.get_texture().get_image()
	if image == null or image.is_empty():
		return
	if image.get_width() > 1280:
		image.resize(1280, maxi(1, image.get_height() * 1280 / image.get_width()))
	_recording_frames += 1
	image.save_png(ProjectSettings.globalize_path(_recording_dir.path_join("frame_%05d.png" % _recording_frames)))

func _stop_recording() -> void:
	if is_instance_valid(_record_timer):
		_record_timer.stop()
		_record_timer.queue_free()
		_record_timer = null
	var frames := _recording_frames
	var directory := _recording_dir
	_recording_id = ""
	_recording_frames = 0
	_recording_dir = ""
	AppState.status_changed.emit(tr("Recording stopped") + ": %d PNG · %s" % [frames, ProjectSettings.globalize_path(directory)])

func _state_changed() -> void:
	if _properties_dirty: return
	_properties_dirty = true
	_refresh_properties.call_deferred()

func _refresh_properties() -> void:
	_properties_dirty = false
	if AppState.shutting_down: return
	for child in _property_fields.get_children():
		_property_fields.remove_child(child)
		child.queue_free()
	var id := AppState.active_id
	if not AppState.views.has(id): return
	if _settings_mode:
		_build_settings_panel()
		return
	if is_instance_valid(_property_title): _property_title.text = id.replace("view_", tr("View "))
	var view: Node = AppState.views[id]
	if view.selected_face.is_empty():
		_brep_inspector.clear()
	if view.selection_kind == "viewport" and not view.show_viewport_properties:
		_property_title.text = tr("Properties")
		var hint := _label(_property_fields, tr("Click the viewport title to show viewport properties."))
		hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		hint.add_theme_color_override("font_color", Ui.TEXT_3)
		return
	var selected_node: Node = view.selected_node.get_ref() if view.selected_node else null
	if view.selection_kind == "node" and (not is_instance_valid(selected_node) or not selected_node.is_inside_tree() or selected_node.is_queued_for_deletion()):
		view.selected_node = null
		view.selection_kind = "viewport" if view.get_layer(view.selected).is_empty() else "layer"
	if is_instance_valid(_property_title):
		match view.selection_kind:
			"camera":
				_property_title.text = tr("Camera")
			"layer":
				var selected_layer: Dictionary = view.get_layer(view.selected)
				_property_title.text = selected_layer.name if not selected_layer.is_empty() else id.replace("view_", tr("View "))
			"node":
				_property_title.text = str(selected_node.name) if is_instance_valid(selected_node) else id.replace("view_", tr("View "))
			_:
				_property_title.text = id.replace("view_", tr("View "))
		_property_title.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	match view.selection_kind:
		"node":
			if not view.selected_face.is_empty(): _brep_face_properties(view.selected_face, selected_node)
			_label(_property_fields, tr("Type") + ": " + selected_node.get_class())
			var layer: Dictionary = view.get_layer(view.selected)
			if not layer.is_empty():
				_label(_property_fields, tr("Layer") + ": " + layer.name)
				_label(_property_fields, tr("Status") + ": " + tr(layer.status))
			var path_label := _label(_property_fields, tr("Node path") + ": " + str(selected_node.get_path()))
			path_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
			if selected_node.has_meta("brep_persistent_id"):
				_label(_property_fields, "Persistent ID: " + str(selected_node.get_meta("brep_persistent_id")))
				_label(_property_fields, tr("Occurrence") + ": " + str(selected_node.get_meta("brep_occurrence", -1)))
			if selected_node is Node3D:
				_label(_property_fields, tr("Visible") + ": " + tr("Yes" if selected_node.is_visible_in_tree() else "No"))
				_label(_property_fields, tr("Position") + ": " + str(selected_node.position))
				_label(_property_fields, tr("Rotation") + ": " + str(selected_node.rotation_degrees))
				_label(_property_fields, tr("Scale") + ": " + str(selected_node.scale))
			if layer.get("kind") == "brep" and selected_node.get_parent() == layer.node and selected_node.has_meta("brep_occurrence"):
				_layer_properties(id, view, layer)
		"camera":
			_label(_property_fields, tr("Type") + ": " + view.camera.get_class())
			_choice(_property_fields, "Navigation", ["Orbit", "Fly (WASD / QE)"], view.camera_mode, func(value: int): view.camera_mode = value)
			_choice(_property_fields, "Projection", ["Perspective", "Orthographic"], view.camera.projection, func(value: int): view.camera.projection = value)
			_number(_property_fields, "FOV", view.camera.fov, 10, 120, 1, func(value: float): view.camera.fov = value)
			_button(_property_fields, "Viewpoints", _viewpoints.bind(id))
			if Profile.enabled("gis"): _button(_property_fields, "Fly to", _fly_dialog.bind(id))
		"layer":
			var layer: Dictionary = view.get_layer(view.selected)
			if not layer.is_empty(): _layer_properties(id, view, layer)
		_:
			var background := _viewport_property_section("Background", view)
			var background_tiles := HBoxContainer.new()
			background_tiles.add_theme_constant_override("separation", 4)
			background.add_child(background_tiles)
			for index in 4:
				var tile_index: int = index
				var tile := Button.new()
				tile.text = tr(["Color", "Sky", "Panorama", "Image"][index])
				tile.toggle_mode = true
				tile.button_pressed = view.background_mode == index
				tile.size_flags_horizontal = Control.SIZE_EXPAND_FILL
				tile.custom_minimum_size.y = 28
				tile.add_theme_font_size_override("font_size", 10)
				Ui.style_button(tile, view.background_mode == index, true)
				tile.pressed.connect(func():
					view.set_background_mode(tile_index)
					AppState.changed.emit())
				background_tiles.add_child(tile)
			match view.background_mode:
				0:
					_compact_color(background, tr("Color"), view.environment.background_color, func(value: Color): view.environment.background_color = value)
				2:
					var choose_panorama := _button(background, tr("Choose panorama..."), _choose_background_image.bind(view, true))
					choose_panorama.custom_minimum_size.y = 26
					var panorama_note := _label(background, tr("Panorama requires a 2:1 equirectangular image."))
					panorama_note.add_theme_font_size_override("font_size", 10)
					panorama_note.add_theme_color_override("font_color", Ui.TEXT_3)
					panorama_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
					if not view.panorama_path.is_empty():
						var panorama_name := _label(background, view.panorama_path.get_file())
						panorama_name.add_theme_font_size_override("font_size", 10)
						panorama_name.add_theme_color_override("font_color", Ui.TEXT_3)
				3:
					var choose_image := _button(background, tr("Choose image..."), _choose_background_image.bind(view, false))
					choose_image.custom_minimum_size.y = 26
					if not view.flat_path.is_empty():
						var image_name := _label(background, view.flat_path.get_file())
						image_name.add_theme_font_size_override("font_size", 10)
						image_name.add_theme_color_override("font_color", Ui.TEXT_3)
						image_name.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
			if view.background_mode == 1:
				var sky_fields := VBoxContainer.new()
				sky_fields.add_theme_constant_override("separation", 6)
				background.add_child(sky_fields)
				var sky_light_label := _label(sky_fields, tr("Sky light"))
				sky_light_label.add_theme_font_size_override("font_size", 12)
				sky_light_label.add_theme_color_override("font_color", Ui.TEXT_2)
				var sky_material: ProceduralSkyMaterial = view.sky_material as ProceduralSkyMaterial
				if sky_material:
					_compact_color(sky_fields, tr("Sky color"), sky_material.sky_top_color, func(value: Color):
						sky_material.sky_top_color = value
						sky_material.sky_horizon_color = value.lightened(0.18)
						sky_material.ground_horizon_color = value.lightened(0.18))
					_compact_color(sky_fields, tr("Ground"), sky_material.ground_bottom_color, func(value: Color): sky_material.ground_bottom_color = value)
					_compact_number(sky_fields, tr("Sky energy"), sky_material.energy_multiplier, 0.0, 8.0, 0.05, func(value: float): sky_material.energy_multiplier = value)

			var rendering := _viewport_property_section("Rendering", view)
			_compact_choice(rendering, tr("Anti-aliasing"), ["Off", "2x", "4x", "8x"], view.viewport.msaa_3d, func(value: int):
				view.viewport.msaa_3d = value
				AppState.changed.emit())
			_compact_number(rendering, tr("Resolution scale"), view.viewport.scaling_3d_scale, 0.5, 2.0, 0.05, func(value: float): view.viewport.scaling_3d_scale = value)
			_compact_number(rendering, tr("Exposure"), view.environment.tonemap_exposure, 0.1, 4.0, 0.05, func(value: float): view.environment.tonemap_exposure = value)
			_compact_number(rendering, tr("Gamma"), view.environment.adjustment_brightness, 0.5, 2.0, 0.05, func(value: float):
				view.environment.adjustment_enabled = true
				view.environment.adjustment_brightness = value)
			_compact_check(rendering, tr("Fog"), view.environment.fog_enabled, func(value: bool): view.environment.fog_enabled = value)
			_compact_number(rendering, tr("Fog density"), view.environment.fog_density, 0.0, 0.01, 0.00005, func(value: float): view.environment.fog_density = value)
			_compact_number(rendering, tr("Aerial perspective"), view.environment.fog_aerial_perspective, 0.0, 1.0, 0.05, func(value: float): view.environment.fog_aerial_perspective = value)
			_compact_check(rendering, tr("SSAO"), view.environment.ssao_enabled, func(value: bool): view.environment.ssao_enabled = value)
			_compact_check(rendering, tr("Glow"), view.environment.glow_enabled, func(value: bool): view.environment.glow_enabled = value)
			_compact_check(rendering, tr("Tonemap"), view.environment.tonemap_mode != Environment.TONE_MAPPER_LINEAR, func(value: bool):
				view.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC if value else Environment.TONE_MAPPER_LINEAR)
			_compact_check(rendering, tr("Global illumination"), view.environment.sdfgi_enabled, func(value: bool): view.environment.sdfgi_enabled = value)

			_light_properties(view)

func _light_properties(view: Node) -> void:
	var lights := _viewport_property_section("Lights", view)
	lights.name = "Lights"
	var count := 0
	for entry in view.lights:
		var light_node: Light3D = entry.node
		if not is_instance_valid(light_node): continue
		count += 1
		var card := PanelContainer.new()
		card.name = light_node.name
		card.add_theme_stylebox_override("panel", Ui.box(Ui.RAISED, Ui.LINE, 5, 1, Vector4(7, 6, 7, 6)))
		lights.add_child(card)
		var fields := VBoxContainer.new()
		fields.add_theme_constant_override("separation", 5)
		card.add_child(fields)
		var head := HBoxContainer.new()
		head.add_theme_constant_override("separation", 4)
		fields.add_child(head)
		var type_name := "Directional" if light_node is DirectionalLight3D else ("Point" if light_node is OmniLight3D else "Spot")
		var title := _label(head, tr(type_name) + " · " + str(light_node.name))
		title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		title.add_theme_font_size_override("font_size", 11)
		Ui.icon_button(head, "close", tr("Remove"), func():
			for index in view.lights.size():
				if view.lights[index].node == light_node:
					view.remove_light(index)
					AppState.changed.emit()
					break, false, 22, 10)
		_compact_check(fields, tr("Visible"), light_node.visible, func(value: bool):
			light_node.visible = value
			view.refresh_lighting())
		_compact_number(fields, tr("Energy"), light_node.light_energy, 0.0, maxf(20.0, light_node.light_energy), 0.1, func(value: float): light_node.light_energy = value)
		_compact_color(fields, tr("Color"), light_node.light_color, func(value: Color): light_node.light_color = value)
		_compact_check(fields, tr("Shadow"), light_node.shadow_enabled, func(value: bool): light_node.shadow_enabled = value)
		if light_node is DirectionalLight3D:
			var follow_camera: bool = entry.get("follow_camera", false)
			_compact_check(fields, tr("Follow camera"), follow_camera, func(value: bool):
				view.set_light_follow_camera(light_node, value)
				AppState.changed.emit())
			if not follow_camera:
				_light_direction_fields(fields, light_node)
			_compact_number(fields, tr("Shadow distance"), light_node.directional_shadow_max_distance, 0.0, maxf(5000.0, light_node.directional_shadow_max_distance), 1.0, func(value: float): light_node.directional_shadow_max_distance = value)
		else:
			_light_position_fields(fields, light_node)
			if light_node is OmniLight3D:
				_compact_number(fields, tr("Range"), light_node.omni_range, 0.001, maxf(100.0, light_node.omni_range * 2.0), 0.001, func(value: float): light_node.omni_range = value)
				_compact_number(fields, tr("Attenuation"), light_node.omni_attenuation, 0.0, maxf(10.0, light_node.omni_attenuation), 0.1, func(value: float): light_node.omni_attenuation = value)
			elif light_node is SpotLight3D:
				_light_direction_fields(fields, light_node)
				_compact_number(fields, tr("Range"), light_node.spot_range, 0.001, maxf(100.0, light_node.spot_range * 2.0), 0.001, func(value: float): light_node.spot_range = value)
				_compact_number(fields, tr("Attenuation"), light_node.spot_attenuation, 0.0, maxf(10.0, light_node.spot_attenuation), 0.1, func(value: float): light_node.spot_attenuation = value)
				_compact_number(fields, tr("Spot angle"), light_node.spot_angle, 0.1, 89.9, 0.1, func(value: float): light_node.spot_angle = value, "°")
				_compact_number(fields, tr("Spot attenuation"), light_node.spot_angle_attenuation, 0.0, maxf(10.0, light_node.spot_angle_attenuation), 0.1, func(value: float): light_node.spot_angle_attenuation = value)
	if count == 0:
		var empty := _label(lights, tr("No lights"))
		empty.add_theme_font_size_override("font_size", 11)
		empty.add_theme_color_override("font_color", Ui.TEXT_3)
	var add_row := HBoxContainer.new()
	add_row.add_theme_constant_override("separation", 6)
	lights.add_child(add_row)
	for type in 3:
		var add := _button(add_row, tr(["Add directional light", "Add point light", "Add spot light"][type]), func():
			view.add_light(type)
			AppState.changed.emit())
		add.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		add.custom_minimum_size.y = 26

func _light_direction_fields(parent: Node, light_node: Light3D) -> void:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	parent.add_child(row)
	var elevation := _make_slider_field(tr("Elevation"), -light_node.rotation_degrees.x, -90.0, 90.0, 0.1, func(value: float): light_node.rotation_degrees.x = -value, "°")
	elevation.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(elevation)
	var azimuth := _make_slider_field(tr("Azimuth"), fposmod(light_node.rotation_degrees.y, 360.0), 0.0, 360.0, 0.1, func(value: float): light_node.rotation_degrees.y = value, "°")
	azimuth.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(azimuth)

func _light_position_fields(parent: Node, light_node: Light3D) -> void:
	_compact_label(parent, tr("Position"))
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 6)
	parent.add_child(row)
	for axis in 3:
		var spin := SpinBox.new()
		spin.prefix = ["X", "Y", "Z"][axis]
		spin.min_value = -10000000.0
		spin.max_value = 10000000.0
		spin.allow_greater = true
		spin.allow_lesser = true
		spin.step = 0.001
		spin.value = light_node.position[axis]
		spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		spin.get_line_edit().custom_minimum_size.x = 40
		spin.value_changed.connect(func(value: float): light_node.position[axis] = value)
		row.add_child(spin)

func _transform_fields(parent: Node, layer: Dictionary) -> void:
	var node: Node3D = layer.node
	for field in ["position", "rotation_degrees", "scale"]:
		var caption := _label(parent, {"position": "Position", "rotation_degrees": "Rotation", "scale": "Scale"}[field])
		caption.add_theme_font_size_override("font_size", 12)
		caption.add_theme_color_override("font_color", Ui.TEXT_2)
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", 6)
		parent.add_child(row)
		for axis in 3:
			var spin := SpinBox.new()
			spin.min_value = -10000000
			spin.max_value = 10000000
			spin.step = 0.1
			spin.value = node.get(field)[axis]
			spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			spin.get_line_edit().custom_minimum_size.x = 40
			spin.value_changed.connect(func(value: float):
				if not is_instance_valid(node): return
				var vector: Vector3 = node.get(field)
				vector[axis] = value
				node.set(field, vector)
				layer["transform"] = node.transform)
			row.add_child(spin)

func _choose_background_image(view: Node, panorama := false) -> void:
	var picker := FileDialog.new()
	picker.use_native_dialog = true
	picker.access = FileDialog.ACCESS_FILESYSTEM
	picker.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	picker.filters = PackedStringArray(["*.png,*.jpg,*.jpeg,*.webp,*.bmp;Images"])
	picker.file_selected.connect(func(path: String):
		if panorama:
			if not view.set_panorama_image(path):
				AppState.status_changed.emit(tr("Panorama requires a 2:1 equirectangular image."))
				picker.queue_free()
				return
		else:
			view.set_flat_image(path)
		AppState.changed.emit()
		picker.queue_free())
	picker.canceled.connect(picker.queue_free)
	add_child(picker)
	Dpi.popup(picker, Vector2i.ZERO, get_window(), 0.7)

func _build_dialogs() -> void:
	_confirm = ConfirmationDialog.new()
	add_child(_confirm)
	_confirm.confirmed.connect(func():
		var callback := _pending
		_pending = Callable()
		if callback.is_valid(): callback.call())
	_build_add_dialog()
	_file = FileDialog.new()
	_file.use_native_dialog = true
	_file.access = FileDialog.ACCESS_FILESYSTEM
	_file.file_mode = FileDialog.FILE_MODE_OPEN_FILES
	_file.filters = Profile.file_filters()
	_file.file_selected.connect(func(path: String): _add_pending_paths(PackedStringArray([path])))
	_file.files_selected.connect(_add_pending_paths)
	add_child(_file)
	_update_add_kind(0)
	_diagnostics = AcceptDialog.new()
	_diagnostics.title = "Diagnostics"
	add_child(_diagnostics)
	var diagnostics_box := VBoxContainer.new()
	_diagnostics.add_child(diagnostics_box)
	_report = TextEdit.new()
	_report.editable = false
	_report.custom_minimum_size = Vector2(650, 350)
	diagnostics_box.add_child(_report)
	var actions := HBoxContainer.new()
	diagnostics_box.add_child(actions)
	_button(actions, "Copy report", func(): DisplayServer.clipboard_set(_report.text))
	_button(actions, "Export report", _export_report)
	_button(actions, "Clear", func():
		AppState.errors.clear()
		_show_diagnostics())

func _build_add_dialog() -> void:
	_add = Control.new()
	_add.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_add.mouse_filter = Control.MOUSE_FILTER_STOP
	_add.hide()
	add_child(_add)
	var scrim := ColorRect.new()
	scrim.color = Color(0.01, 0.02, 0.03, 0.62)
	scrim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	scrim.mouse_filter = Control.MOUSE_FILTER_STOP
	scrim.gui_input.connect(func(event: InputEvent):
		if event is InputEventMouseButton and event.pressed: _close_add())
	_add.add_child(scrim)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_add.add_child(center)
	var card := PanelContainer.new()
	card.custom_minimum_size = Vector2(700, 0)
	card.add_theme_stylebox_override("panel", Ui.box(Ui.RAISED, Ui.LINE_STRONG, 10, 1, Vector4(20, 18, 20, 16)))
	center.add_child(card)
	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", 10)
	card.add_child(content)
	var header := HBoxContainer.new()
	content.add_child(header)
	var heading := VBoxContainer.new()
	heading.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	heading.add_theme_constant_override("separation", 2)
	header.add_child(heading)
	var title := Label.new()
	title.text = tr("Add data")
	title.add_theme_font_size_override("font_size", 16)
	title.add_theme_color_override("font_color", Ui.TEXT)
	heading.add_child(title)
	var subtitle := Label.new()
	subtitle.text = tr("Create independent data; drag the layer to another viewport to share it.")
	subtitle.add_theme_font_size_override("font_size", 11)
	subtitle.add_theme_color_override("font_color", Ui.TEXT_3)
	heading.add_child(subtitle)
	Ui.icon_button(header, "close", tr("Close"), _close_add, false, 28, 14)
	var target_row := HBoxContainer.new()
	target_row.add_theme_constant_override("separation", 6)
	content.add_child(target_row)
	var target_prefix := Label.new()
	target_prefix.text = tr("Add to")
	target_prefix.add_theme_font_size_override("font_size", 11)
	target_prefix.add_theme_color_override("font_color", Ui.TEXT_3)
	target_row.add_child(target_prefix)
	_add_target = Label.new()
	_add_target.add_theme_font_size_override("font_size", 11)
	_add_target.add_theme_color_override("font_color", Ui.ACCENT)
	target_row.add_child(_add_target)
	var grow := Control.new()
	grow.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	target_row.add_child(grow)
	var independent := Label.new()
	independent.text = tr("Independent data")
	independent.add_theme_font_size_override("font_size", 10)
	independent.add_theme_color_override("font_color", Ui.TEXT_3)
	target_row.add_child(independent)
	var grid := GridContainer.new()
	grid.columns = 3
	grid.add_theme_constant_override("h_separation", 6)
	grid.add_theme_constant_override("v_separation", 6)
	content.add_child(grid)
	var sources := [
		["Local file", "layers", "glTF / 3D Tiles / OSGB / ZIP"],
		["Remote URL", "globe", "HTTP(S) / glTF / 3D Tiles"],
		["WGS84 ellipsoid", "globe", "Basic globe"],
		["Cesium ion 3D Tiles", "layers", "Asset ID and session token"],
		["GDScript script", "script", ".gd runtime behavior"],
	]
	if not Profile.enabled("3dtiles") and not Profile.enabled("osg"):
		sources[0][2] = "glTF / GLB / ZIP"
		sources[1][2] = "HTTP(S) / glTF / GLB"
	if Profile.enabled("brep"): sources[0][2] += " / BREP / BREP JSON / assembly JSON"
	_kind = OptionButton.new()
	_kind.hide()
	content.add_child(_kind)
	for item in sources: _kind.add_item(item[0])
	for index in sources.size():
		var source: Array = sources[index]
		var button := Button.new()
		button.text = tr(source[0])
		button.icon = Ui.icon(source[1])
		Ui.set_icon_size(button, 18)
		button.expand_icon = false
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.add_theme_constant_override("h_separation", 8)
		button.custom_minimum_size = Vector2(210, 50)
		button.tooltip_text = tr(source[2])
		Ui.style_button(button, index == 0)
		button.pressed.connect(_select_add_kind.bind(index))
		grid.add_child(button)
		_add_source_buttons.append(button)
		var available := _add_kind_available(index)
		button.visible = available
		_kind.set_item_disabled(index, not available)
	_add_source_row = HBoxContainer.new()
	_add_source_row.add_theme_constant_override("separation", 6)
	content.add_child(_add_source_row)
	var source_label := Label.new()
	source_label.text = tr("Source")
	source_label.custom_minimum_size.x = 48
	source_label.add_theme_font_size_override("font_size", 11)
	source_label.add_theme_color_override("font_color", Ui.TEXT_3)
	_add_source_row.add_child(source_label)
	_source = LineEdit.new()
	_source.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_source.placeholder_text = tr("Choose a file, folder, ZIP, or URL")
	_add_source_row.add_child(_source)
	_add_browse = _button(_add_source_row, tr("Browse"), func(): Dpi.popup(_file, Vector2i.ZERO, get_window(), 0.7))
	_add_folder = _button(_add_source_row, tr("Folder"), _browse_folder)
	_pending_box = VBoxContainer.new()
	_pending_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_pending_box.add_theme_constant_override("separation", 3)
	var pending_row := HBoxContainer.new()
	_pending_row = pending_row
	pending_row.add_theme_constant_override("separation", 6)
	content.add_child(pending_row)
	_pending_summary = Label.new()
	_pending_summary.add_theme_font_size_override("font_size", 10)
	_pending_summary.add_theme_color_override("font_color", Ui.TEXT_3)
	_pending_summary.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	pending_row.add_child(_pending_summary)
	var clear_pending := Button.new()
	clear_pending.text = tr("Clear")
	clear_pending.custom_minimum_size = Vector2(52, 22)
	Ui.style_button(clear_pending, false, true)
	clear_pending.pressed.connect(func():
		_pending_sources.clear()
		_refresh_pending_box())
	pending_row.add_child(clear_pending)
	_pending_scroll = ScrollContainer.new()
	_pending_scroll.custom_minimum_size = Vector2(0, 136)
	_pending_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_pending_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_pending_scroll.add_child(_pending_box)
	content.add_child(_pending_scroll)
	_add_script_hint = _label(content, "Choose a .gd file, or leave Source empty to create a new script.")
	_add_script_hint.add_theme_font_size_override("font_size", 11)
	_add_script_hint.add_theme_color_override("font_color", Ui.TEXT_3)
	_add_script_hint.hide()
	_add_zip_label = _label(content, "ZIP entry (if multiple models)")
	_add_zip_label.add_theme_font_size_override("font_size", 11)
	_add_zip_label.add_theme_color_override("font_color", Ui.TEXT_3)
	_zip_entry = LineEdit.new()
	content.add_child(_zip_entry)
	_axis = OptionButton.new()
	_axis.add_item("Y-UP")
	_axis.add_item("Z-UP")
	_axis.tooltip_text = tr("Up axis")
	content.add_child(_axis)
	_ion_fields = VBoxContainer.new()
	content.add_child(_ion_fields)
	_ion_id = _number(_ion_fields, "Asset ID", 0, 0, 9007199254740991.0, 1, Callable())
	_label(_ion_fields, "Access token (session only)")
	_ion_token = LineEdit.new()
	_ion_token.secret = true
	_ion_fields.add_child(_ion_token)
	_ion_fields.hide()
	_http_fields = VBoxContainer.new()
	content.add_child(_http_fields)
	_label(_http_fields, "HTTP headers for glTF / GLB (session only)")
	_http_headers = TextEdit.new()
	_http_headers.custom_minimum_size = Vector2(620, 76)
	_http_headers.placeholder_text = "Authorization: Bearer …"
	_http_fields.add_child(_http_headers)
	var header_note := _label(_http_fields, "Headers are sent only to the model URL origin.")
	header_note.add_theme_font_size_override("font_size", 10)
	header_note.add_theme_color_override("font_color", Ui.TEXT_3)
	_http_fields.hide()
	var notice := Label.new()
	notice.text = tr("Added data appears only in the target viewport. Drag the layer to another viewport to create a shared reference.")
	notice.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	notice.add_theme_font_size_override("font_size", 10)
	notice.add_theme_color_override("font_color", Ui.TEXT_3)
	content.add_child(notice)
	var footer := HBoxContainer.new()
	footer.add_theme_constant_override("separation", 6)
	content.add_child(footer)
	var footer_grow := Control.new()
	footer_grow.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	footer.add_child(footer_grow)
	_button(footer, tr("Cancel"), _close_add)
	var add_button := Button.new()
	add_button.text = tr("Add")
	add_button.icon = Ui.icon("plus")
	Ui.set_icon_size(add_button, 14)
	add_button.custom_minimum_size = Vector2(90, 32)
	add_button.add_theme_color_override("font_color", Ui.ON_ACCENT)
	add_button.add_theme_color_override("icon_normal_color", Ui.ON_ACCENT)
	add_button.add_theme_stylebox_override("normal", Ui.box(Ui.ACCENT, Color.TRANSPARENT, 7, 0, Vector4(12, 5, 12, 5)))
	add_button.add_theme_stylebox_override("hover", Ui.box(Ui.ACCENT_HOVER, Color.TRANSPARENT, 7, 0, Vector4(12, 5, 12, 5)))
	add_button.pressed.connect(_confirm_add)
	footer.add_child(add_button)
	_kind.item_selected.connect(_update_add_kind)

func _add_kind_available(index: int) -> bool:
	return (index != 2 or Profile.enabled("gis")) and (index != 3 or Profile.enabled("3dtiles"))

func _select_add_kind(index: int) -> void:
	if not _add_kind_available(index): return
	if index not in [0, 4]:
		_pending_sources.clear()
		_refresh_pending_box()
	_kind.select(index)
	_update_add_kind(index)

func _update_add_kind(index: int) -> void:
	_source.editable = index < 2 or index == 4
	_add_script_hint.visible = index == 4
	_add_folder.visible = index == 0 and Profile.enabled("osg")
	_add_browse.visible = index in [0, 4]
	_add_source_row.visible = index not in [2, 3]
	_axis.visible = index < 2 and (Profile.enabled("brep") or Profile.enabled("osg"))
	_add_zip_label.visible = index == 0
	_zip_entry.visible = index == 0
	_file.filters = PackedStringArray(["*.gd;GDScript"]) if index == 4 else Profile.file_filters()
	_ion_fields.visible = index == 3
	_http_fields.visible = index == 1
	for card_index in _add_source_buttons.size():
		Ui.style_button(_add_source_buttons[card_index], card_index == index)
	_refresh_pending_box()

func _close_add() -> void:
	_add.hide()
	_pending_sources.clear()
	_refresh_pending_box()
	_ion_token.clear()
	_http_headers.clear()

func _add_pending_paths(paths: PackedStringArray) -> void:
	for path in paths:
		if path.is_empty() or _pending_sources.has(path): continue
		_pending_sources.append(path)
	_refresh_pending_box()

func _refresh_pending_box() -> void:
	for child in _pending_box.get_children():
		_pending_box.remove_child(child)
		child.queue_free()
	var has_sources := not _pending_sources.is_empty()
	_pending_row.visible = has_sources
	_pending_scroll.visible = has_sources
	_pending_summary.text = tr("Selected sources") + ": %d" % _pending_sources.size()
	for index in _pending_sources.size():
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", 6)
		var path_label := Label.new()
		path_label.text = _pending_sources[index]
		path_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		path_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		path_label.add_theme_font_size_override("font_size", 10)
		path_label.add_theme_color_override("font_color", Ui.TEXT_3)
		row.add_child(path_label)
		var remove := Button.new()
		remove.text = "×"
		remove.tooltip_text = tr("Remove")
		remove.custom_minimum_size = Vector2(24, 22)
		Ui.style_button(remove, false, true)
		remove.pressed.connect(_remove_pending.bind(index))
		row.add_child(remove)
		_pending_box.add_child(row)

func _remove_pending(index: int) -> void:
	if index >= 0 and index < _pending_sources.size():
		_pending_sources.remove_at(index)
		_refresh_pending_box()

func _add_directory_matching(id: String, directory: String, options: Dictionary) -> bool:
	if not Profile.enabled("osg"): return false
	var base := directory.get_file()
	for extension in [".osgb", ".osg"]:
		var candidate := directory.path_join(base + extension)
		if FileAccess.file_exists(candidate):
			_add_osgb_source(id, candidate, options)
			return true
	return false

func _add_osgb_source(id: String, path: String, options: Dictionary) -> String:
	return AppState.add_source(id, path, "auto", options)

func _import_options(entry := "") -> Dictionary:
	return {"entry": entry, "z_up": _axis.selected == 1}

func _add_multi_sources() -> void:
	var id := _target_id
	if not AppState.views.has(id): return
	var options := _import_options()
	var added := 0
	for path in _pending_sources:
		if _kind.selected == 4:
			if path.get_extension().to_lower() == "gd":
				AppState.add_source(id, path, "script")
				added += 1
		elif DirAccess.dir_exists_absolute(path):
			if _add_directory_matching(id, path, options): added += 1
		elif path.get_extension().to_lower() in ["osgb", "osg"]:
			_add_osgb_source(id, path, options)
			added += 1
		else:
			AppState.add_source(id, path, "auto", options)
			added += 1
	if added == 0:
		_status.text = tr("No matching .osg or .osgb data found in the selected directories.")
		return
	_pending_sources.clear()
	_refresh_pending_box()
	_close_add()
	get_tree().create_timer(0.5).timeout.connect(func():
		if AppState.views.has(id): AppState.store.frame_all(id))

func _browse_folder() -> void:
	var overlay := Control.new()
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(overlay)
	var scrim := ColorRect.new()
	scrim.color = Color(0.01, 0.02, 0.03, 0.62)
	scrim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	scrim.mouse_filter = Control.MOUSE_FILTER_STOP
	scrim.gui_input.connect(func(event: InputEvent):
		if event is InputEventMouseButton and event.pressed: overlay.queue_free())
	overlay.add_child(scrim)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	overlay.add_child(center)
	var card := PanelContainer.new()
	card.custom_minimum_size = Vector2(660, 500)
	card.add_theme_stylebox_override("panel", Ui.box(Ui.RAISED, Ui.LINE_STRONG, 10, 1, Vector4(18, 16, 18, 14)))
	center.add_child(card)
	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", 8)
	card.add_child(content)
	var header := HBoxContainer.new()
	content.add_child(header)
	var title := Label.new()
	title.text = tr("Select folders")
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title.add_theme_font_size_override("font_size", 15)
	title.add_theme_color_override("font_color", Ui.TEXT)
	header.add_child(title)
	Ui.icon_button(header, "close", tr("Close"), func(): overlay.queue_free(), false, 26, 13)
	var path_row := HBoxContainer.new()
	path_row.add_theme_constant_override("separation", 6)
	content.add_child(path_row)
	var path_label := Label.new()
	path_label.text = tr("Path")
	path_label.custom_minimum_size.x = 42
	path_label.add_theme_font_size_override("font_size", 11)
	path_label.add_theme_color_override("font_color", Ui.TEXT_3)
	path_row.add_child(path_label)
	var path := LineEdit.new()
	var initial_path := _last_add_directory
	if not DirAccess.dir_exists_absolute(initial_path) and DirAccess.dir_exists_absolute(_source.text):
		initial_path = _source.text
	if not DirAccess.dir_exists_absolute(initial_path):
		initial_path = OS.get_system_dir(OS.SYSTEM_DIR_DOCUMENTS)
	if initial_path.is_empty():
		initial_path = OS.get_system_dir(OS.SYSTEM_DIR_DESKTOP)
	if initial_path.is_empty(): initial_path = OS.get_environment("USERPROFILE")
	if initial_path.is_empty(): initial_path = ProjectSettings.globalize_path("user://")
	path.text = initial_path
	path.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	path_row.add_child(path)
	var list := ItemList.new()
	list.select_mode = ItemList.SELECT_MULTI
	list.custom_minimum_size = Vector2(620, 330)
	list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	content.add_child(list)
	var status := Label.new()
	status.add_theme_font_size_override("font_size", 10)
	status.add_theme_color_override("font_color", Ui.TEXT_3)
	content.add_child(status)
	var refresh := func():
		list.clear()
		var directory := DirAccess.open(path.text)
		if directory == null:
			status.text = tr("Directory cannot be opened.")
			return
		_last_add_directory = path.text
		status.text = tr("Ctrl / Shift select multiple folders")
		for child in directory.get_directories():
			list.add_item(child)
			list.set_item_metadata(list.item_count - 1, path.text.path_join(child))
	refresh.call()
	var choose_folder := Button.new()
	choose_folder.text = tr("Browse")
	choose_folder.custom_minimum_size = Vector2(74, 28)
	choose_folder.pressed.connect(func(): _choose_add_directory(path, refresh))
	path_row.add_child(choose_folder)
	path.text_submitted.connect(func(_value: String): refresh.call())
	list.item_activated.connect(func(index: int):
		var target: String = list.get_item_metadata(index)
		if DirAccess.dir_exists_absolute(target):
			path.text = target
			refresh.call())
	var footer := HBoxContainer.new()
	footer.add_theme_constant_override("separation", 6)
	content.add_child(footer)
	_button(footer, tr("Up"), func():
		path.text = path.text.get_base_dir()
		refresh.call())
	_button(footer, tr("Select all"), func():
		for index in list.item_count: list.select(index, false))
	_button(footer, tr("Invert selection"), func():
		for index in list.item_count:
			if list.is_selected(index): list.deselect(index)
			else: list.select(index, false))
	var grow := Control.new()
	grow.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	footer.add_child(grow)
	_button(footer, tr("Cancel"), func(): overlay.queue_free())
	var add := Button.new()
	add.text = tr("Add")
	add.icon = Ui.icon("plus")
	Ui.set_icon_size(add, 14)
	add.custom_minimum_size = Vector2(90, 32)
	add.add_theme_color_override("font_color", Ui.ON_ACCENT)
	add.add_theme_color_override("icon_normal_color", Ui.ON_ACCENT)
	add.add_theme_stylebox_override("normal", Ui.box(Ui.ACCENT, Color.TRANSPARENT, 7, 0, Vector4(12, 5, 12, 5)))
	add.add_theme_stylebox_override("hover", Ui.box(Ui.ACCENT_HOVER, Color.TRANSPARENT, 7, 0, Vector4(12, 5, 12, 5)))
	add.pressed.connect(func():
		var selected: Array[String] = []
		for index in list.get_selected_items(): selected.append(list.get_item_metadata(index))
		if selected.is_empty():
			status.text = tr("Select at least one folder.")
			return
		_add_pending_paths(PackedStringArray(selected))
		_last_add_directory = path.text
		overlay.queue_free())
	footer.add_child(add)
	overlay.show()
	path.grab_focus()

func _choose_add_directory(path: LineEdit, refresh: Callable) -> void:
	var picker := FileDialog.new()
	picker.use_native_dialog = true
	picker.access = FileDialog.ACCESS_FILESYSTEM
	picker.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	picker.dir_selected.connect(func(directory: String):
		path.text = directory
		_last_add_directory = directory
		refresh.call()
		picker.queue_free())
	picker.canceled.connect(picker.queue_free)
	add_child(picker)
	Dpi.popup(picker, Vector2i.ZERO, get_window(), 0.7)

func _confirm_add() -> void:
	if not AppState.views.has(_target_id): return
	if not _add_kind_available(_kind.selected): return
	var id := _target_id
	var source := _source.text.strip_edges()
	var options := _import_options(_zip_entry.text.strip_edges())
	if not _pending_sources.is_empty() and _kind.selected in [0, 4]:
		_add_multi_sources()
		return
	if _kind.selected == 4:
		if source.is_empty():
			_open_script(AppState.scripts.create(AppState.scripts.BLANK, "untitled.gd", false, id))
			_close_add()
		elif source.get_extension().to_lower() == "gd":
			AppState.add_source(id, source, "script")
			_close_add()
		else:
			_status.text = tr("Choose a .gd file, or leave Source empty to create a new script.")
		_source.clear()
		return
	if _kind.selected == 1:
		var headers := {}
		var valid := true
		for line in _http_headers.text.split("\n", false):
			var colon: int = line.find(":")
			var key: String = line.substr(0, colon).strip_edges().to_lower()
			if colon <= 0 or headers.has(key):
				valid = false
				break
			headers[key] = line.substr(colon + 1).strip_edges()
		var header_error: String = preload("res://scripts/desktop/http_source.gd").headers_error(headers)
		if not valid or not header_error.is_empty():
			_status.text = tr("Invalid HTTP headers. Use one unique Name: value per line.")
			_http_headers.clear()
			return
		if not headers.is_empty() and source.get_slice("?", 0).get_extension().to_lower() not in ["gltf", "glb"]:
			_status.text = tr("Custom HTTP headers currently require glTF or GLB.")
			_http_headers.clear()
			return
		options["headers"] = headers
		_http_headers.clear()
	if _kind.selected == 3:
		if _ion_id.value < 1 or _ion_token.text.strip_edges().is_empty():
			_status.text = tr("Asset ID and token are required.")
			_ion_token.clear()
			return
		options["ion_asset_id"] = int(_ion_id.value)
		options["ion_token"] = _ion_token.text.strip_edges()
		AppState.add_source(id, "", "tiled", options)
		_ion_token.clear()
		_close_add()
		return
	if _kind.selected != 2 and source.is_empty():
		_status.text = tr("Choose a data source.")
		return
	if _kind.selected == 2:
		AppState.add_source(id, "", "ellipsoid")
	elif DirAccess.dir_exists_absolute(source):
		if _add_directory_matching(id, source, options):
			_close_add()
		else:
			_status.text = tr("No matching .osg or .osgb data found in the selected directory.")
		return
	elif source.get_extension().to_lower() == "zip" and options.entry.is_empty():
		var inspection: Dictionary = preload("res://scripts/desktop/zip_guard.gd").inspect(source)
		if inspection.has("error"):
			AppState.add_source(id, source, "auto", options)
		else:
			var candidates: Array[String] = []
			var archive := ZIPReader.new()
			if archive.open(source) == OK:
				candidates = Sources.zip_candidates(archive, Profile.features())
				archive.close()
			_select_entry(candidates, func(entry: String):
				options.entry = entry
				if AppState.views.has(id): AppState.add_source(id, source, "auto", options)
				_close_add())
			return
	else:
		if source.get_extension().to_lower() in ["osgb", "osg"]:
			_add_osgb_source(id, source, options)
		else:
			AppState.add_source(id, source, "auto", options)
	_source.clear()
	_zip_entry.clear()
	_close_add()

func _find_entries(base: String, path: String, candidates: Array[String], budget: Array) -> void:
	var directory := DirAccess.open(path)
	if directory == null or budget[0] <= 0: return
	# A tileset is the entry for its entire directory, including child models.
	if Profile.enabled("3dtiles") and Profile.model_entry(path.path_join("tileset.json")):
		candidates.append(path.path_join("tileset.json").trim_prefix(base + "/").trim_prefix(base + "\\"))
		return
	for filename in directory.get_files():
		budget[0] -= 1
		if budget[0] < 0: return
		if Profile.model_entry(path.path_join(filename)):
			candidates.append(path.path_join(filename).trim_prefix(base + "/").trim_prefix(base + "\\"))
	for child in directory.get_directories():
		budget[0] -= 1
		if not directory.is_link(child): _find_entries(base, path.path_join(child), candidates, budget)

func _select_entry(candidates: Array[String], callback: Callable) -> void:
	if candidates.is_empty():
		_status.text = tr("No supported model entry found.")
		return
	if candidates.size() == 1:
		callback.call(candidates[0])
		return
	var dialog := ConfirmationDialog.new()
	dialog.title = tr("Choose model entry")
	var list := ItemList.new()
	list.custom_minimum_size = Vector2(500, 300)
	for entry in candidates: list.add_item(entry)
	list.select(0)
	dialog.add_child(list)
	dialog.confirmed.connect(func():
		if not list.get_selected_items().is_empty(): callback.call(candidates[list.get_selected_items()[0]])
		dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free)
	add_child(dialog)
	Dpi.popup(dialog)

func _open_add(id: String) -> void:
	_target_id = id
	_axis.select(0)
	_pending_sources.clear()
	_refresh_pending_box()
	_add_target.text = id.replace("view_", tr("View "))
	_select_add_kind(0)
	_add.show()
	_source.grab_focus()

func _ask(message: String, callback: Callable) -> void:
	_pending = callback
	_confirm.dialog_text = tr(message)
	Dpi.popup(_confirm, Vector2i(520, 180), panes[AppState.active_id].get_window())

func _fly_dialog(id: String) -> void:
	if not Profile.enabled("gis"): return
	var dialog := ConfirmationDialog.new()
	dialog.title = tr("Fly to") + " · " + id
	var box := VBoxContainer.new()
	dialog.add_child(box)
	var longitude := _number(box, "Longitude", 0, -180, 180, 0.001, Callable())
	var latitude := _number(box, "Latitude", 0, -90, 90, 0.001, Callable())
	var height := _number(box, "Height (m)", 10000000, 1, 100000000, 100, Callable())
	dialog.confirmed.connect(func():
		if AppState.views.has(id): AppState.views[id].fly_to(longitude.value, latitude.value, height.value)
		dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free)
	add_child(dialog)
	Dpi.popup(dialog, Vector2i(400, 280), panes[id].get_window())

func _display_dialog(id: String) -> void:
	var dialog := AcceptDialog.new()
	dialog.title = tr("Display") + " · " + id
	var box := VBoxContainer.new()
	dialog.add_child(box)
	var view: Node = AppState.views[id]
	_choice(box, "Display", ["Textured", "Unlit", "Wireframe"], view.render_mode, func(value: int):
		if is_instance_valid(view): view.set_render_mode(value))
	if is_instance_valid(view.light):
		_check(box, "Shadows", view.light.shadow_enabled, func(value: bool):
			if is_instance_valid(view.light): view.light.shadow_enabled = value)
	dialog.confirmed.connect(dialog.queue_free)
	dialog.canceled.connect(dialog.queue_free)
	add_child(dialog)
	Dpi.popup(dialog, Vector2i(350, 160), panes[id].get_window())

func _viewpoints(id: String) -> void:
	var dialog := AcceptDialog.new()
	dialog.title = tr("Viewpoints") + " · " + id
	var box := VBoxContainer.new()
	dialog.add_child(box)
	var name_field := LineEdit.new()
	name_field.placeholder_text = tr("Name")
	box.add_child(name_field)
	var list := ItemList.new()
	list.custom_minimum_size = Vector2(350, 200)
	box.add_child(list)
	var refresh := func():
		list.clear()
		if not AppState.views.has(id): return
		for key in AppState.views[id].viewpoints: list.add_item(key)
	refresh.call()
	_button(box, "Save viewpoint", func():
		if AppState.views.has(id) and not name_field.text.strip_edges().is_empty():
			AppState.views[id].viewpoints[name_field.text.left(100)] = AppState.views[id].pose()
			refresh.call())
	_button(box, "Go", func():
		if AppState.views.has(id) and not list.get_selected_items().is_empty(): AppState.views[id].restore_pose(AppState.views[id].viewpoints[list.get_item_text(list.get_selected_items()[0])]))
	_button(box, "Delete viewpoint", func():
		if AppState.views.has(id) and not list.get_selected_items().is_empty():
			AppState.views[id].viewpoints.erase(list.get_item_text(list.get_selected_items()[0]))
			refresh.call())
	dialog.confirmed.connect(dialog.queue_free)
	dialog.canceled.connect(dialog.queue_free)
	add_child(dialog)
	Dpi.popup(dialog, Vector2i.ZERO, panes[id].get_window())

func _make_bridge(mode: String) -> Node:
	var bridge = PluginBridge.new() if mode == "plugin" else Bridge.new()
	bridge.token_path = "user://plugin/bearer_token" if mode == "plugin" else "user://mcp/bearer_token"
	return bridge

func _switch_automation(mode: String, endpoint_label: Label) -> void:
	if mode == _automation_mode: return
	if is_instance_valid(AppState.bridge):
		AppState.bridge.stop()
		remove_child(AppState.bridge)
		AppState.bridge.queue_free()
	_automation_mode = mode
	AppState.bridge = _make_bridge(mode)
	add_child(AppState.bridge)
	var error: int = AppState.bridge.start()
	var base := "http://127.0.0.1:%d/" % AppState.bridge.port
	endpoint_label.text = base + ("bridge" if mode == "plugin" else "mcp")
	_status.text = ("Plugin bridge started" if mode == "plugin" else "MCP started") if error == OK else error_string(error)

func _settings_section(title: String, expanded := true) -> VBoxContainer:
	var header := Button.new()
	header.text = tr(title)
	header.icon = Ui.icon("chevron-down" if expanded else "chevron-right")
	Ui.set_icon_size(header, 11)
	header.alignment = HORIZONTAL_ALIGNMENT_LEFT
	header.add_theme_constant_override("h_separation", 5)
	header.custom_minimum_size.y = 30
	Ui.style_button(header, false)
	_property_fields.add_child(header)
	var body := VBoxContainer.new()
	body.add_theme_constant_override("separation", 6)
	body.visible = expanded
	_property_fields.add_child(body)
	header.pressed.connect(func():
		body.visible = not body.visible
		header.icon = Ui.icon("chevron-down" if body.visible else "chevron-right"))
	return body

func _layer_properties(id: String, view: Node, layer: Dictionary) -> void:
	var layer_fields := _property_section("Layer")
	var name_caption := _label(layer_fields, tr("Name"))
	name_caption.add_theme_font_size_override("font_size", 12)
	name_caption.add_theme_color_override("font_color", Ui.TEXT_2)
	var name_field := LineEdit.new()
	name_field.text = layer.name
	name_field.text_submitted.connect(func(value: String):
		layer.name = value.left(128)
		var script_id: String = AppState.store.records[layer.data_id].get("script_id", "")
		if AppState.scripts.documents.has(script_id):
			AppState.scripts.documents[script_id].filename = layer.name
			if _script.selected_id == script_id: _script._filename.text = layer.name
			AppState.scripts._sync(script_id)
		AppState.changed.emit())
	layer_fields.add_child(name_field)
	var meta := _label(layer_fields, "%s  ·  %s  ·  %s %d" % [layer.kind, tr(layer.status), tr("References"), AppState.store.reference_count(layer.data_id)])
	meta.add_theme_font_size_override("font_size", 11)
	meta.add_theme_color_override("font_color", Ui.TEXT_3)
	_compact_check(layer_fields, tr("Visible"), layer.visible, func(value: bool): AppState.set_visible(id, layer.id, value))
	if layer.kind == "tiled":
		_compact_label(layer_fields, tr("Receive lighting"))
		var light_row := HBoxContainer.new()
		light_row.add_theme_constant_override("separation", 4)
		layer_fields.add_child(light_row)
		var lighting_mode: int = int(layer.get("receive_lighting_mode", -1))
		for light_index in 3:
			var mode_index: int = [-1, 1, 0][light_index]
			var light_tile := Button.new()
			light_tile.text = tr(["Source default", "Receive lighting", "Unlit"][light_index])
			light_tile.toggle_mode = true
			light_tile.button_pressed = lighting_mode == mode_index
			light_tile.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			light_tile.custom_minimum_size.y = 28
			light_tile.add_theme_font_size_override("font_size", 10)
			Ui.style_button(light_tile, lighting_mode == mode_index, true)
			light_tile.pressed.connect(func():
				layer["receive_lighting_mode"] = mode_index
				if is_instance_valid(layer.node) and layer.node.has_method("set_receive_lighting_mode"):
					layer.node.call("set_receive_lighting_mode", mode_index)
				else:
					AppState.store.rebuild_instance(id, layer)
				AppState.changed.emit())
			light_row.add_child(light_tile)
	if layer.kind != "tiled":
		_compact_number(layer_fields, tr("Opacity"), layer.opacity, 0.0, 1.0, 0.05, func(value: float): AppState.set_opacity(id, layer.id, value))
	if layer.kind == "ellipsoid":
		_compact_check(layer_fields, tr("Grid"), view.grid_visible, func(value: bool): view.set_grid(value))
	if layer.kind == "brep":
		var data: Dictionary = AppState.store.records[layer.data_id]
		var stats: Dictionary = data.get("brep_stats", {})
		if not stats.is_empty():
			_label(layer_fields, "BREP %s · %d %s · %d %s" % [stats.format_version, stats.faces, tr("Faces"), stats.triangles, tr("Triangles")])
			if stats.get("unit_to_millimeters", 0.0) > 0.0:
				_label(layer_fields, tr("Millimeters per source unit") + ": " + str(stats.unit_to_millimeters))
			else:
				_label(layer_fields, tr("Units: unknown (source coordinates)"))
		for warning in data.get("brep_warnings", []):
			var warning_label := _label(layer_fields, str(warning))
			warning_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var download: Dictionary = AppState.store.records[layer.data_id].get("download", {})
	if not download.is_empty():
		var progress := _label(layer_fields, "%s: %d / %d · %.2f MiB" % [tr("Download"), download.completed, download.dependencies, download.bytes / 1048576.0])
		progress.add_theme_font_size_override("font_size", 11)
		progress.add_theme_color_override("font_color", Ui.TEXT_3)
	if not layer.error.is_empty():
		var error_label := _label(layer_fields, layer.error)
		error_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		error_label.add_theme_color_override("font_color", Color("f07a82"))
	if layer.kind == "script":
		var script_id: String = AppState.store.records[layer.data_id].get("script_id", "")
		if AppState.scripts.documents.has(script_id):
			var script_actions := _property_section("Actions")
			var run_row := HBoxContainer.new()
			run_row.add_theme_constant_override("separation", 6)
			script_actions.add_child(run_row)
			var open_button := _button(run_row, tr("Open in workbench"), func(): _open_script(script_id))
			open_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			var run_button := _button(run_row, tr("▶ Run"), func():
				_open_script(script_id)
				_script._request_run())
			run_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			_button(script_actions, tr("Stop script"), AppState.scripts.documents[script_id].runner.stop)
	else:
		var transform_fields := _property_section("Transform")
		if is_instance_valid(layer.node):
			_transform_fields(transform_fields, layer)
		if layer.kind == "gltf" and is_instance_valid(layer.node):
			for player in layer.node.find_children("*", "AnimationPlayer", true, false):
				var animations: Array = Array(player.get_animation_list())
				if not animations.is_empty():
					_compact_choice(transform_fields, "Animation", ["Stopped"] + animations, 0, func(value: int):
						if not is_instance_valid(player): return
						if value == 0: player.stop()
						else: player.play(animations[value - 1]))
		var actions := _property_section("Actions")
		var navigation_row := HBoxContainer.new()
		navigation_row.add_theme_constant_override("separation", 6)
		actions.add_child(navigation_row)
		var frame_button := _button(navigation_row, tr("Frame layer"), func(): AppState.store.frame_layer(id, layer))
		frame_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var pause_button := _button(navigation_row, tr("Pause / resume"), func(): AppState.store.pause(id, layer.id, not layer.paused))
		pause_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var order_row := HBoxContainer.new()
		order_row.add_theme_constant_override("separation", 6)
		actions.add_child(order_row)
		var up_button := _button(order_row, tr("Move up"), func(): AppState.reorder(id, layer.id, -1))
		up_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var down_button := _button(order_row, tr("Move down"), func(): AppState.reorder(id, layer.id, 1))
		down_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var data_row := HBoxContainer.new()
		data_row.add_theme_constant_override("separation", 6)
		actions.add_child(data_row)
		if layer.kind != "script" or not AppState.store.records[layer.data_id].has("script_id"):
			var reload_button := _button(data_row, tr("Reload"), func():
				if AppState.store.reference_count(layer.data_id) > 1: _ask("Reload shared data in all referencing viewports?", func(): AppState.store.reload_data(layer.data_id))
				else: AppState.store.reload_data(layer.data_id))
			reload_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var remove_button := _button(data_row, tr("Remove"), func():
			var script_id: String = AppState.store.records[layer.data_id].get("script_id", "")
			if AppState.scripts.documents.has(script_id):
				_open_script(script_id)
				_script._request_remove(script_id)
			else: AppState.remove_layer(id, layer.id))
		remove_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	if view.last_pick.get("layer", "") == layer.id:
		var pick_fields := _property_section("Picked point")
		var pick_text := _label(pick_fields, str(view.last_pick.get("position", Vector3.ZERO)))
		pick_text.add_theme_font_size_override("font_size", 11)
		pick_text.add_theme_color_override("font_color", Ui.TEXT_3)
		var pick_actions := HBoxContainer.new()
		pick_actions.add_theme_constant_override("separation", 6)
		pick_fields.add_child(pick_actions)
		var focus_button := _button(pick_actions, tr("Focus picked point"), func():
			view.target = view.last_pick.position
			view.distance = maxf(0.01, view.distance * 0.25)
			view.update_camera())
		focus_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var copy_button := _button(pick_actions, tr("Copy details"), func(): DisplayServer.clipboard_set(AppState.redact(str(view.last_pick))))
		copy_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL

func _brep_face_properties(face: Dictionary, selected_node: Node) -> void:
	var fields := _property_section("BREP definition")
	fields.name = "BRepDefinitionFields"
	_label(fields, tr("Face index") + ": " + str(face.get("face_index", -1)))
	_label(fields, tr("Face persistent ID") + ": " + str(face.get("persistent_id", 0)))
	var definition: Dictionary = face.get("definition", {})
	var document: RefCounted = selected_node.mesh.get_meta("brep_document") if selected_node is MeshInstance3D and selected_node.mesh != null and selected_node.mesh.has_meta("brep_document") else null
	if document == null or definition.is_empty():
		_brep_inspector.clear()
		_label(fields, tr("BREP definition unavailable; reload the model."))
		return
	if definition.has("source_face_index"):
		_label(fields, tr("Source face index") + ": " + str(definition.source_face_index))
	else:
		var mapping := _label(fields, tr("Source face mapping is ambiguous; original JSON contains all candidate faces.") if definition.get("source_mapping") == "ambiguous" else tr("Source face mapping unavailable."))
		mapping.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		_label(fields, tr("Source face indices") + ": " + str(definition.get("source_face_indices", [])))
	var units := _label(fields, tr("Source geometry coordinates; before assembly and display transforms."))
	units.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	units.add_theme_color_override("font_color", Ui.TEXT_3)
	var hint := _label(fields, tr("Exported geometry uses the original face. OCC JSON is reconstructed on demand and may contain multiple repaired faces.") if definition.get("source_mapping") == "source_index" else tr("Original JSON contains the full source face; OCC JSON contains the selected reconstructed face."))
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var output: Dictionary = _brep_inspector.inspect(document, int(face.get("face_index", -1)))
	if output.is_empty():
		_label(fields, tr("Exporting face BREP JSON..."))
		return
	_brep_json_field(fields, "Original BREP JSON", "SourceBRep", str(output.get("source_json", "")), str(output.get("source_error", "")))
	_brep_json_field(fields, "OCC reconstructed BREP JSON", "ReconstructedBRep", str(output.get("reconstructed_json", "")), str(output.get("reconstructed_error", "")))

func _brep_json_field(parent: Node, title: String, prefix: String, text: String, error: String) -> void:
	var caption := _label(parent, tr(title))
	caption.add_theme_font_size_override("font_size", 13)
	if not error.is_empty():
		var message := _label(parent, tr("JSON export failed") + ": " + error)
		message.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		message.add_theme_color_override("font_color", Color("f07a82"))
		return
	var copy_button := _button(parent, "Copy JSON", func():
		DisplayServer.clipboard_set(text)
		AppState.status_changed.emit(tr("BREP definition copied")))
	copy_button.name = "Copy" + prefix
	var editor := TextEdit.new()
	editor.name = prefix + "Text"
	editor.editable = false
	editor.custom_minimum_size.y = 220
	editor.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	editor.add_theme_font_size_override("font_size", 12)
	# Keep the SDK's exact string, including full precision and topology schema.
	editor.text = text.left(65536)
	parent.add_child(editor)
	if text.length() > 65536:
		var hint := _label(parent, tr("Preview truncated; copy to obtain the full definition."))
		hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART

func _viewport_property_section(title: String, view: Node) -> VBoxContainer:
	return _property_section(title, view.property_section_expanded.get(title, false), func(expanded: bool):
		view.property_section_expanded[title] = expanded)

func _property_section(title: String, expanded := true, on_toggled: Callable = Callable()) -> VBoxContainer:
	var card := PanelContainer.new()
	card.add_theme_stylebox_override("panel", Ui.box(Ui.SURFACE, Ui.LINE, 7, 1, Vector4(9, 7, 9, 8)))
	_property_fields.add_child(card)
	var body := VBoxContainer.new()
	body.add_theme_constant_override("separation", 7)
	card.add_child(body)
	var header := Button.new()
	header.text = tr(title)
	header.icon = Ui.icon("chevron-down" if expanded else "chevron-right")
	Ui.set_icon_size(header, 10)
	header.alignment = HORIZONTAL_ALIGNMENT_LEFT
	header.add_theme_constant_override("h_separation", 6)
	header.add_theme_font_size_override("font_size", 13)
	header.add_theme_color_override("font_color", Ui.TEXT)
	header.custom_minimum_size.y = 24
	Ui.style_button(header, false)
	body.add_child(header)
	var fields := VBoxContainer.new()
	fields.add_theme_constant_override("separation", 6)
	fields.visible = expanded
	body.add_child(fields)
	header.pressed.connect(func():
		fields.visible = not fields.visible
		header.icon = Ui.icon("chevron-down" if fields.visible else "chevron-right")
		if on_toggled.is_valid(): on_toggled.call(fields.visible))
	return fields

func _compact_row(parent: Node) -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	row.custom_minimum_size.y = 25
	parent.add_child(row)
	return row

func _compact_label(parent: Node, text: String) -> Label:
	var label := _label(parent, text)
	label.custom_minimum_size.x = 82
	label.add_theme_font_size_override("font_size", 12)
	label.add_theme_color_override("font_color", Ui.TEXT_3)
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	return label

func _compact_choice(parent: Node, text: String, options: Array, selected: int, callback: Callable) -> OptionButton:
	var row := _compact_row(parent)
	_compact_label(row, text)
	var choice := OptionButton.new()
	for option in options: choice.add_item(option)
	choice.selected = selected
	choice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	choice.custom_minimum_size.y = 24
	choice.add_theme_font_size_override("font_size", 11)
	choice.item_selected.connect(callback)
	row.add_child(choice)
	return choice

func _make_slider_field(text: String, value: float, minimum: float, maximum: float, step: float, callback: Callable, suffix := "") -> VBoxContainer:
	var field := VBoxContainer.new()
	field.add_theme_constant_override("separation", 1)
	var caption := _label(field, text)
	caption.add_theme_font_size_override("font_size", 12)
	caption.add_theme_color_override("font_color", Ui.TEXT_2)
	var panel := PanelContainer.new()
	panel.add_theme_stylebox_override("panel", Ui.box(Ui.SURFACE, Ui.LINE, 6, 1, Vector4(8, 1, 8, 2)))
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	field.add_child(panel)
	var stack := VBoxContainer.new()
	stack.add_theme_constant_override("separation", 0)
	panel.add_child(stack)
	var digits := 0
	if step > 0.0 and step < 1.0:
		digits = int(ceil(-log(step) / log(10.0)))
	var format := "%." + str(digits) + "f"
	var display_value := 0.0 if absf(value) < step * 0.5 else value
	var readout_row := HBoxContainer.new()
	readout_row.alignment = BoxContainer.ALIGNMENT_CENTER
	readout_row.add_theme_constant_override("separation", 0)
	stack.add_child(readout_row)
	var readout := LineEdit.new()
	readout.text = format % display_value
	readout.alignment = HORIZONTAL_ALIGNMENT_CENTER
	readout.custom_minimum_size = Vector2(40, 20)
	readout.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	readout.add_theme_font_size_override("font_size", 14)
	readout.add_theme_color_override("font_color", Ui.TEXT)
	readout.add_theme_color_override("caret_color", Ui.TEXT)
	readout.add_theme_stylebox_override("normal", StyleBoxEmpty.new())
	readout.add_theme_stylebox_override("focus", StyleBoxEmpty.new())
	readout.add_theme_stylebox_override("read_only", StyleBoxEmpty.new())
	readout.focus_entered.connect(readout.select_all)
	readout_row.add_child(readout)
	if not suffix.is_empty():
		var suffix_label := _label(readout_row, suffix)
		suffix_label.add_theme_font_size_override("font_size", 13)
		suffix_label.add_theme_color_override("font_color", Ui.TEXT)
	var slider := HSlider.new()
	slider.min_value = minimum
	slider.max_value = maximum
	slider.step = step
	slider.value = value
	slider.custom_minimum_size.y = 10
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.add_theme_stylebox_override("slider", Ui.box(Color("#39454b"), Color.TRANSPARENT, 1, 0, Vector4(0, 1, 0, 1)))
	slider.add_theme_stylebox_override("grabber_area", Ui.box(Color("#d9e0e4"), Color.TRANSPARENT, 1, 0, Vector4(0, 1, 0, 1)))
	slider.add_theme_stylebox_override("grabber_area_highlight", Ui.box(Color("#d9e0e4"), Color.TRANSPARENT, 1, 0, Vector4(0, 1, 0, 1)))
	if _slider_grabber == null:
		var square := Image.create(6, 6, false, Image.FORMAT_RGBA8)
		square.fill(Color("#e8edf0"))
		_slider_grabber = ImageTexture.create_from_image(square)
	if _slider_grabber_highlight == null:
		var ring := Image.create(10, 10, false, Image.FORMAT_RGBA8)
		ring.fill(Color(0, 0, 0, 0))
		for y in 10:
			for x in 10:
				var distance := Vector2(x - 4.5, y - 4.5).length()
				if distance <= 4.5 and distance >= 2.5:
					var color := Color("#eef3f5")
					color.a = clampf(4.5 - distance, 0.0, 1.0)
					ring.set_pixel(x, y, color)
		_slider_grabber_highlight = ImageTexture.create_from_image(ring)
	slider.add_theme_icon_override("grabber", _slider_grabber)
	slider.add_theme_icon_override("grabber_highlight", _slider_grabber_highlight)
	slider.add_theme_icon_override("grabber_disabled", _slider_grabber)
	var commit := func():
		var parsed := clampf(readout.text.strip_edges().replace("°", "").to_float(), minimum, maximum)
		var shown := 0.0 if absf(parsed) < step * 0.5 else parsed
		readout.text = format % shown
		slider.value = parsed
		if callback.is_valid(): callback.call(parsed)
	readout.text_submitted.connect(func(_text: String): commit.call())
	readout.focus_exited.connect(commit)
	slider.value_changed.connect(func(changed: float):
		var shown := 0.0 if absf(changed) < step * 0.5 else changed
		readout.text = format % shown
		if callback.is_valid(): callback.call(changed))
	var slider_margin := MarginContainer.new()
	slider_margin.add_theme_constant_override("margin_top", 1)
	slider_margin.add_child(slider)
	stack.add_child(slider_margin)
	field.set_meta("slider", slider)
	return field

func _compact_number(parent: Node, text: String, value: float, minimum: float, maximum: float, step: float, callback: Callable, suffix := "") -> Control:
	var field := _make_slider_field(text, value, minimum, maximum, step, callback, suffix)
	parent.add_child(field)
	return field

func _compact_check(parent: Node, text: String, value: bool, callback: Callable) -> CheckBox:
	var row := _compact_row(parent)
	var label := _compact_label(row, text)
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var check := CheckBox.new()
	check.button_pressed = value
	check.custom_minimum_size.y = 24
	check.toggled.connect(callback)
	row.add_child(check)
	return check

func _compact_color(parent: Node, text: String, value: Color, callback: Callable) -> ColorPickerButton:
	var field := VBoxContainer.new()
	field.add_theme_constant_override("separation", 2)
	parent.add_child(field)
	var caption := _label(field, text)
	caption.add_theme_font_size_override("font_size", 12)
	caption.add_theme_color_override("font_color", Ui.TEXT_2)
	var color := ColorPickerButton.new()
	color.color = value
	color.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	color.custom_minimum_size.y = 28
	color.color_changed.connect(callback)
	field.add_child(color)
	return color

func _build_settings_panel() -> void:
	if is_instance_valid(_property_title): _property_title.text = tr("Settings")
	var ui := _settings_section("UI")
	_choice(ui, tr("Language"), ["English", "简体中文"], 1 if TranslationServer.get_locale().begins_with("zh") else 0, func(index: int):
		TranslationServer.set_locale("zh_CN" if index == 1 else "en"))
	_choice(ui, tr("Theme"), [Profile.value("theme_name")], 0, func(_index: int): pass)
	var theme_note := _label(ui, "%s · %s" % [Profile.value("product_name"), Profile.id()])
	theme_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	theme_note.add_theme_color_override("font_color", Ui.TEXT_3)
	theme_note.add_theme_font_size_override("font_size", 11)
	_button(ui, tr("About"), _about)

	var mcp := _settings_section("MCP")
	_check(mcp, tr("Enable local bridge"), AppState.bridge.enabled, func(value: bool):
		if value: AppState.bridge.start()
		else: AppState.bridge.stop())
	var endpoint_label: Label
	_choice(mcp, tr("Automation transport"), ["MCP", "Plugin bridge"], 1 if AppState.bridge.is_plugin() else 0,
		func(index: int): _switch_automation("plugin" if index == 1 else "mcp", endpoint_label))
	endpoint_label = _label(mcp, "http://127.0.0.1:%d/%s" % [AppState.bridge.port, "bridge" if AppState.bridge.is_plugin() else "mcp"])
	endpoint_label.add_theme_color_override("font_color", Ui.ACCENT)
	endpoint_label.add_theme_font_size_override("font_size", 11)
	for permission in ["scene.read", "script.run", "capture"]:
		_check(mcp, permission, AppState.bridge.permissions[permission], func(value: bool): AppState.bridge.permissions[permission] = value)
	var token_row := HBoxContainer.new()
	mcp.add_child(token_row)
	_button(token_row, tr("Copy token"), func():
		if AppState.bridge.ensure_token() == OK: DisplayServer.clipboard_set(AppState.bridge.token)
		else: _status.text = tr("Bridge token could not be loaded or saved."))
	_button(token_row, tr("Reset token"), func(): _change_mcp_token(get_window(), false))
	var explanation := _label(mcp, tr("No built-in AI. External clients may submit scripts for manual review."))
	explanation.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	explanation.add_theme_color_override("font_color", Ui.TEXT_3)
	explanation.add_theme_font_size_override("font_size", 11)

func _settings() -> void:
	var dialog := AcceptDialog.new()
	dialog.title = tr("Settings")
	var box := VBoxContainer.new()
	dialog.add_child(box)
	_choice(box, "Language", ["English", "简体中文"], 1 if TranslationServer.get_locale().begins_with("zh") else 0, func(index: int):
		TranslationServer.set_locale("zh_CN" if index == 1 else "en")
		_refresh_properties()
		_script._update_status()
		_script.refresh_tree()
		for pane in panes.values(): pane.refresh())
	var endpoint_label: Label
	_choice(box, "Automation transport", ["MCP", "Plugin bridge"], 1 if AppState.bridge.is_plugin() else 0,
		func(index: int): _switch_automation("plugin" if index == 1 else "mcp", endpoint_label))
	endpoint_label = _label(box, "http://127.0.0.1:%d/%s" % [AppState.bridge.port, "bridge" if AppState.bridge.is_plugin() else "mcp"])
	_check(box, "Enable local bridge", AppState.bridge.enabled, func(value: bool):
		if value: AppState.bridge.start()
		else: AppState.bridge.stop())
	for permission in ["scene.read", "script.run", "capture"]:
		_check(box, permission, AppState.bridge.permissions[permission], func(value: bool): AppState.bridge.permissions[permission] = value)
	var token_note := _label(box, "Bridge token is saved locally and reused after restart.")
	token_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var tokens := HBoxContainer.new()
	box.add_child(tokens)
	_button(tokens, "Copy token", func():
		if AppState.bridge.ensure_token() == OK: DisplayServer.clipboard_set(AppState.bridge.token)
		else: _status.text = tr("Bridge token could not be loaded or saved."))
	_button(tokens, "Use existing token", _change_mcp_token.bind(dialog, true))
	_button(tokens, "Reset token", _change_mcp_token.bind(dialog, false))
	var explanation := _label(box, "No built-in AI. External clients may submit scripts for manual review.")
	explanation.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	dialog.confirmed.connect(dialog.queue_free)
	dialog.canceled.connect(dialog.queue_free)
	add_child(dialog)
	Dpi.popup(dialog, Vector2i(560, 360))

func _change_mcp_token(settings: Window, use_existing: bool) -> void:
	var dialog := ConfirmationDialog.new()
	dialog.title = tr("Use existing bridge token" if use_existing else "Reset bridge token")
	dialog.dialog_text = tr("Paste your existing Bearer token (without the Bearer prefix)." if use_existing else "Generate a new bridge token? Connected clients must update their credentials.")
	dialog.get_label().autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var input := LineEdit.new()
	input.secret = true
	input.max_length = 512
	input.visible = use_existing
	dialog.add_child(input)
	dialog.confirmed.connect(func():
		var error: int = AppState.bridge.replace_token(input.text) if use_existing else AppState.bridge.reset_token()
		input.clear()
		_status.text = tr("Bridge token saved. Reconnect clients to start a new session.") if error == OK else tr("Bridge token update failed. Use 32–512 valid Bearer characters and a writable user directory.")
		dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free)
	settings.add_child(dialog)
	Dpi.popup(dialog, Vector2i(520, 160), settings)

func _open_draft(source: String, filename: String) -> void:
	_show_panel("script")
	_script._ask_replace(source, filename)

func _open_script(id: String) -> void:
	if not AppState.scripts.documents.has(id): return
	_show_panel("script")
	_script.select_script(id)

func _show_diagnostics() -> void:
	var report := {"engine": Engine.get_version_info().string, "session": AppState.describe(), "errors": AppState.errors, "fps": Engine.get_frames_per_second()}
	_report.text = AppState.redact(JSON.stringify(report, "  "))
	Dpi.popup(_diagnostics)

func _export_report() -> void:
	var dialog := FileDialog.new()
	dialog.access = FileDialog.ACCESS_FILESYSTEM
	dialog.file_mode = FileDialog.FILE_MODE_SAVE_FILE
	dialog.current_file = "digitalviewer-diagnostics.json"
	dialog.filters = PackedStringArray(["*.json;JSON"])
	var text := _report.text
	dialog.file_selected.connect(func(path: String):
		var file := FileAccess.open(path, FileAccess.WRITE)
		if file:
			file.store_string(text)
			file.close()
		dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free)
	_diagnostics.add_child(dialog)
	Dpi.popup(dialog, Vector2i.ZERO, _diagnostics, 0.6)

func _about() -> void:
	var dialog := AcceptDialog.new()
	dialog.title = Profile.value("product_name") + " 1.0.0"
	dialog.dialog_text = Engine.get_version_info().string + "\n\n" + Profile.id() + " · " + Profile.value("description")
	dialog.confirmed.connect(dialog.queue_free)
	dialog.canceled.connect(dialog.queue_free)
	add_child(dialog)
	Dpi.popup(dialog, Vector2i(640, 200))

func _benchmark_sample(count: int, orbit: bool) -> Dictionary:
	var view: Node = AppState.views[AppState.active_id]
	var cpu: Array[float] = []
	var draw_calls: Array[float] = []
	var primitives: Array[float] = []
	var video_memory: Array[float] = []
	var fps: Array[float] = []
	for index in count:
		if orbit:
			view.orbit(Vector2(0.0018, 0.0))
		await get_tree().process_frame
		cpu.append(Performance.get_monitor(Performance.TIME_PROCESS) * 1000.0)
		draw_calls.append(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME))
		primitives.append(Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME))
		video_memory.append(Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED) / 1048576.0)
		fps.append(Performance.get_monitor(Performance.TIME_FPS))
	return {
		"frames": count,
		"cpu_ms": _benchmark_percentiles(cpu),
		"draw_calls": _benchmark_percentiles(draw_calls),
		"primitives": _benchmark_percentiles(primitives),
		"video_mb": _benchmark_percentiles(video_memory),
		"fps": _benchmark_percentiles(fps),
		"nodes": Performance.get_monitor(Performance.OBJECT_NODE_COUNT),
		"resources": Performance.get_monitor(Performance.OBJECT_RESOURCE_COUNT),
	}

func _benchmark_percentiles(values: Array[float]) -> Dictionary:
	if values.is_empty(): return {}
	var sorted := values.duplicate()
	sorted.sort()
	var total := 0.0
	for value in sorted: total += value
	var result := {
		"avg": total / sorted.size(),
		"p50": sorted[clampi(int(sorted.size() * 0.50), 0, sorted.size() - 1)],
		"p95": sorted[clampi(int(sorted.size() * 0.95), 0, sorted.size() - 1)],
		"p99": sorted[clampi(int(sorted.size() * 0.99), 0, sorted.size() - 1)],
		"max": sorted[sorted.size() - 1],
	}
	return result

func _run_script_benchmark() -> void:
	await get_tree().process_frame
	var before := await _benchmark_sample(180, false)
	_script.show()
	_script.open_workbench()
	await get_tree().create_timer(0.5).timeout
	var after := await _benchmark_sample(180, false)
	print("SCRIPT_BENCHMARK=", JSON.stringify({"before": before, "after": after}))
	get_tree().quit()

func _run_benchmark() -> void:
	var result := {}
	await get_tree().process_frame
	result["empty"] = await _benchmark_sample(120, false)
	var view: Node = AppState.views[AppState.active_id]
	var path := "E:/WorkSpace/TJH/Mesh调度测试/桃园市政府/Data/Tile_314341526340/Tile_314341526340.osgb"
	var started := Time.get_ticks_msec()
	var layer_id := AppState.add_source(AppState.active_id, path)
	var layer: Dictionary = view.get_layer(layer_id)
	var deadline := started + 120000
	while Time.get_ticks_msec() < deadline:
		layer = view.get_layer(layer_id)
		var stats: Dictionary = layer.get("stats", {})
		if layer.status == "Ready" and (stats.is_empty() or int(stats.get("resident_contents", 0)) > 0):
			break
		await get_tree().process_frame
	layer = view.get_layer(layer_id)
	result["ready_ms"] = Time.get_ticks_msec() - started
	await get_tree().create_timer(4.0).timeout
	result["loaded"] = await _benchmark_sample(360, true)
	result["stats"] = layer.get("stats", {})
	if is_instance_valid(layer.node) and layer.node.has_method("get_streaming_stats"):
		result["streaming"] = layer.node.call("get_streaming_stats")
	if is_instance_valid(layer.node) and layer.node.has_method("get_dataset_aabb"):
		result["aabb"] = str(layer.node.call("get_dataset_aabb"))
	AppState.store.pause(AppState.active_id, layer.id, true)
	await get_tree().create_timer(0.5).timeout
	result["paused"] = await _benchmark_sample(120, false)
	_script.show()
	_script.open_workbench()
	await get_tree().create_timer(0.5).timeout
	result["script_panel"] = await _benchmark_sample(180, false)
	if is_instance_valid(layer.node) and layer.node.has_method("set_receive_lighting_mode"):
		layer.node.call("set_receive_lighting_mode", 0)
		await get_tree().create_timer(0.5).timeout
		result["unlit"] = await _benchmark_sample(180, true)
	var output := "E:/WorkSpace/GitHub/godot/projects/DigitalViewer/build/benchmark.json"
	var file := FileAccess.open(output, FileAccess.WRITE)
	if file:
		file.store_string(JSON.stringify(result, "\t"))
		file.close()
	print("BENCHMARK_PATH=", output)
	print(JSON.stringify(result))
	get_tree().quit()

func _process(delta: float) -> void:
	_elapsed += delta
	if _elapsed >= 0.5:
		_elapsed = 0
		_fps.text = "%d FPS" % Engine.get_frames_per_second()

func _unhandled_key_input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo: return
	if event.keycode == KEY_ESCAPE:
		if _full_id != "": toggle_fullscreen(_full_id)
		else: AppState.views[AppState.active_id].cancel_flight()
	elif event.ctrl_pressed:
		if event.keycode >= KEY_1 and event.keycode <= KEY_9:
			var index: int = event.keycode - KEY_1
			if index < AppState.views.size(): AppState.activate(AppState.views.keys()[index])
		elif event.keycode == KEY_P: _properties.visible = not _properties.visible
		elif event.keycode == KEY_L: panes[AppState.active_id].tree.visible = not panes[AppState.active_id].tree.visible
		elif event.keycode == KEY_F and event.shift_pressed: toggle_fullscreen(AppState.active_id)
		else: return
	else: return
	get_viewport().set_input_as_handled()

func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST: _quit()

func _quit() -> void:
	AppState.shutdown()
	get_tree().quit()

func _button(parent: Node, text: String, callback: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size.y = 30
	button.pressed.connect(callback)
	parent.add_child(button)
	return button

func _label(parent: Node, text: String) -> Label:
	var label := Label.new()
	label.text = text
	parent.add_child(label)
	return label

func _check(parent: Node, text: String, value: bool, callback: Callable) -> CheckBox:
	var check := CheckBox.new()
	check.text = text
	check.button_pressed = value
	check.toggled.connect(callback)
	parent.add_child(check)
	return check

func _number(parent: Node, text: String, value: float, minimum: float, maximum: float, step: float, callback: Callable) -> SpinBox:
	_label(parent, text)
	var spin := SpinBox.new()
	spin.min_value = minimum
	spin.max_value = maximum
	spin.step = step
	spin.value = value
	if callback.is_valid(): spin.value_changed.connect(callback)
	parent.add_child(spin)
	return spin

func _choice(parent: Node, text: String, options: Array, selected: int, callback: Callable) -> void:
	_label(parent, text)
	var choice := OptionButton.new()
	for option in options: choice.add_item(option)
	choice.selected = selected
	choice.item_selected.connect(callback)
	parent.add_child(choice)
