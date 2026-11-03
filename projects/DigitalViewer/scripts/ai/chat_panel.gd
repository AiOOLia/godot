extends PanelContainer

var _history: RichTextLabel
var _input: TextEdit
var _send: Button
var _stop: Button
var _status: Label
var _context: TextEdit
var _mode: OptionButton
var _endpoint: LineEdit
var _model: LineEdit
var _key: LineEdit
var _timeout: SpinBox
var _token_parameter: OptionButton
var _output_tokens: SpinBox
var _connection_status: Label
var _test_button: Button
var _connection_stop: Button
var _tabs: TabContainer
var _is_mobile := false
var _last_keyboard := -1
var _examples: Control
var _title: Label
var _program: TextEdit
var _runtime_status: Label


func _ready() -> void:
	name = "AIChatPanel"
	var style := StyleBoxFlat.new()
	style.bg_color = Color("#14232c")
	style.border_color = Color("#32505c")
	style.set_border_width_all(1)
	style.set_corner_radius_all(12)
	add_theme_stylebox_override("panel", style)
	var margin := MarginContainer.new()
	for side: String in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 12)
	add_child(margin)
	var root := VBoxContainer.new()
	root.add_theme_constant_override("separation", 10)
	margin.add_child(root)
	var header := HBoxContainer.new()
	root.add_child(header)
	var title := Label.new()
	_title = title
	title.text = "AI 场景助手"
	title.add_theme_font_size_override("font_size", 19)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	var fresh := _button("新聊天", AiAssistant.new_chat)
	fresh.tooltip_text = "清空聊天与撤销记录，保留场景和会话视点"
	header.add_child(fresh)
	header.add_child(_button("×", hide))
	_status = Label.new()
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.add_theme_color_override("font_color", Color("#65d4b8"))
	root.add_child(_status)
	_tabs = TabContainer.new()
	_tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root.add_child(_tabs)
	var conversation := VBoxContainer.new()
	conversation.name = "对话"
	conversation.add_theme_constant_override("separation", 8)
	_tabs.add_child(conversation)
	var examples := HFlowContainer.new()
	_examples = examples
	conversation.add_child(examples)
	for example: Array in [["变红自转", "地球变红并自转"], ["创建圆环", "创建彩色圆环"], ["恢复场景", "恢复场景"], ["隐藏经纬网", "隐藏经纬网"]]:
		var text: String = example[1]
		examples.add_child(_button(example[0], func() -> void: _input.text = text; AiAssistant.draft = text; _input.grab_focus()))
	_history = RichTextLabel.new()
	_history.name = "History"
	_history.bbcode_enabled = false
	_history.selection_enabled = true
	_history.scroll_following = true
	_history.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_history.custom_minimum_size.y = 90
	conversation.add_child(_history)
	_input = TextEdit.new()
	_input.name = "MessageInput"
	_input.placeholder_text = "输入指令…  Ctrl+Enter 发送"
	_input.custom_minimum_size.y = 76
	_input.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	_input.text_changed.connect(func() -> void: AiAssistant.draft = _input.text)
	_input.gui_input.connect(_input_key)
	conversation.add_child(_input)
	var actions := HBoxContainer.new()
	conversation.add_child(actions)
	_stop = _button("停止", AiAssistant.stop)
	_stop.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_stop.tooltip_text = "取消请求、停止飞行和所有持续行为；场景修改保留，可撤销"
	actions.add_child(_stop)
	_send = _button("发送", _submit)
	_send.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	actions.add_child(_send)
	var connection_page := VBoxContainer.new()
	connection_page.name = "连接"
	_tabs.add_child(connection_page)
	_mode = OptionButton.new()
	_mode.add_item("规则演示 · 无需联网")
	_mode.add_item("真实模型 · 兼容工具调用接口")
	_mode.select(0 if AiAssistant.demo_mode else 1)
	_mode.custom_minimum_size.y = 44
	connection_page.add_child(_mode)
	_connection_status = Label.new()
	_connection_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_connection_status.add_theme_font_size_override("font_size", 14)
	_connection_status.add_theme_color_override("font_color", Color("#65d4b8"))
	connection_page.add_child(_connection_status)
	_connection_stop = _button("停止连接请求", AiAssistant.stop)
	connection_page.add_child(_connection_stop)
	var settings_scroll := ScrollContainer.new()
	settings_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	settings_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	connection_page.add_child(settings_scroll)
	var settings := VBoxContainer.new()
	settings.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	settings.add_theme_constant_override("separation", 8)
	settings_scroll.add_child(settings)
	_label(settings, "填写连接信息后点击「连接并测试」，自动切换到真实模型。设置仅在本次运行有效。")
	_endpoint = _field(settings, "请求地址（支持基础地址 /v1 或完整接口）", AiAssistant.client.endpoint)
	_model = _field(settings, "模型名称", "")
	_model.placeholder_text = "服务端已部署、支持 tools 的模型"
	_key = _field(settings, "访问凭据（可留空，本次会话有效）", "")
	_key.secret = true
	_timeout = _number(settings, "请求超时（秒）", 5, 600, AiAssistant.client.timeout_seconds)
	_label(settings, "输出预算参数（不确定时保留服务默认）")
	_token_parameter = OptionButton.new()
	for entry: Array in [["服务默认", ""], ["max_completion_tokens", "max_completion_tokens"], ["max_tokens", "max_tokens"]]:
		_token_parameter.add_item(entry[0])
		_token_parameter.set_item_metadata(_token_parameter.item_count - 1, entry[1])
	settings.add_child(_token_parameter)
	_output_tokens = _number(settings, "最大输出 Token（仅指定参数时生效）", 256, 65536, AiAssistant.client.output_tokens)
	_output_tokens.editable = false
	_token_parameter.item_selected.connect(func(index: int) -> void: _output_tokens.editable = index != 0)
	_label(settings, "支持 Chat Completions 的 tools / tool_calls 格式。云服务建议经自有 HTTPS 网关访问；Web 需要网关允许当前来源。")
	_label(settings, "仅发送聊天与精简场景信息，不上传模型文件。切换连接将新建聊天。")
	var connection_actions := HBoxContainer.new()
	connection_page.add_child(connection_actions)
	var apply := _button("应用设置", _apply_settings)
	apply.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	connection_actions.add_child(apply)
	_test_button = _button("连接并测试", _test_connection)
	_test_button.tooltip_text = "应用已填写的连接信息，切换到真实模型并发送测试请求"
	_test_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	connection_actions.add_child(_test_button)
	var context_page := VBoxContainer.new()
	context_page.name = "上下文"
	_tabs.add_child(context_page)
	context_page.add_child(_button("刷新场景上下文", _refresh_context))
	_context = TextEdit.new()
	_context.editable = false
	_context.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	_context.size_flags_vertical = Control.SIZE_EXPAND_FILL
	context_page.add_child(_context)
	_build_runtime_page()
	_tabs.tab_changed.connect(func(_index: int) -> void: _refresh_context(); _layout())
	AiAssistant.changed.connect(_render)
	AiAssistant.status_changed.connect(_render_status)
	AppState.runtime.activity_changed.connect(_render_runtime)
	get_parent().resized.connect(_layout)
	_render()
	_layout()
	_render_runtime()


