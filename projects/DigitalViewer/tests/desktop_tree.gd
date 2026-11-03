extends Node

func run(test: Node, shell: Control, view_id: String) -> void:
	var library: Node = AppState.scripts
	var original: String = shell._script.selected_id
	var pane: Control = shell.panes[view_id]
	var view: Node = AppState.views[view_id]
	var tree: Tree = pane.tree
	var source := "extends Node\nfunc run(viewer):\n\tvar content = viewer.content(\"%s\")\n\tcontent.name = \"TreeContent\"\n\tvar branch = Node3D.new()\n\tbranch.name = \"Branch\"\n\tcontent.add_child(branch)\n\tvar leaf = MeshInstance3D.new()\n\tleaf.name = \"Leaf\"\n\tleaf.mesh = BoxMesh.new()\n\tbranch.add_child(leaf)\n\tfor i in 60:\n\t\tvar node = Node3D.new()\n\t\tnode.name = \"Row_%%02d\" %% i\n\t\tcontent.add_child(node)\n" % view_id
	var script_id: String = library.create(source, "tree_interaction.gd", true, view_id)
	library.documents[script_id].runner.run_source(source, true)
	shell._script.hide()
	var second: String = shell.split_view(view_id, false)
	var other_layer: Dictionary = library.ensure_layer(script_id, second)
	var layer: Dictionary = library.ensure_layer(script_id, view_id)
	var leaf: Node = layer.node.get_node("Branch/Leaf")
	var branch: Node = leaf.get_parent()
	var layer_key := "layer:" + str(layer.id)
	var branch_key := "node:%s:%d" % [layer.id, branch.get_instance_id()]
	var leaf_key := "node:%s:%d" % [layer.id, leaf.get_instance_id()]
	await _settle()
	var layer_item: TreeItem = pane._items[layer_key]
	test.check(layer_item.get_text(0) == "tree_interaction.gd" and layer_item.get_text(1).is_empty(), "tree layer row shows only its name, without status or reference count")
	test.check(pane._items[leaf_key].get_text(0) == "Leaf" and pane._items[leaf_key].get_text(1).is_empty(), "tree node row omits type suffix")
	test.check(not layer_item.is_editable(0) and layer_item.is_editable(1), "node name and visibility use separate interaction cells")
	_check_height(test, pane, "split viewport")
	AppState.activate(view_id, "node", layer.id, leaf)
	test.check(_properties(shell).contains("MeshInstance3D") and _properties(shell).contains(tr("Running")), "node type and script status appear in the properties panel")
	pane._items[branch_key].collapsed = true
	AppState.activate(view_id, "node", layer.id, branch)
	AppState.changed.emit()
	await _settle()
	test.check(pane._items[branch_key].collapsed and tree.get_selected() == pane._items[branch_key], "data refresh retains collapsed branches and selected node identity")
	test.check(not shell.panes[second]._items["layer:" + str(other_layer.id)].collapsed, "fold state belongs to its own viewport")
	pane._items[branch_key].collapsed = false
	if DisplayServer.get_name() != "headless":
		# Use native Window input, including the physical-to-logical DPI conversion.
		var camera_pose: Dictionary = view.pose()
		var row_point := tree.get_item_area_rect(pane._items[layer_key], 0).get_center()
		_tree_button(tree, row_point, MOUSE_BUTTON_WHEEL_DOWN, true)
		_tree_button(tree, row_point, MOUSE_BUTTON_WHEEL_DOWN, false)
		await _settle()
		test.check(tree.get_scroll().y > 0 and view.pose() == camera_pose, "wheel over a tree row scrolls the tree without zooming the scene")
		tree.scroll_to_item(pane._items["camera"])
		await _settle()
		var scrollbar: VScrollBar = tree.get_children(true).filter(func(node: Node): return node is VScrollBar)[0]
		var bar_point: Vector2 = scrollbar.position + scrollbar.size * Vector2(0.5, 0.7)
		_tree_button(tree, bar_point, MOUSE_BUTTON_LEFT, true)
		_tree_button(tree, bar_point, MOUSE_BUTTON_LEFT, false)
		await _settle()
		test.check(scrollbar.visible and tree.get_scroll().y > 0 and view.pose() == camera_pose, "tree scrollbar remains interactive without forwarding clicks to the scene")
		tree.scroll_to_item(pane._items["camera"])
		await _settle()
		await _click(tree, pane._items[layer_key], "name")
		test.check(layer.visible and layer.node.visible and not pane._items[layer_key].collapsed and view.selection_kind == "layer", "clicking layer name only selects it")
		test.check(_properties(shell).contains(tr("Status")) and _properties(shell).contains(tr("References") + ": 2"), "layer status and shared reference count remain available in properties")
		await _click(tree, pane._items[layer_key], "fold")
		test.check(pane._items[layer_key].collapsed and layer.visible, "fold arrow collapses without changing visibility")
		await _click(tree, pane._items[layer_key], "name")
		test.check(pane._items[layer_key].collapsed and layer.visible, "clicking a collapsed node name does not expand or hide it")
		await _click(tree, pane._items[layer_key], "check")
		test.check(not layer.visible and not layer.node.visible and pane._items[layer_key].collapsed, "visibility checkbox changes only display and preserves folding")
		test.check(view.selection_kind == "layer" and tree.get_selected() == pane._items[layer_key], "checkbox refresh retains selected item")
		await _click(tree, pane._items[layer_key], "check")
		await _click(tree, pane._items[layer_key], "fold")
		await _click(tree, pane._items[leaf_key], "name")
		test.check(view.selection_kind == "node" and view.selected_node.get_ref() == leaf and layer.visible and not pane._items[branch_key].collapsed, "clicking a generated node selects that node without changing display or folding")
		test.check(_properties(shell).contains("Leaf") and _properties(shell).contains("MeshInstance3D"), "clicked generated node shows its actual name and type in properties")
		await _click(tree, pane._items["group:Scripts"], "name")
		test.check(not pane._items["group:Scripts"].collapsed, "clicking a group label does not toggle expansion")
		await _click(tree, pane._items["group:Scripts"], "fold")
		test.check(pane._items["group:Scripts"].collapsed and layer.visible, "group arrow independently collapses the group")
		AppState.changed.emit()
		await _settle()
		test.check(pane._items["group:Scripts"].collapsed, "group folding survives an application refresh")
		await _click(tree, pane._items["group:Scripts"], "fold")
		AppState.activate(view_id, "node", layer.id, leaf)
		var last: Node = layer.node.get_node("Row_59")
		tree.scroll_to_item(pane._items["node:%s:%d" % [layer.id, last.get_instance_id()]])
		await _settle()
		var scroll := tree.get_scroll()
		var late := Node3D.new()
		late.name = "LateTreeNode"
		layer.node.add_child(late)
		await _settle()
		test.check(scroll.y > 0 and tree.get_scroll().distance_to(scroll) < 2, "dynamic node refresh preserves tree scroll position")
		test.check(tree.get_selected() == pane._items[leaf_key] and pane._items.has("node:%s:%d" % [layer.id, late.get_instance_id()]), "dynamic insertion preserves selection and displays the new node")
		shell.detach_view(view_id)
		await _settle()
		_check_height(test, pane, "detached viewport")
		test.check(tree.size.y > 240, "tall floating viewport is not limited to the old 240 pixel tree height")
		var floating: Window = shell._windows[view_id]
		floating.size = Vector2i(Vector2(600, 380) * floating.content_scale_factor)
		await _settle()
		_check_height(test, pane, "resized floating viewport")
		shell.dock_view(view_id)
		await _settle()
		var split: SplitContainer = pane.get_parent()
		split.split_offset = 80
		await _settle()
		_check_height(test, pane, "resized split")
		var lower: String = shell.split_view(view_id, true)
		await _settle()
		_check_height(test, pane, "short vertical split")
		shell.close_view(lower)
		await _settle()
		tree.scroll_to_item(pane._items["camera"])
		await _settle()
		if "--dv-capture" in OS.get_cmdline_user_args():
			await RenderingServer.frame_post_draw
			get_viewport().get_texture().get_image().save_png(OS.get_environment("DV_TEST_DIRECTORY").get_base_dir().path_join("desktop-tree.png"))
	# Inspecting a node must not keep a deleted script instance alive.
	AppState.activate(view_id, "node", layer.id, leaf)
	library.documents[script_id].runner.stop()
	await _settle()
	test.check(view.selection_kind == "layer" and view.selected_node == null and _properties(shell).contains(tr("Stopped")), "stopping script safely returns removed node selection to its layer properties")
	library.remove(script_id)
	AppState.reset_native()
	shell.close_view(second)
	shell._script.select_script(original)
	AppState.activate(view_id)
	await _settle()

