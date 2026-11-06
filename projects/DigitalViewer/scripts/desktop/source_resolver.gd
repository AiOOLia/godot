extends RefCounted
## Format detection only. BRepCore.dll parses BREP geometry and assemblies.
const MAX_JSON_BYTES := 16 * 1024 * 1024

static func extension(source: String) -> String:
	return source.get_slice("?", 0).get_slice("#", 0).get_extension().to_lower()

static func kind(source: String) -> String:
	match extension(source):
		"gltf", "glb": return "gltf"
		"brep": return "brep"
		"json": return "json"
		"osg", "osgb", "b3dm", "i3dm", "pnts", "cmpt": return "tiled"
		"gd": return "script"
		"zip": return "zip"
	return "unsupported"

static func json_text(text: String) -> Dictionary:
	if text.to_utf8_buffer().size() > MAX_JSON_BYTES:
		return {"error": "JSON exceeds the 16 MiB limit."}
	var parser := JSON.new()
	if parser.parse(text.trim_prefix("\ufeff")) != OK:
		return {"error": "Invalid JSON at line %d: %s" % [parser.get_error_line(), parser.get_error_message()]}
	var value: Variant = parser.data
	if not value is Dictionary: return {"error": "Model JSON must contain an object."}
	var root: Variant = value.get("root")
	var brep: Variant = value.get("brep")
	var asset: Variant = value.get("asset")
	var has_brep: bool = value.has("brep")
	var has_tiles: bool = asset is Dictionary and asset.get("version") is String
	var geometry_keys := ["vertices", "edges", "wires", "faces", "shells", "solids"]
	var has_geometry := false
	for key in geometry_keys:
		if value.has(key): has_geometry = true
	if int(has_brep) + int(has_tiles) + int(has_geometry) > 1:
		return {"error": "Ambiguous JSON: conflicting BREP geometry, assembly or 3D Tiles markers."}
	if has_geometry:
		for key in geometry_keys:
			if not value.get(key) is Array:
				return {"error": "BREP JSON requires a %s array." % key}
		if not value.get("version") is Array or value.version.size() != 4 or not (value.get("unitToMillimeters") is float or value.get("unitToMillimeters") is int):
			return {"error": "BREP JSON requires version[4] and unitToMillimeters."}
		return {"kind": "brep", "brep_json": true}
	if has_brep:
		if not brep is Dictionary or not brep.get("file") is String or str(brep.file).is_empty() or not root is Dictionary:
			return {"error": "BREP assembly JSON requires brep.file and a root object."}
		return {"kind": "brep", "dependency": str(brep.file)}
	if has_tiles and root is Dictionary and root.get("boundingVolume") is Dictionary:
		return {"kind": "tiled"}
	return {"error": "Unknown JSON model format. Expected BREP geometry, BREP assembly or 3D Tiles."}

static func detect(source: String) -> Dictionary:
	var format := kind(source)
	if format != "json": return {"kind": format}
	var file := FileAccess.open(source, FileAccess.READ)
	if file == null: return {"error": "Cannot open JSON model: " + source}
	if file.get_length() > MAX_JSON_BYTES: return {"error": "JSON exceeds the 16 MiB limit."}
	return json_text(file.get_as_text())

static func dependency(source: String, reference: String) -> Dictionary:
	var relative := reference.replace("\\", "/")
	if relative.is_absolute_path() or ":" in relative or ".." in relative.split("/") or relative.begins_with("/"):
		return {"error": "BREP dependency must be a relative path inside the model directory."}
	var base := ProjectSettings.globalize_path(source).get_base_dir().replace("\\", "/").simplify_path()
	var path := base.path_join(relative).simplify_path()
	if not path.begins_with(base + "/"): return {"error": "BREP dependency escapes the model directory."}
	if not FileAccess.file_exists(path): return {"error": "Missing BREP dependency: " + reference}
	return {"path": path}

static func candidates(paths: PackedStringArray, read_text: Callable, features: Dictionary) -> Array[String]:
	var result: Array[String] = []
	var dependencies: Array[String] = []
	for path in paths:
		var format := kind(path)
		if format == "json":
			var detected := json_text(read_text.call(path))
			if detected.has("error"): continue
			format = detected.kind
			if format == "brep" and features.get("brep", false) and detected.has("dependency"):
				dependencies.append(path.replace("\\", "/").get_base_dir().path_join(detected.dependency.replace("\\", "/")).simplify_path())
		if format == "gltf" or (format == "brep" and features.get("brep", false)) or (format == "tiled" and features.get("osg" if extension(path) in ["osg", "osgb"] else "3dtiles", false)):
			result.append(path)
	for path in result.duplicate():
		if path.replace("\\", "/").simplify_path() in dependencies: result.erase(path)
	return result

static func zip_candidates(zip: ZIPReader, features: Dictionary) -> Array[String]:
	return candidates(zip.get_files(), func(path: String) -> String: return zip.read_file(path).get_string_from_utf8(), features)
