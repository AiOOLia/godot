extends Node
## Portable, budgeted JSON scene programs. No eval, filesystem, network or native calls.
## Records are authoritative in JSON mode; native takeover suspends their scene projection.

signal scene_changed
signal activity_changed
signal behavior_finished(id: String, status: String, message: String)

const MAX_OBJECTS := 64
const MAX_BEHAVIORS := 8
const MAX_STEPS := 2048
const MAX_DEPTH := 16
const TRANSFORM := ["position", "rotation_degrees", "scale", "visible"]
const MATERIAL := ["material.albedo_color", "material.metallic", "material.roughness", "material.emission_enabled", "material.emission", "material.emission_energy_multiplier", "material.shading_mode", "material.transparency"]
const CREATABLE := ["group", "sphere", "box", "cylinder", "light"]

var state: Node
var objects: Dictionary = {}
var behaviors: Dictionary = {}
var checkpoints: Dictionary = {}
var schemas: Dictionary = {}
var last_program: Array = []
var last_result := "尚未执行场景程序。"
var _error := ""
var _steps := 0
var _world: Dictionary
var _jobs: Dictionary
var _saves: Dictionary
var _frame_mode := false
var _active_job := ""
var _writes: Array[String] = []


func initialize(app_state: Node) -> void:
	state = app_state
	for kind: String in ["group", "sphere", "box", "cylinder", "light", "grid", "camera", "environment"]:
		var prototype := make_object(kind)
		var properties: Array = []
		if prototype is Node3D and kind != "camera":
			properties.append_array(TRANSFORM)
		if prototype is MeshInstance3D:
			properties.append_array(MATERIAL)
		match kind:
			"sphere": properties.append_array(["mesh.radius", "mesh.height"])
			"box": properties.append("mesh.size")
			"cylinder": properties.append_array(["mesh.top_radius", "mesh.bottom_radius", "mesh.height"])
			"light": properties.append_array(["light_color", "light_energy"])
			"camera": properties.append("fov")
			"environment": properties.append_array(["background_color", "ambient_light_color", "ambient_light_energy"])
		var schema := {}
		for path: String in properties:
			var target: Object = prototype
			var field := path
			if "." in path:
				var parts := path.split(".")
				target = prototype.get("material_override" if parts[0] == "material" else parts[0])
				field = parts[1]
			for info: Dictionary in target.get_property_list():
				if str(info.name) == field:
					schema[path] = {"type": type_string(info.type), "default": encode(target.get(field)), "hint": str(info.hint_string)}
		schemas[kind] = schema
		if prototype is Node:
			prototype.free()
	_add_builtin("world", "场景根", "group", "", {})
	_add_builtin("earth", "地球组（含经纬网与标记；自转请修改此对象）", "group", "world", {})
	_add_builtin("globe", "地球", "sphere", "earth", {"mesh.radius": 2.0, "mesh.height": 4.0, "material.albedo_color": "#28798bff", "material.metallic": 0.08, "material.roughness": 0.8})
	_add_builtin("grid", "经纬网", "grid", "earth", {"material.albedo_color": "#adfae87a", "material.shading_mode": 0, "material.transparency": 1})
	_add_builtin("marker", "定位标记", "sphere", "earth", {"position": [0.61, 0.64, 1.82], "mesh.radius": 0.055, "mesh.height": 0.11, "material.albedo_color": "#2cb79aff", "material.emission_enabled": true, "material.emission": "#2cb79aff", "material.emission_energy_multiplier": 1.8})
	_add_builtin("key_light", "主光源", "light", "world", {"rotation_degrees": [-24, -35, 0], "light_color": "#d8f6f0ff", "light_energy": 1.35})
	_add_builtin("rim_light", "轮廓光", "light", "world", {"rotation_degrees": [15, 145, 0], "light_color": "#4b9ec4ff", "light_energy": 0.55})
	_add_builtin("camera", "浏览相机（轨道和飞行用已有相机工具）", "camera", "world", {"fov": 48.0})
	_add_builtin("environment", "背景与环境光", "environment", "world", {"background_color": "#07131aff", "ambient_light_color": "#8dbfc3ff", "ambient_light_energy": 0.42})


