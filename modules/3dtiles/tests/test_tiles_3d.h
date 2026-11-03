/**************************************************************************/
/*  test_tiles_3d.h                                                       */
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

#include "core/config/engine.h"
#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#include "modules/3dtiles/tiles_3d_parser.h"
#include "modules/tiled_mesh_3d/tiled_mesh_data_request_queue.h"
#include "modules/tiled_mesh_3d/tiled_mesh_data_source.h"
#include "modules/tiled_mesh_3d/tiled_mesh_format_registry.h"
#include "modules/tiled_mesh_3d/tiled_mesh_instance_3d.h"

namespace TestTiles3D {

static Dictionary dictionary(const String &p_json) {
	JSON json;
	CHECK(json.parse(p_json) == OK);
	return json.get_data();
}

static PackedByteArray triangle_buffer() {
	PackedByteArray bytes;
	bytes.resize(72);
	const float values[] = { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 };
	for (int i = 0; i < 18; i++) {
		encode_float(values[i], bytes.ptrw() + i * 4);
	}
	return bytes;
}

static Dictionary triangle_json() {
	return dictionary(R"({
		"asset":{"version":"2.0"}, "scene":0, "scenes":[{"nodes":[0]}], "nodes":[{"mesh":0}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1}}]}],
		"buffers":[{"byteLength":72}], "bufferViews":[{"buffer":0,"byteLength":72}],
		"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
		{"bufferView":0,"byteOffset":36,"componentType":5126,"count":3,"type":"VEC3"}]
	})");
}

static PackedByteArray glb(const Dictionary &p_json = triangle_json(), const PackedByteArray &p_binary = triangle_buffer()) {
	PackedByteArray text = JSON::stringify(p_json, "", false, true).to_utf8_buffer();
	while (text.size() % 4) {
		text.push_back(' ');
	}
	PackedByteArray binary = p_binary;
	while (binary.size() % 4) {
		binary.push_back(0);
	}
	PackedByteArray result;
	result.resize(20);
	memcpy(result.ptrw(), "glTF", 4);
	encode_uint32(2, result.ptrw() + 4);
	encode_uint32(28 + text.size() + binary.size(), result.ptrw() + 8);
	encode_uint32(text.size(), result.ptrw() + 12);
	encode_uint32(0x4e4f534a, result.ptrw() + 16);
	result.append_array(text);
	const int offset = result.size();
	result.resize(offset + 8);
	encode_uint32(binary.size(), result.ptrw() + offset);
	encode_uint32(0x004e4942, result.ptrw() + offset + 4);
	result.append_array(binary);
	return result;
}

static PackedByteArray batched(const Vector3 &p_rtc = Vector3(), bool p_binary_rtc = false) {
	Dictionary feature;
	feature["BATCH_LENGTH"] = 0;
	PackedByteArray binary;
	if (p_binary_rtc) {
		Dictionary offset;
		offset["byteOffset"] = 0;
		feature["RTC_CENTER"] = offset;
		binary.resize(12);
		encode_float(p_rtc.x, binary.ptrw());
		encode_float(p_rtc.y, binary.ptrw() + 4);
		encode_float(p_rtc.z, binary.ptrw() + 8);
	} else {
		feature["RTC_CENTER"] = Array({ p_rtc.x, p_rtc.y, p_rtc.z });
	}
	PackedByteArray text = JSON::stringify(feature).to_utf8_buffer();
	while ((28 + text.size()) % 8) {
		text.push_back(' ');
	}
	while (binary.size() % 8) {
		binary.push_back(0);
	}
	PackedByteArray result;
	result.resize(28);
	result.fill(0);
	memcpy(result.ptrw(), "b3dm", 4);
	encode_uint32(1, result.ptrw() + 4);
	encode_uint32(text.size(), result.ptrw() + 12);
	encode_uint32(binary.size(), result.ptrw() + 16);
	result.append_array(text);
	result.append_array(binary);
	result.append_array(glb());
	while (result.size() % 8) {
		result.push_back(0);
	}
	encode_uint32(result.size(), result.ptrw() + 8);
	return result;
}

static PackedByteArray batched_glb(const PackedByteArray &p_glb, bool p_pad_end = true) {
	Dictionary feature;
	feature["BATCH_LENGTH"] = 0;
	PackedByteArray text = JSON::stringify(feature).to_utf8_buffer();
	while ((28 + text.size()) % 8) {
		text.push_back(' ');
	}
	PackedByteArray result;
	result.resize(28);
	result.fill(0);
	memcpy(result.ptrw(), "b3dm", 4);
	encode_uint32(1, result.ptrw() + 4);
	encode_uint32(text.size(), result.ptrw() + 12);
	result.append_array(text);
	result.append_array(p_glb);
	while (p_pad_end && result.size() % 8) {
		result.push_back(0);
	}
	encode_uint32(result.size(), result.ptrw() + 8);
	return result;
}

static PackedByteArray metadata_b3dm(Dictionary p_batch_table = dictionary(R"({"name":["building"],"enabled":[true],"optional":[null],"details":[{"height":12}],"temperature":{"byteOffset":0,"componentType":"FLOAT","type":"SCALAR"},"offset":{"byteOffset":8,"componentType":"SHORT","type":"VEC2"}})")) {
	Dictionary manifest = triangle_json();
	Array views = manifest["bufferViews"];
	Dictionary view = views[0];
	view["byteLength"] = 76;
	views[0] = view;
	manifest["bufferViews"] = views;
	Array buffers = manifest["buffers"];
	Dictionary buffer = buffers[0];
	buffer["byteLength"] = 76;
	buffers[0] = buffer;
	manifest["buffers"] = buffers;
	Array accessors = manifest["accessors"];
	accessors.push_back(dictionary(R"({"bufferView":0,"byteOffset":72,"componentType":5121,"count":3,"type":"SCALAR"})"));
	manifest["accessors"] = accessors;
	Array meshes = manifest["meshes"];
	Dictionary mesh = meshes[0];
	Array primitives = mesh["primitives"];
	Dictionary primitive = primitives[0];
	Dictionary attributes = primitive["attributes"];
	attributes["_BATCHID"] = 2;
	primitive["attributes"] = attributes;
	primitives[0] = primitive;
	mesh["primitives"] = primitives;
	meshes[0] = mesh;
	manifest["meshes"] = meshes;
	PackedByteArray geometry = triangle_buffer();
	geometry.resize(76);
	geometry.set(72, 0);
	geometry.set(73, 0);
	geometry.set(74, 0);

	Dictionary feature;
	feature["BATCH_LENGTH"] = 1;
	PackedByteArray feature_json = JSON::stringify(feature).to_utf8_buffer();
	while ((28 + feature_json.size()) % 8) feature_json.push_back(' ');
	PackedByteArray batch_json = JSON::stringify(p_batch_table).to_utf8_buffer();
	while ((28 + feature_json.size() + batch_json.size()) % 8) batch_json.push_back(' ');
	PackedByteArray batch_binary;
	batch_binary.resize(16);
	encode_float(18.5f, batch_binary.ptrw());
	encode_uint16(uint16_t(int16_t(-2)), batch_binary.ptrw() + 8);
	encode_uint16(7, batch_binary.ptrw() + 10);
	PackedByteArray result;
	result.resize(28);
	result.fill(0);
	memcpy(result.ptrw(), "b3dm", 4);
	encode_uint32(1, result.ptrw() + 4);
	encode_uint32(feature_json.size(), result.ptrw() + 12);
	encode_uint32(batch_json.size(), result.ptrw() + 20);
	encode_uint32(batch_binary.size(), result.ptrw() + 24);
	result.append_array(feature_json);
	result.append_array(batch_json);
	result.append_array(batch_binary);
	result.append_array(glb(manifest, geometry));
	while (result.size() % 8) result.push_back(0);
	encode_uint32(result.size(), result.ptrw() + 8);
	return result;
}

static PackedByteArray tileset(const String &p_root) {
	return ("{\"asset\":{\"version\":\"1.1\"},\"geometricError\":1000,\"root\":" + p_root + "}").to_utf8_buffer();
}

static Error parse(const PackedByteArray &p_bytes, TiledMeshDocument &r_document, String &r_error,
		const PackedByteArray &p_context = PackedByteArray(), const Tiles3DParseOptions &p_options = Tiles3DParseOptions()) {
	TiledMeshParseRequest request;
	request.uri = "res://tiles/tileset.json";
	request.bytes = p_bytes;
	request.context = p_context;
	request.root = p_context.is_empty();
	Vector<String> dependencies;
	return Tiles3DParser::parse(request, p_options, {}, r_document, dependencies, r_error);
}

static PackedByteArray legacy_tile(const char *p_magic, const Dictionary &p_feature, PackedByteArray p_binary, const PackedByteArray &p_payload = PackedByteArray(), uint32_t p_format = 1) {
	const int header = memcmp(p_magic, "i3dm", 4) == 0 ? 32 : 28;
	PackedByteArray text = JSON::stringify(p_feature).to_utf8_buffer();
	while ((header + text.size()) % 8) text.push_back(' ');
	while (p_binary.size() % 8) p_binary.push_back(0);
	PackedByteArray result;
	result.resize(header);
	result.fill(0);
	memcpy(result.ptrw(), p_magic, 4);
	encode_uint32(1, result.ptrw() + 4);
	encode_uint32(text.size(), result.ptrw() + 12);
	encode_uint32(p_binary.size(), result.ptrw() + 16);
	if (header == 32) encode_uint32(p_format, result.ptrw() + 28);
	result.append_array(text);
	result.append_array(p_binary);
	result.append_array(p_payload);
	while (result.size() % 8) result.push_back(header == 32 && p_format == 0 ? ' ' : 0);
	encode_uint32(result.size(), result.ptrw() + 8);
	return result;
}

