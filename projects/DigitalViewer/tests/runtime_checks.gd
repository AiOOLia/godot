extends RefCounted
## Exercises interpreted programs, real viewport projections and recovery semantics.

var _test: Node
var _serial := 0


func run(host: Node, app: Control) -> void:
	_test = host
	var ai := AiAssistant
	var runtime: Node = AppState.runtime
	var saved: Dictionary = runtime.snapshot()
	var original_color: String = runtime.objects.globe.props["material.albedo_color"]
	var original_camera := AppState.camera_snapshot()
	ai.new_chat()
	var desktop: DigitalViewerViewport = app._desktop_shell.find_children("*", "DigitalViewerViewport", true, false)[0]
	var mobile: DigitalViewerViewport = app._mobile_shell.find_children("*", "DigitalViewerViewport", true, false)[0]
	var examples = preload("res://scripts/runtime/scene_examples.gd")
	_check(runtime.schemas.sphere.has("material.albedo_color") and runtime.schemas.sphere.has("position"), "runtime reflects engine material and node property types")
	var result := _program(examples.red_spin())
	_check(result.ok and runtime.behaviors.has("地球自转"), "program commits color and a persistent behavior")
	_check(desktop._runtime_objects.globe.material_override.albedo_color.to_html(false) == "ef3545", "program changes visible globe material")
	_check(desktop._runtime_objects.globe.material_override.albedo_color == mobile._runtime_objects.globe.material_override.albedo_color, "both viewports project shared material")
	var angle: Array = runtime.objects.earth.props.rotation_degrees.duplicate()
	await host.get_tree().create_timer(0.15).timeout
	_check(runtime.objects.earth.props.rotation_degrees != angle, "behavior progresses locally after command finishes")
	_check(desktop._earth_content.rotation.is_equal_approx(mobile._earth_content.rotation), "both viewports share behavior phase")
	_program([{"op": "pause", "id": "*"}])
	angle = runtime.objects.earth.props.rotation_degrees.duplicate()
	await host.get_tree().create_timer(0.1).timeout
	_check(runtime.objects.earth.props.rotation_degrees == angle and runtime.behaviors["地球自转"].paused, "pause freezes local execution")
	_program([{"op": "resume", "id": "*"}])
	await host.get_tree().create_timer(0.1).timeout
	_check(runtime.objects.earth.props.rotation_degrees != angle, "resume continues local execution")
	# Test emergency Stop against the original single-program undo entry.
	runtime.restore_snapshot(saved)
	ai.new_chat()
	_program(examples.red_spin())
	ai.stop()
	angle = runtime.objects.earth.props.rotation_degrees.duplicate()
	await host.get_tree().create_timer(0.1).timeout
	_check(runtime.behaviors.is_empty() and runtime.objects.earth.props.rotation_degrees == angle, "Stop cancels ongoing behavior and retains current scene")
	result = _command("undo_last_action", {})
	_check(result.ok and runtime.objects.globe.props["material.albedo_color"] == original_color and runtime.behaviors.is_empty(), "whole program undo still works after emergency Stop")
	result = _program(examples.red_spin())
	result = _program(examples.restore_original())
	_check(result.ok and runtime.objects.globe.props["material.albedo_color"] == original_color and runtime.behaviors.is_empty(), "checkpoint restores objects and behavior state")
	var before: Dictionary = runtime.snapshot()
	var revision := AppState.revision
	result = _program([{"op": "set", "id": "globe", "property": "material.albedo_color", "value": "#ff0000"}, {"op": "set", "id": "globe", "property": "script", "value": "bad"}])
	_check(not result.ok and runtime.snapshot() == before and AppState.revision == revision, "late invalid property rolls back whole transaction")
	result = _program([{"op": "repeat", "count": 64, "body": [{"op": "repeat", "count": 64, "body": [{"op": "let", "name": "x", "value": 1}]}]}])
	_check(not result.ok and "预算" in result.message and runtime.snapshot() == before, "nested work hits instruction budget without partial effects")
	for instruction: Dictionary in [
		{"op": "execute_script", "source": "while true: pass"},
		{"op": "set", "id": "globe", "property": "visible", "value": "false"},
		{"op": "set", "id": "earth", "property": "scale", "value": [0, 1, 1]},
		{"op": "let", "name": "x", "value": {"div": [1, 0]}},
		{"op": "let", "name": "x", "value": {"var": "missing"}},
		{"op": "create", "id": "unsafe", "type": "HTTPRequest"},
		{"op": "create", "id": [], "type": {}},
		{"op": "pause", "id": {}},
		{"op": "repeat", "count": "64", "body": []},
		{"op": "if", "condition": 1, "then": []},
		{"op": "delete", "id": "globe"},
		{"op": "animate", "id": "bad", "body": [{"op": "create", "id": "x", "type": "box"}]},
		{"op": "animate", "id": "bad", "body": [{"op": "set", "id": "missing", "property": "visible", "value": true}]},
	]:
		result = _program([instruction])
		_check(not result.ok and runtime.snapshot() == before, "invalid program is bounded and non-mutating: " + str(instruction.op))
	result = _program(examples.orbit_ring())
	_check(result.ok and runtime.objects.has("ball_11") and runtime.objects.size() == saved.objects.size() + 13, "loops, variables, math and conditions create a ring of objects")
	_check(runtime.objects.ball_0.props["material.albedo_color"] == "#55ccffff" and runtime.objects.ball_11.props["material.albedo_color"] == "#ffbb44ff", "conditional branches produce different materials")
	_check(desktop._runtime_objects.ball_11.get_parent() == desktop._runtime_objects.ring and mobile._runtime_objects.has("ball_11"), "created hierarchy appears in both viewports")
	result = _program([{"op": "delete", "id": "ring"}])
	_check(result.ok and not runtime.objects.has("ball_11") and not desktop._runtime_objects.has("ring") and runtime.behaviors.is_empty(), "deleting subtree stops dependent behavior and removes projections")
	result = _command("undo_last_action", {})
	_check(result.ok and runtime.objects.has("ring") and desktop._runtime_objects.has("ball_11"), "undo recreates deleted objects and hierarchy")
	runtime.stop_all()
	result = _program([{"op": "delete", "id": "ring"}, {"op": "create", "id": "ring", "type": "box", "parent": "earth"}])
	_check(result.ok and desktop._runtime_objects.ring.mesh is BoxMesh and desktop._runtime_objects.ring.get_parent() == desktop._earth_content, "same ID recreated with different type and parent rebuilds projection")
	result = _program([{"op": "set", "id": "globe", "property": "visible", "value": false}, {"op": "set", "id": "grid", "property": "visible", "value": false}])
	_check(result.ok and not AppState.get_layer(&"globe").visible and not AppState.grid_visible and not desktop._grid_mesh_instance.visible, "runtime visibility updates application and renderer")
	AppState.set_grid_visible(true)
	_check(runtime.objects.grid.props.visible and mobile._grid_mesh_instance.visible, "manual visibility updates runtime model")
	result = _program(examples.red_spin())
	result = _program([{"op": "animate", "id": "conflict", "body": [{"op": "set", "id": "earth", "property": "rotation_degrees", "value": [0, 0, 0]}]}])
	_check(not result.ok and not runtime.behaviors.has("conflict"), "two behaviors cannot own the same property")
	result = _program([{"op": "set", "id": "earth", "property": "rotation_degrees", "value": [0, 0, 0]}])
	_check(result.ok and runtime.behaviors.is_empty(), "direct manipulation cancels behavior owning modified property")
	result = _program([{"op": "animate", "id": "short", "duration": 0.08, "body": [{"op": "set", "id": "earth", "property": "rotation_degrees", "value": [0, {"mul": [{"var": "t"}, 100]}, 0]}]}])
	await host.get_tree().create_timer(0.2).timeout
	_check(result.ok and runtime.behaviors.is_empty() and is_equal_approx(runtime.objects.earth.props.rotation_degrees[1], 8.0), "finite animation completes at exact endpoint")
	result = _program([{"op": "animate", "id": "late_error", "body": [{"op": "if", "condition": {"gt": [{"var": "t"}, 0.02]}, "then": [{"op": "set", "id": "globe", "property": "material.albedo_color", "value": "bad"}]}]}])
	await host.get_tree().create_timer(0.1).timeout
	_check(result.ok and runtime.behaviors.is_empty() and "已停止" in runtime.last_result, "runtime error stops behavior with visible feedback")
	result = _program([{"op": "set", "id": "globe", "property": "material.albedo_color", "value": "#aa2233"}])
	AppState.orbit(Vector2(0.01, 0))
	result = _command("undo_last_action", {})
	_check(not result.ok, "manual edit prevents stale runtime undo")
	ai.configure(true, ai.client.endpoint, "", "")
	await ai.submit("地球变红并自转")
	_check(runtime.behaviors.has("地球自转") and runtime.objects.globe.props["material.albedo_color"] == "#ef3545ff", "chat demo executes scene program")
	await ai.submit("停止自转")
	_check(runtime.behaviors.is_empty(), "chat stops program independently of model requests")
	app._chat._load_program_example(1)
	# Clear previous ring before testing the user-facing program editor.
	if runtime.objects.has("ring"): _program([{"op": "delete", "id": "ring"}])
	app._chat._run_program()
	_check(runtime.objects.has("ball_11"), "program editor runs the same interpreter")
	app._chat._undo_program()
	_check(not runtime.objects.has("ring"), "program editor undoes a complete generated scene")
	app._chat._tabs.current_tab = 3
	app.size = Vector2(844, 390)
	app._chat.set_mobile(true)
	await host.get_tree().process_frame
	await host.get_tree().process_frame
	_check(app._chat.position.y >= 0 and app._chat.get_rect().end.y <= 391.0, "program editor fits mobile landscape")
	app._chat._tabs.current_tab = 0
	runtime.restore_snapshot(saved)
	AppState.restore_camera(original_camera)
	ai.new_chat()


func _program(program: Array) -> Dictionary:
	return _command("scene_run", {"program_json": JSON.stringify(program)})


func _command(name: String, args: Dictionary) -> Dictionary:
	_serial += 1
	return AiAssistant.commands.execute("runtime_test/%d" % _serial, name, args, AppState.revision)


func _check(condition: bool, description: String) -> void:
	_test.check(condition, description)
