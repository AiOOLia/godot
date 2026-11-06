extends Node
## Scene lifetime is independent of UI panes and native windows.
signal backdrop_changed
const Geo = preload("res://scripts/desktop/geo.gd")
const Profile = preload("res://scripts/desktop/build_profile.gd")
var id := ""
var viewport: SubViewport
var root: Node3D
var camera: Camera3D
var environment: Environment
var light: Light3D
var lights: Array[Dictionary] = []
var background_mode := 1
var background_image_path := ""
var panorama_path := ""
var flat_path := ""
var flat_texture: Texture2D
var sky: Sky
var sky_material: Material
var light_serial := 0
var layers: Array[Dictionary] = []
var selected := ""
var selection_kind := "viewport"
var show_viewport_properties := false
var property_section_expanded: Dictionary = {}
var selected_node: WeakRef
var selected_face: Dictionary = {}
var target := Vector3.ZERO
var angles := Vector2(0.3, 0.4)
var distance := 8.0
var viewpoints: Dictionary = {}
var flight: Tween
var grid_visible := false
var render_mode := 0
var camera_mode := 0
var last_pick := {}
var _pick_meshes := {}
var _frame := 0
var _render_resize_hold := 0
var _pick_marker: MeshInstance3D
var _selection_highlight: Node3D

func highlight_selection(nodes: Array[Node], face: Dictionary = {}) -> void:
	selected_face = face.duplicate()
	if not is_instance_valid(_selection_highlight):
		_selection_highlight = preload("res://scripts/desktop/selection_highlight.gd").new()
		_selection_highlight.name = "SelectionHighlight"
		root.add_child(_selection_highlight)
	_selection_highlight.select_nodes(nodes, face)
	if is_instance_valid(_pick_marker): _pick_marker.hide()

func _ready() -> void:
	viewport = SubViewport.new()
	viewport.own_world_3d = true
	viewport.world_3d = World3D.new()
	viewport.size = Vector2i(640, 480)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(viewport)
	_build_world()

func set_render_size(value: Vector2i) -> void:
	if not is_instance_valid(viewport) or viewport.size == value:
		return
	viewport.size = value
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_render_resize_hold = 4

func set_background_mode(mode: int) -> void:
	background_mode = clampi(mode, 0, 3)
	if not is_instance_valid(environment):
		return
	viewport.transparent_bg = false
	if background_mode == 0:
		environment.background_mode = Environment.BG_COLOR
		environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
		environment.ambient_light_color = Color.WHITE
		environment.ambient_light_energy = 0.35
		environment.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED
		backdrop_changed.emit()
		refresh_lighting()
		return
	if background_mode == 3:
		if flat_texture == null and not flat_path.is_empty():
			var flat_image := Image.new()
			if flat_image.load(flat_path) == OK:
				flat_texture = ImageTexture.create_from_image(flat_image)
		viewport.transparent_bg = true
		environment.background_mode = Environment.BG_CLEAR_COLOR
		environment.background_color = Color(0, 0, 0, 0)
		environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
		environment.ambient_light_color = Color.WHITE
		environment.ambient_light_energy = 0.35
		environment.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED
		backdrop_changed.emit()
		refresh_lighting()
		return
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	environment.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	sky = Sky.new()
	if background_mode == 2:
		var material := PanoramaSkyMaterial.new()
		material.filter = true
		if not panorama_path.is_empty():
			var image := Image.new()
			if image.load(panorama_path) == OK:
				material.panorama = ImageTexture.create_from_image(image)
		sky_material = material
	else:
		sky_material = ProceduralSkyMaterial.new()
	sky.sky_material = sky_material
	environment.sky = sky
	environment.background_mode = Environment.BG_SKY
	backdrop_changed.emit()
	refresh_lighting()

func set_panorama_image(path: String) -> bool:
	var image := Image.new()
	if image.load(path) != OK:
		return false
	var ratio := float(image.get_width()) / float(maxi(1, image.get_height()))
	if absf(ratio - 2.0) > 0.35:
		return false
	panorama_path = path
	background_image_path = path
	set_background_mode(2)
	return true

