/**************************************************************************/
/*  test_tiled_mesh_format.h                                              */
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

#include "core/io/file_access.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/scene_tree.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#include "modules/modules_enabled.gen.h"
#include "modules/tiled_mesh_3d/tiled_mesh_format_registry.h"
#include "modules/tiled_mesh_3d/tiled_mesh_image_decode_queue.h"
#include "modules/tiled_mesh_3d/tiled_mesh_instance_3d.h"
#include "modules/tiled_mesh_3d/tiled_mesh_mesh_pack_queue.h"

namespace TestTiledMeshFormat {

static TiledMeshMeshSurfacePayload make_surface(int p_replacement_group = -1) {
	TiledMeshMeshSurfacePayload surface;
	surface.vertices.push_back(Vector3(-0.5, 0, -0.5));
	surface.vertices.push_back(Vector3(0.5, 0, -0.5));
	surface.vertices.push_back(Vector3(0, 0, 0.5));
	surface.normals.push_back(Vector3(0, 1, 0));
	surface.normals.push_back(Vector3(0, 1, 0));
	surface.normals.push_back(Vector3(0, 1, 0));
	surface.indices.push_back(0);
	surface.indices.push_back(1);
	surface.indices.push_back(2);
	surface.replacement_group = p_replacement_group;
	surface.aabb = AABB(Vector3(-0.5, 0, -0.5), Vector3(1, 0, 1));
	surface.estimated_bytes = 128;
	return surface;
}

class SyntheticSession : public TiledMeshFormatSession {
	HashMap<uint64_t, TiledMeshParseResult> results;
	uint64_t next_id = 1;
	bool stopped = false;
	bool nested_replacement = false;
	bool external_texture = false;

public:
	explicit SyntheticSession(bool p_nested_replacement = false, bool p_external_texture = false) :
			nested_replacement(p_nested_replacement), external_texture(p_external_texture) {}

