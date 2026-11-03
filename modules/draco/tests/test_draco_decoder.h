/**************************************************************************/
/*  test_draco_decoder.h                                                  */
/**************************************************************************/

#pragma once

#include "core/io/file_access.h"
#include "tests/test_macros.h"

#include "modules/draco/draco_decoder.h"

namespace TestDracoDecoder {

TEST_CASE("[Draco] Empty and malformed input is rejected") {
	DracoDecoder::Geometry geometry;
	String error;
	CHECK(DracoDecoder::decode(nullptr, 0, geometry, error) == ERR_INVALID_PARAMETER);
	CHECK(!error.is_empty());

	const uint8_t malformed[] = { 'D', 'R', 'A', 'C', 'O' };
	CHECK(DracoDecoder::decode(malformed, sizeof(malformed), geometry, error) == ERR_FILE_CORRUPT);
	CHECK(!error.is_empty());
}

TEST_CASE("[Draco] Input byte limit is enforced") {
	const uint8_t input[] = { 0 };
	DracoDecoder::Geometry geometry;
	DracoDecoder::Limits limits;
	limits.max_input_bytes = 0;
	String error;
	CHECK(DracoDecoder::decode(input, sizeof(input), geometry, error, limits) == ERR_OUT_OF_MEMORY);
	CHECK(!error.is_empty());
}

TEST_CASE("[Draco] Official mesh fixture decodes attributes by unique ID") {
	const PackedByteArray bytes = FileAccess::get_file_as_bytes("modules/draco/tests/data/box.drc");
	REQUIRE(bytes.size() == 152);

	DracoDecoder::Geometry geometry;
	String error;
	const Error result = DracoDecoder::decode(bytes.ptr(), bytes.size(), geometry, error);
	INFO(std::string(error.utf8().get_data()));
	REQUIRE(result == OK);
	CHECK(error.is_empty());
	CHECK(geometry.is_mesh);
	CHECK(geometry.point_count == 24);
	CHECK(geometry.face_count == 12);
	CHECK(geometry.indices.size() == 36);
	REQUIRE(geometry.attributes.size() == 2);

	for (const KeyValue<int, DracoDecoder::Attribute> &entry : geometry.attributes) {
		const DracoDecoder::Attribute *by_unique_id = geometry.attributes.getptr(entry.value.unique_id);
		REQUIRE(by_unique_id != nullptr);
		CHECK(by_unique_id == &entry.value);
		CHECK(entry.value.component_count > 0);
		CHECK(entry.value.values.size() == geometry.point_count * entry.value.component_count);
		CHECK(entry.value.component_type > 0);
		CHECK(entry.value.semantic >= 0);
	}
}

TEST_CASE("[Draco] Official point cloud fixture decodes without mesh indices") {
	const PackedByteArray bytes = FileAccess::get_file_as_bytes("modules/draco/tests/data/point_cloud_no_qp.drc");
	REQUIRE(bytes.size() == 345);

	DracoDecoder::Geometry geometry;
	String error;
	REQUIRE(DracoDecoder::decode(bytes.ptr(), bytes.size(), geometry, error) == OK);
	CHECK_FALSE(geometry.is_mesh);
	CHECK(geometry.point_count > 0);
	CHECK(geometry.face_count == 0);
	CHECK(geometry.indices.is_empty());
	CHECK(geometry.attributes.size() == 2);
}

TEST_CASE("[Draco] Decoded geometry resource limits are enforced") {
	const PackedByteArray bytes = FileAccess::get_file_as_bytes("modules/draco/tests/data/box.drc");
	REQUIRE_FALSE(bytes.is_empty());
	DracoDecoder::Geometry geometry;
	String error;
	DracoDecoder::Limits limits;

	limits.max_points = 23;
	CHECK(DracoDecoder::decode(bytes.ptr(), bytes.size(), geometry, error, limits) == ERR_OUT_OF_MEMORY);
	CHECK(error.contains("point count"));

	limits = DracoDecoder::Limits();
	limits.max_faces = 11;
	CHECK(DracoDecoder::decode(bytes.ptr(), bytes.size(), geometry, error, limits) == ERR_OUT_OF_MEMORY);
	CHECK(error.contains("face count"));

	limits = DracoDecoder::Limits();
	limits.max_attribute_components = 1;
	CHECK(DracoDecoder::decode(bytes.ptr(), bytes.size(), geometry, error, limits) == ERR_OUT_OF_MEMORY);
	CHECK(error.contains("attributes"));
}

TEST_CASE("[Draco] Truncated official stream is rejected") {
	PackedByteArray bytes = FileAccess::get_file_as_bytes("modules/draco/tests/data/box.drc");
	REQUIRE(bytes.size() > 32);
	bytes.resize(32);
	DracoDecoder::Geometry geometry;
	String error;
	CHECK(DracoDecoder::decode(bytes.ptr(), bytes.size(), geometry, error) == ERR_FILE_CORRUPT);
	CHECK_FALSE(error.is_empty());
}

} // namespace TestDracoDecoder
