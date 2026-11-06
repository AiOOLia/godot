extends Node
const Sources = preload("res://scripts/desktop/source_resolver.gd")
const Profile = preload("res://scripts/desktop/build_profile.gd")
var checks := 0
var failures: Array[String] = []

func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		failures.append(message)
		push_error("BREP_CHECK: " + message)

func write(path: String, text: String) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_string(text)
	file.close()

func wait_layer(view: String, layer: String) -> Dictionary:
	for frame in 3000:
		var record: Dictionary = AppState.views[view].get_layer(layer)
		if record.status in ["Ready", "Failed"]: return record
		await get_tree().create_timer(0.01).timeout
	return AppState.views[view].get_layer(layer)

func run(shell: Control) -> void:
	check(ClassDB.class_exists("BRepDocument"), "native extension registered")
	if not ClassDB.class_exists("BRepDocument"):
		_finish()
		return
	var directory := OS.get_environment("DV_TEST_DIRECTORY")
	if directory.is_empty(): directory = "res://build/brep/test"
	directory = ProjectSettings.globalize_path(directory).path_join("中文 装配")
	DirAccess.make_dir_recursive_absolute(directory)
	var binary := directory.path_join("part.brep")
	var file := FileAccess.open(binary, FileAccess.WRITE)
	file.store_buffer(Marshalls.base64_to_raw(FileAccess.get_file_as_string("res://tests/fixtures/planar-face.brep.base64.txt").strip_edges()))
	file.close()
	var assembly := {"version": [1, 0], "brep": {"file": "part.brep"}, "root": {"name": "装配", "children": []}}
	for index in 2:
		assembly.root.children.append({"name": "零件%d" % index, "type": "body", "persistentId": index + 10, "bRepBodyType": "shell", "repBodyIndex": 0, "matrix": [1,0,0,0,0,1,0,0,0,0,1,0,index * 20,0,0,1]})
	var json := directory.path_join("assembly.json")
	write(json, JSON.stringify(assembly))
	check(Sources.detect(json).get("kind") == "brep", "assembly JSON identified by content")
	check(Profile.model_entry(json), "assembly is a supported model entry")
	check("*.json" in ",".join(Profile.file_filters()), "JSON enabled in both profiles")
	var tiles := {"asset": {"version": "1.1"}, "root": {"boundingVolume": {"box": []}}}
	check(Sources.json_text(JSON.stringify(tiles)).get("kind") == "tiled", "tiles identified independently of filename")
	check(Sources.json_text('{"root":{}}').has("error"), "unknown JSON rejected")
	check(Sources.json_text('{broken').has("error"), "invalid JSON rejected")
	var conflict: Dictionary = assembly.duplicate(true)
	conflict["asset"] = {"version": "1.1"}
	check(Sources.json_text(JSON.stringify(conflict)).has("error"), "ambiguous JSON rejected")
	check(Sources.dependency(json, "../escape.brep").has("error"), "dependency traversal rejected")
	check(Sources.dependency(json, "absent.brep").has("error"), "missing dependency diagnosed")
	check(not Profile.source_error("anything.json", "tiled", {}).is_empty() == not Profile.enabled("3dtiles"), "tiles profile checked after detection")
	var view: String = AppState.active_id
	var raw_id: String = AppState.add_source(view, binary)
	var raw := await wait_layer(view, raw_id)
	check(raw.status == "Ready", "local BREP ready: " + raw.error)
	if raw.status == "Ready":
		check(AppState.store.records[raw.data_id].brep_stats.get("unit_to_millimeters", -1.0) == 0.0, "legacy BREP keeps unknown units")
		_check_source_tree(shell.panes[view], raw)
	var layer_id: String = AppState.add_source(view, json, "auto", {"z_up": false})
	var layer := await wait_layer(view, layer_id)
	check(layer.status == "Ready", "assembly JSON ready: " + layer.error)
	if layer.status == "Ready":
		var record: Dictionary = AppState.store.records[layer.data_id]
		check(record.brep_stats.unique_bodies == 1, "repeated bodies tessellated once")
		check(record.brep_stats.triangles > 0 and record.brep_stats.missing_faces == 0, "mesh produced without missing faces")
		check(layer.node.get_child(0).get_child_count() == 2, "assembly hierarchy preserved")
		var a: MeshInstance3D = layer.node.get_child(0).get_child(0).get_child(0)
		var b: MeshInstance3D = layer.node.get_child(0).get_child(1).get_child(0)
		check(a.mesh == b.mesh, "instances share mesh resources")
		check(not a.mesh.surface_get_material(0).depth_bias_enabled, "BREP without edge geometry keeps unbiased surface depth")
		check(is_equal_approx(b.get_parent().position.x, 20), "column-major assembly translation applied")
		check(a.get_meta("brep_persistent_id") == 10, "persistent ID retained")
		check(not a.mesh.get_meta("brep_face_ranges").is_empty(), "face ranges retained")
		var pane: Control = shell.panes[view]
		_check_source_tree(pane, layer)
		await _root_controls(pane, layer, shell)
		pane.refresh()
		var node_key := "node:%s:%d" % [layer.id, a.get_instance_id()]
		check(pane._items.has(node_key), "assembly body appears in the viewport tree")
		pane._activate_tree_item(pane._items[node_key])
		check(AppState.views[view].selection_kind == "node" and AppState.views[view].selected_node.get_ref() == a, "assembly tree selects the actual body")
		var arrays := a.mesh.surface_get_arrays(0)
		var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
		var area := 0.0
		for i in range(0, indices.size(), 3):
			area += (vertices[indices[i + 1]] - vertices[indices[i]]).cross(vertices[indices[i + 2]] - vertices[indices[i]]).length() / 2.0
		check(is_equal_approx(area, 84.0), "tessellation preserves the square hole (area 84)")
		var hits := []
		AppState.views[view]._mesh_pick(layer.node, Vector3(1, 1, 10), Vector3(0, 0, -1), hits)
		check(not hits.is_empty() and hits[0].get("brep", {}).get("persistent_id") == 42, "ray hit maps to source face ID")
		if not hits.is_empty():
			hits[0]["layer"] = layer.id
			AppState.select_hit(view, hits[0])
			await _definition_properties(shell, a, hits[0].brep)
		var hole_hits := []
		AppState.views[view]._mesh_pick(layer.node, Vector3(5, 5, 10), Vector3(0, 0, -1), hole_hits)
		check(hole_hits.is_empty(), "ray passes through hole")
		b.get_parent().scale = Vector3(-2, 1, 1)
		var mirror_hits := []
		AppState.views[view]._mesh_pick(layer.node, Vector3(18, 1, 10), Vector3(0, 0, -1), mirror_hits)
		check(not mirror_hits.is_empty() and mirror_hits[0].get("object_persistent_id") == 11, "mirrored nonuniform instance picking")
		b.get_parent().scale = Vector3.ONE
		var other: String = shell.split_view(view, false)
		check(AppState.share_layer(view, layer_id, other), "share BREP across viewports")
		var shared: Dictionary = AppState.views[other].layers.back()
		check(is_instance_valid(shared.node) and shared.node != layer.node, "viewport instances are independent")
		var shared_mesh: MeshInstance3D = shared.node.get_child(0).get_child(0).get_child(0)
		check(a.mesh == shared_mesh.mesh, "viewport instances share meshes")
		AppState.set_visible(other, shared.id, false)
		check(layer.node.visible, "visibility independent between viewports")
		check(AppState.store.layer_bounds(layer).size.length() > 20, "bounds include assembly transforms")
		await _selection(shell, view, layer, a, b, other)
		AppState.store.reload_data(layer.data_id)
		layer = await wait_layer(view, layer_id)
		check(layer.status == "Ready", "BREP reload succeeds")
		if layer.status == "Ready": _check_source_tree(pane, layer)
		check(AppState.views[view]._selection_highlight.entries.is_empty(), "reload removes overlays referencing old instances")
	var forced: String = AppState.add_source(view, json, "tiled")
	check((await wait_layer(view, forced)).status == "Failed", "explicit kind cannot bypass JSON detection")
	var bad := directory.path_join("bad.brep")
	write(bad, "DBRep_DrawableShape")
	var bad_id: String = AppState.add_source(view, bad)
	check("dialect" in (await wait_layer(view, bad_id)).error, "OCCT text BREP gets a clear error")
	var unknown := directory.path_join("unknown.json")
	write(unknown, "{}")
	var unknown_id: String = AppState.add_source(view, unknown)
	check((await wait_layer(view, unknown_id)).status == "Failed", "unknown JSON does not fall back to tiles")
	var archive := directory.path_join("assembly.zip")
	var zip := ZIPPacker.new()
	zip.open(archive)
	for path in [binary, json]:
		zip.start_file("model/" + path.get_file())
		zip.write_file(FileAccess.get_file_as_bytes(path))
		zip.close_file()
	zip.close()
	var reader := ZIPReader.new()
	reader.open(archive)
	check(Sources.zip_candidates(reader, Profile.features()) == ["model/assembly.json"], "ZIP suppresses referenced BREP duplicate")
	reader.close()
	var zip_id: String = AppState.add_source(view, archive)
	var zip_layer := await wait_layer(view, zip_id)
	check(zip_layer.status == "Ready", "ZIP assembly loads relative BREP dependency")
	if zip_layer.status == "Ready": _check_source_tree(shell.panes[view], zip_layer)
	var remove_id: String = AppState.add_source(view, json)
	await get_tree().process_frame
	AppState.remove_layer(view, remove_id)
	for frame in 30: await get_tree().process_frame
	check(not AppState.views[view].get_layer(remove_id), "in-flight removal does not publish stale result")
	await _updated_sdk(directory, view)
	await _definition_fixtures(directory, view, shell)
	await _json_imports(directory, view, shell, AppState.views[view].get_layer(layer_id))
	await _tessellation_imports(directory, view, shell)
	await preload("res://tests/material_depth_bias.gd").new().run(self)
	await _up_axis(shell, view, binary, json, archive)
	var dataset := OS.get_environment("DV_TEST_DATASET")
	if not dataset.is_empty():
		shell._open_add(view)
		shell._source.text = dataset
		shell._zip_entry.clear()
		shell._confirm_add()
		var real_id: String = AppState.views[view].layers.back().id
		var real := await wait_layer(view, real_id)
		check(real.status == "Ready", "external BREP dataset: " + real.error)
		if real.status == "Ready": check(real.node.basis.is_equal_approx(Basis.IDENTITY), "real BREP imported through dialog defaults to Y-Up")
		if real.status == "Ready": print("BREP_DATASET_STATS ", AppState.store.records[real.data_id].brep_stats)
		if real.status == "Ready": _check_source_tree(shell.panes[view], real)
		AppState.store.frame_layer(view, real)
		if real.status == "Ready":
			await get_tree().process_frame
			var real_view: Node = AppState.views[view]
			check(is_equal_approx(real_view.camera.near, 0.2), "real BREP camera keeps the fixed near plane")
			var hit := _dataset_hit(real_view, real.id)
			check(not hit.is_empty() and hit.has("brep"), "real assembly has a selectable visible face")
			if not hit.is_empty():
				shell.panes[view].refresh()
				AppState.select_hit(view, hit)
				check(real_view._selection_highlight.entries.size() == 1 and real_view.selected_face == hit.brep, "real assembly face selects one occurrence")
				await _wait_definition(shell)
				for prefix in ["SourceBRep", "ReconstructedBRep"]:
					var editor: TextEdit = shell._property_fields.find_child(prefix + "Text", true, false)
					var full: String = shell._brep_inspector.result.source_json if prefix == "SourceBRep" else shell._brep_inspector.result.reconstructed_json
					check(editor != null and editor.text == full.left(65536) and JSON.parse_string(full) is Dictionary, "real assembly selected face displays " + prefix + " SDK JSON with bounded preview")
					if DisplayServer.get_name() != "headless":
						check(await _copy_json(shell._property_fields.find_child("Copy" + prefix, true, false), full), "real assembly copies complete " + prefix + " JSON beyond preview limit")
				var definitions := _face_definitions(real.node)
				var ambiguous := 0
				for face: Dictionary in definitions:
					var definition: Dictionary = face.get("definition", {})
					if not definition.has("source_face_index"): ambiguous += 1
				check(ambiguous == 0, "real assembly faces all map to unique source definitions")
	if "--dv-capture" in OS.get_cmdline_user_args() and DisplayServer.get_name() != "headless":
		await get_tree().create_timer(1).timeout
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(directory.path_join("brep-%s.png" % Profile.id()))
		var occ_editor: TextEdit = shell._property_fields.find_child("ReconstructedBRepText", true, false)
		if occ_editor != null:
			shell._property_scroll.ensure_control_visible(occ_editor)
			await RenderingServer.frame_post_draw
			get_viewport().get_texture().get_image().save_png(directory.path_join("brep-%s-occ.png" % Profile.id()))
		var selected: Node = AppState.views[view].selected_node.get_ref() if AppState.views[view].selected_node else null
		if is_instance_valid(selected):
			AppState.activate(view, "node", AppState.views[view].selected, selected.get_parent())
			await get_tree().create_timer(0.2).timeout
			await RenderingServer.frame_post_draw
			get_viewport().get_texture().get_image().save_png(directory.path_join("brep-%s-tree.png" % Profile.id()))
		shell._open_add(view)
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(directory.path_join("brep-%s-import.png" % Profile.id()))
		shell._close_add()
	_finish()

