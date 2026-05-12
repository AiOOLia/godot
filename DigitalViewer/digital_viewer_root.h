/**************************************************************************/
/*  digital_viewer_root.h                                                 */
/**************************************************************************/
/*                         This is part of DigitalViewer fork app code.    */
/**************************************************************************/

#pragma once

#include "scene/gui/control.h"

// Full-window UI shell (VS Code–like layout): activity strip, side panel, editor area, status bar.
class DigitalViewerRoot : public Control {
	GDCLASS(DigitalViewerRoot, Control);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	DigitalViewerRoot();
};