func _build_runtime_page() -> void:
	var page := VBoxContainer.new()
	page.name = "场景程序"
	_tabs.add_child(page)
	_runtime_status = Label.new()
	_runtime_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_runtime_status.max_lines_visible = 3
	_runtime_status.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_runtime_status.add_theme_font_size_override("font_size", 13)
	page.add_child(_runtime_status)
	var examples := OptionButton.new()
	examples.add_item("地球变红并自转")
	examples.add_item("循环创建彩色圆环")
	examples.add_item("恢复场景")
	examples.custom_minimum_size.y = 44
	page.add_child(examples)
	examples.item_selected.connect(_load_program_example)
	_program = TextEdit.new()
	_program.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	_program.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_program.add_theme_font_size_override("font_size", 13)
	page.add_child(_program)
	var buttons := HBoxContainer.new()
	page.add_child(buttons)
	for entry: Array in [["运行", _run_program], ["查看最近程序", _show_last_program], ["撤销", _undo_program]]:
		var button := _button(entry[0], entry[1])
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		buttons.add_child(button)
	page.add_child(_button("停止所有行为", AiAssistant.stop))
	_load_program_example(0)


func _load_program_example(index: int) -> void:
	var examples = preload("res://scripts/runtime/scene_examples.gd")
	var program: Array = [examples.red_spin(), examples.orbit_ring(), examples.restore_original()][index]
	_program.text = JSON.stringify(program, "  ")


func _run_program() -> void:
	if AiAssistant.busy:
		_runtime_status.text = "请先等待或停止当前聊天请求。"
		return
	var result: Dictionary = AiAssistant.commands.execute("ui/%d" % Time.get_ticks_usec(), "scene_run", {"program_json": _program.text}, AppState.revision)
	AiAssistant._record("scene_run", result)
	AiAssistant.changed.emit()
	_runtime_status.text = result.message


func _undo_program() -> void:
	if AiAssistant.busy: return
	var result: Dictionary = AiAssistant.commands.execute("ui/%d" % Time.get_ticks_usec(), "undo_last_action", {}, AppState.revision)
	AiAssistant._record("undo_last_action", result)
	AiAssistant.changed.emit()
	_runtime_status.text = result.message


func _show_last_program() -> void:
	_program.text = JSON.stringify(AppState.runtime.last_program, "  ")


func _render_runtime() -> void:
	var names: Array[String] = []
	for job: Dictionary in AppState.runtime.behavior_summary():
		names.append(job.id + ("（暂停）" if job.status == "paused" else "（运行）"))
	_runtime_status.text = "%s\n%s" % [AppState.runtime.last_result, "、".join(names) if not names.is_empty() else "无持续行为"]
	_runtime_status.tooltip_text = _runtime_status.text
	_render_status()


func _process(_delta: float) -> void:
	if not DisplayServer.has_feature(DisplayServer.FEATURE_VIRTUAL_KEYBOARD):
		return
	var keyboard := DisplayServer.virtual_keyboard_get_height()
	if keyboard != _last_keyboard:
		_last_keyboard = keyboard
		_layout()


