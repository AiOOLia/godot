extends RefCounted
## Hardware polygon offset regression. Called by the BREP suite on every renderer.

const CONSTANT := 0.5
const SLOPE := 0.5
const SEPARATION := 0.02
var _test: Node
var _viewport: SubViewport
var _camera: Camera3D
var _world: Node3D


func run(test: Node) -> void:
	_test = test
	_resources()
	if DisplayServer.get_name() == "headless":
		print("DEPTH_BIAS: pixel checks skipped on headless display")
		return
	_viewport = SubViewport.new()
	_viewport.size = Vector2i(640, 480)
	_viewport.own_world_3d = true
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_test.add_child(_viewport)
	_world = Node3D.new()
	_viewport.add_child(_world)
	var environment := WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.background_mode = Environment.BG_COLOR
	environment.environment.background_color = Color.WHITE
	_world.add_child(environment)
	_camera = Camera3D.new()
	_viewport.add_child(_camera)
	for projection in [Camera3D.PROJECTION_PERSPECTIVE, Camera3D.PROJECTION_ORTHOGONAL]:
		for model_scale in [1.0, 10000.0]:
			for direction in [Vector3(0, 0, 10), Vector3(5, 3, 9), Vector3(5, 3, 2)]:
				await _edges(projection, model_scale, direction)
	await _material_isolation()
	await _shadows()
	_viewport.queue_free()
	await _test.get_tree().process_frame


func _resources() -> void:
	var material := StandardMaterial3D.new()
	_test.check(not material.depth_bias_enabled and material.depth_bias_constant_factor == 0.0 and material.depth_bias_slope_factor == 0.0, "depth bias defaults preserve existing materials")
	# Set before creating the lazy material RID, then mutate the live material.
	material.depth_bias_constant_factor = CONSTANT
	material.depth_bias_slope_factor = SLOPE
	material.depth_bias_enabled = true
	_test.check(material.get_rid().is_valid(), "biased material creates a valid lazy RID")
	var path := "user://material-depth-bias-test.tres"
	_test.check(ResourceSaver.save(material, path) == OK, "depth bias resource saves")
	var restored: StandardMaterial3D = ResourceLoader.load(path, "", ResourceLoader.CACHE_MODE_IGNORE)
	_test.check(restored != null and restored.depth_bias_enabled and restored.depth_bias_constant_factor == CONSTANT and restored.depth_bias_slope_factor == SLOPE, "depth bias survives resource round trip")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	material.depth_bias_enabled = false
	var copy: StandardMaterial3D = material.duplicate()
	_test.check(not copy.depth_bias_enabled and copy.depth_bias_slope_factor == SLOPE, "disabled bias keeps factors when duplicated")
	var shader := Shader.new()
	shader.code = "shader_type spatial; render_mode unshaded; void fragment() { ALBEDO = vec3(1.0); }"
	var custom := ShaderMaterial.new()
	custom.depth_bias_enabled = true
	custom.depth_bias_constant_factor = CONSTANT
	custom.shader = shader
	_test.check(custom.get_rid().is_valid() and custom.depth_bias_enabled, "spatial ShaderMaterial accepts depth bias before shader assignment")
	shader.code += "\n// Recompile without dropping material rasterization state."
	_test.check(custom.depth_bias_constant_factor == CONSTANT, "shader replacement retains material depth bias")


func _clear_world() -> void:
	for child in _world.get_children():
		if child is not WorldEnvironment:
			child.free()


func _white() -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.albedo_color = Color.WHITE
	return material


func _panel(material: Material, left: float, right: float, bottom: float, top: float, z: float) -> MeshInstance3D:
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(left, bottom, z), Vector3(right, bottom, z), Vector3(right, top, z), Vector3(left, top, z)])
	arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([Vector3.FORWARD, Vector3.FORWARD, Vector3.FORWARD, Vector3.FORWARD])
	arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2, 0, 2, 3])
	var node := MeshInstance3D.new()
	node.mesh = ArrayMesh.new()
	node.mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	node.material_override = material
	_world.add_child(node)
	return node


