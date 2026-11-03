extends Node
signal changed
signal selection_changed
signal status_changed(message: String)
signal draft_received(source: String, filename: String)
signal ownership_changed

const ViewScene = preload("res://scripts/desktop/view_scene.gd")
const DataStore = preload("res://scripts/desktop/data_store.gd")
const Geo = preload("res://scripts/desktop/geo.gd")
var views: Dictionary = {}
var active_id := ""
var store: Node
var runner: Node
var scripts: Node
var workbench: Control
var bridge: Node
var errors: Array[Dictionary] = []
var audit: Array[Dictionary] = []
var native_owned := false
var shutting_down := false
var live_scene: Node:
	get: return self
var _serial := 0
var _baseline: Dictionary = {}
var _baseline_nodes: Dictionary = {}
var _baseline_layers: Dictionary = {}
var _baseline_environment: Dictionary = {}

func _ready() -> void:
	store = DataStore.new()
	add_child(store)
	scripts = preload("res://scripts/scripting/script_library.gd").new()
	add_child(scripts)
	create_view()

func create_view() -> String:
	_serial += 1
	var id := "view_%d" % _serial
	var view := ViewScene.new()
	view.id = id
	views[id] = view
	add_child(view)
	if active_id.is_empty(): active_id = id
	changed.emit()
	return id

func activate(id: String, kind := "viewport", layer_id := "", node: Node = null, viewport_properties := false) -> void:
	if not views.has(id): return
	active_id = id
	views[id].selection_kind = kind
	views[id].show_viewport_properties = viewport_properties if kind == "viewport" else false
	if kind in ["layer", "node"]: views[id].selected = layer_id
	views[id].selected_node = weakref(node) if is_instance_valid(node) else null
	selection_changed.emit()

func close_view(id: String) -> bool:
	if not views.has(id) or views.size() <= 1: return false
	var view: Node = views[id]
	for layer in view.layers.duplicate():
		store.detach(id, layer)
		view.layers.erase(layer)
	views.erase(id)
	view.queue_free()
	if active_id == id: active_id = views.keys()[0]
	changed.emit()
	selection_changed.emit()
	return true

func add_source(view_id: String, source: String, kind := "auto", options := {}) -> String:
	if not views.has(view_id): return ""
	_serial += 1
	var data_id: String = store.create(source, kind, options)
	var record: Dictionary = store.records[data_id]
	var layer := {"id": "layer_%d" % _serial, "data_id": data_id, "name": record.name, "kind": record.kind, "visible": true, "opacity": 1.0, "receive_lighting_mode": -1, "node": null, "status": "Queued", "error": "", "paused": false}
	views[view_id].layers.append(layer)
	store.attach(view_id, layer)
	activate(view_id, "layer", layer.id)
	changed.emit()
	return layer.id

func attach_script_layer(view_id: String, data_id: String) -> Dictionary:
	_serial += 1
	var record: Dictionary = store.records[data_id]
	var layer := {"id": "layer_%d" % _serial, "data_id": data_id, "name": record.name, "kind": "script", "visible": true, "opacity": 1.0, "receive_lighting_mode": -1, "node": null, "status": record.state, "error": "", "paused": false}
	views[view_id].layers.append(layer)
	store.attach(view_id, layer)
	changed.emit()
	return layer

func share_layer(from_view: String, layer_id: String, to_view: String) -> bool:
	if not views.has(from_view) or not views.has(to_view) or from_view == to_view: return false
	var original: Dictionary = views[from_view].get_layer(layer_id)
	if original.is_empty(): return false
	if original.kind == "script":
		var script_id: String = store.records[original.data_id].get("script_id", "")
		if script_id.is_empty(): return false
		var script_layer: Dictionary = scripts.ensure_layer(script_id, to_view)
		activate(to_view, "layer", script_layer.id)
		return true
	# Native streamed datasets currently own their request queues and renderer RIDs.
	# Never claim a duplicate loader is shared data.
	if original.kind == "tiled":
		status_changed.emit(tr("Sharing streamed datasets requires engine support."))
		return false
	_serial += 1
	var layer: Dictionary = original.duplicate()
	layer.id = "layer_%d" % _serial
	layer.node = null
	views[to_view].layers.append(layer)
	store.attach(to_view, layer)
	activate(to_view, "layer", layer.id)
	changed.emit()
	return true

func remove_layer(view_id: String, layer_id: String) -> void:
	if not views.has(view_id): return
	var view: Node = views[view_id]
	var layer: Dictionary = view.get_layer(layer_id)
	if layer.is_empty(): return
	if layer.kind == "script":
		var script_id: String = store.records[layer.data_id].get("script_id", "")
		if scripts.documents.has(script_id):
			scripts.remove(script_id)
			return
	store.detach(view_id, layer)
	view.layers.erase(layer)
	view.clear_pick_cache()
	if view.selected == layer_id:
		view.selected = ""
		view.selection_kind = "viewport"
		view.selected_node = null
	changed.emit()
	selection_changed.emit()

