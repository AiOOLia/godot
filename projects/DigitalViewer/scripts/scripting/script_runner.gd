extends Node
## Trusted local native GDScript. No VM sandbox, instruction budget or force interruption.
signal entry_added(entry: Dictionary)
signal state_changed

const MAX_SOURCE_BYTES := 65536
const SCRIPT_DIR := "user://scripts"
var script_directory := SCRIPT_DIR
var script_id := ""
const ScriptLog = preload("res://scripts/scripting/script_log.gd")
const Context = preload("res://scripts/scripting/script_context.gd")
var entries: Array[Dictionary] = []
var status := "Waiting"
var running := false
var source_snapshot := ""
var source_path := ""
var _logger = ScriptLog.new()
var _session: Node3D
var _instance: Node
var _serial := 0
var _registered := false


func _ready() -> void:
	OS.add_logger(_logger)
	_registered = true


func _exit_tree() -> void:
	_dispose_session()
	if _registered:
		OS.remove_logger(_logger)
		_registered = false


func _process(_delta: float) -> void:
	_collect()


func _record(kind: String, message: String, line := 0) -> void:
	_accept({"kind": kind, "message": message, "line": line, "file": source_path, "stack": []})


func _accept(entry: Dictionary) -> void:
	entries.append(entry)
	if entries.size() > 500:
		entries.pop_front()
	entry_added.emit(entry)


func _collect() -> bool:
	var failed := false
	for entry: Dictionary in _logger.drain():
		_accept(entry)
		failed = failed or entry.kind == "error"
	if failed:
		_dispose_session()
		status = "Script failed; direct scene edits remain"
		state_changed.emit()
	return failed


func run_source(source: String, trusted: bool) -> bool:
	if not trusted:
		status = "Confirm that you trust this script. Native code can access files, networks and processes."
		state_changed.emit()
		return false
	if source.to_utf8_buffer().size() > MAX_SOURCE_BYTES or source.strip_edges().is_empty():
		status = "Script must not be empty or exceed 64 KiB."
		state_changed.emit()
		return false
	stop()
	clear_output()
	source_snapshot = source
	if AppState.scripts.documents.has(script_id):
		var doc: Dictionary = AppState.scripts.documents[script_id]
		var view_id: String = doc.view_id if AppState.views.has(doc.view_id) else AppState.active_id
		AppState.scripts.ensure_layer(script_id, view_id)
	_serial += 1
	source_path = "user://scripts/workbench_run_%d_%d.gd" % [get_instance_id(), _serial]
	_logger.watch(source_path)
	# reload can execute static initializers: trust is required BEFORE compilation.
	AppState.live_scene.begin_native()
	var script := GDScript.new()
	script.resource_path = source_path
	script.source_code = source
	var error := script.reload()
	var failed := _collect()
	if error != OK or failed:
		if not failed:
			_record("error", tr("GDScript compilation failed: %s") % error_string(error))
		status = "Compilation failed"
		state_changed.emit()
		return false
	var base := script.get_instance_base_type()
	if not (base == &"Node" or ClassDB.is_parent_class(base, &"Node")) or not script.can_instantiate():
		_record("error", "Workbench scripts must extend Node or a subclass such as Node3D.")
		status = "Incompatible entry type"
		state_changed.emit()
		return false
	var valid_entry := false
	for method: Dictionary in script.get_script_method_list():
		if method.name == &"_init" and method.args.size() > method.default_args.size():
			_record("error", "_init cannot require arguments. Use run(viewer) to obtain the scene context.")
			status = "Incompatible initializer"
			state_changed.emit()
			return false
		if method.name == &"run" and method.args.size() == 1 and not (int(method.flags) & METHOD_FLAG_STATIC):
			valid_entry = true
	if not valid_entry:
		_record("error", "Define func run(viewer) with one scene context argument.")
		status = "Missing run(viewer)"
		state_changed.emit()
		return false
	_instance = script.new()
	if _collect():
		return false
	if not is_instance_valid(_instance):
		_record("error", "Cannot instantiate script.")
		status = "Instantiation failed"
		state_changed.emit()
		return false
	_session = Node3D.new()
	_session.name = "ScriptSession_" + script_id
	_session.set_meta("digitalviewer_script_session", script_id)
	var root: Node = AppState.live_scene.object("world")
	if not is_instance_valid(root):
		_record("error", "The script deleted the scene root. Reset the scene first.")
		_dispose_session()
		return false
	root.add_child(_session)
	_instance.name = "UserScript"
	_session.add_child(_instance)
	if _collect():
		return false
	if not is_instance_valid(_instance) or _instance.is_queued_for_deletion():
		_record("error", "The script freed itself during initialization; run(viewer) was not called.")
		_dispose_session()
		status = "Initialization ended"
		state_changed.emit()
		return false
	running = true
	_instance.call("run", Context.new(AppState, script_id))
	if _collect():
		return false
	status = "Running · _process / signals remain active"
	_record("info", "run(viewer) was called. Stop removes this script session.")
	state_changed.emit()
	return true


