extends RefCounted
## Inspect central-directory sizes and attributes BEFORE ZIPReader allocates inflated data.
const MAX_ENTRY := 64 * 1024 * 1024
const MAX_TOTAL := 256 * 1024 * 1024

static func inspect(path: String) -> Dictionary:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null: return {"error": "Cannot open ZIP."}
	var length := file.get_length()
	if length < 22 or length > MAX_ENTRY: return {"error": "ZIP must be at most 64 MiB."}
	file.seek(maxi(0, length - 65557))
	var tail := file.get_buffer(mini(length, 65557))
	var end := -1
	for index in range(tail.size() - 22, -1, -1):
		if tail.decode_u32(index) == 0x06054b50 and index + 22 + tail.decode_u16(index + 20) == tail.size():
			end = index
			break
	if end < 0: return {"error": "ZIP central directory is missing."}
	var count := tail.decode_u16(end + 10)
	var central_size := tail.decode_u32(end + 12)
	var offset := tail.decode_u32(end + 16)
	if count == 65535 or offset == 0xffffffff or central_size == 0xffffffff: return {"error": "ZIP64 is not supported."}
	if tail.decode_u16(end + 4) != 0 or tail.decode_u16(end + 6) != 0 or tail.decode_u16(end + 8) != count: return {"error": "Multi-volume ZIP is not supported."}
	if count > 4096 or offset + central_size > length - 22: return {"error": "ZIP directory limits exceeded."}
	file.seek(offset)
	var directory := file.get_buffer(central_size)
	var cursor := 0
	var total := 0
	var names := {}
	var entries: Array[Dictionary] = []
	for index in count:
		if cursor + 46 > directory.size() or directory.decode_u32(cursor) != 0x02014b50: return {"error": "Corrupt ZIP directory."}
		var flags := directory.decode_u16(cursor + 8)
		var method := directory.decode_u16(cursor + 10)
		var compressed := directory.decode_u32(cursor + 20)
		var expanded := directory.decode_u32(cursor + 24)
		var name_size := directory.decode_u16(cursor + 28)
		var extra_size := directory.decode_u16(cursor + 30)
		var comment_size := directory.decode_u16(cursor + 32)
		var attributes := directory.decode_u32(cursor + 38)
		var local_offset := directory.decode_u32(cursor + 42)
		var next := cursor + 46 + name_size + extra_size + comment_size
		if next > directory.size() or local_offset + 30 + compressed > offset: return {"error": "ZIP entry is out of bounds."}
		var name := directory.slice(cursor + 46, cursor + 46 + name_size).get_string_from_utf8()
		if not safe_path(name) or names.has(name.to_lower()): return {"error": "Unsafe or duplicate ZIP path rejected."}
		names[name.to_lower()] = true
		var file_type := (attributes >> 16) & 0xf000
		if file_type not in [0, 0x8000, 0x4000]: return {"error": "ZIP links and special files are not supported."}
		if flags & 1 or method not in [0, 8]: return {"error": "Encrypted or unsupported ZIP compression."}
		total += expanded
		if expanded > MAX_ENTRY or total > MAX_TOTAL or (expanded > 1048576 and expanded > maxi(1, compressed) * 200): return {"error": "ZIP expansion limit exceeded."}
		# A malicious local header must not redirect a validated central entry.
		file.seek(local_offset)
		var header := file.get_buffer(30)
		if header.size() != 30 or header.decode_u32(0) != 0x04034b50: return {"error": "Invalid ZIP local header."}
		var local_name_size := header.decode_u16(26)
		var local_extra_size := header.decode_u16(28)
		if local_offset + 30 + local_name_size + local_extra_size + compressed > offset: return {"error": "ZIP payload is out of bounds."}
		if file.get_buffer(local_name_size).get_string_from_utf8() != name: return {"error": "ZIP entry names disagree."}
		entries.append({"name": name, "size": expanded})
		cursor = next
	if cursor != directory.size(): return {"error": "Unexpected ZIP directory data."}
	return {"entries": entries, "total": total}

static func safe_path(path: String) -> bool:
	if path.is_empty() or path.is_absolute_path() or "\\" in path or ":" in path or path.begins_with("/"): return false
	for segment in path.trim_suffix("/").split("/"):
		if segment in ["", ".", ".."] or segment.ends_with(".") or segment.ends_with(" "): return false
		var stem := segment.get_slice(".", 0).to_upper()
		if stem in ["CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"]: return false
		for character in segment:
			if character.unicode_at(0) < 32 or character in ["<", ">", "\"", "|", "?", "*"]: return false
	return true
