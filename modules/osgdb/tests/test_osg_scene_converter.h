/**************************************************************************/
/*  test_osg_scene_converter.h                                            */
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

#ifndef OSG_LIBRARY_STATIC
#define OSG_LIBRARY_STATIC
#endif
#ifndef OSGDB_LIBRARY_STATIC
#define OSGDB_LIBRARY_STATIC
#endif

#include "tests/test_macros.h"

#include "../osg_parse_queue.h"
#include "../osg_data_request_queue.h"
#include "../osg_data_source.h"
#include "../osg_image_decode_queue.h"
#include "../osg_scene_converter.h"
#include "../osg_static_init.h"
#include "../tiled_mesh_instance_3d.h"

#include "core/io/file_access.h"
#include "core/io/tcp_server.h"
#include "core/os/os.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/scene_tree.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/LOD>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/PagedLOD>
#include <osg/Texture2D>
#include <osgDB/Options.h>
#include <osgDB/ReaderWriter.h>
#include <osgDB/Registry.h>

#include <cstring>
#include <limits>
#include <sstream>

namespace TestOsgdb {

static osg::ref_ptr<osg::Node> make_test_scene(bool p_include_paged_lod = true, bool p_external_image = true, bool p_include_image = true) {
	osg::ref_ptr<osg::Vec3dArray> vertices = new osg::Vec3dArray;
	vertices->push_back(osg::Vec3d(0.0, 0.0, 0.0));
	vertices->push_back(osg::Vec3d(1.0, 0.0, 0.0));
	vertices->push_back(osg::Vec3d(0.0, 1.0, 0.0));

	osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array(osg::Array::BIND_PER_VERTEX);
	normals->push_back(osg::Vec3(0, 0, 1));
	normals->push_back(osg::Vec3(0, 0, 1));
	normals->push_back(osg::Vec3(0, 0, 1));

	osg::ref_ptr<osg::Vec2Array> uvs = new osg::Vec2Array;
	uvs->push_back(osg::Vec2(0, 0));
	uvs->push_back(osg::Vec2(1, 0));
	uvs->push_back(osg::Vec2(0, 1));

	osg::ref_ptr<osg::DrawElementsUShort> indices = new osg::DrawElementsUShort(GL_TRIANGLES);
	indices->push_back(0);
	indices->push_back(1);
	indices->push_back(2);

	osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
	geometry->setVertexArray(vertices.get());
	geometry->setNormalArray(normals.get(), osg::Array::BIND_PER_VERTEX);
	geometry->setTexCoordArray(0, uvs.get(), osg::Array::BIND_PER_VERTEX);
	geometry->addPrimitiveSet(indices.get());

	osg::ref_ptr<osg::Material> material = new osg::Material;
	material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.25f, 0.5f, 0.75f, 0.5f));
	geometry->getOrCreateStateSet()->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
	geometry->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

	if (p_include_image) {
		osg::ref_ptr<osg::Image> image = new osg::Image;
		image->setFileName("textures/albedo.png");
		if (p_external_image) {
			image->setWriteHint(osg::Image::EXTERNAL_FILE);
		} else {
			// Keep the image intentionally empty while supplying sane format enums,
			// so the writer creates the malformed `Data 1 {}` regression fixture
			// without producing an unrelated pixel-type warning.
			image->setInternalTextureFormat(GL_RGBA8);
			image->setPixelFormat(GL_RGBA);
			image->setDataType(GL_UNSIGNED_BYTE);
		}
		osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(image.get());
		geometry->getOrCreateStateSet()->setTextureAttributeAndModes(0, texture.get(), osg::StateAttribute::ON);
	}

	osg::ref_ptr<osg::Geode> geode = new osg::Geode;
	geode->addDrawable(geometry.get());

	osg::ref_ptr<osg::PagedLOD> paged_lod = new osg::PagedLOD;
	paged_lod->setCenter(osg::Vec3d(0.5, 0.5, 0.0));
	paged_lod->setRadius(2.0f);
	paged_lod->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	paged_lod->setRange(0, 0.0f, 100.0f);
	paged_lod->setFileName(0, "child.osgb");

	osg::ref_ptr<osg::MatrixTransform> root = new osg::MatrixTransform;
	root->setMatrix(osg::Matrixd::translate(1000000.0, 2000000.0, 3000000.0));
	root->addChild(geode.get());
	if (p_include_paged_lod) {
		root->addChild(paged_lod.get());
	}
	return root.release();
}

static PackedByteArray write_test_scene(const osg::Node &p_scene, bool p_ascii, bool p_zlib) {
	osgdb_initialize_static_registry();
	osgDB::ReaderWriter *writer = osgDB::Registry::instance()->getReaderWriterForExtension("osg2");
	REQUIRE(writer != nullptr);
	osg::ref_ptr<osgDB::Options> options = new osgDB::Options;
	options->setPluginStringData("fileType", p_ascii ? "Ascii" : "Binary");
	if (p_zlib) {
		options->setPluginStringData("Compressor", "zlib");
	}
	std::ostringstream output(std::ios::out | std::ios::binary);
	const osgDB::ReaderWriter::WriteResult result = writer->writeNode(p_scene, output, options.get());
	REQUIRE_MESSAGE(result.success(), result.statusMessage());
	const std::string bytes = output.str();
	PackedByteArray packed;
	packed.resize((int)bytes.size());
	memcpy(packed.ptrw(), bytes.data(), bytes.size());
	return packed;
}

static void check_converted_scene(const PackedByteArray &p_bytes, const String &p_uri, bool p_expect_paged_lod = true) {
	OsgSceneConvertOptions options;
	options.osg_z_up = true;
	options.unit_scale = 1.0;
	options.use_root_center = true;
	OsgTileDocument document;
	String error;
	const Error parse_error = OsgSceneConverter::parse_bytes(p_bytes, p_uri, options, document, error);
	const std::string error_utf8 = error.utf8().get_data();
	if (parse_error != OK) {
		CHECK_MESSAGE(false, error_utf8);
		return;
	}
	if (document.surfaces.size() != 1 || document.materials.size() != 1) {
		CHECK(document.surfaces.size() == 1);
		CHECK(document.materials.size() == 1);
		return;
	}
	CHECK(document.surfaces[0].vertices.size() == 3);
	CHECK(document.surfaces[0].indices.size() == 3);
	CHECK(document.surfaces[0].indices[0] == 0);
	CHECK(document.surfaces[0].indices[1] == 2);
	CHECK(document.surfaces[0].indices[2] == 1);
	CHECK(document.surfaces[0].normals[0].is_equal_approx(Vector3(0, 1, 0)));
	CHECK(document.local_aabb.size.is_equal_approx(Vector3(1, 0, 1)));
	CHECK(Math::is_equal_approx(document.source_origin.x, 1000000.5));
	CHECK(Math::is_equal_approx(document.source_origin.y, 2000000.5));
	CHECK(Math::is_equal_approx(document.source_origin.z, 3000000.0));
	CHECK(document.materials[0].transparent);
	CHECK(document.materials[0].double_sided);
	CHECK(document.materials[0].albedo.is_equal_approx(Color(0.25, 0.5, 0.75, 0.5)));
	CHECK(document.materials[0].albedo_image.uri == "res://city/textures/albedo.png");
	REQUIRE(document.external_tiles.size() == (p_expect_paged_lod ? 1 : 0));
	if (p_expect_paged_lod) {
		CHECK(document.external_tiles[0].uri == "res://city/child.osgb");
	}
}