func _up_axis(shell: Control, view_id: String, binary: String, json: String, archive: String) -> void:
	# Exercise the real dialog paths that previously injected OSG's hidden default.
	for request in [[json, 0, false], [binary, 0, true], [json, 1, true], [archive, 1, false], [binary, 0, false]]:
		shell._open_add(view_id)
		check(shell._axis.is_visible_in_tree() and shell._axis.item_count == 2 and shell._axis.selected == 0, "import dialog exposes two up axes and defaults to Y-UP")
		shell._axis.select(request[1])
		shell._source.text = request[0]
		shell._zip_entry.text = "model/assembly.json" if request[0] == archive else ""
		if request[2]: shell._add_pending_paths(PackedStringArray([request[0]]))
		shell._confirm_add()
		var id: String = AppState.views[view_id].layers.back().id
		var layer := await wait_layer(view_id, id)
		check(layer.status == "Ready", "dialog imports with selected up axis: " + layer.error)
		if layer.status == "Ready":
			var expected := Basis(Vector3.RIGHT, -PI / 2.0) if request[1] == 1 else Basis.IDENTITY
			check(layer.node.basis.is_equal_approx(expected), "BREP default/Y-Up preserves coordinates; explicit Z-Up rotates once")
		AppState.remove_layer(view_id, id)

