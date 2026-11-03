extends Tree
## Keep the full viewport height for scrolling, but let empty overlay space hit the scene.

func _has_point(point: Vector2) -> bool:
	if not Rect2(Vector2.ZERO, size).has_point(point): return false
	# Tree owns internal scrollbars. Include them even below the final visible row.
	for child in get_children(true):
		if child is ScrollBar and child.visible and child.get_rect().has_point(point): return true
	return get_item_at_position(point) != null
