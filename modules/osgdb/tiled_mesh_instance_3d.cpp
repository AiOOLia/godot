/**************************************************************************/
/*  tiled_mesh_instance_3d.cpp                                            */
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

#include "tiled_mesh_instance_3d.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/templates/hash_set.h"
#include "osg_data_source.h"
#include "osg_data_request_queue.h"
#include "osg_image_decode_queue.h"
#include "osg_parse_queue.h"
#include "osg_scene_converter.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/mesh.h"
#include "servers/rendering/rendering_server.h"

namespace {

static bool aabb_intersects_camera_frustum(const AABB &p_aabb, Camera3D *p_camera) {
	if (!p_camera) {
		return false;
	}
	const Vector3 half_extents = p_aabb.size * 0.5f;
	const Vector3 center = p_aabb.position + half_extents;
	for (const Plane &plane : p_camera->get_frustum()) {
		Vector3 nearest(
				plane.normal.x > 0.0f ? -half_extents.x : half_extents.x,
				plane.normal.y > 0.0f ? -half_extents.y : half_extents.y,
				plane.normal.z > 0.0f ? -half_extents.z : half_extents.z);
		if (plane.is_point_over(center + nearest)) {
			return false;
		}
	}
	return true;
}

static uint64_t refinement_cache_key(int p_parent, int p_fallback_group) {
	return (uint64_t(uint32_t(p_parent)) << 32) | uint32_t(p_fallback_group);
}

} // namespace

void TiledMeshInstance3D::_clear_render_tiles() {
	_cancel_texture_requests_for_tile();
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		render_tiles.clear();
		return;
	}
	for (const TileRenderInstance &tile : render_tiles) {
		if (tile.instance.is_valid()) {
			rs->free_rid(tile.instance);
		}
		if (tile.mesh.is_valid()) {
			rs->free_rid(tile.mesh);
		}
	}
	render_tiles.clear();
	free_render_tiles.clear();
	root_render_tiles.clear();
	resident_aabb = AABB();
}

void TiledMeshInstance3D::_cancel_texture_requests_for_tile(int p_render_tile) {
	OsgDataRequestQueue *data_queue = OsgDataRequestQueue::get_singleton();
	OsgImageDecodeQueue *decode_queue = OsgImageDecodeQueue::get_singleton();
	for (int i = texture_requests.size() - 1; i >= 0; i--) {
		if (p_render_tile >= 0 && texture_requests[i].render_tile != p_render_tile) {
			continue;
		}
		if (data_queue && texture_requests[i].data_request_id != 0) {
			data_queue->cancel(texture_requests[i].data_request_id);
		}
		if (decode_queue && texture_requests[i].decode_request_id != 0) {
			decode_queue->cancel(texture_requests[i].decode_request_id);
		}
		texture_requests.remove_at(i);
	}
}

void TiledMeshInstance3D::_free_render_tile(int p_index, bool p_recompute_bounds) {
	if (p_index < 0 || p_index >= render_tiles.size()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	TileRenderInstance &tile = render_tiles.write[p_index];
	if (!tile.instance.is_valid() && !tile.mesh.is_valid()) {
		return;
	}
	_cancel_texture_requests_for_tile(p_index);
	if (rs) {
		if (tile.instance.is_valid()) {
			rs->free_rid(tile.instance);
		}
		if (tile.mesh.is_valid()) {
			rs->free_rid(tile.mesh);
		}
	}
	Vector<int> *owner_render_tiles = tile.owner_tile < 0 ? &root_render_tiles :
			(tile.owner_tile < streaming_tiles.size() ? &streaming_tiles.write[tile.owner_tile].render_tiles : nullptr);
	if (owner_render_tiles) {
		owner_render_tiles->erase(p_index);
	}
	tile = TileRenderInstance();
	free_render_tiles.push_back(p_index);
	if (p_recompute_bounds) {
		_recompute_resident_aabb();
	}
}

int TiledMeshInstance3D::_store_render_tile(const TileRenderInstance &p_tile) {
	int render_index = -1;
	if (!free_render_tiles.is_empty()) {
		render_index = free_render_tiles[free_render_tiles.size() - 1];
		free_render_tiles.resize(free_render_tiles.size() - 1);
		render_tiles.write[render_index] = p_tile;
	} else {
		render_tiles.push_back(p_tile);
		render_index = render_tiles.size() - 1;
	}
	if (p_tile.owner_tile < 0) {
		root_render_tiles.push_back(render_index);
	} else if (p_tile.owner_tile < streaming_tiles.size()) {
		streaming_tiles.write[p_tile.owner_tile].render_tiles.push_back(render_index);
	}
	return render_index;
}

const Vector<int> *TiledMeshInstance3D::_get_owner_render_tiles(int p_owner_tile) const {
	if (p_owner_tile < 0) {
		return &root_render_tiles;
	}
	return p_owner_tile < streaming_tiles.size() ? &streaming_tiles[p_owner_tile].render_tiles : nullptr;
}

void TiledMeshInstance3D::_recompute_resident_aabb() {
	bool has_aabb = false;
	AABB combined;
	for (const TileRenderInstance &tile : render_tiles) {
		if (!tile.instance.is_valid()) {
			continue;
		}
		AABB tile_aabb = tile.aabb;
		tile_aabb.position += world_to_local(tile.source_center_x, tile.source_center_y, tile.source_center_z);
		if (!has_aabb) {
			combined = tile_aabb;
			has_aabb = true;
		} else {
			combined = combined.merge(tile_aabb);
		}
	}
	resident_aabb = has_aabb ? combined : AABB();
}

Transform3D TiledMeshInstance3D::_get_tile_world_transform(const TileRenderInstance &p_tile) const {
	Transform3D tile_transform;
	tile_transform.origin = world_to_local(p_tile.source_center_x, p_tile.source_center_y, p_tile.source_center_z);
	return get_global_transform() * tile_transform;
}

Error TiledMeshInstance3D::_upload_document(const OsgTileDocument &p_document, String &r_error, int p_owner_tile) {
	r_error.clear();
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		r_error = "RenderingServer is not available.";
		return ERR_UNAVAILABLE;
	}
	if (p_document.surfaces.is_empty()) {
		// A pure paging root is valid. The scheduler will request its children.
		return OK;
	}

	Vector<Pair<int, int>> render_groups;
	for (const OsgMeshSurfacePayload &surface : p_document.surfaces) {
		if (surface.vertices.is_empty() || surface.indices.is_empty()) {
			continue;
		}
		const Pair<int, int> key(surface.fallback_group, surface.lod_branch);
		bool found = false;
		for (const Pair<int, int> &existing : render_groups) {
			if (existing.first == key.first && existing.second == key.second) {
				found = true;
				break;
			}
		}
		if (!found) {
			render_groups.push_back(key);
		}
	}
	if (render_groups.is_empty()) {
		r_error = "No valid triangle surfaces could be uploaded.";
		return ERR_FILE_CORRUPT;
	}

	Vector<int> created_render_tiles;
	for (const Pair<int, int> &render_group : render_groups) {
		TileRenderInstance tile;
		tile.owner_tile = p_owner_tile;
		tile.fallback_group = render_group.first;
		tile.lod_branch = render_group.second;
		int branch_index = tile.lod_branch;
		int branch_guard = 0;
		while (branch_index >= 0 && branch_index < p_document.lod_ranges.size() && branch_guard++ <= p_document.lod_ranges.size()) {
			const OsgLodRangePayload &branch = p_document.lod_ranges[branch_index];
			tile.lod_ranges.push_back(branch);
			branch_index = branch.parent;
		}
		tile.source_center_x = p_document.source_origin.x;
		tile.source_center_y = p_document.source_origin.y;
		tile.source_center_z = p_document.source_origin.z;
		tile.mesh = rs->mesh_create();
		if (!tile.mesh.is_valid()) {
			r_error = "Unable to create a RenderingServer mesh.";
			break;
		}
		bool has_aabb = false;
		Vector<Pair<int, bool>> material_keys;
		Vector<Pair<int, String>> pending_textures;
		for (const OsgMeshSurfacePayload &surface : p_document.surfaces) {
			if (surface.fallback_group != render_group.first || surface.lod_branch != render_group.second || surface.vertices.is_empty() || surface.indices.is_empty()) {
				continue;
			}
			Array arrays;
			arrays.resize(Mesh::ARRAY_MAX);
			arrays[Mesh::ARRAY_VERTEX] = surface.vertices;
			arrays[Mesh::ARRAY_INDEX] = surface.indices;
			if (surface.normals.size() == surface.vertices.size()) arrays[Mesh::ARRAY_NORMAL] = surface.normals;
			if (surface.uv0.size() == surface.vertices.size()) arrays[Mesh::ARRAY_TEX_UV] = surface.uv0;
			if (surface.colors.size() == surface.vertices.size()) arrays[Mesh::ARRAY_COLOR] = surface.colors;
			rs->mesh_add_surface_from_arrays(tile.mesh, RSE::PRIMITIVE_TRIANGLES, arrays);

			const Pair<int, bool> material_key(surface.material, !surface.colors.is_empty());
			int material_index = -1;
			for (int i = 0; i < material_keys.size(); i++) {
				if (material_keys[i] == material_key) {
					material_index = i;
					break;
				}
			}
			if (material_index < 0) {
				Ref<StandardMaterial3D> material;
				material.instantiate();
				String external_texture_uri;
				if (surface.material >= 0 && surface.material < p_document.materials.size()) {
					const OsgMaterialPayload &payload = p_document.materials[surface.material];
					material->set_albedo(payload.albedo);
					material->set_cull_mode(payload.double_sided ? BaseMaterial3D::CULL_DISABLED : BaseMaterial3D::CULL_BACK);
					if (payload.alpha_test) {
						material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
						material->set_alpha_scissor_threshold(payload.alpha_cutoff);
					} else if (payload.transparent || payload.albedo.a < 0.999f) {
						material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
					}
					if (payload.albedo_image.has_pixels()) {
						const Ref<Image> image = Image::create_from_data(payload.albedo_image.width, payload.albedo_image.height, false, Image::FORMAT_RGBA8, payload.albedo_image.rgba8);
						material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(image));
					} else {
						external_texture_uri = payload.albedo_image.uri;
					}
				}
				if (!surface.colors.is_empty()) {
					material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
					material->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
				}
				material_index = tile.materials.size();
				material_keys.push_back(material_key);
				tile.materials.push_back(material);
				if (!external_texture_uri.is_empty()) {
					pending_textures.push_back(Pair<int, String>(material_index, external_texture_uri));
				}
			}
			rs->mesh_surface_set_material(tile.mesh, rs->mesh_get_surface_count(tile.mesh) - 1, tile.materials[material_index]->get_rid());
			tile.aabb = has_aabb ? tile.aabb.merge(surface.aabb) : surface.aabb;
			has_aabb = true;
			tile.estimated_bytes += surface.estimated_bytes;
		}
		if (rs->mesh_get_surface_count(tile.mesh) == 0) {
			rs->free_rid(tile.mesh);
			continue;
		}
		rs->mesh_set_custom_aabb(tile.mesh, tile.aabb);
		tile.instance = rs->instance_create();
		if (!tile.instance.is_valid()) {
			rs->free_rid(tile.mesh);
			r_error = "Unable to create a RenderingServer instance.";
			break;
		}
		rs->instance_set_base(tile.instance, tile.mesh);
		rs->instance_attach_object_instance_id(tile.instance, get_instance_id());
		const int render_index = _store_render_tile(tile);
		created_render_tiles.push_back(render_index);
		if (OsgDataRequestQueue *data_queue = OsgDataRequestQueue::get_singleton()) {
			for (const Pair<int, String> &pending : pending_textures) {
				TextureRequest request;
				request.render_tile = render_index;
				request.material_index = pending.first;
				request.uri = pending.second;
				request.data_request_id = data_queue->request(get_instance_id(), generation, request.uri, uint64_t(max_tile_size_mb) * 1024 * 1024, request_timeout_sec, retry_count, -1.0);
				if (request.data_request_id != 0) {
					texture_requests.push_back(request);
				}
			}
		}
	}
	if (!r_error.is_empty()) {
		for (int render_index : created_render_tiles) {
			_free_render_tile(render_index, false);
		}
		_recompute_resident_aabb();
		return ERR_CANT_CREATE;
	}
	_recompute_resident_aabb();
	// Only the instances created by this upload need their immutable render
	// state initialized. Re-synchronizing every resident tile here turns a
	// burst of small GIS tile uploads into quadratic RenderingServer traffic.
	for (int render_index : created_render_tiles) {
		_sync_render_instance(render_tiles[render_index]);
	}
	return OK;
}