TEST_CASE("[3DTiles] PNTS preserves unindexed points and rejects invalid counts and offsets") {
	Dictionary feature = dictionary(R"({"POINTS_LENGTH":3,"POSITION":{"byteOffset":0},"CONSTANT_RGBA":[255,0,0,255]})");
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(legacy_tile("pnts", feature, triangle_buffer()), document, error) == OK);
	REQUIRE(document.surfaces.size() == 1);
	const TiledMeshMeshSurfacePayload &surface = document.surfaces[0];
	CHECK(surface.points);
	CHECK(surface.indices.is_empty());
	CHECK(surface.vertices.size() == 3);
	REQUIRE(surface.batch_ids.size() == 3);
	CHECK(surface.batch_ids[2] == 2);
	REQUIRE(surface.colors.size() == 3);
	CHECK(surface.colors[0] == Color(1, 0, 0, 1));
	feature["POINTS_LENGTH"] = int64_t(4294967295LL);
	CHECK(parse(legacy_tile("pnts", feature, triangle_buffer()), document, error) == ERR_FILE_CORRUPT);
	feature["POINTS_LENGTH"] = 3;
	feature["POSITION"] = dictionary(R"({"byteOffset":68})");
	CHECK(parse(legacy_tile("pnts", feature, triangle_buffer()), document, error) == ERR_FILE_CORRUPT);
}

TEST_CASE("[3DTiles] I3DM expands embedded models with independent batch IDs") {
	Dictionary feature = dictionary(R"({"INSTANCES_LENGTH":2,"POSITION":{"byteOffset":0}})");
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(legacy_tile("i3dm", feature, triangle_buffer(), glb()), document, error) == OK);
	REQUIRE(document.surfaces.size() == 2);
	for (int i = 0; i < 2; i++) {
		REQUIRE(document.surfaces[i].vertices.size() == 3);
		REQUIRE(document.surfaces[i].batch_ids.size() == 3);
		for (int j = 0; j < 3; j++) CHECK(document.surfaces[i].batch_ids[j] == i);
	}
	CHECK((document.surfaces[1].vertices[0] - document.surfaces[0].vertices[0]).length() == doctest::Approx(1.0));
	feature["NORMAL_UP"] = dictionary(R"({"byteOffset":24})");
	CHECK(parse(legacy_tile("i3dm", feature, triangle_buffer(), glb()), document, error) == ERR_FILE_CORRUPT);
}

TEST_CASE("[3DTiles] I3DM retries external model and model-relative buffer dependencies") {
	const Dictionary feature = dictionary(R"({"INSTANCES_LENGTH":1,"POSITION":{"byteOffset":0}})");
	TiledMeshParseRequest request;
	request.uri = "res://tiles/instances.i3dm";
	request.root = true;
	request.bytes = legacy_tile("i3dm", feature, triangle_buffer(), String("models/tree.gltf").to_utf8_buffer(), 0);
	TiledMeshDocument document;
	Vector<String> dependencies;
	HashMap<String, PackedByteArray> cache;
	String error;
	REQUIRE(Tiles3DParser::parse(request, Tiles3DParseOptions(), cache, document, dependencies, error) == ERR_BUSY);
	REQUIRE(dependencies.size() == 1);
	CHECK(dependencies[0] == "res://tiles/models/tree.gltf");
	Dictionary manifest = triangle_json();
	Array buffers = manifest["buffers"];
	Dictionary buffer = buffers[0];
	buffer["uri"] = "geometry/mesh.bin";
	cache.insert("res://tiles/models/tree.gltf", JSON::stringify(manifest).to_utf8_buffer());
	REQUIRE(Tiles3DParser::parse(request, Tiles3DParseOptions(), cache, document, dependencies, error) == ERR_BUSY);
	REQUIRE(dependencies.size() == 1);
	CHECK(dependencies[0] == "res://tiles/models/geometry/mesh.bin");
	cache.insert(dependencies[0], triangle_buffer());
	REQUIRE(Tiles3DParser::parse(request, Tiles3DParseOptions(), cache, document, dependencies, error) == OK);
	CHECK(dependencies.is_empty());
	CHECK(document.surfaces.size() == 1);
	PackedByteArray invalid = String("models/tree.gltf").to_utf8_buffer();
	invalid.push_back(0);
	request.bytes = legacy_tile("i3dm", feature, triangle_buffer(), invalid, 0);
	CHECK(Tiles3DParser::parse(request, Tiles3DParseOptions(), cache, document, dependencies, error) == ERR_FILE_CORRUPT);
	request.bytes = legacy_tile("i3dm", feature, triangle_buffer(), String("   ").to_utf8_buffer(), 0);
	CHECK(Tiles3DParser::parse(request, Tiles3DParseOptions(), cache, document, dependencies, error) == ERR_FILE_CORRUPT);
}

TEST_CASE("[3DTiles] Ion options preserve ordinary sources and keep tokens out of URLs") {
	Ref<Tiles3DOptions> options;
	options.instantiate();
	CHECK(options->get_source_uri("res://tiles/tileset.json") == "res://tiles/tileset.json");
	CHECK(options->get_source_bearer_token().is_empty());
	options->set_ion_asset_id(42);
	CHECK(options->get_source_uri("").is_empty());
	options->set_ion_access_token("test-user-token");
	CHECK(options->get_source_uri("") == "https://api.cesium.com/v1/assets/42/endpoint");
	CHECK(options->get_source_bearer_token() == "test-user-token");
	options->set_ion_asset_id(0);
	CHECK(options->get_source_uri("https://example.test/tileset.json") == "https://example.test/tileset.json");
	CHECK(options->get_source_bearer_token().is_empty());
}

