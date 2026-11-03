/**************************************************************************/
/*  osg_tiled_mesh_format.h                                               */
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

#include "core/templates/hash_set.h"

#include "modules/tiled_mesh_3d/tiled_mesh_format.h"

class OsgTiledMeshOptions : public TiledMeshFormatOptions {
	GDCLASS(OsgTiledMeshOptions, TiledMeshFormatOptions);

public:
	enum UpAxis {
		UP_AXIS_Z,
		UP_AXIS_Y,
	};

private:
	UpAxis up_axis = UP_AXIS_Z;

protected:
	static void _bind_methods();

public:
	void set_up_axis(UpAxis p_axis);
	UpAxis get_up_axis() const;
	virtual StringName get_format_id() const override { return SNAME("osg"); }
};

VARIANT_ENUM_CAST(OsgTiledMeshOptions::UpAxis);

class OsgTiledMeshSession : public TiledMeshFormatSession {
	bool osg_z_up = true;
	TiledMeshSessionOptions session_options;
	HashSet<uint64_t> requests;
	bool shutting_down = false;

public:
	virtual uint64_t enqueue_parse(const TiledMeshParseRequest &p_request, double p_priority) override;
	virtual void update_priority(uint64_t p_request_id, double p_priority) override;
	virtual void cancel(uint64_t p_request_id) override;
	virtual bool take_result(uint64_t p_request_id, TiledMeshParseResult &r_result) override;
	virtual void shutdown() override;

	OsgTiledMeshSession(bool p_osg_z_up, const TiledMeshSessionOptions &p_options);
	~OsgTiledMeshSession();
};

class OsgTiledMeshFormatLoader : public TiledMeshFormatLoader {
public:
	virtual StringName get_format_id() const override { return SNAME("osg"); }
	virtual Vector<String> get_recognized_extensions() const override;
	virtual int recognize(const String &p_uri, const PackedByteArray &p_bytes) const override;
	virtual bool supports_options(const Ref<TiledMeshFormatOptions> &p_options) const override;
	virtual TiledMeshFormatSession *create_session(const Ref<TiledMeshFormatOptions> &p_options, const TiledMeshSessionOptions &p_session_options, String &r_error) override;
};
