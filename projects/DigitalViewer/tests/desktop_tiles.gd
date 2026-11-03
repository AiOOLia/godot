extends Node
## Containers are generated from the known triangle, not from external datasets.
func run(suite: Node, view_id: String) -> void:
	var directory := OS.get_environment("DV_TEST_DIRECTORY")
	if directory.is_empty(): directory = "res://.godot/desktop-zip-test"
	var doc: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://tests/fixtures/triangle.gltf.txt"))
	var binary := Marshalls.base64_to_raw(doc.buffers[0].uri.get_slice(",", 1))
	doc.buffers[0].erase("uri")
	var helper := preload("res://tests/desktop_network.gd").new()
	var glb: PackedByteArray = helper._glb(doc, binary)
	doc["extras"] = "b3dm"
	var unpadded_glb: PackedByteArray = helper._glb(doc, binary)
	if unpadded_glb.size() % 8 == 0:
		doc["extras"] = "b3dmxxxx"
		unpadded_glb = helper._glb(doc, binary)
	helper.free()
	var b3dm := _legacy("b3dm", {"BATCH_LENGTH": 0}, PackedByteArray(), glb)
	var unpadded := _legacy("b3dm", {"BATCH_LENGTH": 0}, PackedByteArray(), unpadded_glb, false)
	suite.check(unpadded.size() % 8 == 4, "B3DM compatibility fixture omits only final 8-byte padding")
	var pnts := _legacy("pnts", {"POINTS_LENGTH": 3, "POSITION": {"byteOffset": 0}, "CONSTANT_RGBA": [255, 100, 0, 255]}, binary.slice(0, 36))
	var i3dm := _legacy("i3dm", {"INSTANCES_LENGTH": 2, "POSITION": {"byteOffset": 0}}, binary.slice(0, 24), glb)
	var cmpt := PackedByteArray()
	cmpt.resize(16)
	cmpt.encode_u32(0, 0x74706d63)
	cmpt.encode_u32(4, 1)
	cmpt.encode_u32(8, 16 + b3dm.size() + pnts.size())
	cmpt.encode_u32(12, 2)
	cmpt.append_array(b3dm)
	cmpt.append_array(pnts)
	for entry in [["b3dm", b3dm], ["unpadded.b3dm", unpadded], ["pnts", pnts], ["i3dm", i3dm], ["cmpt", cmpt]]:
		var path := directory.path_join("fixture." + entry[0])
		var file := FileAccess.open(path, FileAccess.WRITE)
		file.store_buffer(entry[1])
		file.close()
		var id: String = AppState.add_source(view_id, path, "auto", {"coordinate_system": 1})
		var layer: Dictionary = AppState.views[view_id].get_layer(id)
		var deadline := Time.get_ticks_msec() + 8000
		var stats := {}
		while Time.get_ticks_msec() < deadline:
			await get_tree().process_frame
			if is_instance_valid(layer.node): stats = layer.node.call("get_streaming_stats")
			if layer.status == "Failed" or stats.get("resident_contents", 0) > 0: break
		suite.check(stats.get("resident_contents", 0) > 0, entry[0] + " native content becomes resident: " + layer.error)
		if entry[0] == "pnts" and stats.get("resident_contents", 0) > 0:
			var point: Vector3 = layer.node.call("world_to_local", 0.0, 0.0, 0.0)
			var hit: Dictionary = layer.node.call("intersect_ray", point + Vector3(0, 0, 1), Vector3(0, 0, -1))
			suite.check(hit.has("point_index") and hit.get("batch_id", -1) == 0, "PNTS point picking returns point and batch identifiers")
		AppState.remove_layer(view_id, id)
		DirAccess.remove_absolute(path)
	# Exercise rejection through the exported native engine as well as the C++ parser suite.
	var payload_offset := unpadded.size() - unpadded_glb.size()
	for corruption in 7:
		var invalid := unpadded.duplicate()
		match corruption:
			0: invalid.encode_u32(8, invalid.size() + 4)
			1:
				invalid.resize(invalid.size() - 8)
				invalid.encode_u32(8, invalid.size())
			2: invalid.encode_u32(12, invalid.decode_u32(12) + 4)
			3: invalid.encode_u32(16, 0xffffffff)
			4: invalid.encode_u32(payload_offset + 12, 0xffffffff)
			5: invalid.encode_u32(payload_offset + 8, unpadded_glb.size() - 4)
			6:
				invalid.append(0)
				invalid.encode_u32(8, invalid.size())
		var path := directory.path_join("invalid-%d.b3dm" % corruption)
		var file := FileAccess.open(path, FileAccess.WRITE)
		file.store_buffer(invalid)
		file.close()
		var id: String = AppState.add_source(view_id, path, "auto", {"coordinate_system": 1})
		var layer: Dictionary = AppState.views[view_id].get_layer(id)
		var deadline := Time.get_ticks_msec() + 8000
		while layer.status != "Failed" and Time.get_ticks_msec() < deadline:
			await get_tree().process_frame
		var stats: Dictionary = layer.node.call("get_streaming_stats") if is_instance_valid(layer.node) else {}
		suite.check(layer.status == "Failed" and not layer.error.is_empty() and stats.get("resident_contents", 0) == 0, "corrupt B3DM rejected without partial geometry: %d" % corruption)
		AppState.remove_layer(view_id, id)
		DirAccess.remove_absolute(path)

func _legacy(magic: String, features: Dictionary, binary: PackedByteArray, payload := PackedByteArray(), pad_end := true) -> PackedByteArray:
	var size := 32 if magic == "i3dm" else 28
	var text := JSON.stringify(features).to_utf8_buffer()
	while (size + text.size()) % 8 != 0: text.append(32)
	while binary.size() % 8 != 0: binary.append(0)
	var header := PackedByteArray()
	header.resize(size)
	for index in 4: header[index] = magic.unicode_at(index)
	header.encode_u32(4, 1)
	header.encode_u32(12, text.size())
	header.encode_u32(16, binary.size())
	if size == 32: header.encode_u32(28, 1)
	var result := header + text + binary + payload
	while pad_end and result.size() % 8 != 0: result.append(0)
	result.encode_u32(8, result.size())
	return result
