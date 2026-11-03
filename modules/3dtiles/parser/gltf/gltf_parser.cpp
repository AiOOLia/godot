/**************************************************************************/
/*  gltf_parser.cpp                                                       */
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

bool Decoder::data_uri(const String &p_uri, PackedByteArray &r_bytes) {
		const int comma = p_uri.find_char(',');
		if (comma < 0 || !p_uri.left(comma).ends_with(";base64")) {
			return fail("Only base64-encoded glTF data URIs are supported.", ERR_UNAVAILABLE);
		}
		const CharString encoded = p_uri.substr(comma + 1).ascii();
		r_bytes.resize(encoded.length() / 4 * 3 + 3);
		size_t decoded_length = 0;
		if (CryptoCore::b64_decode(r_bytes.ptrw(), r_bytes.size(), &decoded_length, reinterpret_cast<const uint8_t *>(encoded.get_data()), encoded.length()) != OK || decoded_length == 0) {
			return fail("Invalid or empty base64 glTF data URI.");
		}
		r_bytes.resize(decoded_length);
		return true;
	}

bool Decoder::view(const Gltf &p_gltf, int p_index, const uint8_t *&r_data, int &r_size, int &r_stride) {
		if (p_index < 0 || p_index >= p_gltf.views.size()) {
			return fail("glTF bufferView index is out of range.");
		}
		Dictionary value;
		int buffer_index, offset;
		if (!object(p_gltf.views[p_index], value) || !integer(value.get("buffer", Variant()), buffer_index) ||
				!integer(value.get("byteOffset", 0), offset) || !integer(value.get("byteLength", Variant()), r_size, 1) ||
				!integer(value.get("byteStride", 0), r_stride)) {
			return false;
		}
		if (buffer_index >= p_gltf.buffers.size() || int64_t(offset) + r_size > p_gltf.buffers[buffer_index].size()) {
			return fail("glTF bufferView exceeds its buffer.");
		}
		r_data = p_gltf.buffers[buffer_index].ptr() + offset;
		return true;
	}
int Decoder::component_size(int p_type) {
		switch (p_type) {
			case 5120:
			case 5121:
				return 1;
			case 5122:
			case 5123:
				return 2;
			case 5125:
			case 5126:
				return 4;
			default:
				return 0;
		}
	}
double Decoder::component(const uint8_t *p_data, int p_type, bool p_normalized) {
		switch (p_type) {
			case 5120:
				return p_normalized ? MAX(double(int8_t(*p_data)) / 127.0, -1.0) : double(int8_t(*p_data));
			case 5121:
				return p_normalized ? double(*p_data) / 255.0 : double(*p_data);
			case 5122:
				return p_normalized ? MAX(double(int16_t(decode_uint16(p_data))) / 32767.0, -1.0) : double(int16_t(decode_uint16(p_data)));
			case 5123:
				return p_normalized ? double(decode_uint16(p_data)) / 65535.0 : double(decode_uint16(p_data));
			case 5125:
				return double(decode_uint32(p_data));
			case 5126:
				return decode_float(p_data);
			default:
				return 0;
		}
	}

