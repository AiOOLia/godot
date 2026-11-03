/**************************************************************************/
/*  tiled_mesh_mesh_pack_queue.h                                         */
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

#include "tiled_mesh_format.h"

#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/templates/hash_set.h"
#include "core/templates/list.h"
#include "servers/rendering/rendering_server_types.h"

struct TiledMeshPickSurface {
	bool points = false;
	PackedVector3Array vertices;
	PackedInt32Array indices;
	PackedInt32Array batch_ids;
	int batch_table = -1;
};

struct TiledMeshPackedSurface {
	bool points = false;
	RenderingServerTypes::SurfaceData surface_data;
	PackedVector3Array vertices;
	PackedInt32Array indices;
	PackedInt32Array batch_ids;
	int batch_table = -1;
	int material = -1;
	bool uses_vertex_color = false;
	uint64_t estimated_bytes = 0;
};

struct TiledMeshPackedRenderGroup {
	int replacement_group = -1;
	int lod_branch = -1;
	Vector<TiledMeshPackedSurface> surfaces;
	AABB aabb;
	uint64_t estimated_bytes = 0;
};

struct TiledMeshPackedDocument {
	// The format-neutral metadata remains intact, but surfaces are removed once
	// their RenderingServer CPU buffers have been generated.
	TiledMeshDocument document;
	Vector<TiledMeshPackedRenderGroup> render_groups;
	uint64_t packed_bytes = 0;
};

class TiledMeshMeshPackQueue {
public:
	struct Result {
		uint64_t request_id = 0;
		ObjectID owner;
		uint64_t generation = 0;
		Error error = OK;
		String error_message;
		TiledMeshPackedDocument document;
	};

private:
	struct Request {
		uint64_t request_id = 0;
		ObjectID owner;
		uint64_t generation = 0;
		TiledMeshDocument document;
		double priority = 0.0;
		uint64_t estimated_bytes = 0;
	};

	static TiledMeshMeshPackQueue *singleton;

	mutable Mutex mutex;
	Semaphore semaphore;
	Thread thread;
	List<Request> requests;
	List<Result> results;
	HashSet<uint64_t> cancelled_requests;
	uint64_t active_request_id = 0;
	uint64_t active_request_bytes = 0;
	uint64_t pending_request_bytes = 0;
	uint64_t next_request_id = 1;
	bool exit_requested = false;

	static void _thread_func(void *p_userdata);
	void _thread_loop();
	bool _request_was_cancelled(uint64_t p_request_id) const;
	Error _pack_document(const Request &p_request, TiledMeshPackedDocument &r_document, String &r_error) const;

public:
	static TiledMeshMeshPackQueue *get_singleton() { return singleton; }

	Error start();
	void shutdown();
	uint64_t enqueue(ObjectID p_owner, uint64_t p_generation, const TiledMeshDocument &p_document, double p_priority = 0.0);
	void update_priority(uint64_t p_request_id, double p_priority);
	void cancel(uint64_t p_request_id);
	bool take_result(uint64_t p_request_id, Result &r_result);
	bool take_next_result_for_testing(Result &r_result);
	int get_pending_count() const;
	int get_active_count() const;
	uint64_t get_in_flight_bytes() const;

	TiledMeshMeshPackQueue();
	~TiledMeshMeshPackQueue();
};
