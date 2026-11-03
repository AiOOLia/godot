/**************************************************************************/
/*  tiles_3d_format.cpp                                                   */
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

#include "tiles_3d_format.h"

#include "tiles_3d_parser.h"

#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/templates/list.h"

#include "modules/tiled_mesh_3d/tiled_mesh_data_request_queue.h"
#include "modules/tiled_mesh_3d/tiled_mesh_data_source.h"

Error Tiles3DIonEndpoint::parse(const PackedByteArray &p_bytes, String &r_error) {
	url.clear();
	access_token.clear();
	r_error = "Invalid Cesium ion endpoint response.";
	if (p_bytes.size() > 1024 * 1024) {
		return ERR_OUT_OF_MEMORY;
	}
	JSON json;
	if (json.parse(String::utf8(reinterpret_cast<const char *>(p_bytes.ptr()), p_bytes.size())) != OK || json.get_data().get_type() != Variant::DICTIONARY) {
		return ERR_PARSE_ERROR;
	}
	const Dictionary endpoint = json.get_data();
	if (endpoint.get("type", Variant()) != Variant("3DTILES") || endpoint.has("externalType")) {
		r_error = "Cesium ion supports only hosted 3DTILES assets; terrain, imagery and external providers are not supported.";
		return ERR_UNAVAILABLE;
	}
	const Variant endpoint_url = endpoint.get("url", Variant());
	const Variant token = endpoint.get("accessToken", Variant());
	if (endpoint_url.get_type() != Variant::STRING || token.get_type() != Variant::STRING) {
		return ERR_INVALID_DATA;
	}
	const String candidate = endpoint_url;
	const String candidate_token = token;
	if (candidate_token.is_empty() || candidate_token.contains("\r") || candidate_token.contains("\n") ||
			candidate.contains("#") || !TiledMeshDataSource::is_bearer_uri_allowed(candidate, candidate)) {
		return ERR_INVALID_DATA;
	}
	url = candidate;
	access_token = candidate_token;
	r_error.clear();
	return OK;
}

namespace {

class Tiles3DSession : public TiledMeshFormatSession {
	struct Work {
		uint64_t id = 0;
		TiledMeshParseRequest request;
		HashMap<String, PackedByteArray> buffers;
		double priority = 0.0;
	};
	struct Completed {
		TiledMeshParseResult result;
		Vector<String> dependencies;
	};
	struct Pending {
		Work work;
		HashMap<String, uint64_t> downloads;
		bool downloading_root = false;
	};

	Tiles3DIonEndpoint ion_endpoint;
	bool ion_enabled = false;
	Tiles3DParseOptions options;
	Thread thread;
	Mutex mutex;
	Semaphore semaphore;
	List<Work> work;
	List<Completed> completed;
	HashMap<uint64_t, Pending> pending;
	uint64_t next_id = 1;
	uint64_t active_id = 0;
	bool active_cancelled = false;
	bool stopping = false;

	static void thread_entry(void *p_data) {
		static_cast<Tiles3DSession *>(p_data)->run();
	}

	void run() {
		Thread::set_name("3D Tiles Parse");
		for (;;) {
			semaphore.wait();
			Work request;
			{
				MutexLock lock(mutex);
				if (stopping) {
					return;
				}
				if (work.is_empty()) {
					continue;
				}
				auto *best = work.front();
				for (auto *element = best->next(); element; element = element->next()) {
					if (element->get().priority > best->get().priority) {
						best = element;
					}
				}
				request = best->get();
				work.erase(best);
				active_id = request.id;
				active_cancelled = false;
			}
			Completed output;
			output.result.request_id = request.id;
			output.result.owner = request.request.owner;
			output.result.generation = request.request.generation;
			output.result.uri = request.request.uri;
			output.result.error = Tiles3DParser::parse(request.request, options, request.buffers,
					output.result.document, output.dependencies, output.result.error_message);
			{
				MutexLock lock(mutex);
				active_id = 0;
				if (!stopping && !active_cancelled) {
					completed.push_back(output);
				}
			}
		}
	}

	void submit(const Work &p_work) {
		{
			MutexLock lock(mutex);
			work.push_back(p_work);
		}
		semaphore.post();
	}

public:
	explicit Tiles3DSession(const Tiles3DParseOptions &p_options, bool p_ion_enabled) :
			ion_enabled(p_ion_enabled), options(p_options) {}

