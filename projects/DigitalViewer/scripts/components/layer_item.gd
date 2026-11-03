extends PanelContainer
class_name DigitalViewerLayerItem

signal selected(layer_id: StringName)
signal layer_visibility_toggled(layer_id: StringName, visible: bool)

const COLOR_ACCENT := Color("2cb79a")
const COLOR_ACCENT_SOFT := Color(0.17, 0.64, 0.54, 0.2)
const COLOR_PANEL := Color("121a20")
const COLOR_MUTED := Color("8d9aa0")
const COLOR_WARNING := Color("e1aa4c")

var layer_id: StringName
var _layer: Dictionary
var _selected := false
var _hovered := false
var _visibility_button: Button
var _name_label: Label
var _detail_label: Label
var _status_dot: ColorRect


func _ready() -> void:
	mouse_entered.connect(_set_hovered.bind(true))
	mouse_exited.connect(_set_hovered.bind(false))
	gui_input.connect(_on_gui_input)
	_build_ui()
	_refresh()


func setup(layer: Dictionary, is_selected: bool) -> void:
	_layer = layer
	layer_id = layer.get("id", &"")
	_selected = is_selected
	if is_node_ready():
		_refresh()


func set_selected(value: bool) -> void:
	_selected = value
	_refresh_style()


func _build_ui() -> void:
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 8)
	margin.add_theme_constant_override("margin_top", 5)
	margin.add_theme_constant_override("margin_right", 8)
	margin.add_theme_constant_override("margin_bottom", 5)
	add_child(margin)

	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	margin.add_child(row)

	_visibility_button = Button.new()
	_visibility_button.custom_minimum_size = Vector2(34, 34)
	_visibility_button.flat = true
	_visibility_button.tooltip_text = "显示或隐藏图层"
	_visibility_button.pressed.connect(_on_visibility_pressed)
	row.add_child(_visibility_button)

	var labels := VBoxContainer.new()
	labels.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	labels.add_theme_constant_override("separation", 1)
	row.add_child(labels)

	_name_label = Label.new()
	_name_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_name_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	labels.add_child(_name_label)

	_detail_label = Label.new()
	_detail_label.add_theme_font_size_override("font_size", 11)
	_detail_label.add_theme_color_override("font_color", COLOR_MUTED)
	_detail_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	labels.add_child(_detail_label)

	var dot_center := CenterContainer.new()
	dot_center.custom_minimum_size = Vector2(16, 0)
	row.add_child(dot_center)
	_status_dot = ColorRect.new()
	_status_dot.custom_minimum_size = Vector2(8, 8)
	_status_dot.mouse_filter = Control.MOUSE_FILTER_IGNORE
	dot_center.add_child(_status_dot)


func _refresh() -> void:
	if _layer.is_empty() or _name_label == null:
		return
	var visible: bool = _layer.get("visible", true)
	_name_label.text = str(_layer.get("name", "未命名图层"))
	_name_label.modulate = Color.WHITE if visible else Color(1, 1, 1, 0.45)
	_detail_label.text = "%s · %s" % [_layer.get("kind", "数据"), _layer.get("status", "")]
	_visibility_button.text = "◉" if visible else "○"
	_visibility_button.set_pressed_no_signal(visible)
	var progress: float = _layer.get("progress", 1.0)
	_status_dot.color = COLOR_ACCENT if progress >= 1.0 else COLOR_WARNING
	_status_dot.tooltip_text = "就绪" if progress >= 1.0 else str(_layer.get("status", "处理中"))
	_refresh_style()


func _refresh_style() -> void:
	var style := StyleBoxFlat.new()
	style.bg_color = COLOR_ACCENT_SOFT if _selected else (Color("182229") if _hovered else COLOR_PANEL)
	style.corner_radius_top_left = 8
	style.corner_radius_top_right = 8
	style.corner_radius_bottom_left = 8
	style.corner_radius_bottom_right = 8
	if _selected:
		style.border_width_left = 3
		style.border_color = COLOR_ACCENT
	add_theme_stylebox_override("panel", style)


func _on_gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
		selected.emit(layer_id)
		accept_event()
	elif event is InputEventScreenTouch and event.pressed:
		selected.emit(layer_id)
		accept_event()


func _on_visibility_pressed() -> void:
	var visible := not bool(_layer.get("visible", true))
	_layer["visible"] = visible
	_refresh()
	layer_visibility_toggled.emit(layer_id, visible)


func _set_hovered(value: bool) -> void:
	_hovered = value
	_refresh_style()