func _selection(shell: Control, view_id: String, layer: Dictionary, a: MeshInstance3D, b: MeshInstance3D, other: String) -> void:
	var view: Node = AppState.views[view_id]
	var pane: Control = shell.panes[view_id]
	var other_selection: Array = AppState.views[other]._selection_highlight.entries.duplicate()
	# Two independently selectable faces in one body, so whole-body tint cannot pass.
	var sample := MeshInstance3D.new()
	sample.name = "TwoFaces"
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(40,0,0), Vector3(42,0,0), Vector3(40,2,0), Vector3(43,0,0), Vector3(45,0,0), Vector3(43,2,0)])
	arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0,2,1,3,5,4])
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	mesh.set_meta("brep_face_ranges", [{"face_index": 0, "persistent_id": 501, "triangle_first": 0, "triangle_count": 1}, {"face_index": 1, "persistent_id": 502, "triangle_first": 1, "triangle_count": 1}])
	sample.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.4, 0.4, 0.4)
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	sample.material_override = material
	layer.node.add_child(sample)
	var saved_pose: Dictionary = view.pose()
	view.target = Vector3(42, 1, 0)
	view.angles = Vector2.ZERO
	view.distance = 15
	view.update_camera()
	pane.refresh()
	var key := "node:%s:%d" % [layer.id, sample.get_instance_id()]
	pane._items[key].get_parent().collapsed = true
	for index in 2:
		var point: Vector2 = view.camera.unproject_position(Vector3(40.5 + index * 3, 0.5, 0)) * pane.display.size / Vector2(view.viewport.size)
		for pressed in [true, false]:
			var click := InputEventMouseButton.new()
			click.button_index = MOUSE_BUTTON_LEFT
			click.position = point
			click.pressed = pressed
			pane._input_view(click)
		check(view.selected_face.get("persistent_id") == 501 + index, "single click selects the correct BREP face")
		check(view._selection_highlight.entries.size() == 1, "face selection replaces the previous overlay")
		if view._selection_highlight.entries.size() == 1:
			var overlay: MeshInstance3D = view._selection_highlight.entries[0].overlay
			var selected_indices: PackedInt32Array = overlay.mesh.surface_get_arrays(0)[Mesh.ARRAY_INDEX]
			check(selected_indices == PackedInt32Array([index * 3, index * 3 + 2, index * 3 + 1]), "face overlay contains only the hit face triangles")
		check(view.selected_node.get_ref() == sample and pane.tree.get_selected() == pane._items[key] and not pane._items[key].get_parent().collapsed, "face click reveals the corresponding tree body")
	check(mesh.surface_get_arrays(0)[Mesh.ARRAY_INDEX].size() == 6 and sample.material_overlay == null, "selection leaves original geometry and materials unchanged")
	if DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		var image: Image = view.viewport.get_texture().get_image()
		var selected_pixel: Color = image.get_pixelv(Vector2i(view.camera.unproject_position(Vector3(43.5, 0.5, 0))))
		var other_pixel: Color = image.get_pixelv(Vector2i(view.camera.unproject_position(Vector3(40.5, 0.5, 0))))
		check(selected_pixel.r > selected_pixel.b * 1.4 and other_pixel.r < other_pixel.b * 1.2, "rendered highlight colors only the selected face")
		var occluder := MeshInstance3D.new()
		occluder.mesh = mesh
		occluder.material_override = material
		layer.node.add_child(occluder)
		occluder.position.z = 0.1
		await RenderingServer.frame_post_draw
		image = view.viewport.get_texture().get_image()
		var covered: Color = image.get_pixelv(Vector2i(view.camera.unproject_position(Vector3(43.5, 0.5, 0))))
		check(covered.r < covered.b * 1.2, "selection remains occluded by foreground geometry")
		occluder.free()
	check(AppState.views[other]._selection_highlight.entries == other_selection, "face highlight leaves the other viewport selection unchanged")
	var original_bounds: AABB = AppState.store.layer_bounds(layer)
	pane._activate_tree_item(pane._items[key])
	check(view.selected_face.is_empty() and view._selection_highlight.entries[0].overlay.mesh == mesh, "tree body selection highlights the whole body")
	AppState.set_visible(view_id, layer.id, false)
	view._selection_highlight.sync()
	check(not view._selection_highlight.entries[0].overlay.visible, "layer visibility hides selection overlays")
	AppState.set_visible(view_id, layer.id, true)
	AppState.set_opacity(view_id, layer.id, 0.4)
	view._selection_highlight.sync()
	check(is_equal_approx(view._selection_highlight.entries[0].overlay.transparency, 0.6), "highlight follows layer opacity")
	AppState.set_opacity(view_id, layer.id, 1.0)
	sample.scale = Vector3(-2, 1, 1)
	await get_tree().process_frame
	view._selection_highlight.sync()
	check(view._selection_highlight.entries[0].overlay.global_transform.is_equal_approx(sample.global_transform), "highlight follows mirrored nonuniform instance transforms")
	sample.hide()
	view._selection_highlight.sync()
	check(not view._selection_highlight.entries[0].overlay.visible, "hidden body hides its highlight")
	sample.show()
	sample.scale = Vector3.ONE
	var assembly_key := "node:%s:%d" % [layer.id, layer.node.get_child(0).get_instance_id()]
	pane._activate_tree_item(pane._items[assembly_key])
	check(view._selection_highlight.entries.size() == 2, "tree assembly selects all descendant bodies")
	var a_key := "node:%s:%d" % [layer.id, a.get_instance_id()]
	var b_key := "node:%s:%d" % [layer.id, b.get_instance_id()]
	pane._selected_keys.clear()
	pane._multi_selected(pane._items[a_key], 0, true)
	pane._multi_selected(pane._items[b_key], 0, true)
	check(view._selection_highlight.entries.size() == 2, "tree multi-selection highlights both occurrences")
	pane._multi_selected(pane._items[a_key], 0, false)
	check(view._selection_highlight.entries.size() == 1 and view._selection_highlight.entries[0].source.get_ref() == b, "tree deselection removes only that occurrence highlight")
	pane.refresh()
	check(pane._selected_keys == [b_key] and view._selection_highlight.entries.size() == 1, "tree refresh retains selection and highlight")
	if DisplayServer.get_name() != "headless":
		await _tree_click(pane, a_key, true)
		check(view._selection_highlight.entries.size() == 2, "native Ctrl-click adds a tree occurrence to highlight")
		await _tree_click(pane, a_key, true)
		check(view._selection_highlight.entries.size() == 1 and view._selection_highlight.entries[0].source.get_ref() == b, "native Ctrl-click deselects only that occurrence")
		check(view.selected_node.get_ref() == b, "deselection properties follow the remaining tree selection")
	check(a.mesh == b.mesh and a.material_overlay == null and b.material_overlay == null, "shared occurrences keep their original mesh and material")
	check(AppState.store.layer_bounds(layer).is_equal_approx(original_bounds), "selection overlays do not alter model bounds")
	AppState.select_hit(view_id, {})
	check(view._selection_highlight.entries.is_empty() and view.selection_kind == "viewport", "empty click clears highlight and selection")
	pane._activate_tree_item(pane._items[key])
	sample.queue_free()
	await get_tree().process_frame
	view._selection_highlight.sync()
	check(view._selection_highlight.entries.is_empty(), "deleted source releases its highlight")
	view.restore_pose(saved_pose)
	AppState.activate(view_id, "node", layer.id, a)

