#pragma once

#include "modules/usd/usd_document.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/3d/importer_mesh.h"
#include "tests/test_macros.h"

namespace TestUSDDocument {

TEST_CASE("[USD] Static mesh holes, winding, indexed UV and reset transform") {
	Error error = FAILED;
	Node3D *root = USDDocument::import_scene_internal("modules/usd/tests/static_mesh.usda", true, &error);
	REQUIRE(root != nullptr);
	CHECK(error == OK);
	Node3D *parent = Object::cast_to<Node3D>(root->get_child(0));
	Node3D *quad = Object::cast_to<Node3D>(parent->get_child(0));
	CHECK((parent->get_transform() * quad->get_transform()).origin.is_equal_approx(Vector3(2, 0, 0)));
	ImporterMeshInstance3D *instance = Object::cast_to<ImporterMeshInstance3D>(quad->get_child(0));
	CHECK(instance != nullptr);
	if (instance) {
		Ref<ImporterMesh> mesh = instance->get_mesh();
		CHECK(mesh->get_surface_count() == 1);
		Array arrays = mesh->get_surface_arrays(0);
		PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
		PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
		PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
		PackedColorArray colors = arrays[Mesh::ARRAY_COLOR];
		CHECK(vertices.size() == 6);
		CHECK(uvs.size() == vertices.size());
		CHECK(colors.size() == vertices.size());
		for (int i = 0; i < vertices.size(); i += 3) {
			CHECK((vertices[i + 1] - vertices[i]).cross(vertices[i + 2] - vertices[i]).dot(normals[i]) < 0);
		}
		for (int i = 0; i < uvs.size(); i++) {
			CHECK(uvs[i].is_equal_approx(Vector2(vertices[i].x, 1 - vertices[i].y)));
			CHECK(colors[i].is_equal_approx(Color(0.2, 0.4, 0.6)));
		}
	}
	memdelete(root);
}

TEST_CASE("[USD] Runtime import creates renderable mesh instances") {
	Node3D *root = USDDocument::import_scene_internal("modules/usd/tests/static_mesh.usda", false);
	REQUIRE(root != nullptr);
	MeshInstance3D *instance = Object::cast_to<MeshInstance3D>(root->get_child(0)->get_child(0)->get_child(0));
	CHECK(instance != nullptr);
	if (instance) {
		CHECK(instance->get_mesh().is_valid());
	}
	memdelete(root);
}

} // namespace TestUSDDocument
