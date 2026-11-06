extends Node3D
## View-local overlays never become part of model bounds, picking or shared meshes.
var entries: Array[Dictionary] = []
static var _material: ShaderMaterial

static func _highlight_material() -> ShaderMaterial:
	if _material == null:
		var shader := Shader.new()
		shader.code = """shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_never, fog_disabled;
void vertex() {
	POSITION = PROJECTION_MATRIX * MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
	// Keep the overlay above its own surface, while retaining scene depth tests.
	POSITION.z += (POSITION.z - CLIP_SPACE_FAR * POSITION.w) * 0.00001;
}
void fragment() {
	ALBEDO = vec3(1.0, 0.58, 0.08);
	ALPHA = 0.65;
}
"""
		_material = ShaderMaterial.new()
		_material.shader = shader
		_material.render_priority = 10
	return _material

func clear() -> void:
	for entry in entries:
		entry.overlay.free()
	entries.clear()
	set_process(false)

func select_nodes(nodes: Array[Node], face: Dictionary = {}) -> void:
	clear()
	var visited := {}
	for node in nodes:
		if is_instance_valid(node): _collect(node, face, visited)
	sync()
	set_process(not entries.is_empty())

func _collect(node: Node, face: Dictionary, visited: Dictionary) -> void:
	if node is MeshInstance3D and node.mesh:
		var key := node.get_instance_id()
		if visited.has(key): return
		visited[key] = true
		var mesh: Mesh = node.mesh if face.is_empty() else _face_mesh(node.mesh, face)
		if mesh != null:
			var overlay := MeshInstance3D.new()
			overlay.name = "SelectedSurface"
			overlay.mesh = mesh
			overlay.material_override = _highlight_material()
			overlay.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			overlay.gi_mode = GeometryInstance3D.GI_MODE_DISABLED
			add_child(overlay)
			entries.append({"source": weakref(node), "overlay": overlay})
	for child in node.get_children(): _collect(child, face, visited)

func _face_mesh(mesh: Mesh, face: Dictionary) -> ArrayMesh:
	# BREP bodies use one surface, with face ranges in triangle-index order.
	if mesh.get_surface_count() != 1 or not mesh.has_meta("brep_face_ranges"): return null
	var first: int = face.get("triangle_first", -1) * 3
	var count: int = face.get("triangle_count", 0) * 3
	var arrays := mesh.surface_get_arrays(0)
	var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
	if first < 0 or count <= 0 or first + count > indices.size(): return null
	arrays[Mesh.ARRAY_INDEX] = indices.slice(first, first + count)
	var selected := ArrayMesh.new()
	selected.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return selected

func sync() -> void:
	for index in range(entries.size() - 1, -1, -1):
		var entry: Dictionary = entries[index]
		var source: MeshInstance3D = entry.source.get_ref()
		if not is_instance_valid(source) or not source.is_inside_tree() or source.is_queued_for_deletion():
			entry.overlay.free()
			entries.remove_at(index)
			continue
		entry.overlay.global_transform = source.global_transform
		entry.overlay.visible = source.is_visible_in_tree()
		entry.overlay.transparency = source.transparency
	if entries.is_empty(): set_process(false)

func _process(_delta: float) -> void:
	sync()
