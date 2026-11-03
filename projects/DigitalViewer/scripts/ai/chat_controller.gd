extends Node

signal changed
signal status_changed
const Session := preload("res://scripts/ai/chat_session.gd")
const Commands := preload("res://scripts/commands/scene_command_service.gd")
const Client := preload("res://scripts/ai/model_client.gd")
const Demo := preload("res://scripts/ai/demo_planner.gd")

var session := Session.new()
var commands: RefCounted
var client: Node
var demo_mode := true
var busy := false
var draft := ""
var status_text := "规则演示 · 无需联网"
var _generation := 0


func _ready() -> void:
	commands = Commands.new(AppState)
	client = Client.new()
	add_child(client)
	client.progress_changed.connect(_set_status)
	AppState.flight_finished.connect(_flight_finished)
	AppState.runtime.behavior_finished.connect(_behavior_finished)
	AppState.selected_layer_changed.connect(func(layer_id: StringName) -> void: commands.last_target = str(layer_id))


func configure(use_demo: bool, endpoint: String, model: String, key: String, options: Dictionary = {}) -> bool:
	stop()
	demo_mode = use_demo
	client.endpoint = client.normalized_endpoint(endpoint)
	client.model = model.strip_edges()
	client.api_key = key.strip_edges()
	client.timeout_seconds = options.get("timeout_seconds", 90.0)
	client.token_parameter = options.get("token_parameter", "")
	client.output_tokens = options.get("output_tokens", 4096)
	# Provider changes do not send the previous provider's conversation to a new host.
	new_chat()
	var invalid: String = "" if use_demo else client.configuration_error()
	if not invalid.is_empty():
		_set_status(invalid)
		session.display("系统", invalid)
		changed.emit()
		return false
	_set_status("规则演示 · 无需联网" if use_demo else "连接设置已应用，尚未测试。")
	session.display("系统", "已切换到规则演示。" if use_demo else "已配置模型连接。设置仅保存在本次运行内存中。")
	changed.emit()
	return true


func new_chat() -> void:
	stop()
	session.clear()
	commands.reset_chat()
	draft = ""
	changed.emit()


func test_connection() -> void:
	if busy:
		return
	if demo_mode:
		_set_status("演示模式无需连接。请选择真实模型并填写地址与模型。")
		return
	_generation += 1
	var ticket := _generation
	busy = true
	_set_status("正在测试模型连接…")
	session.begin("测试模型连接")
	changed.emit()
	var result: Dictionary = await client.complete([{"role": "user", "content": "只回复 OK，不执行操作。"}], [])
	if ticket != _generation:
		return
	var answer: String = result.get("error", "模型连接成功，已收到有效回复。可到对话页发送「隐藏经纬网」测试工具调用。")
	_finish(answer, ticket, answer)


func stop() -> void:
	_generation += 1
	client.cancel()
	AppState.cancel_flight()
	if commands.stop_behaviors():
		_set_status("持续行为已停止，场景保留，可撤销上一步程序。")
		session.display("场景", "已停止所有持续行为。")
	if busy:
		busy = false
		_set_status("已停止。可以修改设置或重新发送。")
		session.finish("已停止。本轮已经完成的操作保留，可使用「撤销」。")
	changed.emit()


