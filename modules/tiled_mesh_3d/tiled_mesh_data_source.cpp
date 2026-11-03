/**************************************************************************/
/*  tiled_mesh_data_source.cpp                                            */
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

#include "tiled_mesh_data_source.h"

#include "core/io/file_access.h"

bool TiledMeshDataSource::is_http_uri(const String &p_uri) {
	const String lower = p_uri.to_lower();
	return lower.begins_with("http://") || lower.begins_with("https://");
}

bool TiledMeshDataSource::is_bearer_uri_allowed(const String &p_root, const String &p_uri) {
	String root_scheme, root_host, root_path, root_fragment;
	String scheme, host, path, fragment;
	int root_port = 0, port = 0;
	if (p_root.parse_url(root_scheme, root_host, root_port, root_path, root_fragment) != OK ||
			p_uri.parse_url(scheme, host, port, path, fragment) != OK) {
		return false;
	}
	// 凭据仅用于 HTTPS 同源资产目录，拒绝编码路径和用户信息以避免歧义。
	if (root_scheme != "https://" || scheme != root_scheme || host.to_lower() != root_host.to_lower() ||
			(root_port == 0 ? 443 : root_port) != (port == 0 ? 443 : port) ||
			p_root.get_slice("://", 1).get_slice("/", 0).contains("@") || p_uri.get_slice("://", 1).get_slice("/", 0).contains("@")) {
		return false;
	}
	path = path.get_slice("?", 0);
	root_path = root_path.get_slice("?", 0);
	if (path.contains("%") || path.contains("\\") || root_path.contains("%") || root_path.contains("\\")) {
		return false;
	}
	return path.simplify_path().begins_with(root_path.get_base_dir().trim_suffix("/") + "/");
}

String TiledMeshDataSource::normalize_uri(const String &p_uri) {
	String uri = p_uri.strip_edges().replace("\\", "/");
	if (uri.to_lower().begins_with("file://")) {
		uri = uri.substr(7).uri_decode();
		if (uri.length() >= 3 && uri[0] == '/' && uri[2] == ':') {
			uri = uri.substr(1);
		}
	}
	if (is_http_uri(uri)) {
		return uri;
	}
	return uri.simplify_path();
}

String TiledMeshDataSource::resolve_uri(const String &p_parent_uri, const String &p_reference) {
	String reference = p_reference.strip_edges().replace("\\", "/");
	if (reference.is_empty()) {
		return String();
	}
	const String lower = reference.to_lower();
	if (lower.begins_with("http://") || lower.begins_with("https://") || lower.begins_with("file://") || lower.begins_with("res://") || lower.begins_with("user://")) {
		return normalize_uri(reference);
	}
	const String parent = normalize_uri(p_parent_uri);
	if (is_http_uri(parent)) {
		String scheme;
		String host;
		String parent_path;
		String fragment;
		int port = 0;
		if (parent.parse_url(scheme, host, port, parent_path, fragment) != OK) {
			return String();
		}
		const bool absolute_path = reference.begins_with("/");
		String base_path = absolute_path ? String("/") : parent_path.get_slice("?", 0).get_base_dir();
		if (base_path.is_empty()) {
			base_path = "/";
		}
		Vector<String> segments = base_path.split("/", false);
		const Vector<String> reference_segments = reference.split("/", true);
		for (const String &segment : reference_segments) {
			if (segment.is_empty() || segment == ".") {
				continue;
			}
			if (segment == "..") {
				if (segments.is_empty()) {
					return String(); // Relative URI escaped the HTTP root.
				}
				segments.remove_at(segments.size() - 1);
				continue;
			}
			segments.push_back(segment);
		}
		String normalized_path = "/" + String("/").join(segments);
		String authority = scheme + host;
		if (port > 0 && !((scheme == "http://" && port == 80) || (scheme == "https://" && port == 443))) {
			authority += ":" + itos(port);
		}
		return authority + normalized_path;
	}
	if (reference.is_absolute_path()) {
		return normalize_uri(reference);
	}
	return parent.get_base_dir().path_join(reference).simplify_path();
}

Error TiledMeshDataSource::load_local_bytes(const String &p_uri, uint64_t p_max_bytes, PackedByteArray &r_bytes, String &r_error) {
	r_bytes.clear();
	r_error.clear();
	const String uri = normalize_uri(p_uri);
	if (is_http_uri(uri)) {
		r_error = "HTTP sources require the asynchronous HTTP data source.";
		return ERR_UNAVAILABLE;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(uri, FileAccess::READ, &err);
	if (err != OK || file.is_null()) {
		r_error = vformat("Unable to open '%s' (Error %d).", uri, int(err));
		return err != OK ? err : ERR_CANT_OPEN;
	}
	const uint64_t length = file->get_length();
	if (length == 0) {
		r_error = vformat("Source '%s' is empty.", uri);
		return ERR_FILE_CORRUPT;
	}
	if (p_max_bytes > 0 && length > p_max_bytes) {
		r_error = vformat("Source '%s' is %d bytes, exceeding the %d byte tile limit.", uri, length, p_max_bytes);
		return ERR_OUT_OF_MEMORY;
	}
	if (length > uint64_t(INT32_MAX)) {
		r_error = vformat("Source '%s' is too large for an in-memory tile.", uri);
		return ERR_OUT_OF_MEMORY;
	}
	r_bytes.resize((int)length);
	const uint64_t read = file->get_buffer(r_bytes.ptrw(), length);
	if (read != length) {
		r_bytes.clear();
		r_error = vformat("Short read while loading '%s'.", uri);
		return ERR_FILE_CORRUPT;
	}
	return OK;
}

Error TiledMeshDataSource::decode_image_bytes(const PackedByteArray &p_bytes, const String &p_extension_or_uri, Ref<Image> &r_image, String &r_error) {
	r_image.instantiate();
	r_error.clear();
	String extension_or_uri = p_extension_or_uri;
	const int query_offset = extension_or_uri.find_char('?');
	if (query_offset >= 0) {
		extension_or_uri = extension_or_uri.left(query_offset);
	}
	String ext = extension_or_uri.get_extension().to_lower();
	if (ext.is_empty()) {
		ext = extension_or_uri.to_lower();
	}
	Error err = ERR_FILE_UNRECOGNIZED;
	if (ext == "png") {
		err = r_image->load_png_from_buffer(p_bytes);
	} else if (ext == "jpg" || ext == "jpeg") {
		err = r_image->load_jpg_from_buffer(p_bytes);
	} else if (ext == "bmp") {
		err = r_image->load_bmp_from_buffer(p_bytes);
	} else if (ext == "tga") {
		err = r_image->load_tga_from_buffer(p_bytes);
	}
	if (err != OK) {
		// Embedded files sometimes omit or mislabel their extension. Try the
		// common GIS texture formats by signature-capable decoders.
		err = r_image->load_png_from_buffer(p_bytes);
		if (err != OK) {
			err = r_image->load_jpg_from_buffer(p_bytes);
		}
		if (err != OK) {
			err = r_image->load_bmp_from_buffer(p_bytes);
		}
		if (err != OK) {
			err = r_image->load_tga_from_buffer(p_bytes);
		}
	}
	if (err != OK || r_image->is_empty()) {
		r_image.unref();
		r_error = vformat("Unable to decode image '%s' (Error %d).", ext, int(err));
		return err != OK ? err : ERR_FILE_CORRUPT;
	}
	r_image->convert(Image::FORMAT_RGBA8);
	return OK;
}
