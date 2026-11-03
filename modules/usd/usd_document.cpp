#include "usd_document.h"

#include "core/config/project_settings.h"
#include "core/math/geometry_2d.h"
#include "core/templates/hash_set.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/3d/importer_mesh.h"
#include "scene/resources/material.h"

#include <pxr/base/tf/errorMark.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/xformable.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using namespace pxr;

void require(bool p_valid, const char *p_message) {
	if (!p_valid) {
		throw std::runtime_error(p_message);
	}
}

Vector3 vector3(const GfVec3f &p_value) {
	Vector3 result(p_value[0], p_value[1], p_value[2]);
	require(result.is_finite(), "Non-finite USD vector");
	return result;
}

Transform3D transform3d(const GfMatrix4d &p_matrix) {
	Transform3D result;
	for (int i = 0; i < 3; i++) {
		// USD matrices use row vectors; Godot uses column vectors.
		result.basis.set_column(i, Vector3(p_matrix[i][0], p_matrix[i][1], p_matrix[i][2]));
	}
	result.origin = Vector3(p_matrix[3][0], p_matrix[3][1], p_matrix[3][2]);
	require(result.is_finite(), "Non-finite USD transform");
	return result;
}

template <typename T>
struct AttributeData {
	VtArray<T> values;
	TfToken interpolation;

	void read(const UsdGeomPrimvar &p_var, UsdTimeCode p_time) {
		if (!p_var || !p_var.HasValue()) {
			return;
		}
		require(p_var.GetElementSize() == 1, "USD primvar elementSize must be 1");
		require(p_var.Get(&values, p_time), "Unsupported USD primvar value type");
		interpolation = p_var.GetInterpolation();
		if (p_var.IsIndexed()) {
			VtIntArray indices;
			require(p_var.GetIndices(&indices, p_time), "Cannot read USD primvar indices");
			VtArray<T> flattened(indices.size());
			for (size_t i = 0; i < indices.size(); i++) {
				require(indices[i] >= 0 && size_t(indices[i]) < values.size(), "USD primvar index out of bounds");
				flattened[i] = values[indices[i]];
			}
			values.swap(flattened);
		}
		require(!values.empty(), "Empty authored USD primvar");
	}

	void validate(size_t p_points, size_t p_faces, size_t p_corners) const {
		if (values.empty()) {
			return;
		}
		size_t count = 0;
		if (interpolation == UsdGeomTokens->constant) {
			count = 1;
		} else if (interpolation == UsdGeomTokens->uniform) {
			count = p_faces;
		} else if (interpolation == UsdGeomTokens->vertex || interpolation == UsdGeomTokens->varying) {
			count = p_points;
		} else if (interpolation == UsdGeomTokens->faceVarying) {
			count = p_corners;
		} else {
			require(false, "Unsupported USD primvar interpolation");
		}
		require(values.size() == count, "USD attribute cardinality does not match topology");
	}

	const T &at(size_t p_point, size_t p_face, size_t p_corner) const {
		size_t index = 0;
		if (interpolation == UsdGeomTokens->uniform) {
			index = p_face;
		} else if (interpolation == UsdGeomTokens->vertex || interpolation == UsdGeomTokens->varying) {
			index = p_point;
		} else if (interpolation == UsdGeomTokens->faceVarying) {
			index = p_corner;
		}
		return values[index];
	}
};