void TiledMeshInstance3D::_sync_render_instance(const TileRenderInstance &p_tile) {
	if (!p_tile.instance.is_valid()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	RID scenario;
	if (is_inside_tree() && get_world_3d().is_valid()) {
		scenario = get_world_3d()->get_scenario();
	}
	rs->instance_set_scenario(p_tile.instance, scenario);
	rs->instance_set_transform(p_tile.instance, _get_tile_world_transform(p_tile));
	rs->instance_set_layer_mask(p_tile.instance, get_layer_mask());
	rs->instance_set_visible(p_tile.instance, is_visible_in_tree() && p_tile.visible);
	rs->instance_geometry_set_cast_shadows_setting(p_tile.instance, (RSE::ShadowCastingSetting)cast_shadow);
}

void TiledMeshInstance3D::_sync_render_instances() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	const uint32_t layer_mask = get_layer_mask();
	const bool node_visible = is_visible_in_tree();
	RID scenario;
	if (is_inside_tree() && get_world_3d().is_valid()) {
		scenario = get_world_3d()->get_scenario();
	}
	for (const TileRenderInstance &tile : render_tiles) {
		if (!tile.instance.is_valid()) {
			continue;
		}
		rs->instance_set_scenario(tile.instance, scenario);
		rs->instance_set_transform(tile.instance, _get_tile_world_transform(tile));
		rs->instance_set_layer_mask(tile.instance, layer_mask);
		rs->instance_set_visible(tile.instance, node_visible && tile.visible);
		rs->instance_geometry_set_cast_shadows_setting(tile.instance, (RSE::ShadowCastingSetting)cast_shadow);
	}
	last_layer_mask = layer_mask;
	last_visible = node_visible;
}

Camera3D *TiledMeshInstance3D::_resolve_runtime_camera() const {
	if (!camera_path.is_empty()) {
		return Object::cast_to<Camera3D>(get_node_or_null(camera_path));
	}
	Viewport *viewport = get_viewport();
	return viewport ? viewport->get_camera_3d() : nullptr;
}

void TiledMeshInstance3D::_clear_streaming_tiles() {
	OsgParseQueue *queue = OsgParseQueue::get_singleton();
	OsgDataRequestQueue *data_queue = OsgDataRequestQueue::get_singleton();
	for (StreamingTile &tile : streaming_tiles) {
		if (data_queue && tile.data_request_id != 0) {
			data_queue->cancel(tile.data_request_id);
		}
		if (queue && tile.parse_request_id != 0) {
			queue->cancel(tile.parse_request_id);
		}
		tile.data_request_id = 0;
		tile.parse_request_id = 0;
		tile.has_cpu_document = false;
		tile.cpu_document = OsgTileDocument();
	}
	streaming_tiles.clear();
	streaming_tile_by_uri.clear();
	root_children.clear();
	display_camera_id = ObjectID();
	streaming_camera_states.clear();
	streaming_topology_dirty = true;
}

void TiledMeshInstance3D::_register_external_tiles(const OsgTileDocument &p_document, int p_parent) {
	if (!p_document.external_tiles.is_empty()) {
		streaming_topology_dirty = true;
	}
	for (const OsgExternalTileRef &source_reference : p_document.external_tiles) {
		OsgExternalTileRef reference = source_reference;
		reference.uri = OsgDataSource::normalize_uri(reference.uri);
		if (reference.uri.is_empty()) {
			continue;
		}

		int tile_index = -1;
		if (const int *existing = streaming_tile_by_uri.getptr(reference.uri)) {
			tile_index = *existing;
		} else {
			StreamingTile tile;
			tile.reference = reference;
			tile.parent = p_parent;
			if (const Vector<int> *parent_render_tiles = _get_owner_render_tiles(p_parent)) {
				for (int render_index : *parent_render_tiles) {
					if (render_index < 0 || render_index >= render_tiles.size()) {
						continue;
					}
					const TileRenderInstance &render_tile = render_tiles[render_index];
					if (!render_tile.instance.is_valid() || render_tile.fallback_group != reference.fallback_group) {
						continue;
					}
					AABB fallback_aabb = render_tile.aabb;
					fallback_aabb.position += world_to_local(render_tile.source_center_x, render_tile.source_center_y, render_tile.source_center_z);
					tile.fallback_aabb = tile.has_fallback_aabb ? tile.fallback_aabb.merge(fallback_aabb) : fallback_aabb;
					tile.has_fallback_aabb = true;
				}
			}
			streaming_tiles.push_back(tile);
			tile_index = streaming_tiles.size() - 1;
			streaming_tile_by_uri.insert(reference.uri, tile_index);
		}
		if (tile_index == p_parent) {
			continue;
		}

		Vector<int> *children = p_parent < 0 ? &root_children : &streaming_tiles.write[p_parent].children;
		if (!children->has(tile_index)) {
			children->push_back(tile_index);
		}
	}
}

bool TiledMeshInstance3D::_parent_is_desired(int p_parent) const {
	if (p_parent < 0) {
		return root_tile_state == TILE_STATE_RESIDENT;
	}
	return p_parent < streaming_tiles.size() && streaming_tiles[p_parent].desired;
}

bool TiledMeshInstance3D::_parent_is_display_desired(int p_parent) const {
	if (p_parent < 0) {
		return root_tile_state == TILE_STATE_RESIDENT;
	}
	return p_parent < streaming_tiles.size() && streaming_tiles[p_parent].display_desired;
}

bool TiledMeshInstance3D::_tile_is_displayable(int p_index) const {
	if (p_index < 0 || p_index >= streaming_tiles.size()) {
		return false;
	}
	if (tile_displayable_cache.size() != streaming_tiles.size()) {
		tile_displayable_cache.resize(streaming_tiles.size());
		tile_displayable_cache.fill(0);
	}
	const uint8_t cached = tile_displayable_cache[p_index];
	if (cached >= 2) {
		return cached == 3;
	}
	if (cached == 1) {
		return false;
	}
	tile_displayable_cache.write[p_index] = 1;
	const auto cache_result = [this, p_index](bool p_displayable) {
		tile_displayable_cache.write[p_index] = p_displayable ? 3 : 2;
		return p_displayable;
	};
	const StreamingTile &tile = streaming_tiles[p_index];
	if (tile.state != TILE_STATE_RESIDENT) {
		return cache_result(false);
	}
	Camera3D *display_camera = display_camera_id.is_valid() ? ObjectDB::get_instance<Camera3D>(display_camera_id) : nullptr;
	HashSet<int> required_fallback_groups;
	if (const Vector<int> *owner_render_tiles = _get_owner_render_tiles(p_index)) {
		for (int render_index : *owner_render_tiles) {
			const TileRenderInstance &render_tile = render_tiles[render_index];
			if (render_tile.instance.is_valid() && render_tile.fallback_group >= 0) {
				required_fallback_groups.insert(render_tile.fallback_group);
			}
		}
	}
	for (int child_index : tile.children) {
		if (child_index >= 0 && child_index < streaming_tiles.size() && streaming_tiles[child_index].reference.fallback_group >= 0) {
			required_fallback_groups.insert(streaming_tiles[child_index].reference.fallback_group);
		}
	}
	if (!required_fallback_groups.is_empty()) {
		// An external OSGB may contain several independent spatial PagedLOD
		// groups. It covers its parent only when every group has either its own
		// fallback geometry or a complete resident refinement. Treating the first
		// visible group as sufficient hides the whole parent and leaves holes in
		// the still-uncovered groups.
		for (int fallback_group : required_fallback_groups) {
			bool group_displayable = _parent_group_is_fully_refined(p_index, fallback_group);
			const bool refinement_pending = _parent_group_has_display_refinement(p_index, fallback_group) && !group_displayable;
			if (!group_displayable) {
				for (int render_index : *_get_owner_render_tiles(p_index)) {
					const TileRenderInstance &render_tile = render_tiles[render_index];
					if (render_tile.fallback_group == fallback_group && render_tile.instance.is_valid() &&
							(_render_lod_matches(render_tile, display_camera) || refinement_pending)) {
						group_displayable = true;
						break;
					}
				}
			}
			if (!group_displayable) {
				return cache_result(false);
			}
		}
		return cache_result(true);
	}
	for (int render_index : *_get_owner_render_tiles(p_index)) {
		const TileRenderInstance &render_tile = render_tiles[render_index];
		if (!render_tile.instance.is_valid()) {
			continue;
		}
		const bool refined = render_tile.fallback_group >= 0 ? _parent_group_is_fully_refined(p_index, render_tile.fallback_group) : _parent_is_fully_refined(p_index);
		const bool refinement_pending = render_tile.fallback_group >= 0 && _parent_group_has_display_refinement(p_index, render_tile.fallback_group) && !refined;
		if (!refined && (_render_lod_matches(render_tile, display_camera) || refinement_pending)) {
			return cache_result(true);
		}
	}
	// Paging-only documents, and documents whose own geometry has already been
	// replaced, are displayable only when their active descendants really are.
	// A resident RID alone is insufficient: its inline LOD range may currently
	// be inactive, which would otherwise hide the parent and create a tile hole.
	return cache_result(_parent_is_fully_refined(p_index));
}

