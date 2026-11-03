extends RefCounted
## Godot JSON parses every number as float. Recover the top-level ID directly
## from the validated JSON text, without rounding large integers or touching data.
static func read(text: String) -> Dictionary:
	var depth := 0
	var cursor := 0
	var result := {"valid": true, "present": false, "value": null}
	while cursor < text.length():
		var character := text[cursor]
		if character == '"':
			var start := cursor
			cursor += 1
			while cursor < text.length():
				if text[cursor] == "\\": cursor += 2
				elif text[cursor] == '"':
					cursor += 1
					break
				else: cursor += 1
			var end := cursor
			while cursor < text.length() and text[cursor] in [" ", "\t", "\r", "\n"]: cursor += 1
			if depth == 1 and cursor < text.length() and text[cursor] == ":" and JSON.parse_string(text.substr(start, end - start)) == "id":
				if result.present: return {"valid": false}
				result.present = true
				cursor += 1
				while cursor < text.length() and text[cursor] in [" ", "\t", "\r", "\n"]: cursor += 1
				start = cursor
				if text[cursor] == '"':
					cursor += 1
					while cursor < text.length():
						if text[cursor] == "\\": cursor += 2
						elif text[cursor] == '"':
							cursor += 1
							break
						else: cursor += 1
					result.value = JSON.parse_string(text.substr(start, cursor - start))
				else:
					while cursor < text.length() and not text[cursor] in [",", "}", " ", "\t", "\r", "\n"]: cursor += 1
					var number := text.substr(start, cursor - start)
					var digits := number.trim_prefix("-")
					var limit := "9223372036854775808" if number.begins_with("-") else "9223372036854775807"
					if digits.is_empty() or digits.length() > limit.length() or (digits.length() == limit.length() and digits > limit): return {"valid": false}
					for digit in digits:
						if digit < "0" or digit > "9": return {"valid": false}
					if digits.length() > 1 and digits.begins_with("0"): return {"valid": false}
					result.value = number.to_int()
			continue
		elif character in ["{", "["]: depth += 1
		elif character in ["}", "]"]: depth -= 1
		cursor += 1
	return result
