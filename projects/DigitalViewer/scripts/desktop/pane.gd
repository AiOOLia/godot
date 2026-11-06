extends Control
signal action(id: String, command: String)
const Ui = preload("res://scripts/desktop/ui_widgets.gd")
const Profile = preload("res://scripts/desktop/build_profile.gd")
var view_id := ""
var display: TextureRect
var backdrop: TextureRect
var tree: Tree
var _header: HBoxContainer
var _title: Label
var _empty: VBoxContainer
var _empty_button: Button
var _display_button: Button
var _display_popup: PanelContainer
var _display_scrim: ColorRect
var _display_modes: HBoxContainer
var _display_buttons: Array[Button] = []
var _display_lighting: CheckBox
var _display_shadow: CheckBox
var _left_buttons: Array[Button] = []
var _right_buttons: Array[Button] = []
var _drag := false
var _moved := false
var _pan := false
var _refreshing := false
var _restoring_selection := false
var _items: Dictionary = {}
var _layer_root_keys: Dictionary = {}
var _selected_keys: Array[String] = []
var _suppress_selection_sync := false
var _pixel_scale := Vector2.ONE

func _ready() -> void:
	add_to_group(&"digitalviewer_panes")
	clip_contents = true
	# The pane is a layout container. Let its interactive children (display/tree/header)
	# receive input, and let the display receive clicks through empty tree overlay space.
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	size_flags_horizontal = Control.SIZE_EXPAND_FILL
	size_flags_vertical = Control.SIZE_EXPAND_FILL
	focus_mode = Control.FOCUS_ALL
	_header = HBoxContainer.new()
	_header.add_theme_constant_override("separation", 2)
	add_child(_header)
	_left_buttons.append(Ui.icon_button(_header, "layers", tr("Layers"), func(): tree.visible = not tree.visible, false, 24, 13))
	_left_buttons.append(Ui.icon_button(_header, "plus", tr("Add data"), func(): action.emit(view_id, "add"), false, 24, 13))
	_display_button = Ui.icon_button(_header, "display", tr("Display"), func(): _open_display_popup(), false, 24, 13)
	_left_buttons.append(_display_button)
	_left_buttons.append(Ui.icon_button(_header, "camera", tr("Screenshot"), func(): action.emit(view_id, "screenshot"), false, 24, 13))
	_left_buttons.append(Ui.icon_button(_header, "record", tr("Record"), func(): action.emit(view_id, "record"), false, 24, 13))
	_title = Label.new()
	_title.text = view_id.replace("view_", tr("View "))
	_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_title.add_theme_font_size_override("font_size", 11)
	_title.add_theme_color_override("font_color", Ui.TEXT_2)
	_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_title.mouse_filter = Control.MOUSE_FILTER_STOP
	_title.gui_input.connect(_title_input)
	_header.add_child(_title)
	_right_buttons.append(Ui.icon_button(_header, "split-h", tr("Split horizontally"), func(): action.emit(view_id, "split_h"), false, 24, 13))
	_right_buttons.append(Ui.icon_button(_header, "split-v", tr("Split vertically"), func(): action.emit(view_id, "split_v"), false, 24, 13))
	_right_buttons.append(Ui.icon_button(_header, "popout", tr("Detach / dock"), func(): action.emit(view_id, "detach"), false, 24, 13))
	_right_buttons.append(Ui.icon_button(_header, "fullscreen", tr("Fullscreen viewport"), func(): action.emit(view_id, "fullscreen"), false, 24, 13))
	_right_buttons.append(Ui.icon_button(_header, "close", tr("Close viewport"), func(): action.emit(view_id, "close"), false, 24, 13))
	backdrop = TextureRect.new()
	backdrop.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	backdrop.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
	backdrop.mouse_filter = Control.MOUSE_FILTER_IGNORE
	backdrop.hide()
	add_child(backdrop)
	display = TextureRect.new()
	display.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	display.stretch_mode = TextureRect.STRETCH_SCALE
	display.texture = AppState.views[view_id].viewport.get_texture()
	display.mouse_filter = Control.MOUSE_FILTER_STOP
	display.gui_input.connect(_input_view)
	display.set_drag_forwarding(Callable(), _can_drop, _drop)
	add_child(display)
	AppState.views[view_id].backdrop_changed.connect(_sync_backdrop)
	_sync_backdrop()
	tree = preload("res://scripts/desktop/viewport_tree.gd").new()
	tree.hide_root = true
	tree.columns = 2
	tree.select_mode = Tree.SELECT_MULTI
	tree.set_column_clip_content(0, true)
	tree.set_column_expand(1, false)
	tree.set_column_custom_minimum_width(1, 28)
	tree.add_theme_stylebox_override("panel", StyleBoxEmpty.new())
	tree.item_selected.connect(_selected)
	tree.multi_selected.connect(_multi_selected)
	tree.item_edited.connect(_edited)
	tree.set_drag_forwarding(_get_drag, _can_drop, _drop)
	add_child(tree)
	_empty = VBoxContainer.new()
	_empty.alignment = BoxContainer.ALIGNMENT_CENTER
	_empty.add_theme_constant_override("separation", 9)
	_empty.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_empty)
	var badge_center := CenterContainer.new()
	badge_center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_empty.add_child(badge_center)
	var badge := PanelContainer.new()
	badge.custom_minimum_size = Vector2(42, 42)
	badge.add_theme_stylebox_override("panel", Ui.box(Ui.RAISED, Color.TRANSPARENT, 12, 0, Vector4(0, 0, 0, 0)))
	badge_center.add_child(badge)
	var globe := TextureRect.new()
	globe.texture = Ui.icon("globe" if Profile.enabled("gis") else "layers")
	globe.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	globe.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	globe.custom_minimum_size = Vector2(21, 21)
	globe.modulate = Ui.TEXT_3
	globe.mouse_filter = Control.MOUSE_FILTER_IGNORE
	badge.add_child(globe)
	var empty_title := Label.new()
	empty_title.text = tr("Empty viewport")
	empty_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	empty_title.add_theme_font_size_override("font_size", 13)
	empty_title.add_theme_color_override("font_color", Ui.TEXT_2)
	empty_title.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_empty.add_child(empty_title)
	var empty_body := Label.new()
	empty_body.text = tr("Add a WGS84 ellipsoid as the base globe, or add imagery, terrain, and 3D data.") if Profile.enabled("gis") else tr("Add a glTF/GLB model, ZIP archive, or GDScript script.")
	empty_body.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	empty_body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	empty_body.custom_minimum_size = Vector2(340, 0)
	empty_body.add_theme_font_size_override("font_size", 11)
	empty_body.add_theme_color_override("font_color", Ui.TEXT_3)
	empty_body.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_empty.add_child(empty_body)
	var add_center := CenterContainer.new()
	add_center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_empty.add_child(add_center)
	_empty_button = Button.new()
	_empty_button.text = tr("Add data")
	_empty_button.icon = Ui.icon("plus")
	Ui.set_icon_size(_empty_button, 15)
	_empty_button.expand_icon = true
	_empty_button.custom_minimum_size = Vector2(108, 30)
	_empty_button.add_theme_font_size_override("font_size", 12)
	_empty_button.add_theme_constant_override("h_separation", 5)
	_empty_button.add_theme_stylebox_override("normal", Ui.box(Ui.ACCENT, Color.TRANSPARENT, 7, 0, Vector4(10, 5, 12, 5)))
	_empty_button.add_theme_stylebox_override("hover", Ui.box(Ui.ACCENT_HOVER, Color.TRANSPARENT, 7, 0, Vector4(10, 5, 12, 5)))
	_empty_button.add_theme_stylebox_override("pressed", Ui.box(Ui.ACCENT_PRESSED, Color.TRANSPARENT, 7, 0, Vector4(10, 5, 12, 5)))
	_empty_button.add_theme_stylebox_override("focus", StyleBoxEmpty.new())
	_empty_button.add_theme_color_override("font_color", Ui.ON_ACCENT)
	_empty_button.add_theme_color_override("font_hover_color", Ui.ON_ACCENT)
	_empty_button.add_theme_color_override("icon_normal_color", Ui.ON_ACCENT)
	_empty_button.add_theme_color_override("icon_hover_color", Ui.ON_ACCENT)
	_empty_button.pressed.connect(func(): action.emit(view_id, "add"))
	add_center.add_child(_empty_button)
	_build_display_popup()
	AppState.changed.connect(_schedule_refresh)
	AppState.selection_changed.connect(func():
		_sync_selection()
		_layout()
		queue_redraw())
	resized.connect(_layout)
	refresh()
	_layout()