bool Decoder::accessor(const Gltf &p_gltf, const Variant &p_index, int p_components, Vector<double> &r_values, bool p_indices) {
		int index;
		if (!integer(p_index, index) || index >= p_gltf.accessors.size()) {
			return fail("glTF accessor index is out of range.");
		}
		Dictionary value;
		int count, type, offset;
		if (!object(p_gltf.accessors[index], value) || !integer(value.get("count", Variant()), count, 1) ||
				!integer(value.get("componentType", Variant()), type) || !integer(value.get("byteOffset", 0), offset)) {
			return false;
		}
		const String shape = value.get("type", "");
		const int components = shape == "SCALAR" ? 1 : (shape == "VEC2" ? 2 : (shape == "VEC3" ? 3 : (shape == "VEC4" ? 4 : 0)));
		const bool normalized = value.get("normalized", false);
		const int width = component_size(type);
		if (components != p_components || width == 0 || int64_t(count) * components > 32 * 1024 * 1024 ||
				(p_indices && (normalized || (type != 5121 && type != 5123 && type != 5125)))) {
			return fail("Invalid or excessively large glTF accessor.");
		}
		r_values.resize(count * components);
		r_values.fill(0.0);
		if (value.has("bufferView")) {
			const uint8_t *bytes;
			int view_index, size, stride;
			if (!integer(value["bufferView"], view_index) || !view(p_gltf, view_index, bytes, size, stride)) {
				return false;
			}
			if (stride == 0) {
				stride = width * components;
			}
			if (stride < width * components || stride % width != 0 || int64_t(offset) + int64_t(count - 1) * stride + width * components > size) {
				return fail("glTF accessor exceeds its bufferView or has an invalid stride.");
			}
			for (int i = 0; i < count; i++) {
				for (int c = 0; c < components; c++) {
					r_values.write[i * components + c] = component(bytes + offset + int64_t(i) * stride + c * width, type, normalized);
				}
			}
		} else if (offset != 0) {
			return fail("A glTF accessor without a bufferView cannot have a byteOffset.");
		}
		if (value.has("sparse")) {
			Dictionary sparse, indices, values;
			int sparse_count, index_type, index_view, index_offset, value_view, value_offset;
			if (!object(value["sparse"], sparse) || !integer(sparse.get("count", Variant()), sparse_count, 1) || sparse_count > count ||
					!object(sparse.get("indices", Variant()), indices) || !object(sparse.get("values", Variant()), values) ||
					!integer(indices.get("componentType", Variant()), index_type) || !integer(indices.get("bufferView", Variant()), index_view) ||
					!integer(indices.get("byteOffset", 0), index_offset) || !integer(values.get("bufferView", Variant()), value_view) ||
					!integer(values.get("byteOffset", 0), value_offset)) {
				return fail("Invalid sparse glTF accessor.");
			}
			if (index_type != 5121 && index_type != 5123 && index_type != 5125) {
				return fail("Invalid sparse index component type.");
			}
			const uint8_t *index_bytes, *value_bytes;
			int index_size, value_size, stride;
			const int index_width = component_size(index_type);
			if (!view(p_gltf, index_view, index_bytes, index_size, stride) || !view(p_gltf, value_view, value_bytes, value_size, stride) ||
					int64_t(index_offset) + int64_t(sparse_count) * index_width > index_size ||
					int64_t(value_offset) + int64_t(sparse_count) * width * components > value_size) {
				return fail("Sparse glTF accessor exceeds its bufferViews.");
			}
			int64_t previous = -1;
			for (int i = 0; i < sparse_count; i++) {
				const int64_t destination = int64_t(component(index_bytes + index_offset + int64_t(i) * index_width, index_type, false));
				if (destination <= previous || destination >= count) {
					return fail("Sparse glTF indices must be increasing and in range.");
				}
				previous = destination;
				for (int c = 0; c < components; c++) {
					r_values.write[destination * components + c] = component(value_bytes + value_offset + (int64_t(i) * components + c) * width, type, normalized);
				}
			}
		}
		for (double entry : r_values) {
			if (!std::isfinite(entry)) {
				return fail("Non-finite glTF accessor value.");
			}
		}
		return true;
	}

#ifdef MODULE_DRACO_ENABLED
bool Decoder::draco_attribute(const Gltf &p_gltf, const Dictionary &p_attributes, const Dictionary &p_mapping, const String &p_semantic, int p_components, int p_point_count, const DracoDecoder::Geometry &p_geometry, Vector<double> &r_values) {
		int accessor_index, unique_id;
		Dictionary accessor_value;
		if (!integer(p_attributes.get(p_semantic, Variant()), accessor_index) || accessor_index >= p_gltf.accessors.size() ||
				!integer(p_mapping.get(p_semantic, Variant()), unique_id) || !object(p_gltf.accessors[accessor_index], accessor_value)) {
			return fail("Invalid Draco attribute mapping for " + p_semantic + ".");
		}
		const DracoDecoder::Attribute *attribute = p_geometry.attributes.getptr(unique_id);
		int count, component_type;
		const String expected_shape = p_components == 1 ? "SCALAR" : (p_components == 2 ? "VEC2" : (p_components == 3 ? "VEC3" : "VEC4"));
		if (!attribute || !integer(accessor_value.get("count", Variant()), count, 1) || count != p_point_count ||
				!integer(accessor_value.get("componentType", Variant()), component_type) || String(accessor_value.get("type", "")) != expected_shape ||
				attribute->component_count != p_components || attribute->normalized != bool(accessor_value.get("normalized", false))) {
			return fail("Decoded Draco attribute does not match its glTF accessor for " + p_semantic + ".");
		}
		static const int draco_to_gltf[] = { 0, 5120, 5121, 5122, 5123, 5124, 5125, 5134, 5135, 5126, 5130 };
		if (attribute->component_type <= 0 || attribute->component_type >= int(sizeof(draco_to_gltf) / sizeof(draco_to_gltf[0])) || draco_to_gltf[attribute->component_type] != component_type) {
			return fail("Decoded Draco component type does not match its glTF accessor for " + p_semantic + ".");
		}
		r_values.resize(attribute->values.size());
		for (int i = 0; i < attribute->values.size(); i++) {
			r_values.write[i] = attribute->values[i];
		}
		return true;
	}
#endif