func set_flat_image(path: String) -> void:
	flat_path = path
	var image := Image.new()
	flat_texture = null
	if image.load(path) == OK:
		flat_texture = ImageTexture.create_from_image(image)
	set_background_mode(3)

func add_light(type := 0) -> Dictionary:
	type = clampi(type, 0, 2)
	var node: Light3D
	match type:
		1: node = OmniLight3D.new()
		2: node = SpotLight3D.new()
		_: node = DirectionalLight3D.new()
	light_serial += 1
	node.name = "Light_%d" % light_serial
	root.add_child(node)
	if type != 0 and is_instance_valid(camera):
		# Local lights must reach the framed model, including large CAD assemblies.
		node.position = camera.position
		var reach := maxf(0.01, camera.position.distance_to(target) * 2.0)
		if node is OmniLight3D:
			node.omni_range = reach
		elif node is SpotLight3D:
			node.spot_range = reach
			node.rotation = camera.rotation
	var entry := {"node": node, "name": node.name, "type": type}
	if node is DirectionalLight3D:
		entry["follow_camera"] = false
	lights.append(entry)
	if light == null or not is_instance_valid(light):
		light = node
	refresh_lighting()
	return entry

func set_light_follow_camera(node: Light3D, enabled: bool) -> void:
	if not is_instance_valid(node) or not node is DirectionalLight3D:
		return
	for entry in lights:
		if entry.node != node: continue
		if bool(entry.get("follow_camera", false)) == enabled: return
		entry["follow_camera"] = enabled
		if enabled:
			entry["manual_basis"] = node.basis
			_sync_camera_lights()
		else:
			node.basis = entry.manual_basis
			entry.erase("manual_basis")
		return

func _sync_camera_lights() -> void:
	if not is_instance_valid(camera): return
	var camera_basis := camera.global_basis.orthonormalized()
	for entry in lights:
		if not entry.get("follow_camera", false): continue
		var node: Light3D = entry.node
		if is_instance_valid(node) and node is DirectionalLight3D and not node.global_basis.is_equal_approx(camera_basis):
			node.global_basis = camera_basis

func remove_light(index: int) -> void:
	if index < 0 or index >= lights.size():
		return
	var entry: Dictionary = lights[index]
	var node: Node = entry.node
	lights.remove_at(index)
	if node == light:
		light = lights[0].node if not lights.is_empty() else null
	if is_instance_valid(node):
		node.queue_free()
	refresh_lighting()

func refresh_lighting() -> void:
	var lit := false
	for entry in lights:
		if is_instance_valid(entry.node) and entry.node.visible:
			lit = true
			break
	if not lit:
		environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
		environment.ambient_light_color = Color.WHITE
		environment.ambient_light_energy = 1.0
		return
	if background_mode in [1, 2]:
		environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
		environment.ambient_light_energy = 1.0
	else:
		environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
		environment.ambient_light_color = Color.WHITE
		environment.ambient_light_energy = 0.35

func _build_world() -> void:
	root = Node3D.new()
	root.name = "Scene_" + id
	viewport.add_child(root)
	var env := WorldEnvironment.new()
	environment = Environment.new()
	env.environment = environment
	root.add_child(env)
	lights.clear()
	add_light(0)
	camera = Camera3D.new()
	camera.name = "Camera"
	camera.fov = 48
	camera.near = 0.2
	camera.far = 100000000.0
	root.add_child(camera)
	camera.current = true
	update_camera()
	set_background_mode(background_mode)

func rebuild_world() -> void:
	cancel_flight()
	light = null
	lights.clear()
	if is_instance_valid(root):
		viewport.remove_child(root)
		root.queue_free()
	_pick_marker = null
	clear_pick_cache()
	_build_world()