func _build_display_popup() -> void:
	_display_scrim = ColorRect.new()
	_display_scrim.color = Color.TRANSPARENT
	_display_scrim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_display_scrim.mouse_filter = Control.MOUSE_FILTER_STOP
	_display_scrim.hide()
	_display_scrim.gui_input.connect(func(event: InputEvent):
		if event is InputEventMouseButton and event.pressed:
			_close_display_popup())
	add_child(_display_scrim)
	_display_popup = PanelContainer.new()
	_display_popup.add_theme_stylebox_override("panel", Ui.box(Ui.RAISED, Ui.LINE, 10, 1, Vector4(12, 10, 12, 10)))
	_display_popup.custom_minimum_size = Vector2(252, 0)
	_display_popup.mouse_filter = Control.MOUSE_FILTER_STOP
	_display_popup.hide()
	add_child(_display_popup)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 9)
	_display_popup.add_child(box)
	var head := HBoxContainer.new()
	box.add_child(head)
	var title := Label.new()
	title.text = tr("Display")
	title.add_theme_font_size_override("font_size", 12)
	title.add_theme_color_override("font_color", Ui.TEXT)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(title)
	var segment := PanelContainer.new()
	segment.add_theme_stylebox_override("panel", Ui.box(Ui.BG, Color.TRANSPARENT, 7, 0, Vector4(2, 2, 2, 2)))
	box.add_child(segment)
	_display_modes = HBoxContainer.new()
	_display_modes.add_theme_constant_override("separation", 2)
	segment.add_child(_display_modes)
	for index in 2:
		var label: String = ["Textured", "Wireframe"][index]
		var button := Button.new()
		button.text = tr(label)
		button.custom_minimum_size = Vector2(96, 27)
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		button.add_theme_font_size_override("font_size", 11)
		button.pressed.connect(_set_display_mode.bind(index))
		Ui.style_button(button, false, true)
		_display_modes.add_child(button)
		_display_buttons.append(button)
	var toggles := HBoxContainer.new()
	toggles.add_theme_constant_override("separation", 16)
	box.add_child(toggles)
	_display_lighting = CheckBox.new()
	_display_lighting.text = tr("Lighting")
	_display_lighting.add_theme_font_size_override("font_size", 11)
	_display_lighting.add_theme_color_override("font_color", Ui.TEXT_2)
	_display_lighting.toggled.connect(func(value: bool):
		if AppState.views.has(view_id) and is_instance_valid(AppState.views[view_id].light):
			AppState.views[view_id].light.visible = value
			AppState.views[view_id].refresh_lighting()
			_refresh_display_toggles())
	toggles.add_child(_display_lighting)
	_display_shadow = CheckBox.new()
	_display_shadow.text = tr("Shadows")
	_display_shadow.add_theme_font_size_override("font_size", 11)
	_display_shadow.add_theme_color_override("font_color", Ui.TEXT_2)
	_display_shadow.toggled.connect(func(value: bool):
		if AppState.views.has(view_id) and is_instance_valid(AppState.views[view_id].light): AppState.views[view_id].light.shadow_enabled = value)
	toggles.add_child(_display_shadow)

