/**************************************************************************/
/*  pnts_parser.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/**************************************************************************/
#include "modules/3dtiles/parser/tiles_3d_decoder.h"
#include "modules/3dtiles/parser/tile_table.h"

namespace Tiles3DParserInternal {

bool Decoder::decode_pnts(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth) {
	LegacyTile tile;
	if (!tile.load(p_bytes, 28, false)) return fail(tile.error);
	TileTable &ft = tile.feature;
	Vector<double> values, positions;
	if (!ft.semantic("POINTS_LENGTH", "UNSIGNED_INT", 1, 1, values)) return fail(ft.error);
	if (values[0] > 1000000 || values[0] <= 0) return fail("Invalid POINTS_LENGTH.");
	const int count = int(values[0]);
	if (!ft.positions(count, positions)) return fail(ft.error);
	Vec rtc = p_rtc;
	if (ft.json.has("RTC_CENTER")) {
		if (!ft.semantic("RTC_CENTER", "FLOAT", 3, 1, values)) return fail(ft.error);
		rtc = add(rtc, { values[0], values[1], values[2] });
	}
	if (request.root && request.context.is_empty() && document.surfaces.is_empty()) {
		const Vec center = context.transform.point(rtc);
		choose_frame(center, false);
		document.source_origin = options.session.use_format_default_origin ? center : options.session.explicit_origin;
		document.coordinate_frame.origin = document.source_origin;
		document.coordinate_frame.source_to_local = context.frame;
	}
	TiledMeshMeshSurfacePayload surface;
	surface.points = true;
	surface.replacement_group = 0;
	surface.vertices.resize(count);
	for (int i = 0; i < count; i++) {
		const Vector3 position = local(context.transform.point(add(rtc, { positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2] })));
		if (!position.is_finite()) return fail("Point position exceeds render precision.");
		surface.vertices.set(i, position);
		if (i == 0) surface.aabb = AABB(position, Vector3());
		else surface.aabb.expand_to(position);
	}
	int batch_length = count;
	if (ft.json.has("BATCH_ID")) {
		if (!ft.semantic("BATCH_LENGTH", "UNSIGNED_INT", 1, 1, values) || values[0] > 1000000) return fail("Invalid point BATCH_LENGTH.");
		batch_length = int(values[0]);
		if (!ft.batch_ids(count, batch_length, surface.batch_ids)) return fail(ft.error);
	} else {
		surface.batch_ids.resize(count);
		for (int i = 0; i < count; i++) {
			surface.batch_ids.set(i, i);
		}
	}
	const int previous_length = active_batch_length;
	const int previous_table = active_batch_table;
	if (!batch_table(tile.batch_json, tile.batch_binary, batch_length)) return false;
	surface.batch_table = active_batch_table;
	active_batch_length = previous_length;
	active_batch_table = previous_table;
	const String color_semantic = ft.json.has("RGBA") ? "RGBA" : (ft.json.has("RGB") ? "RGB" : (ft.json.has("RGB565") ? "RGB565" : "CONSTANT_RGBA"));
	bool transparent = false;
	if (ft.json.has(color_semantic)) {
		const int components = color_semantic == "RGB" ? 3 : (color_semantic == "RGB565" ? 1 : 4);
		const bool constant = color_semantic == "CONSTANT_RGBA";
		if (!ft.semantic(color_semantic, color_semantic == "RGB565" ? "UNSIGNED_SHORT" : "UNSIGNED_BYTE", components, constant ? 1 : count, values, !constant)) return fail(ft.error);
		surface.colors.resize(count);
		for (int i = 0; i < count; i++) {
			const int offset = constant ? 0 : i * components;
			Color color;
			if (components == 1) {
				const int packed = int(values[offset]);
				color = Color(((packed >> 11) & 31) / 31.0, ((packed >> 5) & 63) / 63.0, (packed & 31) / 31.0);
			} else color = Color(values[offset] / 255.0, values[offset + 1] / 255.0, values[offset + 2] / 255.0, components == 4 ? values[offset + 3] / 255.0 : 1.0);
			transparent = transparent || color.a < 1.0;
			surface.colors.set(i, color);
		}
	}
	if (ft.json.has("NORMAL") || ft.json.has("NORMAL_OCT16P")) {
		const bool oct = !ft.json.has("NORMAL");
		if (!ft.semantic(oct ? "NORMAL_OCT16P" : "NORMAL", oct ? "UNSIGNED_BYTE" : "FLOAT", oct ? 2 : 3, count, values, true)) return fail(ft.error);
		const Basis basis = context.frame * context.transform.basis().inverse().transposed();
		surface.normals.resize(count);
		for (int i = 0; i < count; i++) {
			const Vector3 normal = oct ? TileTable::oct_normal(values[i * 2], values[i * 2 + 1], 255) : Vector3(values[i * 3], values[i * 3 + 1], values[i * 3 + 2]);
			if (normal.is_zero_approx()) return fail("Invalid point normal.");
			surface.normals.set(i, basis.xform(normal).normalized());
		}
	}
	TiledMeshMaterialPayload material;
	material.unlit = surface.normals.is_empty();
	material.transparent = transparent;
	surface.material = document.materials.size();
	document.materials.push_back(material);
	surface.estimated_bytes = uint64_t(count) * (sizeof(Vector3) * (surface.normals.is_empty() ? 1 : 2) + sizeof(int32_t) + (surface.colors.is_empty() ? 0 : sizeof(Color)));
	document.estimated_cpu_bytes += surface.estimated_bytes;
	document.local_aabb = has_aabb ? document.local_aabb.merge(surface.aabb) : surface.aabb;
	has_aabb = true;
	document.surfaces.push_back(surface);
	return true;
}

} // namespace Tiles3DParserInternal