TEST_CASE("[3DTiles] Ion endpoint validates hosted tiles and rejects unsafe responses") {
	Tiles3DIonEndpoint endpoint;
	String error;
	CHECK(endpoint.parse(String(R"({"type":"3DTILES","url":"https://assets.cesium.com/42/tileset.json","accessToken":"asset-token"})").to_utf8_buffer(), error) == OK);
	CHECK(endpoint.access_token == "asset-token");
	CHECK(error.is_empty());
	for (const String &json : Vector<String>{
				 "not-json", "[]",
				 R"({"type":"TERRAIN"})",
				 R"({"type":"IMAGERY"})",
				 R"({"type":"3DTILES","externalType":"GOOGLE_3D_TILES"})",
				 R"({"type":"3DTILES","url":"http://assets.cesium.com/42/tileset.json","accessToken":"secret"})",
				 R"({"type":"3DTILES","url":"https://assets.cesium.com/42/tileset.json","accessToken":"secret\r\nInjected: yes"})",
				 R"({"type":"3DTILES","url":42,"accessToken":"secret"})" }) {
		CHECK(endpoint.parse(json.to_utf8_buffer(), error) != OK);
		CHECK(endpoint.url.is_empty());
		CHECK(endpoint.access_token.is_empty());
		CHECK_FALSE(error.contains("secret"));
	}
}

TEST_CASE("[3DTiles] Ion credentials are limited to HTTPS same-origin asset resources") {
	const String root = "https://assets.cesium.com/42/tileset.json";
	CHECK(TiledMeshDataSource::is_bearer_uri_allowed(root, "https://assets.cesium.com/42/sub/model.glb"));
	CHECK(TiledMeshDataSource::is_bearer_uri_allowed(root, "https://ASSETS.cesium.com:443/42/texture.png?v=1"));
	for (const String &uri : Vector<String>{
				 "https://evil.test/42/model.glb", "https://assets.cesium.com.evil.test/42/model.glb",
				 "http://assets.cesium.com/42/model.glb", "https://assets.cesium.com:444/42/model.glb",
				 "https://assets.cesium.com/43/model.glb", "https://assets.cesium.com/420/model.glb",
				 "https://assets.cesium.com/42/../43/model.glb", "https://assets.cesium.com/42/%2e%2e/43/model.glb",
				 "https://user@assets.cesium.com/42/model.glb", "res://42/model.glb" }) {
		CHECK_FALSE(TiledMeshDataSource::is_bearer_uri_allowed(root, uri));
	}
}

TEST_CASE("[3DTiles] Ion versioned endpoints preserve queries without widening credential scope") {
	Tiles3DIonEndpoint endpoint;
	String error;
	const String root = "https://assets.ion.cesium.com/354759/tileset.json?v=1";
	CHECK(endpoint.parse(String(R"({"type":"3DTILES","url":"https://assets.ion.cesium.com/354759/tileset.json?v=1","accessToken":"test-asset-token"})").to_utf8_buffer(), error) == OK);
	CHECK(endpoint.url == root);
	CHECK(error.is_empty());
	const String child = TiledMeshDataSource::resolve_uri(root, "sub/model.glb?v=2");
	CHECK(child == "https://assets.ion.cesium.com/354759/sub/model.glb?v=2");
	CHECK(TiledMeshDataSource::is_bearer_uri_allowed(root, child));
	CHECK(TiledMeshDataSource::is_bearer_uri_allowed(root, root));
	CHECK(TiledMeshDataSource::resolve_uri(root + "&next=/unrelated/file", "model.glb") == "https://assets.ion.cesium.com/354759/model.glb");
	CHECK_FALSE(TiledMeshDataSource::is_bearer_uri_allowed(root, "https://assets.ion.cesium.com/354760/model.glb?v=/354759/"));
	CHECK_FALSE(TiledMeshDataSource::is_bearer_uri_allowed(root, "https://evil.test/354759/model.glb?v=1"));
	CHECK_FALSE(TiledMeshDataSource::is_bearer_uri_allowed(root, "http://assets.ion.cesium.com/354759/model.glb?v=1"));
	CHECK_FALSE(TiledMeshDataSource::is_bearer_uri_allowed(root, "https://user@assets.ion.cesium.com/354759/model.glb?v=1"));
	CHECK(endpoint.parse(String(R"({"type":"3DTILES","url":"https://assets.ion.cesium.com/354759/tileset.json?v=1#fragment","accessToken":"test-asset-token"})").to_utf8_buffer(), error) != OK);
	CHECK(endpoint.url.is_empty());
	CHECK(endpoint.access_token.is_empty());
}

TEST_CASE("[3DTiles] Ion session reports errors and cancels endpoint tileset downloads") {
	Ref<Tiles3DOptions> options;
	options.instantiate();
	options->set_ion_asset_id(42);
	options->set_ion_access_token("user-token");
	Tiles3DFormatLoader loader;
	String error;
	TiledMeshFormatSession *session = loader.create_session(options, {}, error);
	REQUIRE(session != nullptr);
	TiledMeshParseRequest request;
	request.root = true;
	request.uri = options->get_source_uri("");
	request.bytes = String(R"({"type":"TERRAIN"})").to_utf8_buffer();
	uint64_t id = session->enqueue_parse(request, 1.0);
	TiledMeshParseResult result;
	CHECK(session->take_result(id, result));
	CHECK(result.error == ERR_UNAVAILABLE);
	request.bytes = String(R"({"type":"3DTILES","url":"https://assets.cesium.com/42/tileset.json","accessToken":"asset-token"})").to_utf8_buffer();
	id = session->enqueue_parse(request, 1.0);
	CHECK(id != 0);
	CHECK(session->get_bearer_token("https://assets.cesium.com/42/sub/texture.png") == "asset-token");
	CHECK(session->get_bearer_token("https://other.test/texture.png").is_empty());
	session->cancel(id);
	CHECK_FALSE(session->take_result(id, result));
	session->shutdown();
	CHECK(session->enqueue_parse(request, 1.0) == 0);
	memdelete(session);
}

class IonHTTPClient : public HTTPClient {
	Status status = STATUS_DISCONNECTED;
	String uri;

public:
	struct Response {
		int code = 200;
		PackedByteArray bytes;
		String location;
	};
	inline static HashMap<String, Response> responses;
	inline static HashMap<String, Vector<String>> requests;
	inline static int request_count = 0;
	inline static HTTPClient *(*previous_factory)(bool) = nullptr;
	static HTTPClient *create_mock(bool p_notify) { return static_cast<HTTPClient *>(ClassDB::creator<IonHTTPClient>(p_notify)); }
	static void install() {
		previous_factory = _create;
		_create = create_mock;
	}
	static void restore() {
		_create = previous_factory;
		responses.clear();
		requests.clear();
	}
	Error connect_to_host(const String &p_host, int p_port, Ref<TLSOptions> p_tls) override {
		uri = (p_tls.is_valid() ? "https://" : "http://") + p_host;
		status = STATUS_CONNECTED;
		return OK;
	}
	Error request(Method p_method, const String &p_url, const Vector<String> &p_headers, const uint8_t *p_body, int p_size) override {
		uri += p_url;
		request_count++;
		requests.insert(uri, p_headers);
		if (!responses.has(uri)) {
			Response missing;
			missing.code = 404;
			responses.insert(uri, missing);
		}
		status = STATUS_BODY;
		return OK;
	}
	void set_connection(const Ref<StreamPeer> &p_connection) override {}
	Ref<StreamPeer> get_connection() const override { return Ref<StreamPeer>(); }
	void close() override { status = STATUS_DISCONNECTED; }
	Status get_status() const override { return status; }
	bool has_response() const override { return status == STATUS_BODY; }
	bool is_response_chunked() const override { return false; }
	int get_response_code() const override { return responses[uri].code; }
	Error get_response_headers(List<String> *r_headers) override {
		if (!responses[uri].location.is_empty()) {
			r_headers->push_back("Location: " + responses[uri].location);
		}
		return OK;
	}
	int64_t get_response_body_length() const override { return responses[uri].bytes.size(); }
	PackedByteArray read_response_body_chunk() override {
		status = STATUS_CONNECTED;
		return responses[uri].bytes;
	}
	void set_blocking_mode(bool p_enabled) override {}
	bool is_blocking_mode_enabled() const override { return false; }
	void set_read_chunk_size(int p_size) override {}
	int get_read_chunk_size() const override { return 65536; }
	Error poll() override { return OK; }
};

struct IonHTTPFixture {
	IonHTTPFixture() { IonHTTPClient::install(); }
	~IonHTTPFixture() { IonHTTPClient::restore(); }
};

TEST_CASE("[3DTiles] Ion HTTP acquisition streams the resolved root and isolates redirect credentials") {
	IonHTTPFixture fixture;
	auto *queue = TiledMeshDataRequestQueue::get_singleton();
	REQUIRE(queue != nullptr);
	const String endpoint_uri = "https://api.cesium.com/v1/assets/42/endpoint";
	const String root_uri = "https://assets.cesium.com/42/tileset.json";
	IonHTTPClient::Response endpoint;
	endpoint.bytes = String(R"({"type":"3DTILES","url":"https://assets.cesium.com/42/tileset.json","accessToken":"asset-token"})").to_utf8_buffer();
	IonHTTPClient::responses.insert(endpoint_uri, endpoint);
	IonHTTPClient::Response root;
	root.bytes = tileset(R"({"boundingVolume":{"sphere":[0,0,0,1]},"geometricError":0,"content":{"uri":"sub/model.glb"}})");
	IonHTTPClient::responses.insert(root_uri, root);
	const uint64_t download = queue->request(ObjectID(), 1, endpoint_uri, 1024 * 1024, 1, 0, 1, "user-token");
	TiledMeshDataRequestQueue::Result data;
	bool ready = false;
	for (int i = 0; i < 20 && !ready; i++) {
		queue->poll();
		ready = queue->take_result(download, data);
	}
	REQUIRE(ready);
	CHECK(data.error == OK);
	CHECK(IonHTTPClient::requests[endpoint_uri].has("Authorization: Bearer user-token"));
	Ref<Tiles3DOptions> options;
	options.instantiate();
	options->set_ion_asset_id(42);
	Tiles3DFormatLoader loader;
	String error;
	TiledMeshFormatSession *session = loader.create_session(options, {}, error);
	REQUIRE(session != nullptr);
	TiledMeshParseRequest request;
	request.root = true;
	request.uri = endpoint_uri;
	request.bytes = data.bytes;
	const uint64_t parse_id = session->enqueue_parse(request, 1);
	TiledMeshParseResult result;
	ready = false;
	for (int i = 0; i < 1000 && !ready; i++) {
		queue->poll();
		ready = session->take_result(parse_id, result);
		OS::get_singleton()->delay_usec(1000);
	}
	CHECK(ready);
	CHECK(result.error == OK);
	CHECK(result.uri == root_uri);
	CHECK_FALSE(result.document.tiles.is_empty());
	CHECK(IonHTTPClient::requests[root_uri].has("Authorization: Bearer asset-token"));
	CHECK_FALSE(IonHTTPClient::requests[root_uri].has("Authorization: Bearer user-token"));
	session->shutdown();
	memdelete(session);

	IonHTTPClient::Response redirect;
	redirect.code = 302;
	redirect.location = "https://other.test/model.glb";
	IonHTTPClient::responses.insert(root_uri, redirect);
	const uint64_t redirected = queue->request(ObjectID(), 1, root_uri, 1024, 1, 0, 1, "asset-token");
	ready = false;
	for (int i = 0; i < 30 && !ready; i++) {
		queue->poll();
		ready = queue->take_result(redirected, data);
	}
	CHECK(ready);
	CHECK(data.error != OK);
	CHECK_FALSE(data.error_message.contains("asset-token"));
	CHECK_FALSE(IonHTTPClient::requests[redirect.location].has("Authorization: Bearer asset-token"));
	IonHTTPClient::Response denied;
	denied.code = 401;
	IonHTTPClient::responses.insert(root_uri, denied);
	const int before = IonHTTPClient::request_count;
	const uint64_t first = queue->request(ObjectID(), 1, root_uri, 1024, 1, 0, 1, "first-token");
	const uint64_t second = queue->request(ObjectID(), 1, root_uri, 1024, 1, 0, 1, "second-token");
	for (int i = 0; i < 20; i++) {
		queue->poll();
	}
	CHECK(IonHTTPClient::request_count == before + 2);
	CHECK(queue->take_result(first, data));
	CHECK(data.response_code == 401);
	CHECK(data.error != OK);
	CHECK_FALSE(data.error_message.contains("first-token"));
	CHECK(queue->take_result(second, data));
	CHECK(data.response_code == 401);
}

TEST_CASE("[3DTiles][SceneTree] Editor ion properties preview changes and scene persistence") {
	IonHTTPFixture fixture;
	struct EditorHintGuard {
		bool previous = Engine::get_singleton()->is_editor_hint();
		EditorHintGuard() { Engine::get_singleton()->set_editor_hint(true); }
		~EditorHintGuard() { Engine::get_singleton()->set_editor_hint(previous); }
	} editor_hint_guard;

	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_editor_preview_enabled(false);
	SceneTree::get_singleton()->get_root()->add_child(node);
	CHECK_FALSE(node->get_configuration_warnings().is_empty());
	const auto set_property = [](Object *p_object, const StringName &p_name, const Variant &p_value) {
		bool valid = false;
		p_object->set(p_name, p_value, &valid);
		return valid;
	};
	Ref<Tiles3DOptions> options;
	options.instantiate();
	CHECK(set_property(options.ptr(), "ion_asset_id", 42));
	CHECK(set_property(node, "format_options", options));
	CHECK_FALSE(node->get_configuration_warnings().is_empty());
	CHECK(set_property(options.ptr(), "ion_access_token", "test-user-token"));
	CHECK(node->get_configuration_warnings().is_empty());
	MessageQueue::get_singleton()->flush();
	CHECK_FALSE(node->is_open());

	CHECK(set_property(node, "editor_preview_enabled", true));
	CHECK(node->is_open());
	CHECK(node->get_source_uri().is_empty());
	const uint64_t generation = node->get_streaming_stats()["generation"];
	CHECK(set_property(options.ptr(), "ion_asset_id", 43));
	MessageQueue::get_singleton()->flush();
	CHECK(node->is_open());
	CHECK(uint64_t(node->get_streaming_stats()["generation"]) > generation);

	node->close();
	Ref<Tiles3DOptions> replacement;
	replacement.instantiate();
	replacement->set_ion_asset_id(44);
	replacement->set_ion_access_token("replacement-test-token");
	node->set_format_options(replacement);
	MessageQueue::get_singleton()->flush();
	CHECK(node->is_open());
	const uint64_t replaced_generation = node->get_streaming_stats()["generation"];
	options->set_ion_asset_id(45);
	MessageQueue::get_singleton()->flush();
	CHECK(uint64_t(node->get_streaming_stats()["generation"]) == replaced_generation);

	CHECK(set_property(node, "editor_preview_enabled", false));
	CHECK_FALSE(node->is_open());
	replacement->set_ion_asset_id(46);
	MessageQueue::get_singleton()->flush();
	CHECK_FALSE(node->is_open());
	Ref<PackedScene> scene;
	scene.instantiate();
	CHECK(scene->pack(node) == OK);
	Node *restored = scene->instantiate();
	TiledMeshInstance3D *restored_tiles = Object::cast_to<TiledMeshInstance3D>(restored);
	CHECK(restored_tiles != nullptr);
	if (restored_tiles) {
		Ref<Tiles3DOptions> restored_options = restored_tiles->get_format_options();
		CHECK(restored_options.is_valid());
		if (restored_options.is_valid()) {
			CHECK(restored_options->get_ion_asset_id() == 46);
			CHECK(restored_options->get_ion_access_token() == "replacement-test-token");
		}
		CHECK(restored_tiles->get_source_uri().is_empty());
		CHECK_FALSE(restored_tiles->is_editor_preview_enabled());
	}
	memdelete(restored);
	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
}

TEST_CASE("[3DTiles] Recognition requires a tileset structure for generic JSON") {
	Tiles3DFormatLoader loader;
	CHECK(loader.recognize("res://unknown.dat", glb()) == 100);
	CHECK(loader.recognize("https://example.test/model.b3dm?key=test", {}) > 0);
	CHECK(loader.recognize("res://ordinary.json", String("{\"asset\":{},\"value\":1}").to_utf8_buffer()) == 0);
	CHECK(loader.recognize("res://anything.dat", tileset(R"({"boundingVolume":{"sphere":[0,0,0,1]},"geometricError":0})")) == 100);
	CHECK(TiledMeshFormatRegistry::find_by_id(SNAME("3dtiles")) != nullptr);
	Ref<TiledMeshFormatOptions> incompatible;
	incompatible.instantiate();
	CHECK_FALSE(loader.supports_options(incompatible));
	String error;
	CHECK(loader.create_session(incompatible, {}, error) == nullptr);
	CHECK_FALSE(error.is_empty());
}

TEST_CASE("[3DTiles] Explicit hierarchy preserves transforms parent SSE refinement and multiple contents") {
	TiledMeshDocument document;
	String error;
	const PackedByteArray bytes = tileset(R"({
		"boundingVolume":{"box":[0,0,0,10,0,0,0,10,0,0,0,10]}, "geometricError":64, "refine":"ADD",
		"transform":[2,0,0,0,0,2,0,0,0,0,2,0,6378137.125,0,0,1], "content":{"uri":"root.glb"},
		"children":[{"boundingVolume":{"sphere":[1,2,3,4]},"geometricError":8,"refine":"REPLACE",
		"transform":[1,0,0,0,0,1,0,0,0,0,1,0,0.25,0,0,1],
		"contents":[{"uri":"a.glb"},{"url":"b.b3dm"}],
		"children":[{"boundingVolume":{"sphere":[0,0,0,1]},"geometricError":0,"content":{"uri":"fine.glb"}}]}]
	})");
	Tiles3DParseOptions options;
	options.coordinate_system = Tiles3DOptions::COORDINATE_SYSTEM_CARTESIAN;
	REQUIRE(parse(bytes, document, error, {}, options) == OK);
	REQUIRE(document.tiles.size() == 3);
	CHECK(document.source_origin.x == doctest::Approx(6378137.125));
	CHECK(document.tiles[1].parent == 0);
	CHECK(document.tiles[1].refinement == TiledMeshTileDefinition::REFINE_ADD);
	CHECK(document.tiles[2].refinement == TiledMeshTileDefinition::REFINE_REPLACE);
	CHECK(document.tiles[1].selection.geometric_error == doctest::Approx(128.0));
	CHECK(document.tiles[2].selection.geometric_error == doctest::Approx(16.0));
	CHECK(document.tiles[1].bounding_volume.center.x == doctest::Approx(6378139.625));
	CHECK(document.tiles[1].bounding_volume.radius == doctest::Approx(8));
	REQUIRE(document.tiles[1].contents.size() == 2);
	CHECK(document.tiles[1].contents[1].uri == "res://tiles/b.b3dm");
	TiledMeshDocument mesh;
	REQUIRE(parse(glb(), mesh, error, document.tiles[1].contents[0].context, options) == OK);
	REQUIRE(mesh.surfaces.size() == 1);
	CHECK(mesh.source_origin.x == doctest::Approx(6378139.625));
	CHECK(mesh.surfaces[0].vertices[0].is_equal_approx(Vector3(-2, -6, 4)));
	CHECK(mesh.surfaces[0].vertices[1].is_equal_approx(Vector3(0, -6, 4)));
	CHECK(mesh.surfaces[0].replacement_group == 0);
}

TEST_CASE("[3DTiles] Region bounds ignore transforms and cover the date line") {
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(tileset(R"({"boundingVolume":{"region":[3.13,-0.01,-3.13,0.01,0,100]},"geometricError":0,
		"transform":[1,0,0,0,0,1,0,0,0,0,1,0,100000000,0,0,1]})"),
					document, error) == OK);
	CHECK(document.source_origin.x < -6300000);
	CHECK(Math::abs(document.source_origin.y) < 0.01);
	CHECK(document.tiles[0].bounding_volume.radius < 200000);
	const Vector3 up = document.coordinate_frame.source_to_local.xform(Vector3(-1, 0, 0));
	CHECK(up.is_equal_approx(Vector3(0, 1, 0)));
}

TEST_CASE("[3DTiles] External tilesets inherit context and reject cycles") {
	TiledMeshDocument root, nested, mesh;
	String error;
	REQUIRE(parse(tileset(R"({"boundingVolume":{"sphere":[5,0,0,10]},"geometricError":8,
		"transform":[1,0,0,0,0,1,0,0,0,0,1,0,20,0,0,1],"content":{"uri":"nested.json"}})"),
					root, error) == OK);
	TiledMeshParseRequest request;
	request.uri = "res://tiles/nested.json";
	request.bytes = tileset(R"({"boundingVolume":{"sphere":[0,0,0,10]},"geometricError":0,
		"transform":[1,0,0,0,0,1,0,0,0,0,1,0,3,0,0,1],"content":{"uri":"mesh.glb"}})");
	request.context = root.tiles[0].contents[0].context;
	Vector<String> dependencies;
	REQUIRE(Tiles3DParser::parse(request, {}, {}, nested, dependencies, error) == OK);
	CHECK(nested.tiles[0].bounding_volume.center.x == doctest::Approx(23));
	CHECK(nested.tiles[0].provider_key != root.tiles[0].provider_key);
	request.uri = "res://tiles/tileset.json";
	CHECK(Tiles3DParser::parse(request, {}, {}, mesh, dependencies, error) != OK);
	CHECK(error.contains("Cyclic"));
}

TEST_CASE("[3DTiles] GLB winding normals and scale match Godot") {
	TiledMeshDocument document;
	String error;
	Tiles3DParseOptions options;
	options.session.unit_scale = 2;
	REQUIRE(parse(glb(), document, error, {}, options) == OK);
	REQUIRE(document.surfaces.size() == 1);
	const TiledMeshMeshSurfacePayload &surface = document.surfaces[0];
	CHECK(surface.vertices[1].is_equal_approx(Vector3(2, 0, 0)));
	CHECK(surface.vertices[2].is_equal_approx(Vector3(0, 2, 0)));
	CHECK(surface.normals[0].is_equal_approx(Vector3(0, 0, 1)));
	CHECK(surface.indices[0] == 0);
	CHECK(surface.indices[1] == 2);
	CHECK(surface.indices[2] == 1);
	CHECK(document.estimated_cpu_bytes > 0);
}

TEST_CASE("[3DTiles] b3dm Batch Table preserves JSON and binary metadata") {
	TiledMeshDocument document;
	String error;
	const Error result = parse(metadata_b3dm(), document, error);
	INFO(std::string(error.utf8().get_data()));
	REQUIRE(result == OK);
	if (result != OK) {
		return;
	}
	CHECK(document.warnings.is_empty());
	REQUIRE(document.batch_tables.size() == 1);
	CHECK(document.batch_tables[0].batch_length == 1);
	const Dictionary properties = document.batch_tables[0].properties;
	CHECK(Array(properties["name"])[0] == "building");
	CHECK(bool(Array(properties["enabled"])[0]));
	CHECK(Array(properties["optional"])[0].get_type() == Variant::NIL);
	CHECK(int(Dictionary(Array(properties["details"])[0])["height"]) == 12);
	CHECK(double(Array(properties["temperature"])[0]) == doctest::Approx(18.5));
	CHECK(int(Array(Array(properties["offset"])[0])[0]) == -2);
	CHECK(int(Array(Array(properties["offset"])[0])[1]) == 7);
	REQUIRE(document.surfaces.size() == 1);
	CHECK(document.surfaces[0].batch_ids == PackedInt32Array({ 0, 0, 0 }));
	CHECK(document.surfaces[0].batch_table == 0);
}

TEST_CASE("[3DTiles] b3dm Batch Table decodes every legacy binary component type") {
	for (const String &component_type : Vector<String>{ "BYTE", "UNSIGNED_BYTE", "SHORT", "UNSIGNED_SHORT", "INT", "UNSIGNED_INT", "FLOAT", "DOUBLE" }) {
		Dictionary descriptor;
		descriptor["byteOffset"] = 0;
		descriptor["componentType"] = component_type;
		descriptor["type"] = "SCALAR";
		Dictionary source;
		source["value"] = descriptor;
		TiledMeshDocument document;
		String error;
		INFO(std::string(component_type.utf8().get_data()));
		REQUIRE(parse(metadata_b3dm(source), document, error) == OK);
		const Variant decoded = Array(document.batch_tables[0].properties["value"])[0];
		CHECK((decoded.get_type() == Variant::INT || decoded.get_type() == Variant::FLOAT));
	}
}

TEST_CASE("[3DTiles] b3dm Batch Table rejects invalid descriptors and counts") {
	for (const Dictionary &table : Vector<Dictionary>{
				dictionary(R"({"name":[]})"),
				dictionary(R"({"value":{"byteOffset":1,"componentType":"SHORT","type":"SCALAR"}})"),
				dictionary(R"({"value":{"byteOffset":0,"componentType":"HALF","type":"SCALAR"}})"),
				dictionary(R"({"value":{"byteOffset":16,"componentType":"FLOAT","type":"SCALAR"}})"),
				dictionary(R"({"value":{"byteOffset":0,"componentType":"FLOAT","type":"MAT4"}})") }) {
		TiledMeshDocument document;
		String error;
		CHECK(parse(metadata_b3dm(table), document, error) == ERR_FILE_CORRUPT);
		CHECK_FALSE(error.is_empty());
	}
}

TEST_CASE("[3DTiles] b3dm accepts JSON and binary RTC_CENTER with GLB padding") {
	for (bool binary : { false, true }) {
		TiledMeshDocument document;
		String error;
		Tiles3DParseOptions options;
		options.coordinate_system = Tiles3DOptions::COORDINATE_SYSTEM_CARTESIAN;
		options.session.use_format_default_origin = false;
		options.session.explicit_origin = { 6378137, 20, 30 };
		REQUIRE(parse(batched(Vector3(6378137, 20, 30), binary), document, error, {}, options) == OK);
		CHECK(document.surfaces[0].vertices[0].is_equal_approx(Vector3()));
		CHECK(document.surfaces[0].vertices[1].is_equal_approx(Vector3(1, 0, 0)));
	}
}

TEST_CASE("[3DTiles] b3dm accepts a complete GLB without final 8-byte padding and rejects corrupt boundaries") {
	Dictionary manifest = triangle_json();
	manifest["extras"] = "b3dm";
	PackedByteArray model = glb(manifest);
	if (model.size() % 8 == 0) {
		manifest["extras"] = "b3dmxxxx";
		model = glb(manifest);
	}
	REQUIRE(model.size() % 8 == 4);
	const PackedByteArray unpadded = batched_glb(model, false);
	REQUIRE(unpadded.size() % 8 == 4);
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(unpadded, document, error) == OK);
	REQUIRE(document.surfaces.size() == 1);
	CHECK(document.surfaces[0].vertices.size() == 3);
	CHECK(parse(batched_glb(model), document, error) == OK);
	const int payload_offset = unpadded.size() - model.size();
	for (int corruption = 0; corruption < 7; corruption++) {
		PackedByteArray bytes = unpadded;
		switch (corruption) {
			case 0: // Incorrect container length.
				encode_uint32(bytes.size() + 4, bytes.ptrw() + 8);
				break;
			case 1: // Truncated GLB, with an otherwise correct container header.
				bytes.resize(bytes.size() - 8);
				encode_uint32(bytes.size(), bytes.ptrw() + 8);
				break;
			case 2: // A table boundary that is only 4-byte aligned is still invalid.
				encode_uint32(decode_uint32(bytes.ptr() + 12) + 4, bytes.ptrw() + 12);
				break;
			case 3: // Oversized table.
				encode_uint32(UINT32_MAX, bytes.ptrw() + 16);
				break;
			case 4: // Correct outer length cannot hide a corrupt GLB chunk.
				encode_uint32(UINT32_MAX, bytes.ptrw() + payload_offset + 12);
				break;
			case 5: // The compatibility path must end exactly at the GLB boundary.
				encode_uint32(model.size() - 4, bytes.ptrw() + payload_offset + 8);
				break;
			case 6: // Non-4-byte-aligned containers remain invalid.
				bytes.push_back(0);
				encode_uint32(bytes.size(), bytes.ptrw() + 8);
				break;
		}
		INFO(corruption);
		CHECK(parse(bytes, document, error) == ERR_FILE_CORRUPT);
		CHECK_FALSE(error.is_empty());
		CHECK(document.surfaces.is_empty());
	}
}

TEST_CASE("[3DTiles] Composite models share one origin across different RTC centers") {
	PackedByteArray bytes;
	bytes.resize(16);
	memcpy(bytes.ptrw(), "cmpt", 4);
	encode_uint32(1, bytes.ptrw() + 4);
	encode_uint32(2, bytes.ptrw() + 12);
	bytes.append_array(batched(Vector3(10, 0, 0)));
	bytes.append_array(batched(Vector3(20, 0, 0)));
	encode_uint32(bytes.size(), bytes.ptrw() + 8);
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(bytes, document, error) == OK);
	REQUIRE(document.surfaces.size() == 2);
	CHECK(document.surfaces[0].vertices[0].x == doctest::Approx(0));
	CHECK(document.surfaces[1].vertices[0].x == doctest::Approx(10));
}

TEST_CASE("[3DTiles] Draco-compressed GLB and b3dm use the shared decoder") {
	const PackedByteArray fixture = FileAccess::get_file_as_bytes("modules/draco/tests/data/Box_Draco.glb");
	REQUIRE(fixture.size() == 2388);

	for (const PackedByteArray &bytes : Vector<PackedByteArray>{ fixture, batched_glb(fixture) }) {
		TiledMeshDocument document;
		String error;
		const Error result = parse(bytes, document, error);
		INFO(std::string(error.utf8().get_data()));
		REQUIRE(result == OK);
		if (result != OK) {
			continue;
		}
		REQUIRE(document.surfaces.size() == 1);
		if (document.surfaces.size() != 1) {
			continue;
		}
		CHECK(document.surfaces[0].vertices.size() == 24);
		CHECK(document.surfaces[0].normals.size() == 24);
		CHECK(document.surfaces[0].indices.size() == 36);
		CHECK(document.materials.size() == 1);
	}
}

TEST_CASE("[3DTiles] Corrupt containers accessors and unsupported required extensions fail without partial geometry") {
	TiledMeshDocument document;
	String error;
	PackedByteArray bytes = glb();
	encode_uint32(UINT32_MAX, bytes.ptrw() + 12);
	CHECK(parse(bytes, document, error) == ERR_FILE_CORRUPT);
	bytes = batched();
	encode_uint32(UINT32_MAX, bytes.ptrw() + 16);
	CHECK(parse(bytes, document, error) == ERR_FILE_CORRUPT);
	Dictionary invalid = triangle_json();
	Array accessors = invalid["accessors"];
	Dictionary accessor = accessors[0];
	accessor["count"] = INT32_MAX;
	CHECK(parse(glb(invalid), document, error) == ERR_FILE_CORRUPT);
	CHECK(document.surfaces.is_empty());
	invalid = triangle_json();
	invalid["extensionsRequired"] = Array({ "EXT_unsupported_test" });
	CHECK(parse(glb(invalid), document, error) == ERR_UNAVAILABLE);
	CHECK(error.contains("EXT_unsupported_test"));
	CHECK(parse(tileset(R"({"boundingVolume":{"sphere":[0,0,0,1]},"geometricError":0,"implicitTiling":{}})"), document, error) == ERR_UNAVAILABLE);
	CHECK(parse(tileset(R"({"boundingVolume":{"sphere":[0,0,0,-1]},"geometricError":0})"), document, error) == ERR_FILE_CORRUPT);
}

TEST_CASE("[3DTiles] Empty intermediate tiles stay traversable even with zero error") {
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(tileset(R"({"boundingVolume":{"sphere":[0,0,0,10]},"geometricError":0,
		"children":[{"boundingVolume":{"sphere":[0,0,0,10]},"geometricError":0,"content":{"uri":"leaf.glb"}}]})"),
					document, error) == OK);
	CHECK(document.tiles[1].selection.mode == TiledMeshSelectionRule::MODE_ALWAYS);
}

TEST_CASE("[3DTiles] Interleaved and sparse accessors decode without losing attributes") {
	Dictionary manifest = triangle_json();
	Array accessors = manifest["accessors"];
	Dictionary positions = accessors[0];
	Dictionary normals = accessors[1];
	normals["byteOffset"] = 12;
	Array views = manifest["bufferViews"];
	Dictionary interleaved_view = views[0];
	interleaved_view["byteStride"] = 24;
	PackedByteArray bytes;
	bytes.resize(72);
	const float values[] = { 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1 };
	for (int i = 0; i < 18; i++) {
		encode_float(values[i], bytes.ptrw() + i * 4);
	}
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(glb(manifest, bytes), document, error) == OK);
	CHECK(document.surfaces[0].vertices[2].is_equal_approx(Vector3(0, 1, 0)));
	CHECK(document.surfaces[0].normals[1].is_equal_approx(Vector3(0, 0, 1)));

	// Sparse POSITION starts at zero and overrides vertices 1 and 2.
	positions.erase("bufferView");
	positions["sparse"] = dictionary(R"({"count":2,"indices":{"bufferView":1,"componentType":5121},"values":{"bufferView":2}})");
	views.push_back(dictionary(R"({"buffer":0,"byteOffset":72,"byteLength":2})"));
	views.push_back(dictionary(R"({"buffer":0,"byteOffset":76,"byteLength":24})"));
	bytes.resize(100);
	bytes.set(72, 1);
	bytes.set(73, 2);
	const float sparse_values[] = { 2, 0, 0, 0, 3, 0 };
	for (int i = 0; i < 6; i++) {
		encode_float(sparse_values[i], bytes.ptrw() + 76 + i * 4);
	}
	Array buffers = manifest["buffers"];
	Dictionary buffer = buffers[0];
	buffer["byteLength"] = 100;
	REQUIRE(parse(glb(manifest, bytes), document, error) == OK);
	CHECK(document.surfaces[0].vertices[1].is_equal_approx(Vector3(2, 0, 0)));
	CHECK(document.surfaces[0].vertices[2].is_equal_approx(Vector3(0, 3, 0)));
	bytes.set(73, 1);
	CHECK(parse(glb(manifest, bytes), document, error) == ERR_FILE_CORRUPT);
}

TEST_CASE("[3DTiles] Mirrored small-scale nodes preserve winding and generate flat normals") {
	Dictionary manifest = triangle_json();
	Array nodes = manifest["nodes"];
	Dictionary node = nodes[0];
	node["scale"] = Array({ -0.001, 0.001, 0.001 });
	Array meshes = manifest["meshes"];
	Dictionary mesh = meshes[0];
	Array primitives = mesh["primitives"];
	Dictionary primitive = primitives[0];
	Dictionary attributes = primitive["attributes"];
	attributes.erase("NORMAL");
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(glb(manifest), document, error) == OK);
	CHECK(document.surfaces[0].vertices[1].x == doctest::Approx(-0.001));
	CHECK(document.surfaces[0].normals[0].is_equal_approx(Vector3(0, 0, 1)));
	CHECK(document.surfaces[0].indices[2] == 2);
}

