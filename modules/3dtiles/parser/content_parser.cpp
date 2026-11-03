/**************************************************************************/
/*  content_parser.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/
#include "modules/3dtiles/parser/tiles_3d_decoder.h"

namespace Tiles3DParserInternal {

bool Decoder::content(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth) {
	if (p_bytes.is_empty() || p_depth > 32) {
		return fail("Empty content or excessively nested composite tile.");
	}
	const uint8_t *bytes = p_bytes.ptr();
	const int size = p_bytes.size();
	if (size >= 4 && memcmp(bytes, "cmpt", 4) == 0) {
		return decode_cmpt(p_bytes, p_rtc, p_depth);
	}
	if (size >= 4 && memcmp(bytes, "b3dm", 4) == 0) {
		return decode_b3dm(p_bytes, p_rtc, p_depth);
	}
	if (size >= 4 && memcmp(bytes, "glTF", 4) == 0) {
		return decode_glb(p_bytes, p_rtc);
	}
	if (size >= 4 && memcmp(bytes, "i3dm", 4) == 0) {
		return decode_i3dm(p_bytes, p_rtc, p_depth);
	}
	if (size >= 4 && memcmp(bytes, "pnts", 4) == 0) {
		return decode_pnts(p_bytes, p_rtc, p_depth);
	}
	if (size >= 4 && memcmp(bytes, "subt", 4) == 0) {
		return fail("Implicit tiling subtree content is not supported.", ERR_UNAVAILABLE);
	}
	Dictionary manifest;
	if (!json(p_bytes, manifest)) {
		return false;
	}
	return manifest.has("root") ? tileset(manifest) : gltf(manifest, PackedByteArray(), p_rtc);
}

} // namespace Tiles3DParserInternal
