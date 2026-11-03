extends ConfirmationDialog
class_name DigitalViewerFlyToDialog

var _longitude: SpinBox
var _latitude: SpinBox
var _height: SpinBox
var _heading: SpinBox
var _pitch: SpinBox


func _ready() -> void:
	confirmed.connect(_on_confirmed)
	_build_content()


func open_dialog() -> void:
	_longitude.value = AppState.current_longitude
	_latitude.value = AppState.current_latitude
	_height.value = AppState.current_height_km
	_heading.value = AppState.current_heading
	_pitch.value = AppState.current_pitch
	popup_centered(Vector2i(480, 390))


func _build_content() -> void:
	var root := VBoxContainer.new()
	root.add_theme_constant_override("separation", 10)
	add_child(root)

	var hint := Label.new()
	hint.text = "输入 WGS84 经纬度与观察参数。经度会自动规范到 −180°…180°。"
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	hint.add_theme_color_override("font_color", Color("9cabb1"))
	root.add_child(hint)

	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", 14)
	grid.add_theme_constant_override("v_separation", 10)
	root.add_child(grid)

	_longitude = _add_spin_row(grid, "经度", -180.0, 180.0, 0.0001, "°")
	_latitude = _add_spin_row(grid, "纬度", -90.0, 90.0, 0.0001, "°")
	_height = _add_spin_row(grid, "观察高度", 0.01, 100000.0, 0.01, " km")
	_heading = _add_spin_row(grid, "航向角", -360.0, 360.0, 0.1, "°")
	_pitch = _add_spin_row(grid, "俯仰角", -90.0, 90.0, 0.1, "°")


func _add_spin_row(parent: GridContainer, label_text: String, minimum: float, maximum: float, step: float, suffix: String) -> SpinBox:
	var label := Label.new()
	label.text = label_text
	label.custom_minimum_size.x = 110
	parent.add_child(label)
	var spin := SpinBox.new()
	spin.min_value = minimum
	spin.max_value = maximum
	spin.step = step
	spin.suffix = suffix
	spin.allow_greater = false
	spin.allow_lesser = false
	spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(spin)
	return spin


func _on_confirmed() -> void:
	AppState.request_fly_to(_longitude.value, _latitude.value, _height.value, _heading.value, _pitch.value)
