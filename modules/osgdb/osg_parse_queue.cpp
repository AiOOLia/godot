/**************************************************************************/
/*  osg_parse_queue.cpp                                                   */
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

#include "osg_parse_queue.h"

#include "core/error/error_macros.h"

OsgParseQueue *OsgParseQueue::singleton = nullptr;

void OsgParseQueue::_thread_func(void *p_userdata) {
	OsgParseQueue *queue = static_cast<OsgParseQueue *>(p_userdata);
	queue->_thread_loop();
}

void OsgParseQueue::_thread_loop() {
	Thread::set_name("OSGDB Parse");
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
		result.source_uri = request.source_uri;
		result.error = OsgSceneConverter::parse_bytes(request.bytes, request.source_uri, request.options, result.document, result.error_message);

		// request and every temporary OSG reference owned by parse_bytes are
		// destroyed here on the parser thread, before publishing CPU-only data.
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

Error OsgParseQueue::start() {
	ERR_FAIL_COND_V(thread.is_started(), ERR_ALREADY_IN_USE);
	exit_requested = false;
	const Thread::ID id = thread.start(_thread_func, this);
	return id == Thread::UNASSIGNED_ID ? ERR_CANT_CREATE : OK;
}

void OsgParseQueue::shutdown() {
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

uint64_t OsgParseQueue::enqueue(ObjectID p_owner, uint64_t p_generation, const String &p_source_uri, const PackedByteArray &p_bytes, const OsgSceneConvertOptions &p_options, double p_priority) {
	ERR_FAIL_COND_V(!thread.is_started(), 0);
	ERR_FAIL_COND_V(p_bytes.is_empty(), 0);

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
		request.source_uri = p_source_uri;
		request.bytes = p_bytes;
		request.options = p_options;
		request.priority = p_priority;
		requests.push_back(request);
	}
	semaphore.post();
	return request.request_id;
}

void OsgParseQueue::update_priority(uint64_t p_request_id, double p_priority) {
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

void OsgParseQueue::cancel(uint64_t p_request_id) {
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

bool OsgParseQueue::take_result(uint64_t p_request_id, Result &r_result) {
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

OsgParseQueue::OsgParseQueue() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}

OsgParseQueue::~OsgParseQueue() {
	shutdown();
	if (singleton == this) {
		singleton = nullptr;
	}
}