TEST_CASE("[OSGDB] Convert OSG2 ASCII through the .osg compatibility path") {
	osg::ref_ptr<osg::Node> scene = make_test_scene(false);
	check_converted_scene(write_test_scene(*scene, true, false), "res://city/root.osg", false);
}

TEST_CASE("[OSGDB] Recover from an empty inline image in an ASCII stream") {
	// OSG's writer can emit `Data 1 {}` for an image that has a filename but
	// no pixels. The reader must preserve the external URI without consuming
	// the image object's closing bracket as Base64 data.
	osg::ref_ptr<osg::Node> scene = make_test_scene(false, false);
	check_converted_scene(write_test_scene(*scene, true, false), "res://city/root.osg", false);
}

TEST_CASE("[OSGDB] Convert uncompressed OSGB") {
	osg::ref_ptr<osg::Node> scene = make_test_scene();
	check_converted_scene(write_test_scene(*scene, false, false), "res://city/root.osgb");
}

TEST_CASE("[OSGDB] Convert zlib-compressed OSGB") {
	osg::ref_ptr<osg::Node> scene = make_test_scene();
	check_converted_scene(write_test_scene(*scene, false, true), "res://city/root.osgb");
}

TEST_CASE("[OSGDB] Derive bounds for a paging-only root") {
	osg::ref_ptr<osg::PagedLOD> paging_root = new osg::PagedLOD;
	paging_root->setCenter(osg::Vec3d(100.0, 200.0, 300.0));
	paging_root->setRadius(25.0f);
	paging_root->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	paging_root->setRange(0, 0.0f, 1000.0f);
	paging_root->setFileName(0, "tiles/root.osgb");
	OsgSceneConvertOptions options;
	options.use_root_center = true;
	OsgTileDocument document;
	String error;
	const Error parse_error = OsgSceneConverter::parse_bytes(write_test_scene(*paging_root, false, false), "res://city/index.osgb", options, document, error);
	REQUIRE_MESSAGE(parse_error == OK, error.utf8().get_data());
	REQUIRE(document.surfaces.is_empty());
	REQUIRE(document.external_tiles.size() == 1);
	CHECK(Math::is_equal_approx(document.source_origin.x, 100.0));
	CHECK(Math::is_equal_approx(document.source_origin.y, 200.0));
	CHECK(Math::is_equal_approx(document.source_origin.z, 300.0));
	CHECK(document.local_aabb.size.is_equal_approx(Vector3(50, 50, 50)));
}

TEST_CASE("[OSGDB] Unsupported primitives do not affect dataset bounds") {
	osg::ref_ptr<osg::Group> root = new osg::Group;
	root->addChild(make_test_scene(false, false, false).get());

	osg::ref_ptr<osg::Vec3dArray> point_vertices = new osg::Vec3dArray;
	point_vertices->push_back(osg::Vec3d(1.0e12, 1.0e12, 1.0e12));
	osg::ref_ptr<osg::Geometry> point_geometry = new osg::Geometry;
	point_geometry->setVertexArray(point_vertices.get());
	point_geometry->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, 1));
	osg::ref_ptr<osg::Geode> point_geode = new osg::Geode;
	point_geode->addDrawable(point_geometry.get());
	root->addChild(point_geode.get());

	OsgSceneConvertOptions options;
	options.use_root_center = true;
	OsgTileDocument document;
	String error;
	const Error parse_error = OsgSceneConverter::parse_bytes(write_test_scene(*root, false, false), "res://city/root.osgb", options, document, error);
	REQUIRE_MESSAGE(parse_error == OK, error.utf8().get_data());
	REQUIRE(document.surfaces.size() == 1);
	CHECK(Math::is_equal_approx(document.source_origin.x, 1000000.5));
	CHECK(Math::is_equal_approx(document.source_origin.y, 2000000.5));
	CHECK(Math::is_equal_approx(document.source_origin.z, 3000000.0));
	CHECK(document.local_aabb.size.is_equal_approx(Vector3(1, 0, 1)));
	CHECK_FALSE(document.warnings.is_empty());
}

TEST_CASE("[OSGDB] Preserve independent PagedLOD fallback groups") {
	osg::ref_ptr<osg::Group> root = new osg::Group;
	for (int i = 0; i < 2; i++) {
		osg::ref_ptr<osg::PagedLOD> paged = new osg::PagedLOD;
		paged->setCenter(osg::Vec3d(1000000.5 + i * 10.0, 2000000.5, 3000000.0));
		paged->setRadius(5.0f);
		paged->addChild(make_test_scene(false, true, false).get(), 0.0f, 1000.0f);
		paged->setFileName(1, vformat("fine_%d.osgb", i).utf8().get_data());
		paged->setRange(1, 0.0f, 1000.0f);
		root->addChild(paged.get());
	}
	OsgSceneConvertOptions options;
	OsgTileDocument document;
	String error;
	const Error parse_error = OsgSceneConverter::parse_bytes(write_test_scene(*root, false, false), "res://city/root.osgb", options, document, error);
	REQUIRE_MESSAGE(parse_error == OK, error.utf8().get_data());
	REQUIRE(document.surfaces.size() == 2);
	REQUIRE(document.external_tiles.size() == 2);
	CHECK(document.surfaces[0].fallback_group >= 0);
	CHECK(document.surfaces[1].fallback_group >= 0);
	CHECK(document.surfaces[0].fallback_group != document.surfaces[1].fallback_group);
	CHECK(document.external_tiles[0].fallback_group == document.surfaces[0].fallback_group);
	CHECK(document.external_tiles[1].fallback_group == document.surfaces[1].fallback_group);
}