TEST_CASE("[3DTiles] Embedded base color texture and material factors produce CPU payloads") {
	Dictionary manifest = triangle_json();
	Ref<Image> image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	image->set_pixel(0, 0, Color(1, 0, 0));
	const PackedByteArray png = image->save_png_to_buffer();
	REQUIRE_FALSE(png.is_empty());
	Dictionary image_json;
	image_json["uri"] = "data:image/png;base64," + CryptoCore::b64_encode_str(png.ptr(), png.size());
	manifest["images"] = Array({ image_json });
	manifest["textures"] = Array({ dictionary(R"({"source":0})") });
	manifest["materials"] = Array({ dictionary(R"({"pbrMetallicRoughness":{"baseColorFactor":[0.25,0.5,1,0.75],
		"baseColorTexture":{"index":0},"metallicFactor":0.25,"roughnessFactor":0.75},"alphaMode":"MASK","alphaCutoff":0.4,"doubleSided":true})") });
	Array meshes = manifest["meshes"];
	Dictionary mesh = meshes[0];
	Array primitives = mesh["primitives"];
	Dictionary primitive = primitives[0];
	primitive["material"] = 0;
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(glb(manifest), document, error) == OK);
	REQUIRE(document.materials.size() == 1);
	const TiledMeshMaterialPayload material = document.materials[0];
	CHECK(material.albedo_image.has_pixels());
	CHECK(material.albedo_image.rgba8[0] == 255);
	CHECK(material.albedo_image.rgba8[1] == 0);
	CHECK(material.metallic == doctest::Approx(0.25));
	CHECK(material.roughness == doctest::Approx(0.75));
	CHECK(material.alpha_test);
	CHECK(material.double_sided);
	CHECK(material.alpha_cutoff == doctest::Approx(0.4));
	image_json["uri"] = "textures/wall.png";
	REQUIRE(parse(glb(manifest), document, error) == OK);
	CHECK(document.materials[0].albedo_image.uri == "res://tiles/textures/wall.png");
}

