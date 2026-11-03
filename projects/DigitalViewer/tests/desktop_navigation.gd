extends Node

func run(test: Node, shell: Control, first: String, osg_path: String) -> void:
	if DisplayServer.get_name() == "headless": return
	var saved_pose: Dictionary = AppState.views[first].pose()
	var second: String = shell.split_view(first, false)
	var layers := {}
	for id in [first, second]:
		layers[id] = AppState.add_source(id, osg_path, "auto", {"z_up": false})
		await test._wait_layer(id, layers[id])
		var layer: Dictionary = AppState.views[id].get_layer(layers[id])
		for frame in 180:
			await get_tree().process_frame
			if is_instance_valid(layer.node) and layer.node.call("get_streaming_stats").get("resident_contents", 0) > 0: break
		test.check(layer.status == "Ready" and layer.node.call("get_streaming_stats").get("resident_contents", 0) > 0, "navigation fixture loads actual OSG geometry in " + id)
	await _settle()
	for id in [first, second]:
		var pane: Control = shell.panes[id]
		var other: Node = AppState.views[second if id == first else first]
		# The full-height transparent tree covers this point, below its three rows.
		var blank: Vector2 = pane.tree.position + Vector2(pane.tree.size.x * 0.5, pane.tree.size.y * 0.7)
		test.check(pane.tree.visible and pane.tree.get_item_at_position(blank - pane.tree.position) == null, "navigation starts in transparent tree space in " + id)
		await _exercise_navigation(test, pane, other, blank, "below tree rows")
		var exposed: Vector2 = Vector2(pane.size.x - 35, pane.size.y * 0.65)
		await _exercise_navigation(test, pane, other, exposed, "outside tree")
		# A drag starting in the scene retains ownership across a tree row and release.
		var row: Rect2 = pane.tree.get_item_area_rect(pane._items["layer:" + str(layers[id])], 0)
		var over_row: Vector2 = pane.tree.position + Vector2(row.size.x * 0.5, row.get_center().y)
		var visible: bool = AppState.views[id].get_layer(layers[id]).visible
		_button(pane, exposed, MOUSE_BUTTON_LEFT, true)
		_motion(pane, exposed, over_row, MOUSE_BUTTON_MASK_LEFT)
		_button(pane, over_row, MOUSE_BUTTON_LEFT, false)
		var released_pose: Dictionary = AppState.views[id].pose()
		_motion(pane, over_row, blank, 0)
		test.check(not pane._drag and AppState.views[id].pose() == released_pose and AppState.views[id].get_layer(layers[id]).visible == visible, "scene drag releases over tree without changing visibility or leaving navigation stuck in " + id)
		await _settle()
	# The same hit testing must work when a viewport becomes a native window.
	shell.detach_view(second)
	await _settle()
	var floating_pane: Control = shell.panes[second]
	var blank: Vector2 = floating_pane.tree.position + floating_pane.tree.size * Vector2(0.5, 0.7)
	await _exercise_navigation(test, floating_pane, AppState.views[first], blank, "floating tree blank space")
	shell.dock_view(second)
	for id in [first, second]: AppState.remove_layer(id, layers[id])
	shell.close_view(second)
	AppState.views[first].restore_pose(saved_pose)
	AppState.activate(first)
	await _settle()

func _exercise_navigation(test: Node, pane: Control, other: Node, at: Vector2, context: String) -> void:
	var view: Node = AppState.views[pane.view_id]
	var saved: Dictionary = view.pose()
	var other_pose: Dictionary = other.pose()
	var end := at + Vector2(20, 15)
	_button(pane, at, MOUSE_BUTTON_LEFT, true)
	_motion(pane, at, end, MOUSE_BUTTON_MASK_LEFT)
	_button(pane, end, MOUSE_BUTTON_LEFT, false)
	test.check(view.angles != saved.angles and view.target == saved.target, "left drag rotates through %s in %s" % [context, pane.view_id])
	for button in [MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE]:
		var target: Vector3 = view.target
		var angles: Vector2 = view.angles
		_button(pane, at, button, true)
		_motion(pane, at, end, MOUSE_BUTTON_MASK_RIGHT if button == MOUSE_BUTTON_RIGHT else MOUSE_BUTTON_MASK_MIDDLE)
		_button(pane, end, button, false)
		test.check(view.target != target and view.angles == angles, "button %d pans through %s in %s" % [button, context, pane.view_id])
	var distance: float = view.distance
	_button(pane, at, MOUSE_BUTTON_WHEEL_UP, true)
	_button(pane, at, MOUSE_BUTTON_WHEEL_UP, false)
	test.check(view.distance < distance, "wheel zooms in through %s in %s" % [context, pane.view_id])
	distance = view.distance
	_button(pane, at, MOUSE_BUTTON_WHEEL_DOWN, true)
	_button(pane, at, MOUSE_BUTTON_WHEEL_DOWN, false)
	test.check(view.distance > distance, "wheel zooms out through %s in %s" % [context, pane.view_id])
	test.check(AppState.active_id == pane.view_id and other.pose() == other_pose and not pane._drag, "navigation activates only its own viewport and preserves sibling camera: " + context)
	view.restore_pose(saved)
	await _settle()

func _button(pane: Control, at: Vector2, button: MouseButton, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.pressed = pressed
	event.position = pane.get_global_transform_with_canvas() * at * pane.get_window().content_scale_factor
	pane.get_window().push_input(event)

func _motion(pane: Control, start: Vector2, end: Vector2, mask: int) -> void:
	var event := InputEventMouseMotion.new()
	event.position = pane.get_global_transform_with_canvas() * end * pane.get_window().content_scale_factor
	event.relative = (end - start) * pane.get_window().content_scale_factor
	event.button_mask = mask
	pane.get_window().push_input(event)

func _settle() -> void:
	await get_tree().process_frame
	await get_tree().process_frame
	await get_tree().process_frame