func _dataset_hit(view: Node, layer: String) -> Dictionary:
	for y in [0.5, 0.4, 0.6, 0.3, 0.7]:
		for x in [0.5, 0.4, 0.6, 0.3, 0.7]:
			var hit: Dictionary = view.pick(Vector2(view.viewport.size) * Vector2(x, y))
			if hit.get("layer") == layer and hit.has("brep"): return hit
	return {}

func _check_source_tree(pane: Control, layer: Dictionary) -> void:
	pane.refresh()
	var roots: Array[Node] = layer.node.get_children()
	check(not pane._items.has("layer:" + layer.id) and not pane._items.has("node:%s:%d" % [layer.id, layer.node.get_instance_id()]), "loaded BREP tree omits filename and runtime layer containers")
	var group: TreeItem = pane._items["group:3D data"]
	var rows := group.get_children().filter(func(item: TreeItem): return item.get_metadata(0) == layer.id)
	check(rows.size() == roots.size(), "each source root appears directly under 3D data")
	for source_root in roots:
		var key := "node:%s:%d" % [layer.id, source_root.get_instance_id()]
		check(pane._items.has(key), "source root identity is retained in the tree")
		if not pane._items.has(key): continue
		var item: TreeItem = pane._items[key]
		check(item.get_parent() == group and item.get_text(0) == str(source_root.name) and item.get_meta("node").get_ref() == source_root, "tree root uses the actual source name and node")
		check(item.is_editable(1) and item.is_checked(1) == layer.visible and item.get_metadata(0) == layer.id, "source root retains dataset visibility and identity")
		check(item.get_child_count() == source_root.get_child_count(), "source root keeps its original children without flattening assembly levels")

func _root_controls(pane: Control, layer: Dictionary, shell: Control) -> void:
	var view: Node = AppState.views[pane.view_id]
	var key := "node:%s:%d" % [layer.id, layer.node.get_child(0).get_instance_id()]
	AppState.activate(pane.view_id, "layer", layer.id)
	check(pane._selected_keys == pane._layer_root_keys[layer.id], "dataset selection maps to its visible source roots")
	pane._activate_tree_item(pane._items[key])
	check(view.selection_kind == "node" and view.selected_node.get_ref() == layer.node.get_child(0) and view._selection_highlight.entries.size() == 2, "source root selects and highlights the actual assembly")
	var buttons: Array[Node] = shell._property_fields.find_children("*", "Button", true, false)
	check(shell._property_title.text == str(layer.node.get_child(0).name) and buttons.any(func(button: Button): return button.text == tr("Reload")) and buttons.any(func(button: Button): return button.text == tr("Remove")), "actual source root properties retain dataset reload and remove actions")
	pane._items[key].collapsed = true
	pane.refresh()
	check(pane._items[key].collapsed and pane._selected_keys == [key], "source root selection and collapse survive tree refresh")
	if DisplayServer.get_name() != "headless":
		await _tree_click(pane, key, false, true)
		check(not layer.visible and not layer.node.visible and not pane._items[key].is_checked(1), "source root checkbox hides the dataset")
		check(pane._items[key].collapsed and pane._selected_keys == [key], "source root visibility preserves collapse and selection")
		await _tree_click(pane, key, false, true)
		check(layer.visible and layer.node.visible and pane._items[key].is_checked(1), "source root checkbox restores the dataset")
		await _tree_click(pane, "group:3D data", false, true)
		check(not layer.visible and not layer.node.visible and not pane._items[key].is_checked(1), "3D data group hides source roots without a filename row")
		await _tree_click(pane, "group:3D data", false, true)
		check(layer.visible and layer.node.visible and pane._items[key].is_checked(1), "3D data group restores source roots")
	pane._items[key].collapsed = false

func _tree_click(pane: Control, key: String, ctrl: bool, checkbox := false) -> void:
	var item: TreeItem = pane._items[key]
	var ancestor := item.get_parent()
	while ancestor != null:
		ancestor.collapsed = false
		ancestor = ancestor.get_parent()
	# Area rectangles are updated when the tree draws after folding or rebuilding.
	await RenderingServer.frame_post_draw
	item = pane._items[key]
	pane.tree.scroll_to_item(item)
	await RenderingServer.frame_post_draw
	item = pane._items[key]
	var rows: Array[int] = []
	var x: float = pane.tree.size.x * 0.65
	if checkbox: x = pane.tree.get_item_area_rect(item, 1).position.x + 10
	for y in range(1, int(pane.tree.size.y) - 1):
		if pane.tree.get_item_at_position(Vector2(x, y)) == item: rows.append(y)
	check(not rows.is_empty(), "native tree click target is visible")
	if rows.is_empty(): return
	var point := Vector2(x, rows[rows.size() / 2])
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		event.ctrl_pressed = ctrl
		event.position = pane.tree.get_global_transform_with_canvas() * point * pane.tree.get_window().content_scale_factor
		pane.tree.get_window().push_input(event)
	await get_tree().process_frame
	await get_tree().process_frame