func _refresh_display_toggles() -> void:
	if not AppState.views.has(view_id): return
	var view: Node = AppState.views[view_id]
	var has_light: bool = is_instance_valid(view.light)
	_display_lighting.set_block_signals(true)
	_display_shadow.set_block_signals(true)
	_display_lighting.button_pressed = has_light and view.light.visible
	_display_shadow.button_pressed = has_light and view.light.shadow_enabled
	_display_lighting.disabled = not has_light
	_display_shadow.disabled = not has_light or not view.light.visible
	_display_lighting.set_block_signals(false)
	_display_shadow.set_block_signals(false)

func _open_display_popup() -> void:
	if not AppState.views.has(view_id): return
	if _display_popup.visible:
		_close_display_popup()
		return
	_refresh_display_popup()
	var pos: Vector2 = get_global_transform_with_canvas().affine_inverse() * _display_button.global_position
	pos += Vector2(0, _display_button.size.y + 4)
	pos.x = clampf(pos.x, 6, maxf(6, size.x - 258))
	pos.y = clampf(pos.y, 36, maxf(36, size.y - 144))
	_display_popup.position = pos
	_display_popup.size = Vector2(252, 0)
	_display_scrim.show()
	_display_popup.show()

func _close_display_popup() -> void:
	_display_popup.hide()
	_display_scrim.hide()

