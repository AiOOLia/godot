extends Node
## Streamable HTTP JSON responses. No SSE subscription, shell, filesystem or network tools.
const PORT := 8731
const MAX_REQUEST := 131072
const RequestId = preload("res://scripts/desktop/mcp_request_id.gd")
const TOOL_NAMES := ["scene.describe", "viewport.list", "script.inspect", "script.propose", "script.run", "capture.screenshot", "audit.list"]
var enabled := false
var token := ""
var token_path := "user://mcp/bearer_token"
var port := PORT
var permissions := {"scene.read": true, "script.run": false, "capture": false}
var _server := TCPServer.new()
var _clients: Array[Dictionary] = []
var _sessions: Dictionary = {}

func is_plugin() -> bool:
	return false

func start() -> int:
	if enabled: return OK
	if OS.has_feature("web") or OS.has_feature("mobile"): return ERR_UNAVAILABLE
	var error := _server.listen(port, "127.0.0.1")
	if error != OK:
		AppState.status_changed.emit("MCP: " + error_string(error) + " (127.0.0.1:%d)" % port)
		return error
	error = ensure_token()
	if error != OK:
		_server.stop()
		AppState.status_changed.emit(tr("MCP token could not be loaded or saved.") + " " + error_string(error))
		return error
	enabled = true
	return OK

func stop() -> void:
	enabled = false
	_server.stop()
	for client in _clients: client.peer.disconnect_from_host()
	_clients.clear()
	_sessions.clear()

func ensure_token() -> int:
	# Windows DirAccess replaces by removing the destination before rename.
	# Recover the previous complete credential if interrupted in that small gap.
	if not FileAccess.file_exists(token_path) and FileAccess.file_exists(token_path + ".previous"):
		var recovered := DirAccess.rename_absolute(token_path + ".previous", token_path)
		if recovered != OK: return recovered
	if FileAccess.file_exists(token_path):
		var file := FileAccess.open(token_path, FileAccess.READ)
		if file == null: return FileAccess.get_open_error()
		if file.get_length() > 514: return ERR_FILE_CORRUPT
		var saved := file.get_as_text().strip_edges()
		if not _valid_token(saved): return ERR_FILE_CORRUPT
		token = saved
		return OK
	return replace_token(Crypto.new().generate_random_bytes(32).hex_encode())

static func _valid_token(value: String) -> bool:
	if value.length() < 32 or value.length() > 512 or value.begins_with("="): return false
	var padding := false
	for character in value:
		if character == "=": padding = true
		elif padding or not character in "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~+/": return false
	return true

func replace_token(value: String) -> int:
	value = value.strip_edges()
	if not _valid_token(value): return ERR_INVALID_PARAMETER
	var error := DirAccess.make_dir_recursive_absolute(token_path.get_base_dir())
	if error != OK: return error
	# Commit only a complete credential. A failed write never replaces a working token.
	var temporary := token_path + ".tmp"
	var file := FileAccess.open(temporary, FileAccess.WRITE)
	if file == null: return FileAccess.get_open_error()
	if OS.get_name() != "Windows":
		error = FileAccess.set_unix_permissions(temporary, FileAccess.UNIX_READ_OWNER | FileAccess.UNIX_WRITE_OWNER)
		if error != OK:
			file.close()
			DirAccess.remove_absolute(temporary)
			return error
	file.store_string(value + "\n")
	file.flush()
	error = file.get_error()
	file.close()
	var backup := token_path + ".previous"
	if error == OK and FileAccess.file_exists(token_path):
		error = DirAccess.copy_absolute(token_path, backup, FileAccess.UNIX_READ_OWNER | FileAccess.UNIX_WRITE_OWNER)
	if error == OK: error = DirAccess.rename_absolute(temporary, token_path)
	if error != OK:
		if not FileAccess.file_exists(token_path) and FileAccess.file_exists(backup): DirAccess.rename_absolute(backup, token_path)
		DirAccess.remove_absolute(temporary)
		return error
	if FileAccess.file_exists(backup): DirAccess.remove_absolute(backup)
	token = value
	_sessions.clear()
	for client in _clients.duplicate(): _close(client)
	AppState.audit_event("mcp.token", "updated")
	return OK

