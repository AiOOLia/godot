/**************************************************************************/
/*  tileset_parser.cpp                                                    */
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
#include "modules/3dtiles/parser/tiles_3d_decoder.h"

namespace Tiles3DParserInternal {

bool Decoder::bounds(const Dictionary &p_json, const Matrix &p_transform, TiledMeshBoundingVolume &r_bounds) {
	if (p_json.has("sphere")) {
		double sphere[4];
		if (!numbers(p_json["sphere"], 4, sphere) || sphere[3] < 0) {
			return fail("Invalid boundingVolume.sphere.");
		}
		r_bounds.center = p_transform.point({ sphere[0], sphere[1], sphere[2] });
		r_bounds.radius = sphere[3] * p_transform.maximum_scale();
	} else if (p_json.has("box")) {
		double box[12];
		if (!numbers(p_json["box"], 12, box)) {
			return false;
		}
		r_bounds.type = TiledMeshBoundingVolume::TYPE_ORIENTED_BOX;
		r_bounds.center = p_transform.point({ box[0], box[1], box[2] });
		for (int i = 0; i < 3; i++) {
			r_bounds.half_axes[i] = p_transform.direction({ box[3 + i * 3], box[4 + i * 3], box[5 + i * 3] });
		}
		for (int mask = 0; mask < 8; mask++) {
			Vec corner;
			for (int i = 0; i < 3; i++) {
				corner = add(corner, multiply(r_bounds.half_axes[i], (mask & (1 << i)) ? 1.0 : -1.0));
			}
			r_bounds.radius = MAX(r_bounds.radius, length(corner));
		}
	} else if (p_json.has("region")) {
		double region[6];
		if (!numbers(p_json["region"], 6, region) || region[0] < -Math::PI || region[0] > Math::PI ||
				region[2] < -Math::PI || region[2] > Math::PI || region[1] < -Math::PI / 2 || region[3] > Math::PI / 2 || region[1] > region[3] || region[4] > region[5]) {
			return fail("Invalid WGS84 boundingVolume.region.");
		}
		// Regions are absolute WGS84 coordinates: tile transforms never
		// apply. Bound the full curved surface, including the date line,
		// with interval arithmetic, not only its eight corners.
		double west = region[0], east = region[2];
		if (east < west) {
			east += 2 * Math::PI;
		}
		auto trig_range = [](double low, double high, bool cosine) {
			double minimum = cosine ? std::cos(low) : std::sin(low);
			double maximum = minimum;
			const double end = cosine ? std::cos(high) : std::sin(high);
			minimum = MIN(minimum, end);
			maximum = MAX(maximum, end);
			const double offset = cosine ? 0.0 : Math::PI / 2;
			for (int k = -4; k <= 6; k++) {
				const double angle = offset + k * Math::PI;
				if (angle >= low && angle <= high) {
					const double value = (k % 2 == 0) ? 1.0 : -1.0;
					minimum = MIN(minimum, value);
					maximum = MAX(maximum, value);
				}
			}
			return Pair<double, double>(minimum, maximum);
		};
		auto product = [](Pair<double, double> a, Pair<double, double> b) {
			const double values[] = { a.first * b.first, a.first * b.second, a.second * b.first, a.second * b.second };
			Pair<double, double> result(values[0], values[0]);
			for (double value : values) {
				result.first = MIN(result.first, value);
				result.second = MAX(result.second, value);
			}
			return result;
		};
		const auto latitude_sin = trig_range(region[1], region[3], false);
		const double min_sin = region[1] <= 0 && region[3] >= 0 ? 0 : MIN(std::abs(latitude_sin.first), std::abs(latitude_sin.second));
		const double max_sin = MAX(std::abs(latitude_sin.first), std::abs(latitude_sin.second));
		constexpr double e2 = 6.6943799901413165e-3;
		const double n_min = 6378137.0 / std::sqrt(1 - e2 * min_sin * min_sin);
		const double n_max = 6378137.0 / std::sqrt(1 - e2 * max_sin * max_sin);
		const auto radial = product({ n_min + region[4], n_max + region[5] }, trig_range(region[1], region[3], true));
		const auto x = product(radial, trig_range(west, east, true));
		const auto y = product(radial, trig_range(west, east, false));
		const auto z = product({ n_min * (1 - e2) + region[4], n_max * (1 - e2) + region[5] }, latitude_sin);
		r_bounds.type = TiledMeshBoundingVolume::TYPE_ORIENTED_BOX;
		r_bounds.center = { (x.first + x.second) / 2, (y.first + y.second) / 2, (z.first + z.second) / 2 };
		r_bounds.half_axes[0].x = (x.second - x.first) / 2;
		r_bounds.half_axes[1].y = (y.second - y.first) / 2;
		r_bounds.half_axes[2].z = (z.second - z.first) / 2;
		r_bounds.radius = length({ r_bounds.half_axes[0].x, r_bounds.half_axes[1].y, r_bounds.half_axes[2].z });
	} else {
		return fail("A tile requires a sphere, box or region bounding volume.", ERR_UNAVAILABLE);
	}
	return std::isfinite(r_bounds.radius) && std::isfinite(length(r_bounds.center)) || fail("Bounding volume overflow.");
}

bool Decoder::tile(const Dictionary &p_tile, const Context &p_parent, int p_parent_index, double p_parent_error,
		const TiledMeshBoundingVolume &p_parent_bounds, int p_depth) {
	if (p_depth > 128 || document.tiles.size() >= 1000000) {
		return fail("Tileset exceeds the supported explicit hierarchy depth or tile count.", ERR_OUT_OF_MEMORY);
	}
	if (p_tile.has("implicitTiling")) {
		return fail("Implicit tiling is not supported by the 3D Tiles loader.", ERR_UNAVAILABLE);
	}
	Context current = p_parent;
	Matrix transform;
	if (p_tile.has("transform") && !matrix(p_tile["transform"], transform)) {
		return false;
	}
	current.transform = p_parent.transform * transform;
	if (p_tile.has("refine")) {
		const String refine = p_tile["refine"];
		if (refine != "ADD" && refine != "REPLACE") {
			return fail("Tile refine must be ADD or REPLACE.");
		}
		current.refinement = refine == "ADD" ? TiledMeshTileDefinition::REFINE_ADD : TiledMeshTileDefinition::REFINE_REPLACE;
	}
	Dictionary volume;
	TiledMeshTileDefinition definition;
	double geometric_error;
	if (!object(p_tile.get("boundingVolume", Variant()), volume) || !bounds(volume, current.transform, definition.bounding_volume) ||
			!number(p_tile.get("geometricError", Variant()), geometric_error) || geometric_error < 0) {
		return fail("Invalid tile bounding volume or geometricError: " + message);
	}
	geometric_error *= current.transform.maximum_scale();
	current.center = definition.bounding_volume.center;
	if (p_parent_index == -1 && request.root) {
		context.center = options.session.use_format_default_origin ? current.center : options.session.explicit_origin;
		choose_frame(current.center, volume.has("region"));
		current.frame = context.frame;
		document.source_origin = context.center;
		document.coordinate_frame.origin = context.center;
		document.coordinate_frame.source_to_local = context.frame;
		// A conservative AABB allows editor framing before geometry arrives.
		const double radius = definition.bounding_volume.radius * options.session.unit_scale;
		const Vector3 center = local(definition.bounding_volume.center);
		document.local_aabb = AABB(center - Vector3(radius, radius, radius), Vector3(radius, radius, radius) * 2);
	}
	definition.parent = p_parent_index;
	definition.provider_key = current.key;
	definition.replacement_group = 0;
	// The generic runtime stores refinement on the incoming edge; 3D
	// Tiles stores it on the parent whose content is being refined.
	definition.refinement = p_parent.refinement;
	if (p_parent_index >= 0 && !document.tiles[p_parent_index].contents.is_empty()) {
		definition.selection.mode = TiledMeshSelectionRule::MODE_SCREEN_SPACE_ERROR;
		definition.selection.center = p_parent_bounds.center;
		definition.selection.radius = p_parent_bounds.radius;
		definition.selection.volume_type = p_parent_bounds.type;
		for (int axis = 0; axis < 3; axis++) {
			definition.selection.half_axes[axis] = p_parent_bounds.half_axes[axis];
		}
		definition.selection.geometric_error = p_parent_error;
	}
	if (p_tile.has("viewerRequestVolume")) {
		document.warnings.push_back("viewerRequestVolume is ignored; tiles are selected by visibility and screen-space error.");
	}
	Array contents;
	if (p_tile.has("content") && p_tile.has("contents")) {
		return fail("A tile cannot define both content and contents.");
	}
	if (p_tile.has("content")) {
		contents.push_back(p_tile["content"]);
	} else if (!array(p_tile.get("contents", Array()), contents)) {
		return false;
	}
	for (int i = 0; i < contents.size(); i++) {
		Dictionary content;
		if (!object(contents[i], content)) {
			return false;
		}
		const Variant uri_value = content.get("uri", content.get("url", Variant()));
		if (uri_value.get_type() != Variant::STRING || String(uri_value).is_empty()) {
			return fail("Tile content requires a non-empty uri (or legacy url).");
		}
		TiledMeshContentRequest descriptor;
		descriptor.uri = TiledMeshDataSource::resolve_uri(request.uri, uri_value);
		if (descriptor.uri.is_empty()) {
			return fail("Unable to resolve tile content URI.");
		}
		descriptor.content_type = SNAME("3dtiles");
		Context content_context = current;
		content_context.key += "/content/" + itos(i);
		descriptor.context = encode_context(content_context);
		definition.contents.push_back(descriptor);
	}
	const int index = document.tiles.size();
	document.tiles.push_back(definition);
	Array children;
	if (!array(p_tile.get("children", Array()), children)) {
		return false;
	}
	for (int i = 0; i < children.size(); i++) {
		Dictionary child;
		Context child_context = current;
		child_context.key += "/children/" + itos(i);
		if (!object(children[i], child) || !tile(child, child_context, index, geometric_error, definition.bounding_volume, p_depth + 1)) {
			return false;
		}
	}
	return true;
}

bool Decoder::tileset(const Dictionary &p_json) {
	Dictionary asset, root;
	double geometric_error;
	if (!object(p_json.get("asset", Variant()), asset) || !object(p_json.get("root", Variant()), root) ||
			!number(p_json.get("geometricError", Variant()), geometric_error) || geometric_error < 0) {
		return fail("A tileset requires asset, root and a nonnegative geometricError.");
	}
	const String version = asset.get("version", "");
	if (version != "1.0" && version != "1.1") {
		return fail("Unsupported 3D Tiles asset version: " + version, ERR_UNAVAILABLE);
	}
	if (!required_extensions(p_json, {})) {
		return false;
	}
	if (context.ancestors.has(request.uri) || context.ancestors.size() >= 64) {
		return fail("Cyclic or excessively deep external tileset reference.");
	}
	context.ancestors.push_back(request.uri);
	context.key += "/root";
	return tile(root, context, -1, 0, TiledMeshBoundingVolume(), 0);
}

} // namespace Tiles3DParserInternal
