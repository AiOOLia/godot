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

#include "tiled_mesh_data_request_queue.h"
#include "tiled_mesh_data_source.h"
#include "tiled_mesh_format_registry.h"
#include "tiled_mesh_image_decode_queue.h"
#include "tiled_mesh_mesh_pack_queue.h"

#include "core/config/engine.h"
#include "core/math/face3.h"
#include "core/math/geometry_3d.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/templates/hash_set.h"
#include "scene/3d/camera_3d.h"
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
		const Vector3 nearest(
				plane.normal.x > 0.0f ? -half_extents.x : half_extents.x,
				plane.normal.y > 0.0f ? -half_extents.y : half_extents.y,
				plane.normal.z > 0.0f ? -half_extents.z : half_extents.z);
		if (plane.is_point_over(center + nearest)) {
			return false;
		}
	}
	return true;
}

// p_offset and the three half axes are in the same world coordinate frame.
static double distance_to_box(const Vector3 &p_offset, const Vector3 (&p_half_axes)[3]) {
	bool orthogonal = true;
	for (int i = 0; i < 3; i++) {
		for (int j = i + 1; j < 3; j++) {
			orthogonal &= Math::abs(p_half_axes[i].dot(p_half_axes[j])) <= CMP_EPSILON * p_half_axes[i].length() * p_half_axes[j].length();
		}
	}
	if (orthogonal) {
		Vector3 closest;
		for (const Vector3 &axis : p_half_axes) {
			const double length_squared = axis.length_squared();
			if (length_squared > 0.0) {
				closest += axis * CLAMP(p_offset.dot(axis) / length_squared, -1.0, 1.0);
			}
		}
		return p_offset.distance_to(closest);
	}

	// Nonuniform node scaling can shear an oriented box. Clamping inverse
	// coordinates does not give the nearest point in that case. Test the
	// faces of the parallelepiped, with a separate inside-volume check.
	const Basis axes(p_half_axes[0], p_half_axes[1], p_half_axes[2]);
	if (axes.determinant() != 0) {
		const Vector3 coordinates = axes.inverse().xform(p_offset).abs();
		if (coordinates.x <= 1 && coordinates.y <= 1 && coordinates.z <= 1) {
			return 0.0;
		}
	}
	double distance_squared = Math::INF;
	for (int i = 0; i < 3; i++) {
		const Vector3 a = p_half_axes[(i + 1) % 3];
		const Vector3 b = p_half_axes[(i + 2) % 3];
		for (int sign : { -1, 1 }) {
			const Vector3 center = p_half_axes[i] * sign;
			const Vector3 corners[] = { center - a - b, center + a - b, center + a + b, center - a + b };
			if (a.cross(b).length_squared() > 0) {
				const Face3 first(corners[0], corners[1], corners[2]);
				const Face3 second(corners[0], corners[2], corners[3]);
				distance_squared = MIN(distance_squared, double(p_offset.distance_squared_to(first.get_closest_point_to(p_offset))));
				distance_squared = MIN(distance_squared, double(p_offset.distance_squared_to(second.get_closest_point_to(p_offset))));
			} else {
				for (int edge = 0; edge < 4; edge++) {
					const Vector3 closest = Geometry3D::get_closest_point_to_segment(p_offset, corners[edge], corners[(edge + 1) % 4]);
					distance_squared = MIN(distance_squared, double(p_offset.distance_squared_to(closest)));
				}
			}
		}
	}
	return Math::sqrt(distance_squared);
}

static uint64_t refinement_cache_key(int p_parent, int p_group) {
	return (uint64_t(uint32_t(p_parent)) << 32) | uint32_t(p_group);
}

} // namespace

void TiledMeshInstance3D::_clear_render_tiles() {
	_cancel_texture_requests_for_tile();
	RenderingServer *rs = RenderingServer::get_singleton();
	for (const TileRenderInstance &tile : render_tiles) {
		if (rs && tile.instance.is_valid()) {
			rs->free_rid(tile.instance);
		}
		if (rs && tile.mesh.is_valid()) {
			rs->free_rid(tile.mesh);
		}
	}
	render_tiles.clear();
	free_render_tiles.clear();
	for (StreamingTile &tile : streaming_tiles) {
		tile.render_tiles.clear();
	}
	for (StreamingContent &content : streaming_contents) {
		content.render_tiles.clear();
	}
	resident_aabb = AABB();
}

void TiledMeshInstance3D::_cancel_texture_requests_for_tile(int p_render_tile) {
	TiledMeshDataRequestQueue *data_queue = TiledMeshDataRequestQueue::get_singleton();
	TiledMeshImageDecodeQueue *decode_queue = TiledMeshImageDecodeQueue::get_singleton();
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

double TiledMeshInstance3D::_get_texture_data_priority(double p_visual_priority) {
	// Geometry/content reads use non-negative priorities. Keep texture reads in
	// a separate lower band so a root texture cannot occupy the local reader or
	// every HTTP slot ahead of newly discovered refinement content, while still
	// preserving the visual ordering between textures.
	const double visual_priority = Math::is_finite(p_visual_priority) ? CLAMP(p_visual_priority, 0.0, 1.0e15) : 0.0;
	return -1.0 / (1.0 + visual_priority);
}

bool TiledMeshInstance3D::_texture_request_is_desired(const TextureRequest &p_request) const {
	if (p_request.render_tile < 0 || p_request.render_tile >= render_tiles.size() || p_request.owner_content < 0 || p_request.owner_content >= streaming_contents.size()) {
		return false;
	}
	const TileRenderInstance &render_tile = render_tiles[p_request.render_tile];
	if (!render_tile.instance.is_valid() || p_request.material_index < 0 || p_request.material_index >= render_tile.materials.size()) {
		return false;
	}
	const StreamingContent &content = streaming_contents[p_request.owner_content];
	Camera3D *display_camera = display_camera_id.is_valid() ? ObjectDB::get_instance<Camera3D>(display_camera_id) : nullptr;
	return content.owner_tile >= 0 && content.owner_tile < streaming_tiles.size() && streaming_tiles[content.owner_tile].desired && render_tile.visible &&
			_render_tile_intersects_camera(render_tile, display_camera);
}

bool TiledMeshInstance3D::_start_texture_request(TextureRequest &r_request) {
	if (!_texture_request_is_desired(r_request) || r_request.data_request_id != 0 || r_request.decode_request_id != 0 || r_request.decoded_image.is_valid()) {
		return true;
	}
	if (!r_request.encoded_bytes.is_empty()) {
		TiledMeshImageDecodeQueue *decode_queue = TiledMeshImageDecodeQueue::get_singleton();
		if (!decode_queue) {
			return false;
		}
		r_request.decode_request_id = decode_queue->enqueue(get_instance_id(), generation, r_request.uri, r_request.encoded_bytes, r_request.priority);
		return r_request.decode_request_id != 0;
	}
	TiledMeshDataRequestQueue *data_queue = TiledMeshDataRequestQueue::get_singleton();
	if (!data_queue) {
		return false;
	}
	r_request.data_request_id = data_queue->request(get_instance_id(), generation, r_request.uri, uint64_t(max_tile_size_mb) * 1024 * 1024,
			request_timeout_sec, retry_count, _get_texture_data_priority(r_request.priority), format_session ? format_session->get_bearer_token(r_request.uri) : String());
	return r_request.data_request_id != 0;
}

void TiledMeshInstance3D::_update_texture_requests(bool p_start_pending) {
	TiledMeshDataRequestQueue *data_queue = TiledMeshDataRequestQueue::get_singleton();
	TiledMeshImageDecodeQueue *decode_queue = TiledMeshImageDecodeQueue::get_singleton();
	for (int i = texture_requests.size() - 1; i >= 0; i--) {
		TextureRequest &request = texture_requests.write[i];
		const bool valid_target = request.render_tile >= 0 && request.render_tile < render_tiles.size() && render_tiles[request.render_tile].instance.is_valid() &&
				request.material_index >= 0 && request.material_index < render_tiles[request.render_tile].materials.size() &&
				request.owner_content >= 0 && request.owner_content < streaming_contents.size();
		if (!valid_target) {
			if (data_queue && request.data_request_id != 0) {
				data_queue->cancel(request.data_request_id);
			}
			if (decode_queue && request.decode_request_id != 0) {
				decode_queue->cancel(request.decode_request_id);
			}
			texture_requests.remove_at(i);
			continue;
		}

		const StreamingContent &content = streaming_contents[request.owner_content];
		const bool desired = _texture_request_is_desired(request);
		if (!desired) {
			if (data_queue && request.data_request_id != 0) {
				data_queue->cancel(request.data_request_id);
			}
			if (decode_queue && request.decode_request_id != 0) {
				decode_queue->cancel(request.decode_request_id);
			}
			request.data_request_id = 0;
			request.decode_request_id = 0;
			request.encoded_bytes.clear();
			request.decoded_image.unref();
			request.priority = 0.0;
			continue;
		}

		request.priority = content.request_priority;
		if (data_queue && request.data_request_id != 0) {
			data_queue->update_priority(request.data_request_id, _get_texture_data_priority(request.priority));
		}
		if (decode_queue && request.decode_request_id != 0) {
			decode_queue->update_priority(request.decode_request_id, request.priority);
		}
		if (p_start_pending) {
			_start_texture_request(request);
		}
	}
}

void TiledMeshInstance3D::_free_render_tile(int p_index, bool p_recompute_bounds) {
	if (p_index < 0 || p_index >= render_tiles.size()) {
		return;
	}
	TileRenderInstance &tile = render_tiles.write[p_index];
	if (!tile.instance.is_valid() && !tile.mesh.is_valid()) {
		return;
	}
	_cancel_texture_requests_for_tile(p_index);
	if (RenderingServer *rs = RenderingServer::get_singleton()) {
		if (tile.instance.is_valid()) {
			rs->free_rid(tile.instance);
		}
		if (tile.mesh.is_valid()) {
			rs->free_rid(tile.mesh);
		}
	}
	if (tile.owner_tile >= 0 && tile.owner_tile < streaming_tiles.size()) {
		streaming_tiles.write[tile.owner_tile].render_tiles.erase(p_index);
	}
	if (tile.owner_content >= 0 && tile.owner_content < streaming_contents.size()) {
		streaming_contents.write[tile.owner_content].render_tiles.erase(p_index);
	}
	tile = TileRenderInstance();
	free_render_tiles.push_back(p_index);
	if (p_recompute_bounds) {
		_recompute_resident_aabb();
	}
}

int TiledMeshInstance3D::_store_render_tile(const TileRenderInstance &p_tile) {
	int index = -1;
	if (free_render_tiles.is_empty()) {
		render_tiles.push_back(p_tile);
		index = render_tiles.size() - 1;
	} else {
		index = free_render_tiles[free_render_tiles.size() - 1];
		free_render_tiles.resize(free_render_tiles.size() - 1);
		render_tiles.write[index] = p_tile;
	}
	if (p_tile.owner_tile >= 0 && p_tile.owner_tile < streaming_tiles.size()) {
		streaming_tiles.write[p_tile.owner_tile].render_tiles.push_back(index);
	}
	if (p_tile.owner_content >= 0 && p_tile.owner_content < streaming_contents.size()) {
		streaming_contents.write[p_tile.owner_content].render_tiles.push_back(index);
	}
	return index;
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
		combined = has_aabb ? combined.merge(tile_aabb) : tile_aabb;
		has_aabb = true;
	}
	resident_aabb = has_aabb ? combined : AABB();
}

Transform3D TiledMeshInstance3D::_get_tile_world_transform(const TileRenderInstance &p_tile) const {
	Transform3D tile_transform;
	tile_transform.origin = world_to_local(p_tile.source_center_x, p_tile.source_center_y, p_tile.source_center_z);
	return get_global_transform() * tile_transform;
}

