/**************************************************************************/
/*  dv_win_decorations.cpp                                               */
/**************************************************************************/
/*  DigitalViewer-only: DWM rounded corners on the main HWND.                */
/*                                                                           */
/*  Borderless edge resize is provided by WS_THICKFRAME in DisplayServerWindows. */
/**************************************************************************/

#include "DigitalViewer/platform/windows/dv_win_decorations.h"

#include "servers/display/display_server.h"
#include "servers/display/display_server_enums.h"

#include "core/typedefs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "Dwmapi.lib")

#include "core/error/error_macros.h"

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

void dv_windows_apply_main_window_decorations() {
	DisplayServer *ds = DisplayServer::get_singleton();
	ERR_FAIL_NULL(ds);

	const int64_t hwnd_val = ds->window_get_native_handle(DisplayServerEnums::WINDOW_HANDLE, DisplayServerEnums::MAIN_WINDOW_ID);
	HWND hwnd = (HWND)(uintptr_t)hwnd_val;
	ERR_FAIL_NULL(hwnd);

	if (!ds->window_get_flag(DisplayServerEnums::WINDOW_FLAG_SHARP_CORNERS, DisplayServerEnums::MAIN_WINDOW_ID)) {
		DWORD pref = DWMWCP_ROUND;
		DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
	}
}
