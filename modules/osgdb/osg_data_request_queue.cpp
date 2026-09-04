/**************************************************************************/
/*  osg_data_request_queue.cpp                                            */
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

#include "osg_data_request_queue.h"

#include "osg_data_source.h"

#include "core/os/os.h"
#include "core/os/time.h"

void OsgDataRequestQueue::_local_thread_func(void *p_userdata) {
	static_cast<OsgDataRequestQueue *>(p_userdata)->_local_thread_loop();
}

void OsgDataRequestQueue::_local_thread_loop() {
	Thread::set_name("OSGDB Data");
	for (;;) {
		local_semaphore.wait();
		LocalRequest request;
		{
			MutexLock lock(local_mutex);
			if (local_exit_requested) {
				return;
			}
			if (local_requests.is_empty()) {
				continue;
			}
			List<LocalRequest>::Element *best = local_requests.front();
			for (List<LocalRequest>::Element *element = best->next(); element; element = element->next()) {
				if (element->get().priority > best->get().priority) {
					best = element;
				}
			}
			request = best->get();
			local_requests.erase(best);
			local_active_request_id = request.request_id;
		}
		Result result;
		result.request_id = request.request_id;
		result.owner = request.owner;
		result.generation = request.generation;
		result.uri = request.uri;
		result.error = OsgDataSource::load_local_bytes(request.uri, request.max_bytes, result.bytes, result.error_message);
		{
			MutexLock lock(local_mutex);
			local_active_request_id = 0;
			if (cancelled_local_requests.has(request.request_id)) {
				cancelled_local_requests.erase(request.request_id);
				continue;
			}
			if (!local_exit_requested) {
				local_results.push_back(result);
			}
		}
	}
}

void OsgDataRequestQueue::_poll_local_results() {
	MutexLock lock(local_mutex);
	for (List<Result>::Element *element = local_results.front(); element;) {
		List<Result>::Element *next = element->next();
		results.push_back(element->get());
		local_results.erase(element);
		element = next;
	}
}

OsgDataRequestQueue *OsgDataRequestQueue::singleton = nullptr;

String OsgDataRequestQueue::_make_key(const String &p_uri, uint64_t p_max_bytes) {
	return OsgDataSource::normalize_uri(p_uri) + "|" + itos(p_max_bytes);
}

String OsgDataRequestQueue::_get_header(const List<String> &p_headers, const String &p_name) {
	const String expected = p_name.to_lower();
	for (const String &header : p_headers) {
		const int separator = header.find_char(':');
		if (separator > 0 && header.substr(0, separator).strip_edges().to_lower() == expected) {
			return header.substr(separator + 1).strip_edges();
		}
	}
	return String();
}

Error OsgDataRequestQueue::_start_attempt(Job &r_job, const String &p_uri, String &r_error) {
	r_error.clear();
	r_job.current_uri = OsgDataSource::normalize_uri(p_uri);
	String scheme;
	String fragment;
	Error err = r_job.current_uri.parse_url(scheme, r_job.host, r_job.port, r_job.request_path, fragment);
	if (err != OK || (scheme != "http://" && scheme != "https://")) {
		r_error = vformat("Invalid HTTP URI '%s'.", r_job.current_uri);
		return ERR_INVALID_PARAMETER;
	}
	r_job.use_tls = scheme == "https://";
	if (r_job.port == 0) {
		r_job.port = r_job.use_tls ? 443 : 80;
	}
	if (r_job.request_path.is_empty()) {
		r_job.request_path = "/";
	}
	r_job.client = Ref<HTTPClient>(HTTPClient::create());
	if (r_job.client.is_null()) {
		r_error = "Unable to create HTTPClient.";
		return ERR_CANT_CREATE;
	}
	r_job.client->set_blocking_mode(false);
	r_job.client->set_read_chunk_size(64 * 1024);
	err = r_job.client->connect_to_host(r_job.host, r_job.port, r_job.use_tls ? TLSOptions::client() : Ref<TLSOptions>());
	if (err != OK) {
		r_job.client.unref();
		r_error = vformat("Unable to connect to '%s' (Error %d).", r_job.current_uri, int(err));
		return err;
	}
	r_job.started = true;
	r_job.request_sent = false;
	r_job.got_response = false;
	r_job.response_code = 0;
	r_job.response_length = -1;
	r_job.body.clear();
	r_job.attempt_started_usec = Time::get_singleton()->get_ticks_usec();
	r_job.retry_at_usec = 0;
	return OK;
}

