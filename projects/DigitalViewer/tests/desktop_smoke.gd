extends Node
const Geo = preload("res://scripts/desktop/geo.gd")
var failures: Array[String] = []
var checks := 0
var fixture_model := ""
var fixture_tileset := ""
var fixture_osg := ""
class RuntimeErrors extends Logger:
	var errors: Array[String] = []
	var mutex := Mutex.new()
	func _log_error(_function: String, file: String, line: int, code: String, rationale: String, _notify: bool, error_type: int, _backtraces: Array[ScriptBacktrace]) -> void:
		if error_type == Logger.ERROR_TYPE_WARNING: return
		mutex.lock()
		errors.append("%s:%d %s %s" % [file, line, code, rationale])
		mutex.unlock()

func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures.append(message)
		push_error("DESKTOP_CHECK: " + message)

func run(shell: Control) -> void:
	var runtime_errors := RuntimeErrors.new()
	OS.add_logger(runtime_errors)
	var fixture_directory := OS.get_environment("DV_TEST_DIRECTORY")
	if fixture_directory.is_empty(): fixture_directory = "res://.godot/desktop-zip-test"
	DirAccess.make_dir_recursive_absolute(fixture_directory)
	var dataset := OS.get_environment("DV_TEST_DATASET")
	if not dataset.is_empty():
		OS.remove_logger(runtime_errors)
		var probe := preload("res://tests/desktop_dataset.gd").new()
		add_child(probe)
		await probe.run(shell, dataset)
		return
	fixture_model = fixture_directory.path_join("triangle.gltf")
	fixture_tileset = fixture_directory.path_join("tileset.json")
	fixture_osg = fixture_directory.path_join("triangle.osg")
	for pair in [[fixture_model, "res://tests/fixtures/triangle.gltf.txt"], [fixture_tileset, "res://tests/fixtures/tileset.json"], [fixture_osg, "res://tests/fixtures/triangle.osg.txt"]]:
		var file := FileAccess.open(pair[0], FileAccess.WRITE)
		if file:
			file.store_buffer(FileAccess.get_file_as_bytes(pair[1]))
			file.close()
	await get_tree().process_frame
	check(AppState.views.size() == 1, "one initial viewport")
	var first: String = AppState.active_id
	check(AppState.views[first].layers.is_empty(), "empty startup with no fake layers")
	var single_view_scripts := preload("res://tests/desktop_scripts.gd").new()
	add_child(single_view_scripts)
	await single_view_scripts.run_single_view_sequence(self, shell, first)
	single_view_scripts.queue_free()
	var tree_tests := preload("res://tests/desktop_tree.gd").new()
	add_child(tree_tests)
	await tree_tests.run(self, shell, first)
	tree_tests.queue_free()
	var navigation_tests := preload("res://tests/desktop_navigation.gd").new()
	add_child(navigation_tests)
	await navigation_tests.run(self, shell, first, fixture_osg)
	navigation_tests.queue_free()
	check(AppState.store.records.is_empty(), "no sources or network requests on startup")
	check(get_node_or_null("/root/AiAssistant") == null, "no built-in AI singleton")
	for coordinates in [[0.0, 0.0, 0.0], [121.4737, 31.2304, 123.45], [-179.9, -80.0, -50.0], [0.0, 90.0, 10.0], [10.0, 89.9999, -5.0]]:
		var p := Geo.ecef(coordinates[0], coordinates[1], coordinates[2])
		var q := Geo.geodetic(p)
		check(q.size() == 3 and absf(q[1] - coordinates[1]) < 1e-7 and absf(q[2] - coordinates[2]) < 1e-4, "WGS84 round trip " + str(coordinates))
	var origin := Geo.ecef(121.0, 31.0)
	var position := Vector3(123.25, -21.5, 4.75)
	check(Geo.local(Geo.from_local(position, origin, 121, 31), origin, 121, 31).distance_to(position) < 0.0001, "ECEF / East-Up-South round trip")
	check(Geo.ecef(INF, 0).is_empty() and Geo.ecef(0, 91).is_empty(), "reject non-finite and invalid coordinates")
	var second: String = shell.split_view(first, false)
	await get_tree().process_frame
	var outer: SplitContainer = shell.panes[first].get_parent()
	await get_tree().process_frame
	check(shell.panes[first].size.x > outer.size.x * 0.25 and shell.panes[second].size.x > outer.size.x * 0.25, "horizontal split gives both panes visible area")
	check(not shell.panes[first].get_global_rect().intersects(shell.panes[second].get_global_rect()), "split panes do not overlap")
	outer.split_offset = 103
	var sibling: Control = shell.panes[second]
	var third: String = shell.split_view(first, true)
	await get_tree().process_frame
	check(sibling.get_parent() == outer and outer.split_offset == 103, "split preserves adjacent subtree and ratio")
	check(AppState.views[first].viewport.world_3d != AppState.views[second].viewport.world_3d, "independent World3D")
	var old_pose: Dictionary = AppState.views[second].pose()
	AppState.views[first].orbit(Vector2(0.3, 0.1))
	check(AppState.views[second].pose() == old_pose, "independent cameras")
	for index in 4: shell.split_view(third, index % 2 == 0)
	check(AppState.views.size() == 7, "more than six recursive viewports")
	var root_before: Node = shell._layout_root
	shell.toggle_fullscreen(second)
	check(shell._full_id == second, "fullscreen active pane")
	shell.toggle_fullscreen(second)
	check(shell._layout_root == root_before and sibling.get_parent() == outer, "fullscreen restores same tree")
	if DisplayServer.get_name() != "headless":
		shell.detach_view(second)
		check(shell._windows.has(second), "native window detaches")
		shell.dock_view(second)
		check(sibling.get_parent() == outer and AppState.views[second].pose() == old_pose, "dock restores identity and pose")
		for key in ["properties", "script"]:
			var runner_before: Node = AppState.runner
			shell._float_panel(key)
			check(shell._windows.has(key), "panel detaches: " + key)
			shell.dock_view(key)
			check(not shell._windows.has(key) and runner_before == AppState.runner, "panel docks without recreating runner: " + key)
	var globe: String = AppState.add_source(first, "", "ellipsoid")
	await get_tree().process_frame
	await get_tree().process_frame
	var layer: Dictionary = AppState.views[first].get_layer(globe)
	check(is_instance_valid(layer.node) and layer.status == "Ready", "explicit ellipsoid creates actual mesh")
	check(AppState.views[second].layers.is_empty(), "adding source affects only target viewport")
	check(AppState.share_layer(first, globe, second), "drag sharing creates reference")
	var shared: Dictionary = AppState.views[second].layers[0]
	check(layer.node != shared.node and layer.node.mesh == shared.node.mesh, "shared geometry, independent scene instances")
	AppState.set_visible(first, globe, false)
	check(not layer.node.visible and shared.node.visible, "visibility is per reference")
	AppState.set_opacity(first, globe, 0.4)
	check(is_equal_approx(layer.node.transparency, 0.6) and shared.node.transparency == 0, "opacity is per reference")
	var data_id: String = layer.data_id
	AppState.remove_layer(first, globe)
	check(AppState.store.reference_count(data_id) == 1 and is_instance_valid(shared.node), "removing one reference retains data")
	AppState.remove_layer(second, shared.id)
	check(not AppState.store.records.has(data_id), "last reference releases prototype")
	var model: String = AppState.add_source(first, fixture_model)
	await _wait_layer(first, model)
	var imported: Dictionary = AppState.views[first].get_layer(model)
	check(imported.status == "Ready" and is_instance_valid(imported.node), "real glTF runtime import")
	if imported.status == "Ready":
		check(AppState.share_layer(first, model, second), "glTF sharing")
		check(AppState.store.reference_count(imported.data_id) == 2, "glTF reference count")
		var hits: Array = []
		AppState.views[first]._mesh_pick(imported.node, Vector3(0.2, 0.2, 1), Vector3(0, 0, -1), hits)
		check(not hits.is_empty() and hits[0].position.distance_to(Vector3(0.2, 0.2, 0)) < 0.001, "glTF triangle picking")
		var view: Node = AppState.views[first]
		var picked: Dictionary = view.pick(view.camera.unproject_position(Vector3(0.2, 0.2, 0)))
		check(not picked.is_empty() and is_instance_valid(view._pick_marker) and view._pick_marker.visible, "screen pick creates a visible marker in its viewport")
		view.clear_pick_cache()
		check(not view._pick_marker.visible, "clearing pick hides its marker")
		imported["transform"] = Transform3D(Basis.IDENTITY, Vector3(4, 5, 6))
		imported.node.transform = imported.transform
		AppState.set_visible(first, model, false)
		AppState.set_opacity(first, model, 0.5)
		AppState.store.reload_data(imported.data_id)
		await _wait_layer(first, model)
		var imported_mesh: MeshInstance3D = imported.node.find_children("*", "MeshInstance3D", true, false)[0]
		check(imported.node.position == Vector3(4, 5, 6) and not imported.node.visible and is_equal_approx(imported_mesh.transparency, 0.5), "reload preserves per-view model transform and display")
		var model_shared: Dictionary = AppState.views[second].layers.back()
		var shared_mesh: MeshInstance3D = model_shared.node.find_children("*", "MeshInstance3D", true, false)[0]
		check(model_shared.node.position == Vector3.ZERO and model_shared.node.visible and shared_mesh.transparency == 0, "shared reload preserves independent reference display")
		AppState.set_visible(first, model, true)
	var bad: String = AppState.add_source(first, "res://tests/fixtures/missing.glb")
	await _wait_layer(first, bad)
	check(AppState.views[first].get_layer(bad).status == "Failed", "missing file produces visible failure")
	var cancelled: String = AppState.add_source(first, fixture_model)
	AppState.remove_layer(first, cancelled)
	await get_tree().process_frame
	check(AppState.views[first].get_layer(cancelled).is_empty(), "delete before deferred import cancels load")
	check(ClassDB.class_exists("TiledMeshInstance3D"), "custom engine provides TiledMeshInstance3D")
	if ClassDB.class_exists("TiledMeshInstance3D"):
		var tiles: String = AppState.add_source(first, fixture_tileset)
		await _wait_layer(first, tiles)
		var streamed: Dictionary = AppState.views[first].get_layer(tiles)
		check(streamed.status == "Ready", "explicit tileset root loads")
		for frame in 180:
			await get_tree().process_frame
			var stats: Dictionary = streamed.node.call("get_streaming_stats")
			if stats.get("resident_contents", 0) > 0: break
		var stats: Dictionary = streamed.node.call("get_streaming_stats")
		check(stats.get("resident_contents", 0) > 0, "tileset content becomes resident")
		AppState.remove_layer(first, tiles)
		var osg: String = AppState.add_source(first, fixture_osg, "auto", {"z_up": false})
		await _wait_layer(first, osg)
		var osg_layer: Dictionary = AppState.views[first].get_layer(osg)
		check(osg_layer.status == "Ready", "OSG2 ASCII root loads through native adapter: " + osg_layer.status + " " + osg_layer.error)
		for frame in 180:
			await get_tree().process_frame
			if osg_layer.node.call("get_streaming_stats").get("resident_contents", 0) > 0: break
		check(osg_layer.node.call("get_streaming_stats").get("resident_contents", 0) > 0, "ASCII OSG geometry becomes resident")
		var osg_hit: Dictionary = osg_layer.node.call("intersect_ray", Vector3(-0.3, -0.3, 1), Vector3(0, 0, -1))
		check(not osg_hit.is_empty(), "native OSG triangle picking")
		AppState.set_visible(first, osg, false)
		AppState.store.reload_data(osg_layer.data_id)
		await _wait_layer(first, osg)
		check(not osg_layer.node.visible, "native dataset reload preserves hidden state")
		AppState.remove_layer(first, osg)
		var format_tests := preload("res://tests/desktop_tiles.gd").new()
		add_child(format_tests)
		await format_tests.run(self, first)
		format_tests.queue_free()
		var osg_tests := preload("res://tests/desktop_osg.gd").new()
		add_child(osg_tests)
		await osg_tests.run(self, first)
		osg_tests.queue_free()
	var runner: Node = AppState.runner
	var source := "extends Node\nfunc run(viewer):\n\tvar content = viewer.content(\"%s\")\n\tvar node = MeshInstance3D.new()\n\tnode.name = \"ScriptCube\"\n\tnode.mesh = BoxMesh.new()\n\tcontent.add_child(node)\n" % first
	check(not runner.run_source(source, false), "trust required before compilation")
	check(runner.run_source(source, true), "trusted native script runs")
	check(AppState.views[first].root.find_child("ScriptCube", true, false) != null, "script edits explicit real viewport")
	AppState.activate(second)
	check(runner.running, "viewport switching keeps application script running")
	var root_identity: Node = AppState.views[first].root
	var layer_identity: Node = imported.node
	AppState.describe()
	check(AppState.views[first].root == root_identity and imported.node == layer_identity, "scene description is read-only")
	runner.stop()
	await get_tree().process_frame
	check(AppState.views[first].root.find_child("ScriptCube", true, false) == null, "stop removes per-viewport script content")
	var old_viewport: SubViewport = AppState.views[first].viewport
	runner.reset_scene()
	check(AppState.views[first].viewport == old_viewport and AppState.views[first].root != root_identity and is_instance_valid(imported.node), "reset rebuilds world without replacing viewport")
	var initializer := "extends Node\nstatic func _static_init():\n\tEngine.set_meta(\"dv_untrusted_test\", true)\nfunc run(viewer):\n\tpass\n"
	check(not runner.run_source(initializer, false) and not Engine.has_meta("dv_untrusted_test"), "trust gate blocks static initializers")
	if DisplayServer.get_name() != "headless":
		shell._script.editor.text = source
		shell._script._request_run()
		check(shell._script._confirm.visible and shell._script._confirm.get_ok_button().disabled, "UI run initially requires explicit trust checkbox")
		shell._script._trust.button_pressed = true
		check(not shell._script._confirm.get_ok_button().disabled, "review acknowledgement enables confirmation")
		shell._script._confirm.confirmed.emit()
		shell._script._confirm.hide()
		check(runner.running and runner.source_snapshot == source, "UI confirmation runs captured script")
		shell._float_panel("script")
		check(runner.running, "detaching running script keeps application session alive")
		shell._windows.script.close_requested.emit()
		check(runner.running and not shell._script.visible and not shell._windows.has("script"), "closing native script window preserves session and closes panel")
		runner.stop()
	var bridge: Node = AppState.bridge
	var script_tests := preload("res://tests/desktop_scripts.gd").new()
	add_child(script_tests)
	await script_tests.run(self, shell, first, second)
	script_tests.queue_free()
	if DisplayServer.get_name() != "headless":
		shell._settings()
		var settings: Window = shell.get_children().back()
		shell._change_mcp_token(settings, true)
		var token_dialog: ConfirmationDialog = settings.get_children().back()
		var token_input: LineEdit = token_dialog.get_children().filter(func(node: Node): return node is LineEdit)[0]
		check(token_input.secret and token_dialog.visible, "existing-token dialog masks credential and opens inside settings")
		var before: String = bridge.token
		token_input.text = before
		token_dialog.confirmed.emit()
		await get_tree().process_frame
		check(bridge.token == before and FileAccess.get_file_as_string(bridge.token_path).strip_edges() == before, "settings imports and saves existing MCP credential")
		shell._change_mcp_token(settings, false)
		var reset_dialog: ConfirmationDialog = settings.get_children().back()
		check(bridge.token == before and reset_dialog.visible, "reset requires explicit confirmation")
		reset_dialog.canceled.emit()
		await get_tree().process_frame
		check(bridge.token == before, "cancelled reset preserves credential")
		settings.queue_free()
		await get_tree().process_frame
	check(bridge.call_tool("script.run", {}).isError, "MCP script.run denied by default")
	bridge.permissions["script.run"] = true
	var run_response: Dictionary = bridge.call_tool("script.run", {})
	check(not run_response.isError and not runner.running, "MCP cannot bypass manual run")
	check(bridge.call_tool("shell.exec", {}).isError, "no shell MCP tool")
	check(bridge.call_tool("capture.screenshot", {"viewport": first}).isError, "capture permission required")
	check(bridge._tools().size() == 7, "only seven documented tools exposed")
	await _test_http(bridge)
	var guard = preload("res://scripts/desktop/zip_guard.gd")
	for path in ["../escape.gltf", "C:/escape.gltf", "models/../../escape", "x/CON.gltf", "model./a", "x\\a", "a:b", "/root"]:
		check(not guard.safe_path(path), "reject ZIP path: " + path)
	check(guard.safe_path("models/scene.gltf"), "valid relative ZIP path")
	await _test_zip(first, guard)
	var network_tests := preload("res://tests/desktop_network.gd").new()
	add_child(network_tests)
	await network_tests.run(self, first, second)
	network_tests.queue_free()
	var redacted: String = AppState.redact("https://user:secret@example.org/a?token=secret&key=private\nAuthorization: Bearer secret\nCookie: a=secret")
	check(not "secret" in redacted and not "private" in redacted, "diagnostic secret redaction")
	check(shell.close_view(third), "close merges only selected branch")
	TranslationServer.set_locale("en")
	check(tr("Properties") == "Properties" and tr("I have reviewed and trust this script") == "I have reviewed and trust this script", "English fallback for shell and workbench")
	check(tr(preload("res://scripts/desktop/workbench_strings.gd").HELP_EN).begins_with("DigitalViewer native GDScript"), "English workbench help")
	TranslationServer.set_locale("zh_CN")
	check(tr("Properties") == "属性", "Chinese translation")
	check(tr(preload("res://scripts/desktop/workbench_strings.gd").HELP_EN).begins_with("DigitalViewer 原生"), "Chinese workbench help")
	for id in AppState.views.keys().duplicate():
		if id != first: shell.close_view(id)
	check(not shell.close_view(first) and AppState.views.size() == 1, "last view cannot close")
	var dpi_tests := preload("res://tests/desktop_dpi.gd").new()
	add_child(dpi_tests)
	await dpi_tests.run(self, shell)
	dpi_tests.queue_free()
	if "--dv-capture" in OS.get_cmdline_user_args() and DisplayServer.get_name() != "headless":
		for old_layer in AppState.views[first].layers.duplicate(): AppState.remove_layer(first, old_layer.id)
		AppState.add_source(first, "", "ellipsoid")
		shell._script.hide()
		AppState.views[first].set_grid(true)
		var capture_directory := OS.get_environment("DV_TEST_DIRECTORY").get_base_dir()
		if capture_directory.is_empty(): capture_directory = "res://tests"
		for dimensions in [Vector2i(1366, 768), Vector2i(1920, 1080)]:
			shell.get_window().size = dimensions
			await get_tree().create_timer(0.5).timeout
			check(shell._properties.get_global_rect().end.x <= shell.get_window().get_visible_rect().size.x + 1, "properties fit logical window at " + str(dimensions))
			await RenderingServer.frame_post_draw
			get_viewport().get_texture().get_image().save_png(capture_directory.path_join("desktop-%dx%d.png" % [dimensions.x, dimensions.y]))
		var split_id: String = shell.split_view(first, false)
		AppState.share_layer(first, AppState.views[first].layers[0].id, split_id)
		AppState.views[split_id].orbit(Vector2(0.7, -0.2))
		AppState.views[split_id].set_grid(true)
		AppState.status_changed.emit(tr("Session only · no history saved"))
		await get_tree().create_timer(0.5).timeout
		check(shell.panes[first].size.x > 300 and shell.panes[split_id].size.x > 300, "final split capture contains two usable viewports")
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(capture_directory.path_join("desktop-split.png"))
	AppState.shutdown()
	DirAccess.remove_absolute(fixture_model)
	DirAccess.remove_absolute(fixture_tileset)
	DirAccess.remove_absolute(fixture_osg)
	OS.remove_logger(runtime_errors)
	check(runtime_errors.errors.is_empty(), "no engine or script errors: " + str(runtime_errors.errors))
	if failures.is_empty(): print("DIGITALVIEWER_DESKTOP_OK checks=%d" % checks)
	else: print("DIGITALVIEWER_DESKTOP_FAILED checks=%d failures=%s" % [checks, failures])
	get_tree().quit(0 if failures.is_empty() else 1)