func _check_height(test: Node, pane: Control, context: String) -> void:
	# The headless display has no usable native window; size checks use graphics runs.
	if DisplayServer.get_name() == "headless": return
	test.check(absf(pane.tree.get_rect().end.y - (pane.size.y - 10)) <= 1 and pane.tree.size.y > 0, "tree fills available viewport height: %s (pane=%s, tree=%s)" % [context, pane.size, pane.tree.get_rect()])

func _properties(shell: Control) -> String:
	var lines := PackedStringArray()
	for child in shell._property_fields.get_children():
		if child is Label: lines.append(child.text)
	return "\n".join(lines)

func _click(tree: Tree, item: TreeItem, part: String) -> void:
	var rect := tree.get_item_area_rect(item, 1 if part == "check" else 0)
	var at := rect.get_center()
	if part == "check": at.x = rect.position.x + 10
	elif part == "name": at.x = rect.position.x + rect.size.x * 0.75
	else:
		var depth := -1
		var parent := item.get_parent()
		while parent:
			depth += 1
			parent = parent.get_parent()
		at.x = (depth + 0.5) * tree.get_theme_constant("item_margin")
	for pressed in [true, false]:
		_tree_button(tree, at, MOUSE_BUTTON_LEFT, pressed)
	await _settle()

func _tree_button(tree: Tree, at: Vector2, button: MouseButton, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.pressed = pressed
	event.position = tree.get_global_transform_with_canvas() * at * tree.get_window().content_scale_factor
	tree.get_window().push_input(event)

func _settle() -> void:
	await get_tree().process_frame
	await get_tree().process_frame
	await get_tree().process_frame