bool OsgDataRequestQueue::_process_response_headers(Job &r_job) {
	r_job.response_code = r_job.client->get_response_code();
	List<String> headers;
	r_job.client->get_response_headers(&headers);
	if (r_job.response_code >= 300 && r_job.response_code < 400) {
		const String location = _get_header(headers, "location");
		if (location.is_empty() || r_job.redirects >= 5) {
			_complete(r_job, ERR_CANT_OPEN, vformat("HTTP redirect failed for '%s'.", r_job.current_uri));
			return true;
		}
		const String redirected_uri = OsgDataSource::resolve_uri(r_job.current_uri, location);
		if (redirected_uri.is_empty() || !OsgDataSource::is_http_uri(redirected_uri)) {
			_complete(r_job, ERR_CANT_OPEN, vformat("HTTP redirect escapes the source root for '%s'.", r_job.current_uri));
			return true;
		}
		r_job.redirects++;
		r_job.client->close();
		r_job.client.unref();
		r_job.started = false;
		r_job.current_uri = redirected_uri;
		r_job.retry_at_usec = 0;
		return false;
	}
	if (r_job.response_code >= 400 && r_job.response_code < 500) {
		_complete(r_job, ERR_FILE_CANT_OPEN, vformat("HTTP %d while loading '%s'.", r_job.response_code, r_job.current_uri));
		return true;
	}
	if (r_job.response_code >= 500) {
		_schedule_retry(r_job, vformat("HTTP %d while loading '%s'.", r_job.response_code, r_job.current_uri));
		return r_job.subscribers.is_empty();
	}
	r_job.got_response = true;
	r_job.response_length = r_job.client->get_response_body_length();
	if (r_job.max_bytes > 0 && r_job.response_length > int64_t(r_job.max_bytes)) {
		_complete(r_job, ERR_OUT_OF_MEMORY, vformat("HTTP response for '%s' exceeds the %d byte tile limit.", r_job.current_uri, r_job.max_bytes));
		return true;
	}
	if (r_job.response_length == 0) {
		_complete(r_job, OK, String());
		return true;
	}
	return false;
}

void OsgDataRequestQueue::_schedule_retry(Job &r_job, const String &p_message) {
	if (r_job.retries_used >= r_job.retry_limit) {
		_complete(r_job, ERR_CONNECTION_ERROR, p_message);
		return;
	}
	r_job.retries_used++;
	if (r_job.client.is_valid()) {
		r_job.client->close();
	}
	r_job.client.unref();
	r_job.started = false;
	r_job.request_sent = false;
	r_job.got_response = false;
	r_job.body.clear();
	const uint64_t delay_usec = r_job.retries_used == 1 ? 500000 : 1000000;
	r_job.retry_at_usec = Time::get_singleton()->get_ticks_usec() + delay_usec;
}

void OsgDataRequestQueue::_complete(Job &p_job, Error p_error, const String &p_message) {
	if (p_job.client.is_valid()) {
		p_job.client->close();
	}
	for (const Subscriber &subscriber : p_job.subscribers) {
		Result result;
		result.request_id = subscriber.request_id;
		result.owner = subscriber.owner;
		result.generation = subscriber.generation;
		result.uri = p_job.original_uri;
		result.error = p_error;
		result.error_message = p_message;
		result.response_code = p_job.response_code;
		if (p_error == OK) {
			result.bytes = p_job.body;
		}
		results.push_back(result);
	}
	p_job.subscribers.clear();
}

int OsgDataRequestQueue::_get_active_count() const {
	int count = 0;
	for (const Job &job : jobs) {
		count += job.started ? 1 : 0;
	}
	return count;
}

