extends Node
## Parsed model resources are shared; scene nodes and display overrides belong to a viewport.
const Geo = preload("res://scripts/desktop/geo.gd")
const Profile = preload("res://scripts/desktop/build_profile.gd")
const Sources = preload("res://scripts/desktop/source_resolver.gd")
const MAX_FILE_BYTES := 64 * 1024 * 1024
var records: Dictionary = {}
var _serial := 0
var _jobs: Array[Dictionary] = []
var _requests: Dictionary = {}
var _temporary: Array[String] = []
var _cache_root := ""
var _data_files: Dictionary = {}
var _data_directories: Dictionary = {}
var _file_generations: Dictionary = {}
var _retired_files: Array[String] = []
var _tick := 0.0

func create(source: String, kind: String, options: Dictionary) -> String:
	_serial += 1
	var id := "data_%d" % _serial
	if kind == "auto":
		kind = Sources.kind(source)
	var title := "WGS84 ellipsoid" if kind == "ellipsoid" else "Cesium ion %s" % options.ion_asset_id if options.has("ion_asset_id") else source.get_slice("?", 0).get_file()
	records[id] = {"id": id, "source": source, "kind": kind, "original_source": source, "original_kind": kind, "name": title, "options": options.duplicate(true), "refs": [], "prototype": null, "state": "Queued", "error": "", "generation": 0}
	return id

func reference_count(id: String) -> int:
	return records[id].refs.size() if records.has(id) else 0

func attach(view_id: String, layer: Dictionary) -> void:
	var data: Dictionary = records[layer.data_id]
	data.refs.append({"view": view_id, "layer": layer.id})
	if data.has("script_id"):
		layer.status = data.state
		return
	if is_instance_valid(data.prototype):
		_instantiate(view_id, layer, data)
	elif data.state == "Failed":
		layer.status = data.state
		layer.error = data.error
	elif data.refs.size() == 1:
		_load.call_deferred(data.id)

func _load(id: String) -> void:
	if not records.has(id): return
	var data: Dictionary = records[id]
	data.generation += 1
	var ext := Sources.extension(data.source)
	if ext == "json":
		var remote: bool = data.source.begins_with("http://") or data.source.begins_with("https://")
		if remote and not data.has("json_probe"):
			_probe_remote_json(id)
			return
		var detected: Dictionary = data.json_probe if remote else Sources.detect(data.source)
		if detected.has("error"):
			_fail(id, detected.error)
			return
		if data.kind not in ["json", detected.kind]:
			_fail(id, "Requested importer does not match JSON content.")
			return
		_set_kind(data, detected.kind)
		if detected.kind == "brep":
			if remote:
				_fail(id, "Remote BREP is not supported in this preview. Download the model and open it locally.")
				return
			data["brep_assembly_path"] = ""
			if detected.get("brep_json", false):
				data["brep_path"] = data.source
			else:
				var dependency := Sources.dependency(data.source, detected.dependency)
				if dependency.has("error"):
					_fail(id, dependency.error)
					return
				data["brep_path"] = dependency.path
				data["brep_assembly_path"] = data.source
	elif ext == "brep":
		if data.kind != "brep":
			_fail(id, "Requested importer does not match BREP content.")
			return
		data["brep_path"] = data.source
		data["brep_assembly_path"] = ""
	var profile_error := Profile.source_error(data.source, data.kind, data.options)
	if not profile_error.is_empty():
		_fail(id, profile_error)
		return
	match data.kind:
		"ellipsoid":
			data.prototype = _ellipsoid()
			_finish(id)
		"gltf":
			if data.source.begins_with("http://") or data.source.begins_with("https://"):
				_download(id)
			else:
				_import_gltf(id, data.source)
		"brep": _import_brep(id)
		"tiled":
			if not ClassDB.class_exists("TiledMeshInstance3D"):
				_fail(id, "This engine does not include TiledMeshInstance3D. Use the matching custom build.")
				return
			for ref in data.refs: _instantiate_tiled(ref.view, AppState.views[ref.view].get_layer(ref.layer), data)
		"script":
			var file := FileAccess.open(data.source, FileAccess.READ)
			if file == null or file.get_length() > 65536:
				_fail(id, "Cannot read script, or script exceeds 64 KiB.")
				return
			var source := file.get_as_text()
			# Text checks only: compiling an untrusted script can execute static initializers.
			var entry := RegEx.new()
			entry.compile("(?m)^func\\s+run\\s*\\(\\s*[A-Za-z_][A-Za-z0-9_]*(?:\\s*:[^,)]+)?\\s*\\)")
			var base := RegEx.new()
			base.compile("(?m)^extends\\s+Node(?:3D|2D)?\\s*(?:#.*)?$")
			if not entry.search(source) or not base.search(source):
				_fail(id, "Script must extend Node and define run(viewer). No code was compiled or executed.")
				return
			_set_state(id, "Review required")
			AppState.scripts.import_record(data, source)
		"zip": _open_zip(id)
		_: _fail(id, "Unsupported format for the %s build profile." % Profile.id())

