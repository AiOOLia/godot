extends Node
## Optional local dataset probe; reads original data without modifying it.
var failures: Array[Dictionary] = []

func run(shell: Control, source: String) -> void:
	var view_id: String = AppState.active_id
	var id: String = AppState.add_source(view_id, source)
	var layer: Dictionary = AppState.views[view_id].get_layer(id)
	var started := Time.get_ticks_msec()
	var seconds := maxi(5, int(OS.get_environment("DV_DATASET_SECONDS")))
	var connected := false
	var stats := {}
	var next_report := started
	var navigation := OS.get_environment("DV_DATASET_NAVIGATE") == "1"
	var phase := 0
	var samples: Array[Dictionary] = []
	while Time.get_ticks_msec() - started < seconds * 1000:
		await get_tree().process_frame
		if is_instance_valid(layer.node):
			if not connected:
				layer.node.load_failed.connect(_failed.bind("root"))
				layer.node.tile_failed.connect(_failed.bind("tile"))
				connected = true
			stats = layer.node.get_streaming_stats()
		var elapsed := Time.get_ticks_msec() - started
		if navigation and phase < 2 and elapsed > (phase + 1) * seconds * 1000 / 3 and stats.get("visible_render_instances", 0) > 0:
			samples.append({"phase": phase, "elapsed_ms": elapsed, "stats": stats.duplicate(true)})
			AppState.views[view_id].orbit(Vector2(1.2, 0.1))
			AppState.views[view_id].zoom(0.5 if phase == 0 else 4.0)
			phase += 1
		if Time.get_ticks_msec() >= next_report:
			print("DATASET_PROGRESS ", JSON.stringify({"elapsed_ms": Time.get_ticks_msec() - started, "status": layer.status, "error": layer.error, "stats": stats}))
			next_report += 5000
		if layer.status == "Failed": break
	var report := {"source": source, "status": layer.status, "error": layer.error, "failures": failures, "stats": stats, "samples": samples, "navigation_phases": phase, "elapsed_ms": Time.get_ticks_msec() - started}
	var directory := OS.get_environment("DV_TEST_DIRECTORY").get_base_dir()
	var file := FileAccess.open(directory.path_join("dataset-report.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify(report, "\t"))
	file.close()
	if "--dv-capture" in OS.get_cmdline_user_args() and DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		shell.get_viewport().get_texture().get_image().save_png(directory.path_join("dataset.png"))
	print("DATASET_RESULT ", JSON.stringify(report))
	var expect_empty := OS.get_environment("DV_DATASET_EXPECT_EMPTY") == "1"
	var ok: bool = failures.is_empty() and layer.error.is_empty() and stats.get("failed_contents", 0) == 0 and stats.get("resident_contents", 0) > 0
	ok = ok and (stats.get("resident_render_instances", -1) == 0 if expect_empty else stats.get("visible_render_instances", 0) > 0)
	if navigation: ok = ok and phase == 2
	AppState.shutdown()
	print("DIGITALVIEWER_DESKTOP_OK checks=1" if ok else "DIGITALVIEWER_DESKTOP_FAILED dataset")
	get_tree().quit(0 if ok else 1)

func _failed(uri: String, message: String, stage: String) -> void:
	if failures.size() < 20:
		failures.append({"uri": AppState.redact(uri), "message": AppState.redact(message), "stage": stage})
		print("DATASET_FAILURE ", stage, " ", AppState.redact(uri), " ", AppState.redact(message))