func _refresh_display_popup() -> void:
	if not AppState.views.has(view_id): return
	var view: Node = AppState.views[view_id]
	var wireframe: bool = view.render_mode == 2
	for index in _display_buttons.size():
		Ui.style_button(_display_buttons[index], (index == 1) == wireframe, true)
	_refresh_display_toggles()

func _set_display_mode(mode: int) -> void:
	if not AppState.views.has(view_id): return
	AppState.views[view_id].set_render_mode(2 if mode == 1 else 0)
	_refresh_display_popup()

func _schedule_refresh() -> void:
	if _refreshing: return
	_refreshing = true
	refresh.call_deferred()

func refresh() -> void:
	_refreshing = false
	if AppState.shutting_down or not is_inside_tree() or not AppState.views.has(view_id): return
	var view: Node = AppState.views[view_id]
	_title.text = view_id.replace("view_", tr("View "))
	var collapsed := {}
	for key in _items: collapsed[key] = _items[key].collapsed
	var selected_keys := _selected_keys.duplicate()
	if selected_keys.is_empty() and tree.get_selected():
		selected_keys.append(tree.get_selected().get_meta("key", ""))
	_restoring_selection = true
	_items.clear()
	_layer_root_keys.clear()
	tree.clear()
	var root := tree.create_item()
	var camera := _item(root, "camera")
	camera.set_text(0, tr("Camera"))
	camera.set_metadata(0, "camera")
	var groups := {}
	for layer in view.layers:
		var group := "3D data"
		if layer.kind == "ellipsoid": group = "Basemap"
		elif layer.kind == "script": group = "Scripts"
		if not groups.has(group):
			groups[group] = _item(root, "group:" + group)
			groups[group].set_text(0, tr(group))
			groups[group].set_metadata(0, "group:" + group)
			groups[group].set_cell_mode(1, TreeItem.CELL_MODE_CHECK)
			groups[group].set_editable(1, true)
		if layer.kind == "brep" and is_instance_valid(layer.node) and layer.node.get_child_count() > 0:
			# The layer and import container own runtime state, not source hierarchy.
			var budget := [2048]
			var keys: Array[String] = []
			for source_root in layer.node.get_children():
				var source_item := _script_node(source_root, groups[group], layer.id, 0, budget, 128)
				if source_item == null: break
				_layer_visibility(source_item, layer)
				source_item.set_tooltip_text(0, layer.name + ("\n" + layer.error if not layer.error.is_empty() else ""))
				keys.append(source_item.get_meta("key"))
			_layer_root_keys[layer.id] = keys
			if budget[0] <= 0:
				var limit := tree.create_item(groups[group])
				limit.set_text(0, "… (assembly tree preview limit: 2048 nodes)")
			continue
		var item := _item(groups[group], "layer:" + layer.id)
		# Keep the name cell read-only; only the separate checkbox changes visibility.
		_layer_visibility(item, layer)
		item.set_text(0, tr(layer.name))
		item.set_tooltip_text(0, layer.error)
		item.set_metadata(0, layer.id)
		if layer.kind == "script":
			var script_id: String = AppState.store.records[layer.data_id].get("script_id", "")
			var doc: Dictionary = AppState.scripts.documents.get(script_id, {})
			var budget := [128]
			if not doc.is_empty() and is_instance_valid(doc.runner._session): _script_node(doc.runner._session, item, layer.id, 0, budget)
			if is_instance_valid(layer.node): _script_node(layer.node, item, layer.id, 0, budget)
	# Status updates and script node changes must not reset navigation in this tree.
	for key in _items:
		_items[key].collapsed = collapsed.get(key, false)
	for group_key in groups:
		var all_visible := true
		for child in groups[group_key].get_children():
			if child.get_metadata(0) != null and not child.is_checked(1):
				all_visible = false
				break
		groups[group_key].set_checked(1, all_visible)
	if selected_keys.is_empty():
		_sync_selection()
	else:
		_restore_selected_keys(selected_keys)
		if _selected_keys.is_empty(): _sync_selection()
	_empty.visible = view.layers.is_empty()
	tree.visible = not view.layers.is_empty()
	_layout()
	queue_redraw()

