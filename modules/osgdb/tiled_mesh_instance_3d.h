/**************************************************************************/
/*  tiled_mesh_instance_3d.h                                              */
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

#include "osg_tile_document.h"

#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/material.h"

#include "core/math/projection.h"
#include "core/templates/hash_map.h"

class Camera3D;

class TiledMeshInstance3D : public VisualInstance3D {
	GDCLASS(TiledMeshInstance3D, VisualInstance3D);

public:
	enum AxisMode {
		AXIS_MODE_OSG_Z_UP,
		AXIS_MODE_GODOT_Y_UP,
	};

	enum WorldOriginMode {
		WORLD_ORIGIN_ROOT_CENTER,
		WORLD_ORIGIN_EXPLICIT,
	};

	enum TileState {
		TILE_STATE_UNLOADED,
		TILE_STATE_REQUESTED,
		TILE_STATE_DOWNLOADED,
		TILE_STATE_PARSING,
		TILE_STATE_CPU_READY,
		TILE_STATE_UPLOADING,
		TILE_STATE_RESIDENT,
		TILE_STATE_FAILED,
		TILE_STATE_CANCELLED,
	};

private:
	struct TileRenderInstance {
		RID mesh;
		RID instance;
		Vector<Ref<Material>> materials;
		int owner_tile = -2; // -1 is the root document.
		int fallback_group = -1;
		int lod_branch = -1;
		Vector<OsgLodRangePayload> lod_ranges;
		double source_center_x = 0.0;
		double source_center_y = 0.0;
		double source_center_z = 0.0;
		AABB aabb;
		uint64_t estimated_bytes = 0;
		uint64_t texture_bytes = 0;
		bool visible = true;
	};

	struct TextureRequest {
		uint64_t data_request_id = 0;
		uint64_t decode_request_id = 0;
		int render_tile = -1;
		int material_index = -1;
		String uri;
	};

	struct StreamingCameraState {
		ObjectID id;
		Transform3D transform;
		Projection projection;
		Vector2 viewport_size;
	};

	struct StreamingTile {
		OsgExternalTileRef reference;
		int parent = -1; // -1 is the root document.
		Vector<int> children;
		Vector<int> render_tiles;
		AABB fallback_aabb;
		bool has_fallback_aabb = false;
		TileState state = TILE_STATE_UNLOADED;
		uint64_t data_request_id = 0;
		uint64_t parse_request_id = 0;
		uint64_t source_bytes = 0;
		OsgTileDocument cpu_document;
		bool has_cpu_document = false;
		bool desired = false;
		uint64_t resident_since_usec = 0;
		uint64_t last_used_usec = 0;
		double request_priority = 0.0;
		bool display_desired = false;
	};

	Vector<TileRenderInstance> render_tiles;
	Vector<int> free_render_tiles;
	Vector<int> root_render_tiles;
	Vector<TextureRequest> texture_requests;
	Vector<StreamingTile> streaming_tiles;
	HashMap<String, int> streaming_tile_by_uri;
	Vector<int> root_children;
	ObjectID display_camera_id;
	Vector<StreamingCameraState> streaming_camera_states;
	bool streaming_topology_dirty = true;
	mutable Vector<uint8_t> tile_displayable_cache;
	mutable HashMap<int, bool> parent_refined_cache;
	mutable HashMap<uint64_t, bool> group_refined_cache;
	mutable HashMap<uint64_t, bool> group_pending_cache;

	String source_uri;
	NodePath camera_path;
	bool auto_load = true;
	bool open_state = false;
	bool paused = false;
	uint64_t generation = 0;
	uint64_t root_data_request_id = 0;
	uint64_t root_parse_request_id = 0;
	uint64_t root_source_bytes = 0;
	TileState root_tile_state = TILE_STATE_UNLOADED;
	OsgTileDocument cpu_ready_document;
	bool has_cpu_ready_document = false;

	AxisMode axis_mode = AXIS_MODE_OSG_Z_UP;
	WorldOriginMode world_origin_mode = WORLD_ORIGIN_ROOT_CENTER;
	double unit_scale = 1.0;
	double world_origin_x = 0.0;
	double world_origin_y = 0.0;
	double world_origin_z = 0.0;

	double lod_bias = 1.0;
	double lod_hysteresis = 0.15;
	int memory_budget_mb = 0;
	int max_concurrent_requests = 0;
	int max_tile_size_mb = 64;
	double upload_budget_ms = 4.0;
	double request_timeout_sec = 30.0;
	int retry_count = 2;
	GeometryInstance3D::ShadowCastingSetting cast_shadow = GeometryInstance3D::SHADOW_CASTING_SETTING_ON;

	bool editor_preview_enabled = true;
	double editor_preview_lod_bias = 0.5;
	int editor_preview_memory_mb = 256;
	int editor_preview_max_requests = 4;
	bool editor_preview_show_bounds = false;

	AABB dataset_aabb;
	AABB resident_aabb;
	uint32_t last_layer_mask = 1;
	bool last_visible = true;

