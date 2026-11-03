extends Control
class_name DigitalViewerDesktopShell

signal add_data_requested
signal fly_to_requested
signal chat_requested
signal script_requested

const LAYER_ITEM_SCENE := preload("res://scenes/components/layer_item.tscn")
const VIEWER_VIEWPORT_SCENE := preload("res://scenes/components/viewer_viewport.tscn")
const COLOR_PANEL := Color("10181e")
const COLOR_PANEL_RAISED := Color("162128")
const COLOR_LINE := Color("2a363e")
const COLOR_MUTED := Color("91a0a6")
const COLOR_ACCENT := Color("2cb79a")

var _topbar: Control
var _left_panel: Control
var _right_panel: Control
var _bottom_panel: Control
var _diagnostics: Control
var _layer_list: VBoxContainer
var _property_content: VBoxContainer
var _selected_title: Label
var _status_label: Label
var _memory_label: Label
var _grid_button: Button
var _terrain_value_label: Label
var _full_viewport := false
var _chat_slot: Control


func _ready() -> void:
	_build_ui()
	_connect_state()
	_refresh_layers()
	_refresh_properties(AppState.selected_layer_id)
	_refresh_memory()


func _unhandled_key_input(event: InputEvent) -> void:
	if not is_visible_in_tree():
		return
	if not event is InputEventKey or not event.pressed or event.echo:
		return
	if event.ctrl_pressed and event.physical_keycode == KEY_L:
		_left_panel.visible = not _left_panel.visible
	elif event.ctrl_pressed and event.physical_keycode == KEY_P:
		_right_panel.visible = not _right_panel.visible
	elif event.ctrl_pressed and event.shift_pressed and event.physical_keycode == KEY_F:
		_toggle_full_viewport()


func _connect_state() -> void:
	AppState.layers_changed.connect(_refresh_layers)
	AppState.layers_changed.connect(_refresh_memory)
	AppState.selected_layer_changed.connect(_on_selected_layer_changed)
	AppState.status_changed.connect(_set_status)
	AppState.grid_visibility_changed.connect(_on_grid_visibility_changed)


func _build_ui() -> void:
	var background := ColorRect.new()
	background.color = Color("0b1115")
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background)

	var root := VBoxContainer.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_theme_constant_override("separation", 0)
	add_child(root)

	_topbar = _build_topbar()
	root.add_child(_topbar)

	var work_split := HSplitContainer.new()
	work_split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	work_split.split_offset = 0
	work_split.dragger_visibility = SplitContainer.DRAGGER_VISIBLE
	root.add_child(work_split)

	_left_panel = _build_layers_panel()
	_left_panel.custom_minimum_size.x = 250
	work_split.add_child(_left_panel)

	var center_right := HSplitContainer.new()
	center_right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	center_right.split_offset = 0
	work_split.add_child(center_right)

	var center := _build_center_panel()
	center.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	center.custom_minimum_size.x = 420
	center_right.add_child(center)

	_right_panel = _build_properties_panel()
	_right_panel.custom_minimum_size.x = 310
	center_right.add_child(_right_panel)
	_chat_slot = Control.new()
	_chat_slot.custom_minimum_size.x = 420
	_chat_slot.visible = false
	center_right.add_child(_chat_slot)

	_bottom_panel = _build_statusbar()
	root.add_child(_bottom_panel)


func set_chat_open(open: bool) -> void:
	_chat_slot.custom_minimum_size.x = 420
	_chat_slot.visible = open
	_right_panel.visible = not open and not _full_viewport


func set_script_open(open: bool, width: float = 420) -> void:
	_left_panel.visible = not open and not _full_viewport
	_right_panel.visible = not open and not _full_viewport
	_chat_slot.visible = open
	_chat_slot.custom_minimum_size.x = width + 12 if open else 420