	String get_bearer_token(const String &p_uri) const override {
		return !ion_endpoint.url.is_empty() && TiledMeshDataSource::is_bearer_uri_allowed(ion_endpoint.url, p_uri) ? ion_endpoint.access_token : String();
	}

	Error start() {
		return thread.start(thread_entry, this) == Thread::UNASSIGNED_ID ? ERR_CANT_CREATE : OK;
	}

	uint64_t enqueue_parse(const TiledMeshParseRequest &p_request, double p_priority) override {
		if (stopping || !thread.is_started()) {
			return 0;
		}
		Pending request;
		request.work.id = next_id++;
		request.work.request = p_request;
		request.work.priority = p_priority;
		if (ion_enabled && p_request.root) {
			Completed output;
			output.result.request_id = request.work.id;
			output.result.owner = p_request.owner;
			output.result.generation = p_request.generation;
			output.result.uri = p_request.uri;
			output.result.error = ion_endpoint.parse(p_request.bytes, output.result.error_message);
			if (output.result.error == OK) {
				auto *queue = TiledMeshDataRequestQueue::get_singleton();
				const uint64_t download = queue ? queue->request(p_request.owner, p_request.generation, ion_endpoint.url,
						256 * 1024 * 1024, 30.0, 2, p_priority, ion_endpoint.access_token) : 0;
				if (download != 0) {
					request.work.request.uri = ion_endpoint.url;
					request.work.request.bytes.clear();
					request.downloading_root = true;
					request.downloads.insert(ion_endpoint.url, download);
					pending.insert(request.work.id, request);
					return request.work.id;
				}
				output.result.error = ERR_CANT_CREATE;
				output.result.error_message = "Unable to queue Cesium ion tileset.";
			}
			pending.insert(request.work.id, request);
			MutexLock lock(mutex);
			completed.push_back(output);
			return request.work.id;
		}
		pending.insert(request.work.id, request);
		submit(request.work);
		return request.work.id;
	}

	void update_priority(uint64_t p_id, double p_priority) override {
		Pending *request = pending.getptr(p_id);
		if (!request) {
			return;
		}
		request->work.priority = p_priority;
		if (auto *queue = TiledMeshDataRequestQueue::get_singleton()) {
			for (const KeyValue<String, uint64_t> &download : request->downloads) {
				queue->update_priority(download.value, p_priority);
			}
		}
		MutexLock lock(mutex);
		for (Work &item : work) {
			if (item.id == p_id) {
				item.priority = p_priority;
			}
		}
	}

	void cancel(uint64_t p_id) override {
		Pending *request = pending.getptr(p_id);
		if (!request) {
			return;
		}
		if (auto *queue = TiledMeshDataRequestQueue::get_singleton()) {
			for (const KeyValue<String, uint64_t> &download : request->downloads) {
				queue->cancel(download.value);
			}
		}
		pending.erase(p_id);
		MutexLock lock(mutex);
		for (auto *element = work.front(); element;) {
			auto *next = element->next();
			if (element->get().id == p_id) {
				work.erase(element);
			}
			element = next;
		}
		for (auto *element = completed.front(); element;) {
			auto *next = element->next();
			if (element->get().result.request_id == p_id) {
				completed.erase(element);
			}
			element = next;
		}
		if (active_id == p_id) {
			active_cancelled = true;
		}
	}

