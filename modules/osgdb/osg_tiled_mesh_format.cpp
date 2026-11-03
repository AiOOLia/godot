/**************************************************************************/
/*  osg_tiled_mesh_format.cpp                                             */
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

#include "osg_tiled_mesh_format.h"

#include "osg_parse_queue.h"

#include "core/object/class_db.h"

void OsgTiledMeshOptions::set_up_axis(UpAxis p_axis) {
	ERR_FAIL_INDEX(p_axis, 2);
	up_axis = p_axis;
	emit_changed();
}

OsgTiledMeshOptions::UpAxis OsgTiledMeshOptions::get_up_axis() const {
	return up_axis;
}

void OsgTiledMeshOptions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_up_axis", "axis"), &OsgTiledMeshOptions::set_up_axis);
	ClassDB::bind_method(D_METHOD("get_up_axis"), &OsgTiledMeshOptions::get_up_axis);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "up_axis", PROPERTY_HINT_ENUM, "Z-Up,Y-Up"), "set_up_axis", "get_up_axis");
	BIND_ENUM_CONSTANT(UP_AXIS_Z);
	BIND_ENUM_CONSTANT(UP_AXIS_Y);
}

OsgTiledMeshSession::OsgTiledMeshSession(bool p_osg_z_up, const TiledMeshSessionOptions &p_options) :
		osg_z_up(p_osg_z_up), session_options(p_options) {
}

OsgTiledMeshSession::~OsgTiledMeshSession() {
	shutdown();
}

uint64_t OsgTiledMeshSession::enqueue_parse(const TiledMeshParseRequest &p_request, double p_priority) {
	OsgParseQueue *queue = OsgParseQueue::get_singleton();
	ERR_FAIL_COND_V(shutting_down || !queue, 0);
	OsgSceneConvertOptions options;
	options.osg_z_up = osg_z_up;
	options.unit_scale = session_options.unit_scale;
	options.use_root_center = !p_request.root || session_options.use_format_default_origin;
	options.explicit_origin = session_options.explicit_origin;
	const uint64_t request_id = queue->enqueue(p_request.owner, p_request.generation, p_request.uri, p_request.bytes, options, p_priority);
	if (request_id != 0) {
		requests.insert(request_id);
	}
	return request_id;
}

void OsgTiledMeshSession::update_priority(uint64_t p_request_id, double p_priority) {
	if (requests.has(p_request_id)) {
		if (OsgParseQueue *queue = OsgParseQueue::get_singleton()) {
			queue->update_priority(p_request_id, p_priority);
		}
	}
}

void OsgTiledMeshSession::cancel(uint64_t p_request_id) {
	if (!requests.erase(p_request_id)) {
		return;
	}
	if (OsgParseQueue *queue = OsgParseQueue::get_singleton()) {
		queue->cancel(p_request_id);
	}
}

bool OsgTiledMeshSession::take_result(uint64_t p_request_id, TiledMeshParseResult &r_result) {
	if (!requests.has(p_request_id)) {
		return false;
	}
	OsgParseQueue *queue = OsgParseQueue::get_singleton();
	if (!queue) {
		return false;
	}
	OsgParseQueue::Result result;
	if (!queue->take_result(p_request_id, result)) {
		return false;
	}
	requests.erase(p_request_id);
	r_result.request_id = result.request_id;
	r_result.owner = result.owner;
	r_result.generation = result.generation;
	r_result.uri = result.source_uri;
	r_result.error = result.error;
	r_result.error_message = result.error_message;
	r_result.document = result.document;
	return true;
}

void OsgTiledMeshSession::shutdown() {
	if (shutting_down) {
		return;
	}
	shutting_down = true;
	if (OsgParseQueue *queue = OsgParseQueue::get_singleton()) {
		for (uint64_t request_id : requests) {
			queue->cancel(request_id);
		}
	}
	requests.clear();
}

Vector<String> OsgTiledMeshFormatLoader::get_recognized_extensions() const {
	Vector<String> extensions;
	extensions.push_back("osg");
	extensions.push_back("osgb");
	return extensions;
}

int OsgTiledMeshFormatLoader::recognize(const String &p_uri, const PackedByteArray &p_bytes) const {
	const String extension = p_uri.get_extension().to_lower();
	int score = extension == "osg" || extension == "osgb" ? 80 : 0;
	if ((p_bytes.size() >= 6 && memcmp(p_bytes.ptr(), "#Ascii", 6) == 0) ||
			(p_bytes.size() >= 5 && memcmp(p_bytes.ptr(), "<?xml", 5) == 0)) {
		score = 100;
	}
	return score;
}

bool OsgTiledMeshFormatLoader::supports_options(const Ref<TiledMeshFormatOptions> &p_options) const {
	return p_options.is_null() || Object::cast_to<OsgTiledMeshOptions>(p_options.ptr()) != nullptr;
}

TiledMeshFormatSession *OsgTiledMeshFormatLoader::create_session(const Ref<TiledMeshFormatOptions> &p_options, const TiledMeshSessionOptions &p_session_options, String &r_error) {
	r_error.clear();
	if (!supports_options(p_options)) {
		r_error = "The supplied format_options resource is not compatible with the OSG tiled mesh loader.";
		return nullptr;
	}
	const OsgTiledMeshOptions *options = Object::cast_to<OsgTiledMeshOptions>(p_options.ptr());
	return memnew(OsgTiledMeshSession(!options || options->get_up_axis() == OsgTiledMeshOptions::UP_AXIS_Z, p_session_options));
}
