extends Node

var checks := 0
var failures: Array[String] = []
var app: Control


func check(condition: bool, description: String) -> void:
	checks += 1
	if not condition:
		failures.append(description)


func _ready() -> void:
	app = load("res://scenes/main.tscn").instantiate()
	add_child(app)
	await get_tree().process_frame
	await get_tree().process_frame
	await _run()
	if failures.is_empty():
		print("DIGITALVIEWER_SCRIPTING_OK checks=%d" % checks)
		get_tree().quit(0)
	else:
		for failure: String in failures:
			printerr("SCRIPTING_TEST_FAILED: " + failure)
		get_tree().quit(1)


func _run() -> void:
	var original_size := app.size
	var panel: Control = app._workbench
	var runner: Node = panel.runner
	var live: Node = AppState.live_scene
	var desktop: DigitalViewerViewport = app._desktop_shell.find_children("*", "DigitalViewerViewport", true, false)[0]
	var mobile: DigitalViewerViewport = app._mobile_shell.find_children("*", "DigitalViewerViewport", true, false)[0]
	var examples = preload("res://scripts/scripting/script_examples.gd")
	var original_camera := AppState.camera_snapshot()
	check(desktop._subviewport == mobile._subviewport and desktop._runtime_objects.globe == mobile._runtime_objects.globe, "layouts render a single real world")
	check(not runner.run_source(examples.satellite(), false) and not live.native_owned, "untrusted source rejected before compile and takeover")
	check(runner.run_source(examples.satellite(), true), "native GDScript compiles and runs")
	check(live.native_owned and runner.running, "native ownership active")
	var session: Node = live.object("world").get_node_or_null("ScriptSession")
	check(session != null and session.get_node("SaturnRing").mesh is TorusMesh, "native class outside JSON whitelist inserted")
	check(live.object("globe").material_override.albedo_color.to_html(false) == "315caa", "direct existing-object edit")
	check(_has_entry(runner, "output", "已插入"), "native print captured")
	var satellite: Node3D = session.get_node("Satellite")
	var before := satellite.position
	await get_tree().create_timer(0.1).timeout
	check(satellite.position != before, "native _process advances live nodes")
	check(is_equal_approx(live.object("camera").position.z, 10.5), "native camera edits are not overwritten each frame")
	app.size = Vector2(390, 844)
	await get_tree().process_frame
	check(mobile._runtime_objects.globe == desktop._runtime_objects.globe and is_instance_valid(satellite), "layout switch preserves native nodes and identity")
	var result: Dictionary = AppState.runtime.execute([{"op": "set", "id": "globe", "property": "visible", "value": false}])
	check(not result.ok and live.object("globe").visible, "JSON mutations blocked during native ownership")
	result = AiAssistant.commands.execute("native_block", "get_scene_state", {}, AppState.revision)
	check(not result.ok and "原生" in AiAssistant.commands.context().scene, "AI reports native ownership instead of stale scene")
	AppState.runtime.scene_changed.emit()
	check(live.object("globe").material_override.albedo_color.to_html(false) == "315caa", "registry refresh cannot overwrite native edits")
	runner.stop()
	await get_tree().process_frame
	check(not runner.running and not is_instance_valid(satellite), "stop removes owned nodes")
	check(live.native_owned and live.object("globe").material_override.albedo_color.to_html(false) == "315caa", "stop preserves external direct edits and ownership")
	check(runner.run_source(examples.satellite(), true), "rerun creates fresh session")
	runner.run_source(examples.satellite(), true)
	check(live.object("world").find_children("ScriptSession", "", false, false).size() == 1, "rerun does not accumulate sessions")
	runner.reset_scene()
	check(not live.native_owned and live.object("globe").material_override.albedo_color.to_html(false) == "28798b", "reset rebuilds baseline")
	check(AppState.camera_snapshot() == original_camera, "reset restores navigation baseline")
	check(desktop._runtime_objects.globe == live.object("globe") and mobile._earth_content == live.object("earth"), "layout references follow reset")
	check(AppState.runtime.execute([{"op": "set", "id": "globe", "property": "visible", "value": true}]).ok, "JSON resumes after reset")
	check(not runner.run_source("extends Node\nfunc run(viewer):\n\tvar broken =\n", true), "syntax error returns failure")
	check(_has_entry(runner, "error", ""), "compile diagnostic captured")
	check(_error_line(runner) == 3, "compile diagnostic includes exact source line")
	var invalid := "extends Node\nfunc run(viewer):\n\tvar missing = null\n\tmissing.no_such_method()\n"
	check(not runner.run_source(invalid, true), "runtime error is not reported as successful execution")
	check(_error_line(runner) == 4, "runtime diagnostic includes exact source line")
	check(not runner.running, "runtime error disposes session")
	var callback_error := "extends Node\nfunc run(viewer):\n\tpass\nfunc _process(_delta):\n\tvar missing = null\n\tmissing.no_such_method()\n"
	runner.run_source(callback_error, true)
	await get_tree().process_frame
	await get_tree().process_frame
	await get_tree().process_frame
	check(not runner.running and _error_line(runner) == 6, "later callback error stops session and records stack line")
	check(not runner.run_source("extends RefCounted\nfunc run(viewer):\n\tpass\n", true), "non-Node scripts rejected")
	check(not runner.run_source("extends Node\n", true), "missing entry rejected")
	check(not runner.run_source("extends Node\nfunc _init(required):\n\tpass\nfunc run(viewer):\n\tpass\n", true), "required constructor parameters rejected before instantiation")
	var static_source := "extends Node\nstatic func _static_init():\n\tEngine.set_meta(&\"dv_native_test\", true)\nfunc run(viewer):\n\tpass\n"
	check(not runner.run_source(static_source, false) and not Engine.has_meta(&"dv_native_test"), "trust gate also protects compilation-time static initializer")
	check(runner.run_source(static_source, true) and Engine.get_meta(&"dv_native_test", false), "trusted native static initialization supported")
	Engine.remove_meta(&"dv_native_test")
	check(not runner.run_source(" ".repeat(65537), true), "oversize source rejected")
	runner.script_directory = "res://.godot/native-script-tests"
	var filename := "sample_%d.gd" % Time.get_ticks_usec()
	check(not runner.save_source("../escape.gd", "bad").ok, "file path traversal rejected")
	check(runner.save_source(filename, examples.satellite()).ok, "source saved in test-isolated directory")
	check(runner.save_source(filename, "modified").get("exists", false), "overwrite requires explicit confirmation")
	check(runner.load_source(filename).source == examples.satellite(), "saved source round trips without execution")
	check(filename in runner.list_scripts(), "saved script appears in library")
	# Remove only the exact file created by this test.
	check(DirAccess.remove_absolute(runner.script_directory.path_join(filename)) == OK, "test artifact cleaned up")
	runner.script_directory = runner.SCRIPT_DIR
	runner.run_source(invalid, true)
	panel.editor.text = "# edited buffer"
	panel._jump_to_entry({"file": runner.source_path, "line": 4})
	check(panel._tabs.current_tab == 3 and panel.snapshot_editor.text == invalid and panel.editor.text == "# edited buffer", "error after editing opens immutable run snapshot")
	panel._tabs.current_tab = 0
	runner.reset_scene()
	app._open_scripts()
	panel.editor.text = examples.material()
	panel._request_run()
	check(panel._confirm.visible and not runner.running, "UI run requires explicit confirmation")
	panel.editor.text = "# changed since confirmation"
	panel._confirm.confirmed.emit()
	panel._confirm.hide()
	check(runner.running and runner.source_snapshot == examples.material(), "approval executes the exact captured source")
	runner.reset_scene()
	panel._replace_source(examples.satellite(), "satellite.gd")
	app.size = Vector2(844, 390)
	panel._layout()
	await get_tree().process_frame
	await get_tree().process_frame
	check(panel.position.y >= 0 and panel.get_rect().end.y <= app.size.y + 1, "workbench fits current window: panel=%s app=%s" % [panel.get_rect(), app.size])
	if "--capture-scripting" in OS.get_cmdline_user_args():
		app.size = original_size
		panel._layout()
		check(runner.run_source(examples.satellite(), true), "capture example running")
		panel.refresh_tree()
		await get_tree().create_timer(0.3).timeout
		await RenderingServer.frame_post_draw
		var suffix := "mobile" if app._using_mobile_layout else "desktop"
		get_viewport().get_texture().get_image().save_png("res://design/native-scripting-%s.png" % suffix)
	runner.reset_scene()


func _has_entry(runner: Node, kind: String, text: String) -> bool:
	for entry: Dictionary in runner.entries:
		if entry.kind == kind and (text.is_empty() or text in entry.message):
			return true
	return false


func _error_line(runner: Node) -> int:
	for entry: Dictionary in runner.entries:
		if entry.kind == "error":
			return int(entry.line)
	return 0
