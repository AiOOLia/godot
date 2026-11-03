extends RefCounted


static func red_spin() -> Array:
	return [
		{"op": "checkpoint", "name": "原样"},
		{"op": "set", "id": "globe", "property": "material.albedo_color", "value": "#ef3545"},
		{"op": "set", "id": "grid", "property": "material.albedo_color", "value": "#ffffffa0"},
		{"op": "animate", "id": "地球自转", "duration": 0, "body": [
			{"op": "set", "id": "earth", "property": "rotation_degrees", "value": [0, {"mul": [{"var": "t"}, 12]}, 0]}
		]},
	]


static func orbit_ring() -> Array:
	return [
		{"op": "checkpoint", "name": "圆环前"},
		{"op": "create", "id": "ring", "type": "group", "parent": "world"},
		{"op": "repeat", "count": 12, "as": "i", "body": [
			{"op": "let", "name": "angle", "value": {"mul": [{"var": "i"}, 0.5235987756]}},
			{"op": "let", "name": "ball", "value": {"concat": ["ball_", {"var": "i"}]}},
			{"op": "create", "id": {"var": "ball"}, "type": "sphere", "parent": "ring", "props": {
				"position": [{"mul": [{"cos": {"var": "angle"}}, 2.8]}, {"mul": [{"sin": {"var": "angle"}}, 2.8]}, 0],
				"scale": [0.25, 0.25, 0.25], "material.albedo_color": "#ffbb44", "material.shading_mode": 0,
			}},
			{"op": "if", "condition": {"lt": [{"var": "i"}, 6]}, "then": [
				{"op": "set", "id": {"var": "ball"}, "property": "material.albedo_color", "value": "#55ccff"},
			]},
		]},
		{"op": "animate", "id": "圆环旋转", "duration": 0, "body": [
			{"op": "set", "id": "ring", "property": "rotation_degrees", "value": [0, 0, {"mul": [{"var": "t"}, 15]}]},
		]},
	]


static func restore_original() -> Array:
	return [{"op": "restore", "name": "原样"}]