func reset_token() -> int:
	return replace_token(Crypto.new().generate_random_bytes(32).hex_encode())

func _exit_tree() -> void:
	stop()

func _process(_delta: float) -> void:
	if not enabled: return
	while _server.is_connection_available():
		var peer := _server.take_connection()
		if _clients.size() >= 16:
			peer.disconnect_from_host()
		else:
			_clients.append({"peer": peer, "bytes": PackedByteArray(), "deadline": Time.get_ticks_msec() + 5000})
	for client in _clients.duplicate():
		var peer: StreamPeerTCP = client.peer
		peer.poll()
		if peer.get_status() != StreamPeerTCP.STATUS_CONNECTED or Time.get_ticks_msec() > client.deadline:
			_close(client)
			continue
		if client.has("response"):
			var outgoing: PackedByteArray = client.response
			var sent := peer.put_partial_data(outgoing.slice(client.sent, mini(outgoing.size(), client.sent + 65536)))
			if sent[0] != OK:
				_close(client)
				continue
			client.sent += sent[1]
			if client.sent >= outgoing.size(): _close(client)
			continue
		var available := peer.get_available_bytes()
		if available > 0:
			if client.bytes.size() + available > MAX_REQUEST:
				_reply(client, 413, {"error": "Request too large"})
				continue
			var received := peer.get_data(available)
			if received[0] != OK:
				_close(client)
				continue
			client.bytes.append_array(received[1])
		_read_request(client)

func _close(client: Dictionary) -> void:
	client.peer.disconnect_from_host()
	_clients.erase(client)

