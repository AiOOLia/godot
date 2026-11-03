extends Control
class_name DigitalViewerMobileShell

signal add_data_requested
signal fly_to_requested
signal chat_requested
signal script_requested

const LAYER_ITEM_SCENE := preload("res://scenes/components/layer_item.tscn")
const VIEWER_VIEWPORT_SCENE := preload("res://scenes/components/viewer_viewport.tscn")
const COLOR_PANEL := Color("10181e")
const COLOR_RAISED := Color("162128")
const COLOR_LINE := Color("2a363e")
const COLOR_MUTED := Color("91a0a6")
const COLOR_ACCENT := Color("2cb79a")

var _safe_margin: MarginContainer
var _safe_content: Control
var _overlay: ColorRect
var _drawer: PanelContainer
var _sheet: PanelContainer
var _layer_list: VBoxContainer
var _sheet_content: VBoxContainer
var _nav_buttons: Array[Button] = []
var _status_toast: Label


func _ready() -> void:
	_build_ui()
	AppState.layers_changed.connect(_refresh_layers)
	AppState.selected_layer_changed.connect(_on_selected_layer_changed)
	AppState.status_changed.connect(_show_status)
	get_viewport().size_changed.connect(_on_viewport_size_changed)
	_refresh_layers()
	_update_safe_area()


func _build_ui() -> void:
	var background := ColorRect.new()
	background.color = Color("07131a")
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(background)

	_safe_margin = MarginContainer.new()
	_safe_margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(_safe_margin)
	_safe_content = Control.new()
	_safe_margin.add_child(_safe_content)

	var root := VBoxContainer.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_theme_constant_override("separation", 0)
	_safe_content.add_child(root)

	root.add_child(_build_topbar())
	var viewer: Control = VIEWER_VIEWPORT_SCENE.instantiate()
	viewer.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root.add_child(viewer)
	root.add_child(_build_bottom_navigation())

	_overlay = ColorRect.new()
	_overlay.color = Color(0, 0, 0, 0.46)
	_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	_overlay.gui_input.connect(_on_overlay_input)
	_overlay.visible = false
	_safe_content.add_child(_overlay)

	_drawer = _build_layer_drawer()
	_drawer.visible = false
	_safe_content.add_child(_drawer)

	_sheet = _build_bottom_sheet()
	_sheet.visible = false
	_safe_content.add_child(_sheet)

	var add_button := Button.new()
	add_button.text = "＋"
	add_button.tooltip_text = "添加数据"
	add_button.custom_minimum_size = Vector2(54, 54)
	add_button.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	add_button.position = Vector2(-70, -142)
	_style_primary(add_button, 27)
	add_button.pressed.connect(add_data_requested.emit)
	_safe_content.add_child(add_button)

	_status_toast = Label.new()
	_status_toast.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	_status_toast.offset_left = 14.0
	_status_toast.offset_top = -142.0
	_status_toast.offset_right = -98.0
	_status_toast.offset_bottom = -100.0
	_status_toast.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	_status_toast.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_status_toast.add_theme_color_override("font_color", Color("d8e6e8"))
	_status_toast.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.75))
	_status_toast.add_theme_constant_override("outline_size", 5)
	_status_toast.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_status_toast.visible = false
	_safe_content.add_child(_status_toast)


func _build_topbar() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.y = 58
	panel.add_theme_stylebox_override("panel", _flat_style(COLOR_RAISED, 0, COLOR_LINE, 0, 0, 1, 0))
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 7)
	margin.add_theme_constant_override("margin_right", 7)
	margin.add_theme_constant_override("margin_top", 6)
	margin.add_theme_constant_override("margin_bottom", 6)
	panel.add_child(margin)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 5)
	margin.add_child(row)
	var layers := _make_icon_button("☰", "打开图层")
	layers.pressed.connect(_open_drawer)
	row.add_child(layers)
	var logo := _make_logo_badge(30)
	row.add_child(logo)
	var title := Label.new()
	title.text = "DigitalViewer"
	title.add_theme_font_size_override("font_size", 15)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	row.add_child(title)
	var fps := Label.new()
	fps.text = "● 60"
	fps.add_theme_color_override("font_color", COLOR_ACCENT)
	row.add_child(fps)
	var fly := _make_icon_button("⌖", "飞往坐标")
	fly.pressed.connect(fly_to_requested.emit)
	row.add_child(fly)
	var more := _make_icon_button("⋮", "更多操作")
	more.pressed.connect(_open_status_sheet)
	row.add_child(more)
	return panel