func submit(text: String) -> void:
	var prompt := text.strip_edges()
	if busy or prompt.is_empty():
		return
	if prompt.length() > 2000:
		session.display("系统", "单条消息最多 2000 字符，请缩短后重试。")
		changed.emit()
		return
	_generation += 1
	var ticket := _generation
	draft = ""
	busy = true
	_set_status("正在处理，可随时停止…")
	session.begin(prompt)
	changed.emit()
	var scene: Dictionary = commands.context()
	if demo_mode:
		# A real cancellable async boundary lets users exercise Stop even offline.
		await get_tree().create_timer(0.35).timeout
		if ticket != _generation:
			return
		if scene.revision != AppState.revision:
			_finish("等待期间场景已变化，请重新发送指令。", ticket)
			return
		var plan: Dictionary = Demo.new().plan(prompt, scene)
		if plan.has("answer"):
			_finish(plan.answer, ticket)
			return
		var results: Array[String] = []
		for index: int in plan.calls.size():
			var call: Dictionary = plan.calls[index]
			var result: Dictionary = commands.execute("%d/demo/%d" % [ticket, index], call.name, call.args, AppState.revision)
			_record(call.name, result)
			results.append(_result_text(result))
			if not result.ok:
				break
		_finish("\n".join(results), ticket)
		return
	for round_index: int in 6:
		scene = commands.context()
		var response: Dictionary = await client.complete(session.messages(scene), commands.tools())
		if ticket != _generation:
			return
		if response.has("error"):
			_finish(response.error, ticket, response.error)
			return
		var message: Dictionary = response.message
		var tool_calls: Variant = message.get("tool_calls", [])
		if tool_calls == null:
			tool_calls = []
		if not tool_calls is Array:
			_finish("工具调用格式错误，未执行。", ticket)
			return
		if tool_calls.is_empty():
			var answer: Variant = message.get("content")
			_finish(answer if answer is String and not answer.is_empty() else "模型未返回文字或工具调用。", ticket)
			return
		if scene.revision != AppState.revision:
			_finish("模型响应期间场景已变化，已拒绝旧指令。请重新发送。", ticket)
			return
		var parsed := _parse_calls(tool_calls)
		if parsed.has("error"):
			_finish(parsed.error, ticket)
			return
		var assistant_turn := {"role": "assistant", "content": message.get("content"), "tool_calls": parsed.wire}
		# Some reasoning providers require this field on the tool-result continuation.
		if message.get("reasoning_content") is String:
			assistant_turn["reasoning_content"] = message.reasoning_content
		session.active.append(assistant_turn)
		var abort_reason := ""
		var can_repair := false
		for call: Dictionary in parsed.calls:
			var result: Dictionary
			if abort_reason.is_empty():
				result = commands.execute("%d/%s" % [ticket, call.id], call.name, call.args, AppState.revision)
			else:
				result = {"ok": false, "status": "skipped", "message": "前一步失败，本批后续操作已跳过。"}
			session.active.append({"role": "tool", "tool_call_id": call.id, "content": JSON.stringify(result)})
			_record(call.name, result)
			if not result.ok:
				abort_reason = result.message
				can_repair = call.name == "scene_run" or can_repair
		changed.emit()
		if not abort_reason.is_empty():
			if can_repair:
				continue
			_finish(abort_reason, ticket)
			return
	_finish("已达到本轮工具调用上限，执行结果见上方记录。请发送下一条消息继续。", ticket)


func _parse_calls(raw: Array) -> Dictionary:
	if raw.size() > 8:
		return {"error": "单次返回超过 8 个工具调用，已拒绝执行。"}
	var parsed: Array = []
	var wire: Array = []
	var seen := {}
	for item: Variant in raw:
		if not item is Dictionary or not item.get("id") is String or not item.get("function") is Dictionary:
			return {"error": "工具调用结构无效。"}
		var id: String = item.id
		var function: Dictionary = item.function
		if id.is_empty() or id.length() > 100 or seen.has(id) or not function.get("name") is String or not function.get("arguments") is String:
			return {"error": "工具调用标识或参数格式无效。"}
		seen[id] = true
		var parser := JSON.new()
		if parser.parse(function.arguments) != OK:
			return {"error": "工具参数不是有效 JSON 对象。"}
		var args: Variant = parser.data
		if not args is Dictionary:
			return {"error": "工具参数不是有效 JSON 对象。"}
		parsed.append({"id": id, "name": function.name, "args": args})
		wire.append({"id": id, "type": "function", "function": {"name": function.name, "arguments": function.arguments}})
	return {"calls": parsed, "wire": wire}


func _record(tool_name: String, result: Dictionary) -> void:
	session.remember(tool_name, result)
	var captions := {"scene_inspect": "观察场景", "scene_language": "读取编程能力", "scene_run": "执行场景程序", "get_scene_state": "读取场景", "select_layer": "选择图层", "set_layer_visible": "图层显隐", "set_grid_visible": "经纬网", "fly_to": "坐标飞行", "cancel_flight": "停止飞行", "save_viewpoint": "保存视点", "restore_viewpoint": "恢复视点", "undo_last_action": "撤销"}
	session.display("操作 · " + str(captions.get(tool_name, "执行结果")), str(result.get("message", "")))


func _result_text(result: Dictionary) -> String:
	if result.has("data") and result.data.has("layers"):
		var scene: Dictionary = result.data
		var names: Array[String] = []
		for layer: Dictionary in scene.layers:
			names.append("%s（%s%s）" % [layer.name, "显示" if layer.visible else "隐藏", "" if layer.rendered else "，仅示例状态"])
		return "\n".join(names) + "\n经度 %.3f°，纬度 %.3f°，预览高度 %.2f km。\n会话视点：%s" % [scene.camera.longitude, scene.camera.latitude, scene.camera.height_km, ", ".join(scene.viewpoints)]
	return result.message


func _finish(answer: String, ticket: int, status: String = "本轮处理完成。") -> void:
	if ticket != _generation:
		return
	busy = false
	_set_status(status)
	session.finish(answer.left(12000))
	changed.emit()


func _set_status(value: String) -> void:
	status_text = value
	status_changed.emit()


func _flight_finished(message: String) -> void:
	if session.transcript.is_empty():
		return
	session.display("场景", message)
	session.remember("flight", {"status": "completed" if "完成" in message else "cancelled", "message": message})
	changed.emit()


func _behavior_finished(id: String, status: String, message: String) -> void:
	session.display("场景", message)
	session.remember("behavior:" + id, {"status": status, "message": message})
	changed.emit()