Ref<ImporterMesh> read_mesh(const UsdGeomMesh &p_mesh, UsdTimeCode p_time) {
	VtVec3fArray points;
	VtIntArray counts, indices, holes;
	require(p_mesh.GetPointsAttr().Get(&points, p_time), "USD mesh has no points");
	require(p_mesh.GetFaceVertexCountsAttr().Get(&counts, p_time) && p_mesh.GetFaceVertexIndicesAttr().Get(&indices, p_time), "USD mesh has no topology");
	p_mesh.GetHoleIndicesAttr().Get(&holes, p_time);
	require(points.size() <= INT32_MAX && counts.size() <= INT32_MAX && indices.size() <= INT32_MAX / 3, "USD mesh exceeds supported size");
	size_t total = 0;
	for (int count : counts) {
		require(count >= 3 && size_t(count) <= indices.size() - total, "Invalid USD face vertex count");
		total += count;
	}
	require(total == indices.size(), "USD face counts do not match indices");
	for (int index : indices) {
		require(index >= 0 && size_t(index) < points.size(), "USD point index out of bounds");
	}
	for (const GfVec3f &point : points) {
		vector3(point);
	}
	HashSet<int> hole_set;
	for (int hole : holes) {
		require(hole >= 0 && size_t(hole) < counts.size(), "USD hole index out of bounds");
		hole_set.insert(hole);
	}
	TfToken scheme, orientation;
	p_mesh.GetSubdivisionSchemeAttr().Get(&scheme, p_time);
	if (scheme != UsdGeomTokens->none) {
		WARN_PRINT("USD subdivision is not evaluated; importing the control cage: " + String(p_mesh.GetPath().GetText()));
	}
	p_mesh.GetOrientationAttr().Get(&orientation, p_time);
	const bool left_handed = orientation == UsdGeomTokens->leftHanded;
	UsdGeomPrimvarsAPI primvars(p_mesh.GetPrim());
	AttributeData<GfVec3f> normals, colors;
	AttributeData<GfVec2f> uvs;
	normals.read(primvars.FindPrimvarWithInheritance(TfToken("normals")), p_time);
	if (normals.values.empty() && p_mesh.GetNormalsAttr().HasValue()) {
		require(p_mesh.GetNormalsAttr().Get(&normals.values, p_time), "Cannot read USD normals");
		normals.interpolation = p_mesh.GetNormalsInterpolation();
	}
	uvs.read(primvars.FindPrimvarWithInheritance(TfToken("st")), p_time);
	colors.read(primvars.FindPrimvarWithInheritance(TfToken("displayColor")), p_time);
	normals.validate(points.size(), counts.size(), indices.size());
	uvs.validate(points.size(), counts.size(), indices.size());
	colors.validate(points.size(), counts.size(), indices.size());

	Vector<Vector3> vertices, vertex_normals;
	Vector<Vector2> vertex_uvs;
	Vector<Color> vertex_colors;
	size_t offset = 0;
	for (size_t face = 0; face < counts.size(); face++) {
		const int count = counts[face];
		if (hole_set.has(int(face))) {
			offset += count;
			continue;
		}
		Vector3 face_normal;
		const Vector3 origin = vector3(points[indices[offset]]);
		for (int c = 1; c + 1 < count; c++) {
			face_normal += (vector3(points[indices[offset + c]]) - origin).cross(vector3(points[indices[offset + c + 1]]) - origin);
		}
		require(face_normal.is_finite() && !face_normal.is_zero_approx(), "Degenerate USD polygon");
		face_normal.normalize();
		const int axis = face_normal.abs().max_axis_index();
		Vector<Vector2> polygon;
		polygon.resize(count);
		for (int c = 0; c < count; c++) {
			const Vector3 v = vector3(points[indices[offset + c]]) - origin;
			require(Math::abs(v.dot(face_normal)) <= 0.00001 * MAX(1.0, double(v.length())), "Non-planar USD polygon is unsupported");
			polygon.write[c] = Vector2(v[(axis + 1) % 3], v[(axis + 2) % 3]);
		}
		for (int a = 0; a < count; a++) {
			for (int b = a + 1; b < count; b++) {
				require(!polygon[a].is_equal_approx(polygon[b]), "Repeated USD polygon vertex");
				if (b == a + 1 || (a == 0 && b == count - 1)) {
					continue;
				}
				require(!Geometry2D::segment_intersects_segment(polygon[a], polygon[(a + 1) % count], polygon[b], polygon[(b + 1) % count], nullptr), "Self-intersecting USD polygon");
			}
		}
		Vector<int> triangles = Geometry2D::triangulate_polygon(polygon);
		require(triangles.size() == (count - 2) * 3, "Cannot triangulate USD polygon");
		// The triangulator returns projected CCW triangles, independent of input order.
		const Vector3 outward = left_handed ? -face_normal : face_normal;
		for (int t = 0; t < triangles.size(); t += 3) {
			int corners[3] = { triangles[t], triangles[t + 1], triangles[t + 2] };
			Vector3 a = vector3(points[indices[offset + corners[0]]]);
			Vector3 b = vector3(points[indices[offset + corners[1]]]);
			Vector3 c = vector3(points[indices[offset + corners[2]]]);
			const Vector3 triangle_normal = (b - a).cross(c - a);
			require(triangle_normal.is_finite() && !triangle_normal.is_zero_approx(), "Degenerate USD triangle");
			if (triangle_normal.dot(outward) > 0) {
				SWAP(corners[1], corners[2]);
			}
			for (int corner : corners) {
				const size_t index = indices[offset + corner];
				vertices.push_back(vector3(points[index]));
				Vector3 normal = normals.values.empty() ? outward : vector3(normals.at(index, face, offset + corner));
				require(!normal.is_zero_approx(), "Zero USD normal");
				vertex_normals.push_back(normal.normalized());
				if (!uvs.values.empty()) {
					const GfVec2f &uv = uvs.at(index, face, offset + corner);
					Vector2 value(uv[0], 1.0f - uv[1]);
					require(value.is_finite(), "Non-finite USD UV");
					vertex_uvs.push_back(value);
				}
				if (!colors.values.empty()) {
					const Vector3 color = vector3(colors.at(index, face, offset + corner));
					vertex_colors.push_back(Color(color.x, color.y, color.z));
				}
			}
		}
		offset += count;
	}
	Ref<ImporterMesh> mesh;
	mesh.instantiate();
	if (vertices.is_empty()) {
		return mesh;
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = vertex_normals;
	if (!vertex_uvs.is_empty()) {
		arrays[Mesh::ARRAY_TEX_UV] = vertex_uvs;
	}
	if (!vertex_colors.is_empty()) {
		arrays[Mesh::ARRAY_COLOR] = vertex_colors;
	}
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, !vertex_colors.is_empty());
	bool double_sided = false;
	p_mesh.GetDoubleSidedAttr().Get(&double_sided, p_time);
	material->set_cull_mode(double_sided ? BaseMaterial3D::CULL_DISABLED : BaseMaterial3D::CULL_BACK);
	mesh->add_surface(Mesh::PRIMITIVE_TRIANGLES, arrays, TypedArray<Array>(), Dictionary(), material);
	mesh->set_name(String(p_mesh.GetPrim().GetName().GetText()));
	return mesh;
}

