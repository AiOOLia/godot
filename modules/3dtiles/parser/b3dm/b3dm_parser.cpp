/**************************************************************************/
/*  b3dm_parser.cpp                                                       */
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
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "modules/3dtiles/parser/tiles_3d_decoder.h"
#include "modules/3dtiles/parser/tile_table.h"

namespace Tiles3DParserInternal {

bool Decoder::batch_table(const PackedByteArray &p_json, const PackedByteArray &p_binary, int p_batch_length) {
	if (p_batch_length < 0 || p_batch_length > 1000000) return fail("Batch Table exceeds the supported row limit.");
	if (p_json.is_empty()) {
		if (!p_binary.is_empty()) return fail("Batch Table binary requires JSON.");
		active_batch_length = p_batch_length;
		active_batch_table = -1;
		return true;
	}
	TileTable source;
	if (!source.load(p_json, p_binary)) return fail(source.error);
	if (source.json.size() > 256) return fail("Batch Table exceeds the property limit.");
	TiledMeshBatchTablePayload table;
	table.batch_length = p_batch_length;
	uint64_t total_values = 0;
	for (const Variant &key_value : source.json.keys()) {
		const String key = key_value;
		const Variant value = source.json[key_value];
		// 保留元数据容器，扩展不能误当作逐要素属性。
		if (key == "extras") { table.extras = value; continue; }
		if (key == "extensions") {
			if (!object(value, table.extensions)) return false;
			if (!table.extensions.is_empty()) return fail("Batch Table extensions are not supported.", ERR_UNAVAILABLE);
			continue;
		}
		Array values;
		if (value.get_type() == Variant::ARRAY) {
			values = value;
			if (values.size() != p_batch_length) return fail("Batch Table property count does not match batch length.");
		} else if (value.get_type() == Variant::DICTIONARY) {
			const Dictionary descriptor = value;
			const String type = descriptor.get("componentType", "");
			const String shape = descriptor.get("type", "");
			const int components = shape == "SCALAR" ? 1 : (shape == "VEC2" ? 2 : (shape == "VEC3" ? 3 : (shape == "VEC4" ? 4 : 0)));
			Vector<double> decoded;
			if (!source.read(value, type, components, p_batch_length, decoded, true)) return fail(source.error);
			values.resize(p_batch_length);
			for (int row = 0; row < p_batch_length; row++) {
				Array vector_value;
				for (int c = 0; c < components; c++) {
					const double number = decoded[row * components + c];
					vector_value.push_back(type == "FLOAT" || type == "DOUBLE" ? Variant(number) : Variant(int64_t(number)));
				}
				values[row] = components == 1 ? vector_value[0] : Variant(vector_value);
			}
		} else return fail("Batch Table properties must be arrays or binary descriptors.");
		total_values += values.size();
		if (total_values > 16 * 1024 * 1024) return fail("Batch Table exceeds the value limit.");
		table.properties[key] = values;
	}
	active_batch_length = p_batch_length;
	active_batch_table = document.batch_tables.size();
	document.batch_tables.push_back(table);
	document.estimated_cpu_bytes += p_json.size() + p_binary.size();
	return true;
}

bool Decoder::decode_b3dm(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth) {
	LegacyTile tile;
	if (!tile.load(p_bytes, 28, true, true)) return fail(tile.error);
	Vector<double> values;
	if (!tile.feature.semantic("BATCH_LENGTH", "UNSIGNED_INT", 1, 1, values)) return fail(tile.feature.error);
	if (values[0] > 1000000) return fail("BATCH_LENGTH exceeds the supported limit.");
	const int batch_length = int(values[0]);
	Vec rtc = p_rtc;
	if (tile.feature.json.has("RTC_CENTER")) {
		if (!tile.feature.semantic("RTC_CENTER", "FLOAT", 3, 1, values)) return fail(tile.feature.error);
		rtc = add(rtc, { values[0], values[1], values[2] });
	}
	if (tile.payload.size() < 12 || memcmp(tile.payload.ptr(), "glTF", 4)) return fail("b3dm payload must contain a GLB.");
	const uint32_t size = decode_uint32(tile.payload.ptr() + 8);
	if (size < 12 || size > uint32_t(tile.payload.size()) || tile.payload.size() - size > 7) return fail("Invalid b3dm embedded GLB length.");
	if (p_bytes.size() % 8 && size != uint32_t(tile.payload.size())) return fail("Unpadded b3dm must end at the embedded GLB boundary.");
	const int previous_batch_length = active_batch_length;
	const int previous_batch_table = active_batch_table;
	if (!batch_table(tile.batch_json, tile.batch_binary, batch_length)) return false;
	const bool parsed = decode_glb(tile.payload.slice(0, size), rtc);
	active_batch_length = previous_batch_length;
	active_batch_table = previous_batch_table;
	return parsed;
}

} // namespace Tiles3DParserInternal