func _lines() -> MeshInstance3D:
	var vertices := PackedVector3Array()
	for x in [-2.0, 0.0, 1.0, 2.0]:
		for segment in 64:
			var z := -SEPARATION if x == 1.0 else 0.0
			vertices.append(Vector3(x, -2.5 + segment * 5.0 / 64.0, z))
			vertices.append(Vector3(x, -2.5 + (segment + 1) * 5.0 / 64.0, z))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	var node := MeshInstance3D.new()
	node.mesh = ArrayMesh.new()
	node.mesh.add_surface_from_arrays(Mesh.PRIMITIVE_LINES, arrays)
	var material := _white()
	material.albedo_color = Color.BLACK
	node.material_override = material
	_world.add_child(node)
	return node


func _setup_camera(projection: int, model_scale: float, direction := Vector3(5, 3, 9)) -> void:
	_world.scale = Vector3.ONE * model_scale
	_camera.projection = projection
	_camera.size = 10.0 * model_scale
	_camera.position = direction * model_scale
	_camera.look_at(Vector3.ZERO)
	# Scale the complete synthetic scene to test hardware bias independently of app navigation.
	_camera.near = 0.2 * model_scale
	_camera.far = 1000.0 * model_scale


func _image() -> Image:
	# Allow render-thread changes and asynchronously compiled pipelines to settle.
	for frame in 4:
		await _test.get_tree().process_frame
		await RenderingServer.frame_post_draw
	return _viewport.get_texture().get_image()


func _dark(image: Image, point: Vector3, model_scale: float) -> bool:
	var pixel := Vector2i(_camera.unproject_position(point * model_scale).round())
	for y in range(pixel.y - 1, pixel.y + 2):
		for x in range(pixel.x - 1, pixel.x + 2):
			if x >= 0 and y >= 0 and x < image.get_width() and y < image.get_height():
				var color := image.get_pixel(x, y)
				if maxf(color.r, maxf(color.g, color.b)) < 0.4:
					return true
	return false


func _coverage(image: Image, x: float, z: float, model_scale: float) -> float:
	var count := 0
	for sample in 200:
		if _dark(image, Vector3(x, lerpf(-2.3, 2.3, (sample + 0.5) / 200.0), z), model_scale):
			count += 1
	return float(count) / 200.0


func _edges(projection: int, model_scale: float, direction: Vector3) -> void:
	_clear_world()
	_setup_camera(projection, model_scale, direction)
	var material := _white()
	_panel(material, -4, 4, -3, 3, 0)
	# Separation is 0.25% of the panel width; both surfaces use the SAME CAD material.
	_panel(material, -0.7, 0.7, -0.7, 0.7, SEPARATION)
	var lines := _lines()
	var baseline := await _image()
	material.depth_bias_constant_factor = CONSTANT
	material.depth_bias_slope_factor = SLOPE
	material.depth_bias_enabled = true
	var biased := await _image()
	var left := _coverage(biased, -2, 0, model_scale)
	var right := _coverage(biased, 2, 0, model_scale)
	var behind := _coverage(biased, 1, -SEPARATION, model_scale)
	var occluded := 0
	for sample in 11:
		if _dark(biased, Vector3(0, lerpf(-0.4, 0.4, sample / 10.0), 0), model_scale):
			occluded += 1
	var label := "projection=%d scale=%s direction=%s" % [projection, model_scale, direction]
	print("DEPTH_BIAS %s visible=%s/%s behind=%s occluded=%d" % [label, left, right, behind, occluded])
	_test.check(left >= 0.98 and right >= 0.98, "coplanar edges remain continuous: " + label)
	_test.check(behind == 0.0 and occluded == 0, "back edges and thin occluders retain depth testing: " + label)
	material.depth_bias_enabled = false
	_test.check((await _image()).get_data() == baseline.get_data(), "disabling depth bias restores the original frame: " + label)
	# Rasterization state must not change independent line primitives, even with large factors.
	lines.material_override.depth_bias_enabled = true
	lines.material_override.depth_bias_constant_factor = 16.0
	lines.material_override.depth_bias_slope_factor = 16.0
	_test.check((await _image()).get_data() == baseline.get_data(), "line primitives ignore material depth bias: " + label)