func _build_topbar() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.y = 58
	panel.add_theme_stylebox_override("panel", _flat_style(COLOR_PANEL_RAISED, 0, COLOR_LINE, 0, 0, 1, 0))
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 12)
	margin.add_theme_constant_override("margin_right", 12)
	margin.add_theme_constant_override("margin_top", 8)
	margin.add_theme_constant_override("margin_bottom", 8)
	panel.add_child(margin)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 7)
	margin.add_child(row)

	var brand := HBoxContainer.new()
	brand.custom_minimum_size.x = 190
	brand.add_theme_constant_override("separation", 9)
	row.add_child(brand)
	var logo := _make_logo_badge(34)
	brand.add_child(logo)
	var brand_label := Label.new()
	brand_label.text = "DigitalViewer  v1.0"
	brand_label.add_theme_font_size_override("font_size", 15)
	brand_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	brand.add_child(brand_label)

	var add_button := _make_button("＋  添加数据", "添加本地、远程或 Cesium ion 数据")
	_style_primary_button(add_button)
	add_button.pressed.connect(add_data_requested.emit)
	row.add_child(add_button)

	row.add_child(VSeparator.new())
	var fly_button := _make_button("⌖  飞往", "输入经纬度并平滑飞行")
	fly_button.pressed.connect(fly_to_requested.emit)
	row.add_child(fly_button)
	var bookmark_button := _make_button("◇  保存视点", "在当前会话中保存视点")
	bookmark_button.pressed.connect(func() -> void:
		AppState.viewpoints["视点 %d" % (AppState.viewpoints.size() + 1)] = AppState.camera_snapshot()
		AppState.revision += 1
		_set_status("当前视点已保存到会话"))
	row.add_child(bookmark_button)
	_grid_button = _make_button("▦  经纬网", "显示或隐藏经纬网")
	_grid_button.toggle_mode = true
	_grid_button.button_pressed = AppState.grid_visible
	_grid_button.toggled.connect(AppState.set_grid_visible)
	row.add_child(_grid_button)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(spacer)
	var script_button := _make_button("脚本", "应用内原生 GDScript 工作台")
	script_button.pressed.connect(script_requested.emit)
	row.add_child(script_button)
	var chat_button := _make_button("AI 助手", "打开或关闭聊天")
	chat_button.pressed.connect(chat_requested.emit)
	row.add_child(chat_button)

	var fps := Label.new()
	fps.text = "● 60 FPS"
	fps.add_theme_color_override("font_color", COLOR_ACCENT)
	fps.tooltip_text = "目标帧率；当前为 UI 原型显示值"
	row.add_child(fps)

	var camera_button := _make_button("轨道相机", "切换轨道/飞行相机")
	camera_button.pressed.connect(_toggle_camera_label.bind(camera_button))
	row.add_child(camera_button)
	var full_button := _make_button("⛶", "全屏视口  Ctrl+Shift+F")
	full_button.custom_minimum_size.x = 42
	full_button.pressed.connect(_toggle_full_viewport)
	row.add_child(full_button)
	var settings_button := _make_button("⚙", "设置")
	settings_button.custom_minimum_size.x = 42
	settings_button.pressed.connect(_set_status.bind("设置面板将在后续里程碑接入"))
	row.add_child(settings_button)
	return panel


func _build_layers_panel() -> Control:
	var panel := PanelContainer.new()
	panel.add_theme_stylebox_override("panel", _flat_style(COLOR_PANEL, 0, COLOR_LINE, 0, 0, 0, 1))
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 0)
	panel.add_child(column)
	column.add_child(_panel_header("图层", "＋", add_data_requested.emit))
	var separator := HSeparator.new()
	column.add_child(separator)
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	column.add_child(scroll)
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 7)
	margin.add_theme_constant_override("margin_top", 7)
	margin.add_theme_constant_override("margin_right", 7)
	margin.add_theme_constant_override("margin_bottom", 12)
	margin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(margin)
	_layer_list = VBoxContainer.new()
	_layer_list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_layer_list.add_theme_constant_override("separation", 2)
	margin.add_child(_layer_list)
	return panel


