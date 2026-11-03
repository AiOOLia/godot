extends Node
## A cancellable download transaction shared by all references to one dataset.
signal completed(path: String, error: String)
signal cached(path: String)
signal progress(done: int, total: int, bytes: int)
const URI = preload("res://scripts/desktop/http_source.gd")
const MAX_FILE_BYTES := 64 * 1024 * 1024
const MAX_TOTAL_BYTES := 256 * 1024 * 1024
const MAX_DEPENDENCIES := 256
var _source := ""
var _directory := ""
var _headers := {}
var _document := {}
var _queue: Array[Dictionary] = []
var _active: HTTPRequest
var _cancelled := false
var _bytes := 0
var _done := 0
var _count := 0
var _root_path := ""
var _root_is_glb := false
var _glb_chunks := PackedByteArray()
var _timer: Timer

func start(source: String, directory: String, headers: Dictionary = {}) -> void:
	_source = URI.resolve(source, "")
	_directory = directory
	_headers = headers.duplicate()
	var validation := URI.headers_error(headers)
	if _source.is_empty():
		_fail("Use a valid HTTP(S) model URL without embedded credentials.")
		return
	if not validation.is_empty():
		_fail(validation)
		return
	if DirAccess.make_dir_recursive_absolute(directory) != OK:
		_fail("Cannot create session cache.")
		return
	_root_is_glb = URI.parse(_source).path.get_extension().to_lower() == "glb"
	_root_path = directory.path_join("scene.glb" if _root_is_glb else "scene.gltf")
	_fetch({"url": _source, "file": _root_path, "root": true, "redirects": 0, "attempt": 0, "credentials": true})

func cancel_request() -> void:
	_cancelled = true
	_queue.clear()
	_headers.clear()
	if is_instance_valid(_active):
		_active.cancel_request()
		_active.queue_free()
		_active = null
	if is_instance_valid(_timer):
		_timer.stop()
		_timer.queue_free()
		_timer = null

func _exit_tree() -> void:
	cancel_request()

func _fetch(task: Dictionary) -> void:
	if _cancelled: return
	if _bytes >= MAX_TOTAL_BYTES:
		_fail("Remote model and dependencies exceed the 256 MiB session limit.")
		return
	var request := HTTPRequest.new()
	request.body_size_limit = mini(MAX_FILE_BYTES, MAX_TOTAL_BYTES - _bytes)
	request.timeout = 30
	# Resolve redirects ourselves so the final document URL is the dependency base,
	# and credentials cannot follow a cross-origin redirect or HTTPS downgrade.
	request.max_redirects = 0
	add_child(request)
	_active = request
	var headers := PackedStringArray()
	if task.credentials:
		for key in _headers: headers.append(key + ": " + _headers[key])
	request.request_completed.connect(func(result: int, code: int, response_headers: PackedStringArray, body: PackedByteArray):
		if _active == request: _active = null
		request.queue_free()
		if not _cancelled: _received(task, result, code, response_headers, body))
	var error := request.request(task.url, headers)
	if error != OK:
		_active = null
		request.queue_free()
		_fail("Cannot start model download: " + error_string(error))

