/**************************************************************************/
/*  tiles_3d_decoder.h                                                    */
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
#pragma once

#include "modules/3dtiles/tiles_3d_parser.h"

#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/math/quaternion.h"
#include "core/templates/hash_set.h"

#include "modules/modules_enabled.gen.h"
#include "modules/tiled_mesh_3d/tiled_mesh_data_source.h"

#ifdef MODULE_DRACO_ENABLED
#include "modules/draco/draco_decoder.h"
#endif

#include <cmath>

namespace Tiles3DParserInternal {

using Vec = TiledMeshVector3d;

Vec add(const Vec &a, const Vec &b);
Vec subtract(const Vec &a, const Vec &b);
Vec multiply(const Vec &a, double b);
double length(const Vec &a);
Vector3 vector3(const Vec &a);

struct Matrix {
	double m[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

	Vec direction(const Vec &p) const;
	Vec point(const Vec &p) const;
	Matrix operator*(const Matrix &p_other) const;
	Basis basis() const;
	double maximum_scale() const;
	Array array() const;
};

struct Context {
	Matrix transform;
	Basis frame = Basis(Vector3(1, 0, 0), Vector3(0, 0, -1), Vector3(0, 1, 0));
	Vec center;
	String key;
	Array ancestors;
	TiledMeshTileDefinition::RefinementMode refinement = TiledMeshTileDefinition::REFINE_REPLACE;
};

class Decoder {
	const TiledMeshParseRequest &request;
	const Tiles3DParseOptions &options;
	const HashMap<String, PackedByteArray> &external_buffers;
	TiledMeshDocument &document;
	Vector<String> &dependencies;
	String &message;
	Error error = ERR_FILE_CORRUPT;
	Context context;
	String gltf_base_uri;
	bool has_aabb = false;
	int active_batch_length = -1;
	int active_batch_table = -1;

	bool fail(const String &p_message, Error p_error = ERR_FILE_CORRUPT);
	bool number(const Variant &p_value, double &r_value);
	bool integer(const Variant &p_value, int &r_value, int p_min = 0);
	bool numbers(const Variant &p_value, int p_count, double *r_values);
	bool vec(const Variant &p_value, Vec &r_value);
	bool matrix(const Variant &p_value, Matrix &r_value);
	bool object(const Variant &p_value, Dictionary &r_value);
	bool array(const Variant &p_value, Array &r_value);
	bool json(const PackedByteArray &p_bytes, Dictionary &r_value);
	bool batch_table(const PackedByteArray &p_json, const PackedByteArray &p_binary, int p_batch_length);
	bool required_extensions(const Dictionary &p_json, const Vector<String> &p_supported);
	static Array vec_array(const Vec &p_vec);
	PackedByteArray encode_context(const Context &p_context);
	bool decode_context();
	void choose_frame(const Vec &p_center, bool p_region);
	bool bounds(const Dictionary &p_json, const Matrix &p_transform, TiledMeshBoundingVolume &r_bounds);
	Vector3 local(const Vec &p_position) const;
	bool tile(const Dictionary &p_tile, const Context &p_parent, int p_parent_index, double p_parent_error,
			const TiledMeshBoundingVolume &p_parent_bounds, int p_depth);
	bool tileset(const Dictionary &p_json);

	bool data_uri(const String &p_uri, PackedByteArray &r_bytes);
	struct Gltf {
		Dictionary json;
		Vector<PackedByteArray> buffers;
		Array views;
		Array accessors;
		Array nodes;
		Array meshes;
		Array materials;
		Array textures;
		Array images;
		HashMap<int, TiledMeshImagePayload> decoded_images;
		HashMap<int, int> decoded_materials;
	};
	bool view(const Gltf &p_gltf, int p_index, const uint8_t *&r_data, int &r_size, int &r_stride);
	static int component_size(int p_type);
	static double component(const uint8_t *p_data, int p_type, bool p_normalized);
	bool accessor(const Gltf &p_gltf, const Variant &p_index, int p_components, Vector<double> &r_values, bool p_indices = false);
#ifdef MODULE_DRACO_ENABLED
	bool draco_attribute(const Gltf &p_gltf, const Dictionary &p_attributes, const Dictionary &p_mapping, const String &p_semantic,
			int p_components, int p_point_count, const DracoDecoder::Geometry &p_geometry, Vector<double> &r_values);
#endif
	bool texture(Gltf &p_gltf, const Dictionary &p_info, TiledMeshImagePayload &r_image);
	bool material(Gltf &p_gltf, const Variant &p_index, int &r_index);
	bool primitive(Gltf &p_gltf, const Dictionary &p_primitive, const Matrix &p_transform);
	bool node(Gltf &p_gltf, int p_index, const Matrix &p_parent, HashSet<int> &r_visited, int p_depth);
	bool gltf(const Dictionary &p_json, const PackedByteArray &p_binary, const Vec &p_rtc);

	bool decode_cmpt(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth);
	bool decode_b3dm(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth);
	bool decode_i3dm(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth);
	bool decode_pnts(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth);
	bool decode_glb(const PackedByteArray &p_bytes, const Vec &p_rtc);
	bool content(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth = 0);

public:
	Decoder(const TiledMeshParseRequest &p_request, const Tiles3DParseOptions &p_options,
			const HashMap<String, PackedByteArray> &p_buffers, TiledMeshDocument &r_document,
			Vector<String> &r_dependencies, String &r_error);
	Error parse();
};

} // namespace Tiles3DParserInternal
