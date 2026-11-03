/**************************************************************************/
/*  tiled_mesh_format_registry.cpp                                        */
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

#include "tiled_mesh_format_registry.h"

#include "core/error/error_macros.h"
#include "core/templates/hash_set.h"

static Vector<TiledMeshFormatLoader *> &
get_tiled_mesh_loaders() {
	// Function-static storage deliberately outlives module deinitializers. A
	// dependent format module can therefore unregister after this module's
	// scene-level teardown has already run.
	static Vector<TiledMeshFormatLoader *> loaders;
	return loaders;
}

Error TiledMeshFormatRegistry::register_loader(TiledMeshFormatLoader *p_loader) {
	ERR_FAIL_NULL_V(p_loader, ERR_INVALID_PARAMETER);
	const StringName format_id = p_loader->get_format_id();
	ERR_FAIL_COND_V_MSG(format_id.is_empty(), ERR_INVALID_PARAMETER, "A tiled mesh format loader must have a non-empty format ID.");
	Vector<TiledMeshFormatLoader *> &loaders = get_tiled_mesh_loaders();
	for (TiledMeshFormatLoader *loader : loaders) {
		ERR_FAIL_COND_V_MSG(loader == p_loader || loader->get_format_id() == format_id, ERR_ALREADY_EXISTS, vformat("A tiled mesh format loader named '%s' is already registered.", format_id));
	}
	loaders.push_back(p_loader);
	return OK;
}

void TiledMeshFormatRegistry::unregister_loader(TiledMeshFormatLoader *p_loader) {
	get_tiled_mesh_loaders().erase(p_loader);
}

TiledMeshFormatLoader *TiledMeshFormatRegistry::find_by_id(const StringName &p_format_id) {
	for (TiledMeshFormatLoader *loader : get_tiled_mesh_loaders()) {
		if (loader->get_format_id() == p_format_id) {
			return loader;
		}
	}
	return nullptr;
}

TiledMeshFormatLoader *TiledMeshFormatRegistry::recognize(const String &p_uri, const PackedByteArray &p_bytes, String &r_error) {
	r_error.clear();
	TiledMeshFormatLoader *best_loader = nullptr;
	int best_score = 0;
	bool ambiguous = false;
	for (TiledMeshFormatLoader *loader : get_tiled_mesh_loaders()) {
		const int score = loader->recognize(p_uri, p_bytes);
		if (score > best_score) {
			best_loader = loader;
			best_score = score;
			ambiguous = false;
		} else if (score > 0 && score == best_score) {
			ambiguous = true;
		}
	}
	if (ambiguous) {
		r_error = vformat("More than one tiled mesh format recognizes '%s' with the same confidence. Set format_id explicitly.", p_uri);
		return nullptr;
	}
	if (!best_loader) {
		r_error = vformat("No registered tiled mesh format recognizes '%s'.", p_uri);
	}
	return best_loader;
}

Vector<String> TiledMeshFormatRegistry::get_recognized_extensions() {
	HashSet<String> unique_extensions;
	for (TiledMeshFormatLoader *loader : get_tiled_mesh_loaders()) {
		for (String extension : loader->get_recognized_extensions()) {
			extension = extension.trim_prefix(".").to_lower();
			if (!extension.is_empty()) {
				unique_extensions.insert(extension);
			}
		}
	}
	Vector<String> result;
	for (const String &extension : unique_extensions) {
		result.push_back(extension);
	}
	result.sort();
	return result;
}
