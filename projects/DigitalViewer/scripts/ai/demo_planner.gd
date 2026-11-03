extends RefCounted
## Explicit deterministic demo, not an LLM or a fallback for model failures.

func plan(text: String, scene: Dictionary) -> Dictionary:
	var examples = preload("res://scripts/runtime/scene_examples.gd")
	if text.strip_edges() in ["地球变红并自转", "把地球变红并缓慢自转", "把地球改成红色并自转"]:
		return {"calls": [_call("scene_run", {"program_json": JSON.stringify(examples.red_spin())})]}
	if text.strip_edges() in ["创建彩色圆环", "创建旋转圆环"]:
		return {"calls": [_call("scene_run", {"program_json": JSON.stringify(examples.orbit_ring())})]}
	if text.strip_edges() in ["停止自转", "停止所有行为"]:
		return {"calls": [_call("scene_run", {"program_json": JSON.stringify([{"op": "stop", "id": "*"}])})]}
	if text.strip_edges() in ["暂停自转", "继续自转"]:
		return {"calls": [_call("scene_run", {"program_json": JSON.stringify([{"op": "pause" if text.strip_edges() == "暂停自转" else "resume", "id": "*"}])})]}
	if text.strip_edges() in ["恢复场景", "停止并恢复原样"]:
		return {"calls": [_call("scene_run", {"program_json": JSON.stringify(examples.restore_original())})]}
	if text.strip_edges() in ["把地球改成红色", "把当前地球改成红色"]:
		return {"calls": [_call("scene_run", {"program_json": JSON.stringify([{"op": "set", "id": "globe", "property": "material.albedo_color", "value": "#ff3030"}])})]}
	var calls: Array = []
	var reference: String = scene.last_referenced_layer
	for part: String in text.replace("；", ";").split(";"):
		var phrase := part.strip_edges()
		if phrase.is_empty():
			continue
		if phrase in ["当前场景", "现在有哪些图层", "现在有哪些图层？", "查询场景"]:
			calls.append(_call("get_scene_state", {}))
		elif phrase in ["撤销", "撤销上一步"]:
			calls.append(_call("undo_last_action", {}))
		elif phrase in ["停止飞行", "取消飞行"]:
			calls.append(_call("cancel_flight", {}))
		elif phrase.begins_with("保存视点 ") or phrase.begins_with("恢复视点 "):
			calls.append(_call("save_viewpoint" if phrase.begins_with("保存") else "restore_viewpoint", {"name": phrase.substr(5).strip_edges()}))
		elif phrase.begins_with("飞往 "):
			var regex := RegEx.new()
			regex.compile("^-?[0-9]+(?:\\.[0-9]+)?$")
			var values := phrase.substr(3).replace("，", " ").replace(",", " ").split(" ", false)
			if values.size() != 3:
				return {"answer": "演示格式：飞往 经度 纬度 高度公里，例如：飞往 121.47 31.23 8"}
			for value: String in values:
				if regex.search(value) == null:
					return {"answer": "请填写三个有效数字：经度、纬度、高度（公里）。"}
			calls.append(_call("fly_to", {"longitude": values[0].to_float(), "latitude": values[1].to_float(), "height_km": values[2].to_float()}))
		elif phrase.begins_with("显示") or phrase.begins_with("隐藏") or phrase.begins_with("选中"):
			var target := phrase.substr(2).strip_edges()
			var layer_id := ""
			if target in ["它", "这个", "当前图层"]:
				layer_id = reference if not reference.is_empty() else str(scene.selected_layer)
			elif target in ["经纬网", "经纬网格"]:
				layer_id = "grid"
			elif target in ["地球", "WGS84 地球"]:
				layer_id = "globe"
			else:
				var matches: Array = []
				for layer: Dictionary in scene.layers:
					if target in [layer.id, layer.name]:
						matches.append(layer.id)
				if matches.size() == 1:
					layer_id = matches[0]
			if layer_id.is_empty():
				return {"answer": "请明确图层名称。演示模式可以操作「地球」和「经纬网」。"}
			reference = layer_id
			if phrase.begins_with("选中"):
				calls.append(_call("select_layer", {"layer_id": layer_id}))
			else:
				calls.append(_call("set_layer_visible", {"layer_id": layer_id, "visible": phrase.begins_with("显示")}))
		else:
			return {"answer": "当前是规则演示，并未连接大模型。试试「隐藏经纬网」「显示它」「保存视点 总览」「飞往 0 0 20」「恢复视点 总览」。多步用分号分隔；自由对话请在连接页配置真实模型。"}
	if calls.size() > 8:
		return {"answer": "每次最多执行 8 个操作，请拆分消息。"}
	return {"calls": calls}


func _call(tool_name: String, args: Dictionary) -> Dictionary:
	return {"name": tool_name, "args": args}
