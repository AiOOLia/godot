/**************************************************************************/
/*  tiled_mesh_format.h                                                   */
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

#include "core/io/resource.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/math_funcs.h"
#include "core/object/object_id.h"
#include "core/templates/vector.h"
#include "scene/resources/material.h"

struct TiledMeshVector3d {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct TiledMeshCoordinateFrame {
	TiledMeshVector3d origin;
	Basis source_to_local;
	double unit_scale = 1.0;
};

struct TiledMeshBoundingVolume {
	enum Type {
		TYPE_SPHERE,
		TYPE_ORIENTED_BOX,
	};

	Type type = TYPE_SPHERE;
	TiledMeshVector3d center;
	TiledMeshVector3d half_axes[3];
	double radius = 0.0;
};

struct TiledMeshSelectionRule {
	enum Mode {
		MODE_ALWAYS,
		MODE_DISTANCE_RANGE,
		MODE_PROJECTED_PIXEL_RANGE,
		MODE_SCREEN_SPACE_ERROR,
	};

	Mode mode = MODE_ALWAYS;
	TiledMeshVector3d center;
	double radius = 0.0;
	double minimum = 0.0;
	double maximum = Math::INF;
	double geometric_error = 0.0;
	// SSE distance is measured to the parent tile's volume, not its center.
	// Other selection modes only use center/radius and ignore these fields.
	TiledMeshBoundingVolume::Type volume_type = TiledMeshBoundingVolume::TYPE_SPHERE;
	TiledMeshVector3d half_axes[3];
};

struct TiledMeshImagePayload {
	String uri;
	PackedByteArray rgba8;
	int width = 0;
	int height = 0;

	bool has_pixels() const { return width > 0 && height > 0 && rgba8.size() == width * height * 4; }
};

struct TiledMeshMaterialPayload {
	Color albedo = Color(1, 1, 1, 1);
	Color emission = Color(0, 0, 0, 1);
	float metallic = 0.0f;
	float roughness = 1.0f;
	bool transparent = false;
	bool unlit = false;
	bool double_sided = false;
	bool alpha_test = false;
	float alpha_cutoff = 0.5f;
	TiledMeshImagePayload albedo_image;
	TiledMeshImagePayload normal_image;
	TiledMeshImagePayload orm_image;
	TiledMeshImagePayload emission_image;
};

struct TiledMeshLodRangePayload {
	// Index of an enclosing resident LOD branch, or -1 for the outermost branch.
	int parent = -1;
	TiledMeshSelectionRule selection;
};

struct TiledMeshBatchTablePayload {
	int batch_length = 0;
	Dictionary properties;
	Variant extras;
	Dictionary extensions;
};

struct TiledMeshMeshSurfacePayload {
	bool points = false;
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uv0;
	PackedVector2Array uv1;
	PackedColorArray colors;
	PackedInt32Array indices;
	PackedInt32Array batch_ids;
	int batch_table = -1;
	int material = -1;
	int replacement_group = -1;
	int lod_branch = -1;
	AABB aabb;
	uint64_t estimated_bytes = 0;
};

struct TiledMeshContentRequest {
	String uri;
	StringName content_type;
	PackedByteArray context;
	bool required_for_display = true;
};

struct TiledMeshTileDefinition {
	enum RefinementMode {
		REFINE_REPLACE,
		REFINE_ADD,
	};

	String provider_key;
	// -1 attaches to the tile whose content produced this document. Other
	// values address an earlier definition in the same document.
	int parent = -1;
	int replacement_group = -1;
	RefinementMode refinement = REFINE_REPLACE;
	TiledMeshBoundingVolume bounding_volume;
	TiledMeshSelectionRule selection;
	Vector<TiledMeshContentRequest> contents;
};

struct TiledMeshDocument {
	TiledMeshVector3d source_origin;
	TiledMeshCoordinateFrame coordinate_frame;
	Vector<TiledMeshMeshSurfacePayload> surfaces;
	Vector<TiledMeshMaterialPayload> materials;
	Vector<TiledMeshBatchTablePayload> batch_tables;
	Vector<TiledMeshTileDefinition> tiles;
	Vector<TiledMeshLodRangePayload> lod_ranges;
	Vector<String> warnings;
	AABB local_aabb;
	uint64_t estimated_cpu_bytes = 0;
};

class TiledMeshFormatOptions : public Resource {
	GDCLASS(TiledMeshFormatOptions, Resource);

protected:
	static void _bind_methods();

public:
	virtual StringName get_format_id() const { return StringName(); }
	virtual String get_source_uri(const String &p_uri) const { return p_uri; }
	virtual String get_source_bearer_token() const { return String(); }
};

struct TiledMeshSessionOptions {
	double unit_scale = 1.0;
	bool use_format_default_origin = true;
	TiledMeshVector3d explicit_origin;
};

struct TiledMeshParseRequest {
	ObjectID owner;
	uint64_t generation = 0;
	String uri;
	StringName content_type;
	PackedByteArray context;
	PackedByteArray bytes;
	bool root = false;
};

struct TiledMeshParseResult {
	uint64_t request_id = 0;
	ObjectID owner;
	uint64_t generation = 0;
	String uri;
	Error error = OK;
	String error_message;
	TiledMeshDocument document;
};

class TiledMeshFormatSession {
public:
	virtual String get_bearer_token(const String &p_uri) const { return String(); }
	virtual uint64_t enqueue_parse(const TiledMeshParseRequest &p_request, double p_priority) = 0;
	virtual void update_priority(uint64_t p_request_id, double p_priority) = 0;
	virtual void cancel(uint64_t p_request_id) = 0;
	virtual bool take_result(uint64_t p_request_id, TiledMeshParseResult &r_result) = 0;
	virtual void shutdown() = 0;
	virtual ~TiledMeshFormatSession() = default;
};

class TiledMeshFormatLoader {
public:
	virtual StringName get_format_id() const = 0;
	virtual Vector<String> get_recognized_extensions() const = 0;
	// A positive score recognizes the data. Equal best scores are deliberately
	// treated as ambiguous by the registry.
	virtual int recognize(const String &p_uri, const PackedByteArray &p_bytes) const = 0;
	virtual bool supports_options(const Ref<TiledMeshFormatOptions> &p_options) const = 0;
	virtual TiledMeshFormatSession *create_session(const Ref<TiledMeshFormatOptions> &p_options, const TiledMeshSessionOptions &p_session_options, String &r_error) = 0;
	virtual ~TiledMeshFormatLoader() = default;
};
