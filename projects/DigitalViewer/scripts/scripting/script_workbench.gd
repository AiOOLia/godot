extends PanelContainer
signal detach_requested
signal closed
var docked := false
## Embedded runtime workbench, not Godot's editor-only ScriptEditor / debugger.
const Runner = preload("res://scripts/scripting/script_runner.gd")
const Examples = preload("res://scripts/scripting/script_examples.gd")
var runner: Node
var selected_id := ""
var _documents: OptionButton
var _switching := false
var editor: CodeEdit
var snapshot_editor: CodeEdit
var _tabs: TabContainer
var _output: RichTextLabel
var _status: Label
var _filename: LineEdit
var _files: OptionButton
var _search: LineEdit
var _tree: Tree
var _properties: TextEdit
var _confirm: ConfirmationDialog
var _pending: Callable
var _saved_text := ""
var _last_error_line := -1
var _output_dirty := false
var _last_keyboard := 0
var _example_row: Control
var _search_row: Control
var _trust: CheckBox


func _ready() -> void:
	name = "ScriptWorkbench"
	var style := StyleBoxFlat.new()
	style.bg_color = Color("#11212b")
	style.border_color = Color("#395568")
	style.set_border_width_all(1)
	style.content_margin_left = 10
	style.content_margin_right = 10
	style.content_margin_top = 8
	style.content_margin_bottom = 8
	add_theme_stylebox_override("panel", style)
	selected_id = AppState.scripts.create(Examples.satellite(), "satellite.gd", false, "", false)
	runner = AppState.scripts.documents[selected_id].runner
	var column := VBoxContainer.new()
	add_child(column)
	var header := HBoxContainer.new()
	column.add_child(header)
	var title := Label.new()
	title.text = "Script workbench · GDScript"
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	if not OS.has_feature("web"):
		_button(header, "Detach / dock", detach_requested.emit)
	_button(header, "Close", close_workbench)
	var actions := HFlowContainer.new()
	column.add_child(actions)
	_documents = OptionButton.new()
	_documents.fit_to_longest_item = false
	_documents.custom_minimum_size.x = 180
	_documents.tooltip_text = "Select script · running scripts stay active"
	_documents.item_selected.connect(func(index: int): select_script(_documents.get_item_metadata(index)))
	actions.add_child(_documents)
	_button(actions, "New script", func(): _ask_replace("extends Node\n\nfunc run(viewer) -> void:\n\tpass\n", "untitled.gd"))
	_button(actions, "▶ Run", _request_run).tooltip_text = "Compile and run script · Ctrl+Enter"
	_button(actions, "■ Stop", func(): runner.stop()).tooltip_text = "Remove script session nodes; direct edits remain. Synchronous loops cannot be interrupted."
	_button(actions, "Reset scene", _request_reset)
	_button(actions, "Save", _save).tooltip_text = "Save to the application script directory · Ctrl+S"
	_button(actions, "Remove script", _request_remove)
	_tabs = TabContainer.new()
	_tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_tabs.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	column.add_child(_tabs)
	_build_code_tab()
	_output = RichTextLabel.new()
	_output.name = "Output"
	_output.selection_enabled = true
	_output.scroll_following = true
	_output.meta_clicked.connect(_jump_to_entry)
	_tabs.add_child(_output)
	_build_scene_tab()
	snapshot_editor = CodeEdit.new()
	snapshot_editor.name = "Run snapshot"
	snapshot_editor.editable = false
	_configure_editor(snapshot_editor)
	_tabs.add_child(snapshot_editor)
	_build_help_tab()
	_status = Label.new()
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.add_theme_font_size_override("font_size", 12)
	column.add_child(_status)
	_confirm = ConfirmationDialog.new()
	_confirm.title = "Confirm action"
	_confirm.get_ok_button().text = "Confirm"
	_confirm.get_cancel_button().text = "Cancel"
	_confirm.get_label().autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_trust = CheckBox.new()
	_trust.text = "I have reviewed and trust this script"
	_trust.toggled.connect(func(value: bool): _confirm.get_ok_button().disabled = not value)
	_confirm.add_child(_trust)
	_confirm.confirmed.connect(func() -> void:
		var action := _pending
		_pending = Callable()
		if action.is_valid(): action.call())
	_confirm.canceled.connect(_cancel_action)
	add_child(_confirm)
	runner.entry_added.connect(_on_entry)
	runner.state_changed.connect(_update_status)
	AppState.live_scene.ownership_changed.connect(_update_status)
	get_viewport().size_changed.connect(_layout)
	get_parent().resized.connect(_layout)
	minimum_size_changed.connect(func() -> void: _layout.call_deferred())
	AppState.scripts.changed.connect(_refresh_documents)
	_switching = true
	select_script(selected_id)
	_refresh_files()
	_update_status()
	_layout()
	hide()


