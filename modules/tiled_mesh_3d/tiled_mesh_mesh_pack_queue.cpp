/**************************************************************************/
/*  tiled_mesh_mesh_pack_queue.cpp                                       */
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

#include "tiled_mesh_mesh_pack_queue.h"

#include "scene/resources/mesh.h"
#include "servers/rendering/rendering_server.h"

TiledMeshMeshPackQueue *TiledMeshMeshPackQueue::singleton = nullptr;

static uint64_t surface_data_size(const RenderingServerTypes::SurfaceData &p_surface) {
	uint64_t size = p_surface.vertex_data.size() + p_surface.attribute_data.size() + p_surface.skin_data.size() + p_surface.index_data.size() + p_surface.blend_shape_data.size();
	for (const RenderingServerTypes::SurfaceData::LOD &lod : p_surface.lods) {
		size += lod.index_data.size();
	}
	return size;
}

void TiledMeshMeshPackQueue::_thread_func(void *p_userdata) {
	static_cast<TiledMeshMeshPackQueue *>(p_userdata)->_thread_loop();
}

bool TiledMeshMeshPackQueue::_request_was_cancelled(uint64_t p_request_id) const {
	MutexLock lock(mutex);
	return exit_requested || cancelled_requests.has(p_request_id);
}

Error TiledMeshMeshPackQueue::_pack_document(const Request &p_request, TiledMeshPackedDocument &r_document, String &r_error) const {
	r_error.clear();
	r_document.document = p_request.document;
	r_document.document.surfaces.clear();
	if (p_request.document.surfaces.is_empty()) {
		return OK;
	}

	Vector<Pair<int, int>> group_keys;
	for (const TiledMeshMeshSurfacePayload &surface : p_request.document.surfaces) {
		if (surface.vertices.is_empty() || (!surface.points && surface.indices.is_empty())) {
			continue;
		}
		const Pair<int, int> key(surface.replacement_group, surface.lod_branch);
		if (!group_keys.has(key)) {
			group_keys.push_back(key);
		}
	}
	if (group_keys.is_empty()) {
		r_error = "No valid mesh surfaces could be packed.";
		return ERR_FILE_CORRUPT;
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		r_error = "RenderingServer is not available for CPU mesh packing.";
		return ERR_UNAVAILABLE;
	}

	for (const Pair<int, int> &group_key : group_keys) {
		if (_request_was_cancelled(p_request.request_id)) {
			return ERR_SKIP;
		}
		TiledMeshPackedRenderGroup group;
		group.replacement_group = group_key.first;
		group.lod_branch = group_key.second;
		bool has_aabb = false;
		for (const TiledMeshMeshSurfacePayload &surface : p_request.document.surfaces) {
			if (surface.replacement_group != group_key.first || surface.lod_branch != group_key.second || surface.vertices.is_empty() || (!surface.points && surface.indices.is_empty())) {
				continue;
			}
			if (_request_was_cancelled(p_request.request_id)) {
				return ERR_SKIP;
			}
			Array arrays;
			arrays.resize(Mesh::ARRAY_MAX);
			arrays[Mesh::ARRAY_VERTEX] = surface.vertices;
			if (!surface.indices.is_empty()) {
				arrays[Mesh::ARRAY_INDEX] = surface.indices;
			}
			if (surface.normals.size() == surface.vertices.size()) {
				arrays[Mesh::ARRAY_NORMAL] = surface.normals;
			}
			if (surface.uv0.size() == surface.vertices.size()) {
				arrays[Mesh::ARRAY_TEX_UV] = surface.uv0;
			}
			if (surface.uv1.size() == surface.vertices.size()) {
				arrays[Mesh::ARRAY_TEX_UV2] = surface.uv1;
			}
			if (surface.colors.size() == surface.vertices.size()) {
				arrays[Mesh::ARRAY_COLOR] = surface.colors;
			}

			TiledMeshPackedSurface packed_surface;
			packed_surface.points = surface.points;
			const Error error = rs->mesh_create_surface_data_from_arrays(&packed_surface.surface_data, surface.points ? RSE::PRIMITIVE_POINTS : RSE::PRIMITIVE_TRIANGLES, arrays);
			if (error != OK) {
				r_error = "Unable to create RenderingServer surface data from the format-neutral mesh arrays.";
				return error;
			}
			packed_surface.vertices = surface.vertices;
			packed_surface.indices = surface.indices;
			packed_surface.batch_ids = surface.batch_ids;
			packed_surface.batch_table = surface.batch_table;
			packed_surface.material = surface.material;
			packed_surface.uses_vertex_color = !surface.colors.is_empty();
			packed_surface.estimated_bytes = surface.estimated_bytes;
			group.estimated_bytes += surface.estimated_bytes;
			group.aabb = has_aabb ? group.aabb.merge(surface.aabb) : surface.aabb;
			has_aabb = true;
			r_document.packed_bytes += surface_data_size(packed_surface.surface_data);
			group.surfaces.push_back(packed_surface);
		}
		if (!group.surfaces.is_empty()) {
			r_document.render_groups.push_back(group);
		}
	}
	return OK;
}