TEST_CASE("[3DTiles] Required unlit materials preserve base color and remain distinct from lit materials") {
	Dictionary manifest = triangle_json();
	manifest["extensionsRequired"] = Array({ "KHR_materials_unlit" });
	Dictionary unlit = dictionary(R"({"extensions":{"KHR_materials_unlit":{}},"pbrMetallicRoughness":{"baseColorFactor":[0.25,0.5,1,0.75],"baseColorTexture":{"index":0}},"alphaMode":"BLEND","doubleSided":true})");
	Dictionary lit = unlit.duplicate(true);
	lit.erase("extensions");
	manifest["materials"] = Array({ unlit, lit });
	manifest["images"] = Array({ dictionary(R"({"uri":"textures/wall.png"})") });
	manifest["textures"] = Array({ dictionary(R"({"source":0})") });
	Array meshes = manifest["meshes"];
	Dictionary mesh = meshes[0];
	Array primitives = mesh["primitives"];
	Dictionary primitive = primitives[0];
	primitive["material"] = 0;
	Dictionary second = primitive.duplicate(true);
	second["material"] = 1;
	primitives.push_back(second);
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(glb(manifest), document, error) == OK);
	REQUIRE(document.materials.size() == 2);
	CHECK(document.materials[0].unlit);
	CHECK_FALSE(document.materials[1].unlit);
	CHECK(document.surfaces[0].material != document.surfaces[1].material);
	CHECK(document.materials[0].transparent);
	CHECK(document.materials[0].double_sided);
	CHECK(document.materials[0].albedo.is_equal_approx(Color(0.25, 0.5, 1, 0.75).linear_to_srgb()));
	CHECK(document.materials[0].albedo_image.uri == "res://tiles/textures/wall.png");
	unlit["alphaMode"] = "MASK";
	REQUIRE(parse(glb(manifest), document, error) == OK);
	CHECK(document.materials[0].alpha_test);
	CHECK(document.materials[0].unlit);
	Dictionary extensions = unlit["extensions"];
	extensions["KHR_materials_unlit"] = 1;
	CHECK(parse(glb(manifest), document, error) == ERR_FILE_CORRUPT);
}

