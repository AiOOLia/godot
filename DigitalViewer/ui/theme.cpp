/**************************************************************************/
/*  theme.cpp                                                             */
/**************************************************************************/
/*                         This is part of DigitalViewer fork app code.    */
/**************************************************************************/

#include "DigitalViewer/ui/theme.h"

#include "DigitalViewer/builtin_fonts.gen.h"

#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/image_loader.h"
#include "core/math/math_defs.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/resources/font.h"
#include "scene/scene_string_names.h"
#include "servers/display/display_server.h"

namespace dw {

Ref<StyleBoxFlat> make_flat_stylebox(const Color &p_color) {
	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(p_color);
	style->set_border_width_all(0);
	return style;
}

Ref<StyleBoxFlat> make_bordered_stylebox(const Color &p_bg, const Color &p_border, int p_border_width) {
	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(p_bg);
	style->set_border_color(p_border);
	style->set_border_width_all(p_border_width);
	return style;
}

static Ref<ImageTexture> load_svg_icon(const String &p_path) {
	Ref<Image> img;
	img.instantiate();
	if (!FileAccess::exists(p_path)) {
		ERR_PRINT("Missing DigitalViewer icon: " + p_path);
		return Ref<ImageTexture>();
	}
	if (ImageLoader::load_image(p_path, img) != OK) {
		ERR_PRINT("Failed to load DigitalViewer icon: " + p_path);
		return Ref<ImageTexture>();
	}
	return ImageTexture::create_from_image(img);
}

Ref<ImageTexture> make_split_grabber_icon() {
	Ref<ImageTexture> icon = load_svg_icon("res://icons/GuiHsplitter.svg");
	if (icon.is_valid()) {
		return icon;
	}

	Ref<Image> img;
	img.instantiate();
	img->initialize_data(2, 64, false, Image::FORMAT_RGBA8);
	img->fill(Color(0, 0, 0, 0));
	for (int y = 2; y < 62; y++) {
		img->set_pixel(0, y, Color(1, 1, 1, 0.4f));
		img->set_pixel(1, y, Color(1, 1, 1, 0.4f));
	}
	return ImageTexture::create_from_image(img);
}

Ref<ImageTexture> make_window_minimize_icon() {
	return load_svg_icon("res://icons/WindowMinimize.svg");
}

Ref<ImageTexture> make_window_maximize_icon() {
	return load_svg_icon("res://icons/WindowMaximize.svg");
}

Ref<ImageTexture> make_window_restore_icon() {
	return load_svg_icon("res://icons/WindowRestore.svg");
}

Ref<ImageTexture> make_window_close_icon() {
	return load_svg_icon("res://icons/WindowClose.svg");
}

static Ref<StyleBoxFlat> make_title_window_button_style(const Color &p_color) {
	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(p_color);
	style->set_border_width_all(0);
	style->set_corner_radius_all(0);
	return style;
}

void configure_title_window_button(Button *p_button) {
	ERR_FAIL_NULL(p_button);
	p_button->set_flat(false);
	p_button->set_expand_icon(true);
	p_button->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	p_button->set_custom_minimum_size(Size2(46, 32));
	p_button->add_theme_style_override(CoreStringName(normal), make_title_window_button_style(Color(0, 0, 0, 0)));
	p_button->add_theme_style_override(SceneStringName(hover), make_title_window_button_style(Color(1, 1, 1, 0.12f)));
	p_button->add_theme_style_override(SceneStringName(pressed), make_title_window_button_style(Color(1, 1, 1, 0.20f)));
	p_button->add_theme_style_override("hover_pressed", make_title_window_button_style(Color(1, 1, 1, 0.20f)));
	p_button->add_theme_style_override("disabled", make_title_window_button_style(Color(0, 0, 0, 0)));
}

static Ref<FontFile> load_internal_font(const uint8_t *p_data, size_t p_size, TypedArray<Font> *r_fallbacks = nullptr) {
	Ref<FontFile> font;
	font.instantiate();
	font->set_data_ptr(p_data, p_size);
	font->set_antialiasing(TextServer::FONT_ANTIALIASING_GRAY);
	font->set_hinting(TextServer::HINTING_LIGHT);
	font->set_force_autohinter(true);
	font->set_subpixel_positioning(TextServer::SUBPIXEL_POSITIONING_AUTO);
	font->set_disable_embedded_bitmaps(true);
	if (r_fallbacks) {
		r_fallbacks->push_back(font);
	}
	return font;
}

static float get_editor_auto_display_scale() {
	DisplayServer *ds = DisplayServer::get_singleton();
	ERR_FAIL_NULL_V(ds, 1.0f);

#if defined(MACOS_ENABLED) || defined(ANDROID_ENABLED)
	return ds->screen_get_max_scale();
#else
	const int screen = ds->window_get_current_screen();
	if (ds->screen_get_size(screen) == Vector2i()) {
		return 1.0f;
	}

#if defined(WINDOWS_ENABLED)
	return ds->screen_get_dpi(screen) / 96.0f;
#else
	const int smallest_dimension = MIN(ds->screen_get_size(screen).x, ds->screen_get_size(screen).y);
	if (ds->screen_get_dpi(screen) >= 192 && smallest_dimension >= 1400) {
		return 2.0f;
	} else if (smallest_dimension >= 1700) {
		return 1.5f;
	} else if (smallest_dimension <= 800) {
		return 0.75f;
	}
	return 1.0f;
#endif
#endif
}

Ref<Theme> make_editor_like_theme() {
	TypedArray<Font> fallbacks;
	load_internal_font(_font_DroidSansFallback, _font_DroidSansFallback_size, &fallbacks);
	Ref<FontFile> main_font = load_internal_font(_font_Inter_Regular, _font_Inter_Regular_size);
	main_font->set_fallbacks(fallbacks);

	Ref<Theme> theme;
	theme.instantiate();
	theme->set_default_font(main_font);
	const int default_font_size = MAX(1, (int)(14 * get_editor_auto_display_scale()));
	theme->set_default_font_size(default_font_size);
	return theme;
}

void label_on_dark(Label *p_label) {
	ERR_FAIL_NULL(p_label);
	p_label->add_theme_color_override(SceneStringName(font_color), Color(0.94f, 0.94f, 0.94f, 1.0f));
}

void label_on_light(Label *p_label, const Color &p_fg) {
	ERR_FAIL_NULL(p_label);
	p_label->add_theme_color_override(SceneStringName(font_color), p_fg);
}

} // namespace dw