func _build_center_panel() -> Control:
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 0)
	var viewer: Control = VIEWER_VIEWPORT_SCENE.instantiate()
	viewer.size_flags_vertical = Control.SIZE_EXPAND_FILL
	viewer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	column.add_child(viewer)

	_diagnostics = PanelContainer.new()
	_diagnostics.visible = false
	_diagnostics.custom_minimum_size.y = 148
	_diagnostics.add_theme_stylebox_override("panel", _flat_style(COLOR_PANEL_RAISED, 0, COLOR_LINE, 0, 1, 0, 0))
	column.add_child(_diagnostics)
	var diagnostic_margin := MarginContainer.new()
	diagnostic_margin.add_theme_constant_override("margin_left", 14)
	diagnostic_margin.add_theme_constant_override("margin_top", 10)
	diagnostic_margin.add_theme_constant_override("margin_right", 14)
	diagnostic_margin.add_theme_constant_override("margin_bottom", 10)
	_diagnostics.add_child(diagnostic_margin)
	var diagnostic_column := VBoxContainer.new()
	diagnostic_margin.add_child(diagnostic_column)
	var title := Label.new()
	title.text = "会话状态与诊断"
	title.add_theme_font_size_override("font_size", 15)
	diagnostic_column.add_child(title)
	var diagnostics_text := Label.new()
	diagnostics_text.text = "请求 6    下载 2    解析 1    Resident 412    可见瓦片 248    失败 0\nCPU 5.8 ms    GPU 8.3 ms    网络 8.4 MB/s    上传预算 4.0 ms"
	diagnostics_text.add_theme_color_override("font_color", COLOR_MUTED)
	diagnostics_text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	diagnostic_column.add_child(diagnostics_text)
	return column


func _build_properties_panel() -> Control:
	var panel := PanelContainer.new()
	panel.add_theme_stylebox_override("panel", _flat_style(COLOR_PANEL, 0, COLOR_LINE, 1, 0, 0, 0))
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 0)
	panel.add_child(column)
	var header := MarginContainer.new()
	header.custom_minimum_size.y = 58
	header.add_theme_constant_override("margin_left", 14)
	header.add_theme_constant_override("margin_top", 8)
	header.add_theme_constant_override("margin_right", 10)
	header.add_theme_constant_override("margin_bottom", 8)
	column.add_child(header)
	var header_row := HBoxContainer.new()
	header.add_child(header_row)
	_selected_title = Label.new()
	_selected_title.text = "图层属性"
	_selected_title.add_theme_font_size_override("font_size", 15)
	_selected_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_selected_title.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_selected_title.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	header_row.add_child(_selected_title)
	var more := _make_button("•••", "更多图层操作")
	more.custom_minimum_size.x = 42
	header_row.add_child(more)
	column.add_child(HSeparator.new())
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	column.add_child(scroll)
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 14)
	margin.add_theme_constant_override("margin_top", 12)
	margin.add_theme_constant_override("margin_right", 14)
	margin.add_theme_constant_override("margin_bottom", 16)
	margin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(margin)
	_property_content = VBoxContainer.new()
	_property_content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_property_content.add_theme_constant_override("separation", 8)
	margin.add_child(_property_content)
	return panel


func _build_statusbar() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.y = 38
	panel.add_theme_stylebox_override("panel", _flat_style(COLOR_PANEL_RAISED, 0, COLOR_LINE, 0, 1, 0, 0))
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 12)
	margin.add_theme_constant_override("margin_right", 8)
	panel.add_child(margin)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 16)
	margin.add_child(row)
	_status_label = Label.new()
	_status_label.text = "就绪"
	_status_label.add_theme_color_override("font_color", COLOR_ACCENT)
	_status_label.custom_minimum_size.x = 120
	row.add_child(_status_label)
	var request_label := Label.new()
	request_label.text = "请求 6"
	request_label.add_theme_color_override("font_color", COLOR_MUTED)
	row.add_child(request_label)
	_memory_label = Label.new()
	_memory_label.add_theme_color_override("font_color", COLOR_MUTED)
	row.add_child(_memory_label)
	var network_label := Label.new()
	network_label.text = "网络 8.4 MB/s"
	network_label.add_theme_color_override("font_color", COLOR_MUTED)
	row.add_child(network_label)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(spacer)
	var warnings := Button.new()
	warnings.text = "△ 2 条警告"
	warnings.flat = true
	warnings.add_theme_color_override("font_color", Color("e1aa4c"))
	warnings.pressed.connect(_toggle_diagnostics)
	row.add_child(warnings)
	var diagnostics_button := _make_button("状态 ▴", "展开状态与诊断")
	diagnostics_button.pressed.connect(_toggle_diagnostics)
	row.add_child(diagnostics_button)
	return panel