bool Decoder::texture(Gltf &p_gltf, const Dictionary &p_info, TiledMeshImagePayload &r_image) {
		int index, source, tex_coord;
		if (!integer(p_info.get("index", Variant()), index) || index >= p_gltf.textures.size() ||
				!integer(p_info.get("texCoord", 0), tex_coord)) {
			return fail("Invalid glTF texture index or texture coordinates.");
		}
		if (tex_coord != 0) {
			return fail("Base color textures currently require TEXCOORD_0.", ERR_UNAVAILABLE);
		}
		Dictionary entry;
		if (!object(p_gltf.textures[index], entry) || !integer(entry.get("source", Variant()), source) || source >= p_gltf.images.size()) {
			return fail("Invalid glTF texture image source.");
		}
		if (const TiledMeshImagePayload *cached = p_gltf.decoded_images.getptr(source)) {
			r_image = *cached;
			return true;
		}
		Dictionary image;
		if (!object(p_gltf.images[source], image)) {
			return false;
		}
		PackedByteArray bytes;
		String extension;
		if (image.has("uri")) {
			const String uri = image["uri"];
			if (uri.begins_with("data:")) {
				if (!data_uri(uri, bytes)) {
					return false;
				}
				extension = uri.get_slice(";", 0).get_slice("/", 1);
			} else {
				r_image.uri = TiledMeshDataSource::resolve_uri(gltf_base_uri.is_empty() ? request.uri : gltf_base_uri, uri);
				if (r_image.uri.is_empty()) {
					return fail("Unable to resolve glTF image URI.");
				}
			}
		} else {
			int view_index, size, stride;
			const uint8_t *data;
			if (!integer(image.get("bufferView", Variant()), view_index) || !view(p_gltf, view_index, data, size, stride)) {
				return false;
			}
			bytes.resize(size);
			memcpy(bytes.ptrw(), data, size);
			extension = String(image.get("mimeType", "")).get_slice("/", 1);
		}
		if (!bytes.is_empty()) {
			Ref<Image> decoded;
			String image_error;
			const Error result = TiledMeshDataSource::decode_image_bytes(bytes, extension, decoded, image_error);
			if (result != OK) {
				return fail("Unable to decode embedded glTF image: " + image_error, result);
			}
			decoded->convert(Image::FORMAT_RGBA8);
			r_image.width = decoded->get_width();
			r_image.height = decoded->get_height();
			r_image.rgba8 = decoded->get_data();
			document.estimated_cpu_bytes += r_image.rgba8.size();
		}
		p_gltf.decoded_images.insert(source, r_image);
		return true;
	}

bool Decoder::material(Gltf &p_gltf, const Variant &p_index, int &r_index) {
		int index;
		if (!integer(p_index, index) || index >= p_gltf.materials.size()) {
			return fail("glTF material index is out of range.");
		}
		if (const int *cached = p_gltf.decoded_materials.getptr(index)) {
			r_index = *cached;
			return true;
		}
		Dictionary entry, pbr, extensions;
		if (!object(p_gltf.materials[index], entry) || !object(entry.get("pbrMetallicRoughness", Dictionary()), pbr) ||
				!object(entry.get("extensions", Dictionary()), extensions)) {
			return false;
		}
		TiledMeshMaterialPayload payload;
		if (extensions.has("KHR_materials_unlit")) {
			Dictionary unlit;
			if (!object(extensions["KHR_materials_unlit"], unlit)) {
				return false;
			}
			payload.unlit = true;
		}
		double color[4] = { 1, 1, 1, 1 };
		double emission[3] = { 0, 0, 0 };
		double metallic, roughness, cutoff;
		if ((pbr.has("baseColorFactor") && !numbers(pbr["baseColorFactor"], 4, color)) ||
				(entry.has("emissiveFactor") && !numbers(entry["emissiveFactor"], 3, emission)) ||
				!number(pbr.get("metallicFactor", 1.0), metallic) || !number(pbr.get("roughnessFactor", 1.0), roughness) ||
				!number(entry.get("alphaCutoff", 0.5), cutoff)) {
			return false;
		}
		payload.albedo = Color(color[0], color[1], color[2], color[3]).linear_to_srgb();
		payload.emission = Color(emission[0], emission[1], emission[2]).linear_to_srgb();
		payload.metallic = metallic;
		payload.roughness = roughness;
		payload.double_sided = entry.get("doubleSided", false);
		const String alpha = entry.get("alphaMode", "OPAQUE");
		if (alpha != "OPAQUE" && alpha != "MASK" && alpha != "BLEND") {
			return fail("Invalid glTF material alphaMode.");
		}
		payload.transparent = alpha == "BLEND";
		payload.alpha_test = alpha == "MASK";
		payload.alpha_cutoff = cutoff;
		if (alpha == "OPAQUE") {
			payload.albedo.a = 1.0;
		}
		if (pbr.has("baseColorTexture")) {
			Dictionary info;
			if (!object(pbr["baseColorTexture"], info) || !texture(p_gltf, info, payload.albedo_image)) {
				return false;
			}
		}
		if (entry.has("normalTexture") || entry.has("emissiveTexture") || entry.has("occlusionTexture") || pbr.has("metallicRoughnessTexture")) {
			document.warnings.push_back("The tiled mesh renderer currently displays base color textures and scalar material factors; other material textures are ignored.");
		}
		r_index = document.materials.size();
		document.materials.push_back(payload);
		p_gltf.decoded_materials.insert(index, r_index);
		return true;
	}

