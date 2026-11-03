#pragma once

#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/templates/vector.h"
#include <cmath>

namespace Tiles3DParserInternal {

// 表读取器只持有当前瓦片的数据，不访问解码器或渲染状态。
class TileTable {
public:
	Dictionary json;
	PackedByteArray binary;
	String error;

	static int width(const String &p_type) {
		if (p_type == "BYTE" || p_type == "UNSIGNED_BYTE") return 1;
		if (p_type == "SHORT" || p_type == "UNSIGNED_SHORT") return 2;
		if (p_type == "INT" || p_type == "UNSIGNED_INT" || p_type == "FLOAT") return 4;
		if (p_type == "DOUBLE") return 8;
		return 0;
	}

	bool fail(const String &p_error) { error = p_error; return false; }

	bool load(const PackedByteArray &p_json, const PackedByteArray &p_binary) {
		if (p_json.is_empty() || p_json.size() > 16 * 1024 * 1024 || p_binary.size() > 256 * 1024 * 1024) return fail("Invalid tile table size.");
		JSON parser;
		if (parser.parse(String::utf8((const char *)p_json.ptr(), p_json.size())) != OK || parser.get_data().get_type() != Variant::DICTIONARY) return fail("Invalid tile table JSON.");
		json = parser.get_data();
		binary = p_binary;
		return true;
	}

	bool read(const Variant &p_value, const String &p_type, int p_components, int p_count, Vector<double> &r_values, bool p_binary_only = false) {
		const int size = width(p_type);
		const uint64_t count = uint64_t(p_count) * p_components;
		if (!size || p_count < 0 || p_components < 1 || p_components > 4 || count > 16 * 1024 * 1024) return fail("Invalid tile table type or count.");
		const uint8_t *data = nullptr;
		Array values;
		if (p_value.get_type() == Variant::DICTIONARY) {
			const Dictionary reference = p_value;
			const Variant offset_value = reference.get("byteOffset", Variant());
			if (offset_value.get_type() != Variant::INT && offset_value.get_type() != Variant::FLOAT) return fail("Missing binary byteOffset.");
			const double offset = offset_value;
			if (!std::isfinite(offset) || offset < 0 || offset > INT32_MAX || offset != std::floor(offset) || uint64_t(offset) % size || uint64_t(offset) + count * size > uint64_t(binary.size())) return fail("Tile table binary reference is out of range or misaligned.");
			if (reference.has("componentType") && reference["componentType"] != Variant(p_type)) return fail("Unexpected tile table componentType.");
			if (count) data = binary.ptr() + int(offset);
		} else {
			if (p_binary_only) return fail("Per-element feature semantics require a binary reference.");
			if (p_value.get_type() == Variant::ARRAY) values = p_value;
			else if (count == 1) values.push_back(p_value);
			if (uint64_t(values.size()) != count) return fail("Tile table value count mismatch.");
		}
		r_values.resize(count);
		for (int i = 0; i < int(count); i++) {
			double value;
			if (data) {
				const uint8_t *entry = data + uint64_t(i) * size;
				if (p_type == "BYTE") value = int8_t(*entry);
				else if (p_type == "UNSIGNED_BYTE") value = *entry;
				else if (p_type == "SHORT") value = int16_t(decode_uint16(entry));
				else if (p_type == "UNSIGNED_SHORT") value = decode_uint16(entry);
				else if (p_type == "INT") value = int32_t(decode_uint32(entry));
				else if (p_type == "UNSIGNED_INT") value = decode_uint32(entry);
				else if (p_type == "FLOAT") value = decode_float(entry);
				else value = decode_double(entry);
			} else {
				if (values[i].get_type() != Variant::INT && values[i].get_type() != Variant::FLOAT) return fail("Expected numeric tile table value.");
				value = values[i];
			}
			if (!std::isfinite(value)) return fail("Non-finite tile table value.");
			if (p_type != "FLOAT" && p_type != "DOUBLE") {
				const bool unsigned_type = p_type.begins_with("UNSIGNED");
				const double maximum = std::pow(2.0, size * 8 - (unsigned_type ? 0 : 1));
				if (value != std::floor(value) || value < (unsigned_type ? 0 : -maximum) || value >= maximum) return fail("Tile table integer is out of range.");
			}
			r_values.write[i] = value;
		}
		return true;
	}