void TiledMeshMeshPackQueue::_thread_loop() {
	Thread::set_name("TiledMesh Mesh Pack");
	for (;;) {
		semaphore.wait();
		Request request;
		{
			MutexLock lock(mutex);
			if (exit_requested) {
				return;
			}
			if (requests.is_empty()) {
				continue;
			}
			List<Request>::Element *best = requests.front();
			for (List<Request>::Element *element = best->next(); element; element = element->next()) {
				if (element->get().priority > best->get().priority) {
					best = element;
				}
			}
			request = best->get();
			requests.erase(best);
			pending_request_bytes -= MIN(pending_request_bytes, request.estimated_bytes);
			active_request_id = request.request_id;
			active_request_bytes = request.estimated_bytes;
		}

		Result result;
		result.request_id = request.request_id;
		result.owner = request.owner;
		result.generation = request.generation;
		result.error = _pack_document(request, result.document, result.error_message);
		{
			MutexLock lock(mutex);
			active_request_id = 0;
			active_request_bytes = 0;
			const bool cancelled = cancelled_requests.has(result.request_id);
			cancelled_requests.erase(result.request_id);
			if (exit_requested) {
				return;
			}
			if (!cancelled && result.error != ERR_SKIP) {
				results.push_back(result);
			}
		}
	}
}

Error TiledMeshMeshPackQueue::start() {
	ERR_FAIL_COND_V(thread.is_started(), ERR_ALREADY_IN_USE);
	exit_requested = false;
	return thread.start(_thread_func, this) == Thread::UNASSIGNED_ID ? ERR_CANT_CREATE : OK;
}

void TiledMeshMeshPackQueue::shutdown() {
	if (!thread.is_started()) {
		return;
	}
	{
		MutexLock lock(mutex);
		exit_requested = true;
		requests.clear();
		results.clear();
		cancelled_requests.clear();
		pending_request_bytes = 0;
	}
	semaphore.post();
	thread.wait_to_finish();
	active_request_id = 0;
	active_request_bytes = 0;
}

uint64_t TiledMeshMeshPackQueue::enqueue(ObjectID p_owner, uint64_t p_generation, const TiledMeshDocument &p_document, double p_priority) {
	ERR_FAIL_COND_V(!thread.is_started(), 0);
	Request request;
	{
		MutexLock lock(mutex);
		ERR_FAIL_COND_V(exit_requested, 0);
		request.request_id = next_request_id++;
		if (next_request_id == 0) {
			next_request_id = 1;
		}
		request.owner = p_owner;
		request.generation = p_generation;
		request.document = p_document;
		request.priority = p_priority;
		request.estimated_bytes = p_document.estimated_cpu_bytes;
		pending_request_bytes += request.estimated_bytes;
		requests.push_back(request);
	}
	semaphore.post();
	return request.request_id;
}

void TiledMeshMeshPackQueue::update_priority(uint64_t p_request_id, double p_priority) {
	if (p_request_id == 0) {
		return;
	}
	MutexLock lock(mutex);
	for (Request &request : requests) {
		if (request.request_id == p_request_id) {
			request.priority = p_priority;
			return;
		}
	}
}

void TiledMeshMeshPackQueue::cancel(uint64_t p_request_id) {
	if (p_request_id == 0) {
		return;
	}
	MutexLock lock(mutex);
	for (List<Request>::Element *element = requests.front(); element;) {
		List<Request>::Element *next = element->next();
		if (element->get().request_id == p_request_id) {
			pending_request_bytes -= MIN(pending_request_bytes, element->get().estimated_bytes);
			requests.erase(element);
			return;
		}
		element = next;
	}
	for (List<Result>::Element *element = results.front(); element;) {
		List<Result>::Element *next = element->next();
		if (element->get().request_id == p_request_id) {
			results.erase(element);
			return;
		}
		element = next;
	}
	if (active_request_id == p_request_id) {
		cancelled_requests.insert(p_request_id);
	}
}

bool TiledMeshMeshPackQueue::take_result(uint64_t p_request_id, Result &r_result) {
	MutexLock lock(mutex);
	for (List<Result>::Element *element = results.front(); element; element = element->next()) {
		if (element->get().request_id == p_request_id) {
			r_result = element->get();
			results.erase(element);
			return true;
		}
	}
	return false;
}

bool TiledMeshMeshPackQueue::take_next_result_for_testing(Result &r_result) {
	MutexLock lock(mutex);
	if (results.is_empty()) {
		return false;
	}
	r_result = results.front()->get();
	results.pop_front();
	return true;
}

int TiledMeshMeshPackQueue::get_pending_count() const {
	MutexLock lock(mutex);
	return requests.size();
}

int TiledMeshMeshPackQueue::get_active_count() const {
	MutexLock lock(mutex);
	return active_request_id != 0 ? 1 : 0;
}

uint64_t TiledMeshMeshPackQueue::get_in_flight_bytes() const {
	MutexLock lock(mutex);
	return pending_request_bytes + active_request_bytes;
}

TiledMeshMeshPackQueue::TiledMeshMeshPackQueue() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}

TiledMeshMeshPackQueue::~TiledMeshMeshPackQueue() {
	shutdown();
	if (singleton == this) {
		singleton = nullptr;
	}
}