func _refresh_layers() -> void:
	for child: Node in _layer_list.get_children():
		child.queue_free()
	var current_group := ""
	for layer: Dictionary in AppState.layers:
		var group: String = layer.get("group", "其他")
		if group != current_group:
			current_group = group
			var group_label := Label.new()
			group_label.text = "⌄  %s" % group
			group_label.custom_minimum_size.y = 32
			group_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
			group_label.add_theme_font_size_override("font_size", 12)
			group_label.add_theme_color_override("font_color", COLOR_MUTED)
			_layer_list.add_child(group_label)
		var item: DigitalViewerLayerItem = LAYER_ITEM_SCENE.instantiate()
		item.setup(layer, layer["id"] == AppState.selected_layer_id)
		item.selected.connect(AppState.select_layer)
		item.layer_visibility_toggled.connect(AppState.set_layer_visible)
		_layer_list.add_child(item)


func _refresh_properties(layer_id: StringName) -> void:
	for child: Node in _property_content.get_children():
		child.queue_free()
	var layer := AppState.get_layer(layer_id)
	if layer.is_empty():
		_selected_title.text = "未选择图层"
		return
	_selected_title.text = str(layer.get("name", "图层属性"))
	_property_content.add_child(_section_label("数据源"))
	_property_content.add_child(_key_value("格式", str(layer.get("kind", "未知"))))
	_property_content.add_child(_key_value("坐标系", "WGS84 / ECEF" if layer_id in [&"globe", &"imagery", &"grid", &"terrain", &"buildings"] else "局部场景"))
	_property_content.add_child(_key_value("状态", str(layer.get("status", "")), COLOR_ACCENT))
	_property_content.add_child(_key_value("内存", "%.1f MB" % float(layer.get("memory_mb", 0.0))))
	_property_content.add_child(HSeparator.new())
	_property_content.add_child(_section_label("显示"))

	var visibility := CheckButton.new()
	visibility.text = "图层可见"
	visibility.button_pressed = layer.get("visible", true)
	visibility.toggled.connect(func(value: bool) -> void: AppState.set_layer_visible(layer_id, value))
	_property_content.add_child(visibility)
	var shadows := CheckButton.new()
	shadows.text = "投射阴影"
	shadows.button_pressed = true
	_property_content.add_child(shadows)
	_property_content.add_child(_slider_row("透明度", 0.0, 100.0, 100.0, "%"))

	if layer_id == &"terrain":
		_property_content.add_child(HSeparator.new())
		_property_content.add_child(_section_label("地形"))
		var exaggeration_row := VBoxContainer.new()
		var row := HBoxContainer.new()
		var label := Label.new()
		label.text = "地形夸张"
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(label)
		_terrain_value_label = Label.new()
		_terrain_value_label.text = "%.1f×" % AppState.terrain_exaggeration
		_terrain_value_label.add_theme_color_override("font_color", COLOR_ACCENT)
		row.add_child(_terrain_value_label)
		exaggeration_row.add_child(row)
		var slider := HSlider.new()
		slider.min_value = 0.0
		slider.max_value = 10.0
		slider.step = 0.1
		slider.value = AppState.terrain_exaggeration
		slider.value_changed.connect(_on_terrain_exaggeration_changed)
		exaggeration_row.add_child(slider)
		_property_content.add_child(exaggeration_row)

	_property_content.add_child(HSeparator.new())
	var focus_button := _make_button("⌖  聚焦图层", "将相机聚焦到图层范围")
	focus_button.pressed.connect(_set_status.bind("正在聚焦“%s”" % layer.get("name", "图层")))
	_property_content.add_child(focus_button)


func _on_selected_layer_changed(layer_id: StringName) -> void:
	_refresh_layers()
	_refresh_properties(layer_id)


func _on_grid_visibility_changed(visible: bool) -> void:
	_grid_button.set_pressed_no_signal(visible)


func _on_terrain_exaggeration_changed(value: float) -> void:
	_terrain_value_label.text = "%.1f×" % value
	AppState.set_terrain_exaggeration(value)


