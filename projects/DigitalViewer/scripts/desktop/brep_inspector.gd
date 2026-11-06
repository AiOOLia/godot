extends Node
## One worker, latest selection wins. The document owns a bounded JSON cache.
signal completed
var document: RefCounted
var face_index := -1
var result: Dictionary = {}
var _thread: Thread
var _running_document: RefCounted
var _running_face := -1

func inspect(value: RefCounted, index: int) -> Dictionary:
	if value != document or index != face_index:
		document = value
		face_index = index
		result = {}
	return result

func clear() -> void:
	document = null
	face_index = -1
	result = {}

func _process(_delta: float) -> void:
	if _thread != null and not _thread.is_alive():
		var output: Dictionary = _thread.wait_to_finish()
		_thread = null
		var current := document == _running_document and face_index == _running_face
		_running_document = null
		if current:
			result = output
			completed.emit()
	if _thread == null and document != null and face_index >= 0 and result.is_empty():
		_running_document = document
		_running_face = face_index
		var worker_document := document
		var worker_face := face_index
		_thread = Thread.new()
		var error := _thread.start(func() -> Dictionary: return worker_document.call("get_face_brep_json", worker_face))
		if error != OK:
			_thread = null
			_running_document = null
			result = {"source_error": error_string(error), "reconstructed_error": error_string(error)}
			completed.emit()

func _exit_tree() -> void:
	clear()
	if _thread != null:
		_thread.wait_to_finish()
		_thread = null
	_running_document = null