func _updated_sdk(directory: String, view: String) -> void:
	for mode in ["units", "split-shell", "split-solid"]:
		var path := directory.path_join(mode + ".brep")
		var file := FileAccess.open(path, FileAccess.WRITE)
		file.store_buffer(Marshalls.base64_to_raw(FileAccess.get_file_as_string("res://tests/fixtures/%s.brep.base64.txt" % mode).strip_edges()))
		file.close()
		if mode == "units":
			var layer_id: String = AppState.add_source(view, path)
			var layer := await wait_layer(view, layer_id)
			check(layer.status == "Ready", "format 1.3 with units loads: " + layer.error)
			if layer.status == "Ready":
				var stats: Dictionary = AppState.store.records[layer.data_id].brep_stats
				check(stats.format_version == "1.3" and is_equal_approx(stats.unit_to_millimeters, 25.4), "inch metadata read through updated BRepCore DLL")
				var mesh: ArrayMesh = layer.node.get_child(0).get_child(0).mesh
				var pair := _json_pair(mesh, mesh.get_meta("brep_face_ranges")[0])
				check(is_equal_approx(pair.source.unitToMillimeters, 25.4) and is_equal_approx(pair.reconstructed.unitToMillimeters, 25.4), "both SDK JSON exports retain the source unit")
				var vertices: PackedVector3Array = mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
				var bounds := AABB(vertices[0], Vector3.ZERO)
				for vertex in vertices: bounds = bounds.expand(vertex)
				# Renderer AABBs add thickness to planar meshes; compare the actual vertices.
				check(bounds.size.is_equal_approx(Vector3(10, 10, 0)) and layer.node.transform.is_equal_approx(Transform3D.IDENTITY), "unit metadata does not rescale source geometry")
			var scaled_id: String = AppState.add_source(view, path, "auto", {"unit_scale": 0.01})
			var scaled := await wait_layer(view, scaled_id)
			check(scaled.status == "Ready" and AppState.store.layer_bounds(scaled).size.is_equal_approx(Vector3(0.1, 0.1, 0)), "explicit display scale applied once with known units")
			continue
		var assembly := {"brep": {"file": mode + ".brep"}, "root": {"name": "修复结果", "children": []}}
		for index in 2:
			assembly.root.children.append({"name": "实例%d" % index, "persistentId": 100 + index, "type": "body", "bRepBodyType": "solid" if mode == "split-solid" else "shell", "repBodyIndex": 0, "matrix": [1,0,0,0,0,1,0,0,0,0,1,0,index * 20,0,0,1]})
		var json := directory.path_join(mode + ".json")
		write(json, JSON.stringify(assembly))
		var layer_id: String = AppState.add_source(view, json)
		var layer := await wait_layer(view, layer_id)
		check(layer.status == "Ready", mode + " repaired assembly loads: " + layer.error)
		if layer.status == "Ready":
			var stats: Dictionary = AppState.store.records[layer.data_id].brep_stats
			check(stats.source_shells == 1 and stats.shells == 2 and stats.faces == 2, mode + " source shell splits into two repaired shells")
			check(stats.unique_bodies == 2 and stats.missing_faces == 0, mode + " both repaired regions displayed once")
			if mode == "split-solid": check(stats.source_solids == 1 and stats.solids == 0, "open source solid maps to result shells")
			var first: Node3D = layer.node.get_child(0).get_child(0)
			var second: Node3D = layer.node.get_child(0).get_child(1)
			check(first.get_child_count() == 2 and second.get_child_count() == 2, mode + " occurrence contains all repaired bodies")
			AppState.activate(view, "node", layer.id, first)
			check(AppState.views[view]._selection_highlight.entries.size() == 2, mode + " selecting source occurrence highlights every repaired body")
			if first.get_child_count() == 2 and second.get_child_count() == 2:
				check(first.get_child(0).mesh == second.get_child(0).mesh and first.get_child(1).mesh == second.get_child(1).mesh, mode + " repeated repaired bodies share meshes")
				check(first.get_child(0).get_meta("brep_source_body_index") == 0 and first.get_child(1).get_meta("brep_source_body_index") == 0, mode + " source identity retained across split")
				for body: MeshInstance3D in first.get_children():
					var ranges: Array = body.mesh.get_meta("brep_face_ranges")
					check(ranges.size() == 1 and ranges[0].persistent_id == 42, mode + " split faces retain source persistent ID")
					var pair := _json_pair(body.mesh, ranges[0])
					check(ranges[0].definition.get("source_face_index", -1) == 0 and pair.source.faces[0].wires.size() == 2 and pair.reconstructed.faces[0].wires.size() == 1, mode + " original JSON retains both regions while OCC JSON exports only the selected repaired face")
			check(is_equal_approx(AppState.store.layer_bounds(layer).size.x, 28), mode + " occurrence transforms include both repaired regions")
		assembly.root.children[0].repBodyIndex = 1
		var invalid := directory.path_join(mode + "-invalid.json")
		write(invalid, JSON.stringify(assembly))
		var invalid_id: String = AppState.add_source(view, invalid)
		var invalid_layer := await wait_layer(view, invalid_id)
		check(invalid_layer.status == "Failed" and "source" in invalid_layer.error and "out of range" in invalid_layer.error, mode + " validates source index rather than result index")

