extends RefCounted
## Explicit viewport API. Native scripts have application permissions.
var _script_id: String
var _application: Node

func _init(application: Node, script_id: String) -> void:
	_application = application
	_script_id = script_id

func viewport_ids() -> Array:
	return _application.views.keys()

func viewport(id: String) -> Node:
	return _application.views.get(id)

func content(id: String) -> Node3D:
	return _application.scripts.content(_script_id, id)