bool OsgDataRequestQueue::_poll_job(Job &r_job) {
	const uint64_t now = Time::get_singleton()->get_ticks_usec();
	if (!r_job.started) {
		if (r_job.retry_at_usec > now || _get_active_count() >= max_active_requests) {
			return false;
		}
		for (const Job &job : jobs) {
			if (&job != &r_job && !job.started && job.retry_at_usec <= now && job.priority > r_job.priority) {
				return false;
			}
		}
		String start_error;
		const Error err = _start_attempt(r_job, r_job.current_uri, start_error);
		if (err != OK) {
			_schedule_retry(r_job, start_error);
		}
		return r_job.subscribers.is_empty();
	}

	if (r_job.timeout_sec > 0.0 && now - r_job.attempt_started_usec > uint64_t(r_job.timeout_sec * 1000000.0)) {
		_schedule_retry(r_job, vformat("Request for '%s' timed out.", r_job.current_uri));
		return r_job.subscribers.is_empty();
	}

	const HTTPClient::Status status = r_job.client->get_status();
	switch (status) {
		case HTTPClient::STATUS_RESOLVING:
		case HTTPClient::STATUS_CONNECTING:
		case HTTPClient::STATUS_REQUESTING: {
			const Error poll_error = r_job.client->poll();
			if (poll_error != OK) {
				_schedule_retry(r_job, vformat("Network error while requesting '%s' (Error %d).", r_job.current_uri, int(poll_error)));
			}
		} break;
		case HTTPClient::STATUS_CONNECTED: {
			if (!r_job.request_sent) {
				Vector<String> headers;
				headers.push_back("Accept: application/octet-stream,*/*;q=0.8");
				headers.push_back("Accept-Encoding: identity");
				const Error request_error = r_job.client->request(HTTPClient::METHOD_GET, r_job.request_path, headers, nullptr, 0);
				if (request_error != OK) {
					_schedule_retry(r_job, vformat("Unable to send GET for '%s' (Error %d).", r_job.current_uri, int(request_error)));
				} else {
					r_job.request_sent = true;
				}
				break;
			}
			if (!r_job.got_response) {
				if (!r_job.client->has_response()) {
					_schedule_retry(r_job, vformat("No HTTP response was received for '%s'.", r_job.current_uri));
					break;
				}
				if (_process_response_headers(r_job)) {
					return true;
				}
			}
		} break;
		case HTTPClient::STATUS_BODY: {
			if (!r_job.got_response) {
				if (_process_response_headers(r_job)) {
					return true;
				}
				if (!r_job.got_response) {
					return false;
				}
			}
			const Error poll_error = r_job.client->poll();
			if (poll_error != OK) {
				_schedule_retry(r_job, vformat("Network error while reading '%s' (Error %d).", r_job.current_uri, int(poll_error)));
				return r_job.subscribers.is_empty();
			}
			if (r_job.client->get_status() == HTTPClient::STATUS_BODY) {
				const PackedByteArray chunk = r_job.client->read_response_body_chunk();
				if (r_job.max_bytes > 0 && uint64_t(r_job.body.size()) + uint64_t(chunk.size()) > r_job.max_bytes) {
					_complete(r_job, ERR_OUT_OF_MEMORY, vformat("HTTP response for '%s' exceeds the %d byte tile limit.", r_job.current_uri, r_job.max_bytes));
					return true;
				}
				r_job.body.append_array(chunk);
			}
			if (r_job.response_length >= 0 && r_job.body.size() == r_job.response_length) {
				_complete(r_job, OK, String());
				return true;
			}
		} break;
		case HTTPClient::STATUS_CANT_RESOLVE:
		case HTTPClient::STATUS_CANT_CONNECT:
		case HTTPClient::STATUS_CONNECTION_ERROR:
		case HTTPClient::STATUS_TLS_HANDSHAKE_ERROR:
		case HTTPClient::STATUS_DISCONNECTED: {
			if (r_job.got_response && r_job.response_length < 0) {
				_complete(r_job, OK, String());
				return true;
			}
			if (r_job.got_response && r_job.response_length >= 0 && r_job.body.size() != r_job.response_length) {
				_schedule_retry(r_job, vformat("HTTP response for '%s' was truncated (%d of %d bytes).", r_job.current_uri, r_job.body.size(), r_job.response_length));
				return r_job.subscribers.is_empty();
			}
			_schedule_retry(r_job, vformat("Connection failed while loading '%s'.", r_job.current_uri));
		} break;
	}
	return r_job.subscribers.is_empty();
}

uint64_t OsgDataRequestQueue::request(ObjectID p_owner, uint64_t p_generation, const String &p_uri, uint64_t p_max_bytes, double p_timeout_sec, int p_retry_count, double p_priority) {
	ERR_FAIL_COND_V(shutting_down, 0);
	const String uri = OsgDataSource::normalize_uri(p_uri);
	ERR_FAIL_COND_V(uri.is_empty(), 0);
	Subscriber subscriber;
	subscriber.request_id = next_request_id++;
	if (next_request_id == 0) {
		next_request_id = 1;
	}
	subscriber.owner = p_owner;
	subscriber.generation = p_generation;
	subscriber.priority = p_priority;

	if (!OsgDataSource::is_http_uri(uri)) {
		LocalRequest request;
		request.request_id = subscriber.request_id;
		request.owner = p_owner;
		request.generation = p_generation;
		request.uri = uri;
		request.max_bytes = p_max_bytes;
		request.priority = p_priority;
		if (!local_thread.is_started()) {
			Result result;
			result.request_id = subscriber.request_id;
			result.owner = p_owner;
			result.generation = p_generation;
			result.uri = uri;
			result.error = OsgDataSource::load_local_bytes(uri, p_max_bytes, result.bytes, result.error_message);
			results.push_back(result);
			return subscriber.request_id;
		}
		{
			MutexLock lock(local_mutex);
			if (local_exit_requested) {
				return 0;
			}
			local_requests.push_back(request);
		}
		local_semaphore.post();
		return subscriber.request_id;
	}

	const String key = _make_key(uri, p_max_bytes);
	for (Job &job : jobs) {
		if (job.key == key) {
			job.subscribers.push_back(subscriber);
			job.priority = MAX(job.priority, p_priority);
			return subscriber.request_id;
		}
	}
	Job job;
	job.key = key;
	job.original_uri = uri;
	job.current_uri = uri;
	job.max_bytes = p_max_bytes;
	job.priority = p_priority;
	job.timeout_sec = MAX(p_timeout_sec, 0.1);
	job.retry_limit = MAX(p_retry_count, 0);
	job.subscribers.push_back(subscriber);
	jobs.push_back(job);
	return subscriber.request_id;
}

