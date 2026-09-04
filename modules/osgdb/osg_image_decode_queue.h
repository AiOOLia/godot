/**************************************************************************/
/*  osg_image_decode_queue.h                                              */
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

#pragma once

#include "core/io/image.h"
#include "core/object/object_id.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/templates/hash_set.h"
#include "core/templates/list.h"

class OsgImageDecodeQueue {
public:
	struct Result {
		uint64_t request_id = 0;
		ObjectID owner;
		uint64_t generation = 0;
		Error error = OK;
		String error_message;
		Ref<Image> image;
	};

private:
	struct Request {
		uint64_t request_id = 0;
		ObjectID owner;
		uint64_t generation = 0;
		String uri;
		PackedByteArray bytes;
	};

	static OsgImageDecodeQueue *singleton;

	Mutex mutex;
	Semaphore semaphore;
	Thread thread;
	List<Request> requests;
	List<Result> results;
	HashSet<uint64_t> cancelled_requests;
	uint64_t active_request_id = 0;
	uint64_t next_request_id = 1;
	bool exit_requested = false;

	static void _thread_func(void *p_userdata);
	void _thread_loop();

public:
	static OsgImageDecodeQueue *get_singleton() { return singleton; }

	Error start();
	void shutdown();
	uint64_t enqueue(ObjectID p_owner, uint64_t p_generation, const String &p_uri, const PackedByteArray &p_bytes);
	void cancel(uint64_t p_request_id);
	bool take_result(uint64_t p_request_id, Result &r_result);

	OsgImageDecodeQueue();
	~OsgImageDecodeQueue();
};
