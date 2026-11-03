extends Node

var failures: Array[String] = []
var checks := 0
var app: Control


func _ready() -> void:
	app = load("res://scenes/main.tscn").instantiate()
	add_child(app)
	await get_tree().process_frame
	await get_tree().process_frame
	await _run()
	if failures.is_empty():
		print("DIGITALVIEWER_AI_OK checks=%d" % checks)
		get_tree().quit(0)
	else:
		for failure: String in failures:
			printerr("AI_TEST_FAILED: " + failure)
		get_tree().quit(1)


func check(condition: bool, description: String) -> void:
	checks += 1
	if not condition:
		failures.append(description)


func _run() -> void:
	var ai := AiAssistant
	ai.new_chat()
	AppState.set_grid_visible(true)
	await ai.submit("隐藏经纬网")
	check(not AppState.grid_visible, "demo changes rendered grid")
	check(not AppState.get_layer(&"grid").visible, "grid state and layer row agree")
	await ai.submit("显示它")
	check(AppState.grid_visible, "pronoun refers to previous target")
	var pose := AppState.camera_snapshot()
	await ai.submit("保存视点 总览")
	AppState.orbit(Vector2(0.4, 0.1))
	await ai.submit("恢复视点 总览")
	check(AppState.camera_snapshot() == pose, "restore exact user camera snapshot")
	await ai.submit("隐藏地球")
	check(not AppState.get_layer(&"globe").visible, "globe visibility updated")
	await ai.submit("撤销")
	check(AppState.get_layer(&"globe").visible, "undo restores visibility")
	ai.submit("隐藏经纬网")
	ai.stop()
	await get_tree().create_timer(0.45).timeout
	check(AppState.grid_visible, "stop discards delayed demo plan")
	check(not ai.busy, "stop unlocks composer")
	await ai.submit("飞往 0 0 20")
	check(AppState.flight_active, "fly starts asynchronously")
	ai.stop()
	var stopped := AppState.camera_snapshot()
	await get_tree().create_timer(0.15).timeout
	check(AppState.camera_snapshot() == stopped, "stopped camera stays in place")
	var service: RefCounted = ai.commands
	var revision: int = AppState.revision
	var result: Dictionary = service.execute("bad_bool", "set_grid_visible", {"visible": "false"}, revision)
	check(not result.ok and AppState.revision == revision, "invalid type is non-mutating")
	result = service.execute("bad_number", "fly_to", {"longitude": 0, "latitude": 99, "height_km": 8}, revision)
	check(not result.ok, "coordinate bounds enforced")
	result = service.execute("unknown", "execute_script", {"code": "anything"}, revision)
	check(not result.ok, "unregistered tools rejected")
	result = service.execute("fake_dem", "set_layer_visible", {"layer_id": "terrain", "visible": false}, revision)
	check(not result.ok, "unimplemented rendering not reported as success")
	result = service.execute("stale", "set_grid_visible", {"visible": false}, revision - 1)
	check(not result.ok and AppState.grid_visible, "stale revision rejected")
	service.execute("once", "set_grid_visible", {"visible": false}, revision)
	var after: int = AppState.revision
	service.execute("once", "set_grid_visible", {"visible": false}, after)
	check(AppState.revision == after, "duplicate tool ID executes only once")
	result = service.execute("once", "set_grid_visible", {"visible": true}, after)
	check(not result.ok, "same ID with different arguments rejected")
	AppState.orbit(Vector2(0.1, 0.0))
	result = service.execute("undo_old", "undo_last_action", {}, AppState.revision)
	check(not result.ok, "undo cannot overwrite manual edits")
	ai.new_chat()
	check(ai.session.transcript.is_empty() and AppState.viewpoints.has("总览"), "new chat preserves scene and viewpoints")
	ai.draft = "旋转后保留草稿"
	ai.changed.emit()
	app.size = Vector2(390, 844)
	app._update_responsive_layout()
	await get_tree().process_frame
	check(app._mobile_shell.visible, "narrow viewport selects mobile shell")
	check(ai.draft == "旋转后保留草稿", "layout change preserves draft")
	check(app._chat._input.text == ai.draft, "shared chat composer preserved")
	check(app._chat.get_rect().end.x <= app.size.x + 1.0, "chat fits mobile width")
	app.size = Vector2(844, 390)
	app._chat.set_mobile(true)
	await get_tree().process_frame
	check(app._chat.position.y >= 0 and app._chat.get_rect().end.y <= 391.0, "chat fits landscape height")
	check(ai.draft == "旋转后保留草稿", "landscape preserves composer")
	app.size = Vector2(390, 844)
	app._chat.set_mobile(true)
	var desktop_world: DigitalViewerViewport = app._desktop_shell.find_children("*", "DigitalViewerViewport", true, false)[0]
	var mobile_world: DigitalViewerViewport = app._mobile_shell.find_children("*", "DigitalViewerViewport", true, false)[0]
	await get_tree().process_frame
	check(desktop_world._earth_pivot.rotation.is_equal_approx(mobile_world._earth_pivot.rotation), "desktop/mobile show shared camera")
	check(desktop_world._grid_mesh_instance.visible == AppState.grid_visible, "renderer reflects command state")
	var session = load("res://scripts/ai/chat_session.gd").new()
	for index: int in 20:
		session.begin("消息 %d" % index)
		session.active.append({"role": "assistant", "content": null, "tool_calls": [{"id": str(index)}]})
		session.active.append({"role": "tool", "tool_call_id": str(index), "content": "{}"})
		session.remember("fly_to", {"status": "started", "message": "尚未到达"})
		session.finish("测试回复")
	check(session.turns.size() == 8 and session.archived_turns == 12, "whole-turn context compaction")
	check(session.archived_requests.size() == 6, "bounded archived request excerpts retained")
	for turn: Array in session.turns:
		check(turn[1].tool_calls[0].id == turn[2].tool_call_id, "call/result pair preserved")
	check("started" in session.notes.back(), "summary never upgrades started to completed")
	session.begin("长".repeat(1800))
	session.finish("长".repeat(12000))
	check(JSON.stringify(session.turns).to_utf8_buffer().size() <= session.MAX_HISTORY_BYTES, "history byte budget bounded")
	await preload("res://tests/runtime_checks.gd").new().run(self, app)
	var endpoint := ""
	for argument: String in OS.get_cmdline_user_args():
		if argument.begins_with("--test-endpoint="):
			endpoint = argument.trim_prefix("--test-endpoint=")
	if not endpoint.is_empty():
		await _http_checks(endpoint)
	if "--capture" in OS.get_cmdline_user_args():
		app.size = get_viewport().get_visible_rect().size
		app._update_responsive_layout()
		app._chat.set_mobile(app._using_mobile_layout)
		AppState.restore_camera({"rotation_x": 0.25, "rotation_y": -0.4, "distance": 7.0, "heading": 0.0, "pitch": -35.0})
		ai.configure(true, ai.client.endpoint, "", "")
		await ai.submit("隐藏经纬网")
		await ai.submit("显示它")
		var capture_name := "ai-chat"
		if "--capture-runtime" in OS.get_cmdline_user_args():
			await ai.submit("地球变红并自转")
			await ai.submit("创建彩色圆环")
			await get_tree().create_timer(0.4).timeout
			capture_name = "ai-runtime"
			if "--capture-program" in OS.get_cmdline_user_args():
				app._chat._show_last_program()
				app._chat._tabs.current_tab = 3
				capture_name = "ai-runtime-program"
		if "--capture-connection" in OS.get_cmdline_user_args() and not endpoint.is_empty():
			app._chat._mode.select(1)
			app._chat._endpoint.text = endpoint + "/error"
			app._chat._model.text = "fixture-tools"
			app._chat._key.text = ""
			app._chat._tabs.current_tab = 1
			await app._chat._test_connection()
			capture_name = "ai-connection"
		await RenderingServer.frame_post_draw
		var image := get_viewport().get_texture().get_image()
		var suffix := "mobile" if get_viewport().get_visible_rect().size.x < 1100 else "desktop"
		image.save_png("res://design/%s-%s.png" % [capture_name, suffix])