func _add_builtin(id: String, label: String, kind: String, parent: String, overrides: Dictionary) -> void:
	var record := _new_record(id, kind, parent)
	record["name"] = label
	record["builtin"] = true
	record.props.merge(overrides, true)
	objects[id] = record


func _new_record(id: String, kind: String, parent: String) -> Dictionary:
	var props := {}
	for key: String in schemas[kind]:
		props[key] = schemas[kind][key].default
	return {"id": id, "name": id, "type": kind, "parent": parent, "builtin": false, "props": props.duplicate(true)}


func make_object(kind: String) -> Object:
	if kind == "environment":
		return Environment.new()
	if kind == "camera":
		return Camera3D.new()
	if kind == "group":
		return Node3D.new()
	if kind == "light":
		return DirectionalLight3D.new()
	var node := MeshInstance3D.new()
	match kind:
		"sphere":
			var mesh := SphereMesh.new()
			mesh.radial_segments = 48
			mesh.rings = 24
			node.mesh = mesh
		"box": node.mesh = BoxMesh.new()
		"cylinder": node.mesh = CylinderMesh.new()
	node.material_override = StandardMaterial3D.new()
	return node


func encode(value: Variant) -> Variant:
	if value is Color:
		return "#" + value.to_html(true)
	if value is Vector3:
		return [value.x, value.y, value.z]
	return value


func apply_properties(target: Object, record: Dictionary) -> void:
	for key: String in record.props:
		var value: Variant = record.props[key]
		match schemas[record.type][key].type:
			"Color": value = Color(value)
			"Vector3": value = Vector3(value[0], value[1], value[2])
		var path := key.replace("material.", "material_override.").replace(".", ":")
		target.set_indexed(NodePath(path), value)


func describe(id: String) -> Dictionary:
	if id == "":
		var inventory: Array = []
		for record: Dictionary in objects.values():
			inventory.append({"id": record.id, "name": record.name, "type": record.type, "parent": record.parent, "builtin": record.builtin})
		return {"objects": inventory, "creatable_types": CREATABLE, "behaviors": behavior_summary(), "checkpoints": checkpoints.keys(), "hint": "指定 id 查询属性与当前值。"}
	if not objects.has(id):
		return {"error": "对象不存在：" + id}
	return {"object": objects[id].duplicate(true), "properties": schemas[objects[id].type].duplicate(true)}


func language() -> Dictionary:
	return {
		"version": "SceneProgram/1", "format": "JSON 数组，每项一个指令；不是 GDScript。事务成功才提交，失败返回原因可修正重试。",
		"instructions": [
			{"op": "set", "id": "globe", "property": "material.albedo_color", "value": "#ff3030"},
			{"op": "create", "id": "ball", "type": "sphere", "parent": "world", "props": {"position": [3, 0, 0], "scale": [0.3, 0.3, 0.3]}},
			{"op": "delete", "id": "ball"},
			{"op": "let", "name": "speed", "value": 8},
			{"op": "repeat", "count": 5, "as": "i", "body": []},
			{"op": "if", "condition": {"lt": [1, 2]}, "then": [], "else": []},
			{"op": "animate", "id": "spin", "duration": 0, "body": [{"op": "set", "id": "earth", "property": "rotation_degrees", "value": [0, {"mul": [{"var": "t"}, 8]}, 0]}]},
			{"op": "pause", "id": "*"}, {"op": "resume", "id": "*"},
			{"op": "stop", "id": "*"}, {"op": "checkpoint", "name": "原样"}, {"op": "restore", "name": "原样"}],
		"expressions": "字面量/数组；{var:变量名}、{get:[对象ID,属性]}；add/sub/mul/div/min/max/eq/lt/gt/concat 接收两个元素数组；sin/cos 接收弧度数。可嵌套。concat 可生成循环对象ID。",
		"animation": "t=本行为已运行秒数，dt=本帧秒数；捕获启动时变量。duration=0 持续直到停止，其他值最多3600秒。每帧只允许 set/let/repeat/if。一个属性同时只允许一个动画写入；直接set或删除会停止写入该属性的行为。",
		"limits": "64对象、8行为、8检查点；程序24KiB、2048计算步、嵌套16层、循环64次；每行为每帧256步。无文件/网络/原生代码执行。",
		"units": "position/scale/rotation_degrees 为三个数字；旋转单位度，位置为预览世界单位；颜色#RRGGBB或#RRGGBBAA，透明需 material.transparency=1。",
		"ranges": "尺寸/scale分量0.01–100；position/rotation分量绝对值≤100000；mesh尺寸0.001–100，球体height≥2*radius；metallic/roughness 0–1；shading_mode=0无光照/1材质，transparency=0不透明/1透明；fov 10–120度；light_energy等其它数值0–100。",
		"types": schemas.duplicate(true),
	}


