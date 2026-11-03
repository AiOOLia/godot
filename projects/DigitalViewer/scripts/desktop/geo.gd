extends RefCounted
## Double precision calculations stay in scalar floats/PackedFloat64Array, never Vector3.
const A := 6378137.0
const B := 6356752.314245179
const E2 := 1.0 - (B * B) / (A * A)

static func ecef(longitude: float, latitude: float, height: float = 0.0) -> PackedFloat64Array:
	if not is_finite(longitude) or not is_finite(latitude) or not is_finite(height) or absf(latitude) > 90.0:
		return PackedFloat64Array()
	var lon := deg_to_rad(wrapf(longitude, -180.0, 180.0))
	var lat := deg_to_rad(latitude)
	var n := A / sqrt(1.0 - E2 * sin(lat) * sin(lat))
	return PackedFloat64Array([(n + height) * cos(lat) * cos(lon), (n + height) * cos(lat) * sin(lon), (n * (1.0 - E2) + height) * sin(lat)])

static func geodetic(p: PackedFloat64Array) -> PackedFloat64Array:
	if p.size() != 3 or not is_finite(p[0]) or not is_finite(p[1]) or not is_finite(p[2]):
		return PackedFloat64Array()
	var r := sqrt(p[0] * p[0] + p[1] * p[1])
	if r < 0.000001:
		if absf(p[2]) < 0.000001:
			return PackedFloat64Array()
		return PackedFloat64Array([0.0, 90.0 * signf(p[2]), absf(p[2]) - B])
	var lat := atan2(p[2], r * (1.0 - E2))
	for iteration in 12:
		var n := A / sqrt(1.0 - E2 * sin(lat) * sin(lat))
		var next := atan2(p[2] + E2 * n * sin(lat), r)
		if absf(next - lat) < 1e-14:
			lat = next
			break
		lat = next
	var n := A / sqrt(1.0 - E2 * sin(lat) * sin(lat))
	var height := r * cos(lat) + p[2] * sin(lat) - n * (1.0 - E2 * sin(lat) * sin(lat))
	return PackedFloat64Array([rad_to_deg(atan2(p[1], p[0])), rad_to_deg(lat), height])

static func local(p: PackedFloat64Array, origin: PackedFloat64Array, lon: float, lat: float) -> Vector3:
	var x := p[0] - origin[0]
	var y := p[1] - origin[1]
	var z := p[2] - origin[2]
	var l := deg_to_rad(lon)
	var f := deg_to_rad(lat)
	return Vector3(-sin(l) * x + cos(l) * y, cos(f) * cos(l) * x + cos(f) * sin(l) * y + sin(f) * z, sin(f) * cos(l) * x + sin(f) * sin(l) * y - cos(f) * z)

static func from_local(p: Vector3, origin: PackedFloat64Array, lon: float, lat: float) -> PackedFloat64Array:
	var l := deg_to_rad(lon)
	var f := deg_to_rad(lat)
	return PackedFloat64Array([origin[0] - sin(l) * p.x + cos(f) * cos(l) * p.y + sin(f) * cos(l) * p.z, origin[1] + cos(l) * p.x + cos(f) * sin(l) * p.y + sin(f) * sin(l) * p.z, origin[2] + sin(f) * p.y - cos(f) * p.z])
