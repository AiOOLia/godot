/**************************************************************************/
/*  draco_decoder.cpp                                                     */
/**************************************************************************/

#include "draco_decoder.h"

#include "draco/compression/decode.h"

#include <cmath>
#include <memory>

Error DracoDecoder::decode(const uint8_t *p_data, uint64_t p_size, Geometry &r_geometry, String &r_error, const Limits &p_limits) {
	r_geometry = Geometry();
	r_error.clear();
	if (!p_data || p_size == 0) {
		r_error = "Draco input is empty.";
		return ERR_INVALID_PARAMETER;
	}
	if (p_size > p_limits.max_input_bytes || p_size > INT32_MAX) {
		r_error = "Draco input exceeds the configured byte limit.";
		return ERR_OUT_OF_MEMORY;
	}

	draco::DecoderBuffer buffer;
	buffer.Init(reinterpret_cast<const char *>(p_data), size_t(p_size));
	auto type = draco::Decoder::GetEncodedGeometryType(&buffer);
	if (!type.ok()) {
		r_error = String::utf8(type.status().error_msg());
		return ERR_FILE_CORRUPT;
	}
	buffer.Init(reinterpret_cast<const char *>(p_data), size_t(p_size));
	draco::Decoder decoder;
	std::unique_ptr<draco::PointCloud> geometry;
	if (type.value() == draco::TRIANGULAR_MESH) {
		auto decoded = decoder.DecodeMeshFromBuffer(&buffer);
		if (!decoded.ok()) {
			r_error = String::utf8(decoded.status().error_msg());
			return ERR_FILE_CORRUPT;
		}
		geometry = std::move(decoded).value();
		r_geometry.is_mesh = true;
	} else if (type.value() == draco::POINT_CLOUD) {
		auto decoded = decoder.DecodePointCloudFromBuffer(&buffer);
		if (!decoded.ok()) {
			r_error = String::utf8(decoded.status().error_msg());
			return ERR_FILE_CORRUPT;
		}
		geometry = std::move(decoded).value();
	} else {
		r_error = "Draco stream has an unsupported geometry type.";
		return ERR_UNAVAILABLE;
	}

	const uint64_t point_count = geometry->num_points();
	if (point_count > p_limits.max_points || point_count > INT32_MAX) {
		r_error = "Decoded Draco point count exceeds the configured limit.";
		return ERR_OUT_OF_MEMORY;
	}
	r_geometry.point_count = int(point_count);

	if (r_geometry.is_mesh) {
		const draco::Mesh *mesh = static_cast<const draco::Mesh *>(geometry.get());
		const uint64_t face_count = mesh->num_faces();
		if (face_count > p_limits.max_faces || face_count > uint64_t(INT32_MAX) / 3) {
			r_error = "Decoded Draco face count exceeds the configured limit.";
			return ERR_OUT_OF_MEMORY;
		}
		r_geometry.face_count = int(face_count);
		r_geometry.indices.resize(int(face_count * 3));
		int32_t *indices = r_geometry.indices.ptrw();
		for (uint64_t i = 0; i < face_count; i++) {
			const draco::Mesh::Face &face = mesh->face(draco::FaceIndex(uint32_t(i)));
			indices[i * 3] = face[0].value();
			indices[i * 3 + 1] = face[1].value();
			indices[i * 3 + 2] = face[2].value();
		}
	}

	uint64_t total_components = 0;
	for (int i = 0; i < geometry->num_attributes(); i++) {
		const draco::PointAttribute *source = geometry->attribute(i);
		if (source->num_components() == 0 || point_count > UINT64_MAX / source->num_components()) {
			r_error = "Decoded Draco attribute component count overflows.";
			return ERR_OUT_OF_MEMORY;
		}
		const uint64_t value_count = point_count * source->num_components();
		if (total_components > p_limits.max_attribute_components || value_count > p_limits.max_attribute_components - total_components || value_count > INT32_MAX) {
			r_error = "Decoded Draco attributes exceed the configured component limit.";
			return ERR_OUT_OF_MEMORY;
		}
		total_components += value_count;
		Attribute attribute;
		attribute.unique_id = int(source->unique_id());
		attribute.semantic = int(source->attribute_type());
		attribute.component_type = int(source->data_type());
		attribute.component_count = int(source->num_components());
		attribute.normalized = source->normalized();
		attribute.values.resize(int(value_count));
		double *values = attribute.values.ptrw();
		for (uint64_t point = 0; point < point_count; point++) {
			if (!source->ConvertValue<double>(source->mapped_index(draco::PointIndex(uint32_t(point))), values + point * attribute.component_count)) {
				r_error = "Unable to convert a decoded Draco attribute value.";
				return ERR_FILE_CORRUPT;
			}
			for (int component = 0; component < attribute.component_count; component++) {
				if (!std::isfinite(values[point * attribute.component_count + component])) {
					r_error = vformat("Decoded Draco attribute %d contains a non-finite value at point %d component %d.", attribute.unique_id, point, component);
					return ERR_FILE_CORRUPT;
				}
			}
		}
		if (r_geometry.attributes.has(attribute.unique_id)) {
			r_error = "Decoded Draco attributes contain duplicate unique IDs.";
			return ERR_FILE_CORRUPT;
		}
		r_geometry.attributes.insert(attribute.unique_id, attribute);
	}
	return OK;
}
