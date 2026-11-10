extends RefCounted
## CPU import results become shared meshes on the main thread.

static func create(result: Dictionary, options: Dictionary) -> Node3D:
	var root := Node3D.new()
	root.name = "BREP"
	root.set_meta("brep_stats", result.stats)
	root.set_meta("brep_warnings", result.warnings)
	var scale_factor: float = options.get("unit_scale", 1.0)
	root.scale = Vector3.ONE * scale_factor
	if options.get("z_up", false): root.rotation.x = -PI / 2.0
	var meshes: Array[ArrayMesh] = []
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("b4c3d4")
	material.roughness = 0.65
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	# Bias filled faces only when the import includes edges. Lines keep their original depth.
	material.depth_bias_constant_factor = 0.5
	material.depth_bias_slope_factor = 0.5
	material.depth_bias_enabled = result.bodies.any(func(body: Dictionary) -> bool: return body.get("lines", false) and not body.indices.is_empty())
	var line_material := StandardMaterial3D.new()
	line_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	line_material.albedo_color = Color("455568")
	for body: Dictionary in result.bodies:
		var mesh := ArrayMesh.new()
		if not body.indices.is_empty():
			var arrays := []
			arrays.resize(Mesh.ARRAY_MAX)
			arrays[Mesh.ARRAY_VERTEX] = body.vertices
			if not body.get("lines", false): arrays[Mesh.ARRAY_NORMAL] = body.normals
			arrays[Mesh.ARRAY_INDEX] = body.indices
			mesh.add_surface_from_arrays(Mesh.PRIMITIVE_LINES if body.get("lines", false) else Mesh.PRIMITIVE_TRIANGLES, arrays)
			mesh.surface_set_material(0, line_material if body.get("lines", false) else material)
		mesh.set_meta("brep_lines", body.get("lines", false))
		mesh.set_meta("brep_face_ranges", body.face_ranges)
		if body.has("edge_ranges"): mesh.set_meta("brep_edge_ranges", body.edge_ranges)
		mesh.set_meta("brep_document", result.document)
		meshes.append(mesh)
	var nodes: Array[Node3D] = []
	for index in result.nodes.size():
		var descriptor: Dictionary = result.nodes[index]
		var node := Node3D.new()
		node.name = str(descriptor.name).validate_node_name() if not str(descriptor.name).is_empty() else "Object_%d" % index
		node.set_meta("brep_persistent_id", descriptor.persistent_id)
		node.set_meta("brep_occurrence", index)
		node.set_meta("brep_object_type", descriptor.get("object_type", 2))
		if descriptor.has("matrix"):
			var m: PackedFloat64Array = descriptor.matrix
			node.transform = Transform3D(Basis(Vector3(m[0], m[1], m[2]), Vector3(m[4], m[5], m[6]), Vector3(m[8], m[9], m[10])), Vector3(m[12], m[13], m[14]))
		var parent: Node3D = root if descriptor.parent < 0 else nodes[descriptor.parent]
		parent.add_child(node)
		for body_index: int in descriptor.bodies:
			if body_index < 0: continue
			var body: Dictionary = result.bodies[body_index]
			var instance := MeshInstance3D.new()
			instance.name = "Body_%d_%d" % [body.body_type, body.body_index]
			if body.get("lines", false): instance.name = "Edges_%d_%d" % [body.body_type, body.body_index]
			if body.has("tessellation_id"):
				instance.name = "Edges_%d" % body.tessellation_id if body.get("lines", false) else instance.name + "_Tess_%d" % body.tessellation_id
			instance.mesh = meshes[body_index]
			instance.set_meta("brep_source_body_type", descriptor.get("source_body_type", body.body_type))
			instance.set_meta("brep_source_body_index", descriptor.get("source_body_index", body.body_index))
			instance.set_meta("brep_body_type", body.body_type)
			instance.set_meta("brep_body_index", body.body_index)
			instance.set_meta("brep_persistent_id", descriptor.persistent_id)
			instance.set_meta("brep_occurrence", index)
			node.add_child(instance)
		nodes.append(node)
	return root
