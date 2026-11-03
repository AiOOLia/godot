extends ConfirmationDialog
class_name DigitalViewerAddDataDialog

var _source_type: OptionButton
var _source_edit: LineEdit
var _token_edit: LineEdit
var _token_label: Label
var _file_dialog: FileDialog
var _error_label: Label


func _ready() -> void:
	confirmed.connect(_on_confirmed)
	_build_content()


func open_dialog() -> void:
	_error_label.text = ""
	popup_centered(Vector2i(520, 360))
	_source_edit.grab_focus()


func _build_content() -> void:
	var root := VBoxContainer.new()
	root.add_theme_constant_override("separation", 12)
	add_child(root)

	var intro := Label.new()
	intro.text = "选择会话数据入口。此原型先创建图层状态，格式加载器将在后续模块接入。"
	intro.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	intro.add_theme_color_override("font_color", Color("9cabb1"))
	root.add_child(intro)

	var type_label := Label.new()
	type_label.text = "数据来源"
	root.add_child(type_label)
	_source_type = OptionButton.new()
	_source_type.add_item("本地文件或目录")
	_source_type.add_item("远程 HTTP(S) / XYZ / WMTS")
	_source_type.add_item("Cesium ion")
	_source_type.item_selected.connect(_on_type_selected)
	root.add_child(_source_type)

	var source_label := Label.new()
	source_label.text = "路径或 URL"
	root.add_child(source_label)
	var source_row := HBoxContainer.new()
	root.add_child(source_row)
	_source_edit = LineEdit.new()
	_source_edit.placeholder_text = "选择文件，或输入资源地址"
	_source_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	source_row.add_child(_source_edit)
	var browse := Button.new()
	browse.text = "浏览…"
	browse.pressed.connect(_on_browse_pressed)
	source_row.add_child(browse)

	_token_label = Label.new()
	_token_label.text = "会话 Access Token（不会保存）"
	_token_label.visible = false
	root.add_child(_token_label)
	_token_edit = LineEdit.new()
	_token_edit.secret = true
	_token_edit.placeholder_text = "仅保留在当前会话内存中"
	_token_edit.visible = false
	root.add_child(_token_edit)

	_error_label = Label.new()
	_error_label.add_theme_color_override("font_color", Color("ff858c"))
	_error_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	root.add_child(_error_label)

	_file_dialog = FileDialog.new()
	_file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	_file_dialog.access = FileDialog.ACCESS_FILESYSTEM
	_file_dialog.use_native_dialog = true
	_file_dialog.add_filter("*.gltf, *.glb, *.json, *.b3dm, *.i3dm, *.pnts, *.cmpt, *.osg, *.osgb, *.tif, *.tiff, *.zip ; DigitalViewer 数据")
	_file_dialog.file_selected.connect(_on_file_selected)
	add_child(_file_dialog)


func _on_type_selected(index: int) -> void:
	var is_local := index == 0
	var is_ion := index == 2
	_source_edit.placeholder_text = "选择本地文件" if is_local else ("输入 Cesium ion Asset ID" if is_ion else "https://example.com/tileset.json")
	_token_label.visible = is_ion
	_token_edit.visible = is_ion


func _on_browse_pressed() -> void:
	if _source_type.selected != 0:
		_source_edit.grab_focus()
		return
	_file_dialog.popup_centered_ratio(0.78)


func _on_file_selected(path: String) -> void:
	_source_edit.text = path


func _on_confirmed() -> void:
	var source := _source_edit.text.strip_edges()
	if source.is_empty():
		_error_label.text = "请输入资源地址或选择本地文件。"
		call_deferred("popup_centered", Vector2i(520, 360))
		return
	var source_names := ["本地文件", "远程资源", "Cesium ion"]
	AppState.add_mock_layer(source_names[_source_type.selected], source)
	_source_edit.clear()
	_token_edit.clear()