func behavior_summary() -> Array:
	var result: Array = []
	for id: String in behaviors:
		result.append({"id": id, "elapsed": snappedf(behaviors[id].elapsed, 0.1), "duration": behaviors[id].duration, "status": "paused" if behaviors[id].paused else "running"})
	return result


func snapshot() -> Dictionary:
	return {"objects": objects.duplicate(true), "behaviors": behaviors.duplicate(true), "checkpoints": checkpoints.duplicate(true)}


func restore_snapshot(saved: Dictionary) -> void:
	objects = saved.objects.duplicate(true)
	behaviors = saved.behaviors.duplicate(true)
	checkpoints = saved.checkpoints.duplicate(true)
	last_result = "已恢复场景与持续行为。"
	_commit()


func execute(program: Array) -> Dictionary:
	if state.live_scene != null and state.live_scene.native_owned:
		return {"ok": false, "status": "native_owned", "message": "原生脚本已接管场景；请先在脚本工作台重置场景。"}
	if JSON.stringify(program).to_utf8_buffer().size() > 24576:
		return {"ok": false, "status": "failed", "message": "场景程序超过24 KiB限制。"}
	_error = ""
	_steps = 0
	_frame_mode = false
	_active_job = ""
	_writes.clear()
	_world = objects.duplicate(true)
	_jobs = behaviors.duplicate(true)
	_saves = checkpoints.duplicate(true)
	_run(program, {}, 0)
	_validate_geometry()
	if _steps > MAX_STEPS: _fail("执行预算耗尽（计算步过多）。")
	last_program = program.duplicate(true)
	if not _error.is_empty():
		last_result = "程序未提交：" + _error
		activity_changed.emit()
		return {"ok": false, "status": "failed", "message": last_result}
	objects = _world
	behaviors = _jobs
	checkpoints = _saves
	last_result = "场景已更新；%d 个持续行为，可随时停止。" % behaviors.size() if not behaviors.is_empty() else "场景已更新。"
	_commit()
	return {"ok": true, "status": "completed", "message": last_result, "steps": _steps, "runtime": describe("")}


func _commit() -> void:
	state.revision += 1
	_sync_visibility()
	scene_changed.emit()
	activity_changed.emit()


func _sync_visibility() -> void:
	var dirty := false
	for id: String in ["globe", "grid"]:
		dirty = dirty or state.get_layer(StringName(id)).visible != objects[id].props.visible
		state.get_layer(StringName(id)).visible = objects[id].props.visible
	state.grid_visible = objects.grid.props.visible
	if dirty:
		state.layers_changed.emit()
		state.grid_visibility_changed.emit(state.grid_visible)


func external_set(id: String, property: String, value: Variant) -> void:
	# Trusted application UI uses the same transaction path as AI edits.
	var result := execute([{"op": "set", "id": id, "property": property, "value": value}])
	if not result.ok:
		state.status_changed.emit(result.message)


func stop_all() -> bool:
	if behaviors.is_empty():
		return false
	behaviors.clear()
	state.revision += 1
	last_result = "所有持续行为已停止，保留当前场景。"
	activity_changed.emit()
	return true


