extends RefCounted
const ZH := {
	"Follow camera": "跟随相机",
	"Add directional light": "添加平行光", "Add spot light": "添加聚光灯", "Range": "范围", "Attenuation": "距离衰减", "Spot angle": "锥角", "Spot attenuation": "边缘衰减",
	"Properties": "属性", "Scripts": "脚本", "Settings": "设置", "Layers": "图层树", "Add data": "添加数据", "Fly to": "飞往", "Viewpoints": "视点", "Camera": "相机", "Viewport properties": "视口属性", "Viewport actions": "视口操作", "View ": "视口 ", "Empty viewport": "空视口", "Add a WGS84 ellipsoid as the base globe, or add imagery, terrain, and 3D data.": "添加 WGS84 椭球作为基础地球，或添加影像、地形和三维数据。", "Split horizontally": "左右分割", "Split vertically": "上下分割", "Fullscreen viewport": "全屏视口", "Frame all": "显示全部", "Frame layer": "定位图层", "Grid": "经纬网", "Detach / dock": "分离 / 停靠", "Close viewport": "关闭视口", "Close": "关闭", "Dock": "停靠", "Basemap": "底图", "3D data": "三维数据", "Shared": "共享", "Ready": "就绪", "Queued": "排队", "Parsing": "解析中", "Downloading": "下载中", "Failed": "失败", "Partial failure": "部分失败", "Paused": "已暂停", "Review required": "待审查", "WGS84 ellipsoid": "WGS84 椭球", "Local file": "本地文件", "Remote URL": "远程地址", "Browse": "浏览", "Source": "数据源", "ZIP entry (if multiple models)": "ZIP 入口（多个模型时填写）", "Cancel": "取消", "Add": "添加", "Target viewport": "目标视口", "Name": "名称", "Visible": "显示", "Receive lighting": "接收光照", "Source default": "资源默认", "Opacity": "透明度", "Move up": "上移", "Move down": "下移", "Reload": "重新加载", "Remove": "移除", "Pause / resume": "暂停 / 继续", "Projection": "投影", "Perspective": "透视", "Orthographic": "正交", "Background": "背景", "Lighting": "光照", "Diagnostics": "诊断", "Copy report": "复制报告", "Export report": "导出报告", "Clear": "清空", "About": "关于", "Quit": "退出", "Language": "语言", "Automation / MCP": "自动化 / MCP", "Enable local MCP": "启用本机 MCP", "Copy token": "复制会话令牌", "scene.read": "scene.read", "script.run": "script.run", "capture": "capture", "Longitude": "经度", "Latitude": "纬度", "Height (m)": "高度（米）", "Save viewpoint": "保存视点", "Delete viewpoint": "删除视点", "Go": "飞往", "No built-in AI. External clients may submit scripts for manual review.": "不内置 AI。外部客户端可提交脚本草案，由用户审查后运行。", "Close this viewport and release its layers?": "关闭此视口并释放其中的图层？", "Reload shared data in all referencing viewports?": "重新加载共享数据会影响所有引用视口，是否继续？", "Session only · no history saved": "仅当前会话 · 不保存历史", "Format": "格式", "Status": "状态", "References": "引用数", "Error": "错误", "Transform": "变换", "Position": "位置", "Rotation": "旋转", "Scale": "缩放", "Run script": "运行脚本", "Open in workbench": "在工作台中打开", "Stop script": "停止脚本", "Reset scene": "重置场景", "Read-only scene inspection": "只读场景检查", "Exit fullscreen": "退出全屏", "Sharing streamed datasets requires engine support.": "瓦片数据共享尚需引擎支持。", "Desktop preview; see release status for unimplemented geospatial features.": "桌面开发版；未完成的地理功能见发布状态。"
}

