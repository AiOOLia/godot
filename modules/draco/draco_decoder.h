/**************************************************************************/
/*  draco_decoder.h                                                       */
/**************************************************************************/

#pragma once

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

class DracoDecoder {
public:
	struct Limits {
		uint64_t max_input_bytes = 64 * 1024 * 1024;
		uint64_t max_points = 16 * 1024 * 1024;
		uint64_t max_faces = 16 * 1024 * 1024;
		uint64_t max_attribute_components = 256 * 1024 * 1024;
	};

	struct Attribute {
		int unique_id = -1;
		int semantic = 0;
		int component_type = 0;
		int component_count = 0;
		bool normalized = false;
		PackedFloat64Array values;
	};

	struct Geometry {
		int point_count = 0;
		int face_count = 0;
		bool is_mesh = false;
		PackedInt32Array indices;
		HashMap<int, Attribute> attributes;
	};

	static Error decode(const uint8_t *p_data, uint64_t p_size, Geometry &r_geometry, String &r_error, const Limits &p_limits = Limits());
};