func _http_checks(base: String) -> void:
	var ai := AiAssistant
	var panel = app._chat
	check(ai.client.normalized_endpoint("http://example.test:3002/v1/") == "http://example.test:3002/v1/chat/completions", "v1 base URL normalized")
	check(ai.client.normalized_endpoint("https://example.test") == "https://example.test/v1/chat/completions", "host-only URL normalized")
	check(ai.configure(false, "http://example.test:3002/v1", "fixture-tools", ""), "remote HTTP gateway allowed")
	check(not ai.configure(false, "http: example.test:3002/v1", "fixture-tools", ""), "malformed HTTP scheme explained before sending")
	check("http://" in ai.status_text, "invalid URL has actionable feedback")
	check(not ai.configure(false, base + "/v1/responses", "fixture-tools", ""), "different API protocol rejected explicitly")
	ai.configure(true, base + "/v1", "", "")
	panel._mode.select(0)
	panel._endpoint.text = base + "/v1/"
	panel._model.text = ""
	await panel._test_connection()
	check(not ai.busy and "模型名称" in panel._connection_status.text, "connect from demo validates missing model instead of refusing to connect")
	ai.configure(true, base + "/v1", "", "")
	panel._mode.select(0)
	panel._model.text = "fixture-tools"
	panel._key.text = "test-only-key"
	panel._tabs.current_tab = 1
	await panel._test_connection()
	check(not ai.demo_mode and panel._mode.selected == 1, "connect button switches demo to real model automatically")
	check(ai.client.model == "fixture-tools" and ai.client.api_key == "test-only-key", "switching mode retains entered model and credential")
	check(not ai.busy and "成功" in panel._connection_status.text, "connection UI applies and tests a base URL")
	check(panel._tabs.current_tab == 1 and panel._connection_status.is_visible_in_tree(), "test result remains visible in connection tab")
	check(panel._endpoint.text == base + "/v1/chat/completions", "resolved endpoint shown in settings")
	panel._endpoint.text = base + "/error"
	await panel._test_connection()
	check("401" in panel._connection_status.text and "API Key" in panel._connection_status.text, "auth error displayed in connection tab")
	check("test-only-key" not in JSON.stringify(ai.session.transcript), "provider errors cannot leak credentials into history")
	panel._endpoint.text = base + "/slow"
	panel._test_connection()
	await get_tree().create_timer(0.15).timeout
	check(ai.busy and "已等待" in panel._connection_status.text and panel._connection_stop.visible, "waiting request has progress and a stop button")
	ai.stop()
	check(not ai.busy and "已停止" in panel._connection_status.text and not panel._test_button.disabled, "stopping a connection test restores UI")
	await get_tree().create_timer(1.4).timeout
	check("已停止" in panel._connection_status.text, "late test response cannot overwrite stopped status")
	ai.configure(false, base + "/timeout", "fixture-tools", "", {"timeout_seconds": 0.15})
	await ai.test_connection()
	check(not ai.busy and "超时" in ai.status_text, "timeout reports actionable failure and unlocks UI")
	for entry: Array in [["empty", "空消息"], ["thinking", "思考内容"], ["truncated", "预算耗尽"], ["wrong_protocol", "choices"]]:
		ai.configure(false, base + "/" + entry[0], "fixture-tools", "")
		await ai.test_connection()
		check(not ai.busy and entry[1] in ai.status_text and "成功" not in ai.status_text, "invalid response cannot pass connection test: " + entry[0])
	ai.configure(false, base + "/defaults", "fixture-tools", "")
	await ai.test_connection()
	check("成功" in ai.status_text, "default omits provider-specific token-limit fields")
	ai.configure(false, base + "/modern", "fixture-tools", "", {"token_parameter": "max_tokens"})
	await ai.test_connection()
	check("输出预算参数" in ai.status_text, "unsupported token field explains how to recover")
	ai.configure(false, base + "/modern", "fixture-tools", "", {"token_parameter": "max_completion_tokens"})
	await ai.test_connection()
	check("成功" in ai.status_text, "modern completion-token limit supported")
	AppState.set_grid_visible(true)
	ai.configure(false, base + "/reasoning", "fixture-tools", "")
	await ai.submit("隐藏经纬网")
	check(not AppState.grid_visible and ai.session.transcript.back().text == "操作已完成。", "reasoning field preserved for tool continuation")
	panel._tabs.current_tab = 0
	AppState.set_grid_visible(true)
	ai.configure(false, base + "/normal", "fixture-tools", "test-only-key")
	await ai.submit("隐藏经纬网")
	check(not AppState.grid_visible and not ai.busy, "HTTP multi-round tool loop executes")
	check(ai.session.turns.back().size() == 6, "wire history preserves both tool result pairs")
	ai.configure(false, base + "/badtool", "fixture-tools", "")
	var revision: int = AppState.revision
	await ai.submit("无效格式")
	check(AppState.revision == revision and not ai.busy, "malformed tool result is non-mutating")
	for route: String in ["error", "badjson", "unknown"]:
		ai.configure(false, base + "/" + route, "fixture-tools", "")
		await ai.submit("测试错误")
		check(not ai.busy, "HTTP error unlocks input: " + route)
	ai.configure(false, base + "/delayed", "fixture-tools", "")
	AppState.set_grid_visible(true)
	ai.submit("隐藏经纬网")
	await get_tree().create_timer(0.08).timeout
	ai.stop()
	await get_tree().create_timer(0.6).timeout
	check(AppState.grid_visible and not ai.busy, "cancel HTTP drops late mutation")
	ai.configure(false, base + "/delayed", "fixture-tools", "")
	ai.submit("隐藏经纬网")
	await get_tree().create_timer(0.08).timeout
	AppState.orbit(Vector2(0.01, 0.0))
	await get_tree().create_timer(0.6).timeout
	check(AppState.grid_visible and not ai.busy, "manual edit invalidates inflight response")
	ai.configure(false, base + "/normal", "fixture-tools", "")
	await ai.test_connection()
	check(not ai.busy and "成功" in ai.session.transcript.back().text, "connection test has no scene tools")
	ai.configure(false, base + "/loop", "fixture-tools", "")
	await ai.submit("循环测试")
	check(not ai.busy and "上限" in ai.session.transcript.back().text, "tool loop is bounded")
	var saved: Dictionary = AppState.runtime.snapshot()
	ai.configure(false, base + "/runtime", "fixture-tools", "test-only-key")
	await ai.submit("把地球变红并持续自转")
	check(not ai.busy and "验证通过" in ai.session.transcript.back().text, "model discovers language, repairs failed program, executes and verifies")
	check(AppState.runtime.objects.globe.props["material.albedo_color"] == "#ee3344ff" and AppState.runtime.behaviors.has("model_spin"), "HTTP model program affects live scene after chat completes")
	AppState.runtime.restore_snapshot(saved)
	ai.new_chat()