bool Decoder::primitive(Gltf &p_gltf, const Dictionary &p_primitive, const Matrix &p_transform) {
		Dictionary attributes, extensions;
		if (!object(p_primitive.get("attributes", Variant()), attributes) || !object(p_primitive.get("extensions", Dictionary()), extensions)) {
			return false;
		}
		int mode;
		if (!integer(p_primitive.get("mode", 4), mode)) {
			return false;
		}
		if (mode != 4 && mode != 5 && mode != 6) {
			return fail("Only glTF triangles, triangle strips and triangle fans are supported.", ERR_UNAVAILABLE);
		}
#ifdef MODULE_DRACO_ENABLED
		DracoDecoder::Geometry draco_geometry;
		Dictionary draco_mapping;
		bool has_draco = false;
		if (extensions.has("KHR_draco_mesh_compression")) {
			Dictionary draco_extension;
			int view_index, size, stride;
			const uint8_t *data;
			if (!object(extensions["KHR_draco_mesh_compression"], draco_extension) || !integer(draco_extension.get("bufferView", Variant()), view_index) ||
					!object(draco_extension.get("attributes", Variant()), draco_mapping) || !view(p_gltf, view_index, data, size, stride)) {
				return false;
			}
			String decode_error;
			const Error decode_result = DracoDecoder::decode(data, size, draco_geometry, decode_error);
			if (decode_result != OK) {
				return fail("Unable to decode Draco primitive: " + decode_error, decode_result);
			}
			if (!draco_geometry.is_mesh || mode != 4) {
				return fail("Draco glTF primitives must contain a triangular mesh using TRIANGLES mode.");
			}
			has_draco = true;
		}
#else
		if (extensions.has("KHR_draco_mesh_compression")) {
			return fail("Draco-compressed glTF primitives require the Draco module.", ERR_UNAVAILABLE);
		}
#endif
		Vector<double> positions;
#ifdef MODULE_DRACO_ENABLED
		if (has_draco) {
			if (!draco_attribute(p_gltf, attributes, draco_mapping, "POSITION", 3, draco_geometry.point_count, draco_geometry, positions)) {
				return false;
			}
		} else
#endif
				if (!accessor(p_gltf, attributes.get("POSITION", Variant()), 3, positions)) {
			return false;
		}
		const int count = positions.size() / 3;
		TiledMeshMeshSurfacePayload surface;
		surface.batch_table = active_batch_table;
		const String batch_semantic = active_batch_length >= 0 ? (attributes.has("_BATCHID") ? "_BATCHID" : (attributes.has("BATCHID") ? "BATCHID" : "")) : "";
		if (!batch_semantic.is_empty()) {
			int accessor_index, component_type, accessor_count;
			Dictionary batch_accessor;
			if (!integer(attributes[batch_semantic], accessor_index) || accessor_index >= p_gltf.accessors.size() ||
					!object(p_gltf.accessors[accessor_index], batch_accessor) || !integer(batch_accessor.get("count", Variant()), accessor_count, 1) ||
					!integer(batch_accessor.get("componentType", Variant()), component_type) || accessor_count != count ||
					String(batch_accessor.get("type", "")) != "SCALAR" || bool(batch_accessor.get("normalized", false)) ||
					(component_type != 5120 && component_type != 5121 && component_type != 5122 && component_type != 5123 && component_type != 5125)) {
				return fail("b3dm batch ID attribute must be a non-normalized integer SCALAR matching POSITION count.");
			}
			Vector<double> ids;
#ifdef MODULE_DRACO_ENABLED
			const bool ids_ok = has_draco ? draco_attribute(p_gltf, attributes, draco_mapping, batch_semantic, 1, draco_geometry.point_count, draco_geometry, ids) : accessor(p_gltf, accessor_index, 1, ids);
#else
			const bool ids_ok = accessor(p_gltf, accessor_index, 1, ids);
#endif
			if (!ids_ok || ids.size() != count) {
				return fail("Invalid b3dm batch ID attribute.");
			}
			surface.batch_ids.resize(count);
			for (int i = 0; i < count; i++) {
				if (ids[i] < 0 || ids[i] >= active_batch_length || ids[i] != std::floor(ids[i])) {
					return fail("b3dm batch ID is outside BATCH_LENGTH.");
				}
				surface.batch_ids.set(i, int(ids[i]));
			}
		} else if (active_batch_length == 1) {
			surface.batch_ids.resize(count);
			surface.batch_ids.fill(0);
		} else if (active_batch_length > 1) {
			return fail("b3dm with BATCH_LENGTH greater than one requires a _BATCHID or BATCHID attribute.");
		}
		surface.replacement_group = 0;
		surface.vertices.resize(count);
		for (int i = 0; i < count; i++) {
			const Vector3 position = local(p_transform.point({ positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2] }));
			if (!position.is_finite()) {
				return fail("Transformed glTF vertex overflow.");
			}
			surface.vertices.set(i, position);
			if (i == 0) {
				surface.aabb = AABB(position, Vector3());
			} else {
				surface.aabb.expand_to(position);
			}
		}
		Vector<double> source_indices;