bool TiledMeshInstance3D::_owner_has_render_geometry(int p_owner_tile) const {
	const Vector<int> *owner_render_tiles = _get_owner_render_tiles(p_owner_tile);
	if (!owner_render_tiles) {
		return false;
	}
	for (int render_index : *owner_render_tiles) {
		if (render_tiles[render_index].instance.is_valid()) {
			return true;
		}
	}
	return false;
}

bool TiledMeshInstance3D::_parent_is_fully_refined(int p_parent) const {
	if (const bool *cached = parent_refined_cache.getptr(p_parent)) {
		return *cached;
	}
	const Vector<int> &children = p_parent < 0 ? root_children : streaming_tiles[p_parent].children;
	bool has_desired_child = false;
	for (int child_index : children) {
		if (child_index < 0 || child_index >= streaming_tiles.size() || !streaming_tiles[child_index].display_desired) {
			continue;
		}
		has_desired_child = true;
		if (!_tile_is_displayable(child_index)) {
			parent_refined_cache.insert(p_parent, false);
			return false;
		}
	}
	parent_refined_cache.insert(p_parent, has_desired_child);
	return has_desired_child;
}

bool TiledMeshInstance3D::_parent_group_is_fully_refined(int p_parent, int p_fallback_group) const {
	const uint64_t cache_key = refinement_cache_key(p_parent, p_fallback_group);
	if (const bool *cached = group_refined_cache.getptr(cache_key)) {
		return *cached;
	}
	const Vector<int> &children = p_parent < 0 ? root_children : streaming_tiles[p_parent].children;
	bool has_desired_child = false;
	for (int child_index : children) {
		if (child_index < 0 || child_index >= streaming_tiles.size()) {
			continue;
		}
		const StreamingTile &child = streaming_tiles[child_index];
		if (child.reference.fallback_group != p_fallback_group || !child.display_desired) {
			continue;
		}
		has_desired_child = true;
		if (!_tile_is_displayable(child_index)) {
			group_refined_cache.insert(cache_key, false);
			return false;
		}
	}
	group_refined_cache.insert(cache_key, has_desired_child);
	return has_desired_child;
}

bool TiledMeshInstance3D::_parent_group_has_display_refinement(int p_parent, int p_fallback_group) const {
	const uint64_t cache_key = refinement_cache_key(p_parent, p_fallback_group);
	if (const bool *cached = group_pending_cache.getptr(cache_key)) {
		return *cached;
	}
	const Vector<int> &children = p_parent < 0 ? root_children : streaming_tiles[p_parent].children;
	for (int child_index : children) {
		if (child_index < 0 || child_index >= streaming_tiles.size()) {
			continue;
		}
		const StreamingTile &child = streaming_tiles[child_index];
		if (child.reference.fallback_group == p_fallback_group && child.display_desired) {
			group_pending_cache.insert(cache_key, true);
			return true;
		}
	}
	group_pending_cache.insert(cache_key, false);
	return false;
}

double TiledMeshInstance3D::_get_lod_metric(const OsgVector3d &p_center, double p_radius, OsgLodRangePayload::RangeMode p_range_mode, Camera3D *p_camera) const {
	if (!p_camera) {
		return 0.0;
	}
	const Vector3 local_center = world_to_local(p_center.x, p_center.y, p_center.z);
	const Vector3 global_center = get_global_transform().xform(local_center);
	const Vector3 global_scale = get_global_transform().basis.get_scale().abs();
	const double maximum_global_scale = MAX(global_scale.x, MAX(global_scale.y, global_scale.z));
	const double radius = MAX(p_radius * unit_scale * maximum_global_scale, 0.0);
	const double center_distance = p_camera->get_camera_transform().origin.distance_to(global_center);
	const double active_bias = Engine::get_singleton()->is_editor_hint() ? editor_preview_lod_bias : lod_bias;

	if (p_range_mode == OsgLodRangePayload::PIXEL_SIZE_ON_SCREEN) {
		Viewport *camera_viewport = p_camera->get_viewport();
		const Vector2 viewport_size = camera_viewport ? camera_viewport->get_visible_rect().size : Vector2(1.0, 1.0);
		const Projection projection = p_camera->get_camera_projection();
		const double focal_pixels_x = Math::abs(double(projection.columns[0].x)) * viewport_size.x * 0.5;
		const double focal_pixels_y = Math::abs(double(projection.columns[1].y)) * viewport_size.y * 0.5;
		// This is the same scalar used by osg::CullingSet::pixelSize(): the
		// radius is projected using the RMS focal length in pixels. Perspective
		// projection divides by camera-space depth, not by Euclidean distance to
		// the center. Euclidean distance underestimates a tile near the edge of
		// the view and makes PIXEL_SIZE_ON_SCREEN LOD switch with viewing angle.
		const double focal_pixels = Math::sqrt((focal_pixels_x * focal_pixels_x + focal_pixels_y * focal_pixels_y) * 0.5);
		double pixel_diameter = radius * 2.0 * focal_pixels;
		if (p_camera->get_projection() != Camera3D::PROJECTION_ORTHOGONAL) {
			const Vector3 view_center = p_camera->get_camera_transform().affine_inverse().xform(global_center);
			pixel_diameter /= MAX(Math::abs(double(view_center.z)), 0.000001);
		}
		return pixel_diameter / MAX(active_bias, 0.01);
	}
	// Match osg::LOD/osg::PagedLOD exactly: DISTANCE_FROM_EYE_POINT uses
	// the distance to the LOD center, not the nearest point of its bound.
	// Subtracting the radius desynchronizes a parent reference from the LOD
	// inside its external child whenever their authored radii differ.
	return center_distance * active_bias;
}

bool TiledMeshInstance3D::_range_matches(const OsgVector3d &p_center, double p_radius, float p_min_range, float p_max_range, OsgLodRangePayload::RangeMode p_range_mode, bool p_was_matching, Camera3D *p_camera) const {
	if (!p_camera) {
		return false;
	}
	const double metric = _get_lod_metric(p_center, p_radius, p_range_mode, p_camera);
	double minimum = p_min_range;
	double maximum = p_max_range;
	if (p_was_matching) {
		minimum *= 1.0 - lod_hysteresis;
		maximum *= 1.0 + lod_hysteresis;
	}
	return metric >= minimum && metric < maximum;
}

bool TiledMeshInstance3D::_render_lod_matches(const TileRenderInstance &p_tile, Camera3D *p_camera) const {
	if (p_tile.lod_ranges.is_empty()) {
		return true;
	}
	for (const OsgLodRangePayload &range : p_tile.lod_ranges) {
		// Inline LOD branches are already resident and can switch atomically.
		// Applying the streaming hysteresis to every sibling independently makes
		// adjacent ranges overlap (and newly uploaded siblings used to all start
		// in the matching state). That renders the coarse and detailed geometry at
		// the same time, causing the coarse mesh to cover or Z-fight the detail.
		// RenderingServer performs conservative culling from the uploaded mesh's
		// actual AABB. Do not cull resident geometry using the producer-authored
		// LOD sphere: real GIS datasets can contain slightly undersized spheres.
		if (!_range_matches(range.center, range.radius, range.min_range, range.max_range, range.range_mode, false, p_camera)) {
			return false;
		}
	}
	return true;
}

bool TiledMeshInstance3D::_range_matches(const StreamingTile &p_tile, Camera3D *p_camera) const {
	if (!_range_matches(p_tile.reference.center, p_tile.reference.radius, p_tile.reference.min_range, p_tile.reference.max_range,
				p_tile.reference.range_mode, p_tile.desired, p_camera)) {
		return false;
	}
	if (p_tile.state == TILE_STATE_RESIDENT) {
		// RenderingServer culls resident geometry from its real mesh AABB.
		return true;
	}

	// Before the external file is resident, its parent's fallback surface is a
	// more reliable spatial bound than the frequently undersized PagedLOD sphere
	// produced by photogrammetry tools. Use the actual uploaded fallback AABB to
	// decide whether this child must be requested.
	if (p_tile.has_fallback_aabb) {
		return aabb_intersects_camera_frustum(get_global_transform().xform(p_tile.fallback_aabb), p_camera);
	}

	// Pure paging nodes have no resident fallback geometry; conservatively pad
	// their declared sphere to tolerate small authoring errors.
	const Vector3 local_center = world_to_local(p_tile.reference.center.x, p_tile.reference.center.y, p_tile.reference.center.z);
	const Vector3 global_center = get_global_transform().xform(local_center);
	const Vector3 global_scale = get_global_transform().basis.get_scale().abs();
	const double maximum_global_scale = MAX(global_scale.x, MAX(global_scale.y, global_scale.z));
	const double padded_radius = MAX((p_tile.reference.radius * 1.1 + 0.1) * unit_scale * maximum_global_scale, 0.0);
	for (const Plane &plane : p_camera->get_frustum()) {
		if (plane.distance_to(global_center) > padded_radius) {
			return false;
		}
	}
	return true;
}

double TiledMeshInstance3D::_get_request_priority(const StreamingTile &p_tile, Camera3D *p_camera, bool p_active_camera) const {
	if (!p_camera) {
		return 0.0;
	}
	Vector3 global_center;
	double global_radius = 0.0;
	if (p_tile.has_fallback_aabb) {
		const AABB global_aabb = get_global_transform().xform(p_tile.fallback_aabb);
		global_center = global_aabb.get_center();
		global_radius = global_aabb.size.length() * 0.5;
	} else {
		const Vector3 local_center = world_to_local(p_tile.reference.center.x, p_tile.reference.center.y, p_tile.reference.center.z);
		global_center = get_global_transform().xform(local_center);
		const Vector3 global_scale = get_global_transform().basis.get_scale().abs();
		const double maximum_global_scale = MAX(global_scale.x, MAX(global_scale.y, global_scale.z));
		global_radius = p_tile.reference.radius * unit_scale * maximum_global_scale;
	}

	Viewport *viewport = p_camera->get_viewport();
	const Vector2 viewport_size = viewport ? viewport->get_visible_rect().size : Vector2(1.0, 1.0);
	const Projection projection = p_camera->get_camera_projection();
	const double focal_pixels_x = Math::abs(double(projection.columns[0].x)) * viewport_size.x * 0.5;
	const double focal_pixels_y = Math::abs(double(projection.columns[1].y)) * viewport_size.y * 0.5;
	const double focal_pixels = Math::sqrt((focal_pixels_x * focal_pixels_x + focal_pixels_y * focal_pixels_y) * 0.5);
	double pixel_diameter = MAX(global_radius * 2.0 * focal_pixels, 1.0);
	if (p_camera->get_projection() != Camera3D::PROJECTION_ORTHOGONAL) {
		const Vector3 view_center = p_camera->get_camera_transform().affine_inverse().xform(global_center);
		const double nearest_depth = Math::abs(double(view_center.z)) - global_radius;
		if (nearest_depth <= 0.000001) {
			pixel_diameter = viewport_size.length();
		} else {
			pixel_diameter /= nearest_depth;
		}
	}

	// Projected screen area estimates how much of the current image this tile
	// can improve. Its position inside the authored LOD interval estimates how
	// urgently that improvement is needed: nearer is more urgent for distance
	// LOD, while larger projected size is more urgent for pixel-size LOD.
	const double viewport_area = MAX(double(viewport_size.x) * viewport_size.y, 1.0);
	double projected_area = viewport_area;
	if (!p_camera->is_position_behind(global_center)) {
		const Vector2 screen_center = p_camera->unproject_position(global_center);
		const Rect2 projected_rect(screen_center - Vector2(pixel_diameter, pixel_diameter) * 0.5, Vector2(pixel_diameter, pixel_diameter));
		const Rect2 visible_rect = projected_rect.intersection(Rect2(Vector2(), viewport_size));
		projected_area = MAX(double(visible_rect.get_area()), 1.0);
	}
	const double metric = _get_lod_metric(p_tile.reference.center, p_tile.reference.radius, p_tile.reference.range_mode, p_camera);
	const double minimum = p_tile.reference.min_range;
	const double maximum = p_tile.reference.max_range;
	double range_urgency = 0.5;
	if (Math::is_finite(maximum) && maximum > minimum + CMP_EPSILON) {
		if (p_tile.reference.range_mode == OsgLodRangePayload::PIXEL_SIZE_ON_SCREEN) {
			range_urgency = CLAMP((metric - minimum) / (maximum - minimum), 0.0, 1.0);
		} else {
			range_urgency = CLAMP((maximum - metric) / (maximum - minimum), 0.0, 1.0);
		}
	}
	const double active_viewport_weight = p_active_camera ? 1.25 : 1.0;
	const double missing_fallback_weight = p_tile.has_fallback_aabb ? 1.0 : 4.0;
	return projected_area * (1.0 + range_urgency) * active_viewport_weight * missing_fallback_weight;
}

