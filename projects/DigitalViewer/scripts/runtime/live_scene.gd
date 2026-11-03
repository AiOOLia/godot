extends Node
## One actual scene, rendered by both responsive layouts. Native scripts edit these objects.
signal changed
signal ownership_changed

const COLOR_ACCENT := Color("2cb79a")
const COLOR_VIEWPORT := Color("07131a")
var _subviewport: SubViewport
var _camera: Camera3D
var _earth_pivot: Node3D
var _earth_content: Node3D
var _grid_mesh_instance: MeshInstance3D
var _runtime_objects: Dictionary = {}
var _runtime_layout: Array = []
var _camera_distance := 6.2
var native_owned := false
var _last_rotation := Vector2(INF, INF)
var _last_distance := INF
var _native_baseline: Dictionary = {}
var _camera_baseline: Dictionary = {}


func _ready() -> void:
	_subviewport = SubViewport.new()
	_subviewport.name = "SharedSceneViewport"
	_subviewport.own_world_3d = true
	_subviewport.size = Vector2i(960, 540)
	_subviewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_subviewport.msaa_3d = Viewport.MSAA_2X
	add_child(_subviewport)
	_build_world()
	AppState.runtime.scene_changed.connect(_sync_runtime)
	_sync_runtime()


func _process(_delta: float) -> void:
	# Only explicit navigation changes the camera; do not overwrite native edits each frame.
	if is_instance_valid(_earth_pivot) and _last_rotation != AppState.camera_rotation:
		_earth_pivot.rotation = Vector3(AppState.camera_rotation.x, AppState.camera_rotation.y, 0)
		_last_rotation = AppState.camera_rotation
	if is_instance_valid(_camera) and _last_distance != AppState.camera_distance:
		_camera.position.z = AppState.camera_distance
		_last_distance = AppState.camera_distance


func begin_native() -> void:
	if native_owned:
		return
	AppState.runtime.stop_all()
	AppState.cancel_flight()
	_native_baseline = AppState.runtime.snapshot()
	_camera_baseline = AppState.camera_snapshot()
	native_owned = true
	AppState.revision += 1
	ownership_changed.emit()


func reset_native() -> void:
	# Explicitly destructive to this live 3D scene only. No rollback of arbitrary script effects.
	if native_owned:
		AppState.runtime.restore_snapshot(_native_baseline)
		AppState.restore_camera(_camera_baseline)
	for child: Node in _subviewport.get_children():
		_subviewport.remove_child(child)
		child.queue_free()
	_runtime_objects.clear()
	_runtime_layout.clear()
	_native_baseline.clear()
	_camera_baseline.clear()
	native_owned = false
	_last_rotation = Vector2(INF, INF)
	_last_distance = INF
	_build_world()
	_sync_runtime()
	AppState.revision += 1
	ownership_changed.emit()


func object(id: String) -> Object:
	var value: Object = _runtime_objects.get(id)
	return value if is_instance_valid(value) else null