func _json_imports(directory: String, view_id: String, shell: Control, original: Dictionary) -> void:
	var seed_bodies: Array[Node] = original.node.find_children("*", "MeshInstance3D", true, false)
	var seed_mesh: ArrayMesh = seed_bodies[0].mesh
	var seed := _json_pair(seed_mesh, seed_mesh.get_meta("brep_face_ranges")[0])
	var paths: Array[String] = []
	for kind in ["source", "reconstructed", "bom"]:
		var text: String = seed.output.reconstructed_json if kind == "reconstructed" else seed.output.source_json
		var path := directory.path_join(kind + "-face.json")
		write(path, "\ufeff" + text if kind == "bom" else text)
		paths.append(path)
		var detected := Sources.detect(path)
		check(detected.get("kind") == "brep" and detected.get("brep_json", false) and not detected.has("dependency"), kind + " JSON is recognized as standalone BREP geometry")
		check(Profile.model_entry(path), kind + " BREP JSON appears as a model candidate")
		shell._open_add(view_id)
		shell._axis.select(1 if kind == "reconstructed" else 0)
		shell._source.text = path
		shell._confirm_add()
		var id: String = AppState.views[view_id].layers.back().id
		var layer := await wait_layer(view_id, id)
		check(layer.status == "Ready", kind + " BREP JSON loads through the add-data dialog: " + layer.error)
		if layer.status != "Ready": continue
		var stats: Dictionary = AppState.store.records[layer.data_id].brep_stats
		check(stats.input_format == "json" and stats.faces == 1 and stats.missing_faces == 0, kind + " JSON uses the SDK geometry parser")
		var expected := Basis(Vector3.RIGHT, -PI / 2.0) if kind == "reconstructed" else Basis.IDENTITY
		check(layer.node.basis.is_equal_approx(expected), kind + " JSON applies the selected up axis once")
		var bodies: Array[Node] = layer.node.find_children("*", "MeshInstance3D", true, false)
		var body: MeshInstance3D = bodies[0]
		var pair := _json_pair(body.mesh, body.mesh.get_meta("brep_face_ranges")[0])
		check(pair.source.faces[0].persistentId == 42 and pair.source.faces[0].wires.size() == 2, kind + " JSON reload preserves original face identity and boundaries")
		check(is_equal_approx(pair.reconstructed.faces[0].area, 84) and pair.reconstructed.vertices.size() == 8, kind + " JSON reconstructs the same trimmed plane")
		if kind == "source":
			var hits := []
			AppState.views[view_id]._mesh_pick(layer.node, Vector3(1,1,10), Vector3(0,0,-1), hits)
			check(not hits.is_empty(), "JSON BREP is pickable")
			if not hits.is_empty():
				hits[0]["layer"] = layer.id
				AppState.select_hit(view_id, hits[0])
				await _wait_definition(shell)
				check(shell._property_fields.find_child("SourceBRepText", true, false) != null and shell._property_fields.find_child("ReconstructedBRepText", true, false) != null, "JSON BREP selection displays both JSON definitions")
			AppState.store.reload_data(layer.data_id)
			check((await wait_layer(view_id, id)).status == "Ready", "standalone BREP JSON reload succeeds")
		AppState.remove_layer(view_id, id)
	var broken := directory.path_join("invalid-topology.json")
	write(broken, str(seed.output.source_json).replace('"startVertexIndex": 0', '"startVertexIndex": 9999999'))
	var invalid_id: String = AppState.add_source(view_id, broken)
	var invalid := await wait_layer(view_id, invalid_id)
	check(invalid.status == "Failed" and "BREP import failed" in invalid.error, "SDK rejects invalid JSON topology without crashing")
	check(Sources.json_text(str(seed.output.source_json).replace('"vertices":', '"missing_vertices":')).has("error"), "incomplete BREP JSON is diagnosed during detection")
	var conflict: Dictionary = seed.source.duplicate(true)
	conflict["brep"] = {"file": "part.brep"}
	conflict["root"] = {}
	check(Sources.json_text(JSON.stringify(conflict)).has("error"), "geometry and assembly markers cannot be mixed")
	conflict.erase("brep")
	conflict["asset"] = {"version": "1.1"}
	check(Sources.json_text(JSON.stringify(conflict)).has("error"), "geometry and 3D Tiles markers cannot be mixed")
	var archive := directory.path_join("json-face.zip")
	var zip := ZIPPacker.new()
	zip.open(archive)
	zip.start_file("geometry/face.json")
	zip.write_file(FileAccess.get_file_as_bytes(paths[0]))
	zip.close_file()
	zip.close()
	var reader := ZIPReader.new()
	reader.open(archive)
	check(Sources.zip_candidates(reader, Profile.features()) == ["geometry/face.json"], "ZIP discovers standalone BREP JSON without a binary dependency")
	reader.close()
	var zip_id: String = AppState.add_source(view_id, archive)
	check((await wait_layer(view_id, zip_id)).status == "Ready", "ZIP containing standalone BREP JSON loads")
	# Assembly references may point to JSON geometry as well as binary BREP.
	var assembly := directory.path_join("json-assembly.json")
	write(assembly, JSON.stringify({"brep": {"file": paths[0].get_file()}, "root": {"name": "JSON零件", "bRepBodyType": "shell", "repBodyIndex": 0}}))
	check(Sources.candidates(PackedStringArray([assembly, paths[0]]), func(path: String): return FileAccess.get_file_as_string(path), Profile.features()) == [assembly], "assembly JSON suppresses its referenced geometry JSON candidate")
	for separator in ["/", "\\"]:
		var entry: String = assembly.replace("\\", "/").replace("/", separator)
		var geometry: String = paths[0].replace("\\", "/").replace("/", separator)
		check(Sources.candidates(PackedStringArray([entry, geometry]), func(path: String): return FileAccess.get_file_as_string(path), Profile.features()) == [entry], "JSON dependency discovery normalizes path separator " + separator)
	var assembly_id: String = AppState.add_source(view_id, assembly)
	check((await wait_layer(view_id, assembly_id)).status == "Ready", "assembly can reference BREP JSON geometry")