void TiledMeshInstance3D::_update_streaming(Camera3D *p_camera) {
	Vector<Camera3D *> cameras;
	if (p_camera) {
		cameras.push_back(p_camera);
	}
	_update_streaming_from_cameras(cameras, p_camera);
}

void TiledMeshInstance3D::_update_streaming_from_cameras(const Vector<Camera3D *> &p_cameras, Camera3D *p_display_camera) {
	Camera3D *display_camera = p_display_camera ? p_display_camera : (p_cameras.is_empty() ? nullptr : p_cameras[0]);
	const ObjectID new_display_camera_id = display_camera ? display_camera->get_instance_id() : ObjectID();
	Vector<StreamingCameraState> camera_states;
	for (Camera3D *camera : p_cameras) {
		if (!camera) {
			continue;
		}
		StreamingCameraState state;
		state.id = camera->get_instance_id();
		state.transform = camera->get_camera_transform();
		state.projection = camera->get_camera_projection();
		Viewport *camera_viewport = camera->get_viewport();
		state.viewport_size = camera_viewport ? camera_viewport->get_visible_rect().size : Vector2(1.0, 1.0);
		camera_states.push_back(state);
	}
	bool cameras_changed = camera_states.size() != streaming_camera_states.size() || new_display_camera_id != display_camera_id;
	for (int i = 0; !cameras_changed && i < camera_states.size(); i++) {
		const StreamingCameraState &current = camera_states[i];
		const StreamingCameraState &previous = streaming_camera_states[i];
		cameras_changed = current.id != previous.id || current.transform != previous.transform || current.projection != previous.projection || current.viewport_size != previous.viewport_size;
	}
	if (!streaming_topology_dirty && !cameras_changed) {
		return;
	}
	streaming_camera_states = camera_states;
	display_camera_id = new_display_camera_id;
	streaming_topology_dirty = false;
	const uint64_t now = Time::get_singleton()->get_ticks_usec();
	OsgDataRequestQueue *data_queue = OsgDataRequestQueue::get_singleton();
	OsgParseQueue *parse_queue = OsgParseQueue::get_singleton();
	for (int i = 0; i < streaming_tiles.size(); i++) {
		StreamingTile &tile = streaming_tiles.write[i];
		bool range_matches = false;
		double request_priority = 0.0;
		for (Camera3D *camera : p_cameras) {
			if (_range_matches(tile, camera)) {
				range_matches = true;
				request_priority = MAX(request_priority, _get_request_priority(tile, camera, camera == display_camera));
			}
		}
		const bool desired = _parent_is_desired(tile.parent) && range_matches;
		const bool display_desired = _parent_is_display_desired(tile.parent) && _range_matches(tile.reference.center, tile.reference.radius, tile.reference.min_range, tile.reference.max_range,
				tile.reference.range_mode, tile.display_desired, display_camera);
		if (!desired && (tile.data_request_id != 0 || tile.parse_request_id != 0)) {
			if (tile.data_request_id != 0) {
				if (data_queue) {
					data_queue->cancel(tile.data_request_id);
				}
				tile.data_request_id = 0;
			}
			if (parse_queue) {
				if (tile.parse_request_id != 0) {
					parse_queue->cancel(tile.parse_request_id);
				}
			}
			tile.parse_request_id = 0;
			tile.source_bytes = 0;
			tile.has_cpu_document = false;
			tile.cpu_document = OsgTileDocument();
			tile.state = TILE_STATE_CANCELLED;
		}
		tile.desired = desired;
		tile.display_desired = display_desired;
		tile.request_priority = request_priority;
		if (desired) {
			tile.last_used_usec = now;
			if (data_queue && tile.data_request_id != 0) {
				data_queue->update_priority(tile.data_request_id, request_priority);
			}
			if (parse_queue && tile.parse_request_id != 0) {
				parse_queue->update_priority(tile.parse_request_id, request_priority);
			}
		}
	}
	_request_streaming_tiles();
	_update_streaming_visibility();
	_enforce_memory_budget();
}

void TiledMeshInstance3D::_request_streaming_tiles() {
	const int configured_limit = Engine::get_singleton()->is_editor_hint() ? editor_preview_max_requests : max_concurrent_requests;
	const int request_limit = configured_limit > 0 ? configured_limit : (OS::get_singleton()->has_feature("web") ? 4 : 8);
	OsgDataRequestQueue *data_queue = OsgDataRequestQueue::get_singleton();
	if (!data_queue) {
		return;
	}

	for (;;) {
		int best_tile = -1;
		double best_priority = -1.0;
		for (int i = 0; i < streaming_tiles.size(); i++) {
			const StreamingTile &candidate = streaming_tiles[i];
			if (candidate.desired && (candidate.state == TILE_STATE_UNLOADED || candidate.state == TILE_STATE_CANCELLED) && candidate.request_priority > best_priority) {
				best_priority = candidate.request_priority;
				best_tile = i;
			}
		}
		if (best_tile < 0) {
			break;
		}
		// Admit the best known visual contributions, rather than letting older
		// low-value work reserve every pipeline slot. A newly discovered child
		// may enter even while lower-priority reads/parses are still pending;
		// the priority-aware data and parser queues will place it ahead of them.
		// This is bounded to the top request_limit priorities at each refinement
		// frontier and avoids cancelling an OSG parse that is already running.
		int higher_priority_pending = (root_data_request_id != 0 || root_parse_request_id != 0) ? 1 : 0;
		for (const StreamingTile &pending : streaming_tiles) {
			const bool in_pipeline = pending.state == TILE_STATE_REQUESTED || pending.state == TILE_STATE_DOWNLOADED || pending.state == TILE_STATE_PARSING;
			if (in_pipeline && pending.request_priority >= best_priority) {
				higher_priority_pending++;
			}
		}
		if (higher_priority_pending >= request_limit) {
			break;
		}
		StreamingTile &tile = streaming_tiles.write[best_tile];
		tile.state = TILE_STATE_REQUESTED;
		const uint64_t tile_limit = uint64_t(max_tile_size_mb) * 1024 * 1024;
		tile.data_request_id = data_queue->request(get_instance_id(), generation, tile.reference.uri, tile_limit, request_timeout_sec, retry_count, tile.request_priority);
		if (tile.data_request_id == 0) {
			tile.state = TILE_STATE_FAILED;
			emit_signal(SNAME("tile_failed"), tile.reference.uri, "Unable to queue the tile data request.");
			continue;
		}
	}
}

void TiledMeshInstance3D::_poll_data_results() {
	OsgDataRequestQueue *data_queue = OsgDataRequestQueue::get_singleton();
	OsgParseQueue *parse_queue = OsgParseQueue::get_singleton();
	if (!data_queue || !parse_queue) {
		return;
	}
	data_queue->poll();
	bool completed_streaming_request = false;

	if (root_data_request_id != 0) {
		OsgDataRequestQueue::Result result;
		if (data_queue->take_result(root_data_request_id, result)) {
			root_data_request_id = 0;
			if (result.generation == generation && result.owner == get_instance_id()) {
				if (result.error != OK) {
					root_tile_state = TILE_STATE_FAILED;
					open_state = false;
					emit_signal(SNAME("load_failed"), result.uri, result.error_message);
				} else {
					root_source_bytes = result.bytes.size();
					root_tile_state = TILE_STATE_DOWNLOADED;
					OsgSceneConvertOptions options;
					options.osg_z_up = axis_mode == AXIS_MODE_OSG_Z_UP;
					options.unit_scale = unit_scale;
					options.use_root_center = world_origin_mode == WORLD_ORIGIN_ROOT_CENTER;
					options.explicit_origin = { world_origin_x, world_origin_y, world_origin_z };
					root_parse_request_id = parse_queue->enqueue(get_instance_id(), generation, result.uri, result.bytes, options, 1.0e15);
					if (root_parse_request_id == 0) {
						root_source_bytes = 0;
						root_tile_state = TILE_STATE_FAILED;
						open_state = false;
						emit_signal(SNAME("load_failed"), result.uri, "Unable to queue the OSG parse request.");
					} else {
						root_tile_state = TILE_STATE_PARSING;
					}
				}
				emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
			}
		}
	}

	for (StreamingTile &tile : streaming_tiles) {
		if (tile.data_request_id == 0) {
			continue;
		}
		OsgDataRequestQueue::Result result;
		if (!data_queue->take_result(tile.data_request_id, result)) {
			continue;
		}
		tile.data_request_id = 0;
		completed_streaming_request = true;
		if (result.generation != generation || result.owner != get_instance_id()) {
			tile.state = TILE_STATE_CANCELLED;
			continue;
		}
		if (result.error != OK) {
			tile.state = TILE_STATE_FAILED;
			emit_signal(SNAME("tile_failed"), tile.reference.uri, result.error_message);
			continue;
		}
		tile.source_bytes = result.bytes.size();
		tile.state = TILE_STATE_DOWNLOADED;
		OsgSceneConvertOptions options;
		options.osg_z_up = axis_mode == AXIS_MODE_OSG_Z_UP;
		options.unit_scale = unit_scale;
		options.use_root_center = true;
		tile.parse_request_id = parse_queue->enqueue(get_instance_id(), generation, result.uri, result.bytes, options, tile.request_priority);
		if (tile.parse_request_id == 0) {
			tile.source_bytes = 0;
			tile.state = TILE_STATE_FAILED;
			emit_signal(SNAME("tile_failed"), tile.reference.uri, "Unable to queue the OSG parse request.");
		} else {
			tile.state = TILE_STATE_PARSING;
		}
	}
	if (completed_streaming_request) {
		_request_streaming_tiles();
	}
}