func _set_kind(data: Dictionary, kind: String) -> void:
	data.kind = kind
	for ref in data.refs:
		if AppState.views.has(ref.view): AppState.views[ref.view].get_layer(ref.layer).kind = kind

func _probe_remote_json(id: String) -> void:
	var data: Dictionary = records[id]
	if preload("res://scripts/desktop/http_source.gd").parse(data.source).is_empty():
		_fail(id, "Invalid JSON source URL.")
		return
	var request := HTTPRequest.new()
	request.body_size_limit = Sources.MAX_JSON_BYTES
	request.timeout = 30
	add_child(request)
	_requests[id] = request
	var generation: int = data.generation
	request.request_completed.connect(func(result: int, code: int, _headers: PackedStringArray, body: PackedByteArray):
		if _requests.get(id) == request: _requests.erase(id)
		request.queue_free()
		if not records.has(id) or records[id].generation != generation: return
		if result != HTTPRequest.RESULT_SUCCESS or code < 200 or code >= 300:
			_fail(id, "JSON download failed (HTTP %d, result %d)." % [code, result])
			return
		data["json_probe"] = Sources.json_text(body.get_string_from_utf8())
		_load(id))
	_set_state(id, "Downloading")
	var error := request.request(data.source)
	if error != OK:
		_requests.erase(id)
		request.queue_free()
		_fail(id, "Cannot request JSON: " + error_string(error))

func _import_brep(id: String) -> void:
	var data: Dictionary = records[id]
	if data.source.begins_with("http://") or data.source.begins_with("https://"):
		_fail(id, "Remote BREP is not supported in this preview. Open a local file.")
		return
	if not ClassDB.class_exists("BRepDocument"):
		_fail(id, "BREP extension is unavailable. Run tools/Build-BRep.ps1 and include its runtime DLLs.")
		return
	var scale_factor: float = data.options.get("unit_scale", 1.0)
	if not is_finite(scale_factor) or scale_factor <= 0:
		_fail(id, "BREP unit_scale must be finite and positive.")
		return
	var importer: RefCounted = ClassDB.instantiate("BRepDocument")
	var thread := Thread.new()
	var path := ProjectSettings.globalize_path(str(data.brep_path))
	var assembly_path: String = data.get("brep_assembly_path", "")
	var json_path := ProjectSettings.globalize_path(assembly_path) if not assembly_path.is_empty() else ""
	var options: Dictionary = data.options.duplicate(true)
	options["input_json"] = Sources.extension(data.brep_path) == "json"
	var error := thread.start(func() -> Dictionary: return importer.call("import_model", path, json_path, options))
	if error != OK:
		_fail(id, "Cannot start BREP importer: " + error_string(error))
		return
	_set_state(id, "Parsing")
	_jobs.append({"thread": thread, "id": id, "generation": data.generation, "kind": "brep", "importer": importer})