func _button(parent: Node, text: String, callback: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size.y = 32
	button.pressed.connect(callback)
	parent.add_child(button)
	return button


func _build_code_tab() -> void:
	var column := VBoxContainer.new()
	column.name = "Code"
	_tabs.add_child(column)
	var file_row := HBoxContainer.new()
	column.add_child(file_row)
	_filename = LineEdit.new()
	_filename.text = "satellite.gd"
	_filename.placeholder_text = "filename.gd"
	_filename.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_filename.text_changed.connect(func(_value: String): _stash())
	_filename.text_submitted.connect(func(_value: String): AppState.scripts._sync(selected_id))
	file_row.add_child(_filename)
	_files = OptionButton.new()
	_files.custom_minimum_size.x = 85
	_files.fit_to_longest_item = false
	file_row.add_child(_files)
	_button(file_row, "Open", _load_selected)
	var tools_row := HFlowContainer.new()
	_example_row = tools_row
	column.add_child(tools_row)
	_button(tools_row, "Example: satellite", func() -> void: _ask_replace(Examples.satellite(), "satellite.gd"))
	_button(tools_row, "Example: material", func() -> void: _ask_replace(Examples.material(), "material.gd"))
	var search_row := HBoxContainer.new()
	_search_row = search_row
	column.add_child(search_row)
	_search = LineEdit.new()
	_search.placeholder_text = "Find code · Ctrl+F"
	_search.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_search.text_submitted.connect(func(_text: String) -> void: _find_next())
	search_row.add_child(_search)
	_button(search_row, "Next", _find_next)
	editor = CodeEdit.new()
	editor.name = "NativeCode"
	editor.size_flags_vertical = Control.SIZE_EXPAND_FILL
	editor.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_configure_editor(editor)
	editor.code_completion_requested.connect(_complete)
	editor.text_changed.connect(func() -> void:
		if _last_error_line >= 0 and _last_error_line < editor.get_line_count():
			editor.set_line_background_color(_last_error_line, Color.TRANSPARENT)
		_last_error_line = -1
		_stash()
		_update_status())
	column.add_child(editor)


func _configure_editor(target: CodeEdit) -> void:
	target.gutters_draw_line_numbers = true
	target.gutters_draw_fold_gutter = true
	target.line_folding = true
	target.indent_automatic = true
	target.auto_brace_completion_enabled = true
	target.code_completion_enabled = true
	target.add_theme_font_size_override("font_size", 14)
	var highlighter := CodeHighlighter.new()
	highlighter.number_color = Color("#e9bc85")
	highlighter.symbol_color = Color("#c4d5e5")
	highlighter.function_color = Color("#7ed9e6")
	highlighter.member_variable_color = Color("#a6d7ef")
	for keyword: String in ["extends", "class_name", "func", "var", "const", "if", "elif", "else", "for", "while", "in", "return", "pass", "await", "signal", "true", "false", "null", "and", "or", "not", "is", "as", "static"]:
		highlighter.add_keyword_color(keyword, Color("#d79bea"))
	for type_name: String in ["Node", "Node3D", "MeshInstance3D", "StandardMaterial3D", "Vector3", "Color", "TorusMesh", "BoxMesh", "float", "int", "String", "void", "bool"]:
		highlighter.add_keyword_color(type_name, Color("#8cc8a8"))
	highlighter.add_color_region("#", "", Color("#7b919b"), true)
	highlighter.add_color_region('"', '"', Color("#a5cc8c"))
	highlighter.add_color_region("'", "'", Color("#a5cc8c"))
	target.syntax_highlighter = highlighter
	target.add_comment_delimiter("#", "", true)
	target.add_string_delimiter('"', '"')
	target.add_string_delimiter("'", "'")


func _complete() -> void:
	# Intentionally only a reference list. Runtime semantic completion is not available.
	for word: String in ["extends", "func", "var", "return", "if", "for", "print", "is_instance_valid", "Node", "Node3D", "MeshInstance3D", "StandardMaterial3D", "TorusMesh", "BoxMesh", "SphereMesh", "ShaderMaterial", "Vector3", "Color", "viewer", "root", "content", "camera", "earth", "globe", "environment"]:
		editor.add_code_completion_option(CodeEdit.KIND_PLAIN_TEXT, word, word)
	editor.update_code_completion_options(true)


func _build_scene_tab() -> void:
	var column := VBoxContainer.new()
	column.name = "Scene"
	_tabs.add_child(column)
	_button(column, "Refresh scene tree / properties", refresh_tree)
	var split := VSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(split)
	_tree = Tree.new()
	_tree.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_tree.custom_minimum_size.y = 70
	_tree.item_selected.connect(_inspect)
	split.add_child(_tree)
	_properties = TextEdit.new()
	_properties.editable = false
	_properties.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_properties.custom_minimum_size.y = 60
	_properties.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	split.add_child(_properties)


func refresh_tree() -> void:
	_tree.clear()
	_properties.text = tr("Select a node to inspect its properties. Limited to 256 nodes and 12 levels per viewport.")
	var root_item := _tree.create_item()
	root_item.set_text(0, "DigitalViewer")
	var sessions := _tree.create_item(root_item)
	sessions.set_text(0, tr("Scripts"))
	for doc in AppState.scripts.documents.values():
		var item := _tree.create_item(sessions)
		item.set_text(0, doc.filename)
		item.set_meta("script_id", doc.id)
		if is_instance_valid(doc.runner._session): _add_node(doc.runner._session, item, 0, [256])
	for view in AppState.views.values():
		if is_instance_valid(view.root): _add_node(view.root, root_item, 0, [256])


func _add_node(node: Node, parent: TreeItem, depth: int, budget: Array) -> void:
	if depth > 12 or budget[0] <= 0:
		return
	budget[0] -= 1
	var item := _tree.create_item(parent)
	item.set_text(0, str(node.name))
	item.set_metadata(0, weakref(node))
	for child: Node in node.get_children():
		_add_node(child, item, depth + 1, budget)


func _inspect() -> void:
	var item := _tree.get_selected()
	if item == null:
		return
	if item.has_meta("script_id"):
		var doc: Dictionary = AppState.scripts.documents.get(item.get_meta("script_id"), {})
		if not doc.is_empty(): _properties.text = "%s\nID: %s\n%s: GDScript\n%s: %s" % [doc.filename, doc.id, tr("Type"), tr("Status"), tr(doc.runner.status)]
		return
	if item.get_metadata(0) == null: return
	var object: Object = item.get_metadata(0).get_ref()
	if not is_instance_valid(object):
		_properties.text = tr("This object was deleted. Refresh the scene tree.")
		return
	var lines := PackedStringArray(["%s (%s)" % [object.name, object.get_class()]])
	for property: Dictionary in object.get_property_list():
		if not (int(property.usage) & PROPERTY_USAGE_EDITOR) or property.type == TYPE_NIL:
			continue
		var value: Variant = object.get(property.name)
		var display: String
		if value is Object:
			display = value.get_class() if is_instance_valid(value) else "<freed>"
		elif value is Array or value is Dictionary:
			display = "<%s, %d items>" % [type_string(typeof(value)), value.size()]
		else:
			display = str(value).left(160)
		lines.append("%s = %s" % [property.name, display])
		if lines.size() >= 120:
			break
	_properties.text = "\n".join(lines)


func _build_help_tab() -> void:
	var help := RichTextLabel.new()
	help.name = "Help"
	help.selection_enabled = true
	help.text = preload("res://scripts/desktop/workbench_strings.gd").HELP_EN
	_tabs.add_child(help)


func _ask(message: String, action: Callable) -> void:
	_trust.hide()
	_confirm.get_ok_button().disabled = false
	_pending = action
	_confirm.dialog_text = message
	preload("res://scripts/desktop/dpi_scaling.gd").popup(_confirm, Vector2i(mini(560, int(get_viewport_rect().size.x) - 24), 0))


func _request_run() -> void:
	_stash()
	var source := editor.text
	var id := selected_id
	if AppState.scripts.is_source_confirmed(id, source):
		_run_confirmed_source(id, source)
		return
	_ask("Run only scripts you trust.\nScripts have application permissions to access files, networks and processes.\nCompilation can execute static initializers. Synchronous loops block the UI and cannot be interrupted by Stop.\nReview the code before confirming.", func() -> void:
		if not _trust.button_pressed or not AppState.scripts.documents.has(id): return
		AppState.scripts.confirm_source(id, source)
		_run_confirmed_source(id, source))
	_trust.set_pressed_no_signal(false)
	_trust.show()
	_confirm.get_ok_button().disabled = true


func _run_confirmed_source(id: String, source: String) -> void:
	if not AppState.scripts.is_source_confirmed(id, source): return
	var executor: Node = AppState.scripts.documents[id].runner
	executor.run_source(source, true)
	AppState.audit_event("script.run", "running" if executor.running else "failed", id)
	if selected_id == id:
		snapshot_editor.text = executor.source_snapshot
		_render_output()
		if not executor.running: _tabs.current_tab = 1
	refresh_tree()


func _cancel_action() -> void:
	_pending = Callable()
	_confirm.hide()


func close_workbench() -> void:
	_stash()
	_cancel_action()
	hide()
	closed.emit()


func _request_reset() -> void:
	_ask("Rebuild the scene and restore the navigation snapshot?\nThis stops all scripts and removes their session nodes. Shared resource edits may remain.\nFiles, network activity and other external effects are not rolled back.", func() -> void:
		runner.reset_scene()
		refresh_tree())


func _save() -> void:
	_stash()
	var filename := _filename.text.strip_edges()
	var source := editor.text
	var id := selected_id
	var executor := runner
	var result: Dictionary = runner.save_source(filename, source)
	if result.get("exists", false):
		_ask(tr("Overwrite user://scripts/%s?") % filename, func() -> void:
			if AppState.scripts.documents.has(id): _saved_result(executor.save_source(filename, source, true), source, id))
	else:
		_saved_result(result, source, id)


func _saved_result(result: Dictionary, source: String, id: String) -> void:
	if result.ok:
		AppState.scripts.documents[id].saved_text = source
		if selected_id == id: _saved_text = source
		AppState.scripts._sync(id)
	_refresh_files()
	_update_status()
	if selected_id == id: _status.text = result.message


func _refresh_files() -> void:
	_files.clear()
	for filename: String in runner.list_scripts():
		_files.add_item(filename)


func _load_selected() -> void:
	if _files.selected < 0:
		return
	var filename := _files.get_item_text(_files.selected)
	var result: Dictionary = runner.load_source(filename)
	if result.ok:
		_ask_replace(result.source, filename, true)
	else:
		_status.text = result.message


func _ask_replace(source: String, filename: String, saved := false) -> void:
	# Opening is additive: never discard another script's draft or running session.
	_stash()
	select_script(AppState.scripts.create(source, filename, saved))


func _stash() -> void:
	if _switching or editor == null or not AppState.scripts.documents.has(selected_id): return
	var doc: Dictionary = AppState.scripts.documents[selected_id]
	doc.source = editor.text
	doc.filename = _filename.text
	doc.saved_text = _saved_text
	doc.caret = Vector2i(editor.get_caret_column(), editor.get_caret_line())
	doc.scroll = editor.scroll_vertical

func select_script(id: String) -> void:
	if not AppState.scripts.documents.has(id): return
	_stash()
	if runner.entry_added.is_connected(_on_entry): runner.entry_added.disconnect(_on_entry)
	if runner.state_changed.is_connected(_update_status): runner.state_changed.disconnect(_update_status)
	_switching = true
	selected_id = id
	var doc: Dictionary = AppState.scripts.documents[id]
	runner = doc.runner
	AppState.runner = runner
	editor.text = doc.source
	_filename.text = doc.filename
	_saved_text = doc.saved_text
	editor.set_caret_line(doc.caret.y)
	editor.set_caret_column(doc.caret.x)
	editor.scroll_vertical = doc.scroll
	snapshot_editor.text = runner.source_snapshot
	_last_error_line = -1
	_switching = false
	runner.entry_added.connect(_on_entry)
	runner.state_changed.connect(_update_status)
	_refresh_documents()
	if AppState.store.records.has(doc.data_id):
		for ref in AppState.store.records[doc.data_id].refs:
			if ref.view == AppState.active_id: AppState.activate(ref.view, "layer", ref.layer)
	_render_output()
	refresh_tree()
	_update_status()

func _refresh_documents() -> void:
	if AppState.shutting_down: return
	if not AppState.scripts.documents.has(selected_id):
		if AppState.scripts.documents.is_empty():
			AppState.scripts.create(AppState.scripts.BLANK, "untitled.gd", false, "", false)
		else: select_script(AppState.scripts.documents.keys()[0])
		return
	_documents.clear()
	for doc in AppState.scripts.documents.values():
		_documents.add_item("%s · %s%s" % [doc.filename, doc.id, " · " + tr("Running") if doc.runner.running else ""])
		var index := _documents.item_count - 1
		_documents.set_item_metadata(index, doc.id)
		if doc.id == selected_id: _documents.select(index)
	if visible: refresh_tree()

func _request_remove(id := "") -> void:
	if id.is_empty(): id = selected_id
	_stash()
	if not AppState.scripts.documents.has(id): return
	var doc: Dictionary = AppState.scripts.documents[id]
	var action := func(): AppState.scripts.remove(id)
	if doc.source != doc.saved_text: _ask("Remove this script and discard its unsaved changes?", action)
	else: action.call()


func _find_next() -> void:
	if _search.text.is_empty():
		return
	var found := editor.search(_search.text, 0, editor.get_caret_line(), mini(editor.get_caret_column() + 1, editor.get_line(editor.get_caret_line()).length()))
	if found.x >= 0:
		editor.set_caret_line(found.y)
		editor.set_caret_column(found.x)
		editor.select(found.y, found.x, found.y, found.x + _search.text.length())
		editor.grab_focus()


func _on_entry(_entry: Dictionary) -> void:
	_output_dirty = true


func _process(_delta: float) -> void:
	if _output_dirty:
		_output_dirty = false
		_render_output()
	if visible and DisplayServer.has_feature(DisplayServer.FEATURE_VIRTUAL_KEYBOARD):
		var keyboard := DisplayServer.virtual_keyboard_get_height()
		if keyboard != _last_keyboard:
			_last_keyboard = keyboard
			_layout()


func _render_output() -> void:
	# Rebuild the bounded ring so the visible console cannot grow indefinitely.
	_output.clear()
	for entry: Dictionary in runner.entries:
		_output.push_color(Color("#ff9898") if entry.kind == "error" else Color("#b6d4df"))
		if entry.line > 0:
			_output.push_meta({"line": entry.line, "file": entry.file})
		_output.add_text("[%s%s] %s\n" % [entry.kind, " L%d" % entry.line if entry.line > 0 else "", tr(entry.message)])
		if entry.line > 0:
			_output.pop()
		_output.pop()
		for frame: Dictionary in entry.stack:
			_output.add_text("  %s · %s:%d\n" % [frame.function, frame.file, frame.line])


func _jump_to_entry(meta: Variant) -> void:
	if not meta is Dictionary or meta.file != runner.source_path:
		return
	var target := editor
	if editor.text != runner.source_snapshot:
		target = snapshot_editor
		snapshot_editor.text = runner.source_snapshot
		_tabs.current_tab = 3
	else:
		_tabs.current_tab = 0
	var line := clampi(int(meta.line) - 1, 0, target.get_line_count() - 1)
	target.set_caret_line(line)
	target.set_line_background_color(line, Color("#613341"))
	if target == editor: _last_error_line = line
	target.grab_focus()
	target.center_viewport_to_caret()


func _update_status() -> void:
	if _status == null:
		return
	_status.text = "%s%s · %s" % [tr("Native session") if AppState.live_scene.native_owned else tr("Idle"), " · " + tr("Unsaved") if editor.text != _saved_text else "", tr(runner.status)]


func open_workbench() -> void:
	show()
	_layout()
	refresh_tree()


func _layout() -> void:
	if docked:
		return
	var screen: Vector2 = get_parent().size
	var inset_top := 0.0
	var inset_bottom := 0.0
	if OS.has_feature("mobile"):
		var physical := DisplayServer.screen_get_size()
		var safe := DisplayServer.get_display_safe_area()
		if physical.y > 0:
			inset_top = safe.position.y * screen.y / physical.y
			inset_bottom = maxf(physical.y - safe.end.y, _last_keyboard) * screen.y / physical.y
	var mobile := screen.x < 1100 or OS.has_feature("mobile") or "--dv-force-mobile" in OS.get_cmdline_user_args()
	var width := screen.x - 12 if mobile else clampf(screen.x * 0.56, 620, 980)
	var top := 6.0 + inset_top if mobile else 64.0
	var height := maxf(120, screen.y - top - 10 - inset_bottom)
	_example_row.visible = height >= 460
	_search_row.visible = height >= 460
	_status.max_lines_visible = 1 if height < 460 else 3
	position = Vector2(screen.x - width - 6, top)
	size = Vector2(width, height)


func _unhandled_key_input(event: InputEvent) -> void:
	if not visible or not event is InputEventKey or not event.pressed or event.echo or not event.ctrl_pressed:
		return
	match event.keycode:
		KEY_ENTER: _request_run()
		KEY_S: _save()
		KEY_F:
			_tabs.current_tab = 0
			_search_row.show()
			_search.grab_focus()
		_: return
	get_viewport().set_input_as_handled()