#ifdef MODULE_DRACO_ENABLED
		if (has_draco) {
			if (!p_primitive.has("indices")) {
				return fail("A Draco mesh primitive must define an indices accessor.");
			}
			int accessor_index, accessor_count, accessor_type;
			Dictionary index_accessor;
			if (!integer(p_primitive["indices"], accessor_index) || accessor_index >= p_gltf.accessors.size() || !object(p_gltf.accessors[accessor_index], index_accessor) ||
					!integer(index_accessor.get("count", Variant()), accessor_count, 1) || accessor_count != draco_geometry.indices.size() ||
					!integer(index_accessor.get("componentType", Variant()), accessor_type) || (accessor_type != 5121 && accessor_type != 5123 && accessor_type != 5125) ||
					String(index_accessor.get("type", "")) != "SCALAR" || bool(index_accessor.get("normalized", false))) {
				return fail("Decoded Draco indices do not match their glTF accessor.");
			}
			source_indices.resize(draco_geometry.indices.size());
			for (int i = 0; i < draco_geometry.indices.size(); i++) {
				source_indices.write[i] = draco_geometry.indices[i];
			}
		} else
#endif
				if (p_primitive.has("indices")) {
			if (!accessor(p_gltf, p_primitive["indices"], 1, source_indices, true)) {
				return false;
			}
		} else {
			source_indices.resize(count);
			for (int i = 0; i < count; i++) {
				source_indices.write[i] = i;
			}
		}
		for (double index : source_indices) {
			if (index < 0 || index >= count) {
				return fail("glTF vertex index is out of range.");
			}
		}
		if (source_indices.size() < 3 || (mode == 4 && source_indices.size() % 3 != 0)) {
			return fail("Invalid glTF triangle index count.");
		}
		const int triangles = mode == 4 ? source_indices.size() / 3 : source_indices.size() - 2;
		surface.indices.resize(triangles * 3);
		const Basis basis = context.frame * p_transform.basis();
		if (basis.determinant() == 0 || !basis.is_finite()) {
			return fail("Singular glTF node transform.");
		}
		const bool mirrored = basis.determinant() < 0;
		for (int i = 0; i < triangles; i++) {
			int a, b, c;
			if (mode == 4) {
				a = int(source_indices[i * 3]);
				b = int(source_indices[i * 3 + 1]);
				c = int(source_indices[i * 3 + 2]);
			} else if (mode == 5) {
				a = int(source_indices[i + (i % 2)]);
				b = int(source_indices[i + 1 - (i % 2)]);
				c = int(source_indices[i + 2]);
			} else {
				a = int(source_indices[0]);
				b = int(source_indices[i + 1]);
				c = int(source_indices[i + 2]);
			}
			// glTF is counterclockwise; Godot uses clockwise front faces.
			surface.indices.set(i * 3, a);
			surface.indices.set(i * 3 + 1, mirrored ? b : c);
			surface.indices.set(i * 3 + 2, mirrored ? c : b);
			if (!surface.batch_ids.is_empty() && (surface.batch_ids[a] != surface.batch_ids[b] || surface.batch_ids[a] != surface.batch_ids[c])) {
				return fail("A b3dm triangle references more than one batch ID.");
			}
		}
		if (attributes.has("NORMAL")) {
			Vector<double> normals;
#ifdef MODULE_DRACO_ENABLED
			const bool normals_ok = has_draco ? draco_attribute(p_gltf, attributes, draco_mapping, "NORMAL", 3, draco_geometry.point_count, draco_geometry, normals) : accessor(p_gltf, attributes["NORMAL"], 3, normals);
#else
			const bool normals_ok = accessor(p_gltf, attributes["NORMAL"], 3, normals);
#endif
			if (!normals_ok || normals.size() != positions.size()) {
				return fail("Invalid glTF NORMAL attribute.");
			}
			const Basis normal_basis = basis.inverse().transposed();
			surface.normals.resize(count);
			for (int i = 0; i < count; i++) {
				surface.normals.set(i, normal_basis.xform(Vector3(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2])).normalized());
			}
		} else {
			// glTF requires flat normals when NORMAL is omitted. Duplicate
			// vertices after all attributes have been decoded below.
			surface.normals.resize(count);
		}
		for (int channel = 0; channel < 2; channel++) {
			const String attribute = "TEXCOORD_" + itos(channel);
			if (!attributes.has(attribute)) {
				continue;
			}
			Vector<double> values;
#ifdef MODULE_DRACO_ENABLED
			const bool texture_coordinates_ok = has_draco ? draco_attribute(p_gltf, attributes, draco_mapping, attribute, 2, draco_geometry.point_count, draco_geometry, values) : accessor(p_gltf, attributes[attribute], 2, values);
#else
			const bool texture_coordinates_ok = accessor(p_gltf, attributes[attribute], 2, values);
#endif
			if (!texture_coordinates_ok || values.size() != count * 2) {
				return fail("Invalid glTF texture coordinates.");
			}
			PackedVector2Array &uv = channel == 0 ? surface.uv0 : surface.uv1;
			uv.resize(count);
			for (int i = 0; i < count; i++) {
				uv.set(i, Vector2(values[i * 2], values[i * 2 + 1]));
			}
		}
		if (attributes.has("COLOR_0")) {
			int accessor_index;
			Dictionary color_accessor;
			if (!integer(attributes["COLOR_0"], accessor_index) || accessor_index >= p_gltf.accessors.size() || !object(p_gltf.accessors[accessor_index], color_accessor)) {
				return fail("Invalid glTF COLOR_0 accessor.");
			}
			const int components = String(color_accessor.get("type", "")) == "VEC3" ? 3 : 4;
			Vector<double> colors;
#ifdef MODULE_DRACO_ENABLED
			const bool colors_ok = has_draco ? draco_attribute(p_gltf, attributes, draco_mapping, "COLOR_0", components, draco_geometry.point_count, draco_geometry, colors) : accessor(p_gltf, accessor_index, components, colors);
#else
			const bool colors_ok = accessor(p_gltf, accessor_index, components, colors);
#endif
			if (!colors_ok || colors.size() != count * components) {
				return fail("Invalid glTF COLOR_0 attribute.");
			}
			surface.colors.resize(count);
			for (int i = 0; i < count; i++) {
				surface.colors.set(i, Color(colors[i * components], colors[i * components + 1], colors[i * components + 2], components == 4 ? colors[i * components + 3] : 1).linear_to_srgb());
			}
		}
		if (!attributes.has("NORMAL")) {
			TiledMeshMeshSurfacePayload flat;
			flat.vertices.resize(surface.indices.size());
			flat.normals.resize(surface.indices.size());
			if (!surface.uv0.is_empty()) {
				flat.uv0.resize(surface.indices.size());
			}
			if (!surface.uv1.is_empty()) {
				flat.uv1.resize(surface.indices.size());
			}
			if (!surface.colors.is_empty()) {
				flat.colors.resize(surface.indices.size());
			}
			if (!surface.batch_ids.is_empty()) {
				flat.batch_ids.resize(surface.indices.size());
			}
			for (int i = 0; i < surface.indices.size(); i += 3) {
				const Vector3 a = surface.vertices[surface.indices[i]];
				const Vector3 b = surface.vertices[surface.indices[i + 1]];
				const Vector3 c = surface.vertices[surface.indices[i + 2]];
				const Vector3 normal = (c - a).cross(b - a).normalized();
				for (int j = 0; j < 3; j++) {
					const int index = surface.indices[i + j];
					flat.vertices.set(i + j, surface.vertices[index]);
					flat.normals.set(i + j, normal);
					if (!flat.uv0.is_empty()) {
						flat.uv0.set(i + j, surface.uv0[index]);
					}
					if (!flat.uv1.is_empty()) {
						flat.uv1.set(i + j, surface.uv1[index]);
					}
					if (!flat.colors.is_empty()) {
						flat.colors.set(i + j, surface.colors[index]);
					}
					if (!flat.batch_ids.is_empty()) {
						flat.batch_ids.set(i + j, surface.batch_ids[index]);
					}
					surface.indices.set(i + j, i + j);
				}
			}
			surface.vertices = flat.vertices;
			surface.normals = flat.normals;
			surface.uv0 = flat.uv0;
			surface.uv1 = flat.uv1;
			surface.colors = flat.colors;
			surface.batch_ids = flat.batch_ids;
		}
		if (p_primitive.has("material")) {
			if (!material(p_gltf, p_primitive["material"], surface.material)) {
				return false;
			}
		} else {
			// glTF's default material has metallicFactor=1.
			TiledMeshMaterialPayload default_material;
			default_material.metallic = 1.0;
			surface.material = document.materials.size();
			document.materials.push_back(default_material);
		}
		if (p_primitive.has("targets")) {
			document.warnings.push_back("glTF morph targets are ignored; the base mesh is displayed.");
		}
		surface.estimated_bytes = uint64_t(surface.vertices.size() + surface.normals.size()) * sizeof(Vector3) +
				uint64_t(surface.uv0.size() + surface.uv1.size()) * sizeof(Vector2) + uint64_t(surface.colors.size()) * sizeof(Color) +
				uint64_t(surface.indices.size() + surface.batch_ids.size()) * sizeof(int32_t);
		document.estimated_cpu_bytes += surface.estimated_bytes;
		document.local_aabb = has_aabb ? document.local_aabb.merge(surface.aabb) : surface.aabb;
		has_aabb = true;
		document.surfaces.push_back(surface);
		return true;
	}