void read_prim(const UsdPrim &p_prim, Node3D *p_parent, Node3D *p_root, const Transform3D &p_parent_world, UsdTimeCode p_time, bool p_importer, int p_depth) {
	require(p_depth < 256, "USD hierarchy exceeds 256 levels");
	require(!p_prim.IsInstance(), "USD native instances are not supported; de-instance before importing");
	Node3D *node = memnew(Node3D);
	p_parent->add_child(node);
	node->set_owner(p_root);
	node->set_name(String(p_prim.GetName().GetText()).validate_node_name());
	Transform3D local;
	bool reset = false;
	UsdGeomXformable xform(p_prim);
	if (xform) {
		GfMatrix4d matrix(1.0);
		require(xform.GetLocalTransformation(&matrix, &reset, p_time), "Cannot read USD transform");
		local = transform3d(matrix);
	}
	Transform3D world = reset ? local : p_parent_world * local;
	if (reset) {
		require(!Math::is_zero_approx(p_parent_world.basis.determinant()), "Cannot reset USD transform below singular parent");
		local = p_parent_world.affine_inverse() * local;
	}
	node->set_transform(local);
	UsdGeomImageable imageable(p_prim);
	if (imageable) {
		node->set_visible(imageable.ComputeVisibility(p_time) != UsdGeomTokens->invisible);
	}
	if (p_prim.IsA<UsdGeomMesh>()) {
		Ref<ImporterMesh> mesh = read_mesh(UsdGeomMesh(p_prim), p_time);
		if (mesh->get_surface_count() > 0) {
			Node3D *instance;
			if (p_importer) {
				ImporterMeshInstance3D *importer = memnew(ImporterMeshInstance3D);
				node->add_child(importer);
				importer->set_mesh(mesh);
				instance = importer;
			} else {
				MeshInstance3D *runtime = memnew(MeshInstance3D);
				node->add_child(runtime);
				runtime->set_mesh(mesh->get_mesh());
				instance = runtime;
			}
			instance->set_name("Mesh");
			instance->set_owner(p_root);
		}
	} else if (p_prim.GetTypeName() != TfToken("Xform") && p_prim.GetTypeName() != TfToken("Scope") && !p_prim.GetTypeName().IsEmpty()) {
		WARN_PRINT("USD imports only static Mesh geometry; retaining transform only for " + String(p_prim.GetPath().GetText()));
	}
	for (const UsdPrim &child : p_prim.GetChildren()) {
		read_prim(child, node, p_root, world, p_time, p_importer, p_depth + 1);
	}
}
} // namespace

