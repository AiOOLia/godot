/**************************************************************************/
/*  osg_image_decode_queue.cpp                                            */
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

#include "osg_image_decode_queue.h"

#include "osg_data_source.h"

OsgImageDecodeQueue *OsgImageDecodeQueue::singleton = nullptr;

void OsgImageDecodeQueue::_thread_func(void *p_userdata) {
	static_cast<OsgImageDecodeQueue *>(p_userdata)->_thread_loop();
}

void OsgImageDecodeQueue::_thread_loop() {
	Thread::set_name("OSGDB Images");
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
			request = requests.front()->get();
			requests.pop_front();
			active_request_id = request.request_id;
		}

		Result result;
		result.request_id = request.request_id;
		result.owner = request.owner;
		result.generation = request.generation;
		result.error = OsgDataSource::decode_image_bytes(request.bytes, request.uri, result.image, result.error_message);
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

Error OsgImageDecodeQueue::start() {
	ERR_FAIL_COND_V(thread.is_started(), ERR_ALREADY_IN_USE);
	exit_requested = false;
	return thread.start(_thread_func, this) == Thread::UNASSIGNED_ID ? ERR_CANT_CREATE : OK;
}

void OsgImageDecodeQueue::shutdown() {
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

uint64_t OsgImageDecodeQueue::enqueue(ObjectID p_owner, uint64_t p_generation, const String &p_uri, const PackedByteArray &p_bytes) {
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
		requests.push_back(request);
	}
	semaphore.post();
	return request.request_id;
}

void OsgImageDecodeQueue::cancel(uint64_t p_request_id) {
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

bool OsgImageDecodeQueue::take_result(uint64_t p_request_id, Result &r_result) {
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

OsgImageDecodeQueue::OsgImageDecodeQueue() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}

OsgImageDecodeQueue::~OsgImageDecodeQueue() {
	shutdown();
	if (singleton == this) {
		singleton = nullptr;
	}
}