func _build_bottom_navigation() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.y = 72
	panel.add_theme_stylebox_override("panel", _flat_style(COLOR_RAISED, 0, COLOR_LINE, 0, 1, 0, 0))
	var row := HBoxContainer.new()
	panel.add_child(row)
	var entries := [
		["◎\n视图", Callable()],
		["▱\n图层", _open_drawer],
		["☷\n属性", _open_property_sheet],
		["△\n状态", _open_status_sheet],
		["AI\n助手", chat_requested.emit],
	]
	for index: int in entries.size():
		var button := Button.new()
		button.text = entries[index][0]
		button.flat = true
		button.toggle_mode = true
		button.button_pressed = index == 0
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		button.custom_minimum_size.y = 64
		button.add_theme_font_size_override("font_size", 12)
		if entries[index][1].is_valid():
			button.pressed.connect(entries[index][1])
		button.pressed.connect(_select_nav.bind(index))
		row.add_child(button)
		_nav_buttons.append(button)
	return panel


func _build_layer_drawer() -> PanelContainer:
	var panel := PanelContainer.new()
	panel.set_anchors_preset(Control.PRESET_LEFT_WIDE)
	panel.anchor_right = 0.0
	panel.offset_right = 330.0
	panel.add_theme_stylebox_override("panel", _flat_style(COLOR_PANEL, 0, COLOR_LINE, 0, 0, 1, 0))
	var column := VBoxContainer.new()
	panel.add_child(column)
	var header_margin := MarginContainer.new()
	header_margin.custom_minimum_size.y = 58
	header_margin.add_theme_constant_override("margin_left", 14)
	header_margin.add_theme_constant_override("margin_right", 8)
	header_margin.add_theme_constant_override("margin_top", 7)
	header_margin.add_theme_constant_override("margin_bottom", 7)
	column.add_child(header_margin)
	var header := HBoxContainer.new()
	header_margin.add_child(header)
	var title := Label.new()
	title.text = "图层"
	title.add_theme_font_size_override("font_size", 16)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	header.add_child(title)
	var add_button := _make_icon_button("＋", "添加数据")
	add_button.pressed.connect(add_data_requested.emit)
	header.add_child(add_button)
	var close := _make_icon_button("×", "关闭")
	close.pressed.connect(_close_overlays)
	header.add_child(close)
	column.add_child(HSeparator.new())
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	column.add_child(scroll)
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 7)
	margin.add_theme_constant_override("margin_top", 7)
	margin.add_theme_constant_override("margin_right", 7)
	margin.add_theme_constant_override("margin_bottom", 14)
	margin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(margin)
	_layer_list = VBoxContainer.new()
	_layer_list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	margin.add_child(_layer_list)
	return panel


func _build_bottom_sheet() -> PanelContainer:
	var panel := PanelContainer.new()
	panel.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	panel.anchor_top = 1.0
	panel.offset_top = -390.0
	panel.add_theme_stylebox_override("panel", _flat_style(COLOR_PANEL, 18, COLOR_LINE, 0, 1, 0, 0))
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 16)
	margin.add_theme_constant_override("margin_top", 10)
	margin.add_theme_constant_override("margin_right", 16)
	margin.add_theme_constant_override("margin_bottom", 16)
	panel.add_child(margin)
	_sheet_content = VBoxContainer.new()
	_sheet_content.add_theme_constant_override("separation", 9)
	margin.add_child(_sheet_content)
	return panel