void USDDocument::_bind_methods() {
	ClassDB::bind_static_method("USDDocument", D_METHOD("import_scene", "path"), &USDDocument::import_scene);
}

Node3D *USDDocument::import_scene(const String &p_path) {
	return import_scene_internal(p_path, false);
}

Node3D *USDDocument::import_scene_internal(const String &p_path, bool p_importer_mesh, Error *r_error) {
	if (r_error) {
		*r_error = ERR_FILE_CORRUPT;
	}
	Node3D *root = nullptr;
	try {
		pxr::TfErrorMark errors;
		const String path = ProjectSettings::get_singleton()->globalize_path(p_path);
		pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(path.utf8().get_data(), pxr::UsdStage::LoadAll);
		require(bool(stage), "OpenUSD could not open the stage");
		root = memnew(Node3D);
		root->set_name(p_path.get_file().get_basename().validate_node_name());
		const double meters = pxr::UsdGeomGetStageMetersPerUnit(stage);
		require(std::isfinite(meters) && meters > 0 && meters <= std::numeric_limits<real_t>::max(), "Invalid USD metersPerUnit");
		Basis conversion;
		const pxr::TfToken up = pxr::UsdGeomGetStageUpAxis(stage);
		if (up == pxr::UsdGeomTokens->z) {
			conversion = Basis(Vector3(1, 0, 0), -Math::PI / 2);
		} else {
			require(up == pxr::UsdGeomTokens->y, "Unsupported USD up axis");
		}
		root->set_transform(Transform3D(conversion.scaled(Vector3(meters, meters, meters)), Vector3()));
		WARN_PRINT("USD static MVP: sampling stage start time; no animation, skinning, subdivision evaluation, material networks, textures or displayOpacity. Source dependencies must remain accessible to the OpenUSD resolver.");
		const pxr::UsdTimeCode time(stage->GetStartTimeCode());
		for (const pxr::UsdPrim &prim : stage->GetPseudoRoot().GetChildren()) {
			read_prim(prim, root, root, Transform3D(), time, p_importer_mesh, 0);
		}
		require(errors.IsClean(), "OpenUSD reported errors while loading the stage");
		if (r_error) {
			*r_error = OK;
		}
		return root;
	} catch (const std::exception &error) {
		ERR_PRINT("USD import failed: " + String(error.what()));
	} catch (...) {
		ERR_PRINT("USD import failed with an unknown SDK exception");
	}
	if (root) {
		memdelete(root);
	}
	return nullptr;
}
