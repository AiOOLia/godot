extends RefCounted
## URI resolution never becomes a filesystem path. Credentials belong to one origin.

static func parse(url: String) -> Dictionary:
	if url.length() > 16384 or "\\" in url: return {}
	for character in url:
		if character.unicode_at(0) <= 32 or character.unicode_at(0) == 127: return {}
	var expression := RegEx.new()
	expression.compile("^(https?)://(\\[[0-9A-Fa-f:]+\\]|[A-Za-z0-9.-]+)(?::([0-9]+))?([^?#]*)(\\?[^#]*)?(?:#.*)?$")
	var found := expression.search(url)
	if found == null: return {}
	var scheme := found.get_string(1)
	var host := found.get_string(2).to_lower()
	var port := int(found.get_string(3)) if not found.get_string(3).is_empty() else (443 if scheme == "https" else 80)
	if port < 1 or port > 65535: return {}
	var path := found.get_string(4)
	if path.is_empty(): path = "/"
	if not path.begins_with("/"): return {}
	var origin := "%s://%s:%d" % [scheme, host, port]
	return {"scheme": scheme, "origin": origin, "path": path, "query": found.get_string(5), "url": origin + path + found.get_string(5)}

static func resolve(base_url: String, reference: String) -> String:
	var base := parse(base_url)
	if base.is_empty(): return ""
	var uri := reference.get_slice("#", 0).replace(" ", "%20")
	if uri.begins_with("//"): uri = base.scheme + ":" + uri
	var target := {}
	if uri.begins_with("http://") or uri.begins_with("https://"):
		target = parse(uri)
	elif ":" in uri.get_slice("/", 0).get_slice("?", 0) or "\\" in uri:
		return ""
	else:
		var path := uri.get_slice("?", 0)
		var query := uri.substr(uri.find("?")) if "?" in uri else ""
		if path.is_empty():
			path = base.path
			if uri.is_empty(): query = base.query
		elif not path.begins_with("/"):
			path = base.path.substr(0, base.path.rfind("/") + 1) + path
		target = parse(base.origin + path + query)
	if target.is_empty() or (base.scheme == "https" and target.scheme != "https"): return ""
	# RFC 3986 dot segments, without decoding percent-encoded path components.
	var segments: Array[String] = []
	for segment in target.path.split("/", true):
		if segment == ".": continue
		if segment == "..":
			if segments.size() > 1: segments.pop_back()
		else: segments.append(segment)
	var path := "/".join(segments)
	if target.path.ends_with("/.") or target.path.ends_with("/.."):
		if not path.ends_with("/"): path += "/"
	if path.is_empty(): path = "/"
	return target.origin + path + target.query

static func headers_error(headers: Dictionary) -> String:
	if headers.size() > 32: return "At most 32 custom HTTP headers are allowed."
	var names := {}
	var total := 0
	var token := RegEx.new()
	token.compile("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$")
	for key in headers:
		if not key is String or not headers[key] is String: return "HTTP header names and values must be text."
		var lower: String = key.to_lower()
		if token.search(key) == null or names.has(lower): return "Invalid or duplicate HTTP header name."
		if lower in ["host", "content-length", "transfer-encoding", "connection", "upgrade", "expect", "accept-encoding", "range", "proxy-authorization", "proxy-connection"]:
			return "Transport HTTP headers cannot be overridden."
		for character in headers[key]:
			if character.unicode_at(0) < 32 or character.unicode_at(0) == 127: return "HTTP header values cannot contain control characters."
		names[lower] = true
		total += key.length() + headers[key].length()
	if total > 8192: return "Custom HTTP headers exceed 8 KiB."
	return ""
