extends Node
const URI = preload("res://scripts/desktop/http_source.gd")
var suite: Node

class FixtureServer extends Node:
	var server := TCPServer.new()
	var clients: Array[Dictionary] = []
	var routes := {}
	var requests: Array[Dictionary] = []
	var base := ""
	func start() -> bool:
		if server.listen(0, "127.0.0.1") != OK: return false
		base = "http://127.0.0.1:%d" % server.get_local_port()
		return true
	func _process(_delta: float) -> void:
		while server.is_connection_available():
			clients.append({"peer": server.take_connection(), "bytes": PackedByteArray()})
		for client in clients.duplicate():
			var peer: StreamPeerTCP = client.peer
			peer.poll()
			if peer.get_status() != StreamPeerTCP.STATUS_CONNECTED:
				clients.erase(client)
				continue
			if client.has("hold"): continue
			var available := peer.get_available_bytes()
			if available > 0:
				var received := peer.get_data(available)
				if received[0] == OK: client.bytes.append_array(received[1])
			var request: String = client.bytes.get_string_from_utf8()
			if not "\r\n\r\n" in request: continue
			var path := request.get_slice("\r\n", 0).get_slice(" ", 1)
			requests.append({"path": path, "raw": request.to_lower()})
			var route: Dictionary = routes.get(path, {"code": 404, "body": PackedByteArray()})
			if route.get("hold", false):
				client["hold"] = true
				continue
			var body: PackedByteArray = route.get("body", PackedByteArray())
			var code: int = route.get("code", 200)
			if route.get("retry", false):
				code = 503
				route.retry = false
			var output := "HTTP/1.1 %d Fixture\r\nContent-Length: %d\r\nConnection: close\r\n" % [code, body.size()]
			if route.has("location"): output += "Location: " + route.location + "\r\n"
			output += "\r\n"
			peer.put_data(output.to_utf8_buffer() + body)
			peer.disconnect_from_host()
			clients.erase(client)
	func count(path: String) -> int:
		var result := 0
		for request in requests:
			if request.path == path: result += 1
		return result
	func _exit_tree() -> void:
		for client in clients: client.peer.disconnect_from_host()
		server.stop()