func _import_gltf(id: String, path: String) -> void:
	if not records.has(id): return
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		_fail(id, "Cannot open model: " + error_string(FileAccess.get_open_error()))
		return
	if file.get_length() > MAX_FILE_BYTES:
		_fail(id, "Model exceeds the 64 MiB per-content limit.")
		return
	file.close()
	_set_state(id, "Parsing")
	var thread := Thread.new()
	var error := thread.start(func() -> Dictionary:
		var document := GLTFDocument.new()
		var state := GLTFState.new()
		var result := document.append_from_file(path, state)
		return {"document": document, "state": state, "error": result})
	if error != OK:
		_fail(id, error_string(error))
		return
	_jobs.append({"thread": thread, "id": id, "generation": records[id].generation})

func _process(delta: float) -> void:
	if AppState.shutting_down: return
	for job in _jobs.duplicate():
		if job.thread.is_alive():
			if job.get("kind") == "brep" and records.has(job.id) and records[job.id].generation == job.generation:
				var stage: String = job.importer.call("get_stage")
				if records[job.id].state != stage: _set_state(job.id, stage)
			continue
		var result: Dictionary = job.thread.wait_to_finish()
		_jobs.erase(job)
		if not records.has(job.id) or records[job.id].generation != job.generation: continue
		if job.get("kind") == "brep":
			if not str(result.error).is_empty():
				_fail(job.id, "BREP import failed: " + str(result.error))
				continue
			_set_state(job.id, "Uploading")
			records[job.id]["brep_stats"] = result.stats
			records[job.id]["brep_warnings"] = result.warnings
			result["document"] = job.importer
			records[job.id].prototype = preload("res://scripts/desktop/brep_scene.gd").create(result, records[job.id].options)
			_finish(job.id)
			for warning in result.warnings:
				for ref in records[job.id].refs: AppState.report(ref.view, ref.layer, "brep", str(warning))
			continue
		if result.error != OK:
			_fail(job.id, "glTF import failed: " + error_string(result.error))
			continue
		var scene: Node = result.document.generate_scene(result.state)
		if scene == null:
			_fail(job.id, "glTF scene generation failed.")
			continue
		records[job.id].prototype = scene
		_finish(job.id)
	for id in _retired_files.duplicate():
		var busy := false
		for job in _jobs:
			if job.id == id: busy = true
		if not busy:
			_cleanup_files(id)
			_retired_files.erase(id)
	_tick += delta
	if _tick < 0.5: return
	_tick = 0
	# Retire superseded generations only once their parser has stopped reading.
	for id in records:
		_cleanup_files(id, records[id].generation)
	var count := 0
	var status_updated := false
	for view in AppState.views.values():
		for layer in view.layers:
			if layer.kind == "tiled" and is_instance_valid(layer.node): count += 1
	for view in AppState.views.values():
		for layer in view.layers:
			if layer.kind != "tiled" or not is_instance_valid(layer.node): continue
			var node: Node = layer.node
			node.set("memory_budget_mb", maxi(1, 1024 / maxi(1, count)))
			node.set("upload_budget_ms", 4.0 / maxi(1, count))
			node.set("lod_bias", 1.0 if view.id == AppState.active_id else maxf(1.5, sqrt(AppState.views.size())))
			layer["stats"] = node.call("get_streaming_stats")
			if layer.error.is_empty():
				var statuses := {"unloaded": "Queued", "requested": "Downloading", "downloaded": "Parsing", "parsing": "Parsing", "cpu_ready": "Uploading", "uploading": "Uploading", "resident": "Ready", "failed": "Failed", "cancelled": "Cancelled"}
				var next: String = "Paused" if layer.paused else "Ready" if layer.stats.get("resident_contents", 0) > 0 else statuses.get(layer.stats.get("root_state", ""), "Loading")
				if layer.status != next:
					layer.status = next
					status_updated = true
	if status_updated: AppState.changed.emit()

func _instantiate(view_id: String, layer: Dictionary, data: Dictionary) -> void:
	if not AppState.views.has(view_id): return
	var node: Node3D = data.prototype.duplicate()
	node.name = layer.id
	layer.node = node
	AppState.views[view_id].root.add_child(node)
	if layer.has("transform"): node.transform = layer.transform
	layer.status = "Ready"
	layer.error = ""
	apply_display(layer)
	if data.kind == "ellipsoid":
		node.get_node("Grid").visible = AppState.views[view_id].grid_visible
	if AppState.views[view_id].layers.size() == 1:
		frame_layer(view_id, layer)