func _read_request(client: Dictionary) -> void:
	var raw: PackedByteArray = client.bytes
	var boundary := -1
	for index in range(maxi(0, int(client.get("scanned", 0)) - 3), raw.size() - 3):
		if raw[index] == 13 and raw[index + 1] == 10 and raw[index + 2] == 13 and raw[index + 3] == 10:
			boundary = index
			break
	client["scanned"] = raw.size()
	if boundary < 0:
		if raw.size() > 8192: _reply(client, 431, {"error": "Headers too large"})
		return
	var header_text := raw.slice(0, boundary).get_string_from_ascii()
	var header_bytes := boundary + 4
	if header_bytes > 8192:
		_reply(client, 431, {"error": "Headers too large"})
		return
	var lines := header_text.split("\r\n")
	var request := lines[0].split(" ")
	var endpoint: String = request[1] if request.size() == 3 else ""
	var expected_endpoint := "/bridge" if is_plugin() else "/mcp"
	if request.size() != 3 or endpoint != expected_endpoint or request[2] != "HTTP/1.1":
		_reply(client, 404, {"error": "Unknown endpoint"})
		return
	var headers := {}
	for line in lines.slice(1):
		var colon: int = line.find(":")
		if colon < 1:
			_reply(client, 400, {"error": "Invalid header"})
			return
		var key: String = line.substr(0, colon).strip_edges().to_lower()
		if headers.has(key):
			_reply(client, 400, {"error": "Duplicate header"})
			return
		headers[key] = line.substr(colon + 1).strip_edges()
	if headers.get("host", "") not in ["127.0.0.1:%d" % port, "localhost:%d" % port] or headers.has("origin"):
		_reply(client, 403, {"error": "Only native loopback clients are permitted"})
		return
	if headers.get("authorization", "") != "Bearer " + token:
		_reply(client, 401, {"error": "Bearer token required"})
		return
	var session: String = headers.get("mcp-session-id", "")
	if request[0] == "DELETE":
		_sessions.erase(session)
		_reply(client, 204, {})
		return
	if request[0] != "POST":
		_reply(client, 405, {"error": "Use POST; server-initiated SSE is not supported"})
		return
	var length_text: String = headers.get("content-length", "")
	if headers.has("transfer-encoding") or not length_text.is_valid_int() or int(length_text) < 0 or int(length_text) > MAX_REQUEST - header_bytes:
		_reply(client, 400, {"error": "A bounded Content-Length is required"})
		return
	if not str(headers.get("content-type", "")).begins_with("application/json"):
		_reply(client, 415, {"error": "Expected application/json"})
		return
	var length := int(length_text)
	if raw.size() < header_bytes + length:
		client.scanned = 0
		return
	var body_text := raw.slice(header_bytes, header_bytes + length).get_string_from_utf8()
	var parser := JSON.new()
	if parser.parse(body_text) != OK:
		_reply(client, 200, _rpc_error(null, -32700, "Parse error"))
		return
	var payload: Variant = parser.data
	if not payload is Dictionary or payload.get("jsonrpc") != "2.0" or not payload.get("method") is String:
		_reply(client, 200, _rpc_error(null, -32600, "Invalid request"))
		return
	var method: String = payload.method
	var id_info := RequestId.read(body_text)
	if not id_info.valid:
		_reply(client, 200, _rpc_error(null, -32600, "ID must be a string or signed 64-bit integer"))
		return
	var rpc_id: Variant = id_info.value
	var params: Variant = payload.get("params", {})
	if not params is Dictionary:
		_reply(client, 200, _rpc_error(rpc_id, -32602, "Expected object params"))
		return
	if is_plugin():
		_handle_plugin(client, method, rpc_id, params)
		return
	if method == "initialize":
		if not payload.has("id") or _sessions.size() >= 64:
			_reply(client, 400, {"error": "Invalid initialization or session limit reached"})
			return
		session = Crypto.new().generate_random_bytes(16).hex_encode()
		_sessions[session] = true
		var version: String = params.get("protocolVersion", "2025-03-26")
		if version not in ["2025-03-26", "2025-06-18", "2025-11-25"]: version = "2025-03-26"
		_reply(client, 200, {"jsonrpc": "2.0", "id": rpc_id, "result": {"protocolVersion": version, "capabilities": {"tools": {"listChanged": false}}, "serverInfo": {"name": "DigitalViewer", "version": "1.0.0"}}}, session)
		return
	if not _sessions.has(session):
		# Local desktop recovery: the app may restart while the Harness client keeps
		# its last Mcp-Session-Id. Re-admit authenticated tool calls so the plugin
		# can recover without restarting the whole Harness profile.
		if not session.is_empty() and method in ["ping", "tools/list", "tools/call"]:
			_sessions[session] = true
		else:
			_reply(client, 404, {"error": "Initialize a new MCP session"})
			return
	if not payload.has("id"):
		_reply(client, 202, {})
		return
	var result: Dictionary
	match method:
		"ping": result = {}
		"tools/list": result = {"tools": _tools()}
		"tools/call":
			var arguments: Variant = params.get("arguments", {})
			if not arguments is Dictionary or not params.get("name") is String:
				_reply(client, 200, _rpc_error(rpc_id, -32602, "Invalid tool arguments"))
				return
			result = call_tool(params.name, arguments)
		_:
			_reply(client, 200, _rpc_error(rpc_id, -32601, "Method not found"))
			return
	_reply(client, 200, {"jsonrpc": "2.0", "id": rpc_id, "result": result})

func _handle_plugin(client: Dictionary, method: String, rpc_id: Variant, params: Dictionary) -> void:
	if method == "ping":
		_reply(client, 200, {"jsonrpc": "2.0", "id": rpc_id, "result": {}})
		return
	if method == "bridge.capabilities":
		_reply(client, 200, {"jsonrpc": "2.0", "id": rpc_id, "result": {"protocol": "dv.bridge.v1", "tools": TOOL_NAMES, "permissions": permissions.duplicate()}})
		return
	if method == "tools/list":
		_reply(client, 200, {"jsonrpc": "2.0", "id": rpc_id, "result": {"tools": _tools()}})
		return
	var tool_name := method
	var arguments := params
	if method == "tools/call":
		tool_name = params.get("name", "") if params.get("name") is String else ""
		arguments = params.get("arguments", {})
		if not arguments is Dictionary:
			_reply(client, 200, _rpc_error(rpc_id, -32602, "Invalid tool arguments"))
			return
	if tool_name not in TOOL_NAMES:
		_reply(client, 200, _rpc_error(rpc_id, -32601, "Method not found"))
		return
	_reply(client, 200, {"jsonrpc": "2.0", "id": rpc_id, "result": call_tool(tool_name, arguments)})