bool Decoder::node(Gltf &p_gltf, int p_index, const Matrix &p_parent, HashSet<int> &r_visited, int p_depth) {
		if (p_index < 0 || p_index >= p_gltf.nodes.size() || r_visited.has(p_index) || p_depth > 128) {
			return fail("Invalid, shared or cyclic glTF node hierarchy.");
		}
		r_visited.insert(p_index);
		Dictionary value;
		if (!object(p_gltf.nodes[p_index], value)) {
			return false;
		}
		if (value.has("skin")) {
			return fail("Skinned glTF meshes are not supported in static 3D Tiles.", ERR_UNAVAILABLE);
		}
		Matrix transform;
		if (value.has("matrix")) {
			if (!matrix(value["matrix"], transform)) {
				return false;
			}
		} else {
			Vec translation, scale = { 1, 1, 1 };
			double rotation[4] = { 0, 0, 0, 1 };
			if ((value.has("translation") && !vec(value["translation"], translation)) ||
					(value.has("scale") && !vec(value["scale"], scale)) ||
					(value.has("rotation") && !numbers(value["rotation"], 4, rotation))) {
				return false;
			}
			Quaternion quaternion(rotation[0], rotation[1], rotation[2], rotation[3]);
			if (!quaternion.is_normalized()) {
				return fail("glTF node rotation must be a unit quaternion.");
			}
			const Basis basis(quaternion);
			const double scales[3] = { scale.x, scale.y, scale.z };
			for (int c = 0; c < 3; c++) {
				for (int r = 0; r < 3; r++) {
					transform.m[c * 4 + r] = double(basis.get_column(c)[r]) * scales[c];
				}
			}
			transform.m[12] = translation.x;
			transform.m[13] = translation.y;
			transform.m[14] = translation.z;
		}
		transform = p_parent * transform;
		if (value.has("mesh")) {
			int mesh_index;
			Dictionary mesh;
			Array primitives;
			if (!integer(value["mesh"], mesh_index) || mesh_index >= p_gltf.meshes.size() ||
					!object(p_gltf.meshes[mesh_index], mesh) || !array(mesh.get("primitives", Variant()), primitives)) {
				return fail("Invalid glTF mesh.");
			}
			for (const Variant &entry : primitives) {
				Dictionary primitive_json;
				if (!object(entry, primitive_json) || !primitive(p_gltf, primitive_json, transform)) {
					return false;
				}
			}
		}
		Array children;
		if (!array(value.get("children", Array()), children)) {
			return false;
		}
		for (const Variant &child : children) {
			int child_index;
			if (!integer(child, child_index) || !node(p_gltf, child_index, transform, r_visited, p_depth + 1)) {
				return false;
			}
		}
		return true;
	}