func _apply_layer_lighting(node: Node, mode: int) -> void:
	if node.has_method("set_receive_lighting_mode"):
		node.call("set_receive_lighting_mode", mode)

func _instantiate_tiled(view_id: String, layer: Dictionary, data: Dictionary) -> void:
	var node: Node3D = ClassDB.instantiate("TiledMeshInstance3D")
	node.set("auto_load", false)
	node.set("source_uri", data.source)
	node.set("max_tile_size_mb", 64)
	node.set("max_concurrent_requests", 8)
	node.set("upload_budget_ms", 4.0)
	if data.source.get_extension().to_lower() in ["osg", "osgb"] and ClassDB.class_exists("OsgTiledMeshOptions"):
		var options: Resource = ClassDB.instantiate("OsgTiledMeshOptions")
		options.set("up_axis", 0 if data.options.get("z_up", true) else 1)
		node.set("format_options", options)
		# ContextCapture tile roots already carry their tile-origin coordinates.
		# Recentering every root would stack all tiles at the same origin.
		node.set("world_origin_mode", 1)
		node.set("world_origin_x", 0.0)
		node.set("world_origin_y", 0.0)
		node.set("world_origin_z", 0.0)
	elif ClassDB.class_exists("Tiles3DOptions"):
		var options: Resource = ClassDB.instantiate("Tiles3DOptions")
		options.set("coordinate_system", data.options.get("coordinate_system", 0))
		if data.options.has("ion_asset_id"):
			options.set("ion_asset_id", data.options.ion_asset_id)
			options.set("ion_access_token", data.options.ion_token)
		node.set("format_options", options)
	layer.node = node
	AppState.views[view_id].root.add_child(node)
	if layer.has("transform"): node.transform = layer.transform
	_apply_layer_lighting(node, layer.get("receive_lighting_mode", -1))
	node.connect("load_failed", func(_uri: String, message: String): _fail(data.id, message))
	node.connect("tile_failed", func(_uri: String, message: String):
		if not records.has(data.id): return
		layer.error = AppState.redact(message)
		layer.status = "Partial failure"
		AppState.report(view_id, layer.id, "stream", message))
	node.connect("root_loaded", func():
		if not records.has(data.id): return
		layer.status = "Ready"
		frame_layer(view_id, layer)
		AppState.changed.emit())
	var error: int = node.call("open")
	if error != OK: _fail(data.id, "Open failed: " + error_string(error))
	apply_display(layer)

func _finish(id: String) -> void:
	var data: Dictionary = records[id]
	data.state = "Ready"
	for ref in data.refs:
		_instantiate(ref.view, AppState.views[ref.view].get_layer(ref.layer), data)
	AppState.changed.emit()

func apply_display(layer: Dictionary) -> void:
	if not is_instance_valid(layer.node): return
	layer.node.visible = layer.visible
	_apply_opacity(layer.node, layer.opacity)
	layer.node.process_mode = Node.PROCESS_MODE_DISABLED if layer.paused else Node.PROCESS_MODE_INHERIT
	if layer.node.has_method("set_streaming_paused"): layer.node.call("set_streaming_paused", layer.paused)

func _apply_opacity(node: Node, opacity: float) -> void:
	if node is GeometryInstance3D: node.transparency = 1.0 - opacity
	for child in node.get_children(): _apply_opacity(child, opacity)

func frame_layer(view_id: String, layer: Dictionary) -> void:
	if not is_instance_valid(layer.node): return
	AppState.views[view_id].frame_bounds(layer_bounds(layer))

func layer_bounds(layer: Dictionary) -> AABB:
	var bounds := AABB()
	if layer.node.has_method("get_dataset_aabb"):
		bounds = layer.node.global_transform * layer.node.call("get_dataset_aabb")
	else:
		var all: Array[AABB] = []
		_bounds(layer.node, Transform3D.IDENTITY, all)
		if all.is_empty(): return bounds
		bounds = all[0]
		for box in all: bounds = bounds.merge(box)
	return bounds