void TiledMeshInstance3D::_poll_texture_results() {
	OsgDataRequestQueue *data_queue = OsgDataRequestQueue::get_singleton();
	OsgImageDecodeQueue *decode_queue = OsgImageDecodeQueue::get_singleton();
	if (!data_queue || !decode_queue) {
		return;
	}
	int texture_uploads = 0;
	for (int i = texture_requests.size() - 1; i >= 0; i--) {
		TextureRequest &request = texture_requests.write[i];
		if (request.data_request_id != 0) {
			OsgDataRequestQueue::Result data_result;
			if (!data_queue->take_result(request.data_request_id, data_result)) {
				continue;
			}
			request.data_request_id = 0;
			const bool current = data_result.generation == generation && data_result.owner == get_instance_id();
			const bool valid_target = request.render_tile >= 0 && request.render_tile < render_tiles.size() && render_tiles[request.render_tile].instance.is_valid();
			if (current && data_result.error == OK && valid_target) {
				request.decode_request_id = decode_queue->enqueue(get_instance_id(), generation, request.uri, data_result.bytes);
				if (request.decode_request_id != 0) {
					continue;
				}
				WARN_PRINT(vformat("TiledMeshInstance3D: Unable to queue texture decode for '%s'.", request.uri));
			} else if (current && data_result.error != OK && !data_result.error_message.is_empty()) {
				WARN_PRINT(vformat("TiledMeshInstance3D: %s", data_result.error_message));
			}
			texture_requests.remove_at(i);
			continue;
		}
		if (request.decode_request_id == 0) {
			texture_requests.remove_at(i);
			continue;
		}
		if (texture_uploads >= 2) {
			continue;
		}
		OsgImageDecodeQueue::Result decode_result;
		if (!decode_queue->take_result(request.decode_request_id, decode_result)) {
			continue;
		}
		request.decode_request_id = 0;
		const bool current = decode_result.generation == generation && decode_result.owner == get_instance_id();
		if (current && decode_result.error == OK && decode_result.image.is_valid() && request.render_tile >= 0 && request.render_tile < render_tiles.size()) {
			TileRenderInstance &tile = render_tiles.write[request.render_tile];
			if (tile.instance.is_valid() && request.material_index >= 0 && request.material_index < tile.materials.size()) {
				StandardMaterial3D *material = Object::cast_to<StandardMaterial3D>(tile.materials[request.material_index].ptr());
				if (material) {
					material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(decode_result.image));
					tile.texture_bytes += uint64_t(decode_result.image->get_width()) * uint64_t(decode_result.image->get_height()) * 4;
					texture_uploads++;
				}
			}
		} else if (current && decode_result.error != OK && !decode_result.error_message.is_empty()) {
			WARN_PRINT(vformat("TiledMeshInstance3D: %s", decode_result.error_message));
		}
		texture_requests.remove_at(i);
	}
}

void TiledMeshInstance3D::_poll_streaming_results() {
	OsgParseQueue *queue = OsgParseQueue::get_singleton();
	if (!queue) {
		return;
	}
	bool completed_parse_request = false;
	for (StreamingTile &tile : streaming_tiles) {
		if (tile.parse_request_id == 0) {
			continue;
		}
		OsgParseQueue::Result result;
		if (!queue->take_result(tile.parse_request_id, result)) {
			continue;
		}
		tile.parse_request_id = 0;
		completed_parse_request = true;
		tile.source_bytes = 0;
		if (result.generation != generation || result.owner != get_instance_id()) {
			tile.state = TILE_STATE_CANCELLED;
			continue;
		}
		if (result.error != OK) {
			tile.state = TILE_STATE_FAILED;
			emit_signal(SNAME("tile_failed"), tile.reference.uri, result.error_message);
			continue;
		}
		tile.cpu_document = result.document;
		tile.has_cpu_document = true;
		tile.state = TILE_STATE_CPU_READY;
	}
	if (completed_parse_request) {
		_request_streaming_tiles();
	}
}

void TiledMeshInstance3D::_upload_streaming_tiles() {
	if (paused) {
		return;
	}
	const uint64_t started = Time::get_singleton()->get_ticks_usec();
	const uint64_t time_budget_usec = uint64_t(upload_budget_ms * 1000.0);
	const uint64_t byte_budget = 16 * 1024 * 1024;
	uint64_t uploaded_bytes = 0;
	bool render_state_changed = false;
	Vector<int> ready_tiles;
	for (int i = 0; i < streaming_tiles.size(); i++) {
		const StreamingTile &tile = streaming_tiles[i];
		if (tile.state == TILE_STATE_CPU_READY && tile.has_cpu_document && tile.desired) {
			ready_tiles.push_back(i);
		}
	}
	while (!ready_tiles.is_empty()) {
		int best_position = 0;
		for (int i = 1; i < ready_tiles.size(); i++) {
			if (streaming_tiles[ready_tiles[i]].request_priority > streaming_tiles[ready_tiles[best_position]].request_priority) {
				best_position = i;
			}
		}
		const int tile_index = ready_tiles[best_position];
		ready_tiles.remove_at(best_position);
		StreamingTile &tile = streaming_tiles.write[tile_index];
		if (uploaded_bytes > 0 && (uploaded_bytes + tile.cpu_document.estimated_cpu_bytes > byte_budget || (time_budget_usec > 0 && Time::get_singleton()->get_ticks_usec() - started >= time_budget_usec))) {
			break;
		}
		// Registering the next level appends to streaming_tiles and can detach or
		// reallocate its CowData. Never retain a StreamingTile reference (or a
		// reference to its cpu_document) across _register_external_tiles(). The
		// stale reference previously left the real tile in CPU_READY/UPLOADING,
		// so its parent's coarse fallback stayed visible over the uploaded detail;
		// it also corrupted document storage during scene shutdown.
		const OsgTileDocument document = tile.cpu_document;
		const String tile_uri = tile.reference.uri;
		tile.state = TILE_STATE_UPLOADING;
		String error_message;
		const uint64_t document_bytes = document.estimated_cpu_bytes;
		const Error upload_error = _upload_document(document, error_message, tile_index);
		if (upload_error != OK) {
			tile.has_cpu_document = false;
			tile.cpu_document = OsgTileDocument();
			tile.state = TILE_STATE_FAILED;
			emit_signal(SNAME("tile_failed"), tile_uri, error_message);
			continue;
		}
		for (const String &warning : document.warnings) {
			WARN_PRINT(vformat("TiledMeshInstance3D: %s", warning));
		}
		tile.has_cpu_document = false;
		tile.cpu_document = OsgTileDocument();
		tile.state = TILE_STATE_RESIDENT;
		tile.resident_since_usec = Time::get_singleton()->get_ticks_usec();
		tile.last_used_usec = tile.resident_since_usec;
		render_state_changed = true;
		// No references into streaming_tiles may be used after this call.
		_register_external_tiles(document, tile_index);
		uploaded_bytes += document_bytes;
		emit_signal(SNAME("tile_loaded"), tile_uri);
	}
	// With a stationary camera and no newly resident geometry, visibility is
	// unchanged. Avoid walking the complete render hierarchy every frame.
	if (render_state_changed) {
		_update_streaming_visibility();
	}
}

void TiledMeshInstance3D::_update_streaming_visibility() {
	tile_displayable_cache.resize(streaming_tiles.size());
	tile_displayable_cache.fill(0);
	parent_refined_cache.clear();
	group_refined_cache.clear();
	group_pending_cache.clear();
	Camera3D *display_camera = display_camera_id.is_valid() ? ObjectDB::get_instance<Camera3D>(display_camera_id) : nullptr;
	RenderingServer *rs = RenderingServer::get_singleton();
	const bool node_visible = is_visible_in_tree();
	for (TileRenderInstance &render_tile : render_tiles) {
		if (!render_tile.instance.is_valid()) {
			continue;
		}
		const int owner = render_tile.owner_tile;
		const bool owner_desired = owner < 0 || (owner < streaming_tiles.size() && streaming_tiles[owner].display_desired);
		// Geometry outside a PagedLOD/ProxyNode has no fallback group and is an
		// unconditional sibling, not a coarse representation of every paging
		// child in the document. Refining one child must never hide this geometry.
		const bool refined = render_tile.fallback_group >= 0 && _parent_group_is_fully_refined(owner, render_tile.fallback_group);
		const bool refinement_pending = render_tile.fallback_group >= 0 && _parent_group_has_display_refinement(owner, render_tile.fallback_group) && !refined;
		const bool lod_visible = _render_lod_matches(render_tile, display_camera);
		const bool visible = owner_desired && !refined && (lod_visible || refinement_pending);
		if (render_tile.visible != visible) {
			render_tile.visible = visible;
			if (rs) {
				rs->instance_set_visible(render_tile.instance, node_visible && visible);
			}
		}
	}
}

void TiledMeshInstance3D::_enforce_memory_budget() {
	const int configured_budget = Engine::get_singleton()->is_editor_hint() ? editor_preview_memory_mb : memory_budget_mb;
	const uint64_t budget = uint64_t(configured_budget > 0 ? configured_budget : (OS::get_singleton()->has_feature("web") ? 256 : 512)) * 1024 * 1024;
	uint64_t resident_bytes = 0;
	for (const TileRenderInstance &tile : render_tiles) {
		resident_bytes += tile.estimated_bytes + tile.texture_bytes;
	}
	if (resident_bytes <= budget) {
		return;
	}
	const uint64_t now = Time::get_singleton()->get_ticks_usec();
	bool evicted_any = false;
	while (resident_bytes > budget) {
		int eviction = -1;
		uint64_t oldest_use = UINT64_MAX;
		for (int i = 0; i < streaming_tiles.size(); i++) {
			const StreamingTile &tile = streaming_tiles[i];
			if (tile.state != TILE_STATE_RESIDENT || tile.desired || !_owner_has_render_geometry(i) || now - tile.resident_since_usec < 2000000) {
				continue;
			}
			if (tile.last_used_usec < oldest_use) {
				oldest_use = tile.last_used_usec;
				eviction = i;
			}
		}
		if (eviction < 0) {
			break;
		}
		StreamingTile &tile = streaming_tiles.write[eviction];
		uint64_t tile_bytes = 0;
		while (!tile.render_tiles.is_empty()) {
			const int render_index = tile.render_tiles[tile.render_tiles.size() - 1];
			tile_bytes += render_tiles[render_index].estimated_bytes + render_tiles[render_index].texture_bytes;
			_free_render_tile(render_index, false);
		}
		evicted_any = true;
		tile.state = TILE_STATE_UNLOADED;
		resident_bytes = resident_bytes > tile_bytes ? resident_bytes - tile_bytes : 0;
	}
	if (evicted_any) {
		_recompute_resident_aabb();
	}
}