TEST_CASE("[OSGDB] Preserve inline LOD child ranges") {
	osg::ref_ptr<osg::LOD> lod = new osg::LOD;
	lod->setCenter(osg::Vec3d(1000000.5, 2000000.5, 3000000.0));
	lod->setRadius(2.0f);
	lod->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	lod->addChild(make_test_scene(false, true, false).get(), 0.0f, 10.0f);
	lod->addChild(make_test_scene(false, true, false).get(), 10.0f, 1000.0f);

	OsgSceneConvertOptions options;
	OsgTileDocument document;
	String error;
	const Error parse_error = OsgSceneConverter::parse_bytes(write_test_scene(*lod, false, false), "res://city/inline_lod.osgb", options, document, error);
	REQUIRE_MESSAGE(parse_error == OK, error.utf8().get_data());
	REQUIRE(document.surfaces.size() == 2);
	REQUIRE(document.lod_ranges.size() == 2);
	CHECK(document.surfaces[0].lod_branch >= 0);
	CHECK(document.surfaces[1].lod_branch >= 0);
	CHECK(document.surfaces[0].lod_branch != document.surfaces[1].lod_branch);
	CHECK(Math::is_equal_approx(document.lod_ranges[document.surfaces[0].lod_branch].min_range, 0.0f));
	CHECK(Math::is_equal_approx(document.lod_ranges[document.surfaces[0].lod_branch].max_range, 10.0f));
	CHECK(Math::is_equal_approx(document.lod_ranges[document.surfaces[1].lod_branch].min_range, 10.0f));
	CHECK(Math::is_equal_approx(document.lod_ranges[document.surfaces[1].lod_branch].max_range, 1000.0f));
}

TEST_CASE("[OSGDB] Preserve winding in native Godot Y-up mode") {
	osg::ref_ptr<osg::Node> scene = make_test_scene(false);
	const PackedByteArray bytes = write_test_scene(*scene, false, false);
	OsgSceneConvertOptions options;
	options.osg_z_up = false;
	options.use_root_center = true;
	OsgTileDocument document;
	String error;
	const Error parse_error = OsgSceneConverter::parse_bytes(bytes, "res://city/root.osgb", options, document, error);
	const std::string error_utf8 = error.utf8().get_data();
	REQUIRE_MESSAGE(parse_error == OK, error_utf8);
	REQUIRE(document.surfaces.size() == 1);
	REQUIRE(document.surfaces[0].indices.size() == 3);
	CHECK(document.surfaces[0].indices[0] == 0);
	CHECK(document.surfaces[0].indices[1] == 1);
	CHECK(document.surfaces[0].indices[2] == 2);
	CHECK(document.surfaces[0].normals[0].is_equal_approx(Vector3(0, 0, 1)));
	CHECK(document.local_aabb.size.is_equal_approx(Vector3(1, 1, 0)));
}

TEST_CASE("[OSGDB] Preserve explicit face culling over the two-sided OpenGL default") {
	osg::ref_ptr<osg::Node> scene = make_test_scene(false, false, false);
	osg::Group *root = scene->asGroup();
	REQUIRE(root != nullptr);
	osg::Geode *geode = dynamic_cast<osg::Geode *>(root->getChild(0));
	REQUIRE(geode != nullptr);
	REQUIRE(geode->getNumDrawables() == 1);
	geode->getDrawable(0)->getOrCreateStateSet()->setMode(GL_CULL_FACE, osg::StateAttribute::ON);

	OsgSceneConvertOptions options;
	options.use_root_center = true;
	OsgTileDocument document;
	String error;
	REQUIRE(OsgSceneConverter::parse_bytes(write_test_scene(*scene, false, false), "res://culled.osgb", options, document, error) == OK);
	REQUIRE(document.materials.size() == 1);
	CHECK_FALSE(document.materials[0].double_sided);
}

TEST_CASE("[OSGDB] Merge geometry with compatible material and LOD ownership") {
	osg::ref_ptr<osg::Group> root = new osg::Group;
	root->addChild(make_test_scene(false, false, false).get());
	root->addChild(make_test_scene(false, false, false).get());

	OsgSceneConvertOptions options;
	OsgTileDocument document;
	String error;
	REQUIRE(OsgSceneConverter::parse_bytes(write_test_scene(*root, false, false), "res://merged.osgb", options, document, error) == OK);
	REQUIRE(document.materials.size() == 1);
	REQUIRE(document.surfaces.size() == 1);
	CHECK(document.surfaces[0].vertices.size() == 6);
	REQUIRE(document.surfaces[0].indices.size() == 6);
	CHECK(document.surfaces[0].indices[3] == 3);
	CHECK(document.surfaces[0].indices[4] == 5);
	CHECK(document.surfaces[0].indices[5] == 4);
}