void OsgDataRequestQueue::update_priority(uint64_t p_request_id, double p_priority) {
	if (p_request_id == 0) {
		return;
	}
	for (Job &job : jobs) {
		for (Subscriber &subscriber : job.subscribers) {
			if (subscriber.request_id == p_request_id) {
				subscriber.priority = p_priority;
				job.priority = job.subscribers[0].priority;
				for (const Subscriber &current : job.subscribers) {
					job.priority = MAX(job.priority, current.priority);
				}
				return;
			}
		}
	}
	MutexLock lock(local_mutex);
	for (LocalRequest &request : local_requests) {
		if (request.request_id == p_request_id) {
			request.priority = p_priority;
			return;
		}
	}
}

void OsgDataRequestQueue::cancel(uint64_t p_request_id) {
	if (p_request_id == 0) {
		return;
	}
	for (List<Result>::Element *element = results.front(); element;) {
		List<Result>::Element *next = element->next();
		if (element->get().request_id == p_request_id) {
			results.erase(element);
			return;
		}
		element = next;
	}
	for (List<Job>::Element *element = jobs.front(); element;) {
		List<Job>::Element *next = element->next();
		Job &job = element->get();
		for (int i = job.subscribers.size() - 1; i >= 0; i--) {
			if (job.subscribers[i].request_id == p_request_id) {
				job.subscribers.remove_at(i);
			}
		}
		if (job.subscribers.is_empty()) {
			if (job.client.is_valid()) {
				job.client->close();
			}
			jobs.erase(element);
			return;
		}
		job.priority = job.subscribers[0].priority;
		for (const Subscriber &subscriber : job.subscribers) {
			job.priority = MAX(job.priority, subscriber.priority);
		}
		element = next;
	}
	{
		MutexLock lock(local_mutex);
		for (List<Result>::Element *element = local_results.front(); element;) {
			List<Result>::Element *next = element->next();
			if (element->get().request_id == p_request_id) {
				local_results.erase(element);
				return;
			}
			element = next;
		}
		for (List<LocalRequest>::Element *element = local_requests.front(); element;) {
			List<LocalRequest>::Element *next = element->next();
			if (element->get().request_id == p_request_id) {
				local_requests.erase(element);
				return;
			}
			element = next;
		}
		if (local_active_request_id == p_request_id) {
			cancelled_local_requests.insert(p_request_id);
		}
	}
}

bool OsgDataRequestQueue::take_result(uint64_t p_request_id, Result &r_result) {
	for (List<Result>::Element *element = results.front(); element; element = element->next()) {
		if (element->get().request_id == p_request_id) {
			r_result = element->get();
			results.erase(element);
			return true;
		}
	}
	return false;
}

void OsgDataRequestQueue::poll() {
	if (shutting_down) {
		return;
	}
	_poll_local_results();
	for (List<Job>::Element *element = jobs.front(); element;) {
		List<Job>::Element *next = element->next();
		if (_poll_job(element->get())) {
			jobs.erase(element);
		}
		element = next;
	}
}

void OsgDataRequestQueue::shutdown() {
	if (local_thread.is_started()) {
		{
			MutexLock lock(local_mutex);
			local_exit_requested = true;
			local_requests.clear();
			local_results.clear();
			cancelled_local_requests.clear();
		}
		local_semaphore.post();
		local_thread.wait_to_finish();
		local_active_request_id = 0;
	}
	shutting_down = true;
	for (Job &job : jobs) {
		if (job.client.is_valid()) {
			job.client->close();
		}
	}
	jobs.clear();
	results.clear();
}

OsgDataRequestQueue::OsgDataRequestQueue() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
	max_active_requests = OS::get_singleton()->has_feature("web") ? 4 : 8;
	local_exit_requested = false;
	if (local_thread.start(_local_thread_func, this) == Thread::UNASSIGNED_ID) {
		WARN_PRINT("OSGDB: Unable to start local data thread; falling back to synchronous local reads.");
	}
}

OsgDataRequestQueue::~OsgDataRequestQueue() {
	shutdown();
	if (singleton == this) {
		singleton = nullptr;
	}
}
