extends SceneTree

func _initialize() -> void:
	var arguments := {}
	for argument in OS.get_cmdline_user_args():
		var separator := argument.find("=")
		if separator > 0: arguments[argument.left(separator)] = argument.substr(separator + 1)
	var source: String = str(arguments.get("--dv-source", "")).replace("\\", "/")
	var stage := ProjectSettings.globalize_path("res://").replace("\\", "/")
	if source.is_empty() or not stage.begins_with(source.trim_suffix("/") + "/build/staging/"):
		_fail("Export configuration may only modify a staged project under build/staging/.")
		return
	var id: String = arguments.get("--dv-profile", "ALL")
	if id not in ["CAD", "ALL"]:
		_fail("Unknown build profile: " + id)
		return
	var profile: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://profiles/%s.json" % id))
	ProjectSettings.set_setting("digital_viewer/profile", id)
	ProjectSettings.set_setting("digital_viewer/profile_locked", true)
	ProjectSettings.set_setting("application/config/name", profile.product_name)
	ProjectSettings.set_setting("application/config/description", profile.description)
	ProjectSettings.set_setting("application/config/icon", profile.icon)
	ProjectSettings.set_setting("application/config/features", PackedStringArray(["4.7", "dv_" + id.to_lower()]))
	ProjectSettings.set_setting("rendering/environment/defaults/default_clear_color", Color(profile.palette.BG))
	# Generate a PNG from the same SVG for the EXE icon and the boot splash.
	var icon := Image.new()
	if icon.load_svg_from_string(FileAccess.get_file_as_string(profile.icon), 2.0) != OK or icon.save_png("res://assets/profile_icon.png") != OK:
		_fail("Cannot generate profile icon.")
		return
	ProjectSettings.set_setting("application/boot_splash/image", "res://assets/profile_icon.png")
	ProjectSettings.set_setting("application/boot_splash/bg_color", Color(profile.palette.BG))
	if ProjectSettings.save() != OK:
		_fail("Cannot save staged project settings.")
		return
	var scene := FileAccess.get_file_as_string("res://scenes/main.tscn")
	var scene_file := FileAccess.open("res://scenes/main.tscn", FileAccess.WRITE)
	scene_file.store_string(scene.replace("res://themes/digitalviewer_theme.tres", profile.theme))
	scene_file.close()
	var presets := ConfigFile.new()
	if presets.load("res://export_presets.cfg") != OK:
		_fail("Cannot read export presets.")
		return
	var preset_found := false
	for section in presets.get_sections():
		if not section.begins_with("preset.") or section.ends_with(".options"): continue
		if presets.get_value(section, "name", "") != arguments.get("--dv-preset", "Windows Desktop"): continue
		preset_found = true
		var options := section + ".options"
		presets.set_value(section, "custom_features", "dv_" + id.to_lower())
		var includes: String = presets.get_value(section, "include_filter", "")
		if arguments.get("--dv-preset") == "Windows Validation": includes += ",tests/brep_smoke.gd"
		presets.set_value(section, "include_filter", includes + ",scripts/desktop/*.gd,profiles/%s.json,assets/*.svg,assets/profile_icon.png,themes/*.tres,addons/brep/*.gdextension" % id)
		var excludes: String = presets.get_value(section, "exclude_filter", "")
		presets.set_value(section, "exclude_filter", excludes + ",tools/*,build/*,profiles/%s.json" % ("ALL" if id == "CAD" else "CAD"))
		# Resolve source-relative template paths against the original project.
		for key in ["custom_template/debug", "custom_template/release"]:
			var path: String = presets.get_value(options, key, "")
			if not path.is_empty() and not path.is_absolute_path():
				presets.set_value(options, key, str(arguments["--dv-source"]).path_join(path).simplify_path())
		if arguments.has("--dv-template"):
			presets.set_value(options, "custom_template/release", arguments["--dv-template"])
		var platform: String = presets.get_value(section, "platform", "")
		if platform == "Windows Desktop":
			presets.set_value(options, "application/icon", "res://assets/profile_icon.png")
			presets.set_value(options, "application/product_name", profile.product_name)
			presets.set_value(options, "application/file_description", profile.description)
		elif platform == "macOS":
			presets.set_value(options, "application/icon", "res://assets/profile_icon.png")
			presets.set_value(options, "application/bundle_identifier", "org.digitalviewer." + id.to_lower())
	if not preset_found or presets.save("res://export_presets.cfg") != OK:
		_fail("Cannot configure selected export preset.")
		return
	print("DIGITALVIEWER_PROFILE_CONFIGURED " + id)
	quit()

func _fail(message: String) -> void:
	push_error(message)
	quit(1)
