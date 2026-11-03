extends Node

func run_single_view_sequence(test: Node, shell: Control, view_id: String) -> void:
	var library: Node = AppState.scripts
	var workbench: Control = shell._script
	var selected_id: String = workbench.selected_id
	var view: Node = AppState.views[view_id]
	var pose: Dictionary = view.pose()
	test.check(AppState.views.size() == 1, "tree then satellite regression starts with exactly one viewport")
	var tree_source := FileAccess.get_file_as_string("res://tests/fixtures/add_tree.gd.txt")
	var satellite_source := FileAccess.get_file_as_string("res://tests/fixtures/satellite.gd.txt")
	var tree_id: String = library.create(tree_source, "add_tree.gd", true, view_id)
	var satellite_id: String = library.create(satellite_source, "satellite.gd", true, view_id)
	var tree_runner: Node = library.documents[tree_id].runner
	var satellite_runner: Node = library.documents[satellite_id].runner
	test.check(tree_runner.run_source(tree_source, true), "actual add_tree.gd runs in the single viewport")
	test.check(satellite_runner.run_source(satellite_source, true), "actual corrected satellite.gd runs after add_tree.gd in the same viewport")
	var tree_layer: Dictionary = library.ensure_layer(tree_id, view_id)
	var satellite_layer: Dictionary = library.ensure_layer(satellite_id, view_id)
	test.check(is_instance_valid(tree_layer.node) and tree_layer.node.name == "LowPolyTree" and tree_layer.node.get_child_count() == 31, "tree geometry survives starting the satellite")
	var satellite: MeshInstance3D = satellite_runner._instance.satellite
	var initial_basis: Basis = satellite.basis
	await get_tree().create_timer(0.15).timeout
	test.check(tree_runner.running and satellite_runner.running and not satellite.basis.is_equal_approx(initial_basis), "tree and satellite stay running while the actual torus animates")
	test.check(satellite.get_parent() == satellite_layer.node and satellite.get_viewport() == view.viewport and satellite.get_world_3d() == view.root.get_world_3d() and satellite.is_visible_in_tree(), "animated satellite belongs to the displayed viewport world")
	workbench.select_script(satellite_id)
	workbench.refresh_tree()
	test.check(_tree_has(workbench._tree.get_root(), "LowPolyTree") and _tree_has(workbench._tree.get_root(), "MeshInstance3D"), "single-view tree and satellite both appear in the structure")
	if DisplayServer.get_name() != "headless":
		var hidden: Array[Image] = []
		for visible_satellite in [false, true]:
			AppState.set_visible(view_id, satellite_layer.id, visible_satellite)
			await RenderingServer.frame_post_draw
			await RenderingServer.frame_post_draw
			hidden.append(view.viewport.get_texture().get_image())
		test.check(hidden[0].get_data() != hidden[1].get_data(), "satellite changes actual rendered pixels after the tree, not only the structure")
		if "--dv-capture" in OS.get_cmdline_user_args():
			hidden[1].save_png(OS.get_environment("DV_TEST_DIRECTORY").get_base_dir().path_join("desktop-tree-satellite.png"))
	library.remove(satellite_id)
	test.check(tree_runner.running and is_instance_valid(tree_layer.node), "removing the animated satellite preserves the existing tree")
	library.remove(tree_id)
	AppState.reset_native()
	view.restore_pose(pose)
	workbench.select_script(selected_id)
	AppState.activate(view_id)