TEST_CASE("[3DTiles] Inline glTF buffers and normalized vertex colors load from JSON") {
	Dictionary manifest = triangle_json();
	PackedByteArray bytes = triangle_buffer();
	bytes.resize(84);
	for (int i = 72; i < 84; i++) {
		bytes.set(i, 255);
	}
	bytes.set(73, 0);
	bytes.set(74, 0);
	Array views = manifest["bufferViews"];
	Dictionary view = views[0];
	view["byteLength"] = bytes.size();
	Array accessors = manifest["accessors"];
	accessors.push_back(dictionary(R"({"bufferView":0,"byteOffset":72,"componentType":5121,"count":3,"type":"VEC4","normalized":true})"));
	Array meshes = manifest["meshes"];
	Dictionary mesh = meshes[0];
	Array primitives = mesh["primitives"];
	Dictionary primitive = primitives[0];
	Dictionary attributes = primitive["attributes"];
	attributes["COLOR_0"] = 2;
	Array buffers = manifest["buffers"];
	Dictionary buffer = buffers[0];
	buffer["byteLength"] = bytes.size();
	buffer["uri"] = "data:application/octet-stream;base64," + CryptoCore::b64_encode_str(bytes.ptr(), bytes.size());
	TiledMeshDocument document;
	String error;
	REQUIRE(parse(JSON::stringify(manifest).to_utf8_buffer(), document, error) == OK);
	CHECK(document.surfaces[0].colors[0].is_equal_approx(Color(1, 0, 0, 1)));
	CHECK(document.surfaces[0].colors[1].is_equal_approx(Color(1, 1, 1, 1)));
}

static void write_file(const String &p_path, const PackedByteArray &p_bytes) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_buffer(p_bytes);
}