func _received(task: Dictionary, result: int, code: int, headers: PackedStringArray, body: PackedByteArray) -> void:
	if code in [301, 302, 303, 307, 308]:
		var location := ""
		for header in headers:
			if header.to_lower().begins_with("location:"):
				location = header.substr(header.find(":") + 1).strip_edges()
		var url := URI.resolve(task.url, location)
		if location.is_empty() or url.is_empty() or task.redirects >= 3:
			_fail("Invalid, insecure or excessive model redirect.")
			return
		task.credentials = task.credentials and URI.parse(url).origin == URI.parse(task.url).origin
		task.url = url
		task.redirects += 1
		_fetch(task)
		return
	var retryable := code in [408, 429, 500, 502, 503, 504] or result in [HTTPRequest.RESULT_CANT_CONNECT, HTTPRequest.RESULT_CONNECTION_ERROR, HTTPRequest.RESULT_TIMEOUT]
	if retryable and task.attempt < 2:
		task.attempt += 1
		_timer = Timer.new()
		_timer.one_shot = true
		add_child(_timer)
		_timer.timeout.connect(func():
			_timer.queue_free()
			_timer = null
			_fetch(task))
		_timer.start(0.5 * task.attempt)
		return
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		_fail("Model download failed (HTTP %d, result %d). Check URL, TLS, file size and access permissions." % [code, result])
		return
	_bytes += body.size()
	if _bytes > MAX_TOTAL_BYTES:
		_fail("Remote model and dependencies exceed the 256 MiB session limit.")
		return
	if task.root:
		if _root_is_glb:
			if body.size() < 20 or body.decode_u32(0) != 0x46546c67 or body.decode_u32(4) != 2 or body.decode_u32(8) != body.size():
				_fail("Remote GLB has an invalid version 2 header.")
				return
			var json_length := body.decode_u32(12)
			if body.decode_u32(16) != 0x4e4f534a or json_length % 4 != 0 or json_length > body.size() - 20:
				_fail("Remote GLB has an invalid JSON chunk.")
				return
			_glb_chunks = body.slice(20 + json_length)
			if not _plan_dependencies(body.slice(20, 20 + json_length), task.url): return
		else:
			if not _plan_dependencies(body, task.url): return
	else:
		if not _write(task.file, body): return
		if not task.root: _done += 1
	progress.emit(_done, _count, _bytes)
	_next()

func _plan_dependencies(body: PackedByteArray, base: String) -> bool:
	var parser := JSON.new()
	if parser.parse(body.get_string_from_utf8()) != OK or not parser.data is Dictionary:
		_fail("Remote glTF is not valid JSON.")
		return false
	_document = parser.data
	if not _document.get("asset") is Dictionary or _document.asset.get("version") != "2.0":
		_fail("Remote glTF must declare asset version 2.0.")
		return false
	var resources := {}
	for section in ["buffers", "images"]:
		var items: Variant = _document.get(section, [])
		if not items is Array:
			_fail("Invalid glTF resource list.")
			return false
		for item in items:
			if not item is Dictionary or (item.has("uri") and not item.uri is String):
				_fail("Invalid glTF dependency URI.")
				return false
			if not item.has("uri") or item.uri.begins_with("data:"): continue
			var url := URI.resolve(base, item.uri)
			if url.is_empty():
				_fail("glTF dependencies must use HTTP(S); HTTPS downgrades are rejected.")
				return false
			if not resources.has(url):
				if resources.size() >= MAX_DEPENDENCIES:
					_fail("Remote glTF exceeds 256 dependencies.")
					return false
				var extension: String = URI.parse(url).path.get_extension().to_lower()
				if extension not in ["bin", "png", "jpg", "jpeg", "webp", "ktx2"]: extension = "bin"
				var filename := "resource_%03d.%s" % [resources.size(), extension]
				resources[url] = filename
				_queue.append({"url": url, "file": _directory.path_join(filename), "root": false, "redirects": 0, "attempt": 0, "credentials": URI.parse(url).origin == URI.parse(_source).origin})
			item.uri = resources[url]
	_count = resources.size()
	return true

func _next() -> void:
	if _cancelled: return
	if not _queue.is_empty():
		_fetch(_queue.pop_front())
		return
	var json := JSON.stringify(_document).to_utf8_buffer()
	if _root_is_glb:
		while json.size() % 4 != 0: json.append(32)
		var header := PackedByteArray()
		header.resize(20)
		header.encode_u32(0, 0x46546c67)
		header.encode_u32(4, 2)
		header.encode_u32(8, 20 + json.size() + _glb_chunks.size())
		header.encode_u32(12, json.size())
		header.encode_u32(16, 0x4e4f534a)
		json = header + json + _glb_chunks
	if not _write(_root_path, json): return
	_headers.clear()
	completed.emit(_root_path, "")

func _write(path: String, bytes: PackedByteArray) -> bool:
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		_fail("Cannot write model to session cache.")
		return false
	# Register the path before writing so even a disk-full partial file is cleaned.
	cached.emit(path)
	var success := file.store_buffer(bytes)
	file.flush()
	var error := file.get_error()
	file.close()
	if not success or error != OK:
		_fail("Cannot write model to session cache.")
		return false
	return true

func _fail(message: String) -> void:
	if _cancelled: return
	cancel_request()
	completed.emit("", message)
