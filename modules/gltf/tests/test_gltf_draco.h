/**************************************************************************/
/*  test_gltf_draco.h                                                     */
/**************************************************************************/

#pragma once

#include "tests/test_macros.h"

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_DRACO_ENABLED

#include "modules/gltf/gltf_document.h"
#include "modules/gltf/gltf_state.h"

namespace TestGltfDraco {

TEST_CASE("[GLTF] KHR_draco_mesh_compression imports official binary fixture") {
	Ref<GLTFDocument> document;
	document.instantiate();
	Ref<GLTFState> state;
	state.instantiate();

	REQUIRE(document->append_from_file("modules/draco/tests/data/Box_Draco.glb", state) == OK);
	CHECK(state->get_meshes().size() == 1);
	CHECK(state->get_nodes().size() == 2);
	CHECK(state->get_materials().size() == 1);
}

} // namespace TestGltfDraco

#endif // MODULE_DRACO_ENABLED
