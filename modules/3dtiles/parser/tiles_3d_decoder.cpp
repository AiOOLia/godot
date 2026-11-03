/**************************************************************************/
/*  tiles_3d_decoder.cpp                                                  */
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

Vec add(const Vec &a, const Vec &b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}
Vec subtract(const Vec &a, const Vec &b) {
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}
Vec multiply(const Vec &a, double b) {
	return { a.x * b, a.y * b, a.z * b };
}
double length(const Vec &a) {
	return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}
Vector3 vector3(const Vec &a) {
	return Vector3(a.x, a.y, a.z);
}

Vec Matrix::direction(const Vec &p) const {
	return { m[0] * p.x + m[4] * p.y + m[8] * p.z,
			m[1] * p.x + m[5] * p.y + m[9] * p.z,
			m[2] * p.x + m[6] * p.y + m[10] * p.z };
}
Vec Matrix::point(const Vec &p) const {
	return add(direction(p), { m[12], m[13], m[14] });
}
Matrix Matrix::operator*(const Matrix &p_other) const {
	Matrix result;
	for (int c = 0; c < 4; c++) {
		for (int r = 0; r < 4; r++) {
			result.m[c * 4 + r] = 0.0;
			for (int k = 0; k < 4; k++) {
				result.m[c * 4 + r] += m[k * 4 + r] * p_other.m[c * 4 + k];
			}
		}
	}
	return result;
}
Basis Matrix::basis() const {
	return Basis(Vector3(m[0], m[1], m[2]), Vector3(m[4], m[5], m[6]), Vector3(m[8], m[9], m[10]));
}
double Matrix::maximum_scale() const {
	// sqrt(||A^T A||_infinity) bounds the largest singular value and
	// remains conservative for shear as well as nonuniform scale.
	double maximum = 0.0;
	for (int i = 0; i < 3; i++) {
		double sum = 0.0;
		for (int j = 0; j < 3; j++) {
			double dot = 0.0;
			for (int k = 0; k < 3; k++) {
				dot += m[i * 4 + k] * m[j * 4 + k];
			}
			sum += std::abs(dot);
		}
		maximum = MAX(maximum, sum);
	}
	return std::sqrt(maximum);
}
Array Matrix::array() const {
	Array result;
	for (double value : m) {
		result.push_back(value);
	}
	return result;
}

bool Decoder::fail(const String &p_message, Error p_error) {
	message = p_message;
	error = p_error;
	return false;
}
bool Decoder::number(const Variant &p_value, double &r_value) {
	if (p_value.get_type() != Variant::INT && p_value.get_type() != Variant::FLOAT) {
		return fail("Expected a JSON number.");
	}
	r_value = p_value;
	return std::isfinite(r_value) || fail("Non-finite JSON number.");
}
bool Decoder::integer(const Variant &p_value, int &r_value, int p_min) {
	double value;
	if (!number(p_value, value)) {
		return false;
	}
	if (value < p_min || value > INT32_MAX || value != std::floor(value)) {
		return fail("JSON index, count or byte offset is outside its valid range.");
	}
	r_value = int(value);
	return true;
}
bool Decoder::numbers(const Variant &p_value, int p_count, double *r_values) {
	if (p_value.get_type() != Variant::ARRAY) {
		return fail("Expected a JSON numeric array.");
	}
	const Array values = p_value;
	if (values.size() != p_count) {
		return fail("Incorrect numeric array length.");
	}
	for (int i = 0; i < p_count; i++) {
		if (!number(values[i], r_values[i])) {
			return false;
		}
	}
	return true;
}
bool Decoder::vec(const Variant &p_value, Vec &r_value) {
	double values[3];
	if (!numbers(p_value, 3, values)) {
		return false;
	}
	r_value = { values[0], values[1], values[2] };
	return true;
}
bool Decoder::matrix(const Variant &p_value, Matrix &r_value) {
	if (!numbers(p_value, 16, r_value.m)) {
		return false;
	}
	return (r_value.m[3] == 0 && r_value.m[7] == 0 && r_value.m[11] == 0 && r_value.m[15] == 1) || fail("Tile and node transforms must be affine matrices.");
}
bool Decoder::object(const Variant &p_value, Dictionary &r_value) {
	if (p_value.get_type() != Variant::DICTIONARY) {
		return fail("Expected a JSON object.");
	}
	r_value = p_value;
	return true;
}
bool Decoder::array(const Variant &p_value, Array &r_value) {
	if (p_value.get_type() != Variant::ARRAY) {
		return fail("Expected a JSON array.");
	}
	r_value = p_value;
	return true;
}
bool Decoder::json(const PackedByteArray &p_bytes, Dictionary &r_value) {
	JSON parser;
	if (parser.parse(String::utf8(reinterpret_cast<const char *>(p_bytes.ptr()), p_bytes.size())) != OK) {
		return fail(vformat("Invalid JSON at line %d: %s", parser.get_error_line(), parser.get_error_message()));
	}
	return object(parser.get_data(), r_value);
}

