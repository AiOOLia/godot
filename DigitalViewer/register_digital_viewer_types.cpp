/**************************************************************************/
/*  register_digital_viewer_types.cpp                                     */
/**************************************************************************/
/*                         This is part of DigitalViewer fork app code.    */
/**************************************************************************/

#include "register_digital_viewer_types.h"

#include "digital_viewer_root.h"

#include "core/object/class_db.h"

void register_digital_viewer_types() {
	GDREGISTER_CLASS(DigitalViewerRoot);
}

void unregister_digital_viewer_types() {
}