TEST_CASE("[3DTiles] Session downloads external buffers and supports cancellation and shutdown") {
	const String path = TestUtils::get_temp_path("tiles3d_external.gltf");
	write_file(path.get_base_dir().path_join("tiles3d_external.bin"), triangle_buffer());
	Dictionary manifest = triangle_json();
	Array buffers = manifest["buffers"];
	Dictionary buffer = buffers[0];
	buffer["uri"] = "tiles3d_external.bin";
	TiledMeshParseRequest request;
	request.uri = path;
	request.bytes = JSON::stringify(manifest).to_utf8_buffer();
	request.root = true;
	request.generation = 42;
	Tiles3DFormatLoader loader;
	String error;
	TiledMeshFormatSession *session = loader.create_session({}, {}, error);
	REQUIRE(session != nullptr);
	const uint64_t cancelled = session->enqueue_parse(request, 0);
	session->cancel(cancelled);
	const uint64_t id = session->enqueue_parse(request, 1);
	session->update_priority(id, 2);
	TiledMeshParseResult result;
	bool completed = false;
	for (int i = 0; i < 2000 && !completed; i++) {
		TiledMeshDataRequestQueue::get_singleton()->poll();
		completed = session->take_result(id, result);
		if (!completed) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	CHECK(completed);
	CHECK(result.error == OK);
	CHECK(result.generation == 42);
	CHECK(result.document.surfaces.size() == 1);
	CHECK_FALSE(session->take_result(cancelled, result));
	session->enqueue_parse(request, 0);
	session->shutdown();
	session->shutdown();
	CHECK(session->enqueue_parse(request, 0) == 0);
	memdelete(session);
}

TEST_CASE("[3DTiles][SceneTree] Ion loads without source URI and authenticates buffers and textures") {
	IonHTTPFixture fixture;
	const String base = "https://assets.cesium.com/42/";
	IonHTTPClient::Response response;
	response.bytes = String(R"({"type":"3DTILES","url":"https://assets.cesium.com/42/tileset.json","accessToken":"asset-token"})").to_utf8_buffer();
	IonHTTPClient::responses.insert("https://api.cesium.com/v1/assets/42/endpoint", response);
	response.bytes = tileset(R"({"boundingVolume":{"sphere":[0,0,0,10]},"geometricError":0,"content":{"uri":"sub/model.gltf"}})");
	IonHTTPClient::responses.insert(base + "tileset.json", response);
	Dictionary manifest = triangle_json();
	manifest["buffers"] = Array({ dictionary(R"({"byteLength":72,"uri":"mesh.bin"})") });
	manifest["images"] = Array({ dictionary(R"({"uri":"texture.png"})") });
	manifest["textures"] = Array({ dictionary(R"({"source":0})") });
	manifest["materials"] = Array({ dictionary(R"({"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}})") });
	Array meshes = manifest["meshes"];
	Dictionary mesh = meshes[0];
	Array primitives = mesh["primitives"];
	Dictionary primitive = primitives[0];
	primitive["material"] = 0;
	response.bytes = JSON::stringify(manifest).to_utf8_buffer();
	IonHTTPClient::responses.insert(base + "sub/model.gltf", response);
	response.bytes = triangle_buffer();
	IonHTTPClient::responses.insert(base + "sub/mesh.bin", response);
	Ref<Image> image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 0));
	response.bytes = image->save_png_to_buffer();
	IonHTTPClient::responses.insert(base + "sub/texture.png", response);

	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 2, 5));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	Ref<Tiles3DOptions> options;
	options.instantiate();
	options->set_ion_asset_id(42);
	options->set_ion_access_token("user-token");
	node->set_format_options(options);
	SceneTree::get_singleton()->get_root()->add_child(node);
	CHECK(node->open() == OK);
	bool ready = false;
	for (int i = 0; i < 2000 && !ready; i++) {
		SceneTree::get_singleton()->process(0.001);
		ready = int(node->get_streaming_stats()["resident_render_instances"]) == 1 && IonHTTPClient::requests.has(base + "sub/texture.png");
		OS::get_singleton()->delay_usec(1000);
	}
	CHECK(ready);
	for (const String &path : Vector<String>{ "tileset.json", "sub/model.gltf", "sub/mesh.bin", "sub/texture.png" }) {
		CHECK(IonHTTPClient::requests.has(base + path));
		CHECK(IonHTTPClient::requests[base + path].has("Authorization: Bearer asset-token"));
	}
	node->close();
	IonHTTPClient::requests.clear();
	CHECK(node->open() == OK);
	node->close();
	TiledMeshDataRequestQueue::get_singleton()->poll();
	CHECK(IonHTTPClient::requests.is_empty());
	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[3DTiles][SceneTree] Runtime ray picking returns traditional batch metadata") {
	const String path = TestUtils::get_temp_path("tiles3d_metadata.b3dm");
	write_file(path, metadata_b3dm());
	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(3.25, 4.25, 7));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(path);
	node->set_position(Vector3(3, 4, 5));
	SceneTree::get_singleton()->get_root()->add_child(node);
	CHECK(node->open() == OK);
	bool ready = false;
	for (int i = 0; i < 2000 && !ready; i++) {
		SceneTree::get_singleton()->process(0.001);
		ready = int(node->get_streaming_stats()["resident_render_instances"]) == 1;
		if (!ready) OS::get_singleton()->delay_usec(1000);
	}
	REQUIRE(ready);
	const Dictionary stats = node->get_streaming_stats();
	CHECK(int(stats["resident_metadata_contents"]) == 1);
	CHECK(int(stats["resident_metadata_batches"]) == 1);
	CHECK(int(stats["resident_metadata_properties"]) == 6);
	const Dictionary hit = node->intersect_ray(Vector3(3.25, 4.25, 7), Vector3(0, 0, -1));
	REQUIRE_FALSE(hit.is_empty());
	CHECK(int(hit["batch_id"]) == 0);
	CHECK(int(hit["primitive_index"]) == 0);
	CHECK(int(hit["triangle_index"]) == 0);
	CHECK(Vector3(hit["position"]).is_equal_approx(Vector3(3.25, 4.25, 5)));
	CHECK(Dictionary(hit["properties"])["name"] == "building");
	CHECK(double(Dictionary(hit["properties"])["temperature"]) == doctest::Approx(18.5));
	const Vector2 screen_point = camera->unproject_position(Vector3(3.25, 4.25, 5));
	CHECK_FALSE(node->intersect_screen_point(camera, screen_point).is_empty());
	CHECK(node->intersect_ray(Vector3(5, 6, 7), Vector3(0, 0, -1)).is_empty());
	node->close();
	CHECK(node->intersect_ray(Vector3(3.25, 4.25, 7), Vector3(0, 0, -1)).is_empty());
	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