TEST_CASE("[OSGDB] Serial parse queue publishes a CPU-only result") {
	osg::ref_ptr<osg::Node> scene = make_test_scene(false);
	const PackedByteArray bytes = write_test_scene(*scene, false, false);
	OsgParseQueue *queue = OsgParseQueue::get_singleton();
	REQUIRE(queue != nullptr);

	OsgSceneConvertOptions options;
	options.osg_z_up = true;
	options.use_root_center = true;
	const ObjectID owner(uint64_t(0x0A5DB));
	const uint64_t generation = 37;
	const uint64_t request_id = queue->enqueue(owner, generation, "res://city/root.osgb", bytes, options);
	REQUIRE(request_id != 0);

	OsgParseQueue::Result result;
	bool completed = false;
	for (int i = 0; i < 5000 && !completed; i++) {
		completed = queue->take_result(request_id, result);
		if (!completed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(completed);
	CHECK(result.request_id == request_id);
	CHECK(result.owner == owner);
	CHECK(result.generation == generation);
	CHECK_MESSAGE(result.error == OK, result.error_message.utf8().get_data());
	CHECK(result.document.surfaces.size() == 1);
	CHECK(result.document.external_tiles.is_empty());
}

TEST_CASE("[OSGDB] External images decode away from the main thread") {
	OsgImageDecodeQueue *queue = OsgImageDecodeQueue::get_singleton();
	REQUIRE(queue != nullptr);
	Ref<Image> source = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	source->fill(Color(0.25, 0.5, 0.75, 1.0));
	const PackedByteArray png = source->save_png_to_buffer();
	REQUIRE_FALSE(png.is_empty());
	const ObjectID owner(uint64_t(0x1A6E));
	const uint64_t request_id = queue->enqueue(owner, 41, "res://texture.png", png);
	REQUIRE(request_id != 0);

	OsgImageDecodeQueue::Result result;
	bool completed = false;
	for (int i = 0; i < 5000 && !completed; i++) {
		completed = queue->take_result(request_id, result);
		if (!completed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(completed);
	CHECK(result.owner == owner);
	CHECK(result.generation == 41);
	CHECK(result.error == OK);
	REQUIRE(result.image.is_valid());
	CHECK(result.image->get_size() == Vector2i(2, 2));
}

TEST_CASE("[OSGDB] HTTP data requests are non-blocking and coalesced") {
	Ref<TCPServer> server;
	server.instantiate();
	REQUIRE(server->listen(0, IPAddress("127.0.0.1")) == OK);
	const int port = server->get_local_port();
	REQUIRE(port > 0);

	OsgDataRequestQueue *queue = OsgDataRequestQueue::get_singleton();
	REQUIRE(queue != nullptr);
	const String uri = vformat("http://127.0.0.1:%d/root.osgb", port);
	const ObjectID owner_a(uint64_t(1001));
	const ObjectID owner_b(uint64_t(1002));
	const uint64_t request_a = queue->request(owner_a, 11, uri, 1024, 2.0, 0);
	const uint64_t request_b = queue->request(owner_b, 12, uri, 1024, 2.0, 0);
	REQUIRE(request_a != 0);
	REQUIRE(request_b != 0);

	const PackedByteArray expected = String("osgdb-http-payload").to_utf8_buffer();
	Ref<StreamPeerTCP> connection;
	int accepted_connections = 0;
	bool response_sent = false;
	OsgDataRequestQueue::Result result_a;
	OsgDataRequestQueue::Result result_b;
	bool completed_a = false;
	bool completed_b = false;
	for (int i = 0; i < 5000 && (!completed_a || !completed_b); i++) {
		queue->poll();
		if (connection.is_null() && server->is_connection_available()) {
			connection = server->take_connection();
			accepted_connections++;
		}
		if (connection.is_valid() && !response_sent && connection->get_available_bytes() > 0) {
			const int available = connection->get_available_bytes();
			PackedByteArray ignored_request;
			ignored_request.resize(available);
			connection->get_data(ignored_request.ptrw(), available);
			PackedByteArray response = vformat("HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", expected.size()).to_utf8_buffer();
			response.append_array(expected);
			REQUIRE(connection->put_data(response.ptr(), response.size()) == OK);
			response_sent = true;
		}
		completed_a = completed_a || queue->take_result(request_a, result_a);
		completed_b = completed_b || queue->take_result(request_b, result_b);
		if (!completed_a || !completed_b) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(completed_a);
	REQUIRE(completed_b);
	CHECK(accepted_connections == 1);
	CHECK(result_a.error == OK);
	CHECK(result_b.error == OK);
	CHECK(result_a.owner == owner_a);
	CHECK(result_b.owner == owner_b);
	CHECK(result_a.bytes == expected);
	CHECK(result_b.bytes == expected);
	server->stop();
}

TEST_CASE("[OSGDB] HTTP data requests follow relative redirects") {
	Ref<TCPServer> server;
	server.instantiate();
	REQUIRE(server->listen(0, IPAddress("127.0.0.1")) == OK);
	const int port = server->get_local_port();

	OsgDataRequestQueue *queue = OsgDataRequestQueue::get_singleton();
	REQUIRE(queue != nullptr);
	const String uri = vformat("http://127.0.0.1:%d/root.osgb", port);
	CHECK(OsgDataSource::resolve_uri(uri, "/redirected.osgb") == vformat("http://127.0.0.1:%d/redirected.osgb", port));
	const uint64_t request_id = queue->request(ObjectID(uint64_t(1003)), 13, uri, 1024, 2.0, 0);
	REQUIRE(request_id != 0);

	const PackedByteArray expected = String("redirected-osgdb-payload").to_utf8_buffer();
	Ref<StreamPeerTCP> connection;
	int responses_sent = 0;
	OsgDataRequestQueue::Result result;
	bool completed = false;
	for (int i = 0; i < 5000 && !completed; i++) {
		queue->poll();
		if (connection.is_null() && server->is_connection_available()) {
			connection = server->take_connection();
		}
		if (connection.is_valid() && connection->get_available_bytes() > 0) {
			PackedByteArray ignored_request;
			ignored_request.resize(connection->get_available_bytes());
			connection->get_data(ignored_request.ptrw(), ignored_request.size());
			PackedByteArray response;
			if (responses_sent == 0) {
				response = String("HTTP/1.1 302 Found\r\nLocation: /redirected.osgb\r\nContent-Length: 0\r\nConnection: close\r\n\r\n").to_utf8_buffer();
			} else {
				response = vformat("HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", expected.size()).to_utf8_buffer();
				response.append_array(expected);
			}
			REQUIRE(connection->put_data(response.ptr(), response.size()) == OK);
			responses_sent++;
			connection.unref();
		}
		completed = queue->take_result(request_id, result);
		if (!completed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(completed);
	CHECK(responses_sent == 2);
	const std::string error_message = result.error_message.utf8().get_data();
	CHECK_MESSAGE(result.error == OK, error_message);
	CHECK(result.response_code == 200);
	CHECK(result.bytes == expected);
	server->stop();
}

TEST_CASE("[OSGDB][SceneTree] Tiled node uploads an asynchronous root and cancels stale work") {
	osg::ref_ptr<osg::Node> scene = make_test_scene(false, true, false);
	const PackedByteArray bytes = write_test_scene(*scene, false, false);
	const String source_path = TestUtils::get_temp_path("osgdb_async_root.osgb");
	Ref<FileAccess> file = FileAccess::open(source_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_buffer(bytes);
	file.unref();

	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(source_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);
	CHECK(node->is_open());
	CHECK(String(node->get_streaming_stats()["root_state"]) == "requested");

	bool resident = false;
	for (int i = 0; i < 5000 && !resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		resident = String(node->get_streaming_stats()["root_state"]) == "resident";
		if (!resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(resident);
	CHECK(int(node->get_streaming_stats()["resident_tiles"]) == 1);
	CHECK(node->get_dataset_aabb().size.is_equal_approx(Vector3(1, 0, 1)));

	REQUIRE(node->reload() == OK);
	node->close();
	for (int i = 0; i < 10; i++) {
		SceneTree::get_singleton()->process(0.001);
		OS::get_singleton()->delay_usec(1000);
	}
	CHECK(String(node->get_streaming_stats()["root_state"]) == "cancelled");
	CHECK(int(node->get_streaming_stats()["resident_tiles"]) == 0);
	CHECK_FALSE(node->is_open());

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
}

TEST_CASE("[OSGDB][SceneTree] Inline LOD displays only the active range") {
	osg::ref_ptr<osg::LOD> lod = new osg::LOD;
	lod->setCenter(osg::Vec3d(1000000.5, 2000000.5, 3000000.0));
	lod->setRadius(2.0f);
	lod->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	lod->addChild(make_test_scene(false, true, false).get(), 0.0f, 10.0f);
	lod->addChild(make_test_scene(false, true, false).get(), 10.0f, 1000.0f);

	const String root_path = TestUtils::get_temp_path("osgdb_inline_lod.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*lod, false, false));
	root_file.unref();

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 0, 5));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool resident = false;
	for (int i = 0; i < 5000 && !resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		resident = String(node->get_streaming_stats()["root_state"]) == "resident";
		if (!resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(resident);
	SceneTree::get_singleton()->process(0.001);
	Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["resident_render_instances"]) == 2);
	CHECK(int(stats["visible_render_instances"]) == 1);

	// Hysteresis is useful while an external tile is loading, but resident
	// inline siblings must remain mutually exclusive at their shared boundary.
	// The old per-branch hysteresis made both [0, 10) and [10, 1000) visible
	// throughout the overlap around 10.
	camera->set_position(Vector3(0, 0, 12.5));
	SceneTree::get_singleton()->process(0.001);
	stats = node->get_streaming_stats();
	CHECK(int(stats["visible_render_instances"]) == 1);

	camera->set_position(Vector3(0, 0, 100));
	SceneTree::get_singleton()->process(0.001);
	stats = node->get_streaming_stats();
	CHECK(int(stats["visible_render_instances"]) == 1);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] Local PagedLOD keeps its parent visible until the child is resident") {
	osg::ref_ptr<osg::Node> root_scene = make_test_scene(true, true, false);
	osg::ref_ptr<osg::Node> child_scene = make_test_scene(false, true, false);
	const PackedByteArray root_bytes = write_test_scene(*root_scene, false, false);
	const PackedByteArray child_bytes = write_test_scene(*child_scene, false, false);
	const String root_path = TestUtils::get_temp_path("osgdb_paged_root.osgb");
	const String child_path = TestUtils::get_temp_path("child.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(root_bytes);
	root_file.unref();
	Ref<FileAccess> child_file = FileAccess::open(child_path, FileAccess::WRITE);
	REQUIRE(child_file.is_valid());
	child_file->store_buffer(child_bytes);
	child_file.unref();

	Camera3D *camera = memnew(Camera3D);
	camera->set_name("OsgdbPagedCamera");
	camera->set_position(Vector3(0, 0, 10));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();

	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool observed_parent_fallback = false;
	bool child_resident = false;
	for (int i = 0; i < 5000 && !child_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		if (int(stats["known_tiles"]) == 2 && int(stats["resident_tiles"]) == 1) {
			observed_parent_fallback = observed_parent_fallback || int(stats["visible_tiles"]) == 1;
		}
		child_resident = int(stats["resident_tiles"]) == 2;
		if (!child_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(observed_parent_fallback);
	REQUIRE(child_resident);
	const Dictionary final_stats = node->get_streaming_stats();
	CHECK(int(final_stats["known_tiles"]) == 2);
	// make_test_scene() also has geometry outside the PagedLOD. It remains an
	// unconditional root sibling after the external child becomes resident.
	CHECK(int(final_stats["visible_tiles"]) == 2);
	CHECK(int(final_stats["failed_tiles"]) == 0);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] External detail is requested only inside its authored LOD range") {
	osg::ref_ptr<osg::Node> fallback_geometry = make_test_scene(false, false, false)->asGroup()->getChild(0);
	osg::ref_ptr<osg::Node> child_geometry = make_test_scene(false, false, false)->asGroup()->getChild(0);
	osg::ref_ptr<osg::PagedLOD> root_scene = new osg::PagedLOD;
	root_scene->setCenter(osg::Vec3d(0.0, 0.0, 0.0));
	root_scene->setRadius(2.0f);
	root_scene->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	root_scene->addChild(fallback_geometry.get(), 10.0f, std::numeric_limits<float>::max());
	root_scene->setFileName(1, "strict_range_child.osgb");
	root_scene->setRange(1, 0.0f, 10.0f);

	const String root_path = TestUtils::get_temp_path("osgdb_strict_range_root.osgb");
	const String child_path = TestUtils::get_temp_path("strict_range_child.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Ref<FileAccess> child_file = FileAccess::open(child_path, FileAccess::WRITE);
	REQUIRE(child_file.is_valid());
	child_file->store_buffer(write_test_scene(*child_geometry, false, false));
	child_file.unref();

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 0, 11));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_world_origin_mode(TiledMeshInstance3D::WORLD_ORIGIN_EXPLICIT);
	node->set_world_origin(0.0, 0.0, 0.0);
	node->set_lod_hysteresis(0.0);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool root_resident = false;
	for (int i = 0; i < 5000 && !root_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		root_resident = String(node->get_streaming_stats()["root_state"]) == "resident";
		if (!root_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(root_resident);
	for (int i = 0; i < 100; i++) {
		SceneTree::get_singleton()->process(0.001);
		OS::get_singleton()->delay_usec(1000);
	}
	Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["resident_tiles"]) == 1);
	CHECK(int(stats["requested_tiles"]) == 0);

	// Crossing into [0, 10) makes the detail eligible. No expanded request
	// range or speculative prefetching is involved.
	camera->set_position(Vector3(0, 0, 9));
	bool child_resident = false;
	for (int i = 0; i < 5000 && !child_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		child_resident = int(node->get_streaming_stats()["resident_tiles"]) == 2;
		if (!child_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(child_resident);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] The largest visual contribution is refined first") {
	osg::ref_ptr<osg::Group> root_scene = new osg::Group;
	for (int i = 0; i < 2; i++) {
		osg::ref_ptr<osg::MatrixTransform> fallback = make_test_scene(false, false, false)->asTransform()->asMatrixTransform();
		fallback->setMatrix(osg::Matrixd::scale(i == 0 ? 1.0 : 8.0, i == 0 ? 1.0 : 8.0, 1.0));
		osg::ref_ptr<osg::PagedLOD> paged = new osg::PagedLOD;
		paged->setCenter(osg::Vec3d(0.0, 0.0, 0.0));
		paged->setRadius(i == 0 ? 1.0 : 8.0);
		paged->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
		paged->addChild(fallback.get(), 0.0f, 1000.0f);
		paged->setFileName(1, i == 0 ? "small_contribution.osgb" : "large_contribution.osgb");
		paged->setRange(1, 0.0f, 1000.0f);
		root_scene->addChild(paged.get());
	}

	const String root_path = TestUtils::get_temp_path("osgdb_visual_priority_root.osgb");
	const String small_path = TestUtils::get_temp_path("small_contribution.osgb");
	const String large_path = TestUtils::get_temp_path("large_contribution.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Vector<String> child_paths;
	child_paths.push_back(small_path);
	child_paths.push_back(large_path);
	for (const String &path : child_paths) {
		Ref<FileAccess> child_file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(child_file.is_valid());
		child_file->store_buffer(write_test_scene(*make_test_scene(false, false, false), false, false));
	}

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 10, 30));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->look_at(Vector3());
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_world_origin_mode(TiledMeshInstance3D::WORLD_ORIGIN_EXPLICIT);
	node->set_world_origin(0.0, 0.0, 0.0);
	node->set_max_concurrent_requests(1);
	node->set_editor_preview_max_requests(1);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	SIGNAL_WATCH(node, "tile_loaded");
	REQUIRE(node->open() == OK);

	bool root_resident = false;
	for (int i = 0; i < 5000 && !root_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		root_resident = String(node->get_streaming_stats()["root_state"]) == "resident";
		if (!root_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(root_resident);
	SIGNAL_DISCARD("tile_loaded");
	bool first_child_resident = false;
	for (int i = 0; i < 5000 && !first_child_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		first_child_resident = int(node->get_streaming_stats()["resident_tiles"]) == 2;
		if (!first_child_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(first_child_resident);
	Array expected_signals;
	Array expected_args;
	expected_args.push_back(OsgDataSource::normalize_uri(large_path));
	expected_signals.push_back(expected_args);
	SIGNAL_CHECK("tile_loaded", expected_signals);
	SIGNAL_UNWATCH(node, "tile_loaded");

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] Pixel-size LOD uses camera-space depth for an off-axis tile") {
	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 0, 10));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();

	const Vector3 source_center(5.5, 0.5, 0.0);
	Viewport *viewport = camera->get_viewport();
	REQUIRE(viewport != nullptr);
	const Vector2 viewport_size = viewport->get_visible_rect().size;
	const Projection projection = camera->get_camera_projection();
	const double focal_x = Math::abs(double(projection.columns[0].x)) * viewport_size.x * 0.5;
	const double focal_y = Math::abs(double(projection.columns[1].y)) * viewport_size.y * 0.5;
	const double focal_pixels = Math::sqrt((focal_x * focal_x + focal_y * focal_y) * 0.5);
	const double radius = 1.0;
	const double view_depth = 10.0;
	const double euclidean_distance = camera->get_position().distance_to(source_center);
	const float threshold = float((radius * 2.0 * focal_pixels / view_depth + radius * 2.0 * focal_pixels / euclidean_distance) * 0.5);
	REQUIRE(radius * 2.0 * focal_pixels / view_depth > threshold);
	REQUIRE(radius * 2.0 * focal_pixels / euclidean_distance < threshold);

	osg::ref_ptr<osg::Node> fallback_geometry = make_test_scene(false, true, false);
	fallback_geometry->asTransform()->asMatrixTransform()->setMatrix(osg::Matrixd::translate(5.0, 0.0, 0.0));
	osg::ref_ptr<osg::Node> child_geometry = make_test_scene(false, true, false);
	child_geometry->asTransform()->asMatrixTransform()->setMatrix(osg::Matrixd::translate(5.0, 0.0, 0.0));
	osg::ref_ptr<osg::PagedLOD> root_scene = new osg::PagedLOD;
	root_scene->setCenter(osg::Vec3d(source_center.x, source_center.y, source_center.z));
	root_scene->setRadius(radius);
	root_scene->setRangeMode(osg::LOD::PIXEL_SIZE_ON_SCREEN);
	root_scene->addChild(fallback_geometry.get(), 0.0f, threshold);
	root_scene->setFileName(1, "off_axis_pixel_child.osgb");
	root_scene->setRange(1, threshold, std::numeric_limits<float>::max());

	const String root_path = TestUtils::get_temp_path("osgdb_off_axis_pixel_root.osgb");
	const String child_path = TestUtils::get_temp_path("off_axis_pixel_child.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Ref<FileAccess> child_file = FileAccess::open(child_path, FileAccess::WRITE);
	REQUIRE(child_file.is_valid());
	child_file->store_buffer(write_test_scene(*child_geometry, false, false));
	child_file.unref();

	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_world_origin_mode(TiledMeshInstance3D::WORLD_ORIGIN_EXPLICIT);
	node->set_world_origin(0.0, 0.0, 0.0);
	node->set_lod_bias(1.0);
	node->set_editor_preview_lod_bias(1.0);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool child_resident = false;
	for (int i = 0; i < 5000 && !child_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		child_resident = int(node->get_streaming_stats()["resident_tiles"]) == 2;
		if (!child_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(child_resident);
	CHECK(int(node->get_streaming_stats()["visible_render_instances"]) == 1);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] Resident PagedLOD uses the mesh AABB instead of an undersized declared sphere") {
	osg::ref_ptr<osg::Node> fallback_geometry = make_test_scene(false, true, false);
	fallback_geometry->asTransform()->asMatrixTransform()->setMatrix(osg::Matrixd::translate(50.0, 0.0, 0.0));
	osg::ref_ptr<osg::Node> child_geometry = make_test_scene(false, true, false);
	child_geometry->asTransform()->asMatrixTransform()->setMatrix(osg::Matrixd::translate(50.0, 0.0, 0.0));
	osg::ref_ptr<osg::PagedLOD> root_scene = new osg::PagedLOD;
	root_scene->setCenter(osg::Vec3d(0.0, 0.0, 0.0));
	root_scene->setRadius(0.25f);
	root_scene->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	root_scene->addChild(fallback_geometry.get(), 0.0f, 1000.0f);
	root_scene->setFileName(1, "undersized_bound_child.osgb");
	root_scene->setRange(1, 0.0f, 1000.0f);

	const String root_path = TestUtils::get_temp_path("osgdb_undersized_bound_root.osgb");
	const String child_path = TestUtils::get_temp_path("undersized_bound_child.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Ref<FileAccess> child_file = FileAccess::open(child_path, FileAccess::WRITE);
	REQUIRE(child_file.is_valid());
	child_file->store_buffer(write_test_scene(*child_geometry, false, false));
	child_file.unref();

	Camera3D *camera = memnew(Camera3D);
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool root_resident = false;
	for (int i = 0; i < 5000 && !root_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		root_resident = String(node->get_streaming_stats()["root_state"]) == "resident";
		if (!root_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(root_resident);
	// Frame the actual geometry. The declared sphere at X=0 is outside this
	// view, so scheduling must use the uploaded fallback AABB at X=50.
	const Vector3 geometry_center = node->world_to_local(50.5, 0.5, 0.0);
	camera->set_position(geometry_center + Vector3(0, 10, 0));
	camera->look_at(geometry_center, Vector3(0, 0, -1));
	bool child_resident = false;
	for (int i = 0; i < 5000 && !child_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		child_resident = int(node->get_streaming_stats()["resident_tiles"]) == 2;
		if (!child_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(child_resident);

	SceneTree::get_singleton()->process(0.001);
	const Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["hidden_fallback_surfaces"]) == 1);
	CHECK(int(stats["visible_render_instances"]) == 1);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] A visible fallback AABB schedules a child whose declared sphere is outside the frustum") {
	osg::ref_ptr<osg::Node> fallback_geometry = make_test_scene(false, true, false)->asGroup()->getChild(0);
	osg::ref_ptr<osg::Node> child_geometry = make_test_scene(false, true, false)->asGroup()->getChild(0);
	osg::ref_ptr<osg::PagedLOD> root_scene = new osg::PagedLOD;
	// Deliberately invalid producer bound: its center is far outside the camera,
	// while the real fallback and external child geometry are at the origin.
	root_scene->setCenter(osg::Vec3d(1000.0, 0.0, 0.0));
	root_scene->setRadius(0.1f);
	root_scene->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	root_scene->addChild(fallback_geometry.get(), 0.0f, 2000.0f);
	root_scene->setFileName(1, "misbounded_child.osgb");
	root_scene->setRange(1, 0.0f, 2000.0f);

	const String root_path = TestUtils::get_temp_path("osgdb_misbounded_root.osgb");
	const String child_path = TestUtils::get_temp_path("misbounded_child.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Ref<FileAccess> child_file = FileAccess::open(child_path, FileAccess::WRITE);
	REQUIRE(child_file.is_valid());
	child_file->store_buffer(write_test_scene(*child_geometry, false, false));
	child_file.unref();

	Camera3D *camera = memnew(Camera3D);
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool root_resident = false;
	for (int i = 0; i < 5000 && !root_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		root_resident = String(node->get_streaming_stats()["root_state"]) == "resident";
		if (!root_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(root_resident);
	const Vector3 geometry_center = node->world_to_local(0.5, 0.5, 0.0);
	camera->set_position(geometry_center + Vector3(0, 10, 0));
	camera->look_at(geometry_center, Vector3(0, 0, -1));

	bool child_resident = false;
	for (int i = 0; i < 5000 && !child_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		child_resident = int(node->get_streaming_stats()["resident_tiles"]) == 2;
		if (!child_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(child_resident);
	CHECK(int(node->get_streaming_stats()["failed_tiles"]) == 0);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] An external document covers its parent only when all fallback groups are displayable") {
	osg::ref_ptr<osg::Node> root_fallback = make_test_scene(false, true, false)->asGroup()->getChild(0);
	osg::ref_ptr<osg::Node> group_a_fallback = make_test_scene(false, true, false)->asGroup()->getChild(0);
	osg::ref_ptr<osg::PagedLOD> root_scene = new osg::PagedLOD;
	root_scene->setCenter(osg::Vec3d(0.5, 0.5, 0.0));
	root_scene->setRadius(2.0f);
	root_scene->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	root_scene->addChild(root_fallback.get(), 0.0f, 1000.0f);
	root_scene->setFileName(1, "multi_group_child.osgb");
	root_scene->setRange(1, 0.0f, 1000.0f);

	osg::ref_ptr<osg::Group> child_scene = new osg::Group;
	osg::ref_ptr<osg::PagedLOD> group_a = new osg::PagedLOD;
	group_a->setCenter(osg::Vec3d(0.5, 0.5, 0.0));
	group_a->setRadius(2.0f);
	group_a->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	group_a->addChild(group_a_fallback.get(), 0.0f, 1000.0f);
	group_a->setFileName(1, "inactive_missing.osgb");
	group_a->setRange(1, 2000.0f, 3000.0f);
	child_scene->addChild(group_a.get());
	osg::ref_ptr<osg::PagedLOD> group_b = new osg::PagedLOD;
	group_b->setCenter(osg::Vec3d(0.5, 0.5, 0.0));
	group_b->setRadius(2.0f);
	group_b->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	group_b->setFileName(0, "required_missing.osgb");
	group_b->setRange(0, 0.0f, 1000.0f);
	child_scene->addChild(group_b.get());

	const String root_path = TestUtils::get_temp_path("osgdb_multi_group_root.osgb");
	const String child_path = TestUtils::get_temp_path("multi_group_child.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Ref<FileAccess> child_file = FileAccess::open(child_path, FileAccess::WRITE);
	REQUIRE(child_file.is_valid());
	child_file->store_buffer(write_test_scene(*child_scene, false, false));
	child_file.unref();

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 0, 10));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_retry_count(0);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool required_group_failed = false;
	for (int i = 0; i < 5000 && !required_group_failed; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		required_group_failed = int(stats["resident_tiles"]) == 2 && int(stats["failed_tiles"]) == 1;
		if (!required_group_failed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(required_group_failed);
	SceneTree::get_singleton()->process(0.001);
	const Dictionary stats = node->get_streaming_stats();
	// Group A's fallback and the root fallback must both remain. The old
	// any-group test hid the root as soon as Group A became displayable.
	CHECK(int(stats["visible_render_instances"]) == 2);
	CHECK(int(stats["hidden_fallback_surfaces"]) == 0);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] Parent fallback remains when a resident child has no active LOD branch") {
	osg::ref_ptr<osg::Node> root_geometry_scene = make_test_scene(false, false, false);
	osg::ref_ptr<osg::Node> child_geometry_scene = make_test_scene(false, false, false);
	osg::ref_ptr<osg::PagedLOD> root_scene = new osg::PagedLOD;
	root_scene->setCenter(osg::Vec3d(0.5, 0.5, 0.0));
	root_scene->setRadius(2.0f);
	root_scene->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	root_scene->addChild(root_geometry_scene->asGroup()->getChild(0), 0.0f, 1000.0f);
	root_scene->setFileName(1, "lod_gap_child.osgb");
	root_scene->setRange(1, 0.0f, 1000.0f);

	osg::ref_ptr<osg::PagedLOD> child_scene = new osg::PagedLOD;
	child_scene->setCenter(osg::Vec3d(0.5, 0.5, 0.0));
	child_scene->setRadius(2.0f);
	child_scene->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	child_scene->addChild(child_geometry_scene->asGroup()->getChild(0), 0.0f, 10.0f);
	child_scene->setFileName(1, "missing_lod_gap_grandchild.osgb");
	child_scene->setRange(1, 20.0f, 1000.0f);

	const String root_path = TestUtils::get_temp_path("osgdb_lod_gap_root.osgb");
	const String child_path = TestUtils::get_temp_path("lod_gap_child.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Ref<FileAccess> child_file = FileAccess::open(child_path, FileAccess::WRITE);
	REQUIRE(child_file.is_valid());
	child_file->store_buffer(write_test_scene(*child_scene, false, false));
	child_file.unref();

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 0, 15));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool child_resident = false;
	for (int i = 0; i < 5000 && !child_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		child_resident = int(stats["known_tiles"]) == 3 && int(stats["resident_tiles"]) == 2;
		if (!child_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(child_resident);
	SceneTree::get_singleton()->process(0.001);
	const Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["visible_render_instances"]) == 1);
	CHECK(int(stats["failed_tiles"]) == 0);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] Uploading a tile can safely register many grandchildren") {
	osg::ref_ptr<osg::Node> root_scene = make_test_scene(true, false, false);
	osg::ref_ptr<osg::Group> child_scene = new osg::Group;
	child_scene->addChild(make_test_scene(false, false, false).get());
	constexpr int grandchild_count = 64;
	for (int i = 0; i < grandchild_count; i++) {
		osg::ref_ptr<osg::PagedLOD> paged = new osg::PagedLOD;
		paged->setCenter(osg::Vec3d(1000000.5 + i, 2000000.5, 3000000.0));
		paged->setRadius(2.0f);
		paged->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
		paged->setRange(0, 0.0f, 1000.0f);
		paged->setFileName(0, vformat("missing_grandchild_%d.osgb", i).utf8().get_data());
		child_scene->addChild(paged.get());
	}

	const String root_path = TestUtils::get_temp_path("osgdb_register_grandchildren_root.osgb");
	const String child_path = TestUtils::get_temp_path("child.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Ref<FileAccess> child_file = FileAccess::open(child_path, FileAccess::WRITE);
	REQUIRE(child_file.is_valid());
	child_file->store_buffer(write_test_scene(*child_scene, false, false));
	child_file.unref();

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 0, 10));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_retry_count(0);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool grandchildren_registered = false;
	for (int i = 0; i < 5000 && !grandchildren_registered; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		grandchildren_registered = int(stats["known_tiles"]) == 2 + grandchild_count;
		if (!grandchildren_registered) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(grandchildren_registered);
	const Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["resident_tiles"]) >= 2);
	CHECK(int(stats["cpu_ready_tiles"]) == 0);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] A resident refinement hides only its matching coarse surface") {
	osg::ref_ptr<osg::Group> root_scene = new osg::Group;
	for (int i = 0; i < 2; i++) {
		osg::ref_ptr<osg::PagedLOD> paged = new osg::PagedLOD;
		paged->setCenter(osg::Vec3d(1000000.5 + i * 10.0, 2000000.5, 3000000.0));
		paged->setRadius(8.0f);
		paged->addChild(make_test_scene(false, true, false).get(), 0.0f, 1000.0f);
		paged->setFileName(1, i == 0 ? "fine_0.osgb" : "missing_fine_1.osgb");
		paged->setRange(1, 0.0f, 1000.0f);
		root_scene->addChild(paged.get());
	}
	const String root_path = TestUtils::get_temp_path("osgdb_grouped_fallback_root.osgb");
	const String fine_path = TestUtils::get_temp_path("fine_0.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Ref<FileAccess> fine_file = FileAccess::open(fine_path, FileAccess::WRITE);
	REQUIRE(fine_file.is_valid());
	fine_file->store_buffer(write_test_scene(*make_test_scene(false, true, false), false, false));
	fine_file.unref();

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 5, 20));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_retry_count(0);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool one_group_hidden = false;
	for (int i = 0; i < 5000 && !one_group_hidden; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		one_group_hidden = int(stats["fallback_surfaces"]) == 2 && int(stats["hidden_fallback_surfaces"]) == 1;
		if (!one_group_hidden) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(one_group_hidden);
	const Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["resident_tiles"]) == 2);
	CHECK(int(stats["failed_tiles"]) == 1);
	CHECK(int(stats["visible_tiles"]) == 2);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] A resident refinement preserves unconditional sibling geometry") {
	osg::ref_ptr<osg::Group> root_scene = new osg::Group;
	root_scene->addChild(make_test_scene(false, true, false).get());

	osg::ref_ptr<osg::PagedLOD> paged = new osg::PagedLOD;
	paged->setCenter(osg::Vec3d(1000000.5, 2000000.5, 3000000.0));
	paged->setRadius(8.0f);
	paged->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
	paged->addChild(make_test_scene(false, true, false).get(), 0.0f, 1000.0f);
	paged->setFileName(1, "sibling_fine.osgb");
	paged->setRange(1, 0.0f, 1000.0f);
	root_scene->addChild(paged.get());

	const String root_path = TestUtils::get_temp_path("osgdb_unconditional_sibling_root.osgb");
	const String fine_path = TestUtils::get_temp_path("sibling_fine.osgb");
	Ref<FileAccess> root_file = FileAccess::open(root_path, FileAccess::WRITE);
	REQUIRE(root_file.is_valid());
	root_file->store_buffer(write_test_scene(*root_scene, false, false));
	root_file.unref();
	Ref<FileAccess> fine_file = FileAccess::open(fine_path, FileAccess::WRITE);
	REQUIRE(fine_file.is_valid());
	fine_file->store_buffer(write_test_scene(*make_test_scene(false, true, false), false, false));
	fine_file.unref();

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 5, 20));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(root_path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	bool child_resident = false;
	for (int i = 0; i < 5000 && !child_resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		child_resident = int(stats["resident_tiles"]) == 2;
		if (!child_resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(child_resident);
	SceneTree::get_singleton()->process(0.001);
	const Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["resident_render_instances"]) == 3);
	CHECK(int(stats["visible_render_instances"]) == 2);
	CHECK(int(stats["hidden_fallback_surfaces"]) == 1);
	CHECK(int(stats["visible_tiles"]) == 2);

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[OSGDB][SceneTree] Tiled node loads an OSGB root over HTTP") {
	osg::ref_ptr<osg::Node> scene = make_test_scene(false, true, false);
	const PackedByteArray osgb = write_test_scene(*scene, false, false);
	Ref<TCPServer> server;
	server.instantiate();
	REQUIRE(server->listen(0, IPAddress("127.0.0.1")) == OK);
	const String uri = vformat("http://127.0.0.1:%d/root.osgb", server->get_local_port());

	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(uri);
	SceneTree::get_singleton()->get_root()->add_child(node);
	REQUIRE(node->open() == OK);

	Ref<StreamPeerTCP> connection;
	bool response_sent = false;
	bool resident = false;
	for (int i = 0; i < 5000 && !resident; i++) {
		SceneTree::get_singleton()->process(0.001);
		if (connection.is_null() && server->is_connection_available()) {
			connection = server->take_connection();
		}
		if (connection.is_valid() && !response_sent && connection->get_available_bytes() > 0) {
			const int available = connection->get_available_bytes();
			PackedByteArray ignored_request;
			ignored_request.resize(available);
			connection->get_data(ignored_request.ptrw(), available);
			PackedByteArray response = vformat("HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", osgb.size()).to_utf8_buffer();
			response.append_array(osgb);
			REQUIRE(connection->put_data(response.ptr(), response.size()) == OK);
			response_sent = true;
		}
		resident = String(node->get_streaming_stats()["root_state"]) == "resident";
		if (!resident) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	REQUIRE(response_sent);
	REQUIRE(resident);
	CHECK(int(node->get_streaming_stats()["resident_tiles"]) == 1);
	CHECK(node->get_dataset_aabb().size.is_equal_approx(Vector3(1, 0, 1)));

	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	server->stop();
}

} // namespace TestOsgdb
