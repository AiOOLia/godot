extends RefCounted

static func satellite() -> String:
	return """extends Node
# Explicit viewport ID; switching active panes never retargets a running script.
var satellite: MeshInstance3D
var angle := 0.0
func run(viewer) -> void:
	var ids = viewer.viewport_ids()
	if ids.is_empty(): return
	var view = viewer.viewport(ids[0])
	var content = viewer.content(ids[0])
	satellite = MeshInstance3D.new()
	satellite.mesh = TorusMesh.new()
	var material = StandardMaterial3D.new()
	material.albedo_color = Color("#2cb79a")
	material.metallic = 0.45
	satellite.material_override = material
	content.add_child(satellite)
	view.frame_bounds(AABB(Vector3(-2, -2, -2), Vector3(4, 4, 4)))
	print("Created a native animated mesh in ", ids[0])
func _process(delta: float) -> void:
	if is_instance_valid(satellite):
		satellite.rotate_y(delta * 0.6)
		satellite.rotate_x(delta * 0.2)
"""

static func material() -> String:
	return """extends Node
func run(viewer) -> void:
	var view = viewer.viewport("view_1")
	if view == null: return
	view.environment.background_color = Color("#182633")
	print("Changed view_1 environment")
"""
