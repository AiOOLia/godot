extends Node
## Application-owned documents; editor selection never owns script execution.
signal opened(id: String)
signal changed
const Runner = preload("res://scripts/scripting/script_runner.gd")
const BLANK := "extends Node\n\nfunc run(viewer) -> void:\n\tpass\n"
var documents: Dictionary = {}
var _serial := 0
var _structure_dirty := false

func _ready() -> void:
	get_tree().node_added.connect(_node_changed)
	get_tree().node_removed.connect(_node_changed)

func _node_changed(node: Node) -> void:
	if AppState.shutting_down or _structure_dirty: return
	var relevant := node is Node3D
	var parent := node
	while not relevant and is_instance_valid(parent):
		relevant = parent.has_meta("digitalviewer_script_owned") or parent.has_meta("digitalviewer_script_session")
		parent = parent.get_parent()
	if not relevant: return
	_structure_dirty = true
	_refresh_structure.call_deferred()

func _refresh_structure() -> void:
	_structure_dirty = false
	if AppState.shutting_down: return
	for doc in documents.values():
		if not AppState.store.records.has(doc.data_id): continue
		for ref in AppState.store.records[doc.data_id].refs:
			AppState.store.apply_display(AppState.views[ref.view].get_layer(ref.layer))
			AppState.views[ref.view].clear_pick_cache()
	changed.emit()
	AppState.changed.emit()

func create(source: String, filename: String, saved := false, view_id := "", with_layer := true) -> String:
	_serial += 1
	var id := "script_%d" % _serial
	var executor := Runner.new()
	executor.script_id = id
	documents[id] = {"id": id, "source": source, "filename": filename, "saved_text": source if saved else "", "runner": executor, "data_id": "", "view_id": view_id if not view_id.is_empty() else AppState.active_id, "caret": Vector2i.ZERO, "scroll": 0.0, "confirmed_source_hash": ""}
	add_child(executor)
	executor.state_changed.connect(_sync.bind(id))
	if with_layer: ensure_layer(id, documents[id].view_id)
	changed.emit()
	return id

func is_source_confirmed(id: String, source: String) -> bool:
	return documents.has(id) and documents[id].confirmed_source_hash == source.sha256_text()

func confirm_source(id: String, source: String) -> void:
	# Explicit workbench confirmation only; never infer trust from imports or runner state.
	# Kept with this document for this application session, not by filename or on disk.
	if documents.has(id): documents[id].confirmed_source_hash = source.sha256_text()

func import_record(data: Dictionary, source: String) -> void:
	var id := create(source, data.name, true, data.refs[0].view, false)
	documents[id].data_id = data.id
	data["script_id"] = id
	data["script_source"] = source
	_sync(id)
	opened.emit(id)

func ensure_layer(id: String, view_id: String) -> Dictionary:
	if not documents.has(id) or not AppState.views.has(view_id): return {}
	var doc: Dictionary = documents[id]
	if doc.data_id.is_empty():
		doc.data_id = AppState.store.create(doc.filename, "script", {})
		var data: Dictionary = AppState.store.records[doc.data_id]
		data["script_id"] = id
		data["script_source"] = doc.source
		data.state = "Review required"
	for layer in AppState.views[view_id].layers:
		if layer.data_id == doc.data_id: return layer
	return AppState.attach_script_layer(view_id, doc.data_id)

func content(id: String, view_id: String) -> Node3D:
	var layer := ensure_layer(id, view_id)
	if layer.is_empty(): return null
	if is_instance_valid(layer.node): return layer.node
	var node := Node3D.new()
	node.name = id
	node.set_meta("digitalviewer_script_owned", id)
	layer.node = node
	AppState.views[view_id].root.add_child(node)
	AppState.store.apply_display(layer)
	return node

func _sync(id: String) -> void:
	if not documents.has(id): return
	var doc: Dictionary = documents[id]
	var executor: Node = doc.runner
	var state := "Running" if executor.running else "Review required" if executor.source_snapshot.is_empty() else "Stopped"
	var error := ""
	for entry in executor.entries:
		if entry.kind == "error": error = entry.message
	if not executor.running and not error.is_empty(): state = "Failed"
	if AppState.store.records.has(doc.data_id):
		var data: Dictionary = AppState.store.records[doc.data_id]
		data.name = doc.filename
		data.state = state
		data.error = error
		data.script_source = doc.source
		for ref in data.refs:
			var layer: Dictionary = AppState.views[ref.view].get_layer(ref.layer)
			layer.name = doc.filename
			layer.status = state
			layer.error = error
			AppState.views[ref.view].clear_pick_cache()
			AppState.store.apply_display(layer)
	changed.emit()
	AppState.changed.emit()

func stop_all() -> void:
	for doc in documents.values(): doc.runner.stop()

func remove(id: String) -> void:
	if not documents.has(id): return
	var doc: Dictionary = documents[id]
	doc.runner.stop()
	documents.erase(id)
	if AppState.store.records.has(doc.data_id):
		for ref in AppState.store.records[doc.data_id].refs.duplicate():
			AppState.remove_layer(ref.view, ref.layer)
	doc.runner.queue_free()
	changed.emit()

func describe() -> Dictionary:
	var list: Array = []
	var running_count := 0
	for doc in documents.values():
		if doc.runner.running: running_count += 1
		list.append({"id": doc.id, "name": doc.filename, "running": doc.runner.running, "status": doc.runner.status, "source_hash": doc.runner.source_snapshot.sha256_text(), "data_id": doc.data_id})
	return {"scripts": list, "running_count": running_count, "selected_script": AppState.workbench.selected_id if is_instance_valid(AppState.workbench) else ""}