bool Decoder::gltf(const Dictionary &p_json, const PackedByteArray &p_binary, const Vec &p_rtc) {
		Dictionary asset;
		if (!object(p_json.get("asset", Variant()), asset) || String(asset.get("version", "")) != "2.0") {
			return fail("Only glTF 2.0 content is supported.", ERR_UNAVAILABLE);
		}
		if (!required_extensions(p_json, { "CESIUM_RTC", "KHR_draco_mesh_compression", "KHR_mesh_quantization", "KHR_materials_unlit" })) {
			return false;
		}
		Gltf data;
		data.json = p_json;
		Array buffers;
		if (!array(p_json.get("buffers", Array()), buffers) || !array(p_json.get("bufferViews", Array()), data.views) ||
				!array(p_json.get("accessors", Array()), data.accessors) || !array(p_json.get("nodes", Array()), data.nodes) ||
				!array(p_json.get("meshes", Array()), data.meshes) || !array(p_json.get("materials", Array()), data.materials) ||
				!array(p_json.get("textures", Array()), data.textures) || !array(p_json.get("images", Array()), data.images)) {
			return false;
		}
		for (int i = 0; i < buffers.size(); i++) {
			Dictionary buffer;
			int byte_length;
			if (!object(buffers[i], buffer) || !integer(buffer.get("byteLength", Variant()), byte_length, 1)) {
				return false;
			}
			PackedByteArray bytes;
			if (buffer.has("uri")) {
				const String uri = buffer["uri"];
				if (uri.begins_with("data:")) {
					if (!data_uri(uri, bytes)) {
						return false;
					}
				} else {
					const String resolved = TiledMeshDataSource::resolve_uri(gltf_base_uri.is_empty() ? request.uri : gltf_base_uri, uri);
					if (resolved.is_empty()) {
						return fail("Unable to resolve glTF buffer URI.");
					}
					if (const PackedByteArray *cached = external_buffers.getptr(resolved)) {
						bytes = *cached;
					} else {
						if (!dependencies.has(resolved)) {
							dependencies.push_back(resolved);
						}
						data.buffers.push_back(bytes);
						continue;
					}
				}
			} else if (i == 0 && !p_binary.is_empty()) {
				bytes = p_binary;
			} else {
				return fail("glTF buffer has no URI or GLB binary chunk.");
			}
			if (bytes.size() < byte_length) {
				return fail("glTF buffer is shorter than byteLength.");
			}
			// Discard GLB padding so bufferViews cannot address it.
			bytes.resize(byte_length);
			data.buffers.push_back(bytes);
		}
		if (!dependencies.is_empty()) {
			return fail("Waiting for external glTF buffers.", ERR_BUSY);
		}
		Matrix rtc;
		rtc.m[12] = p_rtc.x;
		rtc.m[13] = p_rtc.y;
		rtc.m[14] = p_rtc.z;
		Dictionary extensions;
		if (!object(p_json.get("extensions", Dictionary()), extensions)) {
			return false;
		}
		if (extensions.has("CESIUM_RTC")) {
			Dictionary extension;
			Vec center;
			if (!object(extensions["CESIUM_RTC"], extension) || !vec(extension.get("center", Variant()), center)) {
				return false;
			}
			rtc.m[12] += center.x;
			rtc.m[13] += center.y;
			rtc.m[14] += center.z;
		}
		Matrix y_to_z;
		y_to_z.m[5] = 0;
		y_to_z.m[6] = 1;
		y_to_z.m[9] = -1;
		y_to_z.m[10] = 0;
		const Matrix transform = context.transform * rtc * y_to_z;
		if (request.root && request.context.is_empty() && document.surfaces.is_empty()) {
			const Vec center = context.transform.point({ rtc.m[12], rtc.m[13], rtc.m[14] });
			choose_frame(center, false);
			document.source_origin = options.session.use_format_default_origin ? center : options.session.explicit_origin;
			document.coordinate_frame.origin = document.source_origin;
			document.coordinate_frame.source_to_local = context.frame;
		}
		Array scenes, roots;
		if (!array(p_json.get("scenes", Array()), scenes)) {
			return false;
		}
		if (!scenes.is_empty()) {
			int scene_index;
			Dictionary scene;
			if (!integer(p_json.get("scene", 0), scene_index) || scene_index >= scenes.size() ||
					!object(scenes[scene_index], scene) || !array(scene.get("nodes", Array()), roots)) {
				return fail("Invalid glTF default scene.");
			}
		} else {
			HashSet<int> children;
			for (const Variant &entry : data.nodes) {
				Dictionary value;
				Array child_nodes;
				if (!object(entry, value) || !array(value.get("children", Array()), child_nodes)) {
					return false;
				}
				for (const Variant &child : child_nodes) {
					int index;
					if (!integer(child, index) || index >= data.nodes.size()) {
						return fail("Invalid glTF child node index.");
					}
					children.insert(index);
				}
			}
			for (int i = 0; i < data.nodes.size(); i++) {
				if (!children.has(i)) {
					roots.push_back(i);
				}
			}
		}
		HashSet<int> visited;
		const int previous_surfaces = document.surfaces.size();
		for (const Variant &entry : roots) {
			int index;
			if (!integer(entry, index) || !node(data, index, transform, visited, 0)) {
				return false;
			}
		}
		if (document.surfaces.size() == previous_surfaces) {
			return fail("glTF content contains no renderable triangle meshes.");
		}
		if (p_json.has("animations")) {
			document.warnings.push_back("glTF animations are ignored; static node transforms are displayed.");
		}
		return true;
	}