func _layer_visibility(item: TreeItem, layer: Dictionary) -> void:
	item.set_cell_mode(1, TreeItem.CELL_MODE_CHECK)
	item.set_editable(1, true)
	item.set_checked(1, layer.visible)
	item.set_tooltip_text(1, tr("Visible"))

func _item(parent: TreeItem, key: String) -> TreeItem:
	var item := tree.create_item(parent)
	item.set_meta("key", key)
	_items[key] = item
	item.set_selectable(1, false)
	return item

func _sync_selection(group_key := "") -> void:
	if _suppress_selection_sync: return
	if not is_instance_valid(tree) or not AppState.views.has(view_id): return
	var view: Node = AppState.views[view_id]
	var keys: Array[String] = []
	if not group_key.is_empty(): keys.append(group_key)
	else:
		match view.selection_kind:
			"camera": keys.append("camera")
			"layer": keys.assign(_layer_root_keys.get(view.selected, ["layer:" + view.selected]))
			"node":
				var node: Node = view.selected_node.get_ref() if view.selected_node else null
				if is_instance_valid(node): keys.append("node:%s:%d" % [view.selected, node.get_instance_id()])
				else: keys.assign(_layer_root_keys.get(view.selected, ["layer:" + view.selected]))
	var reveal: bool = AppState.active_id == view_id and (not view.selected_face.is_empty() or _selected_keys != keys)
	_restoring_selection = true
	tree.deselect_all()
	_selected_keys.clear()
	for key in keys:
		if not _items.has(key): continue
		if reveal:
			var ancestor: TreeItem = _items[key].get_parent()
			while ancestor != null:
				ancestor.collapsed = false
				ancestor = ancestor.get_parent()
		_items[key].select(0, _selected_keys.is_empty())
		_selected_keys.append(key)
	if reveal and not _selected_keys.is_empty(): tree.scroll_to_item(_items[_selected_keys[0]])
	_restoring_selection = false

func _restore_selected_keys(keys: Array[String]) -> void:
	_restoring_selection = true
	tree.deselect_all()
	_selected_keys.clear()
	for key in keys:
		if _items.has(key):
			_items[key].select(0, _selected_keys.is_empty())
			_selected_keys.append(key)
	_restoring_selection = false

func _script_node(node: Node, parent: TreeItem, layer_id: String, depth: int, budget: Array, max_depth := 12) -> TreeItem:
	if depth > max_depth or budget[0] <= 0: return null
	budget[0] -= 1
	var item := _item(parent, "node:%s:%d" % [layer_id, node.get_instance_id()])
	item.set_text(0, str(node.name))
	item.set_metadata(0, layer_id)
	item.set_meta("node", weakref(node))
	for child in node.get_children(): _script_node(child, item, layer_id, depth + 1, budget, max_depth)
	return item