TEST_CASE("[3DTiles][SceneTree] REPLACE streams real meshes and restores the parent at distance") {
	const String path = TestUtils::get_temp_path("tiles3d_streaming.json");
	write_file(path, tileset(R"({"boundingVolume":{"sphere":[0,0,0,10]},"geometricError":100,"refine":"REPLACE",
		"content":{"uri":"tiles3d_coarse.glb"},"children":[{"boundingVolume":{"sphere":[0,0,0,10]},
		"geometricError":0,"content":{"uri":"tiles3d_fine.glb"}}]})"));
	write_file(path.get_base_dir().path_join("tiles3d_coarse.glb"), glb());
	write_file(path.get_base_dir().path_join("tiles3d_fine.glb"), glb());
	Camera3D *camera = memnew(Camera3D);
	camera->set_position(Vector3(0, 2, 5));
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->make_current();
	TiledMeshInstance3D *node = memnew(TiledMeshInstance3D);
	node->set_auto_load(false);
	node->set_source_uri(path);
	SceneTree::get_singleton()->get_root()->add_child(node);
	CHECK(node->open() == OK);
	bool ready = false;
	for (int i = 0; i < 2000 && !ready; i++) {
		SceneTree::get_singleton()->process(0.001);
		const Dictionary stats = node->get_streaming_stats();
		ready = int(stats["resident_render_instances"]) == 2 && int(stats["hidden_replacement_surfaces"]) == 1;
		if (!ready) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
	CHECK(ready);
	CHECK(StringName(node->get_streaming_stats()["active_format_id"]) == SNAME("3dtiles"));
	CHECK(int(node->get_streaming_stats()["visible_render_instances"]) == 1);
	camera->set_position(Vector3(0, 2, 1000000));
	for (int i = 0; i < 5; i++) {
		SceneTree::get_singleton()->process(0.001);
	}
	CHECK(int(node->get_streaming_stats()["hidden_replacement_surfaces"]) == 0);
	SceneTree::get_singleton()->get_root()->remove_child(node);
	memdelete(node);
	SceneTree::get_singleton()->get_root()->remove_child(camera);
	memdelete(camera);
}

// Fixed viewport dimensions make the expected pixel error independent of the
// test runner's window. This exercises loading and selection through the node.
class LodFixture {
public:
	SubViewport *viewport = nullptr;
	Camera3D *camera = nullptr;
	TiledMeshInstance3D *tiles = nullptr;

	static Dictionary root_tile(const Dictionary &p_volume, double p_error) {
		Dictionary root;
		root["boundingVolume"] = p_volume;
		root["geometricError"] = p_error;
		root["refine"] = "REPLACE";
		root["content"] = dictionary(R"({"uri":"lod_coarse.glb"})");
		Dictionary child;
		child["boundingVolume"] = p_volume.duplicate(true);
		child["geometricError"] = 0;
		child["content"] = dictionary(R"({"uri":"lod_fine.glb"})");
		root["children"] = Array({ child });
		return root;
	}

	explicit LodFixture(const Dictionary &p_root) {
		const String path = TestUtils::get_temp_path("lod_tileset.json");
		Dictionary manifest;
		manifest["asset"] = dictionary(R"({"version":"1.1"})");
		manifest["geometricError"] = 1000;
		manifest["root"] = p_root;
		write_file(path, JSON::stringify(manifest).to_utf8_buffer());
		write_file(path.get_base_dir().path_join("lod_coarse.glb"), glb());
		write_file(path.get_base_dir().path_join("lod_fine.glb"), glb());
		viewport = memnew(SubViewport);
		viewport->set_size(Size2i(1280, 720));
		viewport->set_use_own_world_3d(true);
		SceneTree::get_singleton()->get_root()->add_child(viewport);
		camera = memnew(Camera3D);
		camera->set_fov(90);
		camera->set_near(0.01);
		camera->set_far(2000);
		camera->set_position(Vector3(0, 0, 100));
		viewport->add_child(camera);
		camera->make_current();
		tiles = memnew(TiledMeshInstance3D);
		tiles->set_auto_load(false);
		tiles->set_lod_hysteresis(0);
		tiles->set_source_uri(path);
		viewport->add_child(tiles);
		CHECK(tiles->open() == OK);
	}

	void settle() {
		int stable = 0;
		for (int i = 0; i < 2000 && stable < 30; i++) {
			SceneTree::get_singleton()->process(0.001);
			const Dictionary stats = tiles->get_streaming_stats();
			const int pending = int(stats["requested_contents"]) + int(stats.get("waiting_contents", 0)) + int(stats["packing_contents"]) + int(stats["packed_ready_contents"]);
			stable = String(stats["root_state"]) == "resident" && int(stats["resident_render_instances"]) > 0 && pending == 0 ? stable + 1 : 0;
			OS::get_singleton()->delay_usec(1000);
		}
		CHECK(stable == 30);
		CHECK(int(tiles->get_streaming_stats()["failed_contents"]) == 0);
	}

	int hidden() const { return int(tiles->get_streaming_stats()["hidden_replacement_surfaces"]); }

	~LodFixture() {
		SceneTree::get_singleton()->get_root()->remove_child(viewport);
		memdelete(viewport);
	}
};

TEST_CASE("[3DTiles][SceneTree][LOD] Sphere surface distance drives refinement outside and inside its volume") {
	LodFixture fixture(LodFixture::root_tile(dictionary(R"({"sphere":[0,0,0,90]})"), 1));
	// 720px, 90-degree vertical FOV: 1m error / 10m distance = 36px.
	// Center depth would incorrectly yield 3.6px and keep the coarse tile.
	fixture.settle();
	CHECK(fixture.hidden() == 1);
	fixture.camera->set_position(Vector3(0, 0, 50));
	fixture.settle();
	CHECK(fixture.hidden() == 1);
	fixture.camera->set_position(Vector3(0, 0, 200));
	fixture.settle();
	CHECK(fixture.hidden() == 0);
}

TEST_CASE("[3DTiles][SceneTree][LOD] Lower bias increases detail consistently with distance LOD") {
	LodFixture fixture(LodFixture::root_tile(dictionary(R"({"sphere":[0,0,0,1]})"), 1));
	fixture.camera->set_position(Vector3(0, 0, 31));
	fixture.settle();
	CHECK(fixture.hidden() == 0); // 12px < 16px.
	fixture.tiles->set_lod_bias(0.5);
	fixture.settle();
	CHECK(fixture.hidden() == 1); // 12px / 0.5 = 24px.
	fixture.tiles->set_lod_bias(2.0);
	fixture.settle();
	CHECK(fixture.hidden() == 0);
}

TEST_CASE("[3DTiles][SceneTree][LOD] Rotating a camera at a fixed position does not change SSE") {
	LodFixture fixture(LodFixture::root_tile(dictionary(R"({"sphere":[0,0,0,2]})"), 4));
	fixture.camera->set_position(Vector3(30, 0, 100));
	fixture.settle();
	CHECK(fixture.hidden() == 0);
	fixture.camera->set_rotation(Vector3(0, Math::deg_to_rad(-20.0), 0));
	fixture.settle();
	CHECK(fixture.hidden() == 0);
}

TEST_CASE("[3DTiles][SceneTree][LOD] Box distances use their faces instead of an enclosing sphere") {
	const Dictionary volume = dictionary(R"({"box":[0,0,0,90,0,0,0,1,0,0,0,90]})");
	LodFixture fixture(LodFixture::root_tile(volume, 1));
	fixture.settle();
	CHECK(fixture.hidden() == 0); // Nearest face is 99m away, despite a 127m sphere radius.
	fixture.camera->set_position(Vector3(100, 0, 0));
	fixture.camera->look_at(Vector3());
	fixture.settle();
	CHECK(fixture.hidden() == 1); // Nearest face is 10m away.
}

TEST_CASE("[3DTiles][SceneTree][LOD] Zero geometric error never refines even inside the volume") {
	LodFixture fixture(LodFixture::root_tile(dictionary(R"({"sphere":[0,0,0,90]})"), 0));
	fixture.camera->set_position(Vector3(0, 0, 50));
	fixture.settle();
	CHECK(fixture.hidden() == 0);
}

TEST_CASE("[3DTiles][SceneTree][LOD] Orthographic error depends on view size rather than distance") {
	LodFixture fixture(LodFixture::root_tile(dictionary(R"({"sphere":[0,0,0,90]})"), 1));
	fixture.camera->set_projection(Camera3D::PROJECTION_ORTHOGONAL);
	fixture.camera->set_size(40); // 720 / 40 = 18 pixels per meter.
	fixture.settle();
	CHECK(fixture.hidden() == 1);
	fixture.camera->set_position(Vector3(0, 0, 1000));
	fixture.settle();
	CHECK(fixture.hidden() == 1);
	fixture.camera->set_size(80);
	fixture.settle();
	CHECK(fixture.hidden() == 0);
}

TEST_CASE("[3DTiles][SceneTree][LOD] Global scale and source units scale error and distance together") {
	LodFixture fixture(LodFixture::root_tile(dictionary(R"({"sphere":[0,0,0,9]})"), 0.1));
	fixture.tiles->set_unit_scale(2);
	fixture.tiles->set_scale(Vector3(3, 3, 3));
	fixture.tiles->reload();
	fixture.camera->set_position(Vector3(0, 0, 60)); // Radius=54m, error=0.6m, distance=6m.
	fixture.settle();
	CHECK(fixture.hidden() == 1);
	fixture.camera->set_position(Vector3(0, 0, 120));
	fixture.settle();
	CHECK(fixture.hidden() == 0);
}

TEST_CASE("[3DTiles][SceneTree][LOD] Stationary cameras refill the request queue until all desired siblings load") {
	Dictionary root = LodFixture::root_tile(dictionary(R"({"sphere":[0,0,0,90]})"), 100);
	Array children = root["children"];
	const Dictionary child = children[0];
	for (int i = 1; i < 12; i++) {
		children.push_back(child.duplicate(true));
	}
	LodFixture fixture(root);
	fixture.tiles->set_max_concurrent_requests(1);
	fixture.settle();
	const Dictionary stats = fixture.tiles->get_streaming_stats();
	CHECK(int(stats["resident_render_instances"]) == 13);
	CHECK(int(stats.get("waiting_contents", 0)) == 0);
	CHECK(int(stats["visible_render_instances"]) == 12);
	CHECK(fixture.hidden() == 1);
}

TEST_CASE("[3DTiles][SceneTree][LOD] Resizing the viewport and changing FOV update the pixel threshold") {
	LodFixture fixture(LodFixture::root_tile(dictionary(R"({"sphere":[0,0,0,1]})"), 1));
	fixture.camera->set_position(Vector3(0, 0, 21));
	fixture.settle();
	CHECK(fixture.hidden() == 1); // 18px.
	fixture.viewport->set_size(Size2i(1280, 360));
	fixture.settle();
	CHECK(fixture.hidden() == 0); // 9px.
	fixture.camera->set_fov(45);
	fixture.settle();
	CHECK(fixture.hidden() == 1); // About 21.7px after zooming in.
}

TEST_CASE("[3DTiles][SceneTree][LOD] Sheared and flat boxes preserve nearest-volume distance") {
	SUBCASE("Nonuniform scale shears an oriented box") {
		const Dictionary volume = dictionary(R"({"box":[0,0,0,60,0,60,0,1,0,-60,0,60]})");
		LodFixture fixture(LodFixture::root_tile(volume, 1));
		fixture.tiles->set_scale(Vector3(2, 1, 1));
		fixture.settle();
		CHECK(fixture.hidden() == 0);
		fixture.camera->set_position(Vector3(130, 0, 10));
		fixture.camera->look_at(Vector3());
		fixture.settle();
		CHECK(fixture.hidden() == 1);
	}
	SUBCASE("A box may have a zero-length half axis") {
		const Dictionary volume = dictionary(R"({"box":[0,0,0,90,0,0,0,0,0,0,0,90]})");
		LodFixture fixture(LodFixture::root_tile(volume, 1));
		fixture.settle();
		CHECK(fixture.hidden() == 0);
		fixture.camera->set_position(Vector3(0, 0, 10));
		fixture.settle();
		CHECK(fixture.hidden() == 1);
	}
}

} // namespace TestTiles3D
