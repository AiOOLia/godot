/**************************************************************************/
/*  osg_data_request_queue.h                                              */
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

#include "core/io/http_client.h"
#include "core/object/object_id.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/templates/hash_set.h"
#include "core/templates/list.h"

// Main-thread, non-blocking byte acquisition shared by all tiled nodes.
// HTTP jobs with identical URI and limits are coalesced; parsing remains in
// OsgParseQueue and never runs from this class.
class OsgDataRequestQueue {
public:
	struct Result {
		uint64_t request_id = 0;
		ObjectID owner;
		uint64_t generation = 0;
		String uri;
		Error error = OK;
		String error_message;
		PackedByteArray bytes;
		int response_code = 0;
	};

private:
	struct Subscriber {
		uint64_t request_id = 0;
		ObjectID owner;
		uint64_t generation = 0;
		double priority = 0.0;
	};

	struct Job {
		String key;
		String original_uri;
		String current_uri;
		uint64_t max_bytes = 0;
		double priority = 0.0;
		double timeout_sec = 30.0;
		int retry_limit = 2;
		int retries_used = 0;
		int redirects = 0;
		uint64_t attempt_started_usec = 0;
		uint64_t retry_at_usec = 0;
		Ref<HTTPClient> client;
		String host;
		String request_path;
		int port = 0;
		bool use_tls = false;
		bool started = false;
		bool request_sent = false;
		bool got_response = false;
		int response_code = 0;
		int64_t response_length = -1;
		PackedByteArray body;
		Vector<Subscriber> subscribers;
	};

	struct LocalRequest {
		uint64_t request_id = 0;
		ObjectID owner;
		uint64_t generation = 0;
		String uri;
		uint64_t max_bytes = 0;
		double priority = 0.0;
	};

	static OsgDataRequestQueue *singleton;

	List<Job> jobs;
	List<Result> results;
	Mutex local_mutex;
	Semaphore local_semaphore;
	Thread local_thread;
	List<LocalRequest> local_requests;
	List<Result> local_results;
	HashSet<uint64_t> cancelled_local_requests;
	uint64_t local_active_request_id = 0;
	uint64_t next_request_id = 1;
	int max_active_requests = 0;
	bool shutting_down = false;
	bool local_exit_requested = false;

	static String _make_key(const String &p_uri, uint64_t p_max_bytes);
	static String _get_header(const List<String> &p_headers, const String &p_name);
	Error _start_attempt(Job &r_job, const String &p_uri, String &r_error);
	bool _process_response_headers(Job &r_job);
	void _schedule_retry(Job &r_job, const String &p_message);
	void _complete(Job &p_job, Error p_error, const String &p_message);
	bool _poll_job(Job &r_job);
	int _get_active_count() const;
	static void _local_thread_func(void *p_userdata);
	void _local_thread_loop();
	void _poll_local_results();

public:
	static OsgDataRequestQueue *get_singleton() { return singleton; }

	uint64_t request(ObjectID p_owner, uint64_t p_generation, const String &p_uri, uint64_t p_max_bytes, double p_timeout_sec, int p_retry_count, double p_priority = 0.0);
	void update_priority(uint64_t p_request_id, double p_priority);
	void cancel(uint64_t p_request_id);
	bool take_result(uint64_t p_request_id, Result &r_result);
	void poll();
	void shutdown();

	OsgDataRequestQueue();
	~OsgDataRequestQueue();
};