func _refresh_memory() -> void:
	_memory_label.text = "Resident %.0f MB / 1 GB" % AppState.total_memory_mb()


func _set_status(message: String) -> void:
	_status_label.text = message
	var tween := create_tween()
	tween.tween_interval(3.0)
	tween.tween_callback(func() -> void: _status_label.text = "就绪")


func _toggle_diagnostics() -> void:
	_diagnostics.visible = not _diagnostics.visible


func _toggle_full_viewport() -> void:
	_full_viewport = not _full_viewport
	_left_panel.visible = not _full_viewport
	_right_panel.visible = not _full_viewport
	_bottom_panel.visible = not _full_viewport
	_set_status("全屏视口" if _full_viewport else "已恢复工作台布局")


func _toggle_camera_label(button: Button) -> void:
	button.text = "飞行相机" if button.text == "轨道相机" else "轨道相机"
	_set_status("已切换到%s" % button.text)


func _make_button(text: String, tooltip: String) -> Button:
	var button := Button.new()
	button.text = text
	button.tooltip_text = tooltip
	button.custom_minimum_size.y = 38
	return button


func _make_logo_badge(side: int) -> PanelContainer:
	var badge := PanelContainer.new()
	badge.custom_minimum_size = Vector2(side, side)
	badge.add_theme_stylebox_override("panel", _flat_style(COLOR_ACCENT, 9))
	var label := Label.new()
	label.text = "DV"
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.add_theme_font_size_override("font_size", 11)
	label.add_theme_color_override("font_color", Color.WHITE)
	badge.add_child(label)
	return badge


func _style_primary_button(button: Button) -> void:
	var normal := _flat_style(Color("168d76"), 7)
	var hover := _flat_style(Color("20a489"), 7)
	button.add_theme_stylebox_override("normal", normal)
	button.add_theme_stylebox_override("hover", hover)
	button.add_theme_stylebox_override("pressed", _flat_style(Color("126e5e"), 7))


func _panel_header(title: String, action_text: String, callback: Callable) -> Control:
	var margin := MarginContainer.new()
	margin.custom_minimum_size.y = 52
	margin.add_theme_constant_override("margin_left", 14)
	margin.add_theme_constant_override("margin_right", 8)
	margin.add_theme_constant_override("margin_top", 6)
	margin.add_theme_constant_override("margin_bottom", 6)
	var row := HBoxContainer.new()
	margin.add_child(row)
	var label := Label.new()
	label.text = title
	label.add_theme_font_size_override("font_size", 15)
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	row.add_child(label)
	var action := _make_button(action_text, "添加数据")
	action.custom_minimum_size.x = 42
	action.pressed.connect(callback)
	row.add_child(action)
	return margin


func _section_label(text: String) -> Label:
	var label := Label.new()
	label.text = text.to_upper()
	label.add_theme_font_size_override("font_size", 11)
	label.add_theme_color_override("font_color", COLOR_MUTED)
	label.custom_minimum_size.y = 26
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	return label


func _key_value(key: String, value: String, value_color := Color.WHITE) -> Control:
	var row := HBoxContainer.new()
	row.custom_minimum_size.y = 27
	var key_label := Label.new()
	key_label.text = key
	key_label.custom_minimum_size.x = 82
	key_label.add_theme_color_override("font_color", COLOR_MUTED)
	row.add_child(key_label)
	var value_label := Label.new()
	value_label.text = value
	value_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	value_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	value_label.add_theme_color_override("font_color", value_color)
	row.add_child(value_label)
	return row


func _slider_row(label_text: String, minimum: float, maximum: float, value: float, suffix: String) -> Control:
	var column := VBoxContainer.new()
	var row := HBoxContainer.new()
	var label := Label.new()
	label.text = label_text
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(label)
	var value_label := Label.new()
	value_label.text = "%.0f%s" % [value, suffix]
	value_label.add_theme_color_override("font_color", COLOR_ACCENT)
	row.add_child(value_label)
	column.add_child(row)
	var slider := HSlider.new()
	slider.min_value = minimum
	slider.max_value = maximum
	slider.value = value
	slider.value_changed.connect(func(new_value: float) -> void: value_label.text = "%.0f%s" % [new_value, suffix])
	column.add_child(slider)
	return column


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