func run(checks: Node, first: String, second: String) -> void:
	suite = checks
	var base := "https://example.org/models/a/scene.gltf?token=root"
	suite.check(URI.resolve(base, "../buffers/a.bin?x=1") == "https://example.org:443/models/buffers/a.bin?x=1", "remote glTF relative parent and query resolution")
	suite.check(URI.resolve(base, "/tex/a.png") == "https://example.org:443/tex/a.png", "root-relative dependency does not inherit model query")
	suite.check(URI.resolve(base, "?next=yes") == "https://example.org:443/models/a/scene.gltf?next=yes", "query-only URI resolution")
	suite.check(URI.resolve(base, "http://example.org/a").is_empty() and URI.resolve(base, "file:///a.bin").is_empty(), "reject HTTPS downgrade and local-file dependency")
	suite.check(URI.parse("https://user:password@example.org/a").is_empty(), "reject embedded URL credentials")
	suite.check(not URI.headers_error({"Host": "evil"}).is_empty() and not URI.headers_error({"X-Test": "a\r\nb"}).is_empty(), "reject transport override and header injection")
	var server := FixtureServer.new()
	var other := FixtureServer.new()
	add_child(server)
	add_child(other)
	suite.check(server.start() and other.start(), "local model fixture servers start")
	if server.base.is_empty() or other.base.is_empty(): return
	var doc: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://tests/fixtures/triangle.gltf.txt"))
	var buffer := Marshalls.base64_to_raw(doc.buffers[0].uri.get_slice(",", 1))
	doc.buffers[0].uri = "../buffers/triangle.bin?key=fixture"
	var picture := Image.create(2, 2, false, Image.FORMAT_RGBA8)
	picture.fill(Color(0.25, 0.75, 0.4, 1))
	doc["images"] = [{"uri": other.base + "/texture.png"}, {"uri": other.base + "/texture.png"}]
	doc["textures"] = [{"source": 0}]
	doc["materials"] = [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}]
	doc.meshes[0].primitives[0]["material"] = 0
	server.routes["/redirect.gltf"] = {"code": 302, "location": "/assets/models/scene.gltf"}
	server.routes["/assets/models/scene.gltf"] = {"body": JSON.stringify(doc).to_utf8_buffer()}
	server.routes["/assets/buffers/triangle.bin?key=fixture"] = {"body": buffer, "retry": true}
	other.routes["/texture.png"] = {"body": picture.save_png_to_buffer()}
	var headers := {"Authorization": "Bearer fixture-secret", "X-Api-Key": "fixture-key"}
	var id: String = AppState.add_source(first, server.base + "/redirect.gltf", "auto", {"headers": headers})
	await _wait(first, id)
	var layer: Dictionary = AppState.views[first].get_layer(id)
	suite.check(layer.status == "Ready" and is_instance_valid(layer.node), "remote glTF imports geometry with external buffer and texture")
	suite.check(server.count("/assets/buffers/triangle.bin?key=fixture") == 2, "transient dependency failure is retried")
	suite.check(other.count("/texture.png") == 1, "duplicate glTF image URI downloaded only once")
	var credentials := true
	for request in server.requests: credentials = credentials and "authorization: bearer fixture-secret" in request.raw
	suite.check(credentials and server.requests.size() >= 3, "same-origin requests retain session headers through redirect")
	var isolated := true
	for request in other.requests: isolated = isolated and not "fixture-secret" in request.raw and not "fixture-key" in request.raw
	suite.check(isolated and other.requests.size() > 0, "cross-origin dependencies receive no custom credentials")
	var path: String = AppState.store.records[layer.data_id].get("local_source", "")
	var request_count := server.requests.size() + other.requests.size()
	suite.check(AppState.share_layer(first, id, second), "remote model shares existing parsed data")
	await get_tree().process_frame
	suite.check(server.requests.size() + other.requests.size() == request_count, "shared remote model does not download again")
	var shared: Dictionary = AppState.views[second].layers.back()
	AppState.remove_layer(first, id)
	suite.check(FileAccess.file_exists(path), "remote cache survives while a shared reference remains")
	AppState.remove_layer(second, shared.id)
	await get_tree().process_frame
	await get_tree().process_frame
	suite.check(not FileAccess.file_exists(path) and not DirAccess.dir_exists_absolute(path.get_base_dir()), "last remote reference cleans files and generation directory")
	# Redirecting the document to a different origin must also strip credentials.
	server.routes["/cross.gltf"] = {"code": 302, "location": other.base + "/scene.gltf"}
	other.routes["/scene.gltf"] = {"body": FileAccess.get_file_as_bytes("res://tests/fixtures/triangle.gltf.txt")}
	id = AppState.add_source(first, server.base + "/cross.gltf", "auto", {"headers": headers})
	await _wait(first, id)
	suite.check(AppState.views[first].get_layer(id).status == "Ready" and not "fixture-secret" in other.requests.back().raw, "cross-origin document redirect strips credentials")
	AppState.remove_layer(first, id)
	var binary_doc: Dictionary = doc.duplicate(true)
	binary_doc.buffers[0].erase("uri")
	server.routes["/model.glb"] = {"body": _glb(binary_doc, buffer)}
	id = AppState.add_source(first, server.base + "/model.glb")
	await _wait(first, id)
	suite.check(AppState.views[first].get_layer(id).status == "Ready", "remote GLB retains embedded geometry and downloads external images")
	AppState.remove_layer(first, id)
	# Missing dependencies fail atomically and clean already downloaded files.
	var broken: Dictionary = doc.duplicate(true)
	broken.buffers[0].uri = "/missing.bin"
	server.routes["/broken.gltf"] = {"body": JSON.stringify(broken).to_utf8_buffer()}
	id = AppState.add_source(first, server.base + "/broken.gltf")
	await _wait(first, id)
	layer = AppState.views[first].get_layer(id)
	suite.check(layer.status == "Failed" and not is_instance_valid(layer.node), "missing remote dependency fails without a partial scene")
	suite.check(not AppState.store._data_files.has(layer.data_id), "failed remote transaction cleans cached files")
	AppState.remove_layer(first, id)
	# Deleting a layer while its HTTP response is pending closes that transaction.
	server.routes["/slow.gltf"] = {"hold": true}
	id = AppState.add_source(first, server.base + "/slow.gltf")
	for frame in 120:
		await get_tree().process_frame
		if server.count("/slow.gltf") > 0: break
	var data_id: String = AppState.views[first].get_layer(id).data_id
	suite.check(AppState.store._requests.has(data_id), "remote cancellation fixture has an active request")
	AppState.remove_layer(first, id)
	await get_tree().process_frame
	suite.check(not AppState.store._requests.has(data_id) and not AppState.store.records.has(data_id), "delete cancels active HTTP transaction")
	# A delayed old generation cannot overwrite a reload's new document.
	id = AppState.add_source(first, server.base + "/slow.gltf")
	for frame in 120:
		await get_tree().process_frame
		if server.count("/slow.gltf") > 1: break
	layer = AppState.views[first].get_layer(id)
	server.routes["/slow.gltf"] = {"body": FileAccess.get_file_as_bytes("res://tests/fixtures/triangle.gltf.txt")}
	AppState.store.reload_data(layer.data_id)
	await _wait(first, id)
	suite.check(layer.status == "Ready" and AppState.store.records[layer.data_id].generation == 2, "reload cancels delayed HTTP generation and loads the new one")
	AppState.remove_layer(first, id)
	server.queue_free()
	other.queue_free()

func _wait(view_id: String, layer_id: String) -> void:
	var deadline := Time.get_ticks_msec() + 10000
	while Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
		if AppState.views[view_id].get_layer(layer_id).status in ["Ready", "Failed"]: return

func _glb(document: Dictionary, bytes: PackedByteArray) -> PackedByteArray:
	var json := JSON.stringify(document).to_utf8_buffer()
	while json.size() % 4 != 0: json.append(32)
	var binary := bytes.duplicate()
	while binary.size() % 4 != 0: binary.append(0)
	var header := PackedByteArray()
	header.resize(20)
	header.encode_u32(0, 0x46546c67)
	header.encode_u32(4, 2)
	header.encode_u32(8, 28 + json.size() + binary.size())
	header.encode_u32(12, json.size())
	header.encode_u32(16, 0x4e4f534a)
	var chunk := PackedByteArray()
	chunk.resize(8)
	chunk.encode_u32(0, binary.size())
	chunk.encode_u32(4, 0x004e4942)
	return header + json + chunk + binary