func _material_isolation() -> void:
	_clear_world()
	_setup_camera(Camera3D.PROJECTION_PERSPECTIVE, 1.0)
	var first := _white()
	first.depth_bias_enabled = true
	first.depth_bias_constant_factor = CONSTANT
	first.depth_bias_slope_factor = SLOPE
	var second: StandardMaterial3D = first.duplicate()
	second.depth_bias_constant_factor = -4
	second.depth_bias_slope_factor = -2
	_panel(first, -4, 0, -3, 3, 0)
	_panel(second, 0, 4, -3, 3, 0)
	_lines()
	var image := await _image()
	_test.check(_coverage(image, -2, 0, 1) >= 0.98 and _coverage(image, 2, 0, 1) < 0.1, "materials sharing a shader use independent positive/negative bias pipelines")
	RenderingServer.material_set_depth_bias(second.get_rid(), true, CONSTANT, SLOPE)
	image = await _image()
	_test.check(_coverage(image, 2, 0, 1) >= 0.98, "RenderingServer updates live material bias")
	second.depth_bias_slope_factor = -3
	image = await _image()
	_test.check(_coverage(image, 2, 0, 1) < 0.1 and _coverage(image, -2, 0, 1) >= 0.98, "changing one material never leaks rasterization state into another")
	var shader := Shader.new()
	shader.code = "shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = vec3(1.0); }"
	var custom := ShaderMaterial.new()
	custom.shader = shader
	custom.depth_bias_enabled = true
	custom.depth_bias_constant_factor = CONSTANT
	custom.depth_bias_slope_factor = SLOPE
	for child in _world.get_children():
		if child is MeshInstance3D and child.mesh.surface_get_primitive_type(0) == Mesh.PRIMITIVE_TRIANGLES:
			child.material_override = custom
	image = await _image()
	_test.check(_coverage(image, -2, 0, 1) >= 0.98 and _coverage(image, 2, 0, 1) >= 0.98, "spatial ShaderMaterial applies the same hardware bias")
	# Changing shader mode forces material data recreation in both storages.
	shader.code = "shader_type canvas_item; void fragment() { COLOR = vec4(1.0); }"
	shader.code = "shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = vec3(1.0); }"
	image = await _image()
	_test.check(_coverage(image, -2, 0, 1) >= 0.98, "shader data recreation preserves material hardware bias")


func _shadows() -> void:
	_clear_world()
	_world.scale = Vector3.ONE
	_camera.free()
	_camera = Camera3D.new()
	_viewport.add_child(_camera)
	_camera.projection = Camera3D.PROJECTION_PERSPECTIVE
	_camera.near = 0.05
	_camera.far = 100.0
	_camera.position = Vector3(0, -6, 10)
	_camera.look_at(Vector3.ZERO)
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.6, 0.6, 0.6)
	material.roughness = 1.0
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	_panel(material, -8, 8, -8, 8, 0)
	var caster_material: StandardMaterial3D = material.duplicate()
	caster_material.albedo_color = Color(0.2, 0.4, 0.8)
	var caster := MeshInstance3D.new()
	caster.mesh = BoxMesh.new()
	caster.position.z = 1.0
	caster.material_override = caster_material
	_world.add_child(caster)
	var light := DirectionalLight3D.new()
	light.shadow_enabled = true
	light.directional_shadow_max_distance = 40
	_world.add_child(light)
	light.position = Vector3(6, 6, 8)
	light.look_at(Vector3.ZERO)
	var shadowed := await _image()
	caster_material.depth_bias_constant_factor = 16
	caster_material.depth_bias_slope_factor = 16
	caster_material.depth_bias_enabled = true
	var biased := await _image()
	light.shadow_enabled = false
	var unshadowed := await _image()
	var shadow_samples := 0
	var changed := 0
	# Floor samples far from the caster's silhouette, including its cast shadow.
	for y in 24:
		for x in 24:
			var point := Vector3(-3.0 + x * 0.1, -3.0 + y * 0.1, 0)
			var pixel := Vector2i(_camera.unproject_position(point).round())
			var before := shadowed.get_pixelv(pixel)
			if before.r + 0.05 < unshadowed.get_pixelv(pixel).r:
				shadow_samples += 1
			if absf(before.r - biased.get_pixelv(pixel).r) > 0.01:
				changed += 1
	print("DEPTH_BIAS_SHADOW samples=%d changed=%d" % [shadow_samples, changed])
	_test.check(shadow_samples > 10, "shadow regression samples a visible cast shadow")
	_test.check(changed == 0, "camera material bias does not change shadow-map rendering")