func _rpc_error(id: Variant, code: int, message: String) -> Dictionary:
	return {"jsonrpc": "2.0", "id": id, "error": {"code": code, "message": message}}

func _tools() -> Array:
	var result: Array = []
	for name in TOOL_NAMES:
		var properties := {}
		var required: Array = []
		if name == "script.propose":
			properties = {"source": {"type": "string", "maxLength": 65536}, "filename": {"type": "string"}}
			required = ["source"]
		elif name == "capture.screenshot":
			properties = {"viewport": {"type": "string"}}
			required = ["viewport"]
		result.append({"name": name, "description": "Submit a draft for manual review. No compilation or execution." if name == "script.propose" else "Request manual review and execution in the application." if name == "script.run" else name, "inputSchema": {"type": "object", "properties": properties, "required": required, "additionalProperties": false}})
	return result

func call_tool(name: String, args: Dictionary) -> Dictionary:
	if name not in TOOL_NAMES: return _result({"error": "Unknown tool"}, true)
	var permission := "script.run" if name == "script.run" else "capture" if name == "capture.screenshot" else "scene.read"
	if not permissions.get(permission, false):
		AppState.audit_event(name, "permission_denied")
		return _result({"error": "Permission denied: " + permission}, true)
	AppState.audit_event(name, "requested")
	match name:
		"scene.describe", "viewport.list": return _result(AppState.describe())
		"script.inspect":
			var description: Dictionary = AppState.scripts.describe()
			description.merge({"running": AppState.runner.running, "status": AppState.runner.status, "source_hash": AppState.runner.source_snapshot.sha256_text()})
			return _result(description)
		"script.propose":
			if not args.get("source") is String or args.source.to_utf8_buffer().size() > 65536 or args.source.strip_edges().is_empty(): return _result({"error": "Source must be 1–65536 UTF-8 bytes"}, true)
			var filename: String = args.get("filename", "external_draft.gd") if args.get("filename", "") is String else "external_draft.gd"
			filename = filename.get_file().left(100)
			if not filename.ends_with(".gd"): filename = "external_draft.gd"
			AppState.draft_received.emit(args.source, filename)
			return _result({"status": "review_required", "source_hash": args.source.sha256_text(), "executed": false})
		"script.run":
			AppState.workbench.show()
			# Never press the UI trust confirmation on behalf of a client.
			return _result({"status": "manual_run_required", "executed": false, "message": "Review the workbench source and use Run in the application. Native scripts are not sandboxed."})
		"capture.screenshot":
			if not args.get("viewport") is String or not AppState.views.has(args.viewport): return _result({"error": "Unknown viewport"}, true)
			if DisplayServer.get_name() == "headless": return _result({"error": "Rendering is unavailable in headless mode"}, true)
			var picture: Image = AppState.views[args.viewport].viewport.get_texture().get_image()
			if picture == null or picture.is_empty(): return _result({"error": "No rendered frame"}, true)
			if picture.get_width() > 1920: picture.resize(1920, maxi(1, picture.get_height() * 1920 / picture.get_width()))
			return {"content": [{"type": "image", "mimeType": "image/png", "data": Marshalls.raw_to_base64(picture.save_png_to_buffer())}]}
		"audit.list": return _result({"entries": AppState.audit})
	return _result({"error": "Unknown tool"}, true)

func _result(value: Dictionary, error := false) -> Dictionary:
	return {"content": [{"type": "text", "text": JSON.stringify(value)}], "isError": error}

func _reply(client: Dictionary, code: int, value: Dictionary, session := "") -> void:
	var body := "" if code in [202, 204] else JSON.stringify(value)
	var headers := "HTTP/1.1 %d Response\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\n" % [code, body.to_utf8_buffer().size()]
	if not session.is_empty(): headers += "Mcp-Session-Id: " + session + "\r\n"
	client["response"] = (headers + "\r\n" + body).to_utf8_buffer()
	client["sent"] = 0
	client.deadline = Time.get_ticks_msec() + 10000