func _sync_backdrop() -> void:
	if not AppState.views.has(view_id) or not is_instance_valid(backdrop): return
	var view: Node = AppState.views[view_id]
	backdrop.texture = view.flat_texture
	backdrop.visible = view.background_mode == 3 and view.flat_texture != null

func _layout() -> void:
	if not is_inside_tree() or not AppState.views.has(view_id): return
	if not is_instance_valid(display) or not display.is_inside_tree(): return
	_header.position = Vector2(5, 2)
	_header.size = Vector2(maxf(0, size.x - 10), 24)
	var compact := size.x < 390
	for index in _left_buttons.size():
		_left_buttons[index].visible = not compact or index < 3
	for index in _right_buttons.size():
		_right_buttons[index].visible = not compact or index >= 2
	_title.visible = size.x >= 170
	display.position = Vector2(1, 28)
	display.size = Vector2(maxf(1, size.x - 2), maxf(1, size.y - 29))
	backdrop.position = display.position
	backdrop.size = display.size
	tree.position = Vector2(10, 35)
	tree.size = Vector2(minf(300, maxf(0, size.x - 20)), maxf(0, size.y - tree.position.y - 10))
	_empty.position = Vector2(maxf(0, size.x / 2 - 170), maxf(40, size.y / 2 - 76))
	_empty.size = Vector2(340, 152)
	var has_content := false
	if is_instance_valid(AppState.views[view_id].root):
		for child in AppState.views[view_id].root.get_children():
			if child.has_meta("digitalviewer_script_owned"): has_content = true
	_empty.visible = AppState.views[view_id].layers.is_empty() and not has_content and size.x > 220 and size.y > 160
	if AppState.views.has(view_id):
		var scale_factor := 1.0 if AppState.active_id == view_id else minf(1.0, 2.0 / sqrt(AppState.views.size()))
		_pixel_scale = display.get_screen_transform().get_scale().abs()
		var pixels := display.size * _pixel_scale * scale_factor
		AppState.views[view_id].set_render_size(Vector2i(maxi(2, roundi(pixels.x)), maxi(2, roundi(pixels.y))))

func _process(delta: float) -> void:
	if is_instance_valid(display) and not _pixel_scale.is_equal_approx(display.get_screen_transform().get_scale().abs()): _layout()
	if not has_focus() or not AppState.views.has(view_id): return
	var view: Node = AppState.views[view_id]
	if view.camera_mode != 1: return
	var movement := Vector3(float(Input.is_physical_key_pressed(KEY_D)) - float(Input.is_physical_key_pressed(KEY_A)), float(Input.is_physical_key_pressed(KEY_E)) - float(Input.is_physical_key_pressed(KEY_Q)), float(Input.is_physical_key_pressed(KEY_S)) - float(Input.is_physical_key_pressed(KEY_W)))
	if movement.length_squared() > 0: view.move_flight(movement.normalized(), delta, Input.is_physical_key_pressed(KEY_SHIFT))

func _draw() -> void:
	draw_rect(Rect2(0, 0, size.x, 28), Ui.SURFACE)
	draw_line(Vector2(0, 27.5), Vector2(size.x, 27.5), Ui.LINE, 1.0)
	draw_rect(Rect2(Vector2.ZERO, size), Ui.LINE if AppState.active_id != view_id else Ui.LINE_STRONG, false, 1.0)

func _title_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		AppState.activate(view_id, "viewport", "", null, true)
		grab_focus()

func _selected() -> void:
	if _restoring_selection: return
	var item := tree.get_selected()
	if item == null: return
	_selected_keys = [str(item.get_meta("key", ""))]
	_activate_tree_item(item)