func _tick(depth: int) -> bool:
	_steps += 1
	if depth > MAX_DEPTH or _steps > (256 if _frame_mode else MAX_STEPS):
		_fail("执行预算耗尽（嵌套或计算步过多）。")
	return _error.is_empty()


func _fail(message: String) -> void:
	if _error.is_empty():
		_error = message.left(250)


func _run(block: Variant, vars: Dictionary, depth: int) -> void:
	if not block is Array:
		_fail("指令块必须是数组。")
		return
	for instruction: Variant in block:
		if not _tick(depth):
			return
		if not instruction is Dictionary or not instruction.get("op") is String:
			_fail("指令必须包含 op。")
			return
		var op: String = instruction.op
		if _frame_mode and op not in ["set", "let", "repeat", "if"]:
			_fail("逐帧行为只允许 set/let/repeat/if。")
			return
		match op:
			"set":
				var id: Variant = _eval(instruction.get("id"), vars, depth + 1)
				var property: Variant = instruction.get("property")
				var value: Variant = _eval(instruction.get("value"), vars, depth + 1)
				_set_property(id, property, value)
			"let":
				var key: Variant = instruction.get("name")
				if not _identifier(key) or key in ["t", "dt"]:
					_fail("变量名无效或为保留变量 t/dt。")
					return
				vars[key] = _eval(instruction.get("value"), vars, depth + 1)
			"repeat":
				var count: Variant = _eval(instruction.get("count"), vars, depth + 1)
				var key: Variant = instruction.get("as", "i")
				if not _number(count) or count != int(count) or count < 0 or count > 64 or not _identifier(key) or key in ["t", "dt"]:
					_fail("循环次数应为0–64整数，循环变量不能为 t/dt。")
					return
				for index: int in int(count):
					var scope := vars.duplicate(true)
					scope[key] = index
					_run(instruction.get("body"), scope, depth + 1)
					if not _error.is_empty(): return
			"if":
				var condition: Variant = _eval(instruction.get("condition"), vars, depth + 1)
				if not condition is bool:
					_fail("if 条件必须为布尔值。")
					return
				_run(instruction.get("then" if condition else "else", []), vars, depth + 1)
			"create":
				var id: Variant = _eval(instruction.get("id"), vars, depth + 1)
				var kind: Variant = instruction.get("type")
				var parent: Variant = _eval(instruction.get("parent", "world"), vars, depth + 1)
				if not _identifier(id) or _world.has(id) or not kind in CREATABLE or not parent is String or not _world.has(parent) or _world[parent].type not in ["group", "sphere", "box", "cylinder"] or _world.size() >= MAX_OBJECTS:
					_fail("创建失败：ID重复/无效、类型或父对象不支持，或对象数达到64。")
					return
				_world[id] = _new_record(id, kind, parent)
				var props: Variant = instruction.get("props", {})
				if not props is Dictionary:
					_fail("props 必须是属性对象。")
					return
				for key: Variant in props:
					_set_property(id, key, _eval(props[key], vars, depth + 1))
			"delete":
				var id: Variant = _eval(instruction.get("id"), vars, depth + 1)
				if not id is String or not _world.has(id) or _world[id].builtin:
					_fail("只能删除已创建的会话对象，内置对象请隐藏。")
					return
				_delete_tree(id)
			"animate":
				var id: Variant = instruction.get("id")
				var duration: Variant = instruction.get("duration", 0)
				if not _identifier(id) or not _number(duration) or duration < 0 or duration > 3600 or not instruction.get("body") is Array or (_jobs.size() >= MAX_BEHAVIORS and not _jobs.has(id)):
					_fail("行为ID/指令块/时长无效，或超过8个行为。")
					return
				# Preflight on a temporary world: invalid behavior never starts.
				var saved_world := _world.duplicate(true)
				var saved_steps := _steps
				var frame_vars := vars.duplicate(true)
				frame_vars.merge({"t": 0.0, "dt": 0.0}, true)
				_frame_mode = true
				_active_job = id
				_writes.clear()
				_steps = 0
				_run(instruction.body, frame_vars, 0)
				_validate_geometry()
				_world = saved_world
				_steps += saved_steps
				_frame_mode = false
				_active_job = ""
				_jobs[id] = {"body": instruction.body.duplicate(true), "vars": vars.duplicate(true), "duration": duration, "elapsed": 0.0, "writes": _writes.duplicate(), "paused": false}
			"pause", "resume":
				var id: Variant = instruction.get("id")
				if id is String and id == "*":
					for job: Dictionary in _jobs.values(): job.paused = op == "pause"
				elif id is String and _jobs.has(id):
					_jobs[id].paused = op == "pause"
				else:
					_fail("找不到该行为。")
			"stop":
				var id: Variant = instruction.get("id")
				if id is String and id == "*":
					_jobs.clear()
				elif id is String and _jobs.has(id):
					_jobs.erase(id)
				else:
					_fail("找不到该行为。使用 scene_inspect 查询，或 id='*' 停止全部。")
			"checkpoint":
				var key: Variant = instruction.get("name")
				if not _identifier(key) or (_saves.size() >= 8 and not _saves.has(key)):
					_fail("检查点名称无效或超过8个检查点。")
					return
				_saves[key] = {"objects": _world.duplicate(true), "behaviors": _jobs.duplicate(true)}
			"restore":
				var key: Variant = instruction.get("name")
				if not key is String or not _saves.has(key):
					_fail("检查点不存在。")
					return
				_world = _saves[key].objects.duplicate(true)
				_jobs = _saves[key].behaviors.duplicate(true)
			_:
				_fail("未知指令：" + op)
		if not _error.is_empty(): return