	virtual uint64_t enqueue_parse(const TiledMeshParseRequest &p_request, double p_priority) override {
		if (stopped || p_request.bytes.is_empty()) {
			return 0;
		}
		TiledMeshParseResult result;
		result.request_id = next_id++;
		result.owner = p_request.owner;
		result.generation = p_request.generation;
		result.uri = p_request.uri;
		result.document.coordinate_frame.source_to_local = Basis();
		result.document.coordinate_frame.unit_scale = 1.0;
		result.document.local_aabb = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));
		result.document.estimated_cpu_bytes = 128;
		if (external_texture) {
			TiledMeshMeshSurfacePayload surface = make_surface(p_request.root ? 31 : -1);
			if (p_request.root) {
				result.document.surfaces.push_back(surface);
				TiledMeshTileDefinition child;
				child.provider_key = "external_texture_child";
				child.replacement_group = 31;
				child.refinement = TiledMeshTileDefinition::REFINE_REPLACE;
				child.selection.mode = TiledMeshSelectionRule::MODE_DISTANCE_RANGE;
				child.selection.center = { 0, 0, 0 };
				child.selection.radius = 10.0;
				child.selection.minimum = 0.0;
				child.selection.maximum = 20.0;
				child.bounding_volume.center = { 0, 0, 0 };
				child.bounding_volume.radius = 10.0;
				TiledMeshContentRequest content;
				content.uri = p_request.uri.get_base_dir().path_join("external_texture_child.bin");
				content.content_type = SNAME("synthetic_mesh");
				child.contents.push_back(content);
				result.document.tiles.push_back(child);
			} else {
				surface.material = 0;
				result.document.surfaces.push_back(surface);
				TiledMeshMaterialPayload material;
				material.unlit = true;
				material.albedo_image.uri = p_request.uri.get_base_dir().path_join("external_texture.png");
				result.document.materials.push_back(material);
				material.unlit = false;
				material.albedo_image.uri = String();
				result.document.materials.push_back(material);
				surface.material = 1;
				result.document.surfaces.push_back(surface);
			}
			results.insert(result.request_id, result);
			return result.request_id;
		}
		if (nested_replacement) {
			const bool child = p_request.uri.ends_with("nested_child.bin");
			const int replacement_group = p_request.root ? 17 : (child ? 23 : -1);
			result.document.surfaces.push_back(make_surface(replacement_group));
			if (p_request.root || child) {
				TiledMeshTileDefinition replacement;
				replacement.provider_key = p_request.root ? "nested_child" : "nested_grandchild";
				replacement.replacement_group = p_request.root ? 17 : 23;
				replacement.refinement = TiledMeshTileDefinition::REFINE_REPLACE;
				replacement.selection.mode = TiledMeshSelectionRule::MODE_ALWAYS;
				replacement.bounding_volume.radius = 10.0;
				TiledMeshContentRequest content;
				content.uri = p_request.uri.get_base_dir().path_join(p_request.root ? "nested_child.bin" : "nested_grandchild.bin");
				content.content_type = SNAME("synthetic_mesh");
				replacement.contents.push_back(content);
				result.document.tiles.push_back(replacement);
			}
			results.insert(result.request_id, result);
			return result.request_id;
		}
		if (p_request.root) {
			result.document.surfaces.push_back(make_surface(7));
			TiledMeshTileDefinition replacement;
			replacement.provider_key = "replacement";
			replacement.replacement_group = 7;
			replacement.refinement = TiledMeshTileDefinition::REFINE_REPLACE;
			replacement.selection.mode = TiledMeshSelectionRule::MODE_ALWAYS;
			replacement.bounding_volume.radius = 10.0;
			TiledMeshContentRequest first;
			first.uri = p_request.uri.get_base_dir().path_join("synthetic_a.bin");
			first.content_type = SNAME("synthetic_mesh");
			replacement.contents.push_back(first);
			TiledMeshContentRequest second;
			second.uri = p_request.uri.get_base_dir().path_join("synthetic_b.bin");
			second.content_type = SNAME("synthetic_mesh");
			replacement.contents.push_back(second);
			result.document.tiles.push_back(replacement);
		} else {
			result.document.surfaces.push_back(make_surface());
			if (p_request.uri.ends_with("synthetic_a.bin")) {
				TiledMeshTileDefinition additive;
				additive.provider_key = "additive_grandchild";
				additive.refinement = TiledMeshTileDefinition::REFINE_ADD;
				additive.selection.mode = TiledMeshSelectionRule::MODE_SCREEN_SPACE_ERROR;
				additive.selection.center = { 0, 0, 0 };
				additive.selection.geometric_error = 100.0;
				additive.bounding_volume.radius = 10.0;
				TiledMeshContentRequest grandchild;
				grandchild.uri = p_request.uri.get_base_dir().path_join("synthetic_grandchild.bin");
				grandchild.content_type = SNAME("synthetic_mesh");
				additive.contents.push_back(grandchild);
				result.document.tiles.push_back(additive);
			}
		}
		results.insert(result.request_id, result);
		return result.request_id;
	}

	virtual void update_priority(uint64_t p_request_id, double p_priority) override {}
	virtual void cancel(uint64_t p_request_id) override { results.erase(p_request_id); }
	virtual bool take_result(uint64_t p_request_id, TiledMeshParseResult &r_result) override {
		const TiledMeshParseResult *result = results.getptr(p_request_id);
		if (!result) {
			return false;
		}
		r_result = *result;
		results.erase(p_request_id);
		return true;
	}
	virtual void shutdown() override {
		stopped = true;
		results.clear();
	}
};

class SyntheticLoader : public TiledMeshFormatLoader {
	StringName id;
	int score = 90;
	bool nested_replacement = false;
	bool external_texture = false;

public:
	explicit SyntheticLoader(const StringName &p_id = SNAME("synthetic"), int p_score = 90, bool p_nested_replacement = false, bool p_external_texture = false) :
			id(p_id), score(p_score), nested_replacement(p_nested_replacement), external_texture(p_external_texture) {}
	virtual StringName get_format_id() const override { return id; }
	virtual Vector<String> get_recognized_extensions() const override {
		Vector<String> extensions;
		extensions.push_back("tmesh");
		return extensions;
	}
	virtual int recognize(const String &p_uri, const PackedByteArray &p_bytes) const override {
		return p_uri.get_extension().to_lower() == "tmesh" && !p_bytes.is_empty() ? score : 0;
	}
	virtual bool supports_options(const Ref<TiledMeshFormatOptions> &p_options) const override { return p_options.is_null(); }
	virtual TiledMeshFormatSession *create_session(const Ref<TiledMeshFormatOptions> &p_options, const TiledMeshSessionOptions &p_session_options, String &r_error) override {
		return memnew(SyntheticSession(nested_replacement, external_texture));
	}
};

