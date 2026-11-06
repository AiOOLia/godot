extends Node

func run(test: Node, shell: Control, view_id: String) -> void:
	var view: Node = AppState.views[view_id]
	var original_pose: Dictionary = view.pose()
	var original_background: int = view.background_mode
	AppState.activate(view_id)
	view.show_viewport_properties = true
	shell._refresh_properties()
	var sun: DirectionalLight3D = view.lights[0].node
	var original_rotation := sun.rotation_degrees
	var card := _card(shell, sun)
	test.check(card != null and not _has_text(_lights(shell), "No lights"), "default directional light appears in the lights panel")
	if card == null: return
	_slider(card, "Elevation").value = 38.0
	_slider(card, "Azimuth").value = 127.0
	test.check(is_equal_approx(sun.rotation_degrees.x, -38.0) and is_equal_approx(sun.rotation_degrees.y, 127.0), "direction controls update the actual directional light")
	var shadow: CheckBox = card.find_children("*", "CheckBox", true, false)[1]
	shadow.button_pressed = true
	test.check(sun.shadow_enabled, "shadow checkbox updates the actual light")
	shadow.button_pressed = false
	var original_energy := sun.light_energy
	_slider(card, "Energy").value = 2.3
	test.check(is_equal_approx(sun.light_energy, 2.3), "energy control updates the actual light")
	if "--dv-capture" in OS.get_cmdline_user_args() and DisplayServer.get_name() != "headless":
		for frame in 3: await get_tree().process_frame
		shell._property_scroll.scroll_vertical = int(shell._property_scroll.get_v_scroll_bar().max_value)
		for frame in 3: await get_tree().process_frame
		await RenderingServer.frame_post_draw
		var directory := OS.get_environment("DV_TEST_DIRECTORY")
		if directory.is_empty(): directory = "res://.godot/profile-test"
		DirAccess.make_dir_recursive_absolute(directory)
		get_viewport().get_texture().get_image().save_png(directory.path_join("lights.png"))
	await _test_camera_follow(test, shell, view_id, sun)
	for mode in [0, 2, 3, 1]:
		view.set_background_mode(mode)
		shell._refresh_properties()
		test.check(_card(shell, sun) != null, "directional light stays editable with background %d" % mode)
	# Large, offset CAD assemblies must receive new local lights too.
	view.target = Vector3(12000, -4000, 5000)
	view.distance = 6000.0
	view.update_camera()
	var added: Array[Light3D] = []
	for type in 3:
		_button(_lights(shell), ["Add directional light", "Add point light", "Add spot light"][type]).pressed.emit()
		await get_tree().process_frame
		shell._refresh_properties()
		var light_node: Light3D = view.lights[-1].node
		added.append(light_node)
		card = _card(shell, light_node)
		test.check(card != null and int(view.lights[-1].type) == type, "add button creates and displays light type %d" % type)
		if type == 0:
			test.check(light_node is DirectionalLight3D, "directional button creates a directional light")
			continue
		var local_distance := light_node.position.distance_to(view.target)
		var reach: float = light_node.omni_range if light_node is OmniLight3D else light_node.spot_range
		test.check(light_node.position.is_equal_approx(view.camera.position) and reach > local_distance, "new local light reaches the framed CAD model")
		var position_fields := card.find_children("*", "SpinBox", true, false)
		for axis in 3: position_fields[axis].value = 100.0 + axis
		test.check(light_node.position.is_equal_approx(Vector3(100, 101, 102)), "XYZ position controls bind to separate axes")
		_slider(card, "Range").value = 24000.0
		_slider(card, "Attenuation").value = 2.0
		if type == 1:
			test.check(light_node is OmniLight3D and is_equal_approx(light_node.omni_range, 24000.0) and is_equal_approx(light_node.omni_attenuation, 2.0), "point light controls bind to point range and attenuation")
		else:
			test.check((-light_node.basis.z).dot(-view.camera.basis.z) > 0.999, "new spot light points toward the framed model")
			_slider(card, "Elevation").value = 24.0
			_slider(card, "Azimuth").value = 80.0
			_slider(card, "Spot angle").value = 32.0
			_slider(card, "Spot attenuation").value = 3.0
			test.check(light_node is SpotLight3D and is_equal_approx(light_node.spot_range, 24000.0) and is_equal_approx(light_node.spot_attenuation, 2.0), "spot light controls bind to spot range and attenuation")
			test.check(is_equal_approx(light_node.rotation_degrees.x, -24.0) and is_equal_approx(light_node.rotation_degrees.y, 80.0) and is_equal_approx(light_node.spot_angle, 32.0) and is_equal_approx(light_node.spot_angle_attenuation, 3.0), "spot direction and cone controls update the actual spot light")
	# Two callbacks from the same panel must still remove the right nodes after indices shift.
	var point_remove := _remove_button(_card(shell, added[1]))
	var spot_remove := _remove_button(_card(shell, added[2]))
	point_remove.pressed.emit()
	spot_remove.pressed.emit()
	test.check(view.lights.size() == 2 and view.lights[0].node == sun and view.lights[1].node == added[0], "removal uses node identity when light indices shift")
	view.remove_light(1)
	sun.rotation_degrees = original_rotation
	sun.light_energy = original_energy
	view.restore_pose(original_pose)
	view.set_background_mode(original_background)
	view.show_viewport_properties = false
	AppState.activate(view_id)
	await get_tree().process_frame