	void _clear_render_tiles();
	void _cancel_texture_requests_for_tile(int p_render_tile = -1);
	void _free_render_tile(int p_index, bool p_recompute_bounds = true);
	int _store_render_tile(const TileRenderInstance &p_tile);
	const Vector<int> *_get_owner_render_tiles(int p_owner_tile) const;
	void _recompute_resident_aabb();
	Error _upload_document(const OsgTileDocument &p_document, String &r_error, int p_owner_tile);
	void _sync_render_instance(const TileRenderInstance &p_tile);
	void _sync_render_instances();
	Transform3D _get_tile_world_transform(const TileRenderInstance &p_tile) const;
	Camera3D *_resolve_runtime_camera() const;
	void _poll_parse_result();
	void _poll_data_results();
	void _poll_texture_results();
	void _upload_cpu_ready_document();
	void _clear_streaming_tiles();
	void _register_external_tiles(const OsgTileDocument &p_document, int p_parent);
	void _update_streaming(Camera3D *p_camera);
	void _update_streaming_from_cameras(const Vector<Camera3D *> &p_cameras, Camera3D *p_display_camera = nullptr);
	void _request_streaming_tiles();
	void _poll_streaming_results();
	void _upload_streaming_tiles();
	void _update_streaming_visibility();
	void _enforce_memory_budget();
	bool _range_matches(const StreamingTile &p_tile, Camera3D *p_camera) const;
	double _get_lod_metric(const OsgVector3d &p_center, double p_radius, OsgLodRangePayload::RangeMode p_range_mode, Camera3D *p_camera) const;
	double _get_request_priority(const StreamingTile &p_tile, Camera3D *p_camera, bool p_active_camera) const;
	bool _parent_is_desired(int p_parent) const;
	bool _parent_is_display_desired(int p_parent) const;
	bool _parent_is_fully_refined(int p_parent) const;
	bool _parent_group_is_fully_refined(int p_parent, int p_fallback_group) const;
	bool _parent_group_has_display_refinement(int p_parent, int p_fallback_group) const;
	bool _range_matches(const OsgVector3d &p_center, double p_radius, float p_min_range, float p_max_range, OsgLodRangePayload::RangeMode p_range_mode, bool p_was_matching, Camera3D *p_camera) const;
	bool _render_lod_matches(const TileRenderInstance &p_tile, Camera3D *p_camera) const;
	bool _tile_is_displayable(int p_index) const;
	bool _owner_has_render_geometry(int p_owner_tile) const;
	static String _tile_state_name(TileState p_state);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_source_uri(const String &p_uri);
	String get_source_uri() const;
	void set_auto_load(bool p_enabled);
	bool is_auto_load_enabled() const;
	void set_camera_path(const NodePath &p_path);
	NodePath get_camera_path() const;

	void set_axis_mode(AxisMode p_mode);
	AxisMode get_axis_mode() const;
	void set_unit_scale(double p_scale);
	double get_unit_scale() const;
	void set_world_origin_mode(WorldOriginMode p_mode);
	WorldOriginMode get_world_origin_mode() const;
	void set_world_origin(double p_x, double p_y, double p_z);
	double get_world_origin_x() const;
	double get_world_origin_y() const;
	double get_world_origin_z() const;
	void set_world_origin_x(double p_value);
	void set_world_origin_y(double p_value);
	void set_world_origin_z(double p_value);

	void set_lod_bias(double p_value);
	double get_lod_bias() const;
	void set_lod_hysteresis(double p_value);
	double get_lod_hysteresis() const;
	void set_memory_budget_mb(int p_value);
	int get_memory_budget_mb() const;
	void set_max_concurrent_requests(int p_value);
	int get_max_concurrent_requests() const;
	void set_max_tile_size_mb(int p_value);
	int get_max_tile_size_mb() const;
	void set_upload_budget_ms(double p_value);
	double get_upload_budget_ms() const;
	void set_request_timeout_sec(double p_value);
	double get_request_timeout_sec() const;
	void set_retry_count(int p_value);
	int get_retry_count() const;
	void set_cast_shadow(GeometryInstance3D::ShadowCastingSetting p_setting);
	GeometryInstance3D::ShadowCastingSetting get_cast_shadow() const;

	void set_editor_preview_enabled(bool p_enabled);
	bool is_editor_preview_enabled() const;
	void set_editor_preview_lod_bias(double p_value);
	double get_editor_preview_lod_bias() const;
	void set_editor_preview_memory_mb(int p_value);
	int get_editor_preview_memory_mb() const;
	void set_editor_preview_max_requests(int p_value);
	int get_editor_preview_max_requests() const;
	void set_editor_preview_show_bounds(bool p_enabled);
	bool is_editor_preview_showing_bounds() const;

	Error open();
	void close();
	Error reload();
	bool is_open() const;
	void set_streaming_paused(bool p_paused);
	bool is_streaming_paused() const;

	Vector3 world_to_local(double p_x, double p_y, double p_z) const;
	PackedFloat64Array local_to_world(const Vector3 &p_local_position) const;
	Dictionary get_streaming_stats() const;
	AABB get_dataset_aabb() const;
	AABB get_resident_aabb() const;
	virtual AABB get_aabb() const override;

#ifdef TOOLS_ENABLED
	void editor_update_cameras(const Vector<Camera3D *> &p_cameras, Camera3D *p_active_camera);
#endif

	TiledMeshInstance3D();
	~TiledMeshInstance3D();
};

VARIANT_ENUM_CAST(TiledMeshInstance3D::AxisMode);
VARIANT_ENUM_CAST(TiledMeshInstance3D::WorldOriginMode);
VARIANT_ENUM_CAST(TiledMeshInstance3D::TileState);