	bool take_result(uint64_t p_id, TiledMeshParseResult &r_result) override {
		Pending *request = pending.getptr(p_id);
		if (!request) {
			return false;
		}
		auto *queue = TiledMeshDataRequestQueue::get_singleton();
		if (!request->downloads.is_empty()) {
			Vector<String> ready;
			for (const KeyValue<String, uint64_t> &download : request->downloads) {
				TiledMeshDataRequestQueue::Result result;
				if (queue && !queue->take_result(download.value, result)) {
					continue;
				}
				if (!queue || result.error != OK) {
					r_result.request_id = p_id;
					r_result.owner = request->work.request.owner;
					r_result.generation = request->work.request.generation;
					r_result.uri = request->work.request.uri;
					r_result.error = queue ? result.error : ERR_UNAVAILABLE;
					r_result.error_message = request->downloading_root ? "Unable to load Cesium ion tileset: " + result.error_message : "Unable to load glTF buffer '" + download.key + "': " + result.error_message;
					cancel(p_id);
					return true;
				}
				if (request->downloading_root) {
					request->work.request.bytes = result.bytes;
				} else {
					request->work.buffers.insert(download.key, result.bytes);
				}
				ready.push_back(download.key);
			}
			for (const String &uri : ready) {
				request->downloads.erase(uri);
			}
			if (request->downloads.is_empty()) {
				request->downloading_root = false;
				submit(request->work);
			}
			return false;
		}

		Completed output;
		bool found = false;
		{
			MutexLock lock(mutex);
			for (auto *element = completed.front(); element; element = element->next()) {
				if (element->get().result.request_id == p_id) {
					output = element->get();
					completed.erase(element);
					found = true;
					break;
				}
			}
		}
		if (!found) {
			return false;
		}
		if (output.result.error == ERR_BUSY && !output.dependencies.is_empty() && queue) {
			for (const String &uri : output.dependencies) {
				const uint64_t download = queue->request(request->work.request.owner, request->work.request.generation,
						uri, 256 * 1024 * 1024, 30.0, 2, request->work.priority, get_bearer_token(uri));
				if (download == 0) {
					output.result.error = ERR_CANT_CREATE;
					output.result.error_message = "Unable to queue external glTF buffer: " + uri;
					break;
				}
				request->downloads.insert(uri, download);
			}
			if (output.result.error == ERR_BUSY) {
				return false;
			}
		}
		if (output.result.error == ERR_BUSY) {
			output.result.error = ERR_UNAVAILABLE;
			output.result.error_message = "External glTF buffers require the tiled mesh data queue.";
		}
		r_result = output.result;
		cancel(p_id);
		return true;
	}

	void shutdown() override {
		if (stopping) {
			return;
		}
		while (!pending.is_empty()) {
			cancel(pending.begin()->key);
		}
		ion_endpoint = Tiles3DIonEndpoint();
		{
			MutexLock lock(mutex);
			stopping = true;
			work.clear();
			completed.clear();
		}
		if (thread.is_started()) {
			semaphore.post();
			thread.wait_to_finish();
		}
	}

	~Tiles3DSession() override { shutdown(); }
};

} // namespace

void Tiles3DOptions::set_ion_asset_id(int64_t p_id) {
	ERR_FAIL_COND(p_id < 0);
	ion_asset_id = p_id;
	emit_changed();
}

void Tiles3DOptions::set_ion_access_token(const String &p_token) {
	ERR_FAIL_COND(p_token.contains("\r") || p_token.contains("\n"));
	ion_access_token = p_token;
	emit_changed();
}

String Tiles3DOptions::get_source_uri(const String &p_uri) const {
	if (ion_asset_id == 0) {
		return p_uri;
	}
	return ion_access_token.is_empty() ? String() : "https://api.cesium.com/v1/assets/" + itos(ion_asset_id) + "/endpoint";
}

void Tiles3DOptions::set_coordinate_system(CoordinateSystem p_system) {
	ERR_FAIL_INDEX(p_system, 3);
	coordinate_system = p_system;
	emit_changed();
}

void Tiles3DOptions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ion_asset_id", "id"), &Tiles3DOptions::set_ion_asset_id);
	ClassDB::bind_method(D_METHOD("get_ion_asset_id"), &Tiles3DOptions::get_ion_asset_id);
	ClassDB::bind_method(D_METHOD("set_ion_access_token", "token"), &Tiles3DOptions::set_ion_access_token);
	ClassDB::bind_method(D_METHOD("get_ion_access_token"), &Tiles3DOptions::get_ion_access_token);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ion_asset_id", PROPERTY_HINT_RANGE, "0,9223372036854775807,1"), "set_ion_asset_id", "get_ion_asset_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ion_access_token", PROPERTY_HINT_PASSWORD), "set_ion_access_token", "get_ion_access_token");
	ClassDB::bind_method(D_METHOD("set_coordinate_system", "system"), &Tiles3DOptions::set_coordinate_system);
	ClassDB::bind_method(D_METHOD("get_coordinate_system"), &Tiles3DOptions::get_coordinate_system);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "coordinate_system", PROPERTY_HINT_ENUM, "Auto,Cartesian Z-Up,ECEF"), "set_coordinate_system", "get_coordinate_system");
	BIND_ENUM_CONSTANT(COORDINATE_SYSTEM_AUTO);
	BIND_ENUM_CONSTANT(COORDINATE_SYSTEM_CARTESIAN);
	BIND_ENUM_CONSTANT(COORDINATE_SYSTEM_ECEF);
}