func frame_all(view_id: String) -> void:
	var boxes: Array[AABB] = []
	for layer in AppState.views[view_id].layers:
		if layer.visible and is_instance_valid(layer.node): boxes.append(layer_bounds(layer))
	if boxes.is_empty(): return
	var bounds := boxes[0]
	for box in boxes: bounds = bounds.merge(box)
	AppState.views[view_id].frame_bounds(bounds)

func _bounds(node: Node, parent: Transform3D, result: Array[AABB]) -> void:
	var pose := parent
	if node is Node3D: pose = parent * node.transform
	if node is MeshInstance3D and node.mesh: result.append(pose * node.get_aabb())
	for child in node.get_children(): _bounds(child, pose, result)

func _set_state(id: String, state: String) -> void:
	if not records.has(id): return
	records[id].state = state
	for ref in records[id].refs:
		AppState.views[ref.view].get_layer(ref.layer).status = state
	AppState.changed.emit()

func _fail(id: String, message: String) -> void:
	if not records.has(id): return
	_set_state(id, "Failed")
	records[id].error = AppState.redact(tr(message))
	for ref in records[id].refs:
		AppState.views[ref.view].get_layer(ref.layer).error = records[id].error
		AppState.report(ref.view, ref.layer, "load", message)

func pause(view_id: String, layer_id: String, paused: bool) -> void:
	var layer: Dictionary = AppState.views[view_id].get_layer(layer_id)
	if not is_instance_valid(layer.node): return
	layer.paused = paused
	if layer.node.has_method("set_streaming_paused"): layer.node.call("set_streaming_paused", paused)
	layer.node.process_mode = Node.PROCESS_MODE_DISABLED if paused else Node.PROCESS_MODE_INHERIT
	layer.status = "Paused" if paused else "Ready"
	AppState.changed.emit()

func rebuild_instance(view_id: String, layer: Dictionary) -> void:
	_dispose_node(layer)
	var data: Dictionary = records[layer.data_id]
	if is_instance_valid(data.prototype): _instantiate(view_id, layer, data)
	elif data.kind == "tiled": _instantiate_tiled(view_id, layer, data)

func reload_data(id: String) -> void:
	if not records.has(id): return
	# Script imports are editable documents; opening never discards unsaved edits.
	if records[id].has("script_id"):
		AppState.scripts.opened.emit(records[id].script_id)
		return
	if _requests.has(id):
		_requests[id].cancel_request()
		_requests[id].queue_free()
		_requests.erase(id)
	var data: Dictionary = records[id]
	_cancel_jobs(id)
	data.erase("json_probe")
	for ref in data.refs: _dispose_node(AppState.views[ref.view].get_layer(ref.layer))
	if is_instance_valid(data.prototype): data.prototype.free()
	data.prototype = null
	data.error = ""
	data.source = data.original_source
	data.kind = data.original_kind
	_set_state(id, "Queued")
	_load(id)

func _dispose_node(layer: Dictionary) -> void:
	if is_instance_valid(layer.node):
		if layer.node.has_method("close"): layer.node.call("close")
		layer.node.get_parent().remove_child(layer.node)
		layer.node.queue_free()
	layer.node = null

func detach(view_id: String, layer: Dictionary) -> void:
	_dispose_node(layer)
	var data: Dictionary = records[layer.data_id]
	for ref in data.refs.duplicate():
		if ref.view == view_id and ref.layer == layer.id: data.refs.erase(ref)
	if not data.refs.is_empty(): return
	_cancel_jobs(data.id)
	if _requests.has(data.id):
		_requests[data.id].cancel_request()
		_requests[data.id].queue_free()
		_requests.erase(data.id)
	if is_instance_valid(data.prototype): data.prototype.free()
	records.erase(data.id)
	if data.has("script_id"): AppState.scripts.remove(data.script_id)
	_retired_files.append(data.id)

func _cancel_jobs(id: String) -> void:
	for job in _jobs:
		if job.id == id and job.has("importer"): job.importer.call("cancel")