func set_mobile(value: bool) -> void:
	_is_mobile = value
	_layout()


func _layout() -> void:
	if not is_inside_tree():
		return
	var available: Vector2 = get_parent().size
	var inset_top := 0.0
	var inset_bottom := 0.0
	if OS.has_feature("mobile"):
		var safe := DisplayServer.get_display_safe_area()
		var screen := DisplayServer.screen_get_size()
		if screen.y > 0:
			inset_top = safe.position.y * available.y / screen.y
			inset_bottom = maxf(screen.y - safe.end.y, _last_keyboard) * available.y / screen.y
	var width := minf(410.0, available.x - 16.0)
	var compact := available.y - inset_top - inset_bottom < 600.0
	_examples.visible = not compact
	_status.visible = not compact and _tabs.current_tab != 1
	_runtime_status.max_lines_visible = 1 if compact else 3
	_title.text = ("AI · 演示" if AiAssistant.demo_mode else "AI · 模型") if compact else "AI 场景助手"
	_input.custom_minimum_size.y = 48 if compact else 76
	_history.custom_minimum_size.y = 24 if compact else 90
	var height := available.y - (16.0 if compact else 110.0) - inset_top - inset_bottom
	if _is_mobile:
		width = available.x - 16.0
		if not compact:
			height = minf(height, maxf(420.0, available.y * 0.70))
	size = Vector2(width, height)
	position = Vector2(available.x - width - 8.0, available.y - height - (8.0 if compact else 46.0) - inset_bottom)


func _render() -> void:
	if _history == null:
		return
	_render_status()
	_history.clear()
	if AiAssistant.session.transcript.is_empty():
		_history.append_text("可以直接修改运行中的场景。\n\n先试试：地球变红并自转 → 停止自转 → 恢复场景\n或：创建彩色圆环 → 撤销\n\n演示模式仅识别这些示例；配置真实模型后可自由描述。场景程序页可查看和运行程序。")
	for entry: Dictionary in AiAssistant.session.transcript:
		_history.append_text("%s\n%s\n\n" % [entry.role, entry.text])
	if _input.text != AiAssistant.draft:
		_input.text = AiAssistant.draft
	_send.disabled = AiAssistant.busy
	_refresh_context()
	_layout()


func _render_status() -> void:
	_status.text = ("规则演示 · 不调用大模型" if AiAssistant.demo_mode else "真实模型 · " + str(AiAssistant.client.model)) + "\n" + AiAssistant.status_text
	if not AppState.runtime.behaviors.is_empty():
		_status.text += "\n%d 个持续行为 · 点击停止可接回控制" % AppState.runtime.behaviors.size()
	_connection_status.text = AiAssistant.status_text
	_connection_stop.visible = AiAssistant.busy
	_test_button.disabled = AiAssistant.busy


func _refresh_context() -> void:
	if _context == null:
		return
	_context.text = "保留最近 %d 个完整轮次；已移除 %d 个旧轮次。\n历史采用 UTF-8 字节预算，工具调用与结果成组保留。\n旧请求摘录（不代表已完成）：\n%s\n\n最近操作事实（含 started / failed）：\n%s\n\n实时场景：\n%s" % [AiAssistant.session.turns.size(), AiAssistant.session.archived_turns, "\n".join(AiAssistant.session.archived_requests), "\n".join(AiAssistant.session.notes), JSON.stringify(AiAssistant.commands.context(), "  ")]


func _submit() -> void:
	AiAssistant.submit(_input.text)


func _input_key(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.ctrl_pressed and event.keycode == KEY_ENTER:
		_submit()
		_input.accept_event()


func _apply_settings() -> bool:
	var applied := AiAssistant.configure(_mode.selected == 0, _endpoint.text, _model.text, _key.text, {
		"timeout_seconds": _timeout.value,
		"token_parameter": _token_parameter.get_selected_metadata(),
		"output_tokens": int(_output_tokens.value),
	})
	if applied:
		_endpoint.text = AiAssistant.client.endpoint
	return applied


func _test_connection() -> void:
	if AiAssistant.busy:
		return
	# Clicking Connect explicitly chooses the real service, even from demo mode.
	_mode.select(1)
	if _apply_settings():
		await AiAssistant.test_connection()


func _button(text: String, action: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size.y = 44
	button.pressed.connect(action)
	return button


func _field(parent: Control, caption: String, value: String) -> LineEdit:
	_label(parent, caption)
	var field := LineEdit.new()
	field.text = value
	field.custom_minimum_size.y = 44
	field.add_theme_font_size_override("font_size", 16)
	parent.add_child(field)
	return field


func _number(parent: Control, caption: String, minimum: float, maximum: float, value: float) -> SpinBox:
	_label(parent, caption)
	var field := SpinBox.new()
	field.min_value = minimum
	field.max_value = maximum
	field.value = value
	field.custom_minimum_size.y = 44
	parent.add_child(field)
	return field


func _label(parent: Control, text: String) -> void:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	parent.add_child(label)
