extends RefCounted
## The exporter locks the selected profile into the staged project settings.
## Editor runs may preview a profile with -- --dv-profile=CAD.
static var _data: Dictionary = _read()

static func _read() -> Dictionary:
	var id := str(ProjectSettings.get_setting("digital_viewer/profile", "ALL"))
	if OS.has_feature("editor") and not ProjectSettings.get_setting("digital_viewer/profile_locked", false):
		for argument in OS.get_cmdline_user_args():
			if argument.begins_with("--dv-profile="):
				id = argument.trim_prefix("--dv-profile=").to_upper()
	if id not in ["CAD", "ALL"]:
		push_error("Unknown DigitalViewer profile: " + id)
		return {}
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string("res://profiles/%s.json" % id))
	if not parsed is Dictionary or parsed.get("id") != id:
		push_error("Invalid DigitalViewer profile: " + id)
		return {}
	return parsed

static func id() -> String:
	return _data.get("id", "INVALID")

static func value(key: String, fallback: String = "") -> String:
	return str(_data.get(key, fallback))

static func enabled(feature: String) -> bool:
	return _data.get("features", {}).get(feature, false)

static func color(key: String) -> Color:
	return Color(_data.get("palette", {}).get(key, "#ff00ff"))

static func features() -> Dictionary:
	return _data.get("features", {}).duplicate()

static func template_error() -> String:
	if enabled("brep") and not ClassDB.class_exists("BRepDocument"):
		return "BREP extension missing or incompatible: check addons/brep/bin runtime DLLs."
	if enabled("brep") and not OS.has_feature("editor"):
		var legacy := OS.get_executable_path().get_base_dir().path_join("addons/brep/bin/DigitalViewerBRep.dll")
		if FileAccess.file_exists(legacy):
			return "Obsolete BREP runtime: %s. Extract the complete package into a clean directory; mixing old and new DLLs is unsupported." % legacy
	var required := {"GLTFDocument": true, "OsgTiledMeshOptions": enabled("osg"), "Tiles3DOptions": enabled("3dtiles"), "TiledMeshInstance3D": enabled("osg") or enabled("3dtiles")}
	for type in required:
		if ClassDB.class_exists(type) != required[type]:
			return "Template does not match %s: %s expected=%s" % [id(), type, required[type]]
	return ""

static func extension(source: String) -> String:
	return source.get_slice("?", 0).get_slice("#", 0).get_extension().to_lower()

static func model_entry(source: String) -> bool:
	var ext := extension(source)
	if ext == "json":
		var detected := preload("res://scripts/desktop/source_resolver.gd").detect(source)
		return not detected.has("error") and enabled("brep" if detected.kind == "brep" else "3dtiles")
	return ext in ["gltf", "glb"] or (enabled("brep") and ext == "brep") or (enabled("osg") and ext in ["osg", "osgb"])

static func file_filters() -> PackedStringArray:
	var extensions := "*.glb,*.gltf,*.zip,*.gd"
	if enabled("osg"): extensions += ",*.osg,*.osgb"
	if enabled("brep"): extensions += ",*.brep"
	if enabled("brep") or enabled("3dtiles"): extensions += ",*.json"
	if enabled("3dtiles"): extensions += ",*.b3dm,*.i3dm,*.pnts,*.cmpt"
	return PackedStringArray([extensions + ";Supported data"])

static func source_error(source: String, kind: String, options: Dictionary) -> String:
	var feature := ""
	var ext := extension(source)
	if ext in ["osg", "osgb"]: feature = "osg"
	if ext in ["b3dm", "i3dm", "pnts", "cmpt"] or (ext == "json" and kind == "tiled") or options.has("ion_asset_id"): feature = "3dtiles"
	if ext == "brep" or kind == "brep": feature = "brep"
	if kind in ["ellipsoid", "imagery", "terrain", "dem", "gis"] or ext in ["tif", "tiff"]: feature = "gis"
	if not feature.is_empty() and not enabled(feature):
		return "%s is disabled in the %s build profile." % [feature, id()]
	if kind == "tiled" and not enabled("osg") and not enabled("3dtiles"):
		return "Tiled datasets are disabled in the %s build profile." % id()
	return ""
