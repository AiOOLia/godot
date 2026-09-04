/**************************************************************************/
/*  osg_scene_converter.cpp                                               */
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

#include "osg_scene_converter.h"

#include "osg_data_source.h"

#include <osg/AlphaFunc>
#include <osg/Array>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/Image>
#include <osg/LOD>
#include <osg/Material>
#include <osg/PagedLOD>
#include <osg/ProxyNode>
#include <osg/StateSet>
#include <osg/Texture2D>
#include <osg/Transform>
#include <osgDB/Options.h>
#include <osgDB/ReaderWriter.h>
#include <osgDB/Registry.h>

#include <cmath>
#include <limits>
#include <set>
#include <sstream>
#include <vector>

namespace {

struct RawMaterial {
	Color albedo = Color(1, 1, 1, 1);
	bool transparent = false;
	// OpenGL's default is GL_CULL_FACE disabled. OSG StateSet::INHERIT at the
	// root therefore means two-sided rendering, not Godot's default back-face
	// culling. Photogrammetry tiles commonly rely on this default and may contain
	// locally inconsistent winding.
	bool double_sided = true;
	bool alpha_test = false;
	float alpha_cutoff = 0.5f;
	const osg::Image *image = nullptr;
};

struct RawSurface {
	std::vector<osg::Vec3d> vertices;
	std::vector<osg::Vec3d> normals;
	std::vector<osg::Vec2d> uv0;
	std::vector<Color> colors;
	std::vector<int32_t> indices;
	RawMaterial material;
	int fallback_group = -1;
	int lod_branch = -1;
};

struct ConvertContext {
	String source_uri;
	OsgSceneConvertOptions options;
	std::vector<RawSurface> surfaces;
	std::vector<RawMaterial> materials;
	Vector<OsgExternalTileRef> external_tiles;
	Vector<OsgLodRangePayload> lod_ranges;
	Vector<String> warnings;
	std::set<std::string> warning_keys;
	osg::Vec3d minimum = osg::Vec3d(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
	osg::Vec3d maximum = osg::Vec3d(-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity());
	bool has_bounds = false;
	int next_fallback_group = 0;
};

static bool finite_vec3(const osg::Vec3d &p_value) {
	return std::isfinite(p_value.x()) && std::isfinite(p_value.y()) && std::isfinite(p_value.z());
}

static void add_warning(ConvertContext &r_context, const String &p_key, const String &p_message) {
	const CharString key_utf8 = p_key.utf8();
	if (r_context.warning_keys.insert(key_utf8.get_data()).second) {
		r_context.warnings.push_back(p_message);
	}
}

static void expand_bounds(ConvertContext &r_context, const osg::Vec3d &p_position) {
	if (!finite_vec3(p_position)) {
		return;
	}
	r_context.has_bounds = true;
	for (int axis = 0; axis < 3; axis++) {
		r_context.minimum[axis] = MIN(r_context.minimum[axis], p_position[axis]);
		r_context.maximum[axis] = MAX(r_context.maximum[axis], p_position[axis]);
	}
}

static bool read_vec3(const osg::Array *p_array, unsigned int p_index, osg::Vec3d &r_value) {
	if (!p_array || p_index >= p_array->getNumElements()) {
		return false;
	}
	if (const osg::Vec3Array *array = dynamic_cast<const osg::Vec3Array *>(p_array)) {
		const osg::Vec3 &v = (*array)[p_index];
		r_value.set(v.x(), v.y(), v.z());
		return finite_vec3(r_value);
	}
	if (const osg::Vec3dArray *array = dynamic_cast<const osg::Vec3dArray *>(p_array)) {
		r_value = (*array)[p_index];
		return finite_vec3(r_value);
	}
	return false;
}

static bool read_vec2(const osg::Array *p_array, unsigned int p_index, osg::Vec2d &r_value) {
	if (!p_array || p_index >= p_array->getNumElements()) {
		return false;
	}
	if (const osg::Vec2Array *array = dynamic_cast<const osg::Vec2Array *>(p_array)) {
		const osg::Vec2 &v = (*array)[p_index];
		r_value.set(v.x(), v.y());
		return std::isfinite(r_value.x()) && std::isfinite(r_value.y());
	}
	if (const osg::Vec2dArray *array = dynamic_cast<const osg::Vec2dArray *>(p_array)) {
		r_value = (*array)[p_index];
		return std::isfinite(r_value.x()) && std::isfinite(r_value.y());
	}
	return false;
}

static bool read_color(const osg::Array *p_array, unsigned int p_index, Color &r_color) {
	if (!p_array || p_index >= p_array->getNumElements()) {
		return false;
	}
	if (const osg::Vec4Array *array = dynamic_cast<const osg::Vec4Array *>(p_array)) {
		const osg::Vec4 &v = (*array)[p_index];
		r_color = Color(v.r(), v.g(), v.b(), v.a());
		return true;
	}
	if (const osg::Vec4ubArray *array = dynamic_cast<const osg::Vec4ubArray *>(p_array)) {
		const osg::Vec4ub &v = (*array)[p_index];
		r_color = Color(float(v.r()) / 255.0f, float(v.g()) / 255.0f, float(v.b()) / 255.0f, float(v.a()) / 255.0f);
		return true;
	}
	if (const osg::Vec3Array *array = dynamic_cast<const osg::Vec3Array *>(p_array)) {
		const osg::Vec3 &v = (*array)[p_index];
		r_color = Color(v.x(), v.y(), v.z(), 1.0f);
		return true;
	}
	return false;
}

static double matrix_determinant_3x3(const osg::Matrixd &p_matrix) {
	return p_matrix(0, 0) * (p_matrix(1, 1) * p_matrix(2, 2) - p_matrix(1, 2) * p_matrix(2, 1)) -
			p_matrix(0, 1) * (p_matrix(1, 0) * p_matrix(2, 2) - p_matrix(1, 2) * p_matrix(2, 0)) +
			p_matrix(0, 2) * (p_matrix(1, 0) * p_matrix(2, 1) - p_matrix(1, 1) * p_matrix(2, 0));
}

static RawMaterial apply_state_set(const osg::StateSet *p_state_set, const RawMaterial &p_parent) {
	RawMaterial state = p_parent;
	if (!p_state_set) {
		return state;
	}

	if (const osg::Material *material = dynamic_cast<const osg::Material *>(p_state_set->getAttribute(osg::StateAttribute::MATERIAL))) {
		const osg::Vec4 &diffuse = material->getDiffuse(osg::Material::FRONT);
		state.albedo = Color(diffuse.r(), diffuse.g(), diffuse.b(), diffuse.a());
		state.transparent = state.transparent || diffuse.a() < 0.999f;
	}
	if (const osg::AlphaFunc *alpha = dynamic_cast<const osg::AlphaFunc *>(p_state_set->getAttribute(osg::StateAttribute::ALPHAFUNC))) {
		state.alpha_test = alpha->getFunction() != osg::AlphaFunc::ALWAYS;
		state.alpha_cutoff = alpha->getReferenceValue();
	}
	const osg::StateAttribute::GLModeValue blend = p_state_set->getMode(GL_BLEND);
	if (blend != osg::StateAttribute::INHERIT) {
		state.transparent = (blend & osg::StateAttribute::ON) != 0;
	}
	if (p_state_set->getRenderingHint() == osg::StateSet::TRANSPARENT_BIN) {
		state.transparent = true;
	}
	const osg::StateAttribute::GLModeValue cull = p_state_set->getMode(GL_CULL_FACE);
	if (cull != osg::StateAttribute::INHERIT) {
		state.double_sided = (cull & osg::StateAttribute::ON) == 0;
	}
	if (const osg::Texture2D *texture = dynamic_cast<const osg::Texture2D *>(p_state_set->getTextureAttribute(0, osg::StateAttribute::TEXTURE))) {
		state.image = texture->getImage();
	}
	return state;
}

static bool append_triangle(std::vector<int32_t> &r_indices, uint32_t p_a, uint32_t p_b, uint32_t p_c, uint32_t p_vertex_count, bool p_reverse) {
	if (p_a >= p_vertex_count || p_b >= p_vertex_count || p_c >= p_vertex_count || p_a == p_b || p_b == p_c || p_c == p_a) {
		return false;
	}
	if (p_reverse) {
		std::swap(p_b, p_c);
	}
	r_indices.push_back((int32_t)p_a);
	r_indices.push_back((int32_t)p_b);
	r_indices.push_back((int32_t)p_c);
	return true;
}

static bool triangulate_sequence(GLenum p_mode, const std::vector<uint32_t> &p_sequence, uint32_t p_vertex_count, bool p_reverse, std::vector<int32_t> &r_indices) {
	bool added = false;
	if (p_mode == GL_TRIANGLES) {
		for (size_t i = 0; i + 2 < p_sequence.size(); i += 3) {
			added |= append_triangle(r_indices, p_sequence[i], p_sequence[i + 1], p_sequence[i + 2], p_vertex_count, p_reverse);
		}
	} else if (p_mode == GL_TRIANGLE_STRIP) {
		for (size_t i = 0; i + 2 < p_sequence.size(); i++) {
			const bool odd = (i & 1) != 0;
			added |= append_triangle(r_indices, p_sequence[i + (odd ? 1 : 0)], p_sequence[i + (odd ? 0 : 1)], p_sequence[i + 2], p_vertex_count, p_reverse);
		}
	} else if (p_mode == GL_TRIANGLE_FAN) {
		for (size_t i = 1; i + 1 < p_sequence.size(); i++) {
			added |= append_triangle(r_indices, p_sequence[0], p_sequence[i], p_sequence[i + 1], p_vertex_count, p_reverse);
		}
	}
	return added;
}

static void append_primitive_set(const osg::PrimitiveSet *p_primitive, uint32_t p_vertex_count, bool p_reverse, RawSurface &r_surface, ConvertContext &r_context) {
	if (!p_primitive) {
		return;
	}
	const GLenum mode = p_primitive->getMode();
	if (mode != GL_TRIANGLES && mode != GL_TRIANGLE_STRIP && mode != GL_TRIANGLE_FAN) {
		add_warning(r_context, vformat("primitive_%d", mode), vformat("Unsupported OSG primitive mode %d was skipped.", mode));
		return;
	}

	if (const osg::DrawArrayLengths *lengths = dynamic_cast<const osg::DrawArrayLengths *>(p_primitive)) {
		uint32_t first = MAX(lengths->getFirst(), 0);
		for (osg::DrawArrayLengths::const_iterator it = lengths->begin(); it != lengths->end(); ++it) {
			std::vector<uint32_t> sequence;
			sequence.reserve(MAX(*it, 0));
			for (GLsizei i = 0; i < *it; i++) {
				sequence.push_back(first + i);
			}
			triangulate_sequence(mode, sequence, p_vertex_count, p_reverse, r_surface.indices);
			first += MAX(*it, 0);
		}
		return;
	}

	std::vector<uint32_t> sequence;
	sequence.reserve(p_primitive->getNumIndices());
	for (unsigned int i = 0; i < p_primitive->getNumIndices(); i++) {
		sequence.push_back(p_primitive->index(i));
	}
	triangulate_sequence(mode, sequence, p_vertex_count, p_reverse, r_surface.indices);
}

static void process_geometry(const osg::Geometry *p_geometry, const osg::Matrixd &p_matrix, const RawMaterial &p_parent_material, int p_fallback_group, int p_lod_branch, ConvertContext &r_context) {
	const osg::Array *vertex_array = p_geometry->getVertexArray();
	if (!vertex_array || vertex_array->getNumElements() == 0) {
		return;
	}
	RawSurface surface;
	surface.fallback_group = p_fallback_group;
	surface.lod_branch = p_lod_branch;
	surface.material = apply_state_set(p_geometry->getStateSet(), p_parent_material);
	const uint32_t vertex_count = vertex_array->getNumElements();
	surface.vertices.resize(vertex_count);

	for (uint32_t i = 0; i < vertex_count; i++) {
		osg::Vec3d position;
		if (!read_vec3(vertex_array, i, position)) {
			add_warning(r_context, "invalid_vertices", "A geometry containing unsupported or non-finite vertices was skipped.");
			return;
		}
		position = position * p_matrix;
		if (!finite_vec3(position)) {
			add_warning(r_context, "invalid_transformed_vertices", "A geometry produced non-finite transformed vertices and was skipped.");
			return;
		}
		surface.vertices[i] = position;
	}

	// The OSG Z-up to Godot Y-up mapping contains one reflection. A mirrored
	// node transform contributes a second reflection, cancelling the first.
	// In native Godot Y-up mode only the node transform affects winding.
	const bool reverse_winding = r_context.options.osg_z_up != (matrix_determinant_3x3(p_matrix) < 0.0);
	for (unsigned int primitive_index = 0; primitive_index < p_geometry->getNumPrimitiveSets(); primitive_index++) {
		append_primitive_set(p_geometry->getPrimitiveSet(primitive_index), vertex_count, reverse_winding, surface, r_context);
	}
	if (surface.indices.empty()) {
		return;
	}
	for (const osg::Vec3d &position : surface.vertices) {
		expand_bounds(r_context, position);
	}

	const osg::Array *normal_array = p_geometry->getNormalArray();
	if (normal_array && (normal_array->getBinding() == osg::Array::BIND_PER_VERTEX || normal_array->getBinding() == osg::Array::BIND_OVERALL)) {
		const bool overall = normal_array->getBinding() == osg::Array::BIND_OVERALL;
		surface.normals.resize(vertex_count);
		const osg::Matrixd inverse = osg::Matrixd::inverse(p_matrix);
		for (uint32_t i = 0; i < vertex_count; i++) {
			osg::Vec3d normal;
			if (!read_vec3(normal_array, overall ? 0 : i, normal)) {
				surface.normals.clear();
				break;
			}
			normal = osg::Matrixd::transform3x3(inverse, normal);
			normal.normalize();
			surface.normals[i] = normal;
		}
	}

	const osg::Array *uv_array = p_geometry->getTexCoordArray(0);
	if (uv_array && uv_array->getNumElements() >= vertex_count) {
		surface.uv0.resize(vertex_count);
		for (uint32_t i = 0; i < vertex_count; i++) {
			if (!read_vec2(uv_array, i, surface.uv0[i])) {
				surface.uv0.clear();
				break;
			}
		}
	}

	const osg::Array *color_array = p_geometry->getColorArray();
	if (color_array && (color_array->getBinding() == osg::Array::BIND_PER_VERTEX || color_array->getBinding() == osg::Array::BIND_OVERALL)) {
		const bool overall = color_array->getBinding() == osg::Array::BIND_OVERALL;
		surface.colors.resize(vertex_count);
		for (uint32_t i = 0; i < vertex_count; i++) {
			if (!read_color(color_array, overall ? 0 : i, surface.colors[i])) {
				surface.colors.clear();
				break;
			}
		}
	}

	r_context.surfaces.push_back(surface);
}

static double maximum_scale(const osg::Matrixd &p_matrix) {
	double scale = 0.0;
	for (int row = 0; row < 3; row++) {
		double length_squared = 0.0;
		for (int column = 0; column < 3; column++) {
			length_squared += p_matrix(row, column) * p_matrix(row, column);
		}
		scale = MAX(scale, std::sqrt(length_squared));
	}
	return scale;
}

static void collect_external_tiles(const osg::Node *p_node, const osg::Matrixd &p_matrix, int p_fallback_group, ConvertContext &r_context) {
	if (const osg::PagedLOD *paged = dynamic_cast<const osg::PagedLOD *>(p_node)) {
		const osg::Vec3d center = paged->getCenter() * p_matrix;
		for (unsigned int i = 0; i < paged->getNumFileNames(); i++) {
			if (paged->getFileName(i).empty()) {
				continue;
			}
			OsgExternalTileRef reference;
			reference.uri = OsgDataSource::resolve_uri(r_context.source_uri, String::utf8((paged->getDatabasePath() + paged->getFileName(i)).c_str()));
			reference.center = { center.x(), center.y(), center.z() };
			reference.radius = paged->getRadius() * maximum_scale(p_matrix);
			// A paging-only document often has no resident geometry of its own.
			// Include the declared bounding sphere in the source bounds so the
			// dataset still has a useful frame/selection box and root origin.
			const double bound_radius = MAX(reference.radius, 0.0);
			for (int axis = 0; axis < 3; axis++) {
				r_context.minimum[axis] = MIN(r_context.minimum[axis], center[axis] - bound_radius);
				r_context.maximum[axis] = MAX(r_context.maximum[axis], center[axis] + bound_radius);
			}
			r_context.has_bounds = true;
			if (i < paged->getNumRanges()) {
				reference.min_range = paged->getMinRange(i);
				reference.max_range = paged->getMaxRange(i);
			}
			reference.range_mode = paged->getRangeMode() == osg::LOD::PIXEL_SIZE_ON_SCREEN ? OsgLodRangePayload::PIXEL_SIZE_ON_SCREEN : OsgLodRangePayload::DISTANCE_FROM_EYE_POINT;
			reference.fallback_group = p_fallback_group;
			r_context.external_tiles.push_back(reference);
		}
	}
	if (const osg::ProxyNode *proxy = dynamic_cast<const osg::ProxyNode *>(p_node)) {
		const osg::Vec3d center = proxy->getCenter() * p_matrix;
		for (unsigned int i = 0; i < proxy->getNumFileNames(); i++) {
			if (proxy->getFileName(i).empty()) {
				continue;
			}
			OsgExternalTileRef reference;
			reference.uri = OsgDataSource::resolve_uri(r_context.source_uri, String::utf8((proxy->getDatabasePath() + proxy->getFileName(i)).c_str()));
			reference.center = { center.x(), center.y(), center.z() };
			reference.radius = proxy->getRadius() * maximum_scale(p_matrix);
			const double bound_radius = MAX(reference.radius, 0.0);
			for (int axis = 0; axis < 3; axis++) {
				r_context.minimum[axis] = MIN(r_context.minimum[axis], center[axis] - bound_radius);
				r_context.maximum[axis] = MAX(r_context.maximum[axis], center[axis] + bound_radius);
			}
			r_context.has_bounds = true;
			reference.min_range = 0.0f;
			reference.max_range = std::numeric_limits<float>::max();
			reference.fallback_group = p_fallback_group;
			r_context.external_tiles.push_back(reference);
		}
	}
}

static int append_lod_branch(const osg::LOD *p_lod, unsigned int p_child, const osg::Matrixd &p_matrix, int p_parent_branch, ConvertContext &r_context) {
	OsgLodRangePayload branch;
	branch.parent = p_parent_branch;
	const osg::Vec3d center = p_lod->getCenter() * p_matrix;
	branch.center = { center.x(), center.y(), center.z() };
	branch.radius = p_lod->getRadius() * maximum_scale(p_matrix);
	if (p_child < p_lod->getNumRanges()) {
		branch.min_range = p_lod->getMinRange(p_child);
		branch.max_range = p_lod->getMaxRange(p_child);
	} else {
		branch.min_range = 0.0f;
		branch.max_range = std::numeric_limits<float>::max();
	}
	branch.range_mode = p_lod->getRangeMode() == osg::LOD::PIXEL_SIZE_ON_SCREEN ? OsgLodRangePayload::PIXEL_SIZE_ON_SCREEN : OsgLodRangePayload::DISTANCE_FROM_EYE_POINT;
	r_context.lod_ranges.push_back(branch);
	return r_context.lod_ranges.size() - 1;
}

static void traverse_node(const osg::Node *p_node, const osg::Matrixd &p_parent_matrix, const RawMaterial &p_parent_material, int p_fallback_group, int p_lod_branch, ConvertContext &r_context) {
	if (!p_node) {
		return;
	}
	osg::Matrixd matrix = p_parent_matrix;
	if (const osg::Transform *transform = dynamic_cast<const osg::Transform *>(p_node)) {
		transform->computeLocalToWorldMatrix(matrix, nullptr);
	}
	const RawMaterial material = apply_state_set(p_node->getStateSet(), p_parent_material);
	int child_fallback_group = p_fallback_group;
	const osg::PagedLOD *paged = dynamic_cast<const osg::PagedLOD *>(p_node);
	const osg::ProxyNode *proxy = dynamic_cast<const osg::ProxyNode *>(p_node);
	if ((paged && paged->getNumFileNames() > 0) || (proxy && proxy->getNumFileNames() > 0)) {
		child_fallback_group = r_context.next_fallback_group++;
	}
	collect_external_tiles(p_node, matrix, child_fallback_group, r_context);

	if (const osg::Geometry *geometry = p_node->asDrawable() ? p_node->asDrawable()->asGeometry() : nullptr) {
		process_geometry(geometry, matrix, material, p_fallback_group, p_lod_branch, r_context);
	}
	if (const osg::Geode *geode = dynamic_cast<const osg::Geode *>(p_node)) {
		for (unsigned int i = 0; i < geode->getNumDrawables(); i++) {
			const osg::Drawable *drawable = geode->getDrawable(i);
			if (const osg::Geometry *geometry = drawable ? drawable->asGeometry() : nullptr) {
				process_geometry(geometry, matrix, material, p_fallback_group, p_lod_branch, r_context);
			} else if (drawable) {
				add_warning(r_context, String::utf8(drawable->className()), vformat("Unsupported OSG drawable '%s' was skipped.", String::utf8(drawable->className())));
			}
		}
		return;
	}
	if (const osg::Group *group = p_node->asGroup()) {
		const osg::LOD *lod = dynamic_cast<const osg::LOD *>(p_node);
		for (unsigned int i = 0; i < group->getNumChildren(); i++) {
			const int child_lod_branch = lod ? append_lod_branch(lod, i, matrix, p_lod_branch, r_context) : p_lod_branch;
			traverse_node(group->getChild(i), matrix, material, child_fallback_group, child_lod_branch, r_context);
		}
	}
}

static Vector3 source_to_local(const osg::Vec3d &p_source, const OsgVector3d &p_origin, const OsgSceneConvertOptions &p_options) {
	const double x = (p_source.x() - p_origin.x) * p_options.unit_scale;
	const double y = (p_source.y() - p_origin.y) * p_options.unit_scale;
	const double z = (p_source.z() - p_origin.z) * p_options.unit_scale;
	return p_options.osg_z_up ? Vector3(x, z, -y) : Vector3(x, y, z);
}

static Vector3 source_normal_to_local(const osg::Vec3d &p_source, bool p_osg_z_up) {
	Vector3 result = p_osg_z_up ? Vector3(p_source.x(), p_source.z(), -p_source.y()) : Vector3(p_source.x(), p_source.y(), p_source.z());
	return result.normalized();
}

static void generate_normals(OsgMeshSurfacePayload &r_surface) {
	r_surface.normals.resize(r_surface.vertices.size());
	for (int i = 0; i + 2 < r_surface.indices.size(); i += 3) {
		const int32_t ia = r_surface.indices[i];
		const int32_t ib = r_surface.indices[i + 1];
		const int32_t ic = r_surface.indices[i + 2];
		const Vector3 normal = (r_surface.vertices[ib] - r_surface.vertices[ia]).cross(r_surface.vertices[ic] - r_surface.vertices[ia]);
		r_surface.normals.set(ia, r_surface.normals[ia] + normal);
		r_surface.normals.set(ib, r_surface.normals[ib] + normal);
		r_surface.normals.set(ic, r_surface.normals[ic] + normal);
	}
	for (int i = 0; i < r_surface.normals.size(); i++) {
		r_surface.normals.set(i, r_surface.normals[i].normalized());
	}
}

static void copy_image_payload(const osg::Image *p_image, const String &p_source_uri, OsgImagePayload &r_payload, ConvertContext &r_context) {
	if (!p_image) {
		return;
	}
	if (!p_image->getFileName().empty()) {
		r_payload.uri = OsgDataSource::resolve_uri(p_source_uri, String::utf8(p_image->getFileName().c_str()));
	}
	if (!p_image->valid()) {
		return;
	}
	if (p_image->getDataType() != GL_UNSIGNED_BYTE || p_image->r() != 1 || p_image->isCompressed()) {
		add_warning(r_context, "unsupported_image_pixels", "An embedded image with an unsupported pixel format was left as a URI-only texture.");
		return;
	}
	const GLenum format = p_image->getPixelFormat();
	if (format != GL_RGBA && format != GL_BGRA && format != GL_RGB && format != GL_BGR && format != GL_LUMINANCE && format != GL_LUMINANCE_ALPHA && format != GL_ALPHA) {
		add_warning(r_context, vformat("image_format_%d", format), vformat("Embedded image pixel format %d is not supported.", format));
		return;
	}
	r_payload.width = p_image->s();
	r_payload.height = p_image->t();
	r_payload.rgba8.resize(r_payload.width * r_payload.height * 4);
	uint8_t *destination = r_payload.rgba8.ptrw();
	for (int y = 0; y < r_payload.height; y++) {
		const int source_y = p_image->getOrigin() == osg::Image::BOTTOM_LEFT ? r_payload.height - 1 - y : y;
		const uint8_t *source = p_image->data(0, source_y);
		for (int x = 0; x < r_payload.width; x++) {
			uint8_t r = 255;
			uint8_t g = 255;
			uint8_t b = 255;
			uint8_t a = 255;
			if (format == GL_RGBA) {
				r = source[x * 4 + 0]; g = source[x * 4 + 1]; b = source[x * 4 + 2]; a = source[x * 4 + 3];
			} else if (format == GL_BGRA) {
				b = source[x * 4 + 0]; g = source[x * 4 + 1]; r = source[x * 4 + 2]; a = source[x * 4 + 3];
			} else if (format == GL_RGB) {
				r = source[x * 3 + 0]; g = source[x * 3 + 1]; b = source[x * 3 + 2];
			} else if (format == GL_BGR) {
				b = source[x * 3 + 0]; g = source[x * 3 + 1]; r = source[x * 3 + 2];
			} else if (format == GL_LUMINANCE) {
				r = g = b = source[x];
			} else if (format == GL_LUMINANCE_ALPHA) {
				r = g = b = source[x * 2]; a = source[x * 2 + 1];
			} else if (format == GL_ALPHA) {
				a = source[x];
			}
			const int offset = (y * r_payload.width + x) * 4;
			destination[offset + 0] = r;
			destination[offset + 1] = g;
			destination[offset + 2] = b;
			destination[offset + 3] = a;
		}
	}
}

static OsgMaterialPayload make_material_payload(const RawMaterial &p_raw, ConvertContext &r_context) {
	OsgMaterialPayload material;
	material.albedo = p_raw.albedo;
	material.transparent = p_raw.transparent;
	material.double_sided = p_raw.double_sided;
	material.alpha_test = p_raw.alpha_test;
	material.alpha_cutoff = p_raw.alpha_cutoff;
	copy_image_payload(p_raw.image, r_context.source_uri, material.albedo_image, r_context);
	return material;
}

static int find_or_add_material(const RawMaterial &p_material, ConvertContext &r_context, OsgTileDocument &r_document) {
	for (int i = 0; i < (int)r_context.materials.size(); i++) {
		const RawMaterial &existing = r_context.materials[i];
		if (existing.albedo == p_material.albedo && existing.transparent == p_material.transparent &&
				existing.double_sided == p_material.double_sided && existing.alpha_test == p_material.alpha_test &&
				existing.alpha_cutoff == p_material.alpha_cutoff && existing.image == p_material.image) {
			return i;
		}
	}
	r_context.materials.push_back(p_material);
	r_document.materials.push_back(make_material_payload(p_material, r_context));
	return r_document.materials.size() - 1;
}

static bool merge_compatible_surface(const OsgMeshSurfacePayload &p_surface, OsgTileDocument &r_document) {
	for (OsgMeshSurfacePayload &existing : r_document.surfaces) {
		if (existing.material != p_surface.material || existing.fallback_group != p_surface.fallback_group || existing.lod_branch != p_surface.lod_branch ||
				existing.normals.is_empty() != p_surface.normals.is_empty() || existing.uv0.is_empty() != p_surface.uv0.is_empty() || existing.colors.is_empty() != p_surface.colors.is_empty()) {
			continue;
		}
		const int vertex_offset = existing.vertices.size();
		if (vertex_offset > INT32_MAX - p_surface.vertices.size()) {
			continue;
		}
		const int old_vertex_count = existing.vertices.size();
		existing.vertices.resize(old_vertex_count + p_surface.vertices.size());
		for (int i = 0; i < p_surface.vertices.size(); i++) {
			existing.vertices.set(old_vertex_count + i, p_surface.vertices[i]);
		}
		if (!p_surface.normals.is_empty()) {
			existing.normals.resize(old_vertex_count + p_surface.normals.size());
			for (int i = 0; i < p_surface.normals.size(); i++) {
				existing.normals.set(old_vertex_count + i, p_surface.normals[i]);
			}
		}
		if (!p_surface.uv0.is_empty()) {
			existing.uv0.resize(old_vertex_count + p_surface.uv0.size());
			for (int i = 0; i < p_surface.uv0.size(); i++) {
				existing.uv0.set(old_vertex_count + i, p_surface.uv0[i]);
			}
		}
		if (!p_surface.colors.is_empty()) {
			existing.colors.resize(old_vertex_count + p_surface.colors.size());
			for (int i = 0; i < p_surface.colors.size(); i++) {
				existing.colors.set(old_vertex_count + i, p_surface.colors[i]);
			}
		}
		const int old_index_count = existing.indices.size();
		existing.indices.resize(old_index_count + p_surface.indices.size());
		for (int i = 0; i < p_surface.indices.size(); i++) {
			existing.indices.set(old_index_count + i, p_surface.indices[i] + vertex_offset);
		}
		existing.aabb = existing.aabb.merge(p_surface.aabb);
		existing.estimated_bytes += p_surface.estimated_bytes;
		return true;
	}
	return false;
}

static osgDB::ReaderWriter::ReadResult read_node_with(const char *p_extension, const PackedByteArray &p_bytes, osgDB::Options *p_options) {
	osgDB::ReaderWriter *reader = osgDB::Registry::instance()->getReaderWriterForExtension(p_extension);
	if (!reader) {
		return osgDB::ReaderWriter::ReadResult(vformat("OSG reader '%s' is not registered.", p_extension).utf8().get_data());
	}
	std::string contents(reinterpret_cast<const char *>(p_bytes.ptr()), p_bytes.size());
	std::istringstream stream(contents, std::ios::in | std::ios::binary);
	return reader->readNode(stream, p_options);
}

} // namespace

Error OsgSceneConverter::parse_bytes(const PackedByteArray &p_bytes, const String &p_source_uri, const OsgSceneConvertOptions &p_options, OsgTileDocument &r_document, String &r_error) {
	r_document = OsgTileDocument();
	r_error.clear();
	if (p_bytes.is_empty()) {
		r_error = "OSG input is empty.";
		return ERR_FILE_CORRUPT;
	}

	osg::ref_ptr<osgDB::Options> options = new osgDB::Options;
	options->setPluginStringData("godotDisableExternalResources", "1");
	options->setPluginStringData("fileType", p_source_uri.get_extension().to_lower() == "osgb" ? "Binary" : "");

	osgDB::ReaderWriter::ReadResult result;
	const String extension = p_source_uri.get_extension().to_lower();
	if (extension == "osg") {
		const bool osg2_stream = (p_bytes.size() >= 6 && memcmp(p_bytes.ptr(), "#Ascii", 6) == 0) ||
				(p_bytes.size() >= 5 && memcmp(p_bytes.ptr(), "<?xml", 5) == 0);
		result = read_node_with(osg2_stream ? "osg2" : "osg", p_bytes, options.get());
		if (!result.validNode()) {
			result = read_node_with(osg2_stream ? "osg" : "osg2", p_bytes, options.get());
		}
	} else {
		result = read_node_with("osg2", p_bytes, options.get());
	}
	if (!result.validNode()) {
		r_error = String::utf8(result.statusMessage().c_str());
		if (r_error.is_empty()) {
			r_error = vformat("Unable to parse '%s'.", p_source_uri);
		}
		return ERR_PARSE_ERROR;
	}

	osg::ref_ptr<osg::Node> root = result.takeNode();
	ConvertContext context;
	context.source_uri = OsgDataSource::normalize_uri(p_source_uri);
	context.options = p_options;
	osg::Matrixd identity;
	identity.makeIdentity();
	traverse_node(root.get(), identity, RawMaterial(), -1, -1, context);
	if (context.surfaces.empty() && context.external_tiles.is_empty()) {
		r_error = vformat("'%s' contains no supported geometry or external tiles.", p_source_uri);
		return ERR_FILE_CORRUPT;
	}

	OsgVector3d origin = p_options.explicit_origin;
	if (p_options.use_root_center && context.has_bounds) {
		origin.x = (context.minimum.x() + context.maximum.x()) * 0.5;
		origin.y = (context.minimum.y() + context.maximum.y()) * 0.5;
		origin.z = (context.minimum.z() + context.maximum.z()) * 0.5;
	}

	r_document.source_origin = origin;
	r_document.external_tiles = context.external_tiles;
	r_document.lod_ranges = context.lod_ranges;
	bool document_has_aabb = false;
	// Pure PagedLOD/ProxyNode roots still need a useful frame/selection box.
	if (context.has_bounds && context.surfaces.empty()) {
		for (int mask = 0; mask < 8; mask++) {
			const osg::Vec3d corner(
				(mask & 1) ? context.maximum.x() : context.minimum.x(),
				(mask & 2) ? context.maximum.y() : context.minimum.y(),
				(mask & 4) ? context.maximum.z() : context.minimum.z());
			const Vector3 local_corner = source_to_local(corner, origin, p_options);
			if (!document_has_aabb) {
				r_document.local_aabb = AABB(local_corner, Vector3());
				document_has_aabb = true;
			} else {
				r_document.local_aabb.expand_to(local_corner);
			}
		}
	}
	for (const RawSurface &raw : context.surfaces) {
		OsgMeshSurfacePayload surface;
		surface.vertices.resize((int)raw.vertices.size());
		for (int i = 0; i < (int)raw.vertices.size(); i++) {
			const Vector3 vertex = source_to_local(raw.vertices[i], origin, p_options);
			surface.vertices.set(i, vertex);
			if (i == 0) {
				surface.aabb = AABB(vertex, Vector3());
			} else {
				surface.aabb.expand_to(vertex);
			}
		}
		surface.indices.resize((int)raw.indices.size());
		for (int i = 0; i < (int)raw.indices.size(); i++) {
			surface.indices.set(i, raw.indices[i]);
		}
		if (raw.normals.size() == raw.vertices.size()) {
			surface.normals.resize((int)raw.normals.size());
			for (int i = 0; i < (int)raw.normals.size(); i++) {
				surface.normals.set(i, source_normal_to_local(raw.normals[i], p_options.osg_z_up));
			}
		} else {
			generate_normals(surface);
		}
		if (raw.uv0.size() == raw.vertices.size()) {
			surface.uv0.resize((int)raw.uv0.size());
			for (int i = 0; i < (int)raw.uv0.size(); i++) {
				surface.uv0.set(i, Vector2(raw.uv0[i].x(), 1.0 - raw.uv0[i].y()));
			}
		}
		if (raw.colors.size() == raw.vertices.size()) {
			surface.colors.resize((int)raw.colors.size());
			for (int i = 0; i < (int)raw.colors.size(); i++) {
				surface.colors.set(i, raw.colors[i]);
			}
		}
		surface.material = find_or_add_material(raw.material, context, r_document);
		surface.fallback_group = raw.fallback_group;
		surface.lod_branch = raw.lod_branch;
		surface.estimated_bytes = uint64_t(surface.vertices.size()) * sizeof(Vector3) + uint64_t(surface.normals.size()) * sizeof(Vector3) + uint64_t(surface.uv0.size()) * sizeof(Vector2) + uint64_t(surface.colors.size()) * sizeof(Color) + uint64_t(surface.indices.size()) * sizeof(int32_t);
		r_document.estimated_cpu_bytes += surface.estimated_bytes;
		if (!document_has_aabb) {
			r_document.local_aabb = surface.aabb;
			document_has_aabb = true;
		} else {
			r_document.local_aabb = r_document.local_aabb.merge(surface.aabb);
		}
		if (!merge_compatible_surface(surface, r_document)) {
			r_document.surfaces.push_back(surface);
		}
	}
	for (const OsgMaterialPayload &material : r_document.materials) {
		r_document.estimated_cpu_bytes += material.albedo_image.rgba8.size();
	}
	r_document.warnings = context.warnings;
	return OK;
}
