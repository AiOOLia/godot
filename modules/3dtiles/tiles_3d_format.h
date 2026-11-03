/**************************************************************************/
/*  tiles_3d_format.h                                                     */
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

#include "modules/tiled_mesh_3d/tiled_mesh_format.h"

class Tiles3DOptions : public TiledMeshFormatOptions {
	GDCLASS(Tiles3DOptions, TiledMeshFormatOptions);

public:
	enum CoordinateSystem {
		COORDINATE_SYSTEM_AUTO,
		COORDINATE_SYSTEM_CARTESIAN,
		COORDINATE_SYSTEM_ECEF,
	};

private:
	CoordinateSystem coordinate_system = COORDINATE_SYSTEM_AUTO;
	int64_t ion_asset_id = 0;
	String ion_access_token;

protected:
	static void _bind_methods();

public:
	void set_ion_asset_id(int64_t p_id);
	int64_t get_ion_asset_id() const { return ion_asset_id; }
	void set_ion_access_token(const String &p_token);
	String get_ion_access_token() const { return ion_access_token; }
	String get_source_uri(const String &p_uri) const override;
	String get_source_bearer_token() const override { return ion_asset_id > 0 ? ion_access_token : String(); }
	void set_coordinate_system(CoordinateSystem p_system);
	CoordinateSystem get_coordinate_system() const { return coordinate_system; }
	StringName get_format_id() const override { return SNAME("3dtiles"); }
};

VARIANT_ENUM_CAST(Tiles3DOptions::CoordinateSystem);

struct Tiles3DIonEndpoint {
	String url;
	String access_token;
	Error parse(const PackedByteArray &p_bytes, String &r_error);
};

class Tiles3DFormatLoader : public TiledMeshFormatLoader {
public:
	StringName get_format_id() const override { return SNAME("3dtiles"); }
	Vector<String> get_recognized_extensions() const override;
	int recognize(const String &p_uri, const PackedByteArray &p_bytes) const override;
	bool supports_options(const Ref<TiledMeshFormatOptions> &p_options) const override;
	TiledMeshFormatSession *create_session(const Ref<TiledMeshFormatOptions> &p_options, const TiledMeshSessionOptions &p_session_options, String &r_error) override;
};
