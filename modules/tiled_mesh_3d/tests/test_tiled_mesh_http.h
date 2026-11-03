#pragma once

#include "core/io/compression.h"
#include "core/io/tcp_server.h"
#include "core/os/os.h"
#include "modules/tiled_mesh_3d/tiled_mesh_data_request_queue.h"
#include "tests/test_macros.h"

namespace TestTiledMeshHTTP {

TEST_CASE("[TiledMesh3D] HTTP gzip validates decoded limits and stream integrity") {
	PackedByteArray expected = String("{\"asset\":\"gzip-response\"}").repeat(4096).to_utf8_buffer();
	PackedByteArray body;
	body.resize(Compression::get_max_compressed_buffer_size(expected.size(), Compression::MODE_GZIP));
	body.resize(Compression::compress(body.ptrw(), expected.ptr(), expected.size(), Compression::MODE_GZIP));
	uint64_t limit = expected.size();
	Error expected_error = OK;
	String encoding = "gZiP";
	bool chunked = false;
	bool close_delimited = false;
	bool cancel = false;

	SUBCASE("exact decoded limit") {}
	SUBCASE("unlimited request") { limit = 0; }
	SUBCASE("decoded limit exceeded") {
		limit--;
		expected_error = ERR_OUT_OF_MEMORY;
	}
	SUBCASE("compression bomb") {
		limit = 1024;
		expected_error = ERR_OUT_OF_MEMORY;
	}
	SUBCASE("wire limit exceeded") {
		limit = body.size() - 1;
		expected_error = ERR_OUT_OF_MEMORY;
	}
	SUBCASE("truncated gzip trailer") {
		body.resize(body.size() - 1);
		expected_error = ERR_FILE_CORRUPT;
	}
	SUBCASE("invalid checksum") {
		body.set(body.size() - 8, body[body.size() - 8] ^ 1);
		expected_error = ERR_FILE_CORRUPT;
	}
	SUBCASE("invalid header") {
		body.set(0, 0);
		expected_error = ERR_FILE_CORRUPT;
	}
	SUBCASE("empty encoded body") {
		body.clear();
		expected_error = ERR_FILE_CORRUPT;
	}
	SUBCASE("trailing garbage") {
		body.push_back(42);
		expected_error = ERR_FILE_CORRUPT;
	}
	SUBCASE("concatenated members") {
		body.append_array(body);
		expected.append_array(expected);
		limit = expected.size();
	}
	SUBCASE("chunked keep alive") { chunked = true; }
	SUBCASE("close delimited") { close_delimited = true; }
	SUBCASE("unknown encoding") {
		encoding = "br";
		expected_error = ERR_UNAVAILABLE;
	}
	SUBCASE("stacked encoding") {
		encoding = "gzip, gzip";
		expected_error = ERR_UNAVAILABLE;
	}
	SUBCASE("identity remains unchanged") {
		encoding = "identity";
		body = expected;
	}
	SUBCASE("cancel one coalesced subscriber") { cancel = true; }

	Ref<TCPServer> server;
	server.instantiate();
	REQUIRE(server->listen(0, IPAddress("127.0.0.1")) == OK);
	TiledMeshDataRequestQueue *queue = TiledMeshDataRequestQueue::get_singleton();
	REQUIRE(queue != nullptr);
	const String uri = vformat("http://127.0.0.1:%d/tileset.json", server->get_local_port());
	const uint64_t first = queue->request(ObjectID(uint64_t(7001)), 1, uri, limit, 2.0, 0);
	const uint64_t second = queue->request(ObjectID(uint64_t(7002)), 2, uri, limit, 2.0, 0);
	Ref<StreamPeerTCP> connection;
	bool sent = false;
	bool done_first = false;
	bool done_second = false;
	TiledMeshDataRequestQueue::Result result_first;
	TiledMeshDataRequestQueue::Result result_second;
	for (int i = 0; i < 4000 && !(done_second && (done_first || cancel)); i++) {
		queue->poll();
		if (connection.is_null() && server->is_connection_available()) {
			connection = server->take_connection();
		}
		if (connection.is_valid() && !sent && connection->get_available_bytes() > 0) {
			PackedByteArray request;
			request.resize(connection->get_available_bytes());
			REQUIRE(connection->get_data(request.ptrw(), request.size()) == OK);
			String headers = "HTTP/1.1 200 OK\r\nContent-Encoding: " + encoding + "\r\n";
			if (chunked) {
				headers += "Transfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n";
			} else if (close_delimited) {
				headers += "Connection: close\r\n\r\n";
			} else {
				headers += vformat("Content-Length: %d\r\nConnection: keep-alive\r\n\r\n", body.size());
			}
			PackedByteArray response = headers.to_utf8_buffer();
			if (chunked) {
				response.append_array((String::num_uint64(body.size(), 16) + "\r\n").to_utf8_buffer());
			}
			response.append_array(body);
			if (chunked) {
				response.append_array(String("\r\n0\r\n\r\n").to_utf8_buffer());
			}
			REQUIRE(connection->put_data(response.ptr(), response.size()) == OK);
			if (close_delimited) {
				connection->disconnect_from_host();
			}
			if (cancel) {
				queue->cancel(first);
			}
			sent = true;
		}
		done_first = done_first || queue->take_result(first, result_first);
		done_second = done_second || queue->take_result(second, result_second);
		OS::get_singleton()->delay_usec(1000);
	}
	queue->cancel(first);
	queue->cancel(second);
	REQUIRE(done_second);
	CHECK(result_second.error == expected_error);
	CHECK(result_second.response_code == 200);
	if (expected_error == OK) {
		CHECK(result_second.bytes == expected);
	} else {
		CHECK(result_second.bytes.is_empty());
	}
	if (cancel) {
		CHECK_FALSE(done_first);
	} else {
		REQUIRE(done_first);
		CHECK(result_first.error == result_second.error);
		CHECK(result_first.bytes == result_second.bytes);
	}
	server->stop();
}

} // namespace TestTiledMeshHTTP