func set_visible(view_id: String, layer_id: String, value: bool) -> void:
	var layer: Dictionary = views[view_id].get_layer(layer_id)
	if layer.is_empty(): return
	layer.visible = value
	if is_instance_valid(layer.node): layer.node.visible = value
	changed.emit()

func set_opacity(view_id: String, layer_id: String, value: float) -> void:
	var layer: Dictionary = views[view_id].get_layer(layer_id)
	if layer.is_empty(): return
	layer.opacity = clampf(value, 0, 1)
	store.apply_display(layer)

func reorder(view_id: String, layer_id: String, step: int) -> void:
	var list: Array = views[view_id].layers
	var layer: Dictionary = views[view_id].get_layer(layer_id)
	var index := list.find(layer)
	if index < 0: return
	list.remove_at(index)
	list.insert(clampi(index + step, 0, list.size()), layer)
	changed.emit()

func report(view_id: String, layer_id: String, stage: String, message: String) -> void:
	var safe := redact(message)
	errors.append({"viewport": view_id, "layer": layer_id, "stage": stage, "message": safe, "time": Time.get_datetime_string_from_system()})
	if errors.size() > 250: errors.pop_front()
	status_changed.emit(safe)
	changed.emit()

static func redact(value: String) -> String:
	var regex := RegEx.new()
	regex.compile("(?i)(https?://)[^\\s/@]+:[^\\s/@]+@")
	value = regex.sub(value, "$1[redacted]@", true)
	regex.compile("(?i)([?&][^=\\s&#]+)=([^\\s&#]*)")
	value = regex.sub(value, "$1=[redacted]", true)
	regex.compile("(?i)(authorization|cookie|token|api[_-]?key|password)\\s*[:=]\\s*[^\\r\\n]+")
	return regex.sub(value, "$1: [redacted]", true)

func describe() -> Dictionary:
	var result: Array = []
	for id in views:
		var view: Node = views[id]
		var list: Array = []
		for layer in view.layers:
			list.append({"id": layer.id, "name": redact(layer.name), "kind": layer.kind, "status": layer.status, "visible": layer.visible, "opacity": layer.opacity, "references": store.reference_count(layer.data_id)})
		result.append({"id": id, "layers": list, "camera": {"position": [view.camera.position.x, view.camera.position.y, view.camera.position.z], "fov": view.camera.fov}})
	return {"version": "1.0.0", "active_viewport": active_id, "viewports": result}

func audit_event(tool: String, outcome: String, script_id := "") -> void:
	var executor: Node = scripts.documents[script_id].runner if scripts.documents.has(script_id) else runner
	audit.append({"time": Time.get_datetime_string_from_system(), "tool": tool, "result": outcome, "script_id": executor.script_id if is_instance_valid(executor) else "", "script_hash": executor.source_snapshot.sha256_text() if is_instance_valid(executor) else ""})
	if audit.size() > 250: audit.pop_front()

func object(key: String) -> Object:
	# The application API deliberately requires an explicit viewport ID.
	if key == "world": return self
	return null

func begin_native() -> void:
	if native_owned: return
	_baseline.clear()
	_baseline_nodes.clear()
	_baseline_layers.clear()
	_baseline_environment.clear()
	for id in views:
		_baseline[id] = views[id].pose()
		_baseline_nodes[id] = views[id].root.get_children()
		_baseline_layers[id] = views[id].layers.duplicate(true)
		_baseline_environment[id] = {"background": views[id].environment.background_color, "ambient": views[id].environment.ambient_light_energy, "light": views[id].light.light_energy if is_instance_valid(views[id].light) else 1.1}
	native_owned = true
	ownership_changed.emit()

func reset_native() -> void:
	scripts.stop_all()
	for id in views:
		var view: Node = views[id]
		for layer in view.layers: store._dispose_node(layer)
		view.rebuild_world()
		var baseline: Array = _baseline_layers.get(id, [])
		for saved in baseline:
			var existing: Dictionary = view.get_layer(saved.id)
			if existing.is_empty(): continue
			existing.visible = saved.visible
			existing.opacity = saved.opacity
			existing.name = saved.name
			existing.paused = saved.paused
			if saved.has("transform"): existing["transform"] = saved.transform
			else: existing.erase("transform")
		for layer in view.layers:
			store.rebuild_instance(id, layer)
		if _baseline.has(id): view.restore_pose(_baseline[id])
		if _baseline_environment.has(id):
			view.environment.background_color = _baseline_environment[id].background
			view.environment.ambient_light_energy = _baseline_environment[id].ambient
			if is_instance_valid(view.light): view.light.light_energy = _baseline_environment[id].light
		# Rebuild layer instances; owned script nodes are disposed by the runner.
	_baseline.clear()
	_baseline_nodes.clear()
	_baseline_layers.clear()
	_baseline_environment.clear()
	native_owned = false
	ownership_changed.emit()
	changed.emit()

func shutdown() -> void:
	if shutting_down: return
	shutting_down = true
	if bridge: bridge.stop()
	if scripts: scripts.stop_all()
	store.shutdown()

func _exit_tree() -> void:
	if is_instance_valid(store): store.shutdown()