func _wait_layer(id: String, layer_id: String) -> void:
	for frame in 600:
		await get_tree().process_frame
		var layer: Dictionary = AppState.views[id].get_layer(layer_id)
		if layer.status in ["Ready", "Failed"]: return

func _test_http(bridge: Node) -> void:
	check(bridge.enabled, "MCP listener starts")
	if not bridge.enabled: return
	var unauthenticated := await _http({}, "", "")
	check(unauthenticated.code == 401, "HTTP rejects unauthenticated request")
	var init := await _http({"jsonrpc": "2.0", "id": 0, "method": "initialize", "params": {"protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "smoke", "version": "1"}}}, bridge.token, "")
	check(init.code == 200 and init.body is Dictionary and init.body.has("result"), "MCP initialization over HTTP")
	check('"id":0,' in init.raw, "initialize returns integer ID zero, never float 0.0")
	var session := ""
	for header in init.headers:
		if header.to_lower().begins_with("mcp-session-id:"): session = header.get_slice(":", 1).strip_edges()
	check(not session.is_empty(), "MCP returns session ID")
	var listed := await _http({"jsonrpc": "2.0", "id": 2, "method": "tools/list"}, bridge.token, session)
	check(listed.code == 200 and listed.body.get("result", {}).get("tools", []).size() == 7, "HTTP lists seven tools")
	var scene := await _http({"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "scene.describe", "arguments": {}}}, bridge.token, session)
	check(scene.code == 200 and not scene.body.get("result", {}).get("isError", true), "HTTP scene description")
	var blocked := await _http({"jsonrpc": "2.0", "id": 4, "method": "ping"}, bridge.token, session, ["Origin: https://example.org"])
	check(blocked.code == 403, "HTTP rejects browser origin")
	var unknown := await _http({"jsonrpc": "2.0", "id": 5, "method": "ping"}, bridge.token, "invalid")
	check(unknown.code == 404, "HTTP rejects invalid session")
	for id_text in ['0', '-1', '9007199254740993', '9223372036854775807', '-9223372036854775808', '"0"', '"quoted\\\"id"']:
		var echoed := await _http('{"jsonrpc":"2.0","params":{"id":123,"text":"id"},"method":"ping","id":' + id_text + '}', bridge.token, session)
		check(echoed.code == 200 and ('"id":' + id_text + ',') in echoed.raw, "preserve exact integer/string MCP request ID: " + id_text)
	var escaped := await _http('{"jsonrpc":"2.0","i\\u0064":0,"method":"not-a-method"}', bridge.token, session)
	check('"id":0,' in escaped.raw and escaped.body.get("error", {}).get("code") == -32601, "escaped ID key and RPC error preserve integer ID")
	for id_text in ['0.5', 'true', 'null', '[]', '{}', '9223372036854775808', '1e100']:
		var rejected := await _http('{"jsonrpc":"2.0","id":' + id_text + ',"method":"ping"}', bridge.token, session)
		check(rejected.body.get("error", {}).get("code") == -32600, "reject unsupported MCP ID without rounding: " + id_text)
	var duplicate := await _http('{"jsonrpc":"2.0","id":0,"id":1,"method":"ping"}', bridge.token, session)
	check(duplicate.body.get("error", {}).get("code") == -32600, "reject ambiguous duplicate ID")
	var notification := await _http({"jsonrpc": "2.0", "method": "notifications/initialized"}, bridge.token, session)
	check(notification.code == 202 and notification.raw.is_empty(), "notification has no fabricated response ID")
	var saved_token: String = bridge.token
	check(FileAccess.get_file_as_string(bridge.token_path).strip_edges() == saved_token, "MCP credential persists on disk")
	bridge.stop()
	check(not bridge.enabled and bridge._sessions.is_empty() and bridge.token == saved_token, "disabling MCP closes sessions without rotating credential")
	check(bridge.start() == OK and bridge.token == saved_token, "reenabling MCP reuses saved credential")
	var stale := await _http({"jsonrpc": "2.0", "id": 0, "method": "ping"}, saved_token, session)
	check(stale.code == 404, "reenabling MCP does not revive old sessions")
	bridge.stop()
	var reloaded := preload("res://scripts/desktop/mcp_server.gd").new()
	reloaded.token_path = bridge.token_path
	reloaded.port = bridge.port
	add_child(reloaded)
	check(reloaded.start() == OK and reloaded.token == saved_token, "fresh server instance reads same persisted credential")
	var restarted := await _http({"jsonrpc": "2.0", "id": 0, "method": "initialize"}, saved_token, "")
	check(restarted.body.has("result"), "saved credential authenticates fresh server")
	check(reloaded.replace_token("invalid") == ERR_INVALID_PARAMETER and reloaded.token == saved_token, "invalid replacement preserves existing credential")
	check(reloaded.reset_token() == OK and reloaded.token != saved_token, "manual token rotation persists new credential")
	var revoked := await _http({"jsonrpc": "2.0", "id": 0, "method": "initialize"}, saved_token, "")
	check(revoked.code == 401 and reloaded._sessions.is_empty(), "rotation revokes old credential and sessions")
	check(reloaded.replace_token(saved_token) == OK, "existing Codex credential can be imported")
	reloaded.stop()
	reloaded.queue_free()
	var temporary_token_path: String = bridge.token_path + ".tmp"
	DirAccess.make_dir_recursive_absolute(temporary_token_path)
	check(bridge.reset_token() != OK and bridge.token == saved_token and FileAccess.get_file_as_string(bridge.token_path).strip_edges() == saved_token, "failed credential write preserves live and stored token")
	DirAccess.remove_absolute(temporary_token_path)
	bridge.start()

func _http(payload: Variant, token: String, session: String, extra: Array = []) -> Dictionary:
	var request := HTTPRequest.new()
	request.timeout = 3
	add_child(request)
	var headers := PackedStringArray(["Content-Type: application/json", "Accept: application/json, text/event-stream"])
	if not token.is_empty(): headers.append("Authorization: Bearer " + token)
	if not session.is_empty(): headers.append("Mcp-Session-Id: " + session)
	for header in extra: headers.append(header)
	var body: String = payload if payload is String else JSON.stringify(payload)
	var error := request.request("http://127.0.0.1:%d/mcp" % AppState.bridge.port, headers, HTTPClient.METHOD_POST, body)
	if error != OK:
		request.queue_free()
		return {"code": 0, "body": {}, "headers": [], "raw": ""}
	var response: Array = await request.request_completed
	request.queue_free()
	var text: String = response[3].get_string_from_utf8()
	return {"code": response[1], "headers": response[2], "body": JSON.parse_string(text) if not text.is_empty() else {}, "raw": text}

func _test_zip(view_id: String, guard: GDScript) -> void:
	var base := OS.get_environment("DV_TEST_DIRECTORY")
	if base.is_empty(): base = "res://.godot/desktop-zip-test"
	DirAccess.make_dir_recursive_absolute(base)
	AppState.store._cache_root = base.path_join("cache")
	DirAccess.make_dir_recursive_absolute(AppState.store._cache_root)
	var path := base.path_join("model.zip")
	var packer := ZIPPacker.new()
	check(packer.open(path) == OK, "create ZIP fixture")
	packer.start_file("models/triangle.gltf")
	packer.write_file(FileAccess.get_file_as_bytes(fixture_model))
	packer.close_file()
	packer.close()
	check(not guard.inspect(path).has("error"), "ZIP central directory accepts valid model")
	var id: String = AppState.add_source(view_id, path)
	await _wait_layer(view_id, id)
	var layer: Dictionary = AppState.views[view_id].get_layer(id)
	check(layer.status == "Ready", "ZIP model extracts and imports")
	var first_cached: String = AppState.store.records[layer.data_id].source
	AppState.store.reload_data(layer.data_id)
	await _wait_layer(view_id, id)
	check(layer.status == "Ready" and AppState.store.records[layer.data_id].source != first_cached, "ZIP reload reopens original archive into a new generation")
	var cached: String = AppState.store.records[layer.data_id].source
	check(FileAccess.file_exists(cached), "ZIP uses session cache")
	AppState.remove_layer(view_id, id)
	await get_tree().process_frame
	await get_tree().process_frame
	check(not FileAccess.file_exists(cached), "last reference removes extracted files")
	var malicious := base.path_join("traversal.zip")
	packer = ZIPPacker.new()
	packer.open(malicious)
	packer.start_file("../escape.gltf")
	packer.write_file("{}".to_utf8_buffer())
	packer.close_file()
	packer.close()
	check(guard.inspect(malicious).has("error"), "ZIP traversal rejected before extraction")
	var bytes := FileAccess.get_file_as_bytes(path)
	for offset in range(bytes.size() - 46):
		if bytes.decode_u32(offset) == 0x02014b50:
			bytes.encode_u32(offset + 24, 500 * 1024 * 1024)
			break
	var bomb := base.path_join("oversize.zip")
	var file := FileAccess.open(bomb, FileAccess.WRITE)
	file.store_buffer(bytes)
	file.close()
	check(guard.inspect(bomb).has("error"), "ZIP declared expansion rejected before allocation")
	for temporary in [path, malicious, bomb]: DirAccess.remove_absolute(temporary)
