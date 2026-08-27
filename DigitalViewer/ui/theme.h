/**************************************************************************/
/*  theme.h                                                               */
/**************************************************************************/
/*                         This is part of DigitalViewer fork app code.    */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/theme.h"

class Button;
class Label;

namespace dw {

Ref<Theme> make_editor_like_theme();
Ref<StyleBoxFlat> make_flat_stylebox(const Color &p_color);
Ref<StyleBoxFlat> make_bordered_stylebox(const Color &p_bg, const Color &p_border, int p_border_width = 2);
Ref<ImageTexture> make_split_grabber_icon();
Ref<ImageTexture> make_window_minimize_icon();
Ref<ImageTexture> make_window_maximize_icon();
Ref<ImageTexture> make_window_restore_icon();
Ref<ImageTexture> make_window_close_icon();
void configure_title_window_button(Button *p_button);
void label_on_dark(Label *p_label);
void label_on_light(Label *p_label, const Color &p_fg);

} // namespace dw