func run(test: Node, shell: Control, first: String, second: String) -> void:
	var library: Node = AppState.scripts
	var workbench: Control = shell._script
	var original: String = workbench.selected_id
	var before: Array = library.documents.keys()
	# Exercise the same Add data confirmation used by the UI.
	shell._target_id = first
	shell._kind.select(4)
	shell._kind.item_selected.emit(4)
	shell._source.clear()
	shell._confirm_add()
	var a: String = workbench.selected_id
	var doc_a: Dictionary = library.documents[a]
	var layer_a: Dictionary = library.ensure_layer(a, first)
	test.check(a != original and layer_a.kind == "script" and layer_a.status == "Review required", "Add data creates a script layer and opens its own document")
	test.check(workbench.editor.text == library.BLANK and not doc_a.runner.running, "new script starts with valid entry without execution")
	var source_a := _source(first, "CubeA", "LOG_A")
	workbench.editor.text = source_a + "# unsaved A\n"
	workbench._filename.text = "same_name.gd"
	# Import a second script, deliberately sharing a filename with the first.
	var path := OS.get_environment("DV_TEST_DIRECTORY").path_join("same_name.gd")
	var source_b := _source(second, "CubeB", "LOG_B")
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_string(source_b)
	file.close()
	shell._source.text = path
	shell._confirm_add()
	await get_tree().process_frame
	var b: String = workbench.selected_id
	var doc_b: Dictionary = library.documents[b]
	var layer_b: Dictionary = library.ensure_layer(b, first)
	test.check(b != a and doc_b.filename == "same_name.gd" and workbench.editor.text == source_b, "Add data imports .gd into a distinct document even with the same name")
	test.check(not doc_b.runner.running and doc_a.source.ends_with("# unsaved A\n"), "import preserves another script's unsaved draft and never runs code")
	workbench.select_script(a)
	test.check(workbench.editor.text == doc_a.source and workbench._filename.text == "same_name.gd", "switching scripts restores independent draft and filename")
	test.check(doc_a.runner.run_source(source_a, true) and doc_b.runner.run_source(source_b, true), "two scripts run concurrently")
	await get_tree().process_frame
	test.check(doc_a.runner.running and doc_b.runner.running and layer_a.status == "Running" and layer_b.status == "Running", "both script layers show running status")
	test.check(doc_a.runner._instance.ticks > 0 and doc_b.runner._instance.ticks > 0, "both independent script instances receive process callbacks")
	var target_b: Dictionary = library.ensure_layer(b, second)
	test.check(layer_a.node.find_child("CubeA", true, false) != null and target_b.node.find_child("CubeB", true, false) != null, "script contents belong to their explicitly targeted viewport layers")
	test.check(_has_log(doc_a.runner, "LOG_A") and not _has_log(doc_a.runner, "LOG_B") and _has_log(doc_b.runner, "LOG_B") and not _has_log(doc_b.runner, "LOG_A"), "concurrent script output is isolated by source identity")
	workbench.select_script(b)
	test.check(workbench.snapshot_editor.text == source_b and workbench.runner == doc_b.runner, "editor selection restores the corresponding run snapshot and executor")
	workbench.refresh_tree()
	test.check(_tree_has(workbench._tree.get_root(), "UserScript") and _tree_has(workbench._tree.get_root(), "CubeA") and _tree_has(workbench._tree.get_root(), "CubeB"), "workbench structure shows application script instances and both viewport contents")
	shell.panes[first].refresh()
	shell.panes[second].refresh()
	test.check(_tree_has(shell.panes[first].tree.get_root(), "CubeA") and _tree_has(shell.panes[second].tree.get_root(), "CubeB"), "viewport layer trees expose running script nodes")
	if "--dv-capture" in OS.get_cmdline_user_args() and DisplayServer.get_name() != "headless":
		workbench.open_workbench()
		workbench._tabs.current_tab = 2
		await get_tree().create_timer(0.2).timeout
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("DV_TEST_DIRECTORY").get_base_dir().path_join("desktop-multiple-scripts.png"))
		workbench._tabs.current_tab = 0
	var late := Node3D.new()
	late.name = "LateNode"
	target_b.node.add_child(late)
	await get_tree().process_frame
	await get_tree().process_frame
	test.check(_tree_has(shell.panes[second].tree.get_root(), "LateNode"), "nodes added after run automatically appear in the structure tree")
	AppState.set_visible(first, layer_a.id, false)
	test.check(not layer_a.node.visible and target_b.node.visible, "script layer visibility affects only that script in that viewport")
	doc_a.runner.stop()
	test.check(not is_instance_valid(layer_a.node) and doc_b.runner.running and is_instance_valid(target_b.node), "stopping A preserves B and its content")
	test.check(doc_a.runner.run_source(source_a, true) and doc_b.runner.running, "rerunning A preserves B")
	test.check(not layer_a.node.visible, "rerun retains script layer visibility")
	test.check(not doc_a.runner.run_source("extends RefCounted\nfunc run(viewer):\n\tpass\n", true) and doc_b.runner.running, "invalid script entry fails independently without stopping B")
	test.check(layer_a.status == "Failed" and not layer_a.error.is_empty(), "script layer reports its own error")
	doc_a.runner.run_source(source_a, true)
	var inspect: Dictionary = JSON.parse_string(AppState.bridge.call_tool("script.inspect", {}).content[0].text)
	test.check(inspect.running_count == 2 and inspect.scripts.size() == library.documents.size(), "MCP inspect enumerates all script sessions")
	# Imported code is text-only, including native static initialization.
	var untrusted := "extends Node\nstatic func _static_init():\n\tEngine.set_meta(\"dv_import_executed\", true)\nfunc run(viewer):\n\tpass\n"
	file = FileAccess.open(path, FileAccess.WRITE)
	file.store_string(untrusted)
	file.close()
	AppState.add_source(first, path)
	await get_tree().process_frame
	test.check(not Engine.has_meta("dv_import_executed") and doc_a.runner.running and doc_b.runner.running, "import never compiles static initializers or stops existing scripts")
	if DisplayServer.get_name() != "headless":
		await _check_workbench_lifecycle(test, shell, a, b, source_a, source_b)
	AppState.remove_layer(first, layer_a.id)
	test.check(not library.documents.has(a) and doc_b.runner.running and is_instance_valid(target_b.node), "removing script A leaves script B running")
	# Closing an output viewport only releases its reference while other refs exist.
	var extra: String = shell.split_view(first, false)
	library.content(b, extra)
	shell.close_view(extra)
	test.check(doc_b.runner.running and is_instance_valid(target_b.node), "closing one referenced viewport preserves the application script")
	AppState.reset_native()
	test.check(not doc_b.runner.running and not is_instance_valid(target_b.node) and layer_b.status == "Stopped", "global reset stops all scripts and leaves reusable script layers")
	doc_b.runner.run_source(source_b, true)
	AppState.remove_layer(second, target_b.id)
	test.check(not library.documents.has(b) and AppState.views[first].get_layer(layer_b.id).is_empty(), "removing an application script clears all of its viewport references")
	var last_view: String = shell.split_view(first, false)
	var last_id: String = library.create(library.BLANK, "last_reference.gd", false, last_view)
	var last_runner: Node = library.documents[last_id].runner
	workbench.select_script(last_id)
	last_runner.run_source(library.BLANK, true)
	shell.close_view(last_view)
	test.check(not library.documents.has(last_id) and not last_runner.running and workbench.selected_id != last_id, "closing the final viewport reference cleans up the script and selects a surviving document")
	for id in library.documents.keys().duplicate():
		if id not in before: library.remove(id)
	workbench.select_script(original)
	workbench.hide()
	shell._kind.select(0)
	shell._kind.item_selected.emit(0)
	DirAccess.remove_absolute(path)

