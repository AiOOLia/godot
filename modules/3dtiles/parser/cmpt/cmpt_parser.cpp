/**************************************************************************/
/*  cmpt_parser.cpp                                                       */
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

bool Decoder::decode_cmpt(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth) {
	const uint8_t *bytes = p_bytes.ptr();
	const int size = p_bytes.size();
	if (size < 16 || decode_uint32(bytes + 4) != 1 || decode_uint32(bytes + 8) != uint32_t(size)) {
		return fail("Invalid composite tile header.");
	}
	const uint32_t count = decode_uint32(bytes + 12);
	uint64_t offset = 16;
	if (count == 0 || count > uint32_t(size / 12)) {
		return fail("Invalid composite tile count.");
	}
	for (uint32_t i = 0; i < count; i++) {
		if (offset + 12 > uint64_t(size)) {
			return fail("Truncated composite tile.");
		}
		const uint32_t byte_length = decode_uint32(bytes + offset + 8);
		if (byte_length < 12 || byte_length % 8 || offset + byte_length > uint64_t(size)) {
			return fail("Composite inner tile exceeds its container or is not 8-byte aligned.");
		}
		const char *magic = reinterpret_cast<const char *>(bytes + offset);
		if (memcmp(magic, "b3dm", 4) != 0 && memcmp(magic, "i3dm", 4) != 0 &&
				memcmp(magic, "pnts", 4) != 0 && memcmp(magic, "cmpt", 4) != 0) {
			return fail("Composite tile contains an unsupported child tile type.", ERR_UNAVAILABLE);
		}
		if (!content(p_bytes.slice(offset, offset + byte_length), p_rtc, p_depth + 1)) {
			return false;
		}
		offset += byte_length;
	}
	return offset == uint64_t(size) || fail("Unexpected trailing composite tile data.");
}

} // namespace Tiles3DParserInternal