String TiledMeshInstance3D::_tile_state_name(TileState p_state) {
	switch (p_state) {
		case TILE_STATE_UNLOADED: return "unloaded";
		case TILE_STATE_REQUESTED: return "requested";
		case TILE_STATE_DOWNLOADED: return "downloaded";
		case TILE_STATE_PARSING: return "parsing";
		case TILE_STATE_CPU_READY: return "cpu_ready";
		case TILE_STATE_UPLOADING: return "uploading";
		case TILE_STATE_RESIDENT: return "resident";
		case TILE_STATE_FAILED: return "failed";
		case TILE_STATE_CANCELLED: return "cancelled";
	}
	return "unknown";
}

void TiledMeshInstance3D::_poll_parse_result() {
	if (root_parse_request_id == 0) {
		return;
	}
	OsgParseQueue *queue = OsgParseQueue::get_singleton();
	if (!queue) {
		return;
	}
	OsgParseQueue::Result result;
	if (!queue->take_result(root_parse_request_id, result)) {
		return;
	}
	root_parse_request_id = 0;
	root_source_bytes = 0;
	if (result.generation != generation || result.owner != get_instance_id()) {
		return;
	}
	if (result.error != OK) {
		root_tile_state = TILE_STATE_FAILED;
		open_state = false;
		emit_signal(SNAME("load_failed"), result.source_uri, result.error_message);
		emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
		return;
	}
	cpu_ready_document = result.document;
	has_cpu_ready_document = true;
	root_tile_state = TILE_STATE_CPU_READY;
	emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
}

void TiledMeshInstance3D::_upload_cpu_ready_document() {
	if (!has_cpu_ready_document || root_tile_state != TILE_STATE_CPU_READY || paused) {
		return;
	}
	root_tile_state = TILE_STATE_UPLOADING;
	String error_message;
	if (world_origin_mode == WORLD_ORIGIN_ROOT_CENTER) {
		set_world_origin(cpu_ready_document.source_origin.x, cpu_ready_document.source_origin.y, cpu_ready_document.source_origin.z);
	}
	dataset_aabb = cpu_ready_document.local_aabb;
	const Error err = _upload_document(cpu_ready_document, error_message, -1);
	if (err != OK) {
		_clear_render_tiles();
		dataset_aabb = AABB();
		has_cpu_ready_document = false;
		cpu_ready_document = OsgTileDocument();
		root_tile_state = TILE_STATE_FAILED;
		open_state = false;
		emit_signal(SNAME("load_failed"), source_uri, error_message);
		emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
		return;
	}
	for (const String &warning : cpu_ready_document.warnings) {
		WARN_PRINT(vformat("TiledMeshInstance3D: %s", warning));
	}
	_register_external_tiles(cpu_ready_document, -1);
	has_cpu_ready_document = false;
	cpu_ready_document = OsgTileDocument();
	root_tile_state = TILE_STATE_RESIDENT;
	update_gizmos();
	emit_signal(SNAME("root_loaded"));
	emit_signal(SNAME("tile_loaded"), OsgDataSource::normalize_uri(source_uri));
	emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
}

void TiledMeshInstance3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			add_to_group(StringName("_tiled_mesh_instances_3d"));
			if (auto_load && (!Engine::get_singleton()->is_editor_hint() || editor_preview_enabled)) {
				call_deferred(SNAME("open"));
			}
		} break;
		case NOTIFICATION_ENTER_WORLD:
		case NOTIFICATION_TRANSFORM_CHANGED: {
			streaming_topology_dirty = true;
			_sync_render_instances();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			_sync_render_instances();
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			RenderingServer *rs = RenderingServer::get_singleton();
			if (rs) {
				for (const TileRenderInstance &tile : render_tiles) {
					if (tile.instance.is_valid()) {
						rs->instance_set_scenario(tile.instance, RID());
					}
				}
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			close();
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			_poll_data_results();
			_poll_texture_results();
			_poll_parse_result();
			_poll_streaming_results();
			_upload_cpu_ready_document();
			_upload_streaming_tiles();
			if (last_layer_mask != get_layer_mask() || last_visible != is_visible_in_tree()) {
				_sync_render_instances();
			}
			if (open_state && !paused && !Engine::get_singleton()->is_editor_hint()) {
				Camera3D *camera = _resolve_runtime_camera();
				_update_streaming(camera);
			}
		} break;
	}
}

void TiledMeshInstance3D::set_source_uri(const String &p_uri) {
	const String uri = p_uri.strip_edges();
	if (source_uri == uri) {
		return;
	}
	source_uri = uri;
	if (is_inside_tree() && auto_load && (!Engine::get_singleton()->is_editor_hint() || editor_preview_enabled)) {
		reload();
	}
}

String TiledMeshInstance3D::get_source_uri() const { return source_uri; }
void TiledMeshInstance3D::set_auto_load(bool p_enabled) { auto_load = p_enabled; }
bool TiledMeshInstance3D::is_auto_load_enabled() const { return auto_load; }
void TiledMeshInstance3D::set_camera_path(const NodePath &p_path) {
	camera_path = p_path;
	streaming_topology_dirty = true;
}
NodePath TiledMeshInstance3D::get_camera_path() const { return camera_path; }

void TiledMeshInstance3D::set_axis_mode(AxisMode p_mode) {
	ERR_FAIL_INDEX(p_mode, 2);
	if (axis_mode == p_mode) {
		return;
	}
	axis_mode = p_mode;
	if (open_state) {
		reload();
	}
}
TiledMeshInstance3D::AxisMode TiledMeshInstance3D::get_axis_mode() const { return axis_mode; }
void TiledMeshInstance3D::set_unit_scale(double p_scale) {
	const double scale = MAX(p_scale, 0.000001);
	if (unit_scale == scale) {
		return;
	}
	unit_scale = scale;
	if (open_state) {
		reload();
	}
}
double TiledMeshInstance3D::get_unit_scale() const { return unit_scale; }
void TiledMeshInstance3D::set_world_origin_mode(WorldOriginMode p_mode) {
	ERR_FAIL_INDEX(p_mode, 2);
	if (world_origin_mode == p_mode) {
		return;
	}
	world_origin_mode = p_mode;
	if (open_state) {
		reload();
	}
}
TiledMeshInstance3D::WorldOriginMode TiledMeshInstance3D::get_world_origin_mode() const { return world_origin_mode; }

void TiledMeshInstance3D::set_world_origin(double p_x, double p_y, double p_z) {
	world_origin_x = p_x;
	world_origin_y = p_y;
	world_origin_z = p_z;
	streaming_topology_dirty = true;
	_recompute_resident_aabb();
	_sync_render_instances();
	PackedFloat64Array origin;
	origin.resize(3);
	origin.set(0, world_origin_x);
	origin.set(1, world_origin_y);
	origin.set(2, world_origin_z);
	emit_signal(SNAME("world_origin_changed"), origin);
}

double TiledMeshInstance3D::get_world_origin_x() const { return world_origin_x; }
double TiledMeshInstance3D::get_world_origin_y() const { return world_origin_y; }
double TiledMeshInstance3D::get_world_origin_z() const { return world_origin_z; }
void TiledMeshInstance3D::set_world_origin_x(double p_value) { set_world_origin(p_value, world_origin_y, world_origin_z); }
void TiledMeshInstance3D::set_world_origin_y(double p_value) { set_world_origin(world_origin_x, p_value, world_origin_z); }
void TiledMeshInstance3D::set_world_origin_z(double p_value) { set_world_origin(world_origin_x, world_origin_y, p_value); }

void TiledMeshInstance3D::set_lod_bias(double p_value) {
	lod_bias = MAX(p_value, 0.01);
	streaming_topology_dirty = true;
}
double TiledMeshInstance3D::get_lod_bias() const { return lod_bias; }
void TiledMeshInstance3D::set_lod_hysteresis(double p_value) {
	lod_hysteresis = CLAMP(p_value, 0.0, 1.0);
	streaming_topology_dirty = true;
}
double TiledMeshInstance3D::get_lod_hysteresis() const { return lod_hysteresis; }
void TiledMeshInstance3D::set_memory_budget_mb(int p_value) {
	memory_budget_mb = MAX(p_value, 0);
	streaming_topology_dirty = true;
}
int TiledMeshInstance3D::get_memory_budget_mb() const { return memory_budget_mb; }
void TiledMeshInstance3D::set_max_concurrent_requests(int p_value) {
	max_concurrent_requests = MAX(p_value, 0);
	streaming_topology_dirty = true;
}
int TiledMeshInstance3D::get_max_concurrent_requests() const { return max_concurrent_requests; }
void TiledMeshInstance3D::set_max_tile_size_mb(int p_value) { max_tile_size_mb = MAX(p_value, 1); }
int TiledMeshInstance3D::get_max_tile_size_mb() const { return max_tile_size_mb; }
void TiledMeshInstance3D::set_upload_budget_ms(double p_value) { upload_budget_ms = MAX(p_value, 0.0); }
double TiledMeshInstance3D::get_upload_budget_ms() const { return upload_budget_ms; }
void TiledMeshInstance3D::set_request_timeout_sec(double p_value) { request_timeout_sec = MAX(p_value, 0.1); }
double TiledMeshInstance3D::get_request_timeout_sec() const { return request_timeout_sec; }
void TiledMeshInstance3D::set_retry_count(int p_value) { retry_count = MAX(p_value, 0); }
int TiledMeshInstance3D::get_retry_count() const { return retry_count; }
void TiledMeshInstance3D::set_cast_shadow(GeometryInstance3D::ShadowCastingSetting p_setting) {
	ERR_FAIL_COND(p_setting < GeometryInstance3D::SHADOW_CASTING_SETTING_OFF || p_setting > GeometryInstance3D::SHADOW_CASTING_SETTING_SHADOWS_ONLY);
	cast_shadow = p_setting;
	_sync_render_instances();
}
GeometryInstance3D::ShadowCastingSetting TiledMeshInstance3D::get_cast_shadow() const { return cast_shadow; }