TEST_CASE("[TiledMesh3D] Format registry rejects duplicates and ambiguous recognition") {
	SyntheticLoader first(SNAME("synthetic_registry_a"));
	SyntheticLoader duplicate(SNAME("synthetic_registry_a"));
	SyntheticLoader second(SNAME("synthetic_registry_b"));
	CHECK(TiledMeshFormatRegistry::register_loader(&first) == OK);
	ERR_PRINT_OFF;
	CHECK(TiledMeshFormatRegistry::register_loader(&duplicate) == ERR_ALREADY_EXISTS);
	ERR_PRINT_ON;
	CHECK(TiledMeshFormatRegistry::register_loader(&second) == OK);
	PackedByteArray bytes;
	bytes.push_back(1);
	String error;
	CHECK(TiledMeshFormatRegistry::recognize("dataset.tmesh", bytes, error) == nullptr);
	CHECK(error.contains("same confidence"));
	TiledMeshFormatRegistry::unregister_loader(&second);
	error.clear();
	CHECK(TiledMeshFormatRegistry::recognize("dataset.tmesh", bytes, error) == &first);
	TiledMeshFormatRegistry::unregister_loader(&first);
}

TEST_CASE("[TiledMesh3D] Mesh pack queue produces SurfaceData and honors pending priority updates") {
	TiledMeshMeshPackQueue *queue = TiledMeshMeshPackQueue::get_singleton();
	REQUIRE(queue != nullptr);

	TiledMeshDocument blocker;
	TiledMeshMeshSurfacePayload blocker_surface;
	const int blocker_vertex_count = 200000;
	blocker_surface.vertices.resize(blocker_vertex_count);
	blocker_surface.indices.resize(blocker_vertex_count / 3 * 3);
	{
		Vector3 *vertices = blocker_surface.vertices.ptrw();
		for (int i = 0; i < blocker_vertex_count; i++) {
			vertices[i] = Vector3(float(i % 128), 0.0f, float(i / 128));
		}
		int32_t *indices = blocker_surface.indices.ptrw();
		for (int i = 0; i < blocker_surface.indices.size(); i++) {
			indices[i] = i;
		}
	}
	blocker_surface.aabb = AABB(Vector3(), Vector3(128, 1, 1563));
	blocker_surface.estimated_bytes = 4800000;
	blocker.surfaces.push_back(blocker_surface);
	blocker.estimated_cpu_bytes = blocker_surface.estimated_bytes;

	TiledMeshDocument low;
	TiledMeshMeshSurfacePayload low_surface = make_surface(3);
	low_surface.material = 4;
	low.materials.resize(5);
	low.materials.write[4].unlit = true;
	low_surface.colors.push_back(Color(1, 0, 0));
	low_surface.colors.push_back(Color(0, 1, 0));
	low_surface.colors.push_back(Color(0, 0, 1));
	low.surfaces.push_back(low_surface);
	low.estimated_cpu_bytes = low_surface.estimated_bytes;

	TiledMeshDocument promoted = low;
	promoted.surfaces.write[0].replacement_group = 9;
	const ObjectID owner(uint64_t(0x5041434B));
	const uint64_t blocker_id = queue->enqueue(owner, 17, blocker, 1000.0);
	const uint64_t low_id = queue->enqueue(owner, 17, low, 1.0);
	const uint64_t promoted_id = queue->enqueue(owner, 17, promoted, 0.0);
	REQUIRE(blocker_id != 0);
	REQUIRE(low_id != 0);
	REQUIRE(promoted_id != 0);
	queue->update_priority(promoted_id, 10.0);

	TiledMeshMeshPackQueue::Result blocker_result;
	bool blocker_completed = false;
	for (int i = 0; i < 5000 && !blocker_completed; i++) {
		blocker_completed = queue->take_result(blocker_id, blocker_result);
		if (!blocker_completed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(blocker_completed);
	REQUIRE(blocker_result.error == OK);

	TiledMeshMeshPackQueue::Result next_result;
	bool next_completed = false;
	for (int i = 0; i < 5000 && !next_completed; i++) {
		next_completed = queue->take_next_result_for_testing(next_result);
		if (!next_completed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(next_completed);
	CHECK(next_result.request_id == promoted_id);
	REQUIRE(next_result.error == OK);
	REQUIRE(next_result.document.render_groups.size() == 1);
	const TiledMeshPackedRenderGroup &group = next_result.document.render_groups[0];
	CHECK(group.replacement_group == 9);
	REQUIRE(group.surfaces.size() == 1);
	const TiledMeshPackedSurface &surface = group.surfaces[0];
	CHECK(surface.material == 4);
	CHECK(surface.uses_vertex_color);
	CHECK(surface.surface_data.vertex_count == 3);
	CHECK(surface.surface_data.index_count == 3);
	CHECK(surface.surface_data.aabb.position.is_equal_approx(promoted.surfaces[0].aabb.position));
	CHECK(Math::is_equal_approx(surface.surface_data.aabb.size.x, promoted.surfaces[0].aabb.size.x));
	CHECK(surface.surface_data.aabb.size.y > 0.0f);
	CHECK(Math::is_equal_approx(surface.surface_data.aabb.size.z, promoted.surfaces[0].aabb.size.z));
	CHECK(next_result.document.document.surfaces.is_empty());
	REQUIRE(next_result.document.document.materials.size() == 5);
	CHECK(next_result.document.document.materials[4].unlit);
	CHECK_FALSE(next_result.document.document.materials[0].unlit);
	CHECK(next_result.document.packed_bytes > 0);
	queue->cancel(low_id);
}

TEST_CASE("[TiledMesh3D] Mesh pack queue preserves unindexed point surfaces") {
	TiledMeshMeshPackQueue *queue = TiledMeshMeshPackQueue::get_singleton();
	REQUIRE(queue != nullptr);
	TiledMeshDocument document;
	TiledMeshMeshSurfacePayload surface;
	surface.points = true;
	surface.vertices.push_back(Vector3(0, 0, 0));
	surface.vertices.push_back(Vector3(1, 0, 0));
	surface.aabb = AABB(Vector3(), Vector3(1, 0, 0));
	document.surfaces.push_back(surface);
	const uint64_t request_id = queue->enqueue(ObjectID(uint64_t(0x504F494E)), 1, document, 0.0);
	REQUIRE(request_id != 0);
	TiledMeshMeshPackQueue::Result result;
	bool completed = false;
	for (int i = 0; i < 5000 && !completed; i++) {
		completed = queue->take_result(request_id, result);
		if (!completed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(completed);
	REQUIRE(result.error == OK);
	REQUIRE(result.document.render_groups.size() == 1);
	REQUIRE(result.document.render_groups[0].surfaces.size() == 1);
	const TiledMeshPackedSurface &packed = result.document.render_groups[0].surfaces[0];
	CHECK(packed.points);
	CHECK(packed.indices.is_empty());
	CHECK(packed.surface_data.vertex_count == 2);
	CHECK(packed.surface_data.index_count == 0);
}

TEST_CASE("[TiledMesh3D] Image decode queue reprioritizes pending textures") {
	TiledMeshImageDecodeQueue *queue = TiledMeshImageDecodeQueue::get_singleton();
	REQUIRE(queue != nullptr);

	// Keep the single worker occupied long enough to enqueue both pending jobs.
	// The blocker also has the highest priority, making the ordering deterministic
	// if the worker has not started by the time all three requests are present.
	Ref<Image> blocker_source = Image::create_empty(2048, 2048, false, Image::FORMAT_RGBA8);
	blocker_source->fill(Color(0.1, 0.2, 0.3, 1.0));
	const PackedByteArray blocker_png = blocker_source->save_png_to_buffer();
	REQUIRE_FALSE(blocker_png.is_empty());
	Ref<Image> small_source = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	small_source->fill(Color(0.4, 0.5, 0.6, 1.0));
	const PackedByteArray small_png = small_source->save_png_to_buffer();
	REQUIRE_FALSE(small_png.is_empty());

	const ObjectID owner(uint64_t(0x7E87));
	const uint64_t blocker_id = queue->enqueue(owner, 9, "res://texture_blocker.png", blocker_png, 1000.0);
	const uint64_t low_id = queue->enqueue(owner, 9, "res://texture_low.png", small_png, 1.0);
	const uint64_t promoted_id = queue->enqueue(owner, 9, "res://texture_promoted.png", small_png, 0.0);
	REQUIRE(blocker_id != 0);
	REQUIRE(low_id != 0);
	REQUIRE(promoted_id != 0);
	queue->update_priority(promoted_id, 10.0);

	TiledMeshImageDecodeQueue::Result blocker_result;
	bool blocker_completed = false;
	for (int i = 0; i < 5000 && !blocker_completed; i++) {
		blocker_completed = queue->take_result(blocker_id, blocker_result);
		if (!blocker_completed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(blocker_completed);
	blocker_result.image.unref();

	TiledMeshImageDecodeQueue::Result next_result;
	bool next_completed = false;
	for (int i = 0; i < 5000 && !next_completed; i++) {
		next_completed = queue->take_next_result_for_testing(next_result);
		if (!next_completed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(next_completed);
	CHECK(next_result.request_id == promoted_id);
	queue->cancel(low_id);
}

TEST_CASE("[TiledMesh3D][SceneTree] External texture work pauses outside the active tile set and resumes") {
	SyntheticLoader loader(SNAME("synthetic_texture"), 90, false, true);
	REQUIRE(TiledMeshFormatRegistry::register_loader(&loader) == OK);
	const String root_path = TestUtils::get_temp_path("external_texture_root.tmesh");
	const String child_path = root_path.get_base_dir().path_join("external_texture_child.bin");
	for (const String &path : Vector<String>({ root_path, child_path })) {
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_8(1);
	}
	Ref<Image> texture_source = Image::create_empty(1024, 1024, false, Image::FORMAT_RGBA8);
	texture_source->fill(Color(0.2, 0.4, 0.6, 1.0));
	const PackedByteArray texture_png = texture_source->save_png_to_buffer();
	REQUIRE_FALSE(texture_png.is_empty());
	Ref<FileAccess> texture_file = FileAccess::open(root_path.get_base_dir().path_join("external_texture.png"), FileAccess::WRITE);
	REQUIRE(texture_file.is_valid());
	texture_file->store_buffer(texture_png);
	texture_file.unref();

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 2, 5));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_format_id(SNAME("synthetic_texture"));
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool texture_queued = false;
	for (int i = 0; i < 5000 && !texture_queued; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		texture_queued = int(stats["resident_contents"]) == 2 && int(stats["texture_requests"]) == 1;
		if (!texture_queued) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(texture_queued);

	camera->set_position(Vector3(0, 2, 100));
	SceneTree::get_singleton()->process(0.001);
	Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["texture_requests"]) == 1);
	CHECK(int(stats["active_texture_requests"]) == 0);
	CHECK(int(stats["paused_texture_requests"]) == 1);

	camera->set_position(Vector3(0, 2, 5));
	SceneTree::get_singleton()->process(0.001);
	stats = node->get_streaming_stats();
	CHECK(int(stats["active_texture_requests"]) == 1);
	CHECK(int(stats["paused_texture_requests"]) == 0);

	bool texture_installed = false;
	for (int i = 0; i < 5000 && !texture_installed; i++) {
		SceneTree::get_singleton()->process(0.001);
		stats = node->get_streaming_stats();
		texture_installed = int(stats["texture_requests"]) == 0;
		if (!texture_installed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(texture_installed);
	CHECK(int(stats["resident_materials"]) == 3);
	CHECK(int(stats["resident_unlit_materials"]) == 1);
	CHECK(int(stats["resident_textured_materials"]) == 1);
	CHECK(uint64_t(stats["estimated_resident_bytes"]) >= uint64_t(1024) * 1024 * 4);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
	TiledMeshFormatRegistry::unregister_loader(&loader);
}

TEST_CASE("[TiledMesh3D][SceneTree] Synthetic format streams SSE, REPLACE, ADD, multiple contents, and a dynamic subtree") {
	SyntheticLoader loader;
	REQUIRE(TiledMeshFormatRegistry::register_loader(&loader) == OK);
	const String root_path = TestUtils::get_temp_path("synthetic_root.tmesh");
	for (const String &path : Vector<String>({ root_path,
				 root_path.get_base_dir().path_join("synthetic_a.bin"),
				 root_path.get_base_dir().path_join("synthetic_b.bin"),
				 root_path.get_base_dir().path_join("synthetic_grandchild.bin") })) {
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_8(1);
	}

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 2, 5));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_maximum_screen_space_error(1.0);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool complete = false;
	for (int i = 0; i < 2000 && !complete; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		complete = int(stats["logical_tiles"]) == 3 && int(stats["resident_contents"]) == 4;
		if (!complete) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(complete);
	const Dictionary stats = node->get_streaming_stats();
	CHECK(StringName(stats["active_format_id"]) == SNAME("synthetic"));
	CHECK(int(stats["contents"]) == 4);
	CHECK(int(stats["resident_render_instances"]) == 4);
	CHECK(int(stats["inline_lod_render_instances"]) == 0);
	CHECK(int(stats["visible_inline_lod_render_instances"]) == 0);
	CHECK(int(stats["hidden_replacement_surfaces"]) == 1);
	CHECK(int(stats["visible_render_instances"]) == 3);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
	TiledMeshFormatRegistry::unregister_loader(&loader);
}

TEST_CASE("[TiledMesh3D][SceneTree] A fully refined nested replacement hides every ancestor fallback") {
	SyntheticLoader loader(SNAME("synthetic_nested"), 90, true);
	REQUIRE(TiledMeshFormatRegistry::register_loader(&loader) == OK);
	const String root_path = TestUtils::get_temp_path("nested_root.tmesh");
	for (const String &path : Vector<String>({ root_path,
				 root_path.get_base_dir().path_join("nested_child.bin"),
				 root_path.get_base_dir().path_join("nested_grandchild.bin") })) {
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_8(1);
	}

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 2, 5));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_format_id(SNAME("synthetic_nested"));
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool complete = false;
	for (int i = 0; i < 2000 && !complete; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		complete = int(stats["logical_tiles"]) == 3 && int(stats["resident_contents"]) == 3;
		if (!complete) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(complete);
	const Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["resident_render_instances"]) == 3);
	CHECK(int(stats["hidden_replacement_surfaces"]) == 2);
	CHECK(int(stats["visible_render_instances"]) == 1);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
	TiledMeshFormatRegistry::unregister_loader(&loader);
}

TEST_CASE("[TiledMesh3D][SceneTree] Explicit format rejects incompatible options without leaving a request") {
	SyntheticLoader loader;
	REQUIRE(TiledMeshFormatRegistry::register_loader(&loader) == OK);
	const String root_path = TestUtils::get_temp_path("synthetic_incompatible_options.tmesh");
	Ref<FileAccess> file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_8(1);
	file.unref();

	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_format_id(SNAME("synthetic"));
	Ref<TiledMeshFormatOptions> incompatible_options;
	incompatible_options.instantiate();
	node->set_format_options(incompatible_options);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	for (int i = 0; i < 2000 && node->is_open(); i++) {
		SceneTree::get_singleton()->process(0.001);
		OS::get_singleton()->delay_usec(1000);
	}
	CHECK_FALSE(node->is_open());
	const Dictionary stats = node->get_streaming_stats();
	CHECK(String(stats["root_state"]) == "failed");
	CHECK(int(stats["requested_contents"]) == 0);
	CHECK(StringName(stats["active_format_id"]).is_empty());

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	TiledMeshFormatRegistry::unregister_loader(&loader);
}

#ifndef MODULE_OSGDB_ENABLED
TEST_CASE("[TiledMesh3D][SceneTree] OSG source fails clearly when the OSG adapter is disabled") {
	const String root_path = TestUtils::get_temp_path("missing_adapter.osgb");
	Ref<FileAccess> file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_8(1);
	file.unref();

	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	for (int i = 0; i < 2000 && node->is_open(); i++) {
		SceneTree::get_singleton()->process(0.001);
		OS::get_singleton()->delay_usec(1000);
	}
	CHECK_FALSE(node->is_open());
	const Dictionary stats = node->get_streaming_stats();
	CHECK(String(stats["root_state"]) == "failed");
	CHECK(int(stats["requested_contents"]) == 0);
	CHECK(StringName(stats["active_format_id"]).is_empty());

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
}
#endif

} // namespace TestTiledMeshFormat