func _tessellation_imports(directory: String, view_id: String, shell: Control) -> void:
	var folder := directory.path_join("离散 几何")
	DirAccess.make_dir_recursive_absolute(folder)
	var binary := folder.path_join("cached.brep")
	var geom := folder.path_join("cached.geom")
	for item in [[binary, "brep"], [geom, "geom"]]:
		var file := FileAccess.open(item[0], FileAccess.WRITE)
		file.store_buffer(Marshalls.base64_to_raw(FileAccess.get_file_as_string("res://tests/fixtures/tessellation.%s.base64.txt" % item[1]).strip_edges()))
		file.close()
	var root := {"name": "离散装配", "children": []}
	for i in 2:
		root.children.append({"name": "实例%d" % i, "type": "body", "bRepBodyType": "shell", "repBodyIndex": 0, "persistentId": i+100, "faceTessIds": [101], "edgeTessIds": [102], "matrix": [1,0,0,0,0,1,0,0,0,0,1,0,i*20,0,0,1]})
	var assembly := folder.path_join("cached.json")
	write(assembly, JSON.stringify({"brep": {"file": "cached.brep"}, "root": root}))
	var ids: Array[String] = []
	var geometry_json := folder.path_join("geometry.json")
	var document: RefCounted = ClassDB.instantiate("BRepDocument")
	var seed: Dictionary = document.call("import_model", binary, "", {})
	check(seed.error.is_empty() and seed.stats.geometry_source == "geom", "native GEOM import completes without OCC")
	var source_json: Dictionary = document.call("get_face_brep_json", 0)
	write(geometry_json, source_json.source_json)
	var json_geom := FileAccess.open(folder.path_join("geometry.geom"), FileAccess.WRITE)
	json_geom.store_buffer(FileAccess.get_file_as_bytes(geom))
	json_geom.close()
	for entry in [binary, assembly, geometry_json]:
		var id: String = AppState.add_source(view_id, entry)
		ids.append(id)
		var layer := await wait_layer(view_id, id)
		check(layer.status == "Ready", "GEOM input loads: " + layer.error)
		if layer.status != "Ready": continue
		_check_source_tree(shell.panes[view_id], layer)
		var stats: Dictionary = AppState.store.records[layer.data_id].brep_stats
		check(stats.geometry_source == "geom" and not stats.occ_reconstructed, "GEOM display bypasses OCC reconstruction and meshing")
		check(stats.triangles == 8 and stats.lines == 8 and stats.missing_faces == 0, "exported face and edge primitive counts are preserved")
		check(AppState.store.records[layer.data_id].brep_warnings.is_empty(), "valid GEOM needs no fallback")
		var meshes: Array[Node] = layer.node.find_children("*", "MeshInstance3D", true, false)
		var surfaces: Array[Node] = meshes.filter(func(n: Node): return not n.mesh.get_meta("brep_lines", false))
		var lines: Array[Node] = meshes.filter(func(n: Node): return n.mesh.get_meta("brep_lines", false))
		check(surfaces.size() == (2 if entry == assembly else 1) and lines.size() == surfaces.size(), "GEOM creates face and line meshes for each instance")
		check(lines[0].mesh.surface_get_primitive_type(0) == Mesh.PRIMITIVE_LINES, "exported edges use line primitives")
		var face_material: StandardMaterial3D = surfaces[0].mesh.surface_get_material(0)
		var edge_material: StandardMaterial3D = lines[0].mesh.surface_get_material(0)
		check(face_material.depth_bias_enabled and face_material.depth_bias_constant_factor == 0.5 and face_material.depth_bias_slope_factor == 0.5, "GEOM faces use hardware polygon offset")
		check(not edge_material.depth_bias_enabled and edge_material.shading_mode == BaseMaterial3D.SHADING_MODE_UNSHADED, "GEOM edges retain ordinary material and unmodified depth")
		var arrays: Array = surfaces[0].mesh.surface_get_arrays(0)
		check(arrays[Mesh.ARRAY_VERTEX].size() == 8 and arrays[Mesh.ARRAY_NORMAL][0].z > 0.99, "exported positions stay unchanged and missing normals are generated from buffers")
		if entry == assembly:
			check(surfaces[0].mesh == surfaces[1].mesh and lines[0].mesh == lines[1].mesh, "repeated occurrences share tessellation meshes")
			check(is_equal_approx(surfaces[1].get_parent().position.x, 20.0), "GEOM preserves assembly transforms")
		var hits := []
		AppState.views[view_id]._mesh_pick(layer.node, Vector3(1,1,10), Vector3(0,0,-1), hits)
		check(not hits.is_empty() and hits[0].brep.persistent_id == 42 and hits[0].brep.definition.source_face_index == 0, "GEOM picking maps groups directly to the source face")
		var hole_hits := []
		AppState.views[view_id]._mesh_pick(layer.node, Vector3(5,5,10), Vector3(0,0,-1), hole_hits)
		check(hole_hits.is_empty(), "exported hole remains open and lines do not interfere with face picking")
		if not hits.is_empty():
			hits[0]["layer"] = layer.id
			AppState.select_hit(view_id, hits[0])
			await _wait_definition(shell)
			check(AppState.views[view_id]._selection_highlight.entries.size() == 1, "GEOM selected face is highlighted")
			var pair := _json_pair(surfaces[0].mesh, hits[0].brep)
			check(pair.source.faces[0].persistentId == 42 and is_equal_approx(pair.reconstructed.faces[0].area, 84), "GEOM original JSON and on-demand OCC JSON remain available")
		AppState.store.reload_data(layer.data_id)
		check((await wait_layer(view_id, id)).status == "Ready", "GEOM reload succeeds")
	var archive := folder.path_join("cached.zip")
	var zip := ZIPPacker.new()
	zip.open(archive)
	for path in [binary, geom, assembly]:
		zip.start_file("nested/" + path.get_file())
		zip.write_file(FileAccess.get_file_as_bytes(path))
		zip.close_file()
	zip.close()
	var zip_id: String = AppState.add_source(view_id, archive)
	ids.append(zip_id)
	var zipped := await wait_layer(view_id, zip_id)
	check(zipped.status == "Ready" and AppState.store.records[zipped.data_id].brep_stats.geometry_source == "geom", "ZIP keeps GEOM beside BREP and bypasses OCC")
	# An explicit object reference to a missing tessellation must fall back.
	var bad_root: Dictionary = root.duplicate(true)
	bad_root.children[0].faceTessIds = [999]
	var bad := folder.path_join("bad.json")
	write(bad, JSON.stringify({"brep": {"file": "cached.brep"}, "root": bad_root}))
	var bad_id: String = AppState.add_source(view_id, bad)
	ids.append(bad_id)
	var bad_layer := await wait_layer(view_id, bad_id)
	check(bad_layer.status == "Ready" and AppState.store.records[bad_layer.data_id].brep_stats.geometry_source == "occ" and not AppState.store.records[bad_layer.data_id].brep_warnings.is_empty(), "missing GEOM object references fall back to OCC with a diagnostic")
	var backup := FileAccess.get_file_as_bytes(geom)
	write(geom, "NDSGEOM")
	var corrupt_id: String = AppState.add_source(view_id, binary)
	ids.append(corrupt_id)
	var corrupt := await wait_layer(view_id, corrupt_id)
	check(corrupt.status == "Ready" and AppState.store.records[corrupt.data_id].brep_stats.geometry_source == "occ", "truncated GEOM safely falls back to OCC")
	var file := FileAccess.open(geom, FileAccess.WRITE)
	file.store_buffer(backup)
	file.close()
	# GEOM has only one face; the replacement BREP contains two source faces.
	var original_binary := FileAccess.get_file_as_bytes(binary)
	file = FileAccess.open(binary, FileAccess.WRITE)
	file.store_buffer(Marshalls.base64_to_raw(FileAccess.get_file_as_string("res://tests/fixtures/duplicate-id.brep.base64.txt").strip_edges()))
	file.close()
	var incomplete_id: String = AppState.add_source(view_id, binary)
	ids.append(incomplete_id)
	var incomplete := await wait_layer(view_id, incomplete_id)
	check(incomplete.status == "Ready" and AppState.store.records[incomplete.data_id].brep_stats.geometry_source == "occ", "incomplete GEOM face coverage falls back rather than hiding faces")
	file = FileAccess.open(binary, FileAccess.WRITE)
	file.store_buffer(original_binary)
	file.close()
	for id in ids: AppState.remove_layer(view_id, id)

func _json_pair(mesh: ArrayMesh, face: Dictionary) -> Dictionary:
	var document: RefCounted = mesh.get_meta("brep_document")
	var output: Dictionary = document.call("get_face_brep_json", face.face_index)
	check(str(output.source_error).is_empty() and str(output.reconstructed_error).is_empty(), "both SDK JSON exports succeed: " + str(output))
	var source = JSON.parse_string(output.source_json)
	var reconstructed = JSON.parse_string(output.reconstructed_json)
	check(source is Dictionary and reconstructed is Dictionary, "both SDK outputs are valid JSON BREP documents")
	return {"source": source if source is Dictionary else {}, "reconstructed": reconstructed if reconstructed is Dictionary else {}, "output": output}

func _copy_json(button: Button, expected: String) -> bool:
	# System clipboard listeners can hold the Windows clipboard after a write.
	# Bound retries so persistent failures still fail the same content assertion.
	for attempt in 5:
		await get_tree().create_timer(0.3).timeout
		button.pressed.emit()
		await get_tree().create_timer(0.3).timeout
		if DisplayServer.clipboard_get().replace("\r\n", "\n") == expected: return true
	return false

func _wait_definition(shell: Control) -> void:
	for frame in 3000:
		if not shell._brep_inspector.result.is_empty():
			shell._refresh_properties()
			return
		await get_tree().create_timer(0.01).timeout
	check(false, "face JSON export completes before timeout")