static func install() -> void:
	var extra := {
		"Preparing geometry": "准备几何中",
		"Exported geometry uses the original face. OCC JSON is reconstructed on demand and may contain multiple repaired faces.": "离散几何对应原始面。OCC JSON 按需重建，可能包含修复后拆分的多个面。",
		"Up axis": "Up 轴",
		"Millimeters per source unit": "每源坐标单位的毫米数",
		"Face index": "面索引",
		"Face persistent ID": "面持久 ID",
		"BREP definition": "BREP 定义",
		"Original BREP JSON": "原始 BREP JSON",
		"OCC reconstructed BREP JSON": "OCC 重构后 BREP JSON",
		"Copy JSON": "复制 JSON",
		"JSON export failed": "JSON 导出失败",
		"Exporting face BREP JSON...": "正在导出面的 BREP JSON…",
		"Source face indices": "源面索引列表",
		"Source face mapping unavailable.": "源面关联不可用。",
		"Source face mapping is ambiguous; original JSON contains all candidate faces.": "源面关联存在歧义，原始 JSON 包含全部候选面。",
		"Original JSON contains the full source face; OCC JSON contains the selected reconstructed face.": "原始 JSON 显示完整源面；OCC JSON 显示当前选中的重构结果面。",
		"Source face index": "源面索引",
		"Surface": "曲面",
		"Plane": "平面", "Cylinder": "圆柱面", "Cone": "圆锥面", "Sphere": "球面", "Torus": "圆环面", "NURBS": "NURBS 曲面", "Extrusion": "拉伸面", "Revolution": "旋转面", "Other surface": "其他曲面", "Unavailable": "不可用",
		"Source geometry coordinates; before assembly and display transforms.": "源几何坐标（装配及显示变换前）",
		"Source definition is preserved when repair splits a face.": "修复拆分后仍显示完整源面定义，选中面可能只是其中一部分。",
		"Area (source units squared)": "面积（源单位²）",
		"Perimeter (source units)": "周长（源单位）",
		"Boundary wires": "边界环", "Edge uses": "有向边",
		"Copy BREP definition": "复制 BREP 定义",
		"BREP definition copied": "已复制 BREP 定义",
		"BREP definition unavailable; reload the model.": "BREP 定义不可用，请重新加载模型。",
		"Source face cannot be uniquely identified; showing the repaired OCCT BREP.": "无法唯一关联源面，显示修复后的 OCCT BREP 定义。",
		"Preview truncated; copy to obtain the full definition.": "预览已截断，复制可获取完整定义。",
		"Units: unknown (source coordinates)": "单位：未知（保留源坐标）", "Occurrence": "实例编号", "Faces": "面", "Triangles": "三角形", "Reconstructing": "拓扑重建中", "Tessellating": "三角化中", "Uploading": "上传中",
		"Add a glTF/GLB model, ZIP archive, or GDScript script.": "添加 glTF/GLB 模型、ZIP 压缩包或 GDScript 脚本。",
		"Type": "类型", "Background type": "背景类型", "Color": "颜色", "Atmosphere": "大气", "Skybox": "天空盒", "Image": "图片", "Rendering": "渲染", "Anti-aliasing": "抗锯齿", "Resolution scale": "分辨率缩放", "Exposure": "曝光", "Gamma": "伽马", "Lights": "光源", "No lights": "暂无光源", "Add light": "添加平行光", "Add point light": "添加点光源", "Directional": "平行光", "Point": "点光源", "Spot": "聚光灯", "Energy": "强度", "Shadow": "阴影", "Azimuth": "方位角", "Elevation": "高度角", "Shadow distance": "阴影距离", "Sky top": "天顶颜色", "Sky horizon": "地平线颜色", "Sun": "太阳", "Environment": "环境", "Sky": "天空", "Sky light": "天空光", "Sky color": "天空颜色", "Panorama": "全景", "Ground": "地面颜色", "No additional lights": "没有附加光源", "Choose panorama...": "选择全景图…", "Panorama requires a 2:1 equirectangular image.": "全景图需要 2:1 等距柱状图。", "Sky energy": "天空能量", "SSAO": "环境光遮蔽", "Glow": "辉光", "Tonemap": "色调映射", "Global illumination": "全局光", "Sun uses the first directional light.": "太阳方向取视口内的第一个平行光。", "Time": "时间", "Fog": "雾", "Fog density": "雾密度", "Aerial perspective": "大气透视", "Choose image...": "选择图片…", "Layer": "图层", "Actions": "操作", "Picked point": "拾取点", "Copy details": "复制详情", "Node path": "节点路径", "Yes": "是", "No": "否",
		"Copy token": "复制令牌", "Use existing token": "使用已有令牌", "Reset token": "重置令牌",
		"MCP token is saved locally and reused after restart.": "MCP 令牌保存在本机，重启后保持不变。",
		"MCP token could not be loaded or saved.": "无法读取或保存 MCP 令牌。",
		"Paste your existing Bearer token (without the Bearer prefix).": "粘贴已有 Bearer 令牌（不包含 Bearer 前缀）。",
		"Generate a new token? Connected clients must update their credentials.": "生成新令牌？已连接的客户端需要更新凭据。",
		"MCP token saved. Reconnect clients to start a new session.": "MCP 令牌已保存，请重新连接客户端以建立新会话。",
		"MCP token update failed. Use 32–512 valid Bearer characters and a writable user directory.": "MCP 令牌更新失败。请使用 32–512 个合法 Bearer 字符，并确认用户目录可写。",
		"Display": "显示", "Screenshot": "截图", "Record": "录制", "Recording": "录制中", "Recording stopped": "录制结束", "Screenshot saved": "截图已保存", "Screenshot failed": "截图保存失败", "No rendered frame": "没有可用的渲染帧", "Rendering is unavailable in headless mode": "无头模式不可用渲染", "Done": "完成", "Shadows": "阴影", "Lighting": "光照", "Click the viewport title to show viewport properties.": "点击视口标题栏查看视口属性。", "Language": "语言", "Theme": "主题", "Dark": "深色", "Light theme is not available in this build.": "当前版本尚未提供浅色主题。", "Enable local bridge": "启用本机桥接", "Automation transport": "自动化传输", "Create independent data; drag the layer to another viewport to share it.": "创建独立数据；需要共享请添加后拖到其他视口。", "Add to": "添加到", "Independent data": "独立数据", "Choose a file, folder, ZIP, or URL": "选择文件、目录、ZIP 或 URL", "Added data appears only in the target viewport. Drag the layer to another viewport to create a shared reference.": "添加后只出现在目标视口；拖到其他视口才会建立共享引用。", "Remote URL": "远程地址", "GDScript script": "GDScript 脚本", "Select folders": "选择目录", "Up": "上一级", "No matching .osg or .osgb data found in the selected directory.": "所选目录中没有与目录同名的 .osg 或 .osgb 数据。", "No matching .osg or .osgb data found in the selected directories.": "所选目录中没有与目录同名的 .osg 或 .osgb 数据。", "Path": "路径", "Directory cannot be opened.": "无法打开目录。", "Ctrl / Shift select multiple folders": "Ctrl / Shift 多选目录", "Select at least one folder.": "请至少选择一个目录。", "Selected sources": "已选来源", "Select all": "全选", "Invert selection": "反选", "Textured": "材质", "Unlit": "无光照", "Wireframe": "线框", "Navigation": "导航模式", "Orbit": "轨道", "Fly (WASD / QE)": "飞行（WASD / QE）", "Focus picked point": "飞往拾取点", "Folder": "目录", "Asset ID": "资产 ID", "Access token (session only)": "访问令牌（仅当前会话）", "Asset ID and token are required.": "请输入资产 ID 和访问令牌。", "Choose a data source.": "请选择数据源。", "No supported model entry found.": "没有找到支持的模型入口。", "Choose model entry": "选择模型入口", "Animation": "动画", "Stopped": "停止", "Script workbench · GDScript": "脚本工作台 · GDScript", "▶ Run": "▶ 运行", "■ Stop": "■ 停止", "Reset scene": "重置场景", "Save": "保存", "Code": "代码", "Output": "输出", "Scene": "场景", "Run snapshot": "运行快照", "Help": "说明", "Confirm": "确认", "Confirm action": "确认操作", "I have reviewed and trust this script": "我已审查并信任这份脚本", "Open": "打开", "Example: satellite": "示例：卫星", "Example: material": "示例：材质", "Next": "下一个", "Find code · Ctrl+F": "查找代码 · Ctrl+F", "Refresh scene tree / properties": "刷新实际场景树 / 属性", "Unsaved": "未保存", "Native session": "原生接管", "Idle": "未接管", "New script": "新建脚本", "Waiting": "待运行", "Compilation failed": "编译失败", "Incompatible entry type": "入口类型不兼容", "Missing run(viewer)": "缺少 run(viewer) 入口", "Running · _process / signals remain active": "运行中 · _process / 信号继续有效", "Stopped · session nodes removed; direct edits retained": "已停止 · 会话节点已清理，直接修改保留", "Layer scenes rebuilt and navigation restored": "已重建图层场景并恢复导航快照"
	}
	var chinese := Translation.new()
	chinese.locale = "zh_CN"
	var messages := ZH.duplicate()
	messages.merge(extra, true)
	messages.merge(preload("res://scripts/desktop/workbench_strings.gd").ZH, true)
	messages[preload("res://scripts/desktop/workbench_strings.gd").HELP_EN] = preload("res://scripts/desktop/workbench_strings.gd").HELP_ZH
	for key in messages:
		chinese.add_message(key, messages[key])
	TranslationServer.add_translation(chinese)
	TranslationServer.set_locale("zh_CN" if OS.get_locale_language() == "zh" else "en")