void TiledMeshInstance3D::set_editor_preview_enabled(bool p_enabled) {
	editor_preview_enabled = p_enabled;
	if (Engine::get_singleton()->is_editor_hint()) {
		if (p_enabled && auto_load) {
			open();
		} else if (!p_enabled) {
			close();
		}
	}
}
bool TiledMeshInstance3D::is_editor_preview_enabled() const { return editor_preview_enabled; }
void TiledMeshInstance3D::set_editor_preview_lod_bias(double p_value) {
	editor_preview_lod_bias = MAX(p_value, 0.01);
	streaming_topology_dirty = true;
}
double TiledMeshInstance3D::get_editor_preview_lod_bias() const { return editor_preview_lod_bias; }
void TiledMeshInstance3D::set_editor_preview_memory_mb(int p_value) {
	editor_preview_memory_mb = MAX(p_value, 1);
	streaming_topology_dirty = true;
}
int TiledMeshInstance3D::get_editor_preview_memory_mb() const { return editor_preview_memory_mb; }
void TiledMeshInstance3D::set_editor_preview_max_requests(int p_value) {
	editor_preview_max_requests = MAX(p_value, 1);
	streaming_topology_dirty = true;
}
int TiledMeshInstance3D::get_editor_preview_max_requests() const { return editor_preview_max_requests; }
void TiledMeshInstance3D::set_editor_preview_show_bounds(bool p_enabled) { editor_preview_show_bounds = p_enabled; update_gizmos(); }
bool TiledMeshInstance3D::is_editor_preview_showing_bounds() const { return editor_preview_show_bounds; }