Vector<String> Tiles3DFormatLoader::get_recognized_extensions() const {
	return { "json", "b3dm", "i3dm", "pnts", "glb", "gltf", "cmpt" };
}

int Tiles3DFormatLoader::recognize(const String &p_uri, const PackedByteArray &p_bytes) const {
	if (p_bytes.size() >= 12 && (memcmp(p_bytes.ptr(), "b3dm", 4) == 0 || memcmp(p_bytes.ptr(), "i3dm", 4) == 0 || memcmp(p_bytes.ptr(), "pnts", 4) == 0 || memcmp(p_bytes.ptr(), "glTF", 4) == 0 || memcmp(p_bytes.ptr(), "cmpt", 4) == 0)) {
		return 100;
	}
	// Recognition runs on the main thread. Scan only top-level keys without
	// constructing a potentially very large JSON tree; validation is performed
	// by the parser worker. Quoted values and nested keys do not count.
	bool asset = false, root = false, geometric_error = false;
	int depth = 0;
	const uint8_t *bytes = p_bytes.ptr();
	for (int i = 0; i < p_bytes.size(); i++) {
		const uint8_t value = bytes[i];
		if (value == '"') {
			const int start = ++i;
			while (i < p_bytes.size() && bytes[i] != '"') {
				if (bytes[i] == '\\') {
					i++;
				}
				i++;
			}
			const int length = i - start;
			int next = i + 1;
			while (next < p_bytes.size() && (bytes[next] == ' ' || bytes[next] == '\t' || bytes[next] == '\r' || bytes[next] == '\n')) {
				next++;
			}
			if (depth == 1 && next < p_bytes.size() && bytes[next] == ':') {
				asset |= length == 5 && memcmp(bytes + start, "asset", 5) == 0;
				root |= length == 4 && memcmp(bytes + start, "root", 4) == 0;
				geometric_error |= length == 14 && memcmp(bytes + start, "geometricError", 14) == 0;
				if (asset && root && geometric_error) {
					return 100;
				}
			}
		} else if (value == '{' || value == '[') {
			depth++;
		} else if (value == '}' || value == ']') {
			if (--depth <= 0) {
				break;
			}
		} else if (depth == 0 && value != ' ' && value != '\t' && value != '\r' && value != '\n') {
			break;
		}
	}
	String path = p_uri.get_slice("?", 0).get_slice("#", 0);
	const String extension = path.get_extension().to_lower();
	if (extension == "b3dm" || extension == "i3dm" || extension == "pnts" || extension == "glb" || extension == "gltf" || extension == "cmpt") {
		return 70;
	}
	return path.get_file().to_lower() == "tileset.json" ? 50 : 0;
}

bool Tiles3DFormatLoader::supports_options(const Ref<TiledMeshFormatOptions> &p_options) const {
	return p_options.is_null() || Object::cast_to<Tiles3DOptions>(p_options.ptr()) != nullptr;
}

TiledMeshFormatSession *Tiles3DFormatLoader::create_session(const Ref<TiledMeshFormatOptions> &p_options, const TiledMeshSessionOptions &p_session_options, String &r_error) {
	r_error.clear();
	if (!supports_options(p_options)) {
		r_error = "The 3D Tiles loader requires Tiles3DOptions.";
		return nullptr;
	}
	Tiles3DParseOptions options;
	options.session = p_session_options;
	if (p_options.is_valid()) {
		options.coordinate_system = Object::cast_to<Tiles3DOptions>(p_options.ptr())->get_coordinate_system();
	}
	Tiles3DSession *session = memnew(Tiles3DSession(options, p_options.is_valid() && Object::cast_to<Tiles3DOptions>(p_options.ptr())->get_ion_asset_id() > 0));
	if (session->start() != OK) {
		memdelete(session);
		r_error = "Unable to start the 3D Tiles parser thread.";
		return nullptr;
	}
	return session;
}
