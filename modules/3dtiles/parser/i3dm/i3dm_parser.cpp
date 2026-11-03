/**************************************************************************/
/*  i3dm_parser.cpp                                                       */
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

bool Decoder::decode_i3dm(const PackedByteArray &p_bytes, const Vec &p_rtc, int p_depth) {
	LegacyTile tile;
	if (!tile.load(p_bytes, 32, true)) return fail(tile.error);
	const uint32_t format = decode_uint32(p_bytes.ptr() + 28);
	if (format > 1) return fail("Invalid i3dm gltfFormat.");
	TileTable &ft = tile.feature;
	Vector<double> values, positions, right, up, scales, nonuniform;
	if (!ft.semantic("INSTANCES_LENGTH", "UNSIGNED_INT", 1, 1, values)) return fail(ft.error);
	if (values[0] <= 0 || values[0] > 1000000) return fail("Invalid INSTANCES_LENGTH.");
	const int count = int(values[0]);
	if (!ft.positions(count, positions)) return fail(ft.error);
	Vec rtc = p_rtc;
	if (ft.json.has("RTC_CENTER")) {
		if (!ft.semantic("RTC_CENTER", "FLOAT", 3, 1, values)) return fail(ft.error);
		rtc = add(rtc, { values[0], values[1], values[2] });
	}
	const bool oct = ft.json.has("NORMAL_RIGHT_OCT32P") || ft.json.has("NORMAL_UP_OCT32P");
	const bool oriented = oct || ft.json.has("NORMAL_RIGHT") || ft.json.has("NORMAL_UP");
	if (oct && (ft.json.has("NORMAL_RIGHT") || ft.json.has("NORMAL_UP"))) return fail("Conflicting i3dm orientation semantics.");
	if (oriented && (!ft.semantic(oct ? "NORMAL_RIGHT_OCT32P" : "NORMAL_RIGHT", oct ? "UNSIGNED_SHORT" : "FLOAT", oct ? 2 : 3, count, right, true) || !ft.semantic(oct ? "NORMAL_UP_OCT32P" : "NORMAL_UP", oct ? "UNSIGNED_SHORT" : "FLOAT", oct ? 2 : 3, count, up, true))) return fail(ft.error);
	bool enu = false;
	if (ft.json.has("EAST_NORTH_UP")) {
		if (ft.json["EAST_NORTH_UP"].get_type() != Variant::BOOL) return fail("EAST_NORTH_UP must be boolean.");
		enu = ft.json["EAST_NORTH_UP"];
	}
	if (ft.json.has("SCALE") && !ft.semantic("SCALE", "FLOAT", 1, count, scales, true)) return fail(ft.error);
	if (ft.json.has("SCALE_NON_UNIFORM") && !ft.semantic("SCALE_NON_UNIFORM", "FLOAT", 3, count, nonuniform, true)) return fail(ft.error);
	PackedInt32Array ids;
	if (ft.json.has("BATCH_ID")) {
		if (!ft.batch_ids(count, count, ids)) return fail(ft.error);
	} else {
		ids.resize(count);
		for (int i = 0; i < count; i++) ids.set(i, i);
	}
	PackedByteArray model = tile.payload;
	const String previous_uri = gltf_base_uri;
	String model_uri = previous_uri;
	if (format == 0) {
		for (int i = 0; i < model.size(); i++) {
			if (model[i] == 0) return fail("External i3dm glTF URI contains NUL.");
		}
		String uri;
		if (uri.append_utf8((const char *)model.ptr(), model.size()) != OK) return fail("Invalid UTF-8 in external i3dm glTF URI.");
		uri = uri.strip_edges();
		if (uri.is_empty()) return fail("Empty external i3dm glTF URI.");
		model_uri = TiledMeshDataSource::resolve_uri(request.uri, uri);
		if (model_uri.is_empty()) return fail("Invalid external i3dm glTF URI.");
		const PackedByteArray *cached = external_buffers.getptr(model_uri);
		if (!cached) {
			if (!dependencies.has(model_uri)) dependencies.push_back(model_uri);
			return fail("Waiting for external i3dm glTF.", ERR_BUSY);
		}
		model = *cached;
	} else {
		if (model.size() < 12 || memcmp(model.ptr(), "glTF", 4)) return fail("Embedded i3dm payload must be GLB.");
		const uint32_t size = decode_uint32(model.ptr() + 8);
		if (size < 12 || size > uint32_t(model.size()) || model.size() - size > 7) return fail("Invalid i3dm GLB length.");
		model = model.slice(0, size);
	}
	const int previous_length = active_batch_length;
	const int previous_table = active_batch_table;
	if (!batch_table(tile.batch_json, tile.batch_binary, count)) return false;
	gltf_base_uri = model_uri;
	const int table_index = active_batch_table;
	active_batch_length = -1;
	active_batch_table = -1;
	const Matrix parent = context.transform;
	bool parsed = true;
	for (int i = 0; i < count; i++) {
		const Vec position = add(rtc, { positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2] });
		Basis basis;
		if (oriented) {
			const Vector3 r = oct ? TileTable::oct_normal(right[i * 2], right[i * 2 + 1], 65535) : Vector3(right[i * 3], right[i * 3 + 1], right[i * 3 + 2]);
			const Vector3 u = oct ? TileTable::oct_normal(up[i * 2], up[i * 2 + 1], 65535) : Vector3(up[i * 3], up[i * 3 + 1], up[i * 3 + 2]);
			if (r.is_zero_approx() || u.is_zero_approx() || Math::abs(r.normalized().dot(u.normalized())) > 0.001) { parsed = fail("Invalid instance orientation."); break; }
			basis.set_column(0, r.normalized());
			basis.set_column(1, u.cross(r).normalized());
			basis.set_column(2, u.normalized());
		} else if (enu) {
			const Vec world = parent.point(position);
			Vector3 normal(world.x / (6378137.0 * 6378137.0), world.y / (6378137.0 * 6378137.0), world.z / (6356752.314245 * 6356752.314245));
			if (normal.length_squared() == 0) { parsed = fail("ENU is undefined at the Earth center."); break; }
			normal.normalize();
			Vector3 east(-world.y, world.x, 0);
			if (east.length_squared() == 0) east = Vector3(0, 1, 0);
			east.normalize();
			basis.set_column(0, east);
			basis.set_column(1, normal.cross(east).normalized());
			basis.set_column(2, normal);
			basis = parent.basis().inverse() * basis;
		}
		Matrix instance;
		for (int axis = 0; axis < 3; axis++) {
			const double scale = (scales.is_empty() ? 1.0 : scales[i]) * (nonuniform.is_empty() ? 1.0 : nonuniform[i * 3 + axis]);
			if (!std::isfinite(scale) || scale <= 0) { parsed = fail("Invalid instance scale."); break; }
			for (int row = 0; row < 3; row++) instance.m[axis * 4 + row] = basis.get_column(axis)[row] * scale;
		}
		if (!parsed) break;
		instance.m[12] = position.x;
		instance.m[13] = position.y;
		instance.m[14] = position.z;
		context.transform = parent * instance;
		const int start = document.surfaces.size();
		if (model.size() >= 4 && memcmp(model.ptr(), "glTF", 4) == 0) parsed = decode_glb(model, Vec());
		else {
			Dictionary manifest;
			parsed = json(model, manifest) && gltf(manifest, PackedByteArray(), Vec());
		}
		if (!parsed) break;
		for (int s = start; s < document.surfaces.size(); s++) {
			TiledMeshMeshSurfacePayload &surface = document.surfaces.write[s];
			surface.batch_table = table_index;
			surface.batch_ids.resize(surface.vertices.size());
			surface.batch_ids.fill(ids[i]);
			surface.estimated_bytes += uint64_t(surface.vertices.size()) * sizeof(int32_t);
			document.estimated_cpu_bytes += uint64_t(surface.vertices.size()) * sizeof(int32_t);
		}
		if (document.estimated_cpu_bytes > 512 * 1024 * 1024) { parsed = fail("Expanded instance geometry exceeds the memory limit."); break; }
	}
	context.transform = parent;
	gltf_base_uri = previous_uri;
	active_batch_length = previous_length;
	active_batch_table = previous_table;
	return parsed;
}

} // namespace Tiles3DParserInternal