func _multi_selected(item: TreeItem, _column: int, selected: bool) -> void:
	if _restoring_selection or item == null: return
	var key := str(item.get_meta("key", ""))
	if key.is_empty(): return
	if selected:
		if not _selected_keys.has(key): _selected_keys.append(key)
		_suppress_selection_sync = true
		_activate_tree_item(item)
		_suppress_selection_sync = false
	else:
		_selected_keys.erase(key)
		_suppress_selection_sync = true
		if _selected_keys.is_empty(): AppState.activate(view_id)
		elif _items.has(_selected_keys.back()): _activate_tree_item(_items[_selected_keys.back()])
		_suppress_selection_sync = false
	_highlight_selected_items()

func _highlight_selected_items() -> void:
	var targets: Array[Node] = []
	for key in _selected_keys:
		if not _items.has(key): continue
		var item: TreeItem = _items[key]
		var node: Node = item.get_meta("node").get_ref() if item.has_meta("node") else null
		if is_instance_valid(node): targets.append(node)
		elif key.begins_with("layer:"):
			var layer: Dictionary = AppState.views[view_id].get_layer(str(item.get_metadata(0)))
			if layer.get("kind") == "brep" and is_instance_valid(layer.node): targets.append(layer.node)
	AppState.views[view_id].highlight_selection(targets)

func _activate_tree_item(item: TreeItem) -> void:
	var key := str(item.get_meta("key", ""))
	if key.begins_with("group:"): return
	var id: Variant = item.get_metadata(0)
	if id == null: return
	var node: Node = item.get_meta("node").get_ref() if item.has_meta("node") else null
	AppState.activate(view_id, "node" if is_instance_valid(node) else "camera" if id == "camera" else "layer", str(id), node)

func _edited() -> void:
	var item := tree.get_edited()
	if item == null or tree.get_edited_column() != 1: return
	var key := str(item.get_meta("key", ""))
	if key.begins_with("group:"):
		var value := item.is_checked(1)
		for child in item.get_children():
			var child_id: Variant = child.get_metadata(0)
			if child_id != null and not str(child_id).begins_with("group:"):
				AppState.set_visible(view_id, str(child_id), value)
		return
	if item.get_metadata(0) != null:
		AppState.set_visible(view_id, str(item.get_metadata(0)), item.is_checked(1))

func _focus_view_only() -> void:
	if AppState.active_id == view_id: return
	AppState.active_id = view_id
	for candidate in get_tree().get_nodes_in_group(&"digitalviewer_panes"):
		candidate.queue_redraw()

func _input_view(event: InputEvent) -> void:
	var view: Node = AppState.views[view_id]
	if event is InputEventMouseButton:
		if event.pressed:
			_focus_view_only()
			grab_focus()
		if event.button_index in [MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE]:
			if not event.pressed and _drag and not _moved and event.button_index == MOUSE_BUTTON_LEFT:
				var hit: Dictionary = view.pick(event.position * Vector2(view.viewport.size) / display.size)
				AppState.select_hit(view_id, hit)
				if not hit.is_empty():
					AppState.status_changed.emit(AppState.redact(str(hit)))
			_drag = event.pressed
			_pan = event.button_index != MOUSE_BUTTON_LEFT or event.shift_pressed
			_moved = false
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP: view.zoom(0.85)
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN: view.zoom(1.15)
	elif event is InputEventMouseMotion and _drag:
		_moved = true
		if _pan: view.pan(event.relative)
		else: view.orbit(event.relative * 0.006)
	elif event is InputEventMagnifyGesture: view.zoom(1.0 / event.factor)

func _get_drag(at: Vector2) -> Variant:
	if tree.get_column_at_position(at) != 0: return null
	var item := tree.get_item_at_position(at)
	if item == null: return null
	var id: Variant = item.get_metadata(0)
	if id == null or id == "camera": return null
	var preview := Label.new()
	preview.text = item.get_text(0)
	tree.set_drag_preview(preview)
	return {"digitalviewer_layer": str(id), "viewport": view_id}

func _can_drop(_at: Vector2, data: Variant) -> bool:
	return data is Dictionary and data.has("digitalviewer_layer") and data.get("viewport", view_id) != view_id

func _drop(_at: Vector2, data: Variant) -> void:
	if _can_drop(_at, data): AppState.share_layer(data.viewport, data.digitalviewer_layer, view_id)