func _definition_properties(shell: Control, body: MeshInstance3D, face: Dictionary) -> void:
	var pair := _json_pair(body.mesh, face)
	var source: Dictionary = pair.source
	check(source.faces.size() == 1 and source.faces[0].persistentId == 42 and source.faces[0].surface.type == "plane", "SDK original JSON contains the selected source plane")
	check(source.unitToMillimeters == 0, "SDK JSON retains unknown legacy units")
	check(source.faces[0].wires.size() == 2 and source.wires[0].edges.size() == 4 and source.wires[1].edges.size() == 4, "SDK source JSON includes outer and inner boundary loops")
	check(source.edges[0].curve.type == "line" and source.vertices[0].point.size() == 3 and is_equal_approx(source.edges[0].tolerance, 1e-7), "SDK source JSON includes curve, vertices and tolerance")
	check(pair.reconstructed.faces.size() == 1 and pair.reconstructed.faces[0].wires.size() == 2 and is_equal_approx(pair.reconstructed.faces[0].area, 84), "SDK OCC JSON includes selected repaired face and its hole")
	await _wait_definition(shell)
	for prefix in ["SourceBRep", "ReconstructedBRep"]:
		var editor: TextEdit = shell._property_fields.find_child(prefix + "Text", true, false)
		check(editor != null and not editor.editable, "property panel has read-only " + prefix + " JSON")
		var expected: String = pair.output.source_json if prefix == "SourceBRep" else pair.output.reconstructed_json
		if editor == null: continue
		check(editor.text == expected, prefix + " panel preserves the SDK JSON string")
		if DisplayServer.get_name() != "headless":
			check(await _copy_json(shell._property_fields.find_child("Copy" + prefix, true, false), expected), "copy button copies exact " + prefix + " JSON")
	var view := AppState.active_id
	AppState.activate(view, "node", AppState.views[view].selected, body.get_parent())
	check(shell._property_fields.find_child("SourceBRepText", true, false) == null and shell._brep_inspector.document == null, "tree selection clears both face JSON panels")
	var document: RefCounted = body.mesh.get_meta("brep_document")
	var invalid: Dictionary = document.call("get_face_brep_json", -1)
	check(not str(invalid.source_error).is_empty() and not str(invalid.reconstructed_error).is_empty(), "invalid face returns independent errors without crashing")

func _face_definitions(node: Node) -> Array:
	var result := []
	if node is MeshInstance3D and node.mesh.has_meta("brep_face_ranges"):
		result.append_array(node.mesh.get_meta("brep_face_ranges"))
	for child in node.get_children(): result.append_array(_face_definitions(child))
	return result

func _definition_fixtures(directory: String, view: String, shell: Control) -> void:
	for mode in ["nurbs", "zero-id", "duplicate-id", "shell-duplicate-id"]:
		var path := directory.path_join(mode + ".brep")
		var file := FileAccess.open(path, FileAccess.WRITE)
		file.store_buffer(Marshalls.base64_to_raw(FileAccess.get_file_as_string("res://tests/fixtures/%s.brep.base64.txt" % mode).strip_edges()))
		file.close()
		var layer_id: String = AppState.add_source(view, path)
		var layer := await wait_layer(view, layer_id)
		check(layer.status == "Ready", mode + " definition fixture loads: " + layer.error)
		if layer.status != "Ready": continue
		if mode == "shell-duplicate-id":
			_check_source_tree(shell.panes[view], layer)
			AppState.activate(view, "layer", layer.id)
			check(layer.node.get_child_count() == 2 and shell.panes[view]._selected_keys.size() == 2, "multiple raw source roots remain siblings and share dataset selection")
		DirAccess.remove_absolute(path)
		for body in layer.node.find_children("*", "MeshInstance3D", true, false):
			for face: Dictionary in body.mesh.get_meta("brep_face_ranges"):
				var definition: Dictionary = face.definition
				var pair := _json_pair(body.mesh, face)
				var source: Dictionary = pair.source
				check(pair.reconstructed.faces.size() == 1 and pair.reconstructed.faces[0].persistentId == face.persistent_id, mode + " OCC JSON contains exactly the selected result")
				if mode == "duplicate-id":
					check(definition.source_mapping == "ambiguous" and source.faces.size() == 2, "duplicate IDs export all original candidates with explicit ambiguity")
				else:
					check(source.faces.size() == 1, mode + " original JSON selects a unique source face")
					if mode == "zero-id": check(source.faces[0].persistentId == 0, "SDK JSON supports zero persistent ID")
					if mode == "shell-duplicate-id":
						check(definition.source_mapping == "shell_persistent_id" and int(source.faces[0].surface.origin[2]) == (0 if definition.source_face_index == 0 else 5), "source shell disambiguation selects the correct SDK JSON")
					if mode == "nurbs":
						var surface: Dictionary = source.faces[0].surface
						check(surface.type == "nurbs" and surface.uDegree == 1 and surface.vDegree == 1 and surface.poles.size() == 12 and surface.weights.size() == 4, "SDK NURBS JSON includes poles, degrees and weights")
						check(absf(surface.poles[2] - 0.12345678901234567) < 1e-16, "SDK JSON preserves double precision")
						var json_path := directory.path_join("nurbs-geometry.json")
						write(json_path, pair.output.source_json)
						var json_id: String = AppState.add_source(view, json_path)
						var json_layer := await wait_layer(view, json_id)
						check(json_layer.status == "Ready", "NURBS BREP JSON loads through the SDK parser: " + json_layer.error)
						if json_layer.status == "Ready":
							var json_bodies: Array[Node] = json_layer.node.find_children("*", "MeshInstance3D", true, false)
							var json_mesh: ArrayMesh = json_bodies[0].mesh
							var json_pair := _json_pair(json_mesh, json_mesh.get_meta("brep_face_ranges")[0])
							check(absf(json_pair.source.faces[0].surface.poles[2] - 0.12345678901234567) < 1e-16 and is_equal_approx(json_pair.source.unitToMillimeters, 1.0), "NURBS JSON import and face export preserve source precision and units")
						AppState.remove_layer(view, json_id)
				# Switch faces before the worker publishes. Only the latest result may appear.
				shell._brep_inspector.inspect(body.mesh.get_meta("brep_document"), face.face_index)
				await get_tree().process_frame
		if mode == "duplicate-id":
			var bodies: Array[Node] = layer.node.find_children("*", "MeshInstance3D", true, false)
			var first_mesh: ArrayMesh = bodies[0].mesh
			var last_mesh: ArrayMesh = bodies.back().mesh
			var first_face: Dictionary = first_mesh.get_meta("brep_face_ranges")[0]
			var last_face: Dictionary = last_mesh.get_meta("brep_face_ranges").back()
			var document: RefCounted = first_mesh.get_meta("brep_document")
			var probe := preload("res://scripts/desktop/brep_inspector.gd").new()
			add_child(probe)
			probe.inspect(document, first_face.face_index)
			await get_tree().process_frame
			probe.inspect(document, last_face.face_index)
			# Wait directly: the UI still refers to a tree selection in this probe.
			for frame in 3000:
				if not probe.result.is_empty(): break
				await get_tree().create_timer(0.01).timeout
			var expected: Dictionary = document.call("get_face_brep_json", last_face.face_index)
			check(probe.result.get("reconstructed_json") == expected.reconstructed_json, "rapid selection publishes only the latest face JSON")
			probe.free()
		AppState.remove_layer(view, layer_id)
		shell._refresh_properties()
	check(shell._brep_inspector.document == null, "removal clears pending face inspection")

func _finish() -> void:
	print("DIGITALVIEWER_BREP_%s profile=%s checks=%d %s" % ["OK" if failures.is_empty() else "FAILED", Profile.id(), checks, str(failures)])
	AppState.shutdown()
	get_tree().quit(0 if failures.is_empty() else 1)
