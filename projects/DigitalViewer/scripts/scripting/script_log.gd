extends Logger
## Never logs from a logger callback. Copies only bounded text into a thread-safe queue.
var _mutex := Mutex.new()
var _source := ""
var _entries: Array[Dictionary] = []
var _dropped := 0


func watch(source: String) -> void:
	_mutex.lock()
	_source = source
	_mutex.unlock()


func _watched() -> String:
	_mutex.lock()
	var source := _source
	_mutex.unlock()
	return source


func _append(entry: Dictionary) -> void:
	_mutex.lock()
	if _entries.size() < 500:
		_entries.append(entry)
	else:
		_dropped += 1
	_mutex.unlock()


func drain() -> Array[Dictionary]:
	_mutex.lock()
	var result := _entries
	_entries = []
	if _dropped > 0:
		result.append({"kind": "warning", "message": tr("Output limit reached; %d messages were dropped.") % _dropped, "line": 0, "file": "", "stack": []})
		_dropped = 0
	_mutex.unlock()
	return result


func _frames(traces: Array[ScriptBacktrace]) -> Array:
	var frames: Array = []
	for trace: ScriptBacktrace in traces:
		for index: int in mini(trace.get_frame_count(), 16):
			frames.append({"file": trace.get_frame_file(index).left(512), "line": trace.get_frame_line(index), "function": trace.get_frame_function(index).left(128)})
	return frames


func _log_message(message: String, error: bool) -> void:
	var source := _watched()
	if source.is_empty():
		return
	var frames := _frames(Engine.capture_script_backtraces(false))
	for frame: Dictionary in frames:
		if frame.file == source:
			_append({"kind": "stderr" if error else "output", "message": message.left(4096), "file": source, "line": frame.line, "stack": []})
			return


func _log_error(_function: String, file: String, line: int, code: String, rationale: String, _editor_notify: bool, error_type: int, script_backtraces: Array[ScriptBacktrace]) -> void:
	var source := _watched()
	if source.is_empty():
		return
	var frames := _frames(script_backtraces)
	var matched := file == source
	for frame: Dictionary in frames:
		if frame.file == source:
			matched = true
			file = source
			line = frame.line
			break
	if matched:
		_append({"kind": "warning" if error_type == ERROR_TYPE_WARNING else "error", "message": (rationale if not rationale.is_empty() else code).left(4096), "file": file, "line": line, "stack": frames})
