extends Node
## Non-streaming Chat Completions transport, with visible progress and bounded requests.
## All configuration stays in RAM. Provider errors/bodies are never logged.

signal progress_changed(message: String)

var endpoint := "http://127.0.0.1:11434/v1/chat/completions"
var model := ""
var api_key := ""
var timeout_seconds := 90.0
# Service default also works with providers that reject either token-limit parameter.
var token_parameter := ""
var output_tokens := 4096
var _pending: HTTPRequest
var _started_at := 0
var _progress := ""


func normalized_endpoint(value: String) -> String:
	var url := value.strip_edges()
	while url.ends_with("/"):
		url = url.trim_suffix("/")
	if url.ends_with("/v1"):
		return url + "/chat/completions"
	var scheme_end := url.find("://")
	if scheme_end >= 0 and url.find("/", scheme_end + 3) == -1:
		return url + "/v1/chat/completions"
	return url


func configuration_error() -> String:
	var url := normalized_endpoint(endpoint)
	if not (url.begins_with("https://") or url.begins_with("http://")):
		return "请求地址必须以 https:// 或 http:// 开头，例如 http://主机:端口/v1。"
	var authority := url.substr(url.find("://") + 3).get_slice("/", 0)
	if authority.is_empty() or "@" in authority or "?" in url or "#" in url or " " in url or "\n" in url or "\r" in url or "\t" in url:
		return "请求地址无效：请填写基础地址或完整接口地址，不包含空格、凭据或查询参数。"
	if url.ends_with("/responses") or url.ends_with("/messages") or url.ends_with("/api/chat"):
		return "当前支持 Chat Completions 协议，请使用服务的 /v1/chat/completions 兼容接口。"
	if model.strip_edges().is_empty():
		return "请先填写服务端实际支持的模型名称。"
	if "\n" in api_key or "\r" in api_key:
		return "访问凭据包含无效换行字符。"
	if token_parameter not in ["", "max_tokens", "max_completion_tokens"]:
		return "不支持的输出预算参数。"
	if not is_finite(timeout_seconds) or timeout_seconds <= 0 or output_tokens <= 0:
		return "请求超时和输出预算必须大于零。"
	return ""


func _process(_delta: float) -> void:
	if not is_instance_valid(_pending):
		return
	var elapsed := (Time.get_ticks_msec() - _started_at) / 1000.0
	var stage := "等待模型回复"
	match _pending.get_http_client_status():
		HTTPClient.STATUS_RESOLVING:
			stage = "解析服务地址"
		HTTPClient.STATUS_CONNECTING:
			stage = "连接模型服务"
		HTTPClient.STATUS_BODY:
			stage = "接收回复（%d 字节）" % _pending.get_downloaded_bytes()
	_report_progress("%s · 已等待 %d / %d 秒，可停止" % [stage, int(elapsed), int(ceil(timeout_seconds))])


func _report_progress(value: String) -> void:
	if _progress != value:
		_progress = value
		progress_changed.emit(value)


func cancel() -> void:
	if is_instance_valid(_pending):
		var request := _pending
		_pending = null
		request.cancel_request()
		# Wake the awaiting coroutine before freeing its request node.
		request.request_completed.emit(HTTPRequest.RESULT_REQUEST_FAILED, 0, PackedStringArray(), PackedByteArray())