	bool semantic(const String &p_name, const String &p_type, int p_components, int p_count, Vector<double> &r_values, bool p_binary_only = false) {
		return read(json.get(p_name, Variant()), p_type, p_components, p_count, r_values, p_binary_only);
	}
	bool positions(int p_count, Vector<double> &r_values) {
		if (json.has("POSITION")) return semantic("POSITION", "FLOAT", 3, p_count, r_values, true);
		Vector<double> offset, scale;
		if (!semantic("POSITION_QUANTIZED", "UNSIGNED_SHORT", 3, p_count, r_values, true) || !semantic("QUANTIZED_VOLUME_OFFSET", "FLOAT", 3, 1, offset) || !semantic("QUANTIZED_VOLUME_SCALE", "FLOAT", 3, 1, scale)) return false;
		for (int i = 0; i < r_values.size(); i++) r_values.write[i] = offset[i % 3] + r_values[i] * scale[i % 3] / 65535.0;
		return true;
	}

	bool batch_ids(int p_count, int p_length, PackedInt32Array &r_ids) {
		Vector<double> values;
		String type = "UNSIGNED_SHORT";
		if (json["BATCH_ID"].get_type() == Variant::DICTIONARY) {
			Dictionary descriptor = json["BATCH_ID"];
			type = descriptor.get("componentType", type);
		}
		if (type != "UNSIGNED_BYTE" && type != "UNSIGNED_SHORT" && type != "UNSIGNED_INT") return fail("Invalid BATCH_ID component type.");
		if (!semantic("BATCH_ID", type, 1, p_count, values, true)) return false;
		r_ids.resize(p_count);
		for (int i = 0; i < p_count; i++) {
			if (values[i] >= p_length) return fail("BATCH_ID exceeds batch length.");
			r_ids.set(i, int(values[i]));
		}
		return true;
	}

	static Vector3 oct_normal(double p_x, double p_y, double p_range) {
		Vector3 normal(p_x * 2.0 / p_range - 1.0, p_y * 2.0 / p_range - 1.0, 0);
		normal.z = 1.0 - Math::abs(normal.x) - Math::abs(normal.y);
		if (normal.z < 0) {
			const real_t x = normal.x;
			normal.x = (1.0 - Math::abs(normal.y)) * (x >= 0 ? 1 : -1);
			normal.y = (1.0 - Math::abs(x)) * (normal.y >= 0 ? 1 : -1);
		}
		return normal.normalized();
	}
};

// 标准 1.0 头和表段边界由各格式共用，拒绝历史非规范短头。
struct LegacyTile {
	TileTable feature;
	PackedByteArray batch_json;
	PackedByteArray batch_binary;
	PackedByteArray payload;
	String error;

	bool load(const PackedByteArray &p_bytes, int p_header_size, bool p_has_payload, bool p_allow_unpadded_glb = false) {
		// Some ContextCapture b3dm files end at the 4-byte-aligned GLB boundary.
		// Only their final padding is optional; all table boundaries remain 8-byte aligned.
		const int alignment = p_allow_unpadded_glb ? 4 : 8;
		if (p_bytes.size() < p_header_size || p_bytes.size() % alignment || decode_uint32(p_bytes.ptr() + 4) != 1 || decode_uint32(p_bytes.ptr() + 8) != uint32_t(p_bytes.size())) {
			error = "Invalid standard version 1 tile header or alignment.";
			return false;
		}
		uint64_t ends[5] = { uint64_t(p_header_size) };
		for (int i = 0; i < 4; i++) {
			ends[i + 1] = ends[i] + decode_uint32(p_bytes.ptr() + 12 + 4 * i);
			if (ends[i + 1] > uint64_t(p_bytes.size()) || ends[i + 1] % 8) {
				error = "Tile tables exceed their container or are not 8-byte aligned.";
				return false;
			}
		}
		if ((!p_has_payload && ends[4] != uint64_t(p_bytes.size())) || (p_has_payload && ends[4] == uint64_t(p_bytes.size()))) {
			error = "Invalid tile payload length.";
			return false;
		}
		if (!feature.load(p_bytes.slice(ends[0], ends[1]), p_bytes.slice(ends[1], ends[2]))) { error = feature.error; return false; }
		batch_json = p_bytes.slice(ends[2], ends[3]);
		batch_binary = p_bytes.slice(ends[3], ends[4]);
		payload = p_bytes.slice(ends[4]);
		return true;
	}
};

} // namespace Tiles3DParserInternal