bool Decoder::decode_glb(const PackedByteArray &p_bytes, const Vec &p_rtc) {
	const uint8_t *bytes = p_bytes.ptr();
	const int size = p_bytes.size();
	if (size < 20 || decode_uint32(bytes + 4) != 2 || decode_uint32(bytes + 8) != uint32_t(size)) {
		return fail("Invalid GLB header; only GLB version 2 is supported.");
	}
	PackedByteArray json_bytes, binary;
	uint64_t offset = 12;
	bool has_binary = false;
	while (offset < uint64_t(size)) {
		if (offset + 8 > uint64_t(size)) {
			return fail("Truncated GLB chunk header.");
		}
		const uint32_t byte_length = decode_uint32(bytes + offset);
		const uint32_t type = decode_uint32(bytes + offset + 4);
		offset += 8;
		if (offset + byte_length > uint64_t(size) || byte_length % 4 != 0) {
			return fail("GLB chunk exceeds the container or is not aligned.");
		}
		if (type == 0x4e4f534a && json_bytes.is_empty() && offset == 20) {
			json_bytes = p_bytes.slice(offset, offset + byte_length);
		} else if (type == 0x004e4942 && !has_binary && !json_bytes.is_empty()) {
			binary = p_bytes.slice(offset, offset + byte_length);
			has_binary = true;
		} else if (type == 0x4e4f534a || type == 0x004e4942 || json_bytes.is_empty()) {
			return fail("Invalid GLB chunk order or duplicate chunk.");
		}
		offset += byte_length;
	}
	Dictionary manifest;
	return json(json_bytes, manifest) && gltf(manifest, binary, p_rtc);
}

} // namespace Tiles3DParserInternal