bool Decoder::required_extensions(const Dictionary &p_json, const Vector<String> &p_supported) {
	Array required;
	if (!array(p_json.get("extensionsRequired", Array()), required)) {
		return false;
	}
	for (const Variant &value : required) {
		if (value.get_type() != Variant::STRING || !p_supported.has(String(value))) {
			return fail("Unsupported required extension: " + String(value), ERR_UNAVAILABLE);
		}
	}
	return true;
}
Array Decoder::vec_array(const Vec &p_vec) {
	return Array({ p_vec.x, p_vec.y, p_vec.z });
}
PackedByteArray Decoder::encode_context(const Context &p_context) {
	Dictionary data;
	data["transform"] = p_context.transform.array();
	data["center"] = vec_array(p_context.center);
	Array frame;
	for (int i = 0; i < 3; i++) {
		const Vector3 column = p_context.frame.get_column(i);
		frame.push_back(column.x);
		frame.push_back(column.y);
		frame.push_back(column.z);
	}
	data["frame"] = frame;
	data["key"] = p_context.key;
	data["ancestors"] = p_context.ancestors;
	data["refine"] = int(p_context.refinement);
	return JSON::stringify(data, "", false, true).to_utf8_buffer();
}
bool Decoder::decode_context() {
	context.key = request.uri;
	if (request.context.is_empty()) {
		return true;
	}
	Dictionary data;
	double frame[9];
	int refinement;
	if (!json(request.context, data) || !matrix(data.get("transform", Variant()), context.transform) ||
			!vec(data.get("center", Variant()), context.center) || !numbers(data.get("frame", Variant()), 9, frame) ||
			!array(data.get("ancestors", Variant()), context.ancestors) || !integer(data.get("refine", Variant()), refinement) || refinement > 1) {
		return fail("Invalid 3D Tiles content context.");
	}
	context.frame = Basis(Vector3(frame[0], frame[1], frame[2]), Vector3(frame[3], frame[4], frame[5]), Vector3(frame[6], frame[7], frame[8]));
	context.refinement = TiledMeshTileDefinition::RefinementMode(refinement);
	context.key = data.get("key", request.uri);
	return true;
}
void Decoder::choose_frame(const Vec &p_center, bool p_region) {
	const bool ecef = options.coordinate_system == Tiles3DOptions::COORDINATE_SYSTEM_ECEF ||
			(options.coordinate_system == Tiles3DOptions::COORDINATE_SYSTEM_AUTO && (p_region || length(p_center) > 1000000.0));
	if (!ecef) {
		return;
	}
	const double longitude = std::atan2(p_center.y, p_center.x);
	const double xy = std::hypot(p_center.x, p_center.y);
	const double eccentricity_squared = 6.6943799901413165e-3;
	double latitude = std::atan2(p_center.z, xy * (1.0 - eccentricity_squared));
	for (int i = 0; i < 8; i++) {
		const double sin_lat = std::sin(latitude);
		const double normal = 6378137.0 / std::sqrt(1.0 - eccentricity_squared * sin_lat * sin_lat);
		latitude = std::atan2(p_center.z + eccentricity_squared * normal * sin_lat, xy);
	}
	const double slon = std::sin(longitude), clon = std::cos(longitude);
	const double slat = std::sin(latitude), clat = std::cos(latitude);
	// Local X=east, Y=up, Z=south (Godot's forward is -Z).
	context.frame = Basis(Vector3(-slon, clat * clon, slat * clon),
			Vector3(clon, clat * slon, slat * slon), Vector3(0, slat, -clat));
}

Vector3 Decoder::local(const Vec &p_position) const {
	return context.frame.xform(vector3(multiply(subtract(p_position, document.source_origin), options.session.unit_scale)));
}

Decoder::Decoder(const TiledMeshParseRequest &p_request, const Tiles3DParseOptions &p_options,
		const HashMap<String, PackedByteArray> &p_buffers, TiledMeshDocument &r_document, Vector<String> &r_dependencies, String &r_error) :
		request(p_request), options(p_options), external_buffers(p_buffers), document(r_document), dependencies(r_dependencies), message(r_error) {}

Error Decoder::parse() {
	if (!std::isfinite(options.session.unit_scale) || options.session.unit_scale <= 0 || !decode_context()) {
		return ERR_INVALID_PARAMETER;
	}
	document.source_origin = context.center;
	document.coordinate_frame.origin = context.center;
	document.coordinate_frame.source_to_local = context.frame;
	document.coordinate_frame.unit_scale = options.session.unit_scale;
	if (!content(request.bytes, Vec())) {
		document = TiledMeshDocument();
		return error;
	}
	return OK;
}

} // namespace Tiles3DParserInternal
