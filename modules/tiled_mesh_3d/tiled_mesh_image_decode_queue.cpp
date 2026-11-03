/**************************************************************************/
/*  tiled_mesh_image_decode_queue.cpp                                     */
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

#include "tiled_mesh_image_decode_queue.h"

#include "tiled_mesh_data_source.h"

TiledMeshImageDecodeQueue *TiledMeshImageDecodeQueue::singleton = nullptr;

void TiledMeshImageDecodeQueue::_thread_func(void *p_userdata) {
	static_cast<TiledMeshImageDecodeQueue *>(p_userdata)->_thread_loop();
}

void TiledMeshImageDecodeQueue::_thread_loop() {
	Thread::set_name("TiledMesh Images");
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
			active_request_id = request.request_id;
		}

		Result result;
		result.request_id = request.request_id;
		result.owner = request.owner;
		result.generation = request.generation;
		result.error = TiledMeshDataSource::decode_image_bytes(request.bytes, request.uri, result.image, result.error_message);
		{
			MutexLock lock(mutex);
			active_request_id = 0;
			if (cancelled_requests.has(result.request_id)) {
				cancelled_requests.erase(result.request_id);
				continue;
			}
			if (!exit_requested) {
				results.push_back(result);
			}
		}
	}
}

Error TiledMeshImageDecodeQueue::start() {
	ERR_FAIL_COND_V(thread.is_started(), ERR_ALREADY_IN_USE);
	exit_requested = false;
	return thread.start(_thread_func, this) == Thread::UNASSIGNED_ID ? ERR_CANT_CREATE : OK;
}

void TiledMeshImageDecodeQueue::shutdown() {
	if (!thread.is_started()) {
		return;
	}
	{
		MutexLock lock(mutex);
		exit_requested = true;
		requests.clear();
		results.clear();
		cancelled_requests.clear();
	}
	semaphore.post();
	thread.wait_to_finish();
	active_request_id = 0;
}

uint64_t TiledMeshImageDecodeQueue::enqueue(ObjectID p_owner, uint64_t p_generation, const String &p_uri, const PackedByteArray &p_bytes, double p_priority) {
	ERR_FAIL_COND_V(!thread.is_started() || p_bytes.is_empty(), 0);
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
		request.uri = p_uri;
		request.bytes = p_bytes;
		request.priority = p_priority;
		requests.push_back(request);
	}
	semaphore.post();
	return request.request_id;
}

void TiledMeshImageDecodeQueue::update_priority(uint64_t p_request_id, double p_priority) {
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

void TiledMeshImageDecodeQueue::cancel(uint64_t p_request_id) {
	if (p_request_id == 0) {
		return;
	}
	MutexLock lock(mutex);
	for (List<Request>::Element *element = requests.front(); element;) {
		List<Request>::Element *next = element->next();
		if (element->get().request_id == p_request_id) {
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

bool TiledMeshImageDecodeQueue::take_result(uint64_t p_request_id, Result &r_result) {
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

bool TiledMeshImageDecodeQueue::take_next_result_for_testing(Result &r_result) {
	MutexLock lock(mutex);
	if (results.is_empty()) {
		return false;
	}
	r_result = results.front()->get();
	results.pop_front();
	return true;
}

TiledMeshImageDecodeQueue::TiledMeshImageDecodeQueue() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}

TiledMeshImageDecodeQueue::~TiledMeshImageDecodeQueue() {
	shutdown();
	if (singleton == this) {
		singleton = nullptr;
	}
}