func _check_workbench_lifecycle(test: Node, shell: Control, a: String, b: String, source_a: String, source_b: String) -> void:
	var library: Node = AppState.scripts
	var workbench: Control = shell._script
	var executor: Node = library.documents[a].runner
	var other: Node = library.documents[b].runner
	var other_instance: Node = other._instance
	var previous: Node = executor._instance
	shell._show_panel("script")
	workbench.select_script(a)
	workbench.editor.text = source_a
	workbench._request_run()
	test.check(workbench._confirm.visible and workbench._confirm.get_ok_button().disabled, "first UI run requires review even after internal runner execution")
	workbench._confirm.canceled.emit()
	test.check(not workbench._pending.is_valid() and executor._instance == previous and not library.is_source_confirmed(a, source_a), "canceling review preserves running script without recording confirmation")
	workbench._request_run()
	var later_draft := source_a + "# edited while confirmation is pending\n"
	workbench.editor.text = later_draft
	workbench.select_script(b)
	_confirm_run(workbench)
	test.check(executor.source_snapshot == source_a and other.source_snapshot == source_b and other._instance == other_instance, "pending run confirmation stays bound to its original script and source")
	test.check(library.is_source_confirmed(a, source_a) and not library.is_source_confirmed(a, later_draft) and not library.is_source_confirmed(b, source_b), "confirmation authorizes only the captured document and code")
	workbench.select_script(a)
	test.check(workbench.editor.text == later_draft, "confirming captured source preserves subsequent draft edits")
	workbench.editor.text = source_a
	previous = executor._instance
	workbench._request_run()
	test.check(not workbench._confirm.visible and executor.running and executor._instance != previous and other._instance == other_instance, "unchanged confirmed code reruns directly without restarting the other script")
	executor.stop()
	workbench._request_run()
	test.check(executor.running and not workbench._confirm.visible and other._instance == other_instance, "explicit Stop retains confirmation for unchanged rerun")
	# An edit containing a static initializer must remain text until reviewed.
	var edited := source_a + "\nstatic func _static_init():\n\tEngine.set_meta(\"dv_reviewed_edit\", true)\n"
	workbench.editor.text = edited
	previous = executor._instance
	workbench._request_run()
	test.check(workbench._confirm.visible and workbench._confirm.get_ok_button().disabled and executor._instance == previous and not Engine.has_meta("dv_reviewed_edit"), "edited source requires fresh review before compilation and preserves previous execution")
	workbench._confirm.canceled.emit()
	test.check(not library.is_source_confirmed(a, edited) and executor._instance == previous, "canceling an edited version does not approve it or stop the previous version")
	workbench._request_run()
	_confirm_run(workbench)
	test.check(Engine.has_meta("dv_reviewed_edit") and executor.source_snapshot == edited and library.is_source_confirmed(a, edited), "explicit review authorizes and executes the edited version")
	Engine.remove_meta("dv_reviewed_edit")
	# Exercise the actual header Close callback in both docked and floating layouts.
	var close_button: Button = workbench.get_child(0).get_child(0).get_children().filter(func(node: Node): return node is Button and node.text == "Close")[0]
	for mode in ["docked", "native window", "floating header"]:
		shell._show_panel("script")
		if mode != "docked": shell._float_panel("script")
		var instance: Node = executor._instance
		var content: Node = library.ensure_layer(a, library.documents[a].view_id).node
		var ticks: int = instance.ticks
		var other_ticks: int = other_instance.ticks
		if mode == "native window": shell._windows.script.close_requested.emit()
		else: close_button.pressed.emit()
		await get_tree().process_frame
		await get_tree().process_frame
		test.check(not workbench.visible and not shell._windows.has("script"), "%s Close releases the panel and native window" % mode)
		test.check(executor.running and other.running and executor._instance == instance and other._instance == other_instance and library.ensure_layer(a, library.documents[a].view_id).node == content, "%s Close preserves both instances and scene content" % mode)
		test.check(instance.ticks > ticks and other_instance.ticks > other_ticks, "%s Close preserves both process callbacks" % mode)
		shell._show_panel("script")
		test.check(workbench.editor.text == edited and workbench.snapshot_editor.text == edited and executor._instance == instance, "%s reopen restores draft and snapshot without restarting execution" % mode)
		workbench._request_run()
		test.check(not workbench._confirm.visible and executor.running and executor._instance != instance and other._instance == other_instance, "%s reopen retains confirmation for direct rerun" % mode)
		Engine.remove_meta("dv_reviewed_edit")
	# Closing while review is pending cancels that action, not the active sessions.
	workbench.editor.text = edited + "# unconfirmed change\n"
	workbench._request_run()
	close_button.pressed.emit()
	test.check(not workbench._confirm.visible and not workbench._pending.is_valid() and executor.running and other.running, "closing panel cancels pending review while preserving execution")
	shell._show_panel("script")
	workbench.editor.text = edited
	var duplicate: String = library.create(edited, library.documents[a].filename, true)
	workbench.select_script(duplicate)
	workbench._request_run()
	test.check(workbench._confirm.visible and workbench._confirm.get_ok_button().disabled and not Engine.has_meta("dv_reviewed_edit"), "new document with identical name and code requires its own confirmation")
	_confirm_run(workbench)
	test.check(library.documents[duplicate].runner.running and executor.running and other.running, "new document runs independently after its own confirmation")
	library.remove(duplicate)
	Engine.remove_meta("dv_reviewed_edit")
	duplicate = library.create(edited, library.documents[a].filename, true)
	workbench.select_script(duplicate)
	workbench._request_run()
	test.check(workbench._confirm.visible and not library.is_source_confirmed(duplicate, edited), "removing and reopening a script does not inherit the removed document confirmation")
	workbench._confirm.canceled.emit()
	library.remove(duplicate)
	workbench.select_script(a)
	# MCP remains a request to the UI, including for already reviewed source.
	previous = executor._instance
	var previous_permission: bool = AppState.bridge.permissions["script.run"]
	AppState.bridge.permissions["script.run"] = true
	var result: Dictionary = AppState.bridge.call_tool("script.run", {})
	AppState.bridge.permissions["script.run"] = previous_permission
	test.check("manual_run_required" in str(result) and executor._instance == previous, "MCP cannot automatically execute an already confirmed local script")

func _confirm_run(workbench: Control) -> void:
	workbench._trust.button_pressed = true
	workbench._confirm.confirmed.emit()
	workbench._confirm.hide()

func _source(view_id: String, node_name: String, log_message: String) -> String:
	return "extends Node\nvar ticks = 0\nfunc run(viewer):\n\tvar content = viewer.content(\"%s\")\n\tvar node = MeshInstance3D.new()\n\tnode.name = \"%s\"\n\tnode.mesh = BoxMesh.new()\n\tcontent.add_child(node)\n\tprint(\"%s\")\nfunc _process(_delta):\n\tticks += 1\n" % [view_id, node_name, log_message]

func _has_log(runner: Node, message: String) -> bool:
	for entry in runner.entries:
		if entry.message.strip_edges() == message: return true
	return false

func _tree_has(item: TreeItem, text: String) -> bool:
	if item == null: return false
	if text in item.get_text(0): return true
	for child in item.get_children():
		if _tree_has(child, text): return true
	return false