func _process(_delta: float) -> void:
	_frame += 1
	_sync_camera_lights()
	if is_instance_valid(_pick_marker) and is_instance_valid(camera):
		var marker_size := maxf(0.001, camera.global_position.distance_to(_pick_marker.global_position) * 0.012)
		_pick_marker.scale = Vector3.ONE * marker_size
	# Inactive panes render at 30 fps on the 60 fps desktop loop.
	if _render_resize_hold > 0:
		_render_resize_hold -= 1
		viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	elif AppState.views.size() > 1 and id != AppState.active_id:
		viewport.render_target_update_mode = SubViewport.UPDATE_ONCE if _frame % 2 == 0 else SubViewport.UPDATE_DISABLED
	else:
		viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS

func update_camera() -> void:
	if not is_instance_valid(camera):
		return
	var direction := Vector3(cos(angles.x) * sin(angles.y), sin(angles.x), cos(angles.x) * cos(angles.y))
	camera.position = target + direction * distance
	camera.look_at(target, Vector3.UP)
	camera.size = distance
	camera.far = maxf(1000.0, distance * 100.0)
	_sync_camera_lights()

func cancel_flight() -> void:
	if flight and flight.is_valid():
		flight.kill()

func orbit(delta: Vector2) -> void:
	cancel_flight()
	angles.x = clampf(angles.x + delta.y, -1.56, 1.56)
	angles.y += delta.x
	update_camera()

func pan(delta: Vector2) -> void:
	cancel_flight()
	target += (-camera.basis.x * delta.x + camera.basis.y * delta.y) * distance * 0.002
	update_camera()

func zoom(factor: float) -> void:
	cancel_flight()
	distance = clampf(distance * factor, 0.01, 100000000.0)
	update_camera()

func frame_bounds(bounds: AABB) -> void:
	cancel_flight()
	target = bounds.get_center()
	distance = maxf(0.1, bounds.size.length() * 1.4)
	update_camera()

func move_flight(input: Vector3, delta: float, fast: bool) -> void:
	cancel_flight()
	var movement := camera.basis * input * maxf(0.1, distance * 0.6) * delta * (4.0 if fast else 1.0)
	target += movement
	update_camera()

func set_render_mode(value: int) -> void:
	render_mode = clampi(value, 0, 2)
	if render_mode == 2: RenderingServer.set_debug_generate_wireframes(true)
	viewport.debug_draw = [Viewport.DEBUG_DRAW_DISABLED, Viewport.DEBUG_DRAW_UNSHADED, Viewport.DEBUG_DRAW_WIREFRAME][render_mode]

func fly_to(longitude: float, latitude: float, height: float) -> void:
	if not Profile.enabled("gis"): return
	cancel_flight()
	var to := Vector2(deg_to_rad(clampf(latitude, -89.9, 89.9)), angles.y + wrapf(deg_to_rad(longitude) - angles.y, -PI, PI))
	flight = create_tween().set_parallel(true).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN_OUT)
	flight.tween_property(self, "angles", to, 1.2)
	flight.tween_property(self, "distance", 2.0 + height / Geo.A * 2.0, 1.2)
	flight.tween_property(self, "target", Vector3.ZERO, 1.2)
	flight.tween_method(func(_v: float): update_camera(), 0.0, 1.0, 1.2)

func pose() -> Dictionary:
	return {"target": target, "angles": angles, "distance": distance, "projection": camera.projection, "fov": camera.fov}

func restore_pose(value: Dictionary) -> void:
	cancel_flight()
	target = value.target
	angles = value.angles
	distance = value.distance
	camera.projection = value.projection
	camera.fov = value.fov
	update_camera()

func get_layer(layer_id: String) -> Dictionary:
	for layer in layers:
		if layer.id == layer_id:
			return layer
	return {}

func object(key: String) -> Object:
	match key:
		"world", "root", "earth": return root
		"camera": return camera
		"environment": return environment
		"globe", "grid":
			for layer in layers:
				if layer.kind == "ellipsoid" and is_instance_valid(layer.node):
					return layer.node if key == "globe" else layer.node.get_node_or_null("Grid")
	return null