func _dispose_session() -> void:
	running = false
	for view in AppState.views.values():
		if not is_instance_valid(view.root): continue
		for child in view.root.get_children():
			if child.get_meta("digitalviewer_script_owned", "") == script_id and not script_id.is_empty():
				view.root.remove_child(child)
				child.queue_free()
		for layer in view.layers:
			if layer.kind == "script" and AppState.store.records.get(layer.data_id, {}).get("script_id", "") == script_id:
				layer.node = null
		view.clear_pick_cache()
	if is_instance_valid(_session):
		_session.process_mode = Node.PROCESS_MODE_DISABLED
		if _session.get_parent() != null:
			_session.get_parent().remove_child(_session)
		_session.queue_free()
	elif is_instance_valid(_instance):
		_instance.queue_free()
	_session = null
	_instance = null


func stop() -> void:
	_dispose_session()
	_logger.watch("")
	_collect()
	status = "Stopped · session nodes removed; direct edits retained"
	state_changed.emit()


func reset_scene() -> void:
	AppState.live_scene.reset_native()
	status = "Layer scenes rebuilt and navigation restored"
	state_changed.emit()


func clear_output() -> void:
	_logger.drain()
	entries.clear()


func list_scripts() -> PackedStringArray:
	var result := PackedStringArray()
	var directory := DirAccess.open(script_directory)
	if directory:
		for file: String in directory.get_files():
			if file.ends_with(".gd"):
				result.append(file)
	result.sort()
	return result


func _valid_filename(filename: String) -> bool:
	return filename == filename.get_file() and filename.is_valid_filename() and filename.ends_with(".gd") and filename.length() <= 100


func save_source(filename: String, source: String, overwrite := false) -> Dictionary:
	if not _valid_filename(filename) or source.to_utf8_buffer().size() > MAX_SOURCE_BYTES:
		return {"ok": false, "message": "Use a valid .gd filename without directories. Scripts are limited to 64 KiB."}
	var path := script_directory.path_join(filename)
	if FileAccess.file_exists(path) and not overwrite:
		return {"ok": false, "exists": true, "message": "This file already exists. Overwrite it?"}
	var error := DirAccess.make_dir_recursive_absolute(script_directory)
	if error != OK:
		return {"ok": false, "message": error_string(error)}
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		return {"ok": false, "message": error_string(FileAccess.get_open_error())}
	file.store_string(source)
	file.flush()
	error = file.get_error()
	file.close()
	return {"ok": error == OK, "message": tr("Saved %s") % path if error == OK else error_string(error)}


func load_source(filename: String) -> Dictionary:
	if not _valid_filename(filename):
		return {"ok": false, "message": "Invalid filename."}
	var file := FileAccess.open(script_directory.path_join(filename), FileAccess.READ)
	if file == null:
		return {"ok": false, "message": error_string(FileAccess.get_open_error())}
	if file.get_length() > MAX_SOURCE_BYTES:
		return {"ok": false, "message": "Script exceeds 64 KiB."}
	return {"ok": true, "source": file.get_as_text()}