func _ellipsoid() -> MeshInstance3D:
	var globe := MeshInstance3D.new()
	var sphere := SphereMesh.new()
	sphere.radius = 2.0
	sphere.height = 4.0 * Geo.B / Geo.A
	sphere.radial_segments = 128
	sphere.rings = 64
	globe.mesh = sphere
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("397b89")
	material.roughness = 0.9
	globe.material_override = material
	var lines := ImmediateMesh.new()
	lines.surface_begin(Mesh.PRIMITIVE_LINES)
	for latitude in range(-75, 76, 15):
		for longitude in range(-180, 180):
			lines.surface_add_vertex(_globe_point(longitude, latitude))
			lines.surface_add_vertex(_globe_point(longitude + 1, latitude))
	for longitude in range(-180, 180, 15):
		for latitude in range(-90, 90):
			lines.surface_add_vertex(_globe_point(longitude, latitude))
			lines.surface_add_vertex(_globe_point(longitude, latitude + 1))
	lines.surface_end()
	var grid := MeshInstance3D.new()
	grid.name = "Grid"
	grid.mesh = lines
	var line_material := StandardMaterial3D.new()
	line_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	line_material.albedo_color = Color("75aaab")
	grid.material_override = line_material
	globe.add_child(grid)
	return globe

func _globe_point(longitude: float, latitude: float) -> Vector3:
	var p := Geo.ecef(longitude, latitude, 1500)
	return Vector3(p[1], p[2], p[0]) * (2.0 / Geo.A)

func _cache() -> String:
	if _cache_root.is_empty():
		_cache_root = "user://session_%d_%d" % [OS.get_process_id(), Time.get_ticks_usec()]
		DirAccess.make_dir_recursive_absolute(_cache_root)
	return _cache_root

func _download(id: String) -> void:
	var data: Dictionary = records[id]
	var request := preload("res://scripts/desktop/remote_gltf.gd").new()
	add_child(request)
	_requests[id] = request
	var generation: int = data.generation
	var directory := _cache().path_join("%s_%d" % [id, generation])
	_track_directory(id, directory)
	request.cached.connect(func(path: String):
		_temporary.append(path)
		_track_file(id, path))
	request.progress.connect(func(done: int, total: int, bytes: int):
		if not records.has(id) or records[id].generation != generation: return
		data["download"] = {"completed": done, "dependencies": total, "bytes": bytes}
		AppState.changed.emit())
	request.completed.connect(func(path: String, message: String):
		if _requests.get(id) == request: _requests.erase(id)
		request.queue_free()
		if not records.has(id) or records[id].generation != generation: return
		if not message.is_empty():
			_cleanup_files(id)
			_fail(id, message)
			return
		data["local_source"] = path
		_import_gltf(id, path))
	_set_state(id, "Downloading")
	request.start(data.source, directory, data.options.get("headers", {}))