func _delete_tree(id: String) -> void:
	for child: String in _world.keys():
		if _world.has(child) and _world[child].parent == id:
			_delete_tree(child)
	_world.erase(id)
	for job_id: String in _jobs.keys():
		for path: String in _jobs[job_id].writes:
			if path.begins_with(id + ":"):
				_jobs.erase(job_id)
				break


func _eval(value: Variant, vars: Dictionary, depth: int) -> Variant:
	if not _tick(depth): return null
	if value is Array:
		if value.size() > 64:
			_fail("表达式数组超过64项。")
			return null
		var result: Array = []
		for item: Variant in value:
			result.append(_eval(item, vars, depth + 1))
		return result
	if not value is Dictionary:
		if value is String and value.length() > 256: _fail("字符串过长。")
		if value is float and not is_finite(value): _fail("数值不是有限值。")
		return value
	if value.size() != 1:
		_fail("表达式必须包含且仅包含一个运算符。")
		return null
	var op: String = str(value.keys()[0])
	var raw: Variant = value.values()[0]
	if op == "var":
		if not raw is String or not vars.has(raw):
			_fail("变量不存在。")
			return null
		return vars[raw]
	var args: Variant = _eval(raw, vars, depth + 1)
	if not _error.is_empty(): return null
	if op in ["sin", "cos"]:
		if not _number(args):
			_fail("三角函数需要一个数字。")
			return null
		return sin(args) if op == "sin" else cos(args)
	if not args is Array or args.size() != 2:
		_fail("运算符需要两个参数。")
		return null
	var a: Variant = args[0]
	var b: Variant = args[1]
	if op == "get":
		if not a is String or not b is String or not _world.has(a) or not _world[a].props.has(b):
			_fail("读取的对象或属性不存在。")
			return null
		return _world[a].props[b]
	if op == "eq":
		if typeof(a) != typeof(b) and not (_number(a) and _number(b)): return false
		return a == b
	if op == "concat": return (str(a) + str(b)).left(128)
	if not _number(a) or not _number(b):
		_fail("算术/比较运算需要数字。")
		return null
	match op:
		"add": return a + b
		"sub": return a - b
		"mul": return a * b
		"div":
			if b != 0: return float(a) / b
			_fail("除数不能为零。")
		"min": return minf(a, b)
		"max": return maxf(a, b)
		"lt": return a < b
		"gt": return a > b
		_: _fail("未知表达式运算符：" + op)
	return null