func complete(messages: Array, tools: Array) -> Dictionary:
	var invalid := configuration_error()
	if not invalid.is_empty():
		return {"error": invalid}
	if is_instance_valid(_pending):
		return {"error": "已有模型请求正在处理，请等待或停止。"}
	var payload := {"model": model, "messages": messages, "stream": false}
	if not token_parameter.is_empty():
		payload[token_parameter] = output_tokens
	if not tools.is_empty():
		payload["tools"] = tools
		payload["tool_choice"] = "auto"
	var body := JSON.stringify(payload)
	if body.to_utf8_buffer().size() > 52000:
		return {"error": "本轮上下文超过请求预算，请新建聊天或缩短消息。"}
	var request := HTTPRequest.new()
	request.timeout = timeout_seconds
	request.body_size_limit = 256 * 1024
	request.max_redirects = 0
	add_child(request)
	_pending = request
	_started_at = Time.get_ticks_msec()
	_report_progress("正在发送模型请求 · 可停止")
	var headers := PackedStringArray(["Content-Type: application/json", "Accept: application/json"])
	if not api_key.is_empty():
		headers.append("Authorization: Bearer " + api_key)
	var error := request.request(normalized_endpoint(endpoint), headers, HTTPClient.METHOD_POST, body)
	if error != OK:
		_pending = null
		request.queue_free()
		return {"error": "无法启动模型请求，请检查连接地址。"}
	var response: Array = await request.request_completed
	if _pending == request:
		_pending = null
	request.queue_free()
	if response[0] != HTTPRequest.RESULT_SUCCESS:
		return {"error": _transport_error(response[0])}
	if response[1] < 200 or response[1] >= 300:
		return {"error": _service_error(response[1], response[3].get_string_from_utf8())}
	var parser := JSON.new()
	if parser.parse(response[3].get_string_from_utf8()) != OK:
		return {"error": "服务未返回 JSON。请检查是否填成网页地址，或网关是否忽略 stream=false 返回了流式数据。"}
	var decoded: Variant = parser.data
	if not decoded is Dictionary:
		return {"error": "模型返回了无效 JSON。"}
	var choices: Variant = decoded.get("choices")
	if not choices is Array or choices.is_empty() or not choices[0] is Dictionary:
		return {"error": "该服务未返回 Chat Completions 的 choices 数据，请检查网关的协议与接口路径。"}
	if choices[0].get("finish_reason") == "length":
		return {"error": "模型输出预算耗尽，回复被截断；请提高输出预算或选择服务默认值。推理模型的思考也可能占用预算。本轮未执行截断回复中的操作。"}
	var message: Variant = choices[0].get("message")
	if not message is Dictionary:
		return {"error": "模型消息格式不兼容。"}
	var content: Variant = message.get("content")
	var calls: Variant = message.get("tool_calls")
	if not (content is String and not content.strip_edges().is_empty()) and not (calls is Array and not calls.is_empty()):
		if message.get("reasoning_content") is String and not message.reasoning_content.is_empty():
			return {"error": "模型只返回了思考内容，没有最终文字或工具调用。请提高输出预算或检查网关对推理模型的支持。"}
		if message.get("refusal") is String and not message.refusal.is_empty():
			return {"error": "模型拒绝了本次请求，请调整消息后重试。"}
		return {"error": "模型返回空消息，没有文字或工具调用；请检查模型名称与网关协议。"}
	return {"message": message}


func _transport_error(result: int) -> String:
	match result:
		HTTPRequest.RESULT_CANT_RESOLVE:
			return "无法解析模型服务域名，请检查地址与 DNS。"
		HTTPRequest.RESULT_CANT_CONNECT:
			return "无法连接模型服务，请检查主机、端口、服务是否启动以及网络可达性。" + _web_hint()
		HTTPRequest.RESULT_TLS_HANDSHAKE_ERROR:
			return "HTTPS 握手失败，请检查服务端证书、系统时间和证书信任链。"
		HTTPRequest.RESULT_TIMEOUT:
			return "模型请求超时（%d 秒），请检查服务状态；本地加载或较慢推理可在连接页增加超时。" % int(ceil(timeout_seconds))
		HTTPRequest.RESULT_BODY_SIZE_LIMIT_EXCEEDED:
			return "模型回复超过 256 KiB 限制，请减小输出预算。"
		HTTPRequest.RESULT_REDIRECT_LIMIT_REACHED:
			return "服务要求重定向，请填写最终 API 地址。为避免转发凭据，当前不自动跟随重定向。"
	return "模型连接中断或服务未返回完整响应，请检查网关与网络。" + _web_hint()


func _web_hint() -> String:
	return " Web 还需服务允许当前来源的 CORS，HTTPS 页面不能连接 HTTP 服务。" if OS.has_feature("web") else ""


func _service_error(status: int, body: String) -> String:
	var prefix := "模型服务返回 HTTP %d：" % status
	match status:
		401:
			return prefix + "鉴权失败，请填写或检查 API Key（访问凭据），再连接并测试。"
		403:
			return prefix + "访问被拒绝，请检查凭据的模型权限或网关访问策略。"
		404:
			return prefix + "接口或模型不存在，请检查 /v1/chat/completions 路径和模型名称。"
		429:
			return prefix + "请求限流或额度不足，请稍后重试并检查服务配额。"
	if status >= 500:
		return prefix + "网关或上游模型服务异常，请检查服务端状态。"
	# Only use a provider error internally to select a fixed diagnostic. Never echo it:
	# gateways may include request bodies or credentials in their error text.
	var hint := body.to_lower()
	if "max_tokens" in hint or "max_completion_tokens" in hint:
		return prefix + "服务不接受当前输出预算参数，请在连接页选择「服务默认」或服务要求的参数。"
	if "tool" in hint:
		return prefix + "服务拒绝工具调用格式，请确认模型和网关支持 tools / tool_calls / role=tool。"
	if "model" in hint:
		return prefix + "模型参数被拒绝，请使用网关实际提供的模型 ID。"
	return prefix + "请求参数或协议不兼容，请检查网关是否支持 Chat Completions。"
