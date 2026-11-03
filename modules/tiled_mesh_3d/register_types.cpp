/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "tiled_mesh_data_request_queue.h"
#include "tiled_mesh_format.h"
#include "tiled_mesh_image_decode_queue.h"
#include "tiled_mesh_instance_3d.h"
#include "tiled_mesh_mesh_pack_queue.h"
#include "tiled_origin_controller_3d.h"

#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "editor/plugins/editor_plugin.h"
#include "editor/tiled_mesh_instance_3d_editor_plugin.h"
#endif

static TiledMeshDataRequestQueue *tiled_mesh_data_request_queue = nullptr;
static TiledMeshImageDecodeQueue *tiled_mesh_image_decode_queue = nullptr;
static TiledMeshMeshPackQueue *tiled_mesh_mesh_pack_queue = nullptr;

void initialize_tiled_mesh_3d_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		GDREGISTER_ABSTRACT_CLASS(TiledMeshFormatOptions);
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		tiled_mesh_data_request_queue = memnew(TiledMeshDataRequestQueue);
		tiled_mesh_image_decode_queue = memnew(TiledMeshImageDecodeQueue);
		tiled_mesh_mesh_pack_queue = memnew(TiledMeshMeshPackQueue);
		if (tiled_mesh_image_decode_queue->start() != OK) {
			ERR_PRINT("TiledMesh3D: Unable to start the image decoder thread.");
		}
		if (tiled_mesh_mesh_pack_queue->start() != OK) {
			ERR_PRINT("TiledMesh3D: Unable to start the mesh packing thread.");
		}
		GDREGISTER_CLASS(TiledMeshInstance3D);
		GDREGISTER_CLASS(TiledOriginController3D);
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_VIRTUAL_CLASS(TiledMeshInstance3DEditorPlugin);
		EditorPlugins::add_by_type<TiledMeshInstance3DEditorPlugin>();
	}
#endif
}

void uninitialize_tiled_mesh_3d_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	if (tiled_mesh_mesh_pack_queue) {
		memdelete(tiled_mesh_mesh_pack_queue);
		tiled_mesh_mesh_pack_queue = nullptr;
	}
	if (tiled_mesh_image_decode_queue) {
		memdelete(tiled_mesh_image_decode_queue);
		tiled_mesh_image_decode_queue = nullptr;
	}
	if (tiled_mesh_data_request_queue) {
		memdelete(tiled_mesh_data_request_queue);
		tiled_mesh_data_request_queue = nullptr;
	}
}