func _refresh_layers() -> void:
	for child: Node in _layer_list.get_children():
		child.queue_free()
	var current_group := ""
	for layer: Dictionary in AppState.layers:
		var group: String = layer.get("group", "其他")
		if current_group != group:
			current_group = group
			var group_label := Label.new()
			group_label.text = group
			group_label.custom_minimum_size.y = 32
			group_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
			group_label.add_theme_font_size_override("font_size", 12)
			group_label.add_theme_color_override("font_color", COLOR_MUTED)
			_layer_list.add_child(group_label)
		var item: DigitalViewerLayerItem = LAYER_ITEM_SCENE.instantiate()
		item.setup(layer, layer["id"] == AppState.selected_layer_id)
		item.selected.connect(_select_layer)
		item.layer_visibility_toggled.connect(AppState.set_layer_visible)
		_layer_list.add_child(item)


func _select_layer(layer_id: StringName) -> void:
	AppState.select_layer(layer_id)
	_open_property_sheet()


func _on_selected_layer_changed(_layer_id: StringName) -> void:
	_refresh_layers()
	if _sheet.visible:
		_fill_property_sheet()


func _open_drawer() -> void:
	_close_panels_only()
	_overlay.visible = true
	_drawer.visible = true


func _open_property_sheet() -> void:
	_close_panels_only()
	_fill_property_sheet()
	_overlay.visible = true
	_sheet.visible = true


func _fill_property_sheet() -> void:
	_clear_sheet()
	_sheet_content.add_child(_sheet_handle())
	var layer := AppState.get_layer(AppState.selected_layer_id)
	var title := Label.new()
	title.text = "%s · 属性" % layer.get("name", "未选择图层")
	title.add_theme_font_size_override("font_size", 16)
	_sheet_content.add_child(title)
	_sheet_content.add_child(_key_value("格式", str(layer.get("kind", "未知"))))
	_sheet_content.add_child(_key_value("坐标系", "WGS84 / ECEF"))
	_sheet_content.add_child(_key_value("状态", str(layer.get("status", "")), COLOR_ACCENT))
	var visible_toggle := CheckButton.new()
	visible_toggle.text = "图层可见"
	visible_toggle.button_pressed = layer.get("visible", true)
	visible_toggle.toggled.connect(func(value: bool) -> void: AppState.set_layer_visible(AppState.selected_layer_id, value))
	_sheet_content.add_child(visible_toggle)
	var opacity_row := VBoxContainer.new()
	var opacity_label := Label.new()
	opacity_label.text = "透明度  100%"
	opacity_row.add_child(opacity_label)
	var opacity := HSlider.new()
	opacity.min_value = 0
	opacity.max_value = 100
	opacity.value = 100
	opacity.value_changed.connect(func(value: float) -> void: opacity_label.text = "透明度  %.0f%%" % value)
	opacity_row.add_child(opacity)
	_sheet_content.add_child(opacity_row)
	var close := Button.new()
	close.text = "完成"
	close.pressed.connect(_close_overlays)
	_sheet_content.add_child(close)


func _open_status_sheet() -> void:
	_close_panels_only()
	_clear_sheet()
	_sheet_content.add_child(_sheet_handle())
	var title := Label.new()
	title.text = "会话状态"
	title.add_theme_font_size_override("font_size", 16)
	_sheet_content.add_child(title)
	var scripts := Button.new()
	scripts.text = "脚本工作台 · GDScript"
	scripts.pressed.connect(func() -> void:
		_close_overlays()
		script_requested.emit())
	_sheet_content.add_child(scripts)
	_sheet_content.add_child(_key_value("帧率", "60 FPS", COLOR_ACCENT))
	_sheet_content.add_child(_key_value("Resident", "%.0f MB / 384 MB" % AppState.total_memory_mb()))
	_sheet_content.add_child(_key_value("请求", "6（下载 2 / 解析 1）"))
	_sheet_content.add_child(_key_value("网络", "8.4 MB/s"))
	var warning := Label.new()
	warning.text = "△ 2 条格式警告 · 未发现失败任务"
	warning.add_theme_color_override("font_color", Color("e1aa4c"))
	warning.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_sheet_content.add_child(warning)
	var close := Button.new()
	close.text = "完成"
	close.pressed.connect(_close_overlays)
	_sheet_content.add_child(close)
	_overlay.visible = true
	_sheet.visible = true