func _set_property(id: Variant, property: Variant, value: Variant) -> void:
	if not _error.is_empty(): return
	if not id is String or not property is String or not _world.has(id) or not schemas[_world[id].type].has(property):
		_fail("对象或可编辑属性不存在，请先 scene_inspect。")
		return
	var rule: Dictionary = schemas[_world[id].type][property]
	match rule.type:
		"bool":
			if not value is bool: _fail("属性需要布尔值。")
		"int", "float":
			if not _number(value):
				_fail("属性需要有限数字。")
			else:
				var low := 0.0
				var high := 100.0
				if property in ["material.metallic", "material.roughness"]: high = 1.0
				if property in ["material.shading_mode", "material.transparency"]:
					high = 1.0
					if value != int(value): _fail("枚举属性需要整数。")
				if property.begins_with("mesh."): low = 0.001
				if property == "fov":
					low = 10.0
					high = 120.0
				if value < low or value > high: _fail("属性 %s 范围为 %s–%s。" % [property, low, high])
		"Vector3":
			if not value is Array or value.size() != 3:
				_fail("向量属性需要三个数字。")
			else:
				for number: Variant in value:
					if not _number(number) or absf(float(number)) > 100000:
						_fail("向量分量无效或超出范围。")
					elif property in ["scale", "mesh.size"] and (number < 0.01 or number > 100):
						_fail("尺寸和缩放范围为0.01–100。")
		"Color":
			if not value is String or not value.begins_with("#") or value.length() not in [7, 9] or not Color.html_is_valid(value):
				_fail("颜色需要 #RRGGBB 或 #RRGGBBAA。")
			else:
				value = "#" + Color(value).to_html(true)
		_: _fail("该属性类型尚未开放。")
	if _error.is_empty():
		var path: String = id + ":" + property
		for job_id: String in _jobs.keys():
			if job_id != _active_job and path in _jobs[job_id].writes:
				if _frame_mode:
					_fail("属性已由其他行为控制：" + path)
					return
				_jobs.erase(job_id)
		if path not in _writes: _writes.append(path)
		_world[id].props[property] = value


func _identifier(value: Variant) -> bool:
	return value is String and not value.strip_edges().is_empty() and value.length() <= 64 and not "\n" in value and not "/" in value and not ":" in value


func _number(value: Variant) -> bool:
	return (value is float or value is int) and is_finite(float(value))


func _validate_geometry() -> void:
	if not _error.is_empty(): return
	for record: Dictionary in _world.values():
		if record.type == "sphere" and record.props["mesh.height"] < record.props["mesh.radius"] * 2.0:
			_fail("球体高度必须至少为半径的2倍，请同时修改 mesh.height 或使用 scale。")
			return


func _process(delta: float) -> void:
	if behaviors.is_empty(): return
	_frame_mode = true
	var dirty := false
	for id: String in behaviors.keys():
		var job: Dictionary = behaviors[id]
		if job.paused: continue
		_error = ""
		_steps = 0
		_active_job = id
		_jobs = behaviors
		_writes.clear()
		_world = objects.duplicate(true)
		var elapsed: float = job.elapsed + minf(delta, 0.1)
		if job.duration > 0: elapsed = minf(elapsed, job.duration)
		var vars: Dictionary = job.vars.duplicate(true)
		vars["t"] = elapsed
		vars["dt"] = elapsed - job.elapsed
		_run(job.body, vars, 0)
		_validate_geometry()
		if not _error.is_empty():
			behaviors.erase(id)
			last_result = "行为 %s 已停止：%s" % [id, _error]
			behavior_finished.emit(id, "failed", last_result)
			activity_changed.emit()
			continue
		objects = _world
		job.writes = _writes.duplicate()
		job.elapsed = elapsed
		dirty = true
		if job.duration > 0 and elapsed >= job.duration:
			behaviors.erase(id)
			behavior_finished.emit(id, "completed", "行为 %s 已完成。" % id)
			activity_changed.emit()
	_frame_mode = false
	_active_job = ""
	# Behavior frames do not invalidate an in-flight model request or its undo entry.
	if dirty:
		_sync_visibility()
		scene_changed.emit()