Error TiledMeshInstance3D::_upload_document(const TiledMeshPackedDocument &p_packed_document, String &r_error, int p_owner_tile, int p_owner_content) {
	r_error.clear();
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		r_error = "RenderingServer is not available.";
		return ERR_UNAVAILABLE;
	}
	const TiledMeshDocument &p_document = p_packed_document.document;
	if (p_packed_document.render_groups.is_empty()) {
		return OK;
	}

	Vector<int> created;
	for (const TiledMeshPackedRenderGroup &render_group : p_packed_document.render_groups) {
		TileRenderInstance tile;
		tile.owner_tile = p_owner_tile;
		tile.owner_content = p_owner_content;
		tile.batch_tables = p_document.batch_tables;
		tile.replacement_group = render_group.replacement_group;
		tile.lod_branch = render_group.lod_branch;
		int branch_index = tile.lod_branch;
		int branch_guard = 0;
		while (branch_index >= 0 && branch_index < p_document.lod_ranges.size() && branch_guard++ <= p_document.lod_ranges.size()) {
			const TiledMeshLodRangePayload &branch = p_document.lod_ranges[branch_index];
			tile.lod_ranges.push_back(branch);
			branch_index = branch.parent;
		}
		tile.source_center_x = p_document.source_origin.x;
		tile.source_center_y = p_document.source_origin.y;
		tile.source_center_z = p_document.source_origin.z;
		Vector<Pair<int, bool>> material_keys;
		Vector<Pair<int, String>> pending_textures;
		Vector<RenderingServerTypes::SurfaceData> mesh_surfaces;
		mesh_surfaces.reserve(render_group.surfaces.size());
		for (const TiledMeshPackedSurface &surface : render_group.surfaces) {
			TiledMeshPickSurface pick_surface;
			pick_surface.vertices = surface.vertices;
			pick_surface.indices = surface.indices;
			pick_surface.batch_ids = surface.batch_ids;
			pick_surface.points = surface.points;
			pick_surface.batch_table = surface.batch_table;
			tile.pick_surfaces.push_back(pick_surface);
			const Pair<int, bool> material_key(surface.material, surface.uses_vertex_color);
			int material_index = material_keys.find(material_key);
			if (material_index < 0) {
				Ref<StandardMaterial3D> material;
				material.instantiate();
				String external_texture_uri;
				if (surface.material >= 0 && surface.material < p_document.materials.size()) {
					const TiledMeshMaterialPayload &payload = p_document.materials[surface.material];
					bool unlit = receive_lighting_mode < 0 ? payload.unlit : receive_lighting_mode == 0;
					material->set_shading_mode(unlit ? BaseMaterial3D::SHADING_MODE_UNSHADED : BaseMaterial3D::SHADING_MODE_PER_PIXEL);
					material->set_meta("authored_unlit", payload.unlit);
					material->set_albedo(payload.albedo);
					material->set_metallic(payload.metallic);
					material->set_roughness(payload.roughness);
					if (!payload.unlit && payload.emission != Color(0, 0, 0, 1)) {
						material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
						material->set_emission(payload.emission);
					}
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
				if (surface.uses_vertex_color) {
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
			RenderingServerTypes::SurfaceData surface_data = surface.surface_data;
			surface_data.material = tile.materials[material_index]->get_rid();
			mesh_surfaces.push_back(surface_data);
			tile.estimated_bytes += surface.estimated_bytes;
		}
		if (mesh_surfaces.is_empty()) {
			continue;
		}
		tile.aabb = render_group.aabb;
		tile.mesh = rs->mesh_create_from_surfaces(mesh_surfaces);
		if (!tile.mesh.is_valid()) {
			r_error = "Unable to create a RenderingServer mesh from packed surface data.";
			break;
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
		created.push_back(render_index);
		for (const Pair<int, String> &pending : pending_textures) {
			TextureRequest request;
			request.render_tile = render_index;
			request.owner_content = p_owner_content;
			request.material_index = pending.first;
			request.uri = pending.second;
			request.priority = p_owner_content >= 0 && p_owner_content < streaming_contents.size() ? streaming_contents[p_owner_content].request_priority : 0.0;
			texture_requests.push_back(request);
		}
	}
	if (!r_error.is_empty()) {
		for (int render_index : created) {
			_free_render_tile(render_index, false);
		}
		_recompute_resident_aabb();
		return ERR_CANT_CREATE;
	}
	_recompute_resident_aabb();
	for (int render_index : created) {
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
	for (const TileRenderInstance &tile : render_tiles) {
		_sync_render_instance(tile);
	}
	last_layer_mask = get_layer_mask();
	last_visible = is_visible_in_tree();
}

Camera3D *TiledMeshInstance3D::_resolve_runtime_camera() const {
	if (!camera_path.is_empty()) {
		return Object::cast_to<Camera3D>(get_node_or_null(camera_path));
	}
	Viewport *viewport = get_viewport();
	return viewport ? viewport->get_camera_3d() : nullptr;
}

void TiledMeshInstance3D::_destroy_format_session() {
	if (format_session) {
		format_session->shutdown();
		memdelete(format_session);
		format_session = nullptr;
	}
	active_loader = nullptr;
	active_format_id = StringName();
}

void TiledMeshInstance3D::_clear_streaming() {
	streaming_requests_pending = false;
	TiledMeshDataRequestQueue *data_queue = TiledMeshDataRequestQueue::get_singleton();
	TiledMeshMeshPackQueue *pack_queue = TiledMeshMeshPackQueue::get_singleton();
	if (data_queue && root_data_request_id != 0) {
		data_queue->cancel(root_data_request_id);
	}
	root_data_request_id = 0;
	for (StreamingContent &content : streaming_contents) {
		if (data_queue && content.data_request_id != 0) {
			data_queue->cancel(content.data_request_id);
		}
		if (format_session && content.parse_request_id != 0) {
			format_session->cancel(content.parse_request_id);
		}
		if (pack_queue && content.pack_request_id != 0) {
			pack_queue->cancel(content.pack_request_id);
		}
		content.data_request_id = 0;
		content.parse_request_id = 0;
		content.pack_request_id = 0;
		content.has_packed_document = false;
		content.packed_document = TiledMeshPackedDocument();
	}
	_destroy_format_session();
	streaming_contents.clear();
	streaming_tiles.clear();
	streaming_tile_by_key.clear();
	root_tile = -1;
	root_content = -1;
	display_camera_id = ObjectID();
	streaming_camera_states.clear();
	streaming_topology_dirty = true;
}

int TiledMeshInstance3D::_add_content(int p_owner_tile, const TiledMeshContentRequest &p_descriptor) {
	if (p_owner_tile < 0 || p_owner_tile >= streaming_tiles.size()) {
		return -1;
	}
	TiledMeshContentRequest descriptor = p_descriptor;
	descriptor.uri = TiledMeshDataSource::normalize_uri(descriptor.uri);
	if (descriptor.uri.is_empty()) {
		return -1;
	}
	StreamingContent content;
	content.owner_tile = p_owner_tile;
	content.descriptor = descriptor;
	streaming_contents.push_back(content);
	const int content_index = streaming_contents.size() - 1;
	streaming_tiles.write[p_owner_tile].contents.push_back(content_index);
	return content_index;
}

void TiledMeshInstance3D::_register_tiles(const TiledMeshDocument &p_document, int p_parent) {
	if (p_document.tiles.is_empty()) {
		return;
	}
	streaming_topology_dirty = true;
	Vector<int> local_to_runtime;
	local_to_runtime.resize(p_document.tiles.size());
	local_to_runtime.fill(-1);
	for (int definition_index = 0; definition_index < p_document.tiles.size(); definition_index++) {
		const TiledMeshTileDefinition &definition = p_document.tiles[definition_index];
		int parent = p_parent;
		if (definition.parent >= 0) {
			if (definition.parent >= definition_index || local_to_runtime[definition.parent] < 0) {
				WARN_PRINT(vformat("TiledMeshInstance3D: Format '%s' returned a tile whose local parent is not an earlier valid definition.", active_format_id));
				continue;
			}
			parent = local_to_runtime[definition.parent];
		}
		if (parent < 0 || parent >= streaming_tiles.size()) {
			continue;
		}

		String key = definition.provider_key;
		if (key.is_empty() && !definition.contents.is_empty()) {
			key = definition.contents[0].uri;
		}
		if (key.is_empty()) {
			key = vformat("%s:%d:%d", active_format_id, parent, definition_index);
		}
		int tile_index = -1;
		if (const int *existing = streaming_tile_by_key.getptr(key)) {
			tile_index = *existing;
		} else {
			StreamingTile tile;
			tile.provider_key = key;
			tile.parent = parent;
			tile.replacement_group = definition.replacement_group;
			tile.refinement = definition.refinement;
			tile.bounding_volume = definition.bounding_volume;
			tile.selection = definition.selection;
			streaming_tiles.push_back(tile);
			tile_index = streaming_tiles.size() - 1;
			streaming_tile_by_key.insert(key, tile_index);
			for (const TiledMeshContentRequest &content : definition.contents) {
				_add_content(tile_index, content);
			}
		}
		local_to_runtime.write[definition_index] = tile_index;
		if (!streaming_tiles[parent].children.has(tile_index) && tile_index != parent) {
			streaming_tiles.write[parent].children.push_back(tile_index);
		}

		StreamingTile &tile = streaming_tiles.write[tile_index];
		for (int render_index : streaming_tiles[parent].render_tiles) {
			if (render_index < 0 || render_index >= render_tiles.size()) {
				continue;
			}
			const TileRenderInstance &render_tile = render_tiles[render_index];
			if (!render_tile.instance.is_valid() || render_tile.replacement_group != tile.replacement_group) {
				continue;
			}
			AABB replacement_aabb = render_tile.aabb;
			replacement_aabb.position += world_to_local(render_tile.source_center_x, render_tile.source_center_y, render_tile.source_center_z);
			tile.replacement_aabb = tile.has_replacement_aabb ? tile.replacement_aabb.merge(replacement_aabb) : replacement_aabb;
			tile.has_replacement_aabb = true;
		}
	}
}

bool TiledMeshInstance3D::_parent_is_desired(int p_parent) const {
	return p_parent < 0 || (p_parent < streaming_tiles.size() && streaming_tiles[p_parent].desired);
}

bool TiledMeshInstance3D::_parent_is_display_desired(int p_parent) const {
	return p_parent < 0 || (p_parent < streaming_tiles.size() && streaming_tiles[p_parent].display_desired);
}

bool TiledMeshInstance3D::_tile_required_contents_resident(int p_tile) const {
	if (p_tile < 0 || p_tile >= streaming_tiles.size()) {
		return false;
	}
	for (int content_index : streaming_tiles[p_tile].contents) {
		const StreamingContent &content = streaming_contents[content_index];
		if (content.descriptor.required_for_display && content.state != TILE_STATE_RESIDENT) {
			return false;
		}
	}
	return true;
}

bool TiledMeshInstance3D::_tile_has_render_geometry(int p_tile) const {
	if (p_tile < 0 || p_tile >= streaming_tiles.size()) {
		return false;
	}
	for (int render_index : streaming_tiles[p_tile].render_tiles) {
		if (render_index >= 0 && render_index < render_tiles.size() && render_tiles[render_index].instance.is_valid()) {
			return true;
		}
	}
	return false;
}

bool TiledMeshInstance3D::_tile_is_displayable(int p_index) const {
	if (p_index < 0 || p_index >= streaming_tiles.size() || !_tile_required_contents_resident(p_index)) {
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
	const auto cache_result = [this, p_index](bool p_value) {
		tile_displayable_cache.write[p_index] = p_value ? 3 : 2;
		return p_value;
	};

	const StreamingTile &tile = streaming_tiles[p_index];
	Camera3D *display_camera = display_camera_id.is_valid() ? ObjectDB::get_instance<Camera3D>(display_camera_id) : nullptr;
	HashSet<int> required_groups;
	HashSet<int> intersecting_groups;
	for (int render_index : tile.render_tiles) {
		const TileRenderInstance &render_tile = render_tiles[render_index];
		if (render_tile.instance.is_valid() && render_tile.replacement_group >= 0) {
			required_groups.insert(render_tile.replacement_group);
			if (_render_tile_intersects_camera(render_tile, display_camera)) {
				intersecting_groups.insert(render_tile.replacement_group);
			}
		}
	}
	for (int child_index : tile.children) {
		const StreamingTile &child = streaming_tiles[child_index];
		if (child.refinement == TiledMeshTileDefinition::REFINE_REPLACE && child.replacement_group >= 0) {
			required_groups.insert(child.replacement_group);
			if (_streaming_tile_intersects_camera(child, display_camera)) {
				intersecting_groups.insert(child.replacement_group);
			}
		}
	}
	for (int group : required_groups) {
		// Independent replacement groups outside the display camera do not
		// contribute any visible coverage. Requiring an inactive off-screen
		// group to be displayable keeps a broader ancestor fallback visible over
		// the resident detail of the groups that are actually on screen.
		if (!intersecting_groups.has(group)) {
			continue;
		}
		bool displayable = _parent_group_is_fully_refined(p_index, group);
		const bool pending = _parent_group_has_display_refinement(p_index, group) && !displayable;
		if (!displayable) {
			for (int render_index : tile.render_tiles) {
				const TileRenderInstance &render_tile = render_tiles[render_index];
				if (render_tile.replacement_group == group && render_tile.instance.is_valid() && (_render_lod_matches(render_tile, display_camera) || pending)) {
					displayable = true;
					break;
				}
			}
		}
		if (!displayable) {
			return cache_result(false);
		}
	}
	if (!required_groups.is_empty()) {
		// A tile remains a complete replacement for its parent when each of its
		// own replacement regions is covered, even if that coverage now comes
		// entirely from deeper descendants. Requiring one of this tile's local
		// fallback render instances to remain visible would keep an ancestor's
		// coarse geometry visible throughout a multi-level REPLACE chain.
		return cache_result(true);
	}
	for (int render_index : tile.render_tiles) {
		const TileRenderInstance &render_tile = render_tiles[render_index];
		if (!render_tile.instance.is_valid()) {
			continue;
		}
		const bool refined = render_tile.replacement_group >= 0 && _parent_group_is_fully_refined(p_index, render_tile.replacement_group);
		const bool pending = render_tile.replacement_group >= 0 && _parent_group_has_display_refinement(p_index, render_tile.replacement_group) && !refined;
		if (!refined && (_render_lod_matches(render_tile, display_camera) || pending)) {
			return cache_result(true);
		}
	}
	if (_tile_has_render_geometry(p_index)) {
		return cache_result(false);
	}
	return cache_result(_parent_is_fully_refined(p_index) || tile.children.is_empty());
}

bool TiledMeshInstance3D::_parent_is_fully_refined(int p_parent) const {
	if (const bool *cached = parent_refined_cache.getptr(p_parent)) {
		return *cached;
	}
	if (p_parent < 0 || p_parent >= streaming_tiles.size()) {
		return false;
	}
	bool has_child = false;
	for (int child_index : streaming_tiles[p_parent].children) {
		const StreamingTile &child = streaming_tiles[child_index];
		if (child.refinement != TiledMeshTileDefinition::REFINE_REPLACE || !child.display_desired) {
			continue;
		}
		has_child = true;
		if (!_tile_is_displayable(child_index)) {
			parent_refined_cache.insert(p_parent, false);
			return false;
		}
	}
	parent_refined_cache.insert(p_parent, has_child);
	return has_child;
}

bool TiledMeshInstance3D::_parent_group_is_fully_refined(int p_parent, int p_group) const {
	const uint64_t cache_key = refinement_cache_key(p_parent, p_group);
	if (const bool *cached = group_refined_cache.getptr(cache_key)) {
		return *cached;
	}
	if (p_parent < 0 || p_parent >= streaming_tiles.size()) {
		return false;
	}
	bool has_child = false;
	for (int child_index : streaming_tiles[p_parent].children) {
		const StreamingTile &child = streaming_tiles[child_index];
		if (child.refinement != TiledMeshTileDefinition::REFINE_REPLACE || child.replacement_group != p_group || !child.display_desired) {
			continue;
		}
		has_child = true;
		if (!_tile_is_displayable(child_index)) {
			group_refined_cache.insert(cache_key, false);
			return false;
		}
	}
	group_refined_cache.insert(cache_key, has_child);
	return has_child;
}

bool TiledMeshInstance3D::_parent_group_has_display_refinement(int p_parent, int p_group) const {
	const uint64_t cache_key = refinement_cache_key(p_parent, p_group);
	if (const bool *cached = group_pending_cache.getptr(cache_key)) {
		return *cached;
	}
	if (p_parent >= 0 && p_parent < streaming_tiles.size()) {
		for (int child_index : streaming_tiles[p_parent].children) {
			const StreamingTile &child = streaming_tiles[child_index];
			if (child.refinement == TiledMeshTileDefinition::REFINE_REPLACE && child.replacement_group == p_group && child.display_desired) {
				group_pending_cache.insert(cache_key, true);
				return true;
			}
		}
	}
	group_pending_cache.insert(cache_key, false);
	return false;
}

double TiledMeshInstance3D::_get_lod_metric(const TiledMeshSelectionRule &p_selection, Camera3D *p_camera) const {
	if (!p_camera) {
		return 0.0;
	}
	const Vector3 local_center = world_to_local(p_selection.center.x, p_selection.center.y, p_selection.center.z);
	const Vector3 global_center = get_global_transform().xform(local_center);
	const Vector3 global_scale = get_global_transform().basis.get_scale().abs();
	const double maximum_global_scale = MAX(global_scale.x, MAX(global_scale.y, global_scale.z));
	const double radius = MAX(p_selection.radius * unit_scale * maximum_global_scale, 0.0);
	const double active_bias = Engine::get_singleton()->is_editor_hint() ? editor_preview_lod_bias : lod_bias;
	if (p_selection.mode == TiledMeshSelectionRule::MODE_DISTANCE_RANGE) {
		return p_camera->get_camera_transform().origin.distance_to(global_center) * active_bias;
	}

	Viewport *viewport = p_camera->get_viewport();
	const Vector2 viewport_size = viewport ? viewport->get_visible_rect().size : Vector2(1.0, 1.0);
	const Projection projection = p_camera->get_camera_projection();
	const double focal_x = Math::abs(double(projection.columns[0].x)) * viewport_size.x * 0.5;
	const double focal_y = Math::abs(double(projection.columns[1].y)) * viewport_size.y * 0.5;
	const double focal_pixels = Math::sqrt((focal_x * focal_x + focal_y * focal_y) * 0.5);
	if (p_selection.mode == TiledMeshSelectionRule::MODE_SCREEN_SPACE_ERROR) {
		double distance = 1.0;
		if (p_camera->get_projection() != Camera3D::PROJECTION_ORTHOGONAL) {
			const Vector3 camera_offset = p_camera->get_camera_transform().origin - global_center;
			if (p_selection.volume_type == TiledMeshBoundingVolume::TYPE_ORIENTED_BOX) {
				const Basis source_to_global = get_global_transform().basis * source_to_local_basis;
				Vector3 half_axes[3];
				for (int i = 0; i < 3; i++) {
					const TiledMeshVector3d &axis = p_selection.half_axes[i];
					half_axes[i] = source_to_global.xform(Vector3(axis.x * unit_scale, axis.y * unit_scale, axis.z * unit_scale));
				}
				distance = distance_to_box(camera_offset, half_axes);
			} else {
				distance = MAX(double(camera_offset.length()) - radius, 0.0);
			}
		}
		// A camera on or inside the volume must keep refining nonzero error.
		// Lower bias means more detail, consistently with the other LOD modes.
		return p_selection.geometric_error * unit_scale * maximum_global_scale * focal_y / MAX(distance, 0.000001) / MAX(active_bias, 0.01);
	}
	double depth = 1.0;
	if (p_camera->get_projection() != Camera3D::PROJECTION_ORTHOGONAL) {
		depth = MAX(Math::abs(double(p_camera->get_camera_transform().affine_inverse().xform(global_center).z)), 0.000001);
	}
	return radius * 2.0 * focal_pixels / depth / MAX(active_bias, 0.01);
}

bool TiledMeshInstance3D::_selection_matches(const TiledMeshSelectionRule &p_selection, bool p_was_matching, Camera3D *p_camera) const {
	if (p_selection.mode == TiledMeshSelectionRule::MODE_ALWAYS) {
		return true;
	}
	if (!p_camera) {
		return false;
	}
	const double metric = _get_lod_metric(p_selection, p_camera);
	if (p_selection.mode == TiledMeshSelectionRule::MODE_SCREEN_SPACE_ERROR) {
		double threshold = maximum_screen_space_error;
		if (p_was_matching) {
			threshold *= 1.0 - lod_hysteresis;
		}
		return metric > threshold;
	}
	double minimum = p_selection.minimum;
	double maximum = p_selection.maximum;
	if (p_was_matching) {
		minimum *= 1.0 - lod_hysteresis;
		maximum *= 1.0 + lod_hysteresis;
	}
	return metric >= minimum && metric < maximum;
}

bool TiledMeshInstance3D::_render_lod_matches(const TileRenderInstance &p_tile, Camera3D *p_camera) const {
	for (const TiledMeshLodRangePayload &range : p_tile.lod_ranges) {
		if (!_selection_matches(range.selection, false, p_camera)) {
			return false;
		}
	}
	return true;
}

bool TiledMeshInstance3D::_render_tile_intersects_camera(const TileRenderInstance &p_tile, Camera3D *p_camera) const {
	if (!p_camera || !p_tile.instance.is_valid()) {
		return p_camera == nullptr;
	}
	AABB local_aabb = p_tile.aabb;
	local_aabb.position += world_to_local(p_tile.source_center_x, p_tile.source_center_y, p_tile.source_center_z);
	return aabb_intersects_camera_frustum(get_global_transform().xform(local_aabb), p_camera);
}

bool TiledMeshInstance3D::_streaming_tile_intersects_camera(const StreamingTile &p_tile, Camera3D *p_camera) const {
	if (!p_camera || p_tile.selection.mode == TiledMeshSelectionRule::MODE_ALWAYS) {
		return true;
	}
	if (p_tile.has_replacement_aabb) {
		return aabb_intersects_camera_frustum(get_global_transform().xform(p_tile.replacement_aabb), p_camera);
	}
	if (p_tile.bounding_volume.type == TiledMeshBoundingVolume::TYPE_ORIENTED_BOX) {
		bool has_aabb = false;
		AABB aabb;
		for (int mask = 0; mask < 8; mask++) {
			TiledMeshVector3d corner = p_tile.bounding_volume.center;
			for (int axis = 0; axis < 3; axis++) {
				const double sign = (mask & (1 << axis)) ? 1.0 : -1.0;
				corner.x += p_tile.bounding_volume.half_axes[axis].x * sign;
				corner.y += p_tile.bounding_volume.half_axes[axis].y * sign;
				corner.z += p_tile.bounding_volume.half_axes[axis].z * sign;
			}
			const Vector3 point = get_global_transform().xform(world_to_local(corner.x, corner.y, corner.z));
			if (!has_aabb) {
				aabb = AABB(point, Vector3());
				has_aabb = true;
			} else {
				aabb.expand_to(point);
			}
		}
		return has_aabb && aabb_intersects_camera_frustum(aabb, p_camera);
	}
	const Vector3 center = get_global_transform().xform(world_to_local(p_tile.bounding_volume.center.x, p_tile.bounding_volume.center.y, p_tile.bounding_volume.center.z));
	const Vector3 scale = get_global_transform().basis.get_scale().abs();
	const double radius = MAX((p_tile.bounding_volume.radius * 1.1 + 0.1) * unit_scale * MAX(scale.x, MAX(scale.y, scale.z)), 0.0);
	for (const Plane &plane : p_camera->get_frustum()) {
		if (plane.distance_to(center) > radius) {
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
	if (p_tile.has_replacement_aabb) {
		const AABB global_aabb = get_global_transform().xform(p_tile.replacement_aabb);
		global_center = global_aabb.get_center();
		global_radius = global_aabb.size.length() * 0.5;
	} else {
		global_center = get_global_transform().xform(world_to_local(p_tile.bounding_volume.center.x, p_tile.bounding_volume.center.y, p_tile.bounding_volume.center.z));
		const Vector3 global_scale = get_global_transform().basis.get_scale().abs();
		global_radius = p_tile.bounding_volume.radius * unit_scale * MAX(global_scale.x, MAX(global_scale.y, global_scale.z));
	}
	Viewport *viewport = p_camera->get_viewport();
	const Vector2 viewport_size = viewport ? viewport->get_visible_rect().size : Vector2(1.0, 1.0);
	const Projection projection = p_camera->get_camera_projection();
	const double focal_x = Math::abs(double(projection.columns[0].x)) * viewport_size.x * 0.5;
	const double focal_y = Math::abs(double(projection.columns[1].y)) * viewport_size.y * 0.5;
	const double focal_pixels = Math::sqrt((focal_x * focal_x + focal_y * focal_y) * 0.5);
	double pixel_diameter = MAX(global_radius * 2.0 * focal_pixels, 1.0);
	if (p_camera->get_projection() != Camera3D::PROJECTION_ORTHOGONAL) {
		const double nearest_depth = Math::abs(double(p_camera->get_camera_transform().affine_inverse().xform(global_center).z)) - global_radius;
		pixel_diameter = nearest_depth <= 0.000001 ? viewport_size.length() : pixel_diameter / nearest_depth;
	}
	const double viewport_area = MAX(double(viewport_size.x) * viewport_size.y, 1.0);
	double projected_area = viewport_area;
	if (!p_camera->is_position_behind(global_center)) {
		const Vector2 screen_center = p_camera->unproject_position(global_center);
		const Rect2 projected_rect(screen_center - Vector2(pixel_diameter, pixel_diameter) * 0.5, Vector2(pixel_diameter, pixel_diameter));
		projected_area = MAX(double(projected_rect.intersection(Rect2(Vector2(), viewport_size)).get_area()), 1.0);
	}
	double urgency = 0.5;
	if (p_tile.selection.mode == TiledMeshSelectionRule::MODE_SCREEN_SPACE_ERROR) {
		urgency = CLAMP(_get_lod_metric(p_tile.selection, p_camera) / MAX(maximum_screen_space_error, 0.01), 0.0, 4.0);
	} else if (Math::is_finite(p_tile.selection.maximum) && p_tile.selection.maximum > p_tile.selection.minimum + CMP_EPSILON) {
		const double metric = _get_lod_metric(p_tile.selection, p_camera);
		if (p_tile.selection.mode == TiledMeshSelectionRule::MODE_PROJECTED_PIXEL_RANGE) {
			urgency = CLAMP((metric - p_tile.selection.minimum) / (p_tile.selection.maximum - p_tile.selection.minimum), 0.0, 1.0);
		} else {
			urgency = CLAMP((p_tile.selection.maximum - metric) / (p_tile.selection.maximum - p_tile.selection.minimum), 0.0, 1.0);
		}
	}
	return projected_area * (1.0 + urgency) * (p_active_camera ? 1.25 : 1.0);
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
		Viewport *viewport = camera->get_viewport();
		state.viewport_size = viewport ? viewport->get_visible_rect().size : Vector2(1.0, 1.0);
		camera_states.push_back(state);
	}
	bool cameras_changed = camera_states.size() != streaming_camera_states.size() || new_display_camera_id != display_camera_id;
	for (int i = 0; !cameras_changed && i < camera_states.size(); i++) {
		cameras_changed = camera_states[i].id != streaming_camera_states[i].id || camera_states[i].transform != streaming_camera_states[i].transform ||
				camera_states[i].projection != streaming_camera_states[i].projection || camera_states[i].viewport_size != streaming_camera_states[i].viewport_size;
	}
	if (!streaming_topology_dirty && !cameras_changed) {
		// Selection can be cached, but a request slot or shared pack backlog
		// may have become available since the previous frame.
		if (streaming_requests_pending) {
			_request_streaming_contents();
		}
		return;
	}
	streaming_topology_dirty = false;
	streaming_camera_states = camera_states;
	display_camera_id = new_display_camera_id;
	const uint64_t now = Time::get_singleton()->get_ticks_usec();

	for (int tile_index = 0; tile_index < streaming_tiles.size(); tile_index++) {
		StreamingTile &tile = streaming_tiles.write[tile_index];
		bool desired = tile_index == root_tile;
		bool display_desired = tile_index == root_tile;
		double priority = desired ? 1.0e15 : 0.0;
		if (tile_index != root_tile && _parent_is_desired(tile.parent)) {
			for (Camera3D *camera : p_cameras) {
				if (_selection_matches(tile.selection, tile.desired, camera) && _streaming_tile_intersects_camera(tile, camera)) {
					desired = true;
					priority = MAX(priority, _get_request_priority(tile, camera, camera == display_camera));
				}
			}
		}
		if (tile_index != root_tile && _parent_is_display_desired(tile.parent) && _selection_matches(tile.selection, tile.display_desired, display_camera) && _streaming_tile_intersects_camera(tile, display_camera)) {
			display_desired = true;
		}
		tile.desired = desired;
		tile.display_desired = display_desired;
		for (int content_index : tile.contents) {
			StreamingContent &content = streaming_contents.write[content_index];
			content.request_priority = priority;
			if (desired) {
				content.last_used_usec = now;
				if (content.data_request_id != 0) {
					TiledMeshDataRequestQueue::get_singleton()->update_priority(content.data_request_id, priority);
				}
				if (format_session && content.parse_request_id != 0) {
					format_session->update_priority(content.parse_request_id, priority);
				}
				if (TiledMeshMeshPackQueue *pack_queue = TiledMeshMeshPackQueue::get_singleton()) {
					if (content.pack_request_id != 0) {
						pack_queue->update_priority(content.pack_request_id, priority);
					}
				}
			} else if (content.state == TILE_STATE_REQUESTED || content.state == TILE_STATE_DOWNLOADED || content.state == TILE_STATE_PARSING || content.state == TILE_STATE_CPU_READY) {
				if (content.data_request_id != 0 && TiledMeshDataRequestQueue::get_singleton()) {
					TiledMeshDataRequestQueue::get_singleton()->cancel(content.data_request_id);
				}
				if (content.parse_request_id != 0 && format_session) {
					format_session->cancel(content.parse_request_id);
				}
				if (content.pack_request_id != 0 && TiledMeshMeshPackQueue::get_singleton()) {
					TiledMeshMeshPackQueue::get_singleton()->cancel(content.pack_request_id);
				}
				content.data_request_id = 0;
				content.parse_request_id = 0;
				content.pack_request_id = 0;
				content.has_packed_document = false;
				content.packed_document = TiledMeshPackedDocument();
				content.state = TILE_STATE_CANCELLED;
			}
		}
	}
	_request_streaming_contents();
	_update_streaming_visibility();
	_update_texture_requests();
	_enforce_memory_budget();
}

void TiledMeshInstance3D::_request_streaming_contents() {
	streaming_requests_pending = false;
	const int configured_limit = Engine::get_singleton()->is_editor_hint() ? editor_preview_max_requests : max_concurrent_requests;
	const int request_limit = configured_limit > 0 ? configured_limit : (OS::get_singleton()->has_feature("web") ? 4 : 8);
	TiledMeshDataRequestQueue *data_queue = TiledMeshDataRequestQueue::get_singleton();
	TiledMeshMeshPackQueue *pack_queue = TiledMeshMeshPackQueue::get_singleton();
	if (!data_queue || !format_session) {
		return;
	}
	const int configured_memory_mb = Engine::get_singleton()->is_editor_hint() ? editor_preview_memory_mb : memory_budget_mb;
	const uint64_t resident_budget = uint64_t(configured_memory_mb > 0 ? configured_memory_mb : (OS::get_singleton()->has_feature("web") ? 256 : 512)) * 1024 * 1024;
	const uint64_t pack_backlog_byte_limit = MAX(uint64_t(64 * 1024 * 1024), resident_budget / 2);
	for (;;) {
		// Parsing may continue while one document is packed, but do not let a
		// faster adapter build an unbounded queue of documents whose original
		// arrays temporarily coexist with their packed SurfaceData.
		if (pack_queue && (pack_queue->get_pending_count() >= request_limit || pack_queue->get_in_flight_bytes() >= pack_backlog_byte_limit)) {
			streaming_requests_pending = true;
			break;
		}
		int best_content = -1;
		double best_priority = -1.0;
		for (int i = 0; i < streaming_contents.size(); i++) {
			const StreamingContent &content = streaming_contents[i];
			const bool eligible = content.owner_tile >= 0 && streaming_tiles[content.owner_tile].desired &&
					(content.state == TILE_STATE_UNLOADED || content.state == TILE_STATE_CANCELLED);
			if (eligible && content.request_priority > best_priority) {
				best_priority = content.request_priority;
				best_content = i;
			}
		}
		if (best_content < 0) {
			break;
		}
		int higher_priority_pending = 0;
		for (const StreamingContent &content : streaming_contents) {
			const bool pending = content.state == TILE_STATE_REQUESTED || content.state == TILE_STATE_DOWNLOADED || content.state == TILE_STATE_PARSING;
			if (pending && content.request_priority >= best_priority) {
				higher_priority_pending++;
			}
		}
		if (higher_priority_pending >= request_limit) {
			streaming_requests_pending = true;
			break;
		}
		StreamingContent &content = streaming_contents.write[best_content];
		content.state = TILE_STATE_REQUESTED;
		content.data_request_id = data_queue->request(get_instance_id(), generation, content.descriptor.uri, uint64_t(max_tile_size_mb) * 1024 * 1024,
				request_timeout_sec, retry_count, content.request_priority, format_session ? format_session->get_bearer_token(content.descriptor.uri) : String());
		if (content.data_request_id == 0) {
			content.state = TILE_STATE_FAILED;
			emit_signal(SNAME("tile_failed"), content.descriptor.uri, "Unable to queue the tiled mesh content request.");
		}
	}
}

Error TiledMeshInstance3D::_select_format_and_start_parse(const String &p_uri, const PackedByteArray &p_bytes) {
	StringName requested_format = format_id;
	if (requested_format.is_empty() && format_options.is_valid()) {
		requested_format = format_options->get_format_id();
	}
	String selection_error;
	TiledMeshFormatLoader *loader = requested_format.is_empty() ? TiledMeshFormatRegistry::recognize(p_uri, p_bytes, selection_error) : TiledMeshFormatRegistry::find_by_id(requested_format);
	if (!loader) {
		if (selection_error.is_empty()) {
			selection_error = vformat("No tiled mesh format loader named '%s' is registered.", requested_format);
		}
		_set_load_failed(p_uri, selection_error);
		return ERR_FILE_UNRECOGNIZED;
	}
	if (!loader->supports_options(format_options)) {
		_set_load_failed(p_uri, vformat("format_options is not compatible with tiled mesh format '%s'.", loader->get_format_id()));
		return ERR_INVALID_PARAMETER;
	}

	TiledMeshSessionOptions options;
	options.unit_scale = unit_scale;
	options.use_format_default_origin = world_origin_mode == WORLD_ORIGIN_FORMAT_DEFAULT;
	options.explicit_origin = { world_origin_x, world_origin_y, world_origin_z };
	format_session = loader->create_session(format_options, options, selection_error);
	if (!format_session) {
		_set_load_failed(p_uri, selection_error.is_empty() ? "Unable to create the tiled mesh format session." : selection_error);
		return ERR_CANT_CREATE;
	}
	active_loader = loader;
	active_format_id = loader->get_format_id();

	StreamingContent &root = streaming_contents.write[root_content];
	root.source_bytes = p_bytes.size();
	root.state = TILE_STATE_DOWNLOADED;
	TiledMeshParseRequest request;
	request.owner = get_instance_id();
	request.generation = generation;
	request.uri = p_uri;
	request.content_type = root.descriptor.content_type;
	request.context = root.descriptor.context;
	request.bytes = p_bytes;
	request.root = true;
	root.parse_request_id = format_session->enqueue_parse(request, 1.0e15);
	if (root.parse_request_id == 0) {
		root.source_bytes = 0;
		root.state = TILE_STATE_FAILED;
		_set_load_failed(p_uri, "Unable to queue the root format parse request.");
		return ERR_CANT_CREATE;
	}
	root.state = TILE_STATE_PARSING;
	return OK;
}

void TiledMeshInstance3D::_poll_data_results() {
	TiledMeshDataRequestQueue *data_queue = TiledMeshDataRequestQueue::get_singleton();
	if (!data_queue) {
		return;
	}
	data_queue->poll();
	if (root_data_request_id != 0) {
		TiledMeshDataRequestQueue::Result result;
		if (data_queue->take_result(root_data_request_id, result)) {
			root_data_request_id = 0;
			if (result.generation == generation && result.owner == get_instance_id()) {
				if (result.error != OK) {
					streaming_contents.write[root_content].state = TILE_STATE_FAILED;
					_set_load_failed(result.uri, result.error_message);
				} else {
					_select_format_and_start_parse(result.uri, result.bytes);
				}
			}
		}
	}
	bool completed = false;
	for (StreamingContent &content : streaming_contents) {
		if (content.data_request_id == 0) {
			continue;
		}
		TiledMeshDataRequestQueue::Result result;
		if (!data_queue->take_result(content.data_request_id, result)) {
			continue;
		}
		content.data_request_id = 0;
		completed = true;
		if (result.generation != generation || result.owner != get_instance_id()) {
			content.state = TILE_STATE_CANCELLED;
			continue;
		}
		if (result.error != OK) {
			content.state = TILE_STATE_FAILED;
			emit_signal(SNAME("tile_failed"), content.descriptor.uri, result.error_message);
			continue;
		}
		content.source_bytes = result.bytes.size();
		content.state = TILE_STATE_DOWNLOADED;
		TiledMeshParseRequest request;
		request.owner = get_instance_id();
		request.generation = generation;
		request.uri = result.uri;
		request.content_type = content.descriptor.content_type;
		request.context = content.descriptor.context;
		request.bytes = result.bytes;
		content.parse_request_id = format_session ? format_session->enqueue_parse(request, content.request_priority) : 0;
		if (content.parse_request_id == 0) {
			content.source_bytes = 0;
			content.state = TILE_STATE_FAILED;
			emit_signal(SNAME("tile_failed"), content.descriptor.uri, "Unable to queue the format parse request.");
		} else {
			content.state = TILE_STATE_PARSING;
		}
	}
	if (completed) {
		_request_streaming_contents();
	}
}

void TiledMeshInstance3D::_poll_texture_results() {
	TiledMeshDataRequestQueue *data_queue = TiledMeshDataRequestQueue::get_singleton();
	TiledMeshImageDecodeQueue *decode_queue = TiledMeshImageDecodeQueue::get_singleton();
	if (!data_queue || !decode_queue) {
		return;
	}
	for (int i = texture_requests.size() - 1; i >= 0; i--) {
		TextureRequest &request = texture_requests.write[i];
		bool remove_request = false;
		bool decode_started_this_poll = false;
		if (request.data_request_id != 0) {
			TiledMeshDataRequestQueue::Result result;
			if (data_queue->take_result(request.data_request_id, result)) {
				request.data_request_id = 0;
				const bool current = result.generation == generation && result.owner == get_instance_id();
				const bool valid_target = request.render_tile >= 0 && request.render_tile < render_tiles.size() && render_tiles[request.render_tile].instance.is_valid();
				if (current && result.error == OK && valid_target) {
					request.encoded_bytes = result.bytes;
					if (_texture_request_is_desired(request) && !_start_texture_request(request)) {
						WARN_PRINT(vformat("TiledMeshInstance3D: Unable to queue texture decode for '%s'.", request.uri));
						remove_request = true;
					} else {
						decode_started_this_poll = request.decode_request_id != 0;
					}
				} else {
					if (current && result.error != OK && !result.error_message.is_empty()) {
						WARN_PRINT(vformat("TiledMeshInstance3D: %s", result.error_message));
					}
					remove_request = true;
				}
			}
		}
		if (!remove_request && !decode_started_this_poll && request.decode_request_id != 0) {
			TiledMeshImageDecodeQueue::Result result;
			if (decode_queue->take_result(request.decode_request_id, result)) {
				request.decode_request_id = 0;
				request.encoded_bytes.clear();
				const bool current = result.generation == generation && result.owner == get_instance_id();
				if (current && result.error == OK && result.image.is_valid() && _texture_request_is_desired(request)) {
					request.decoded_image = result.image;
				} else {
					if (current && result.error != OK && !result.error_message.is_empty()) {
						WARN_PRINT(vformat("TiledMeshInstance3D: %s", result.error_message));
					}
					remove_request = true;
				}
			}
		}
		if (remove_request) {
			texture_requests.remove_at(i);
		}
	}

	// Decode completions may arrive together. Install the most visually useful
	// images first instead of depending on render-instance insertion order.
	for (int texture_uploads = 0; texture_uploads < 2; texture_uploads++) {
		int best_request = -1;
		double best_priority = -1.0;
		for (int i = 0; i < texture_requests.size(); i++) {
			const TextureRequest &request = texture_requests[i];
			if (request.decoded_image.is_valid() && _texture_request_is_desired(request) && request.priority > best_priority) {
				best_priority = request.priority;
				best_request = i;
			}
		}
		if (best_request < 0) {
			break;
		}
		const TextureRequest &request = texture_requests[best_request];
		TileRenderInstance &tile = render_tiles.write[request.render_tile];
		StandardMaterial3D *material = Object::cast_to<StandardMaterial3D>(tile.materials[request.material_index].ptr());
		if (material) {
			material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(request.decoded_image));
			tile.texture_bytes += uint64_t(request.decoded_image->get_width()) * uint64_t(request.decoded_image->get_height()) * 4;
		}
		texture_requests.remove_at(best_request);
	}
}

void TiledMeshInstance3D::_poll_parse_results() {
	if (!format_session) {
		return;
	}
	TiledMeshMeshPackQueue *pack_queue = TiledMeshMeshPackQueue::get_singleton();
	bool completed = false;
	bool root_state_changed = false;
	for (StreamingContent &content : streaming_contents) {
		if (content.parse_request_id == 0) {
			continue;
		}
		TiledMeshParseResult result;
		if (!format_session->take_result(content.parse_request_id, result)) {
			continue;
		}
		content.parse_request_id = 0;
		content.source_bytes = 0;
		completed = true;
		if (result.generation != generation || result.owner != get_instance_id()) {
			content.state = TILE_STATE_CANCELLED;
			continue;
		}
		if (result.error != OK) {
			content.state = TILE_STATE_FAILED;
			if (content.owner_tile == root_tile) {
				_set_load_failed(result.uri, result.error_message);
			} else {
				emit_signal(SNAME("tile_failed"), result.uri, result.error_message);
			}
			continue;
		}
		content.pack_request_id = pack_queue ? pack_queue->enqueue(get_instance_id(), generation, result.document, content.request_priority) : 0;
		if (content.pack_request_id == 0) {
			content.state = TILE_STATE_FAILED;
			if (content.owner_tile == root_tile) {
				_set_load_failed(result.uri, "Unable to queue the CPU mesh packing request.");
			} else {
				emit_signal(SNAME("tile_failed"), result.uri, "Unable to queue the CPU mesh packing request.");
			}
			continue;
		}
		content.state = TILE_STATE_CPU_READY;
		root_state_changed = root_state_changed || content.owner_tile == root_tile;
	}
	if (completed) {
		_request_streaming_contents();
	}
	// Preserve the pre-framework signal behavior: child completion already has
	// tile_loaded/tile_failed, while rebuilding a full statistics dictionary for
	// every parse completion becomes quadratic as a large dynamic tree grows.
	if (root_state_changed) {
		emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
	}
}

void TiledMeshInstance3D::_poll_mesh_pack_results() {
	TiledMeshMeshPackQueue *pack_queue = TiledMeshMeshPackQueue::get_singleton();
	if (!pack_queue) {
		return;
	}
	bool root_state_changed = false;
	for (StreamingContent &content : streaming_contents) {
		if (content.pack_request_id == 0) {
			continue;
		}
		TiledMeshMeshPackQueue::Result result;
		if (!pack_queue->take_result(content.pack_request_id, result)) {
			continue;
		}
		content.pack_request_id = 0;
		if (result.generation != generation || result.owner != get_instance_id()) {
			content.state = TILE_STATE_CANCELLED;
			continue;
		}
		if (result.error != OK) {
			content.state = TILE_STATE_FAILED;
			if (content.owner_tile == root_tile) {
				_set_load_failed(content.descriptor.uri, result.error_message);
			} else {
				emit_signal(SNAME("tile_failed"), content.descriptor.uri, result.error_message);
			}
			continue;
		}
		content.packed_document = result.document;
		content.has_packed_document = true;
		content.state = TILE_STATE_CPU_READY;
		root_state_changed = root_state_changed || content.owner_tile == root_tile;
	}
	if (root_state_changed) {
		emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
	}
}

void TiledMeshInstance3D::_upload_ready_contents() {
	if (paused) {
		return;
	}
	const uint64_t started = Time::get_singleton()->get_ticks_usec();
	const uint64_t time_budget_usec = uint64_t(upload_budget_ms * 1000.0);
	const uint64_t byte_budget = 16 * 1024 * 1024;
	uint64_t uploaded_bytes = 0;
	Vector<int> ready;
	for (int i = 0; i < streaming_contents.size(); i++) {
		const StreamingContent &content = streaming_contents[i];
		if (content.state == TILE_STATE_CPU_READY && content.has_packed_document && content.owner_tile >= 0 && streaming_tiles[content.owner_tile].desired) {
			ready.push_back(i);
		}
	}
	bool render_state_changed = false;
	bool root_state_changed = false;
	while (!ready.is_empty()) {
		int best_position = 0;
		for (int i = 1; i < ready.size(); i++) {
			if (streaming_contents[ready[i]].request_priority > streaming_contents[ready[best_position]].request_priority) {
				best_position = i;
			}
		}
		const int content_index = ready[best_position];
		ready.remove_at(best_position);
		const StreamingContent &candidate = streaming_contents[content_index];
		if (uploaded_bytes > 0 && (uploaded_bytes + candidate.packed_document.packed_bytes > byte_budget || (time_budget_usec > 0 && Time::get_singleton()->get_ticks_usec() - started >= time_budget_usec))) {
			break;
		}

		// Registering a returned subtree can reallocate both runtime vectors.
		const TiledMeshPackedDocument packed_document = candidate.packed_document;
		const TiledMeshDocument &document = packed_document.document;
		const String content_uri = candidate.descriptor.uri;
		const int owner_tile = candidate.owner_tile;
		const bool root_document = owner_tile == root_tile;
		streaming_contents.write[content_index].state = TILE_STATE_UPLOADING;
		if (root_document) {
			source_to_local_basis = document.coordinate_frame.source_to_local;
			if (world_origin_mode == WORLD_ORIGIN_FORMAT_DEFAULT) {
				set_world_origin(document.source_origin.x, document.source_origin.y, document.source_origin.z);
			}
			dataset_aabb = document.local_aabb;
		}
		String error_message;
		const Error error = _upload_document(packed_document, error_message, owner_tile, content_index);
		if (error != OK) {
			StreamingContent &content = streaming_contents.write[content_index];
			content.has_packed_document = false;
			content.packed_document = TiledMeshPackedDocument();
			content.state = TILE_STATE_FAILED;
			if (root_document) {
				_set_load_failed(content_uri, error_message);
			} else {
				emit_signal(SNAME("tile_failed"), content_uri, error_message);
			}
			continue;
		}
		for (const String &warning : document.warnings) {
			WARN_PRINT(vformat("TiledMeshInstance3D: %s", warning));
		}
		const uint64_t document_bytes = packed_document.packed_bytes;
		StreamingContent &content = streaming_contents.write[content_index];
		content.has_packed_document = false;
		content.packed_document = TiledMeshPackedDocument();
		content.state = TILE_STATE_RESIDENT;
		content.resident_since_usec = Time::get_singleton()->get_ticks_usec();
		content.last_used_usec = content.resident_since_usec;
		_register_tiles(document, owner_tile);
		uploaded_bytes += document_bytes;
		render_state_changed = true;
		if (root_document) {
			update_gizmos();
			emit_signal(SNAME("root_loaded"));
			root_state_changed = true;
		}
		emit_signal(SNAME("tile_loaded"), content_uri);
	}
	if (render_state_changed) {
		_update_streaming_visibility();
		// Cancel work for render branches that were just replaced, but defer new
		// texture reads until the camera pass has admitted newly discovered
		// geometry content to the shared data queue.
		_update_texture_requests(!streaming_topology_dirty);
	}
	if (root_state_changed) {
		emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
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
	for (TileRenderInstance &render_tile : render_tiles) {
		if (!render_tile.instance.is_valid()) {
			continue;
		}
		const int owner = render_tile.owner_tile;
		const bool owner_desired = owner == root_tile || (owner >= 0 && owner < streaming_tiles.size() && streaming_tiles[owner].display_desired);
		const bool refined = render_tile.replacement_group >= 0 && _parent_group_is_fully_refined(owner, render_tile.replacement_group);
		const bool pending = render_tile.replacement_group >= 0 && _parent_group_has_display_refinement(owner, render_tile.replacement_group) && !refined;
		const bool visible = owner_desired && !refined && (_render_lod_matches(render_tile, display_camera) || pending);
		if (render_tile.visible != visible) {
			render_tile.visible = visible;
			if (rs) {
				rs->instance_set_visible(render_tile.instance, is_visible_in_tree() && visible);
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
	bool evicted = false;
	while (resident_bytes > budget) {
		int eviction = -1;
		uint64_t oldest = UINT64_MAX;
		for (int i = 0; i < streaming_contents.size(); i++) {
			const StreamingContent &content = streaming_contents[i];
			if (content.state != TILE_STATE_RESIDENT || content.owner_tile == root_tile || streaming_tiles[content.owner_tile].desired ||
					content.render_tiles.is_empty() || now - content.resident_since_usec < 2000000) {
				continue;
			}
			if (content.last_used_usec < oldest) {
				oldest = content.last_used_usec;
				eviction = i;
			}
		}
		if (eviction < 0) {
			break;
		}
		StreamingContent &content = streaming_contents.write[eviction];
		uint64_t content_bytes = 0;
		while (!content.render_tiles.is_empty()) {
			const int render_index = content.render_tiles[content.render_tiles.size() - 1];
			content_bytes += render_tiles[render_index].estimated_bytes + render_tiles[render_index].texture_bytes;
			_free_render_tile(render_index, false);
		}
		content.state = TILE_STATE_UNLOADED;
		resident_bytes = resident_bytes > content_bytes ? resident_bytes - content_bytes : 0;
		evicted = true;
	}
	if (evicted) {
		_recompute_resident_aabb();
		_update_streaming_visibility();
		_update_texture_requests();
	}
}

TiledMeshInstance3D::TileState TiledMeshInstance3D::_get_tile_state(int p_tile) const {
	if (p_tile < 0 || p_tile >= streaming_tiles.size()) {
		return TILE_STATE_UNLOADED;
	}
	TileState state = streaming_tiles[p_tile].contents.is_empty() ? TILE_STATE_RESIDENT : TILE_STATE_UNLOADED;
	for (int content_index : streaming_tiles[p_tile].contents) {
		const TileState content_state = streaming_contents[content_index].state;
		if (content_state == TILE_STATE_FAILED) {
			return TILE_STATE_FAILED;
		}
		if (content_state < TILE_STATE_RESIDENT && content_state > state) {
			state = content_state;
		} else if (content_state == TILE_STATE_RESIDENT && (state == TILE_STATE_UNLOADED || state == TILE_STATE_RESIDENT)) {
			state = TILE_STATE_RESIDENT;
		}
	}
	return state;
}

String TiledMeshInstance3D::_tile_state_name(TileState p_state) {
	switch (p_state) {
		case TILE_STATE_UNLOADED:
			return "unloaded";
		case TILE_STATE_REQUESTED:
			return "requested";
		case TILE_STATE_DOWNLOADED:
			return "downloaded";
		case TILE_STATE_PARSING:
			return "parsing";
		case TILE_STATE_CPU_READY:
			return "cpu_ready";
		case TILE_STATE_UPLOADING:
			return "uploading";
		case TILE_STATE_RESIDENT:
			return "resident";
		case TILE_STATE_FAILED:
			return "failed";
		case TILE_STATE_CANCELLED:
			return "cancelled";
	}
	return "unknown";
}

void TiledMeshInstance3D::_set_load_failed(const String &p_uri, const String &p_message) {
	last_root_state = TILE_STATE_FAILED;
	if (root_content >= 0 && root_content < streaming_contents.size()) {
		streaming_contents.write[root_content].state = TILE_STATE_FAILED;
	}
	open_state = false;
	emit_signal(SNAME("load_failed"), p_uri, p_message);
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
			if (RenderingServer *rs = RenderingServer::get_singleton()) {
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
			// Apply camera changes before consuming completions so stale, offscreen
			// pack results are not uploaded and waiting work is reprioritized first.
			if (!paused && open_state && !Engine::get_singleton()->is_editor_hint()) {
				_update_streaming(_resolve_runtime_camera());
			}
			_poll_data_results();
			_poll_texture_results();
			_poll_parse_results();
			_poll_mesh_pack_results();
			_upload_ready_contents();
			// Uploading a document may discover a dynamic subtree. Run a second,
			// normally no-op selection pass only when that topology became dirty.
			if (!paused && open_state && streaming_topology_dirty && !Engine::get_singleton()->is_editor_hint()) {
				_update_streaming(_resolve_runtime_camera());
			}
			if (get_layer_mask() != last_layer_mask || is_visible_in_tree() != last_visible) {
				_sync_render_instances();
			}
		} break;
	}
}

void TiledMeshInstance3D::set_source_uri(const String &p_uri) {
	if (source_uri == p_uri) {
		return;
	}
	const bool reopen = open_state;
	close();
	source_uri = p_uri;
	update_configuration_warnings();
	if (reopen || (is_inside_tree() && auto_load && (!Engine::get_singleton()->is_editor_hint() || editor_preview_enabled))) {
		call_deferred(SNAME("open"));
	}
}

String TiledMeshInstance3D::get_source_uri() const {
	return source_uri;
}

void TiledMeshInstance3D::set_format_id(const StringName &p_format_id) {
	if (format_id == p_format_id) {
		return;
	}
	const bool reopen = open_state;
	close();
	format_id = p_format_id;
	notify_property_list_changed();
	if (reopen) {
		call_deferred(SNAME("open"));
	}
}

StringName TiledMeshInstance3D::get_format_id() const {
	return format_id;
}
StringName TiledMeshInstance3D::get_active_format_id() const {
	return active_format_id;
}

void TiledMeshInstance3D::set_format_options(const Ref<TiledMeshFormatOptions> &p_options) {
	if (format_options == p_options) {
		return;
	}
	const bool reopen = open_state;
	close();
	if (format_options.is_valid()) {
		format_options->disconnect_changed(callable_mp(this, &TiledMeshInstance3D::_format_options_changed));
	}
	format_options = p_options;
	if (format_options.is_valid()) {
		format_options->connect_changed(callable_mp(this, &TiledMeshInstance3D::_format_options_changed));
	}
	update_configuration_warnings();
	if (reopen || (is_inside_tree() && auto_load && (!Engine::get_singleton()->is_editor_hint() || editor_preview_enabled))) {
		call_deferred(SNAME("open"));
	}
}

void TiledMeshInstance3D::_format_options_changed() {
	update_configuration_warnings();
	if (open_state || (is_inside_tree() && auto_load && (!Engine::get_singleton()->is_editor_hint() || editor_preview_enabled))) {
		close();
		call_deferred(SNAME("open"));
	}
}

Ref<TiledMeshFormatOptions> TiledMeshInstance3D::get_format_options() const {
	return format_options;
}
void TiledMeshInstance3D::set_auto_load(bool p_enabled) {
	auto_load = p_enabled;
}
bool TiledMeshInstance3D::is_auto_load_enabled() const {
	return auto_load;
}
void TiledMeshInstance3D::set_camera_path(const NodePath &p_path) {
	camera_path = p_path;
}
NodePath TiledMeshInstance3D::get_camera_path() const {
	return camera_path;
}

void TiledMeshInstance3D::set_unit_scale(double p_scale) {
	p_scale = MAX(p_scale, 0.000001);
	if (Math::is_equal_approx(unit_scale, p_scale)) {
		return;
	}
	unit_scale = p_scale;
	if (open_state) {
		reload();
	}
}

double TiledMeshInstance3D::get_unit_scale() const {
	return unit_scale;
}

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

TiledMeshInstance3D::WorldOriginMode TiledMeshInstance3D::get_world_origin_mode() const {
	return world_origin_mode;
}

void TiledMeshInstance3D::set_world_origin(double p_x, double p_y, double p_z) {
	if (world_origin_x == p_x && world_origin_y == p_y && world_origin_z == p_z) {
		return;
	}
	world_origin_x = p_x;
	world_origin_y = p_y;
	world_origin_z = p_z;
	streaming_topology_dirty = true;
	_recompute_resident_aabb();
	_sync_render_instances();
	PackedFloat64Array origin;
	origin.resize(3);
	origin.set(0, p_x);
	origin.set(1, p_y);
	origin.set(2, p_z);
	emit_signal(SNAME("world_origin_changed"), origin);
}

double TiledMeshInstance3D::get_world_origin_x() const {
	return world_origin_x;
}
double TiledMeshInstance3D::get_world_origin_y() const {
	return world_origin_y;
}
double TiledMeshInstance3D::get_world_origin_z() const {
	return world_origin_z;
}
void TiledMeshInstance3D::set_world_origin_x(double p_value) {
	set_world_origin(p_value, world_origin_y, world_origin_z);
}
void TiledMeshInstance3D::set_world_origin_y(double p_value) {
	set_world_origin(world_origin_x, p_value, world_origin_z);
}
void TiledMeshInstance3D::set_world_origin_z(double p_value) {
	set_world_origin(world_origin_x, world_origin_y, p_value);
}

void TiledMeshInstance3D::set_lod_bias(double p_value) {
	lod_bias = MAX(p_value, 0.01);
	streaming_topology_dirty = true;
}
double TiledMeshInstance3D::get_lod_bias() const {
	return lod_bias;
}
void TiledMeshInstance3D::set_lod_hysteresis(double p_value) {
	lod_hysteresis = CLAMP(p_value, 0.0, 1.0);
	streaming_topology_dirty = true;
}
double TiledMeshInstance3D::get_lod_hysteresis() const {
	return lod_hysteresis;
}
void TiledMeshInstance3D::set_maximum_screen_space_error(double p_value) {
	maximum_screen_space_error = MAX(p_value, 0.01);
	streaming_topology_dirty = true;
}
double TiledMeshInstance3D::get_maximum_screen_space_error() const {
	return maximum_screen_space_error;
}
void TiledMeshInstance3D::set_memory_budget_mb(int p_value) {
	memory_budget_mb = MAX(p_value, 0);
	streaming_topology_dirty = true;
}
int TiledMeshInstance3D::get_memory_budget_mb() const {
	return memory_budget_mb;
}
void TiledMeshInstance3D::set_max_concurrent_requests(int p_value) {
	max_concurrent_requests = CLAMP(p_value, 0, 64);
	streaming_topology_dirty = true;
}
int TiledMeshInstance3D::get_max_concurrent_requests() const {
	return max_concurrent_requests;
}
void TiledMeshInstance3D::set_max_tile_size_mb(int p_value) {
	max_tile_size_mb = MAX(p_value, 1);
}
int TiledMeshInstance3D::get_max_tile_size_mb() const {
	return max_tile_size_mb;
}
void TiledMeshInstance3D::set_upload_budget_ms(double p_value) {
	upload_budget_ms = MAX(p_value, 0.0);
}
double TiledMeshInstance3D::get_upload_budget_ms() const {
	return upload_budget_ms;
}
void TiledMeshInstance3D::set_request_timeout_sec(double p_value) {
	request_timeout_sec = MAX(p_value, 0.1);
}
double TiledMeshInstance3D::get_request_timeout_sec() const {
	return request_timeout_sec;
}
void TiledMeshInstance3D::set_retry_count(int p_value) {
	retry_count = MAX(p_value, 0);
}
int TiledMeshInstance3D::get_retry_count() const {
	return retry_count;
}

void TiledMeshInstance3D::set_cast_shadow(GeometryInstance3D::ShadowCastingSetting p_setting) {
	ERR_FAIL_COND(p_setting < GeometryInstance3D::SHADOW_CASTING_SETTING_OFF || p_setting > GeometryInstance3D::SHADOW_CASTING_SETTING_SHADOWS_ONLY);
	cast_shadow = p_setting;
	_sync_render_instances();
}

GeometryInstance3D::ShadowCastingSetting TiledMeshInstance3D::get_cast_shadow() const {
	return cast_shadow;
}

void TiledMeshInstance3D::set_receive_lighting_mode(int p_mode) {
	receive_lighting_mode = CLAMP(p_mode, -1, 1);
	for (TileRenderInstance &tile : render_tiles) {
		for (Ref<Material> &material : tile.materials) {
			Ref<StandardMaterial3D> standard = material;
			if (standard.is_valid()) {
				bool authored_unlit = standard->has_meta("authored_unlit") && bool(standard->get_meta("authored_unlit"));
				bool unlit = receive_lighting_mode < 0 ? authored_unlit : receive_lighting_mode == 0;
				standard->set_shading_mode(unlit ? BaseMaterial3D::SHADING_MODE_UNSHADED : BaseMaterial3D::SHADING_MODE_PER_PIXEL);
			}
		}
	}
}

int TiledMeshInstance3D::get_receive_lighting_mode() const {
	return receive_lighting_mode;
}
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
bool TiledMeshInstance3D::is_editor_preview_enabled() const {
	return editor_preview_enabled;
}
void TiledMeshInstance3D::set_editor_preview_lod_bias(double p_value) {
	editor_preview_lod_bias = MAX(p_value, 0.01);
	streaming_topology_dirty = true;
}
double TiledMeshInstance3D::get_editor_preview_lod_bias() const {
	return editor_preview_lod_bias;
}
void TiledMeshInstance3D::set_editor_preview_memory_mb(int p_value) {
	editor_preview_memory_mb = MAX(p_value, 1);
	streaming_topology_dirty = true;
}
int TiledMeshInstance3D::get_editor_preview_memory_mb() const {
	return editor_preview_memory_mb;
}
void TiledMeshInstance3D::set_editor_preview_max_requests(int p_value) {
	editor_preview_max_requests = MAX(p_value, 1);
	streaming_topology_dirty = true;
}
int TiledMeshInstance3D::get_editor_preview_max_requests() const {
	return editor_preview_max_requests;
}
void TiledMeshInstance3D::set_editor_preview_show_bounds(bool p_enabled) {
	editor_preview_show_bounds = p_enabled;
	update_gizmos();
}
bool TiledMeshInstance3D::is_editor_preview_showing_bounds() const {
	return editor_preview_show_bounds;
}

PackedStringArray TiledMeshInstance3D::get_configuration_warnings() const {
	PackedStringArray warnings = VisualInstance3D::get_configuration_warnings();
	const String resolved_source = format_options.is_valid() ? format_options->get_source_uri(source_uri) : source_uri;
	if (resolved_source.is_empty()) {
		warnings.push_back(RTR("Set Source URI or complete the source configuration in Format Options (for Cesium ion, both Ion Asset ID and Ion Access Token are required)."));
	}
	return warnings;
}

Error TiledMeshInstance3D::open() {
	const String resolved_source = format_options.is_valid() ? format_options->get_source_uri(source_uri) : source_uri;
	if (resolved_source.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (open_state) {
		return OK;
	}
	generation++;
	_clear_streaming();
	_clear_render_tiles();
	dataset_aabb = AABB();
	paused = false;
	source_to_local_basis = Basis();

	StreamingTile root;
	root.provider_key = "__root__";
	root.selection.mode = TiledMeshSelectionRule::MODE_ALWAYS;
	root.desired = true;
	root.display_desired = true;
	streaming_tiles.push_back(root);
	root_tile = 0;
	streaming_tile_by_key.insert(root.provider_key, root_tile);
	TiledMeshContentRequest root_descriptor;
	root_descriptor.uri = TiledMeshDataSource::normalize_uri(resolved_source);
	root_descriptor.content_type = SNAME("root");
	root_content = _add_content(root_tile, root_descriptor);
	last_root_state = TILE_STATE_REQUESTED;
	streaming_contents.write[root_content].state = TILE_STATE_REQUESTED;
	streaming_contents.write[root_content].request_priority = 1.0e15;

	TiledMeshDataRequestQueue *data_queue = TiledMeshDataRequestQueue::get_singleton();
	if (!data_queue) {
		streaming_contents.write[root_content].state = TILE_STATE_FAILED;
		_set_load_failed(source_uri, "The tiled mesh data service is not available.");
		return ERR_UNAVAILABLE;
	}
	root_data_request_id = data_queue->request(get_instance_id(), generation, root_descriptor.uri, uint64_t(max_tile_size_mb) * 1024 * 1024,
			request_timeout_sec, retry_count, 1.0e15, format_options.is_valid() ? format_options->get_source_bearer_token() : String());
	if (root_data_request_id == 0) {
		streaming_contents.write[root_content].state = TILE_STATE_FAILED;
		_set_load_failed(source_uri, "Unable to queue the root tiled mesh data request.");
		return ERR_CANT_CREATE;
	}
	open_state = true;
	streaming_topology_dirty = true;
	emit_signal(SNAME("streaming_stats_changed"), get_streaming_stats());
	return OK;
}

void TiledMeshInstance3D::close() {
	generation++;
	if (root_tile >= 0) {
		last_root_state = TILE_STATE_CANCELLED;
	}
	open_state = false;
	paused = false;
	_clear_streaming();
	_clear_render_tiles();
	dataset_aabb = AABB();
	update_gizmos();
}

Error TiledMeshInstance3D::reload() {
	close();
	return open();
}

bool TiledMeshInstance3D::is_open() const {
	return open_state;
}
void TiledMeshInstance3D::set_streaming_paused(bool p_paused) {
	paused = p_paused;
	if (!paused) {
		streaming_topology_dirty = true;
	}
}
bool TiledMeshInstance3D::is_streaming_paused() const {
	return paused;
}

Vector3 TiledMeshInstance3D::world_to_local(double p_x, double p_y, double p_z) const {
	const Vector3 difference((p_x - world_origin_x) * unit_scale, (p_y - world_origin_y) * unit_scale, (p_z - world_origin_z) * unit_scale);
	return source_to_local_basis.xform(difference);
}

PackedFloat64Array TiledMeshInstance3D::local_to_world(const Vector3 &p_local_position) const {
	const Vector3 source = source_to_local_basis.inverse().xform(p_local_position) / unit_scale;
	PackedFloat64Array result;
	result.resize(3);
	result.set(0, source.x + world_origin_x);
	result.set(1, source.y + world_origin_y);
	result.set(2, source.z + world_origin_z);
	return result;
}

Dictionary TiledMeshInstance3D::intersect_ray(const Vector3 &p_origin, const Vector3 &p_direction, double p_max_distance, bool p_visible_only, bool p_hit_back_faces) const {
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), Dictionary(), "TiledMeshInstance3D.intersect_ray() must be called on the main thread.");
	ERR_FAIL_COND_V_MSG(!p_origin.is_finite() || !p_direction.is_finite() || p_direction.is_zero_approx(), Dictionary(), "The ray origin and direction must be finite, and direction must be non-zero.");
	ERR_FAIL_COND_V_MSG(Math::is_nan(p_max_distance) || p_max_distance <= 0.0, Dictionary(), "max_distance must be positive.");
	if (p_visible_only && !is_visible_in_tree()) {
		return Dictionary();
	}
	const Vector3 direction = p_direction.normalized();
	double closest_distance = p_max_distance;
	Dictionary closest;
	uint64_t triangle_count = 0;
	for (const TileRenderInstance &tile : render_tiles) {
		if (!tile.instance.is_valid() || (p_visible_only && !tile.visible)) {
			continue;
		}
		const Transform3D transform = _get_tile_world_transform(tile);
		if (!transform.xform(tile.aabb).intersects_ray(p_origin, direction)) {
			continue;
		}
		for (int surface_index = 0; surface_index < tile.pick_surfaces.size(); surface_index++) {
			const TiledMeshPickSurface &surface = tile.pick_surfaces[surface_index];
			if (surface.points) {
				for (int point_index = 0; point_index < surface.vertices.size(); point_index++) {
					if (++triangle_count > 10000000) {
						ERR_PRINT("TiledMeshInstance3D.intersect_ray() exceeded the 10 million point safety limit.");
						return Dictionary();
					}
					const Vector3 point = transform.xform(surface.vertices[point_index]);
					const double distance_along_ray = (point - p_origin).dot(direction);
					if (distance_along_ray < 0.0 || distance_along_ray > closest_distance) {
						continue;
					}
					const Vector3 projected = p_origin + direction * distance_along_ray;
					const double tolerance = 0.05;
					if (point.distance_squared_to(projected) > tolerance * tolerance) {
						continue;
					}
					const int batch_id = surface.batch_ids.size() == surface.vertices.size() ? surface.batch_ids[point_index] : -1;
					Dictionary properties;
					if (batch_id >= 0 && surface.batch_table >= 0 && surface.batch_table < tile.batch_tables.size()) {
						const TiledMeshBatchTablePayload &table = tile.batch_tables[surface.batch_table];
						if (batch_id < table.batch_length) {
							for (const Variant &key : table.properties.keys()) {
								const Array values = table.properties[key];
								if (batch_id < values.size()) {
									properties[key] = values[batch_id];
								}
							}
						}
					}
					closest_distance = distance_along_ray;
					closest["position"] = point;
					closest["normal"] = -direction;
					closest["distance"] = distance_along_ray;
					closest["primitive_index"] = surface_index;
					closest["point_index"] = point_index;
					closest["tile_index"] = tile.owner_tile;
					closest["content_index"] = tile.owner_content;
					closest["tile_id"] = tile.owner_tile >= 0 && tile.owner_tile < streaming_tiles.size() ? streaming_tiles[tile.owner_tile].provider_key : String();
					closest["content_uri"] = tile.owner_content >= 0 && tile.owner_content < streaming_contents.size() ? streaming_contents[tile.owner_content].descriptor.uri : String();
					closest["batch_id"] = batch_id;
					closest["properties"] = properties;
				}
				continue;
			}
			for (int index = 0; index + 2 < surface.indices.size(); index += 3) {
				if (++triangle_count > 10000000) {
					ERR_PRINT("TiledMeshInstance3D.intersect_ray() exceeded the 10 million triangle safety limit.");
					return Dictionary();
				}
				const int ia = surface.indices[index];
				const int ib = surface.indices[index + 1];
				const int ic = surface.indices[index + 2];
				if (ia < 0 || ib < 0 || ic < 0 || ia >= surface.vertices.size() || ib >= surface.vertices.size() || ic >= surface.vertices.size()) {
					continue;
				}
				const Vector3 a = transform.xform(surface.vertices[ia]);
				const Vector3 b = transform.xform(surface.vertices[ib]);
				const Vector3 c = transform.xform(surface.vertices[ic]);
				Vector3 normal = (c - a).cross(b - a);
				if (normal.length_squared() <= CMP_EPSILON2 || (!p_hit_back_faces && normal.dot(direction) >= 0.0f)) {
					continue;
				}
				Vector3 position;
				if (!Geometry3D::ray_intersects_triangle(p_origin, direction, a, b, c, &position)) {
					continue;
				}
				const double distance = p_origin.distance_to(position);
				if (distance > closest_distance) {
					continue;
				}
				normal.normalize();
				const int batch_id = surface.batch_ids.size() == surface.vertices.size() ? surface.batch_ids[ia] : -1;
				Dictionary properties;
				if (batch_id >= 0 && surface.batch_table >= 0 && surface.batch_table < tile.batch_tables.size()) {
					const TiledMeshBatchTablePayload &table = tile.batch_tables[surface.batch_table];
					if (batch_id < table.batch_length) {
						for (const Variant &key : table.properties.keys()) {
							const Array values = table.properties[key];
							if (batch_id < values.size()) {
								properties[key] = values[batch_id];
							}
						}
					}
				}
				closest_distance = distance;
				closest["position"] = position;
				closest["normal"] = normal;
				closest["distance"] = distance;
				closest["primitive_index"] = surface_index;
				closest["triangle_index"] = index / 3;
				closest["tile_index"] = tile.owner_tile;
				closest["content_index"] = tile.owner_content;
				closest["tile_id"] = tile.owner_tile >= 0 && tile.owner_tile < streaming_tiles.size() ? streaming_tiles[tile.owner_tile].provider_key : String();
				closest["content_uri"] = tile.owner_content >= 0 && tile.owner_content < streaming_contents.size() ? streaming_contents[tile.owner_content].descriptor.uri : String();
				closest["batch_id"] = batch_id;
				closest["properties"] = properties;
			}
		}
	}
	return closest;
}

Dictionary TiledMeshInstance3D::intersect_screen_point(Camera3D *p_camera, const Vector2 &p_screen_point, double p_max_distance, bool p_visible_only, bool p_hit_back_faces) const {
	ERR_FAIL_NULL_V_MSG(p_camera, Dictionary(), "A Camera3D is required to intersect a screen point.");
	ERR_FAIL_COND_V_MSG(!p_screen_point.is_finite(), Dictionary(), "The screen point must be finite.");
	return intersect_ray(p_camera->project_ray_origin(p_screen_point), p_camera->project_ray_normal(p_screen_point), p_max_distance, p_visible_only, p_hit_back_faces);
}

Dictionary TiledMeshInstance3D::get_streaming_stats() const {
	Dictionary stats;
	stats["open"] = open_state;
	stats["paused"] = paused;
	stats["generation"] = generation;
	stats["active_format_id"] = active_format_id;
	stats["logical_tiles"] = streaming_tiles.size();
	stats["contents"] = streaming_contents.size();
	stats["root_state"] = _tile_state_name(root_tile >= 0 ? _get_tile_state(root_tile) : last_root_state);
	int requested = 0;
	int waiting = 0;
	int packing = 0;
	int cpu_ready = 0;
	int resident = 0;
	int failed = 0;
	uint64_t source_bytes = 0;
	uint64_t cpu_ready_bytes = 0;
	for (const StreamingContent &content : streaming_contents) {
		waiting += content.owner_tile >= 0 && streaming_tiles[content.owner_tile].desired &&
						(content.state == TILE_STATE_UNLOADED || content.state == TILE_STATE_CANCELLED)
				? 1
				: 0;
		requested += content.state == TILE_STATE_REQUESTED || content.state == TILE_STATE_DOWNLOADED || content.state == TILE_STATE_PARSING ? 1 : 0;
		packing += content.pack_request_id != 0 ? 1 : 0;
		cpu_ready += content.has_packed_document ? 1 : 0;
		resident += content.state == TILE_STATE_RESIDENT ? 1 : 0;
		failed += content.state == TILE_STATE_FAILED ? 1 : 0;
		source_bytes += content.source_bytes;
		cpu_ready_bytes += content.has_packed_document ? content.packed_document.packed_bytes : 0;
	}
	stats["requested_contents"] = requested;
	stats["waiting_contents"] = waiting;
	stats["packing_contents"] = packing;
	stats["packed_ready_contents"] = cpu_ready;
	stats["cpu_ready_contents"] = cpu_ready;
	stats["resident_contents"] = resident;
	stats["failed_contents"] = failed;
	// Compatibility aliases retained for diagnostics that consumed the old keys.
	stats["known_tiles"] = streaming_tiles.size();
	stats["requested_tiles"] = requested;
	stats["cpu_ready_tiles"] = cpu_ready;
	stats["resident_tiles"] = resident;
	stats["failed_tiles"] = failed;
	stats["source_bytes"] = source_bytes;
	stats["packed_ready_bytes"] = cpu_ready_bytes;
	stats["cpu_ready_bytes"] = cpu_ready_bytes;
	if (TiledMeshMeshPackQueue *pack_queue = TiledMeshMeshPackQueue::get_singleton()) {
		stats["mesh_pack_queue_pending"] = pack_queue->get_pending_count();
		stats["mesh_pack_queue_active"] = pack_queue->get_active_count();
		stats["mesh_pack_in_flight_bytes"] = pack_queue->get_in_flight_bytes();
	} else {
		stats["mesh_pack_queue_pending"] = 0;
		stats["mesh_pack_queue_active"] = 0;
		stats["mesh_pack_in_flight_bytes"] = uint64_t(0);
	}
	uint64_t estimated_bytes = 0;
	int resident_instances = 0;
	int visible_instances = 0;
	int resident_materials = 0;
	int unlit_materials = 0;
	int textured_materials = 0;
	int inline_lod_instances = 0;
	int visible_inline_lod_instances = 0;
	HashSet<int> visible_owners;
	HashSet<int> metadata_contents;
	int resident_batches = 0;
	int resident_metadata_properties = 0;
	for (const TileRenderInstance &tile : render_tiles) {
		if (!tile.instance.is_valid()) {
			continue;
		}
		resident_instances++;
		if (!tile.batch_tables.is_empty() && !metadata_contents.has(tile.owner_content)) {
			metadata_contents.insert(tile.owner_content);
			for (const TiledMeshBatchTablePayload &table : tile.batch_tables) {
				resident_batches += table.batch_length;
				resident_metadata_properties += table.properties.size();
			}
		}
		// 读取实际材质资源，便于无窗口环境验证材质与纹理已完成上传。
		for (const Ref<StandardMaterial3D> &material : tile.materials) {
			resident_materials++;
			unlit_materials += material->get_shading_mode() == BaseMaterial3D::SHADING_MODE_UNSHADED ? 1 : 0;
			textured_materials += material->get_texture(BaseMaterial3D::TEXTURE_ALBEDO).is_valid() ? 1 : 0;
		}
		estimated_bytes += tile.estimated_bytes + tile.texture_bytes;
		if (!tile.lod_ranges.is_empty()) {
			inline_lod_instances++;
			visible_inline_lod_instances += tile.visible ? 1 : 0;
		}
		if (tile.visible) {
			visible_instances++;
			visible_owners.insert(tile.owner_tile);
		}
	}
	stats["estimated_resident_bytes"] = estimated_bytes;
	stats["resident_render_instances"] = resident_instances;
	stats["resident_materials"] = resident_materials;
	stats["resident_unlit_materials"] = unlit_materials;
	stats["resident_textured_materials"] = textured_materials;
	stats["visible_render_instances"] = visible_instances;
	stats["inline_lod_render_instances"] = inline_lod_instances;
	stats["visible_inline_lod_render_instances"] = visible_inline_lod_instances;
	stats["visible_tiles"] = visible_owners.size();
	stats["resident_metadata_contents"] = metadata_contents.size();
	stats["resident_metadata_batches"] = resident_batches;
	stats["resident_metadata_properties"] = resident_metadata_properties;
	stats["texture_requests"] = texture_requests.size();
	int active_texture_requests = 0;
	int paused_texture_requests = 0;
	int decoded_texture_requests = 0;
	for (const TextureRequest &request : texture_requests) {
		active_texture_requests += request.data_request_id != 0 || request.decode_request_id != 0 ? 1 : 0;
		paused_texture_requests += !_texture_request_is_desired(request) ? 1 : 0;
		decoded_texture_requests += request.decoded_image.is_valid() ? 1 : 0;
	}
	stats["active_texture_requests"] = active_texture_requests;
	stats["paused_texture_requests"] = paused_texture_requests;
	stats["decoded_texture_requests"] = decoded_texture_requests;
	int replacement_surfaces = 0;
	int hidden_replacement_surfaces = 0;
	for (const TileRenderInstance &tile : render_tiles) {
		if (!tile.instance.is_valid() || tile.replacement_group < 0) {
			continue;
		}
		replacement_surfaces++;
		hidden_replacement_surfaces += _parent_group_is_fully_refined(tile.owner_tile, tile.replacement_group) ? 1 : 0;
	}
	stats["replacement_surfaces"] = replacement_surfaces;
	stats["hidden_replacement_surfaces"] = hidden_replacement_surfaces;
	stats["fallback_surfaces"] = replacement_surfaces;
	stats["hidden_fallback_surfaces"] = hidden_replacement_surfaces;
	return stats;
}

AABB TiledMeshInstance3D::get_dataset_aabb() const {
	return dataset_aabb;
}
AABB TiledMeshInstance3D::get_resident_aabb() const {
	return resident_aabb;
}
AABB TiledMeshInstance3D::get_aabb() const {
	return dataset_aabb;
}

#ifdef TOOLS_ENABLED
void TiledMeshInstance3D::editor_update_cameras(const Vector<Camera3D *> &p_cameras, Camera3D *p_active_camera) {
	if (!editor_preview_enabled || paused || !open_state || p_cameras.is_empty()) {
		return;
	}
	_update_streaming_from_cameras(p_cameras, p_active_camera);
}
#endif

void TiledMeshInstance3D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "source_uri") {
		const Vector<String> extensions = TiledMeshFormatRegistry::get_recognized_extensions();
		Vector<String> filters;
		for (const String &extension : extensions) {
			filters.push_back("*." + extension);
		}
		p_property.hint_string = String(",").join(filters);
	}
}

void TiledMeshInstance3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source_uri", "uri"), &TiledMeshInstance3D::set_source_uri);
	ClassDB::bind_method(D_METHOD("get_source_uri"), &TiledMeshInstance3D::get_source_uri);
	ClassDB::bind_method(D_METHOD("set_format_id", "format_id"), &TiledMeshInstance3D::set_format_id);
	ClassDB::bind_method(D_METHOD("get_format_id"), &TiledMeshInstance3D::get_format_id);
	ClassDB::bind_method(D_METHOD("get_active_format_id"), &TiledMeshInstance3D::get_active_format_id);
	ClassDB::bind_method(D_METHOD("set_format_options", "options"), &TiledMeshInstance3D::set_format_options);
	ClassDB::bind_method(D_METHOD("get_format_options"), &TiledMeshInstance3D::get_format_options);
	ClassDB::bind_method(D_METHOD("set_auto_load", "enabled"), &TiledMeshInstance3D::set_auto_load);
	ClassDB::bind_method(D_METHOD("is_auto_load_enabled"), &TiledMeshInstance3D::is_auto_load_enabled);
	ClassDB::bind_method(D_METHOD("set_camera_path", "path"), &TiledMeshInstance3D::set_camera_path);
	ClassDB::bind_method(D_METHOD("get_camera_path"), &TiledMeshInstance3D::get_camera_path);
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
	ClassDB::bind_method(D_METHOD("set_maximum_screen_space_error", "value"), &TiledMeshInstance3D::set_maximum_screen_space_error);
	ClassDB::bind_method(D_METHOD("get_maximum_screen_space_error"), &TiledMeshInstance3D::get_maximum_screen_space_error);
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
	ClassDB::bind_method(D_METHOD("set_receive_lighting_mode", "mode"), &TiledMeshInstance3D::set_receive_lighting_mode);
	ClassDB::bind_method(D_METHOD("get_receive_lighting_mode"), &TiledMeshInstance3D::get_receive_lighting_mode);
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
	ClassDB::bind_method(D_METHOD("intersect_ray", "origin", "direction", "max_distance", "visible_only", "hit_back_faces"), &TiledMeshInstance3D::intersect_ray, DEFVAL(Math::INF), DEFVAL(true), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("intersect_screen_point", "camera", "screen_point", "max_distance", "visible_only", "hit_back_faces"), &TiledMeshInstance3D::intersect_screen_point, DEFVAL(Math::INF), DEFVAL(true), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_streaming_stats"), &TiledMeshInstance3D::get_streaming_stats);
	ClassDB::bind_method(D_METHOD("get_dataset_aabb"), &TiledMeshInstance3D::get_dataset_aabb);
	ClassDB::bind_method(D_METHOD("get_resident_aabb"), &TiledMeshInstance3D::get_resident_aabb);

	ADD_GROUP("Data", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_uri", PROPERTY_HINT_GLOBAL_FILE), "set_source_uri", "get_source_uri");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "format_id"), "set_format_id", "get_format_id");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "format_options", PROPERTY_HINT_RESOURCE_TYPE, TiledMeshFormatOptions::get_class_static()), "set_format_options", "get_format_options");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "active_format_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_active_format_id");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_load"), "set_auto_load", "is_auto_load_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "camera_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Camera3D"), "set_camera_path", "get_camera_path");
	ADD_GROUP("Coordinates", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_scale", PROPERTY_HINT_RANGE, "0.000001,1000000,0.001,or_greater"), "set_unit_scale", "get_unit_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "world_origin_mode", PROPERTY_HINT_ENUM, "Format Default,Explicit"), "set_world_origin_mode", "get_world_origin_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_origin_x"), "set_world_origin_x", "get_world_origin_x");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_origin_y"), "set_world_origin_y", "get_world_origin_y");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_origin_z"), "set_world_origin_z", "get_world_origin_z");
	ADD_GROUP("Streaming", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_bias", PROPERTY_HINT_RANGE, "0.01,16,0.01,or_greater"), "set_lod_bias", "get_lod_bias");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_hysteresis", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_lod_hysteresis", "get_lod_hysteresis");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "maximum_screen_space_error", PROPERTY_HINT_RANGE, "0.01,1024,0.01,or_greater"), "set_maximum_screen_space_error", "get_maximum_screen_space_error");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "memory_budget_mb", PROPERTY_HINT_RANGE, "0,65536,1,or_greater"), "set_memory_budget_mb", "get_memory_budget_mb");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_concurrent_requests", PROPERTY_HINT_RANGE, "0,64,1"), "set_max_concurrent_requests", "get_max_concurrent_requests");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_tile_size_mb", PROPERTY_HINT_RANGE, "1,2048,1,or_greater"), "set_max_tile_size_mb", "get_max_tile_size_mb");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "upload_budget_ms", PROPERTY_HINT_RANGE, "0,50,0.1"), "set_upload_budget_ms", "get_upload_budget_ms");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "request_timeout_sec", PROPERTY_HINT_RANGE, "0.1,600,0.1"), "set_request_timeout_sec", "get_request_timeout_sec");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "retry_count", PROPERTY_HINT_RANGE, "0,10,1"), "set_retry_count", "get_retry_count");
	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadow", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"), "set_cast_shadow", "get_cast_shadow");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "receive_lighting_mode", PROPERTY_HINT_ENUM, "Source Default,Receive Lighting,Unlit"), "set_receive_lighting_mode", "get_receive_lighting_mode");
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
	BIND_ENUM_CONSTANT(WORLD_ORIGIN_FORMAT_DEFAULT);
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