func _clear_sheet() -> void:
	for child: Node in _sheet_content.get_children():
		child.queue_free()


func _close_panels_only() -> void:
	_drawer.visible = false
	_sheet.visible = false


func _close_overlays() -> void:
	_close_panels_only()
	_overlay.visible = false
	_select_nav(0)


func _on_overlay_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		_close_overlays()
	elif event is InputEventScreenTouch and event.pressed:
		_close_overlays()


func _select_nav(index: int) -> void:
	for item_index: int in _nav_buttons.size():
		_nav_buttons[item_index].set_pressed_no_signal(item_index == index)


func _show_status(message: String) -> void:
	_status_toast.text = message
	_status_toast.visible = true
	var tween := create_tween()
	tween.tween_interval(2.5)
	tween.tween_callback(func() -> void: _status_toast.visible = false)


func _on_viewport_size_changed() -> void:
	_update_safe_area()
	if _drawer:
		_drawer.offset_right = minf(340.0, size.x * 0.86)


func _update_safe_area() -> void:
	if not OS.has_feature("mobile"):
		return
	var screen_size := DisplayServer.screen_get_size()
	var safe := DisplayServer.get_display_safe_area()
	if screen_size.x <= 0 or screen_size.y <= 0:
		return
	_safe_margin.add_theme_constant_override("margin_left", roundi(float(safe.position.x) / screen_size.x * size.x))
	_safe_margin.add_theme_constant_override("margin_top", roundi(float(safe.position.y) / screen_size.y * size.y))
	_safe_margin.add_theme_constant_override("margin_right", roundi(float(screen_size.x - safe.end.x) / screen_size.x * size.x))
	_safe_margin.add_theme_constant_override("margin_bottom", roundi(float(screen_size.y - safe.end.y) / screen_size.y * size.y))


func _make_icon_button(text: String, tooltip: String) -> Button:
	var button := Button.new()
	button.text = text
	button.tooltip_text = tooltip
	button.custom_minimum_size = Vector2(44, 44)
	button.add_theme_font_size_override("font_size", 18)
	return button


func _make_logo_badge(side: int) -> PanelContainer:
	var badge := PanelContainer.new()
	badge.custom_minimum_size = Vector2(side, side)
	badge.add_theme_stylebox_override("panel", _flat_style(COLOR_ACCENT, 8))
	var label := Label.new()
	label.text = "DV"
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.add_theme_font_size_override("font_size", 10)
	label.add_theme_color_override("font_color", Color.WHITE)
	badge.add_child(label)
	return badge


func _sheet_handle() -> Control:
	var center := CenterContainer.new()
	center.custom_minimum_size.y = 14
	var handle := ColorRect.new()
	handle.color = Color("45535b")
	handle.custom_minimum_size = Vector2(44, 4)
	center.add_child(handle)
	return center


func _key_value(key: String, value: String, color := Color.WHITE) -> Control:
	var row := HBoxContainer.new()
	row.custom_minimum_size.y = 30
	var key_label := Label.new()
	key_label.text = key
	key_label.custom_minimum_size.x = 92
	key_label.add_theme_color_override("font_color", COLOR_MUTED)
	row.add_child(key_label)
	var value_label := Label.new()
	value_label.text = value
	value_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	value_label.add_theme_color_override("font_color", color)
	value_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	row.add_child(value_label)
	return row


func _style_primary(button: Button, radius: int) -> void:
	button.add_theme_stylebox_override("normal", _flat_style(Color("168d76"), radius))
	button.add_theme_stylebox_override("hover", _flat_style(Color("20a489"), radius))
	button.add_theme_stylebox_override("pressed", _flat_style(Color("126e5e"), radius))


func _flat_style(color: Color, radius: int, border_color := Color.TRANSPARENT, left := 0, top := 0, right := 0, bottom := 0) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.corner_radius_top_left = radius
	style.corner_radius_top_right = radius
	style.corner_radius_bottom_left = radius
	style.corner_radius_bottom_right = radius
	style.border_color = border_color
	style.border_width_left = left
	style.border_width_top = top
	style.border_width_right = right
	style.border_width_bottom = bottom
	return style