func _open_zip(id: String) -> void:
	var data: Dictionary = records[id]
	var inspection: Dictionary = preload("res://scripts/desktop/zip_guard.gd").inspect(data.source)
	if inspection.has("error"):
		_fail(id, inspection.error)
		return
	var zip := ZIPReader.new()
	if zip.open(data.source) != OK:
		_fail(id, "Cannot open ZIP.")
		return
	var entries := zip.get_files()
	var candidates: Array[String] = []
	if entries.size() > 4096:
		zip.close()
		_fail(id, "ZIP exceeds 4096 entries.")
		return
	for entry in entries:
		var path := entry.replace("\\", "/")
		if path.is_absolute_path() or ":" in path or ".." in path.split("/") or path.begins_with("/"):
			zip.close()
			_fail(id, "Unsafe ZIP path rejected.")
			return
	candidates = Sources.zip_candidates(zip, Profile.features())
	# Ambiguous archives must never select a random source.
	var entry: String = data.options.get("entry", "")
	if entry.is_empty() and candidates.size() == 1: entry = candidates[0]
	if entry not in candidates:
		zip.close()
		_fail(id, "ZIP needs an explicit model entry. Candidates: " + ", ".join(candidates).left(500))
		return
	# ZIPReader inflates into memory; inspect compressed input cap before extraction.
	var archive := FileAccess.open(data.source, FileAccess.READ)
	if archive == null or archive.get_length() > MAX_FILE_BYTES:
		zip.close()
		_fail(id, "ZIP exceeds 64 MiB.")
		return
	var base := _cache().path_join("%s_%d" % [id, data.generation])
	_track_directory(id, base)
	DirAccess.make_dir_recursive_absolute(base)
	var total := 0
	for validated: Dictionary in inspection.entries:
		var path: String = validated.name
		if path.ends_with("/"): continue
		var bytes := zip.read_file(path)
		if bytes.size() != validated.size:
			zip.close()
			_fail(id, "ZIP entry is corrupted or truncated.")
			return
		total += bytes.size()
		if total > 256 * 1024 * 1024 or bytes.size() > MAX_FILE_BYTES:
			zip.close()
			_fail(id, "ZIP expanded size exceeds the session limit.")
			return
		var destination := base.path_join(path.replace("\\", "/"))
		DirAccess.make_dir_recursive_absolute(destination.get_base_dir())
		var file := FileAccess.open(destination, FileAccess.WRITE)
		if file == null:
			zip.close()
			_fail(id, "Cannot extract ZIP to session cache.")
			return
		file.store_buffer(bytes)
		file.close()
		_temporary.append(destination)
		_track_file(id, destination)
	zip.close()
	data.source = base.path_join(entry)
	_set_kind(data, Sources.kind(data.source))
	# _load starts a generation; extraction files belong to that generation too.
	data.generation -= 1
	_load(id)

func shutdown() -> void:
	for request in _requests.values():
		if is_instance_valid(request): request.cancel_request()
	_requests.clear()
	for job in _jobs:
		if job.has("importer"): job.importer.call("cancel")
	for job in _jobs:
		if job.thread.is_started(): job.thread.wait_to_finish()
	_jobs.clear()
	for data in records.values():
		if is_instance_valid(data.prototype): data.prototype.free()
		for ref in data.refs:
			if AppState.views.has(ref.view): _dispose_node(AppState.views[ref.view].get_layer(ref.layer))
	records.clear()
	# Delete only exact files created by this session, then empty session directories.
	for path in _temporary:
		DirAccess.remove_absolute(path)
	_temporary.clear()
	_data_files.clear()
	_data_directories.clear()
	_file_generations.clear()
	_retired_files.clear()
	if not _cache_root.is_empty(): _remove_empty_directories(_cache_root)

func _track_file(id: String, path: String) -> void:
	if not _data_files.has(id): _data_files[id] = []
	if path not in _data_files[id]: _data_files[id].append(path)
	_file_generations[path] = records[id].generation

func _track_directory(id: String, path: String) -> void:
	if not _data_directories.has(id): _data_directories[id] = []
	_data_directories[id].append({"path": path, "generation": records[id].generation})

func _cleanup_files(id: String, before_generation: int = 2147483647) -> void:
	var busy: Array[int] = []
	for job in _jobs:
		if job.id == id: busy.append(job.generation)
	for path in _data_files.get(id, []).duplicate():
		var generation: int = _file_generations.get(path, 0)
		if generation >= before_generation or generation in busy: continue
		DirAccess.remove_absolute(path)
		_temporary.erase(path)
		_data_files[id].erase(path)
		_file_generations.erase(path)
	if _data_files.get(id, []).is_empty(): _data_files.erase(id)
	for directory in _data_directories.get(id, []).duplicate():
		if directory.generation >= before_generation or directory.generation in busy: continue
		_remove_empty_directories(directory.path)
		_data_directories[id].erase(directory)
	if _data_directories.get(id, []).is_empty(): _data_directories.erase(id)

func _remove_empty_directories(path: String) -> void:
	var directory := DirAccess.open(path)
	if directory == null: return
	for name in directory.get_directories(): _remove_empty_directories(path.path_join(name))
	DirAccess.remove_absolute(path)
