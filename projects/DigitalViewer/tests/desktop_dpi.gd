extends Node
const Dpi = preload("res://scripts/desktop/dpi_scaling.gd")

func run(test: Node, shell: Control) -> void:
	test.check(shell.theme.default_font_size == 14 and shell._script.editor.get_theme_font_size("font_size") == 14 and shell._script._status.get_theme_font_size("font_size") == 12, "DPI adaptation preserves all existing font sizes")
	var main := shell.get_window()
	test.check(is_equal_approx(main.content_scale_factor, Dpi.screen_scale(main.current_screen)), "main UI uses current monitor scale")
	if DisplayServer.get_name() == "headless": return
	var saved_screen := main.current_screen
	var saved_size := main.size
	var saved_position := main.position
	var events := [0]
	var on_dpi := func(): events[0] += 1
	main.dpi_changed.connect(on_dpi)
	var first: String = AppState.active_id
	var second: String = shell.split_view(first, false)
	var globe: String = AppState.add_source(second, "", "ellipsoid")
	shell.detach_view(second)
	shell._float_panel("properties")
	shell._float_panel("script")
	var floating: Window = shell._windows[second]
	var properties: Window = shell._windows.properties
	var script: Window = shell._windows.script
	var pane: Control = shell.panes[second]
	var screens: Array[int] = []
	for screen in DisplayServer.get_screen_count(): screens.append(screen)
	screens.append(saved_screen) # Round trip must not accumulate scaling.
	var scales: Array[float] = []
	for screen in DisplayServer.get_screen_count(): scales.append(Dpi.screen_scale(screen))
	main.size = Vector2i(Vector2(1050, 700) * main.content_scale_factor)
	await get_tree().create_timer(0.3).timeout
	var main_logical := Vector2(main.size) / main.content_scale_factor
	var float_logical := Vector2(floating.size) / floating.content_scale_factor
	for screen in screens:
		var other := (screen + 1) % DisplayServer.get_screen_count()
		_move(main, screen)
		_move(floating, other)
		_move(properties, screen)
		_move(script, other)
		await get_tree().create_timer(0.4).timeout
		for window: Window in [main, floating, properties, script]:
			var expected := scales[window.current_screen]
			test.check(is_equal_approx(window.content_scale_factor, expected), "independent monitor scale: %s screen=%d factor=%s" % [window.title, window.current_screen, expected])
		test.check(main.current_screen == screen and floating.current_screen == other, "native windows actually moved to requested monitors")
		test.check((Vector2(main.size) / main.content_scale_factor).distance_to(main_logical) < 4, "main logical size survives monitor round trip")
		test.check((Vector2(floating.size) / floating.content_scale_factor).distance_to(float_logical) < 4, "floating logical size survives monitor round trip")
		print("DIGITALVIEWER_DPI screen=%d dpi=%d factor=%s physical=%s logical=%s" % [screen, DisplayServer.screen_get_dpi(screen), main.content_scale_factor, main.size, Vector2(main.size) / main.content_scale_factor])
		AppState.activate(second)
		pane.tree.hide()
		await get_tree().process_frame
		await get_tree().process_frame
		var pixels: Vector2 = pane.display.size * floating.content_scale_factor
		test.check(Vector2(AppState.views[second].viewport.size).distance_to(pixels) < 2, "3D viewport retains physical pixel resolution after DPI change")
		# Native-window input is in physical pixels; Window transforms it into UI coordinates.
		var at: Vector2 = (pane.display.global_position + pane.display.size / 2) * floating.content_scale_factor
		AppState.views[second].clear_pick_cache()
		for pressed in [true, false]:
			var event := InputEventMouseButton.new()
			event.button_index = MOUSE_BUTTON_LEFT
			event.pressed = pressed
			event.position = at
			floating.push_input(event)
		test.check(AppState.views[second].last_pick.get("layer", "") == globe, "physical mouse position picks correct geometry at monitor scale")
		pane._menu.show_popup()
		await get_tree().process_frame
		test.check(pane._menu.get_popup().current_screen == other and is_equal_approx(pane._menu.get_popup().content_scale_factor, scales[other]), "viewport menu follows its floating window")
		pane._menu.get_popup().hide()
		shell._open_add(second)
		await get_tree().process_frame
		test.check(shell._add.current_screen == other and is_equal_approx(shell._add.content_scale_factor, scales[other]), "add dialog opens on originating viewport monitor")
		test.check(shell._add.size.x >= roundi(560 * scales[other]), "dialog minimum width uses logical units")
		Dpi.popup(shell._file, Vector2i.ZERO, shell._add, 0.7)
		await get_tree().process_frame
		test.check(shell._file.current_screen == other and is_equal_approx(shell._file.content_scale_factor, scales[other]), "file dialog follows owner monitor")
		shell._file.hide()
		shell._add.hide()
		shell._script._ask("DPI validation", Callable())
		await get_tree().process_frame
		test.check(shell._script._confirm.current_screen == other and is_equal_approx(shell._script._confirm.content_scale_factor, scales[other]), "script confirmation follows detached workbench")
		shell._script._confirm.hide()
		if "--dv-capture" in OS.get_cmdline_user_args():
			var directory := OS.get_environment("DV_TEST_DIRECTORY").get_base_dir()
			await RenderingServer.frame_post_draw
			main.get_texture().get_image().save_png(directory.path_join("desktop-dpi-%d.png" % roundi(scales[screen] * 100)))
	# Move a dialog while it is already visible, then maximize and restore the main window.
	shell._open_add(second)
	var other := (saved_screen + 1) % DisplayServer.get_screen_count()
	_move(shell._add, other)
	await get_tree().create_timer(0.4).timeout
	test.check(is_equal_approx(shell._add.content_scale_factor, scales[other]), "visible dialog adapts when dragged to another monitor")
	shell._add.hide()
	main.mode = Window.MODE_MAXIMIZED
	await get_tree().create_timer(0.3).timeout
	_move(main, other)
	await get_tree().create_timer(0.4).timeout
	test.check(main.mode == Window.MODE_MAXIMIZED and is_equal_approx(main.content_scale_factor, scales[other]), "maximized window preserves mode and updates monitor scale")
	main.mode = Window.MODE_WINDOWED
	if scales.min() != scales.max(): test.check(events[0] > 0, "Windows backend emits real DPI change notifications")
	main.dpi_changed.disconnect(on_dpi)
	for key in [second, "properties", "script"]: shell.dock_view(key)
	shell._script.hide()
	shell.close_view(second)
	_move(main, saved_screen)
	await get_tree().create_timer(0.3).timeout
	main.size = saved_size
	main.position = saved_position
	AppState.activate(first)

func _move(window: Window, screen: int) -> void:
	window.current_screen = screen
	if window.mode == Window.MODE_WINDOWED:
		window.position = DisplayServer.screen_get_usable_rect(screen).position + Vector2i(40, 80)
