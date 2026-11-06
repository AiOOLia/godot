extends Node
const Profile = preload("res://scripts/desktop/build_profile.gd")
const Ui = preload("res://scripts/desktop/ui_widgets.gd")
var checks := 0
var failures: Array[String] = []

func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures.append(message)
		push_error("PROFILE_CHECK: " + message)

func run(shell: Control) -> void:
	var id: String = AppState.active_id
	check(Profile.id() in ["CAD", "ALL"], "known profile")
	var light_tests := preload("res://tests/desktop_lights.gd").new()
	add_child(light_tests)
	await light_tests.run(self, shell, id)
	light_tests.queue_free()
	check(AppState.describe().profile == Profile.id(), "automation reports selected profile")
	check(shell.get_window().title == Profile.value("product_name"), "window branding")
	check(shell.theme == load(Profile.value("theme")), "selected theme resource")
	check(Ui.ACCENT == Profile.color("ACCENT"), "widget palette follows profile")
	check(Ui.svg_texture(Profile.value("icon")) != null, "profile icon renders")
	check(shell._add_source_buttons[2].visible == Profile.enabled("gis"), "GIS source card")
	check(shell._add_source_buttons[3].visible == Profile.enabled("3dtiles"), "ion source card")
	check(shell._kind.is_item_disabled(2) == not Profile.enabled("gis"), "GIS source selector")
	check(shell._axis.visible == (Profile.enabled("brep") or Profile.enabled("osg")), "model up-axis selector visible in CAD and ALL")
	shell._select_add_kind(4)
	check(not shell._axis.visible, "up-axis selector hidden for scripts")
	shell._select_add_kind(1)
	check(shell._axis.visible, "up-axis selector available for model URLs")
	shell._select_add_kind(0)
	check(shell._add_folder.visible == Profile.enabled("osg"), "OSG directory picker")
	var filters := ",".join(Profile.file_filters())
	check(("*.osgb" in filters) == Profile.enabled("osg"), "OSG file filters")
	check(("*.b3dm" in filters) == Profile.enabled("3dtiles"), "tiles file filters")
	check(Profile.model_entry("folder/model.osgb") == Profile.enabled("osg"), "OSG archive candidates")
	check(Profile.model_entry("res://tests/fixtures/tileset.json") == Profile.enabled("3dtiles"), "tiles candidates require content")
	check(Profile.model_entry("folder/model.brep") == Profile.enabled("brep"), "BREP archive candidates")
	if not OS.has_feature("editor"):
		check(OS.has_feature("dv_" + Profile.id().to_lower()), "export feature tag")
		check(ClassDB.class_exists("OsgTiledMeshOptions") == Profile.enabled("osg"), "native OSG matches profile")
		check(ClassDB.class_exists("Tiles3DOptions") == Profile.enabled("3dtiles"), "native tiles matches profile")
		check(ClassDB.class_exists("TiledMeshInstance3D") == (Profile.enabled("osg") or Profile.enabled("3dtiles")), "native streaming matches profile")
	# Denials happen before files or downloads, including explicit importer kinds.
	if Profile.id() == "CAD":
		for request in [
			["missing.osgb", "auto", {}], ["missing.osg", "gltf", {}],
			["res://tests/fixtures/tileset.json", "auto", {}],
			["missing.b3dm", "tiled", {}], ["", "ellipsoid", {}],
			["", "tiled", {"ion_asset_id": 1, "ion_token": "test"}],
		]:
			var layer_id: String = AppState.add_source(id, request[0], request[1], request[2])
			await get_tree().process_frame
			var layer: Dictionary = AppState.views[id].get_layer(layer_id)
			check(layer.status == "Failed" and "disabled" in layer.error, "blocked source: " + str(request[0]))
			AppState.remove_layer(id, layer_id)
		check(AppState.store._requests.is_empty(), "denied sources never start downloads")
		AppState.views[id].fly_to(120, 30, 1000)
		AppState.views[id].set_grid(true)
		check(AppState.views[id].flight == null and not AppState.views[id].grid_visible, "GIS actions disabled")
	var script_source := "extends Node\nfunc run(viewer) -> void:\n\tvar mesh = MeshInstance3D.new()\n\tmesh.name = \"ProfileScriptBox\"\n\tmesh.mesh = BoxMesh.new()\n\tviewer.content(\"%s\").add_child(mesh)\n" % id
	var script_id: String = AppState.scripts.create(script_source, "profile-test.gd", false, id)
	var runner: Node = AppState.scripts.documents[script_id].runner
	check(runner.run_source(script_source, true), "reviewed native GDScript still executes")
	var script_layer: Dictionary = AppState.scripts.ensure_layer(script_id, id)
	check(is_instance_valid(script_layer.node) and script_layer.node.find_child("ProfileScriptBox", true, false) != null, "script geometry is attached to the viewport")
	AppState.scripts.remove(script_id)
	var directory := OS.get_environment("DV_TEST_DIRECTORY")
	if directory.is_empty(): directory = "res://.godot/profile-test"
	DirAccess.make_dir_recursive_absolute(directory)
	var model := directory.path_join("profile-model.gltf")
	var fixture := FileAccess.get_file_as_bytes("res://tests/fixtures/triangle.gltf.txt")
	var file := FileAccess.open(model, FileAccess.WRITE)
	file.store_buffer(fixture)
	file.close()
	var model_layer: String = AppState.add_source(id, model)
	await _wait_layer(id, model_layer)
	check(AppState.views[id].get_layer(model_layer).status == "Ready", "glTF still loads")
	var other: String = shell.split_view(id, false)
	check(AppState.share_layer(id, model_layer, other), "models share across viewports")
	var archive := directory.path_join("profile-mixed.zip")
	var zip := ZIPPacker.new()
	zip.open(archive)
	for entry in ["model.gltf", "disabled.osg", "tileset.json"]:
		zip.start_file(entry)
		zip.write_file(fixture if entry.ends_with("gltf") else "{}".to_utf8_buffer())
		zip.close_file()
	zip.close()
	var archive_layer: String = AppState.add_source(id, archive, "auto", {} if Profile.id() == "CAD" else {"entry": "model.gltf"})
	await _wait_layer(id, archive_layer)
	check(AppState.views[id].get_layer(archive_layer).status == "Ready", "mixed ZIP selects a supported model")
	if Profile.id() == "CAD":
		var denied: String = AppState.add_source(id, archive, "auto", {"entry": "disabled.osg"})
		await _wait_layer(id, denied)
		check(AppState.views[id].get_layer(denied).status == "Failed", "explicit ZIP entry cannot bypass profile")
	if "--dv-capture" in OS.get_cmdline_user_args() and DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(directory.path_join("profile-%s.png" % Profile.id()))
	for view_id in AppState.views:
		for layer in AppState.views[view_id].layers.duplicate(): AppState.remove_layer(view_id, layer.id)
	DirAccess.remove_absolute(model)
	DirAccess.remove_absolute(archive)
	if failures.is_empty(): print("DIGITALVIEWER_PROFILE_OK profile=%s checks=%d" % [Profile.id(), checks])
	else: print("DIGITALVIEWER_PROFILE_FAILED " + str(failures))
	AppState.shutdown()
	get_tree().quit(0 if failures.is_empty() else 1)

func _wait_layer(view_id: String, layer_id: String) -> void:
	for frame in 600:
		var layer: Dictionary = AppState.views[view_id].get_layer(layer_id)
		if layer.status in ["Ready", "Failed"]: return
		await get_tree().create_timer(0.01).timeout
