extends Control
class_name DigitalViewerViewport

const COLOR_ACCENT := Color("2cb79a")
const COLOR_VIEWPORT := Color("07131a")

var _subviewport: SubViewport
var _camera: Camera3D
var _earth_pivot: Node3D
var _grid_mesh_instance: MeshInstance3D
var _coordinate_label: Label
var _dragging := false
var _last_pointer := Vector2.ZERO
var _runtime_objects: Dictionary = {}
var _earth_content: Node3D
var _render_buttons: Array[Button] = []


func _ready() -> void:
	_build_viewport()
	_build_overlay()
	set_process(true)
	AppState.grid_visibility_changed.connect(_on_grid_visibility_changed)
	AppState.layers_changed.connect(_sync_layer_visibility)
	AppState.runtime.scene_changed.connect(_sync_runtime)
	_sync_runtime()
	_sync_layer_visibility()
	_update_coordinate_label()


func _process(_delta: float) -> void:
	if is_visible_in_tree():
		_subviewport.size = Vector2i(maxi(2, int(size.x)), maxi(2, int(size.y)))
	_update_coordinate_label()


func _build_viewport() -> void:
	var live: Node = AppState.live_scene
	_subviewport = live._subviewport
	_runtime_objects = live._runtime_objects
	_refresh_references()
	var display := TextureRect.new()
	display.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	display.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	display.stretch_mode = TextureRect.STRETCH_SCALE
	display.texture = _subviewport.get_texture()
	display.mouse_filter = Control.MOUSE_FILTER_STOP
	display.gui_input.connect(_on_viewport_input)
	add_child(display)
	live.changed.connect(_refresh_references)


func _refresh_references() -> void:
	var live: Node = AppState.live_scene
	_camera = live._camera
	_earth_pivot = live._earth_pivot
	_earth_content = live._earth_content
	_grid_mesh_instance = live._grid_mesh_instance


func _sync_runtime() -> void:
	for button: Button in _render_buttons:
		button.set_pressed_no_signal(button.text == ("材质" if AppState.runtime.objects.globe.props["material.shading_mode"] == 1 else "无光照"))


func _build_overlay() -> void:
	var top_left := HBoxContainer.new()
	top_left.position = Vector2(12, 12)
	top_left.add_theme_constant_override("separation", 4)
	add_child(top_left)
	for mode: String in ["材质", "无光照"]:
		var button := Button.new()
		button.text = mode
		button.toggle_mode = true
		button.button_pressed = mode == "材质"
		button.custom_minimum_size.y = 34
		button.pressed.connect(_on_render_mode_pressed.bind(button, top_left))
		top_left.add_child(button)
		_render_buttons.append(button)

	var compass := PanelContainer.new()
	compass.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	compass.position = Vector2(-62, 12)
	compass.custom_minimum_size = Vector2(50, 50)
	add_child(compass)
	var compass_label := Label.new()
	compass_label.text = "N\n·"
	compass_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	compass_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	compass_label.add_theme_color_override("font_color", Color("ff8d8d"))
	compass.add_child(compass_label)

	var hud := PanelContainer.new()
	hud.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	hud.position = Vector2(12, -48)
	add_child(hud)
	_coordinate_label = Label.new()
	_coordinate_label.add_theme_font_size_override("font_size", 12)
	_coordinate_label.add_theme_constant_override("outline_size", 4)
	_coordinate_label.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.45))
	_coordinate_label.custom_minimum_size = Vector2(310, 34)
	_coordinate_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_coordinate_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	hud.add_child(_coordinate_label)

	var hint := Label.new()
	hint.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	hint.position = Vector2(-242, -42)
	hint.text = "拖动旋转 · 滚轮/双指缩放"
	hint.add_theme_font_size_override("font_size", 11)
	hint.add_theme_color_override("font_color", Color("9aabb2"))
	add_child(hint)
	hint.visible = false


func _on_viewport_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			_dragging = event.pressed
			_last_pointer = event.position
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP:
			AppState.zoom(-0.35)
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			AppState.zoom(0.35)
	elif event is InputEventMouseMotion and _dragging:
		var pointer_delta: Vector2 = event.position - _last_pointer
		_last_pointer = event.position
		AppState.orbit(pointer_delta * 0.007)
	elif event is InputEventMagnifyGesture:
		AppState.zoom((1.0 - event.factor) * 2.0)
	elif event is InputEventScreenDrag:
		AppState.orbit(event.relative * 0.006)


func _on_render_mode_pressed(pressed_button: Button, group: HBoxContainer) -> void:
	if AppState.live_scene.native_owned:
		AppState.status_changed.emit("原生脚本已接管场景；请通过脚本改材质，或在工作台重置场景。")
		return
	for child: Node in group.get_children():
		if child is Button:
			child.set_pressed_no_signal(child == pressed_button)
	AppState.runtime.external_set("globe", "material.shading_mode", 1 if pressed_button.text == "材质" else 0)
	AppState.status_changed.emit("显示模式：%s" % pressed_button.text)


func _on_grid_visibility_changed(visible: bool) -> void:
	if not AppState.live_scene.native_owned:
		_grid_mesh_instance.visible = visible


func _sync_layer_visibility() -> void:
	if AppState.live_scene.native_owned:
		return
	_runtime_objects.globe.visible = AppState.get_layer(&"globe").get("visible", true)
	_grid_mesh_instance.visible = AppState.grid_visible


func _update_coordinate_label() -> void:
	if _coordinate_label == null:
		return
	_coordinate_label.text = "经度 %.3f°  纬度 %.3f°  预览高度 %.2f km" % [AppState.current_longitude, AppState.current_latitude, AppState.current_height_km]
