extends Node
## Traditional OSG text, external texture, paging, UTF-8 and Windows ANSI paths.
func run(suite: Node, view_id: String) -> void:
	var directory := OS.get_environment("DV_TEST_DIRECTORY").path_join("传统OSG")
	DirAccess.make_dir_recursive_absolute(directory)
	var image := Image.create(2, 2, false, Image.FORMAT_RGBA8)
	image.fill(Color.ORANGE)
	image.save_png(directory.path_join("texture.png"))
	var fixture := FileAccess.get_file_as_string("res://tests/fixtures/triangle-legacy.osg.txt")
	var child := fixture.substr(fixture.find("  Geode {"))
	child = child.substr(0, child.rfind("}"))
	_write(directory.path_join("child.osg"), child.to_utf8_buffer())
	for encoding in ["UTF-8", "CP_ACP"]:
		if encoding == "CP_ACP" and OS.get_name() != "Windows": continue
		var text := fixture.replace("@DATABASE@", directory.replace("\\", "/") + "/")
		var bytes := text.to_utf8_buffer() if encoding == "UTF-8" else text.to_multibyte_char_buffer(encoding)
		var path := directory.path_join("root.osg")
		_write(path, bytes)
		var id: String = AppState.add_source(view_id, path)
		var layer: Dictionary = AppState.views[view_id].get_layer(id)
		var stats := {}
		var deadline := Time.get_ticks_msec() + 8000
		while Time.get_ticks_msec() < deadline:
			await get_tree().process_frame
			if is_instance_valid(layer.node): stats = layer.node.get_streaming_stats()
			if layer.status == "Failed" or (stats.get("resident_contents", 0) >= 2 and stats.get("resident_textured_materials", 0) >= 2): break
		suite.check(layer.error.is_empty() and stats.get("resident_contents", 0) >= 2 and stats.get("failed_contents", 0) == 0, "traditional OSG root and child load using " + encoding + ": " + layer.error)
		suite.check(stats.get("resident_textured_materials", 0) >= 2, "traditional OSG external images load via texture queue: " + encoding)
		AppState.remove_layer(view_id, id)
	# LOAD_IMMEDIATELY is deferred too; the child stays a streamed content.
	var proxy_path := directory.path_join("proxy.osg")
	_write(proxy_path, 'ProxyNode { ExtRefMode LOAD_IMMEDIATELY FileNameList 1 { "child.osg" } }'.to_utf8_buffer())
	var id: String = AppState.add_source(view_id, proxy_path)
	var layer: Dictionary = AppState.views[view_id].get_layer(id)
	var stats := {}
	var deadline := Time.get_ticks_msec() + 8000
	while Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
		if is_instance_valid(layer.node): stats = layer.node.get_streaming_stats()
		if layer.status == "Failed" or stats.get("resident_contents", 0) >= 2: break
	suite.check(layer.error.is_empty() and stats.get("resident_contents", 0) >= 2 and stats.get("visible_render_instances", 0) > 0, "legacy ProxyNode loads children through the streaming queue")
	AppState.remove_layer(view_id, id)
	await _empty_tiles(suite, view_id, directory, fixture, child)
	for file in ["root.osg", "proxy.osg", "child.osg", "texture.png", "empty.osgb", "nested.osg", "truncated.osgb", "points.osg", "empty-paged.osg"]:
		DirAccess.remove_absolute(directory.path_join(file))
	DirAccess.remove_absolute(directory)

func _write(path: String, bytes: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_buffer(bytes)
	file.close()

func _empty_tiles(suite: Node, view_id: String, directory: String, fixture: String, triangle: String) -> void:
	var empty := _empty_osgb()
	var cases := [
		["empty.osgb", empty, true],
		["nested.osg", "Group { Group { } Geode { } }".to_utf8_buffer(), true],
		["truncated.osgb", empty.slice(0, empty.size() - 1), false],
		["points.osg", triangle.replace("TRIANGLES", "POINTS").to_utf8_buffer(), false],
	]
	for entry in cases:
		var path := directory.path_join(entry[0])
		_write(path, entry[1])
		var id: String = AppState.add_source(view_id, path)
		var layer: Dictionary = AppState.views[view_id].get_layer(id)
		var stats := {}
		var deadline := Time.get_ticks_msec() + 8000
		while Time.get_ticks_msec() < deadline:
			await get_tree().process_frame
			if is_instance_valid(layer.node): stats = layer.node.get_streaming_stats()
			if layer.status == "Failed" or stats.get("resident_contents", 0) > 0: break
		if entry[2]:
			suite.check(layer.error.is_empty() and stats.get("resident_contents", 0) == 1 and stats.get("resident_render_instances", -1) == 0, "empty OSG container becomes resident without geometry: " + entry[0])
		else:
			suite.check(layer.status == "Failed" and not layer.error.is_empty() and stats.get("resident_contents", 0) == 0, "empty tile compatibility still rejects " + entry[0])
		AppState.remove_layer(view_id, id)
	# An empty detailed child completes its replacement region; siblings remain visible.
	var paged := fixture.replace("@DATABASE@", directory.replace("\\", "/") + "/").replace("child.osg", "empty.osgb")
	var path := directory.path_join("empty-paged.osg")
	_write(path, ("Group { " + triangle + paged + " }").to_utf8_buffer())
	var id: String = AppState.add_source(view_id, path)
	var layer: Dictionary = AppState.views[view_id].get_layer(id)
	var stats := {}
	var deadline := Time.get_ticks_msec() + 8000
	while Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
		if is_instance_valid(layer.node): stats = layer.node.get_streaming_stats()
		if layer.status == "Failed" or stats.get("resident_contents", 0) >= 2: break
	suite.check(layer.error.is_empty() and stats.get("resident_contents", 0) == 2 and stats.get("failed_contents", 0) == 0 and stats.get("hidden_fallback_surfaces", 0) == 1 and stats.get("visible_render_instances", 0) == 1, "empty paged child replaces its fallback without hiding sibling geometry")
	AppState.views[view_id].zoom(1000.0)
	for frame in 8: await get_tree().process_frame
	stats = layer.node.get_streaming_stats()
	suite.check(stats.get("visible_render_instances", 0) == 2 and stats.get("hidden_fallback_surfaces", -1) == 0, "zooming out restores coarse geometry after an empty detailed tile")
	AppState.remove_layer(view_id, id)

func _empty_osgb() -> PackedByteArray:
	# Synthetic uncompressed OSGB v91: Group with default Object/Node fields, no children.
	var bytes := PackedByteArray([0xa1, 0x0e, 0x91, 0x6c, 0x45, 0x45, 0xfb, 0x1a])
	for value in [1, 91, 0, 1]: _u32(bytes, value)
	bytes.append(48) # Compressor "0".
	_u32(bytes, 10)
	bytes.append_array("osg::Group".to_utf8_buffer())
	for value in [1, 0, 2]: _u32(bytes, value) # ID, empty name, UNSPECIFIED variance.
	bytes.append_array(PackedByteArray([0, 0, 0, 0, 0, 0, 1])) # User data, callbacks, culling.
	_u32(bytes, 0xffffffff)
	bytes.append_array(PackedByteArray([0, 0])) # No state set or children.
	return bytes

func _u32(bytes: PackedByteArray, value: int) -> void:
	var offset := bytes.size()
	bytes.resize(offset + 4)
	bytes.encode_u32(offset, value)