Error TiledMeshInstance3D::open() {
	if (source_uri.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (open_state) {
		return OK;
	}
	generation++;
	const uint64_t requested_generation = generation;
	_clear_streaming_tiles();
	_clear_render_tiles();
	dataset_aabb = AABB();
	paused = false;
	root_tile_state = TILE_STATE_REQUESTED;
	root_data_request_id = 0;
	root_source_bytes = 0;
	has_cpu_ready_document = false;
	cpu_ready_document = OsgTileDocument();

	const uint64_t tile_limit = uint64_t(max_tile_size_mb) * 1024 * 1024;
	OsgDataRequestQueue *data_queue = OsgDataRequestQueue::get_singleton();
	if (!data_queue || !OsgParseQueue::get_singleton()) {
		open_state = false;
		root_tile_state = TILE_STATE_FAILED;
		emit_signal(SNAME("load_failed"), source_uri, "The OSG data or parse queue is not available.");
		return ERR_UNAVAILABLE;
	}
	root_data_request_id = data_queue->request(get_instance_id(), requested_generation, source_uri, tile_limit, request_timeout_sec, retry_count, 1.0e15);
	if (root_data_request_id == 0) {
		open_state = false;
		root_tile_state = TILE_STATE_FAILED;
		emit_signal(SNAME("load_failed"), source_uri, "Unable to queue the root data request.");
		return ERR_CANT_CREATE;
	}
	open_state = true;
	emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
	return OK;
}

void TiledMeshInstance3D::close() {
	generation++;
	if (root_data_request_id != 0) {
		if (OsgDataRequestQueue *data_queue = OsgDataRequestQueue::get_singleton()) {
			data_queue->cancel(root_data_request_id);
		}
		root_data_request_id = 0;
	}
	if (root_parse_request_id != 0) {
		if (OsgParseQueue *queue = OsgParseQueue::get_singleton()) {
			queue->cancel(root_parse_request_id);
		}
		root_parse_request_id = 0;
	}
	open_state = false;
	paused = false;
	root_source_bytes = 0;
	has_cpu_ready_document = false;
	cpu_ready_document = OsgTileDocument();
	if (root_tile_state != TILE_STATE_UNLOADED) {
		root_tile_state = TILE_STATE_CANCELLED;
	}
	_clear_streaming_tiles();
	_clear_render_tiles();
	dataset_aabb = AABB();
	update_gizmos();
}

Error TiledMeshInstance3D::reload() {
	close();
	return open();
}

bool TiledMeshInstance3D::is_open() const { return open_state; }
void TiledMeshInstance3D::set_streaming_paused(bool p_paused) {
	paused = p_paused;
	if (!paused) {
		streaming_topology_dirty = true;
	}
}
bool TiledMeshInstance3D::is_streaming_paused() const { return paused; }

Vector3 TiledMeshInstance3D::world_to_local(double p_x, double p_y, double p_z) const {
	const double x = (p_x - world_origin_x) * unit_scale;
	const double y = (p_y - world_origin_y) * unit_scale;
	const double z = (p_z - world_origin_z) * unit_scale;
	if (axis_mode == AXIS_MODE_OSG_Z_UP) {
		return Vector3(x, z, -y);
	}
	return Vector3(x, y, z);
}

PackedFloat64Array TiledMeshInstance3D::local_to_world(const Vector3 &p_local_position) const {
	double x = p_local_position.x;
	double y = p_local_position.y;
	double z = p_local_position.z;
	if (axis_mode == AXIS_MODE_OSG_Z_UP) {
		y = -p_local_position.z;
		z = p_local_position.y;
	}
	PackedFloat64Array result;
	result.resize(3);
	result.set(0, x / unit_scale + world_origin_x);
	result.set(1, y / unit_scale + world_origin_y);
	result.set(2, z / unit_scale + world_origin_z);
	return result;
}

Dictionary TiledMeshInstance3D::get_streaming_stats() const {
	Dictionary stats;
	stats["open"] = open_state;
	stats["paused"] = paused;
	stats["generation"] = generation;
	stats["root_state"] = _tile_state_name(root_tile_state);
	int requested_tiles = root_tile_state == TILE_STATE_REQUESTED || root_tile_state == TILE_STATE_DOWNLOADED || root_tile_state == TILE_STATE_PARSING ? 1 : 0;
	int cpu_ready_tiles = root_tile_state == TILE_STATE_CPU_READY ? 1 : 0;
	int resident_tiles = root_tile_state == TILE_STATE_RESIDENT ? 1 : 0;
	int failed_tiles = root_tile_state == TILE_STATE_FAILED ? 1 : 0;
	uint64_t source_bytes = root_source_bytes;
	uint64_t cpu_ready_bytes = has_cpu_ready_document ? cpu_ready_document.estimated_cpu_bytes : 0;
	for (const StreamingTile &tile : streaming_tiles) {
		requested_tiles += tile.state == TILE_STATE_REQUESTED || tile.state == TILE_STATE_DOWNLOADED || tile.state == TILE_STATE_PARSING ? 1 : 0;
		cpu_ready_tiles += tile.state == TILE_STATE_CPU_READY ? 1 : 0;
		resident_tiles += tile.state == TILE_STATE_RESIDENT ? 1 : 0;
		failed_tiles += tile.state == TILE_STATE_FAILED ? 1 : 0;
		source_bytes += tile.source_bytes;
		cpu_ready_bytes += tile.has_cpu_document ? tile.cpu_document.estimated_cpu_bytes : 0;
	}
	stats["known_tiles"] = 1 + streaming_tiles.size();
	stats["requested_tiles"] = requested_tiles;
	stats["cpu_ready_tiles"] = cpu_ready_tiles;
	stats["resident_tiles"] = resident_tiles;
	stats["failed_tiles"] = failed_tiles;
	stats["source_bytes"] = source_bytes;
	stats["cpu_ready_bytes"] = cpu_ready_bytes;
	uint64_t estimated_bytes = 0;
	HashSet<int> visible_owners;
	int resident_render_instances = 0;
	int visible_render_instances = 0;
	int inline_lod_render_instances = 0;
	int visible_inline_lod_render_instances = 0;
	for (const TileRenderInstance &tile : render_tiles) {
		if (tile.instance.is_valid()) {
			resident_render_instances++;
			estimated_bytes += tile.estimated_bytes + tile.texture_bytes;
			if (!tile.lod_ranges.is_empty()) {
				inline_lod_render_instances++;
				visible_inline_lod_render_instances += tile.visible ? 1 : 0;
			}
			if (tile.visible) {
				visible_render_instances++;
				visible_owners.insert(tile.owner_tile);
			}
		}
	}
	stats["estimated_resident_bytes"] = estimated_bytes;
	stats["visible_tiles"] = visible_owners.size();
	stats["resident_render_instances"] = resident_render_instances;
	stats["visible_render_instances"] = visible_render_instances;
	stats["inline_lod_render_instances"] = inline_lod_render_instances;
	stats["visible_inline_lod_render_instances"] = visible_inline_lod_render_instances;
	stats["texture_requests"] = texture_requests.size();
	int fallback_surfaces = 0;
	int hidden_fallback_surfaces = 0;
	for (const TileRenderInstance &render_tile : render_tiles) {
		if (!render_tile.instance.is_valid() || render_tile.fallback_group < 0) {
			continue;
		}
		fallback_surfaces++;
		hidden_fallback_surfaces += _parent_group_is_fully_refined(render_tile.owner_tile, render_tile.fallback_group) ? 1 : 0;
	}
	stats["fallback_surfaces"] = fallback_surfaces;
	stats["hidden_fallback_surfaces"] = hidden_fallback_surfaces;
	return stats;
}

AABB TiledMeshInstance3D::get_dataset_aabb() const { return dataset_aabb; }
AABB TiledMeshInstance3D::get_resident_aabb() const { return resident_aabb; }
AABB TiledMeshInstance3D::get_aabb() const { return dataset_aabb; }

#ifdef TOOLS_ENABLED
void TiledMeshInstance3D::editor_update_cameras(const Vector<Camera3D *> &p_cameras, Camera3D *p_active_camera) {
	if (!editor_preview_enabled || paused || !open_state || p_cameras.is_empty()) {
		return;
	}
	_update_streaming_from_cameras(p_cameras, p_active_camera);
}
#endif

void TiledMeshInstance3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source_uri", "uri"), &TiledMeshInstance3D::set_source_uri);
	ClassDB::bind_method(D_METHOD("get_source_uri"), &TiledMeshInstance3D::get_source_uri);
	ClassDB::bind_method(D_METHOD("set_auto_load", "enabled"), &TiledMeshInstance3D::set_auto_load);
	ClassDB::bind_method(D_METHOD("is_auto_load_enabled"), &TiledMeshInstance3D::is_auto_load_enabled);
	ClassDB::bind_method(D_METHOD("set_camera_path", "path"), &TiledMeshInstance3D::set_camera_path);
	ClassDB::bind_method(D_METHOD("get_camera_path"), &TiledMeshInstance3D::get_camera_path);
	ClassDB::bind_method(D_METHOD("set_axis_mode", "mode"), &TiledMeshInstance3D::set_axis_mode);
	ClassDB::bind_method(D_METHOD("get_axis_mode"), &TiledMeshInstance3D::get_axis_mode);
	ClassDB::bind_method(D_METHOD("set_unit_scale", "scale"), &TiledMeshInstance3D::set_unit_scale);
	ClassDB::bind_method(D_METHOD("get_unit_scale"), &TiledMeshInstance3D::get_unit_scale);
	ClassDB::bind_method(D_METHOD("set_world_origin_mode", "mode"), &TiledMeshInstance3D::set_world_origin_mode);
	ClassDB::bind_method(D_METHOD("get_world_origin_mode"), &TiledMeshInstance3D::get_world_origin_mode);
	ClassDB::bind_method(D_METHOD("set_world_origin", "x", "y", "z"), &TiledMeshInstance3D::set_world_origin);
	ClassDB::bind_method(D_METHOD("set_world_origin_x", "value"), &TiledMeshInstance3D::set_world_origin_x);
	ClassDB::bind_method(D_METHOD("get_world_origin_x"), &TiledMeshInstance3D::get_world_origin_x);
	ClassDB::bind_method(D_METHOD("set_world_origin_y", "value"), &TiledMeshInstance3D::set_world_origin_y);
	ClassDB::bind_method(D_METHOD("get_world_origin_y"), &TiledMeshInstance3D::get_world_origin_y);
	ClassDB::bind_method(D_METHOD("set_world_origin_z", "value"), &TiledMeshInstance3D::set_world_origin_z);
	ClassDB::bind_method(D_METHOD("get_world_origin_z"), &TiledMeshInstance3D::get_world_origin_z);
	ClassDB::bind_method(D_METHOD("set_lod_bias", "value"), &TiledMeshInstance3D::set_lod_bias);
	ClassDB::bind_method(D_METHOD("get_lod_bias"), &TiledMeshInstance3D::get_lod_bias);
	ClassDB::bind_method(D_METHOD("set_lod_hysteresis", "value"), &TiledMeshInstance3D::set_lod_hysteresis);
	ClassDB::bind_method(D_METHOD("get_lod_hysteresis"), &TiledMeshInstance3D::get_lod_hysteresis);
	ClassDB::bind_method(D_METHOD("set_memory_budget_mb", "value"), &TiledMeshInstance3D::set_memory_budget_mb);
	ClassDB::bind_method(D_METHOD("get_memory_budget_mb"), &TiledMeshInstance3D::get_memory_budget_mb);
	ClassDB::bind_method(D_METHOD("set_max_concurrent_requests", "value"), &TiledMeshInstance3D::set_max_concurrent_requests);
	ClassDB::bind_method(D_METHOD("get_max_concurrent_requests"), &TiledMeshInstance3D::get_max_concurrent_requests);
	ClassDB::bind_method(D_METHOD("set_max_tile_size_mb", "value"), &TiledMeshInstance3D::set_max_tile_size_mb);
	ClassDB::bind_method(D_METHOD("get_max_tile_size_mb"), &TiledMeshInstance3D::get_max_tile_size_mb);
	ClassDB::bind_method(D_METHOD("set_upload_budget_ms", "value"), &TiledMeshInstance3D::set_upload_budget_ms);
	ClassDB::bind_method(D_METHOD("get_upload_budget_ms"), &TiledMeshInstance3D::get_upload_budget_ms);
	ClassDB::bind_method(D_METHOD("set_request_timeout_sec", "value"), &TiledMeshInstance3D::set_request_timeout_sec);
	ClassDB::bind_method(D_METHOD("get_request_timeout_sec"), &TiledMeshInstance3D::get_request_timeout_sec);
	ClassDB::bind_method(D_METHOD("set_retry_count", "value"), &TiledMeshInstance3D::set_retry_count);
	ClassDB::bind_method(D_METHOD("get_retry_count"), &TiledMeshInstance3D::get_retry_count);
	ClassDB::bind_method(D_METHOD("set_cast_shadow", "setting"), &TiledMeshInstance3D::set_cast_shadow);
	ClassDB::bind_method(D_METHOD("get_cast_shadow"), &TiledMeshInstance3D::get_cast_shadow);
	ClassDB::bind_method(D_METHOD("set_editor_preview_enabled", "enabled"), &TiledMeshInstance3D::set_editor_preview_enabled);
	ClassDB::bind_method(D_METHOD("is_editor_preview_enabled"), &TiledMeshInstance3D::is_editor_preview_enabled);
	ClassDB::bind_method(D_METHOD("set_editor_preview_lod_bias", "value"), &TiledMeshInstance3D::set_editor_preview_lod_bias);
	ClassDB::bind_method(D_METHOD("get_editor_preview_lod_bias"), &TiledMeshInstance3D::get_editor_preview_lod_bias);
	ClassDB::bind_method(D_METHOD("set_editor_preview_memory_mb", "value"), &TiledMeshInstance3D::set_editor_preview_memory_mb);
	ClassDB::bind_method(D_METHOD("get_editor_preview_memory_mb"), &TiledMeshInstance3D::get_editor_preview_memory_mb);
	ClassDB::bind_method(D_METHOD("set_editor_preview_max_requests", "value"), &TiledMeshInstance3D::set_editor_preview_max_requests);
	ClassDB::bind_method(D_METHOD("get_editor_preview_max_requests"), &TiledMeshInstance3D::get_editor_preview_max_requests);
	ClassDB::bind_method(D_METHOD("set_editor_preview_show_bounds", "enabled"), &TiledMeshInstance3D::set_editor_preview_show_bounds);
	ClassDB::bind_method(D_METHOD("is_editor_preview_showing_bounds"), &TiledMeshInstance3D::is_editor_preview_showing_bounds);
	ClassDB::bind_method(D_METHOD("open"), &TiledMeshInstance3D::open);
	ClassDB::bind_method(D_METHOD("close"), &TiledMeshInstance3D::close);
	ClassDB::bind_method(D_METHOD("reload"), &TiledMeshInstance3D::reload);
	ClassDB::bind_method(D_METHOD("is_open"), &TiledMeshInstance3D::is_open);
	ClassDB::bind_method(D_METHOD("set_streaming_paused", "paused"), &TiledMeshInstance3D::set_streaming_paused);
	ClassDB::bind_method(D_METHOD("is_streaming_paused"), &TiledMeshInstance3D::is_streaming_paused);
	ClassDB::bind_method(D_METHOD("world_to_local", "x", "y", "z"), &TiledMeshInstance3D::world_to_local);
	ClassDB::bind_method(D_METHOD("local_to_world", "local_position"), &TiledMeshInstance3D::local_to_world);
	ClassDB::bind_method(D_METHOD("get_streaming_stats"), &TiledMeshInstance3D::get_streaming_stats);
	ClassDB::bind_method(D_METHOD("get_dataset_aabb"), &TiledMeshInstance3D::get_dataset_aabb);
	ClassDB::bind_method(D_METHOD("get_resident_aabb"), &TiledMeshInstance3D::get_resident_aabb);

	ADD_GROUP("Data", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_uri", PROPERTY_HINT_GLOBAL_FILE, "*.osg,*.osgb"), "set_source_uri", "get_source_uri");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_load"), "set_auto_load", "is_auto_load_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "camera_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Camera3D"), "set_camera_path", "get_camera_path");
	ADD_GROUP("Coordinates", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis_mode", PROPERTY_HINT_ENUM, "OSG Z-Up,Godot Y-Up"), "set_axis_mode", "get_axis_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_scale", PROPERTY_HINT_RANGE, "0.000001,1000000,0.001,or_greater"), "set_unit_scale", "get_unit_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "world_origin_mode", PROPERTY_HINT_ENUM, "Root Center,Explicit"), "set_world_origin_mode", "get_world_origin_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_origin_x"), "set_world_origin_x", "get_world_origin_x");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_origin_y"), "set_world_origin_y", "get_world_origin_y");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_origin_z"), "set_world_origin_z", "get_world_origin_z");
	ADD_GROUP("Streaming", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_bias", PROPERTY_HINT_RANGE, "0.01,16,0.01,or_greater"), "set_lod_bias", "get_lod_bias");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_hysteresis", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_lod_hysteresis", "get_lod_hysteresis");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "memory_budget_mb", PROPERTY_HINT_RANGE, "0,65536,1,or_greater"), "set_memory_budget_mb", "get_memory_budget_mb");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_concurrent_requests", PROPERTY_HINT_RANGE, "0,64,1"), "set_max_concurrent_requests", "get_max_concurrent_requests");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_tile_size_mb", PROPERTY_HINT_RANGE, "1,2048,1,or_greater"), "set_max_tile_size_mb", "get_max_tile_size_mb");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "upload_budget_ms", PROPERTY_HINT_RANGE, "0,50,0.1"), "set_upload_budget_ms", "get_upload_budget_ms");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "request_timeout_sec", PROPERTY_HINT_RANGE, "0.1,600,0.1"), "set_request_timeout_sec", "get_request_timeout_sec");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "retry_count", PROPERTY_HINT_RANGE, "0,10,1"), "set_retry_count", "get_retry_count");
	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadow", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"), "set_cast_shadow", "get_cast_shadow");
	ADD_GROUP("Editor Preview", "editor_preview_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_preview_enabled"), "set_editor_preview_enabled", "is_editor_preview_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "editor_preview_lod_bias", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_editor_preview_lod_bias", "get_editor_preview_lod_bias");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "editor_preview_memory_mb", PROPERTY_HINT_RANGE, "1,4096,1"), "set_editor_preview_memory_mb", "get_editor_preview_memory_mb");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "editor_preview_max_requests", PROPERTY_HINT_RANGE, "1,32,1"), "set_editor_preview_max_requests", "get_editor_preview_max_requests");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_preview_show_bounds"), "set_editor_preview_show_bounds", "is_editor_preview_showing_bounds");

	ADD_SIGNAL(MethodInfo("root_loaded"));
	ADD_SIGNAL(MethodInfo("load_failed", PropertyInfo(Variant::STRING, "uri"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("tile_loaded", PropertyInfo(Variant::STRING, "uri")));
	ADD_SIGNAL(MethodInfo("tile_failed", PropertyInfo(Variant::STRING, "uri"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("streaming_stats_changed", PropertyInfo(Variant::DICTIONARY, "stats")));
	ADD_SIGNAL(MethodInfo("world_origin_changed", PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "origin")));

	BIND_ENUM_CONSTANT(AXIS_MODE_OSG_Z_UP);
	BIND_ENUM_CONSTANT(AXIS_MODE_GODOT_Y_UP);
	BIND_ENUM_CONSTANT(WORLD_ORIGIN_ROOT_CENTER);
	BIND_ENUM_CONSTANT(WORLD_ORIGIN_EXPLICIT);
	BIND_ENUM_CONSTANT(TILE_STATE_UNLOADED);
	BIND_ENUM_CONSTANT(TILE_STATE_REQUESTED);
	BIND_ENUM_CONSTANT(TILE_STATE_DOWNLOADED);
	BIND_ENUM_CONSTANT(TILE_STATE_PARSING);
	BIND_ENUM_CONSTANT(TILE_STATE_CPU_READY);
	BIND_ENUM_CONSTANT(TILE_STATE_UPLOADING);
	BIND_ENUM_CONSTANT(TILE_STATE_RESIDENT);
	BIND_ENUM_CONSTANT(TILE_STATE_FAILED);
	BIND_ENUM_CONSTANT(TILE_STATE_CANCELLED);
}

TiledMeshInstance3D::TiledMeshInstance3D() {
	set_process_internal(true);
	set_notify_transform(true);
	last_layer_mask = get_layer_mask();
}

TiledMeshInstance3D::~TiledMeshInstance3D() {
	close();
}
