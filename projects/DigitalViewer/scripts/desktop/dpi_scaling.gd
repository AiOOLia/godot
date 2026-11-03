extends Node
## Window sizes are physical pixels; Control sizes and font sizes remain logical.
## Windows WM_DPICHANGED resizes native windows. Never apply that ratio twice.
const OWNER_META := &"digitalviewer_popup_owner"
var _windows: Dictionary = {}
var _elapsed := 0.0

func _ready() -> void:
	get_tree().node_added.connect(_node_added)
	_track(get_tree().root)
	_scan(get_tree().root)
	var main := get_tree().root
	if DisplayServer.get_name() != "headless" and main.mode == Window.MODE_WINDOWED and not "--resolution" in OS.get_cmdline_args():
		main.size = _fit_size(main, Vector2i(Vector2(main.size) * main.content_scale_factor))
		var area := DisplayServer.screen_get_usable_rect(main.current_screen)
		main.position = area.position + (area.size - main.size) / 2

static func screen_scale(screen: int) -> float:
	if DisplayServer.get_name() == "headless": return 1.0
	var factor := DisplayServer.screen_get_scale(screen)
	if OS.get_name() == "Windows":
		# In per-monitor mode this is effective (user-selected) DPI, not EDID DPI.
		factor = DisplayServer.screen_get_dpi(screen) / 96.0
	return factor if is_finite(factor) and factor > 0.0 else 1.0

static func _owner(window: Window) -> Window:
	if window.has_meta(OWNER_META):
		var reference: WeakRef = window.get_meta(OWNER_META)
		var target: Window = reference.get_ref()
		if is_instance_valid(target): return target
	return window.get_parent().get_window() if window.get_parent() else window

static func prepare(window: Window, owner: Window = null) -> void:
	if owner != null: window.set_meta(OWNER_META, weakref(owner))
	var parent := _owner(window)
	window.content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
	window.content_scale_factor = screen_scale(parent.current_screen)
	if not window.visible: window.current_screen = parent.current_screen

static func popup(window: Window, logical_size := Vector2i.ZERO, owner: Window = null, ratio := 0.0) -> void:
	prepare(window, owner)
	var parent := _owner(window)
	if DisplayServer.get_name() == "headless":
		window.popup_centered(logical_size)
		return
	if owner != null and owner.visible:
		# Scene ownership stays stable when panes dock; native modality follows
		# the originating window instead of always blocking the main window.
		window.transient_to_focused = owner != window.get_parent().get_window()
		if window.transient_to_focused: owner.grab_focus()
	var area := DisplayServer.screen_get_usable_rect(parent.current_screen)
	var desired := Vector2i(Vector2(logical_size) * window.content_scale_factor)
	if ratio > 0: desired = Vector2i(Vector2(area.size) * ratio)
	if desired == Vector2i.ZERO: desired = window.size
	desired = _fit_size(window, desired.max(Vector2i(window.get_contents_minimum_size() * window.content_scale_factor)))
	window.popup(Rect2i(area.position + (area.size - desired) / 2, desired))

static func _fit_size(window: Window, requested: Vector2i) -> Vector2i:
	if DisplayServer.get_name() == "headless": return requested
	var area := DisplayServer.screen_get_usable_rect(window.current_screen)
	# Leave room for the native title bar and resize border.
	return requested.min(area.size - Vector2i(32, 64)).max(Vector2i(1, 1))

func _scan(node: Node) -> void:
	for child in node.get_children(true):
		_node_added(child)
		_scan(child)

func _node_added(node: Node) -> void:
	if node is Window: _track(node)

func _track(window: Window) -> void:
	var id := window.get_instance_id()
	if _windows.has(id): return
	_windows[id] = weakref(window)
	window.content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
	if window == get_tree().root:
		window.content_scale_factor = screen_scale(window.current_screen)
	else:
		prepare(window)
	window.dpi_changed.connect(_sync.bind(window), CONNECT_DEFERRED)
	window.about_to_popup.connect(prepare.bind(window))
	window.visibility_changed.connect(_sync.bind(window), CONNECT_DEFERRED)

func _sync(window: Window) -> void:
	if not is_instance_valid(window) or not window.is_inside_tree(): return
	var screen := window.current_screen if window.visible else _owner(window).current_screen
	var factor := screen_scale(screen)
	if is_equal_approx(window.content_scale_factor, factor): return
	var old_factor := window.content_scale_factor
	window.content_scale_factor = factor
	# Windows already applied the suggested outer rectangle in WM_DPICHANGED.
	if OS.get_name() != "Windows" and window.visible and window.mode == Window.MODE_WINDOWED:
		window.size = _fit_size(window, Vector2i(Vector2(window.size) * factor / old_factor))
	# Refresh separate 3D render targets even if logical Control sizes stay equal.
	get_tree().call_group(&"digitalviewer_panes", &"_layout")

func _process(delta: float) -> void:
	_elapsed += delta
	if _elapsed < 0.25: return
	_elapsed = 0.0
	# Also covers a scale setting change on the same screen and display hotplug.
	for id in _windows.keys():
		var window: Window = _windows[id].get_ref()
		if window == null: _windows.erase(id)
		else: _sync(window)