func _build_world() -> void:
	var world := Node3D.new()
	world.name = "LiveScene"
	_subviewport.add_child(world)
	_runtime_objects["world"] = world

	var environment_node := WorldEnvironment.new()
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = COLOR_VIEWPORT
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color("8dbfc3")
	environment.ambient_light_energy = 0.42
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment_node.environment = environment
	world.add_child(environment_node)
	_runtime_objects["environment"] = environment

	_earth_pivot = Node3D.new()
	_earth_pivot.name = "EarthPivot"
	world.add_child(_earth_pivot)
	_earth_content = Node3D.new()
	_earth_content.name = "RuntimeEarth"
	_earth_pivot.add_child(_earth_content)
	_runtime_objects["earth"] = _earth_content

	var earth := MeshInstance3D.new()
	var sphere := SphereMesh.new()
	sphere.radius = 2.0
	sphere.height = 4.0
	sphere.radial_segments = 64
	sphere.rings = 32
	earth.mesh = sphere
	var earth_material := StandardMaterial3D.new()
	earth_material.albedo_color = Color("28798b")
	earth_material.metallic = 0.08
	earth_material.roughness = 0.8
	earth_material.emission_enabled = true
	earth_material.emission = Color("052d38")
	earth_material.emission_energy_multiplier = 0.28
	earth.material_override = earth_material
	_earth_content.add_child(earth)
	_runtime_objects["globe"] = earth

	_grid_mesh_instance = MeshInstance3D.new()
	_grid_mesh_instance.name = "LatitudeLongitudeGrid"
	_grid_mesh_instance.mesh = _create_grid_mesh(2.014)
	var grid_material := StandardMaterial3D.new()
	grid_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	grid_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	grid_material.albedo_color = Color(0.68, 0.98, 0.91, 0.48)
	_grid_mesh_instance.material_override = grid_material
	_earth_content.add_child(_grid_mesh_instance)
	_runtime_objects["grid"] = _grid_mesh_instance

	var marker := MeshInstance3D.new()
	var marker_mesh := SphereMesh.new()
	marker_mesh.radius = 0.055
	marker_mesh.height = 0.11
	marker.mesh = marker_mesh
	marker.position = Vector3(0.61, 0.64, 1.82)
	var marker_material := StandardMaterial3D.new()
	marker_material.albedo_color = COLOR_ACCENT
	marker_material.emission_enabled = true
	marker_material.emission = COLOR_ACCENT
	marker_material.emission_energy_multiplier = 1.8
	marker.material_override = marker_material
	_earth_content.add_child(marker)
	_runtime_objects["marker"] = marker

	var key_light := DirectionalLight3D.new()
	key_light.rotation_degrees = Vector3(-24.0, -35.0, 0.0)
	key_light.light_color = Color("d8f6f0")
	key_light.light_energy = 1.35
	key_light.shadow_enabled = true
	world.add_child(key_light)
	_runtime_objects["key_light"] = key_light

	var rim_light := DirectionalLight3D.new()
	rim_light.rotation_degrees = Vector3(15.0, 145.0, 0.0)
	rim_light.light_color = Color("4b9ec4")
	rim_light.light_energy = 0.55
	world.add_child(rim_light)
	_runtime_objects["rim_light"] = rim_light

	_camera = Camera3D.new()
	_camera.name = "ViewerCamera"
	_camera.position = Vector3(0.0, 0.0, _camera_distance)
	_camera.fov = 48.0
	_camera.near = 0.05
	_camera.far = 100.0
	world.add_child(_camera)
	_camera.current = true
	_runtime_objects["camera"] = _camera


func _sync_runtime() -> void:
	if native_owned:
		return
	var runtime: Node = AppState.runtime
	var layout: Array = []
	for record: Dictionary in runtime.objects.values():
		if not record.builtin: layout.append([record.id, record.type, record.parent])
	if layout != _runtime_layout:
		for old: Array in _runtime_layout:
			var object: Node = _runtime_objects.get(old[0])
			if is_instance_valid(object) and object.get_parent() != null:
				object.get_parent().remove_child(object)
				object.queue_free()
			_runtime_objects.erase(old[0])
		_runtime_layout = layout
	# Parents precede children in the authoritative insertion-ordered registry.
	for id: String in runtime.objects:
		var record: Dictionary = runtime.objects[id]
		if not _runtime_objects.has(id):
			var object: Node = runtime.make_object(record.type)
			object.name = "Runtime_" + id
			_runtime_objects[record.parent].add_child(object)
			_runtime_objects[id] = object
		runtime.apply_properties(_runtime_objects[id], record)
	changed.emit()


func _create_grid_mesh(radius: float) -> ImmediateMesh:
	var mesh := ImmediateMesh.new()
	mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	const STEPS := 96
	for latitude_degrees: int in range(-60, 61, 30):
		var latitude := deg_to_rad(float(latitude_degrees))
		for step: int in STEPS:
			var longitude_a := TAU * float(step) / float(STEPS)
			var longitude_b := TAU * float(step + 1) / float(STEPS)
			mesh.surface_add_vertex(Vector3(radius * cos(latitude) * sin(longitude_a), radius * sin(latitude), radius * cos(latitude) * cos(longitude_a)))
			mesh.surface_add_vertex(Vector3(radius * cos(latitude) * sin(longitude_b), radius * sin(latitude), radius * cos(latitude) * cos(longitude_b)))
	for longitude_degrees: int in range(0, 360, 30):
		var longitude := deg_to_rad(float(longitude_degrees))
		for step: int in STEPS:
			var latitude_a := -PI * 0.5 + PI * float(step) / float(STEPS)
			var latitude_b := -PI * 0.5 + PI * float(step + 1) / float(STEPS)
			mesh.surface_add_vertex(Vector3(radius * cos(latitude_a) * sin(longitude), radius * sin(latitude_a), radius * cos(latitude_a) * cos(longitude)))
			mesh.surface_add_vertex(Vector3(radius * cos(latitude_b) * sin(longitude), radius * sin(latitude_b), radius * cos(latitude_b) * cos(longitude)))
	mesh.surface_end()
	return mesh
