/**************************************************************************/
/*  register_digital_viewer_types.cpp                                     */
/**************************************************************************/
/*                         This is part of DigitalViewer fork app code.    */
/**************************************************************************/

#include "register_digital_viewer_types.h"

#include "DigitalViewer/ui/main_window.h"

#include "core/object/class_db.h"

namespace dw {

void register_digital_viewer_types() {
	GDREGISTER_CLASS(MainWindow);
}

void unregister_digital_viewer_types() {
}

} // namespace dw