func _test_camera_follow(test: Node, shell: Control, view_id: String, sun: DirectionalLight3D) -> void:
	var view: Node = AppState.views[view_id]
	var original_pose: Dictionary = view.pose()
	var manual_basis := sun.basis
	var follow := _checkbox(_card(shell, sun), "Follow camera")
	test.check(follow != null and not follow.button_pressed, "directional light follows the camera only when enabled")
	if follow == null: return
	follow.button_pressed = true
	await get_tree().process_frame
	test.check(sun.global_basis.is_equal_approx(view.camera.global_basis), "enabling camera follow immediately aligns light with its camera")
	test.check(_slider(_card(shell, sun), "Elevation") == null and _slider(_card(shell, sun), "Azimuth") == null, "manual direction fields are hidden while following the camera")
	view.orbit(Vector2(0.6, -0.2))
	test.check(sun.global_basis.is_equal_approx(view.camera.global_basis), "camera orbit updates the following light immediately")
	view.set_light_follow_camera(sun, true)
	view.camera.rotate_y(0.2)
	for frame in 2: await get_tree().process_frame
	test.check(sun.global_basis.is_equal_approx(view.camera.global_basis), "following light tracks camera changes made directly by scripts")
	var fixed: DirectionalLight3D = view.add_light(0).node
	fixed.rotation_degrees = Vector3(-20, 70, 0)
	var fixed_basis := fixed.basis
	view.orbit(Vector2(-0.4, 0.1))
	test.check(fixed.basis.is_equal_approx(fixed_basis), "camera follow is independent for each directional light")
	view.remove_light(view.lights.size() - 1)
	var other_id: String = shell.split_view(view_id, false)
	var other: Node = AppState.views[other_id]
	other.set_light_follow_camera(other.light, true)
	other.orbit(Vector2(-1.2, 0.4))
	test.check(other.light.global_basis.is_equal_approx(other.camera.global_basis) and sun.global_basis.is_equal_approx(view.camera.global_basis), "each viewport light follows its own camera")
	AppState.activate(view_id, "viewport", "", null, true)
	shell._refresh_properties()
	test.check(_checkbox(_card(shell, sun), "Follow camera").button_pressed, "camera follow state survives property refresh and viewport changes")
	_checkbox(_card(shell, sun), "Follow camera").button_pressed = false
	await get_tree().process_frame
	test.check(sun.basis.is_equal_approx(manual_basis), "disabling camera follow restores the previous manual direction")
	view.orbit(Vector2(0.2, 0.3))
	test.check(sun.basis.is_equal_approx(manual_basis) and other.light.global_basis.is_equal_approx(other.camera.global_basis), "disabling one light preserves other viewport follow settings")
	test.check(is_equal_approx(_slider(_card(shell, sun), "Elevation").value, 38.0) and is_equal_approx(_slider(_card(shell, sun), "Azimuth").value, 127.0), "manual angle controls return with their saved values")
	shell.close_view(other_id)
	view.restore_pose(original_pose)
	AppState.activate(view_id, "viewport", "", null, true)
	shell._refresh_properties()

func _checkbox(card: Node, text: String) -> CheckBox:
	for label in card.find_children("*", "Label", true, false):
		if label.text == tr(text):
			for control in label.get_parent().get_children():
				if control is CheckBox: return control
	return null

func _lights(shell: Control) -> Node:
	return shell._property_fields.find_child("Lights", true, false)

func _card(shell: Control, light_node: Light3D) -> PanelContainer:
	return _lights(shell).get_node_or_null(NodePath(light_node.name)) as PanelContainer

func _slider(card: Node, text: String) -> HSlider:
	for label in card.find_children("*", "Label", true, false):
		if label.text == tr(text) and label.get_parent().has_meta("slider"):
			return label.get_parent().get_meta("slider") as HSlider
	return null

func _has_text(parent: Node, text: String) -> bool:
	return parent.find_children("*", "Label", true, false).any(func(label: Node): return label.text == tr(text))

func _button(parent: Node, text: String) -> Button:
	for button in parent.find_children("*", "Button", true, false):
		if button.text == tr(text): return button
	return null

func _remove_button(card: Node) -> Button:
	for button in card.find_children("*", "Button", true, false):
		if button.tooltip_text == tr("Remove"): return button
	return null
