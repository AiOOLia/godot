/**************************************************************************/
/*  osg_tile_document.h                                                   */
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

#include "core/math/aabb.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/typed_array.h"

struct OsgVector3d {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct OsgImagePayload {
	String uri;
	PackedByteArray rgba8;
	int width = 0;
	int height = 0;

	bool has_pixels() const { return width > 0 && height > 0 && rgba8.size() == width * height * 4; }
};

struct OsgMaterialPayload {
	Color albedo = Color(1, 1, 1, 1);
	bool transparent = false;
	bool double_sided = false;
	bool alpha_test = false;
	float alpha_cutoff = 0.5f;
	OsgImagePayload albedo_image;
};

struct OsgLodRangePayload {
	enum RangeMode {
		DISTANCE_FROM_EYE_POINT,
		PIXEL_SIZE_ON_SCREEN,
	};

	// Index of an enclosing LOD branch, or -1 for the outermost branch.
	int parent = -1;
	OsgVector3d center;
	double radius = 0.0;
	float min_range = 0.0f;
	float max_range = 0.0f;
	RangeMode range_mode = DISTANCE_FROM_EYE_POINT;
};

struct OsgMeshSurfacePayload {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uv0;
	PackedColorArray colors;
	PackedInt32Array indices;
	int material = -1;
	// Identifies the PagedLOD/ProxyNode whose resident geometry is the
	// fallback for this surface. -1 means document-level geometry.
	int fallback_group = -1;
	// Deepest inline osg::LOD/osg::PagedLOD branch containing this surface.
	// Following OsgTileDocument::lod_ranges[parent] reconstructs all enclosing
	// range predicates. -1 means the surface is not controlled by an inline LOD.
	int lod_branch = -1;
	AABB aabb;
	uint64_t estimated_bytes = 0;
};

struct OsgExternalTileRef {
	String uri;
	OsgVector3d center;
	double radius = 0.0;
	float min_range = 0.0f;
	float max_range = 0.0f;
	OsgLodRangePayload::RangeMode range_mode = OsgLodRangePayload::DISTANCE_FROM_EYE_POINT;
	// Local to the document which declared this reference. Surfaces carrying
	// the same value can be hidden independently after this refinement loads.
	int fallback_group = -1;
};

struct OsgTileDocument {
	OsgVector3d source_origin;
	Vector<OsgMeshSurfacePayload> surfaces;
	Vector<OsgMaterialPayload> materials;
	Vector<OsgExternalTileRef> external_tiles;
	Vector<OsgLodRangePayload> lod_ranges;
	Vector<String> warnings;
	AABB local_aabb;
	uint64_t estimated_cpu_bytes = 0;
};
