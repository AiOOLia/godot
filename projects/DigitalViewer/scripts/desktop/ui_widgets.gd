extends RefCounted

const Profile = preload("res://scripts/desktop/build_profile.gd")
static var BG := Profile.color("BG")
static var SURFACE := Profile.color("SURFACE")
static var RAISED := Profile.color("RAISED")
static var HOVER := Profile.color("HOVER")
static var ACTIVE := Profile.color("ACTIVE")
static var LINE := Profile.color("LINE")
static var LINE_STRONG := Profile.color("LINE_STRONG")
static var TEXT := Profile.color("TEXT")
static var TEXT_2 := Profile.color("TEXT_2")
static var TEXT_3 := Profile.color("TEXT_3")
static var ACCENT := Profile.color("ACCENT")
static var ACCENT_SOFT := Color(ACCENT, 0.14)
static var ON_ACCENT := Profile.color("ON_ACCENT")
static var ACCENT_HOVER := Profile.color("ACCENT_HOVER")
static var ACCENT_PRESSED := Profile.color("ACCENT_PRESSED")
static var ACTIVE_BG := Profile.color("ACTIVE_BG")

static func box(bg: Color, border := Color.TRANSPARENT, radius := 7, border_width := 0, margins := Vector4(8, 5, 8, 5)) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = bg
	style.border_color = border
	style.set_border_width_all(border_width)
	style.set_corner_radius_all(radius)
	style.content_margin_left = margins.x
	style.content_margin_top = margins.y
	style.content_margin_right = margins.z
	style.content_margin_bottom = margins.w
	return style

static func svg_texture(path: String) -> Texture2D:
	var svg := FileAccess.get_file_as_string(path)
	if svg.is_empty():
		return null
	var image := Image.new()
	if image.load_svg_from_string(svg, 1.0) != OK:
		return null
	return ImageTexture.create_from_image(image)

static func icon(name: String) -> Texture2D:
	var path := "res://assets/icons/%s.svg" % name
	var texture := svg_texture(path)
	if texture != null:
		return texture
	if ResourceLoader.exists(path):
		return load(path) as Texture2D
	return null

static func style_button(button: Button, active := false, compact := false) -> void:
	var margin := Vector4(3, 2, 3, 2) if compact else Vector4(8, 5, 8, 5)
	button.add_theme_stylebox_override("normal", box(ACTIVE_BG if active else Color.TRANSPARENT, Color.TRANSPARENT, 7, 0, margin))
	button.add_theme_stylebox_override("hover", box(HOVER, Color.TRANSPARENT, 7, 0, margin))
	button.add_theme_stylebox_override("pressed", box(ACTIVE, Color.TRANSPARENT, 7, 0, margin))
	button.add_theme_stylebox_override("focus", StyleBoxEmpty.new())
	button.add_theme_stylebox_override("disabled", box(Color.TRANSPARENT, Color.TRANSPARENT, 7, 0, margin))
	button.add_theme_color_override("font_color", ACCENT if active else TEXT_2)
	button.add_theme_color_override("font_hover_color", ACCENT if active else TEXT)
	button.add_theme_color_override("font_pressed_color", ACCENT if active else TEXT)
	button.add_theme_color_override("icon_normal_color", ACCENT if active else TEXT_2)
	button.add_theme_color_override("icon_hover_color", ACCENT if active else TEXT)
	button.add_theme_color_override("icon_pressed_color", ACCENT if active else TEXT)
	button.add_theme_font_size_override("font_size", 12)
	button.focus_mode = Control.FOCUS_NONE
	button.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND

static func icon_scale() -> float:
	if DisplayServer.get_name() == "headless":
		return 1.0
	return clampf(DisplayServer.screen_get_dpi() / 96.0, 1.0, 1.35)

static func set_icon_size(button: Button, icon_size: int) -> void:
	button.add_theme_constant_override("icon_max_width", roundi(icon_size * icon_scale()))

static func icon_button(parent: Node, icon_name: String, hint: String, callback: Callable, active := false, size := 30, icon_size := 16) -> Button:
	var button := Button.new()
	button.text = ""
	button.icon = icon(icon_name)
	set_icon_size(button, icon_size)
	button.expand_icon = true
	button.tooltip_text = hint
	button.custom_minimum_size = Vector2(size, size)
	button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	style_button(button, active, true)
	button.pressed.connect(callback)
	parent.add_child(button)
	return button

static func text_icon_button(parent: Node, icon_name: String, text: String, callback: Callable, active := false) -> Button:
	var button := Button.new()
	button.text = text
	button.icon = icon(icon_name)
	set_icon_size(button, 16)
	button.expand_icon = false
	button.icon_alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.custom_minimum_size.y = 34
	button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	button.add_theme_constant_override("h_separation", 7)
	style_button(button, active)
	button.pressed.connect(callback)
	parent.add_child(button)
	return button