func set_grid(value: bool) -> void:
	if not Profile.enabled("gis"): return
	grid_visible = value
	for layer in layers:
		if layer.kind == "ellipsoid" and is_instance_valid(layer.node):
			layer.node.get_node("Grid").visible = value

func pick(at: Vector2) -> Dictionary:
	var best := {}
	var origin := camera.project_ray_origin(at)
	var direction := camera.project_ray_normal(at)
	for layer in layers:
		var node: Node3D = layer.node
		if not is_instance_valid(node) or not layer.visible:
			continue
		if node.has_method("intersect_ray"):
			var hit: Dictionary = node.call("intersect_ray", origin, direction)
			if not hit.is_empty() and (best.is_empty() or hit.distance < best.distance):
				best = hit
				best["layer"] = layer.id
		else:
			var candidates: Array = []
			_mesh_pick(node, origin, direction, candidates)
			for hit: Dictionary in candidates:
				if best.is_empty() or hit.distance < best.distance:
					best = hit
					best["layer"] = layer.id
	last_pick = best
	if not best.is_empty() and not best.has("brep"): _show_pick_marker(best.position)
	elif is_instance_valid(_pick_marker): _pick_marker.hide()
	return best

func _show_pick_marker(position: Vector3) -> void:
	if not is_instance_valid(_pick_marker):
		_pick_marker = MeshInstance3D.new()
		_pick_marker.name = "PickMarker"
		var mesh := SphereMesh.new()
		mesh.radius = 0.5
		mesh.height = 1.0
		mesh.radial_segments = 16
		mesh.rings = 8
		_pick_marker.mesh = mesh
		var material := StandardMaterial3D.new()
		material.albedo_color = Color("ffb65c")
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		material.no_depth_test = true
		_pick_marker.material_override = material
		_pick_marker.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		root.add_child(_pick_marker)
	_pick_marker.global_position = position
	_pick_marker.show()

func _mesh_pick(node: Node, origin: Vector3, direction: Vector3, hits: Array) -> void:
	if node is MeshInstance3D and node.mesh and not node.mesh.get_meta("brep_lines", false) and node.is_visible_in_tree() and not is_zero_approx(node.global_basis.determinant()):
		var transform: Transform3D = node.global_transform
		var inverse := transform.affine_inverse()
		var local_origin := inverse * origin
		var local_direction := (inverse.basis * direction).normalized()
		if node.mesh.get_aabb().intersects_ray(local_origin, local_direction) != null:
			var key: int = node.mesh.get_instance_id()
			if not _pick_meshes.has(key):
				var triangles: TriangleMesh = node.mesh.generate_triangle_mesh()
				_pick_meshes[key] = triangles
			var triangles: TriangleMesh = _pick_meshes[key]
			if triangles:
				var hit := triangles.intersect_ray(local_origin, local_direction)
				if not hit.is_empty():
					hit.position = transform * hit.position
					hit.normal = (inverse.basis.transposed() * hit.normal).normalized()
					hit["distance"] = origin.distance_to(hit.position)
					if node.mesh.has_meta("brep_face_ranges"):
						hit["node"] = weakref(node)
						hit["occurrence"] = node.get_meta("brep_occurrence", -1)
						hit["object_persistent_id"] = node.get_meta("brep_persistent_id", 0)
						var triangle: int = hit.get("face_index", -1)
						for face: Dictionary in node.mesh.get_meta("brep_face_ranges"):
							if triangle >= face.triangle_first and triangle < face.triangle_first + face.triangle_count:
								hit["brep"] = face.duplicate()
								break
					hits.append(hit)
	for child in node.get_children(): _mesh_pick(child, origin, direction, hits)

func clear_pick_cache() -> void:
	_pick_meshes.clear()
	last_pick.clear()
	selected_face.clear()
	if is_instance_valid(_selection_highlight): _selection_highlight.clear()
	if is_instance_valid(_pick_marker): _pick_marker.hide()
