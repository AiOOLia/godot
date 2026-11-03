extends RefCounted
const HELP_EN := """DigitalViewer native GDScript workbench

Entry point:
extends Node
func run(viewer) -> void:
    var view = viewer.viewport("view_1")
    var content = viewer.content("view_1")
    print(view.camera.name)

viewer.viewport_ids() lists current viewport IDs. viewer.viewport(id) returns that viewport's actual scene objects. viewer.content(id) creates a Node3D container owned by this script session.
Choose viewport IDs explicitly. Changing the active viewport does not change script targets.

Add data > GDScript script imports a .gd file; leave Source empty to create one. The script selector preserves each document's draft, output and run snapshot. Multiple scripts can run together. Layer trees show script instances and generated content. Stop and Remove affect the selected script. Closing the panel or its floating window keeps scripts running.

The first run of each document requires trust confirmation. Within this application session, unchanged confirmed code runs again without another prompt, including after Stop or reopening the panel. Changed code requires a new confirmation. New or reimported documents require their own confirmation.

Native classes, materials, shaders, signals and _process are available. Scripts run with application permissions, including files, network and processes. Review all code before running it. Compilation can execute static initializers; synchronous loops or long calculations block the interface and cannot be interrupted by Stop.

Stop removes the selected script instance and its owned containers. Direct edits to other objects, shared resources, nodes reparented elsewhere and external effects may remain. Reset stops all scripts, rebuilds viewport worlds and restores navigation snapshots; it does not undo arbitrary code or external effects.

Output captures print, errors and available call stacks. Click an error to open its line or the run snapshot if the code has changed. Scene inspection is read-only. Completion offers common words; native breakpoints, stepping and paused local variables are not included.

Files are saved in user://scripts. Opening a script never runs it automatically. External clients submit drafts to this same workbench for manual review and confirmation.

Ctrl+Enter: Run   Ctrl+S: Save   Ctrl+F: Find"""
const HELP_ZH := """DigitalViewer 原生 GDScript 工作台

入口：
extends Node
func run(viewer) -> void:
    var view = viewer.viewport("view_1")
    var content = viewer.content("view_1")
    print(view.camera.name)

viewer.viewport_ids() 列出当前视口 ID；viewer.viewport(id) 返回指定视口的真实场景对象；viewer.content(id) 创建由本次脚本会话管理的 Node3D 容器。
脚本须显式选择视口 ID。切换活动视口不会改变脚本目标。

“添加数据 → GDScript 脚本”可导入 .gd 文件，数据源留空则新建脚本。顶部下拉框切换文档，分别保留草稿、输出和运行快照。多个脚本可同时运行；图层树显示实例和生成节点。停止、移除只影响所选脚本。关闭面板或其浮动窗口后，脚本继续运行。

每份文档首次运行需要信任确认。同一次应用会话内，已确认且代码未改的脚本再次运行不重复弹窗，停止后重跑、重开面板同样有效。代码修改后需要重新确认。新建或重新导入的文档需要各自确认。

可使用原生类、材质、Shader、信号和 _process。脚本具有应用权限，可以访问文件、网络和进程，运行前须审查代码。编译也可能执行静态初始化；同步死循环或长计算会阻塞界面，停止按钮不能强行中断。

停止会移除所选脚本实例和其管理的容器。对其他对象、共享资源的修改、移到别处的节点及外部副作用可能保留。重置停止所有脚本，重建各视口世界并恢复导航快照，不能撤销任意代码或外部副作用。

输出捕获 print、错误和可用调用栈。点击错误跳转到对应行；代码已变化时显示运行快照。场景属性只读。补全提供常用词提示；未集成原生断点、单步或暂停时局部变量。

文件保存在 user://scripts。打开脚本不会自动运行。外部客户端将草案提交到同一个工作台，仍须手动审查并确认。

Ctrl+Enter：运行   Ctrl+S：保存   Ctrl+F：查找"""
const ZH := {
	"Running": "运行中",
	"GDScript script": "GDScript 脚本",
	"Choose a .gd file, or leave Source empty to create a new script.": "选择 .gd 文件；数据源留空则创建新脚本。",
	"Select script · running scripts stay active": "切换脚本 · 已运行的脚本继续执行",
	"Remove script": "移除脚本",
	"Remove this script and discard its unsaved changes?": "移除此脚本及所有视口中的运行内容，并丢弃未保存的修改？",
	"Rebuild the scene and restore the navigation snapshot?\nThis stops all scripts and removes their session nodes. Shared resource edits may remain.\nFiles, network activity and other external effects are not rolled back.": "重建场景并恢复导航快照？\n将停止所有脚本并移除其会话节点；共享资源修改可能保留。\n文件、网络和其他外部副作用不会回滚。",
	"Compile and run script · Ctrl+Enter": "编译并执行原生脚本 · Ctrl+Enter",
	"Remove script session nodes; direct edits remain. Synchronous loops cannot be interrupted.": "移除本次 ScriptSession；不回滚直接修改，不中断同步死循环。",
	"Save to the application script directory · Ctrl+S": "保存到应用用户目录 · Ctrl+S",
	"filename.gd": "文件名.gd",
	"Select a node to inspect its properties. Limited to 256 nodes and 12 levels per viewport.": "选择节点查看只读属性。最多显示 256 个节点、12 层。",
	"This object was deleted. Refresh the scene tree.": "对象已删除，请刷新场景树。",
	"<freed>": "<已释放>",
	"<%s, %d items>": "<%s，%d 项>",
	"Waiting": "待运行",
	"Script failed; direct scene edits remain": "执行/编译出错（直接场景修改不回滚）",
	"Confirm that you trust this script. Native code can access files, networks and processes.": "请先确认信任脚本；原生代码可访问文件、网络和进程。",
	"Script must not be empty or exceed 64 KiB.": "脚本不能为空，且最多 64 KiB。",
	"GDScript compilation failed: %s": "GDScript 编译失败：%s",
	"Compilation failed": "编译失败",
	"Workbench scripts must extend Node or a subclass such as Node3D.": "工作台脚本必须继承 Node 或 Node3D 等 Node 子类。",
	"Incompatible entry type": "入口类型不兼容",
	"_init cannot require arguments. Use run(viewer) to obtain the scene context.": "_init 不能要求必填参数；请通过 run(viewer) 获取场景上下文。",
	"Incompatible initializer": "初始化入口不兼容",
	"Define func run(viewer) with one scene context argument.": "需要入口 func run(viewer)，并接收一个场景上下文参数。",
	"Missing run(viewer)": "缺少 run(viewer) 入口",
	"Cannot instantiate script.": "无法实例化脚本。",
	"Instantiation failed": "实例化失败",
	"The script deleted the scene root. Reset the scene first.": "场景根已被脚本删除，请先重置场景。",
	"The script freed itself during initialization; run(viewer) was not called.": "脚本在初始化阶段释放了自身，run(viewer) 未执行。",
	"Initialization ended": "初始化已结束",
	"Running · _process / signals remain active": "运行中 · _process / 信号继续有效",
	"run(viewer) was called. Stop removes this script session.": "run(viewer) 已调用；点击“停止”清理本次 ScriptSession。",
	"Stopped · session nodes removed; direct edits retained": "已停止 · 会话节点已清理，直接修改保留",
	"Layer scenes rebuilt and navigation restored": "已重建图层场景并恢复导航快照",
	"Use a valid .gd filename without directories. Scripts are limited to 64 KiB.": "请使用有效的 .gd 文件名（不含目录），脚本最多 64 KiB。",
	"This file already exists. Overwrite it?": "文件已存在，是否覆盖？",
	"Invalid filename.": "无效文件名。",
	"Script exceeds 64 KiB.": "脚本超过 64 KiB。",
	"Output limit reached; %d messages were dropped.": "输出过多，已丢弃 %d 条消息。",
	"Run only scripts you trust.\nScripts have application permissions to access files, networks and processes.\nCompilation can execute static initializers. Synchronous loops block the UI and cannot be interrupted by Stop.\nReview the code before confirming.": "仅运行你信任的脚本！\n代码具有应用权限，可读写文件、联网和启动进程。\n编译可能运行静态初始化；死循环会卡住界面，停止无法强行中断。\n请审查代码，确认后运行。",
	"Rebuild the scene and restore the navigation snapshot?\nThis stops the script and removes its session nodes. Shared resource edits may remain.\nFiles, network activity and other external effects are not rolled back.": "重建场景并恢复导航快照？\n将停止脚本并移除其会话节点；共享资源修改可能保留。\n文件、网络和其他外部副作用不会回滚。",
	"Overwrite user://scripts/%s?": "覆盖 user://scripts/%s？",
	"Discard unsaved changes and open %s?": "当前代码尚未保存，丢弃修改并打开 %s？",
	"Saved %s": "已保存 %s",
	"HTTP headers for glTF / GLB (session only)": "glTF / GLB HTTP 请求头（仅当前会话）",
	"Headers are sent only to the model URL origin.": "请求头仅发送到模型地址的同源服务器。",
	"Invalid HTTP headers. Use one unique Name: value per line.": "HTTP 请求头无效。每行使用不重复的“名称: 值”。",
	"Custom HTTP headers currently require glTF or GLB.": "自定义 HTTP 请求头当前仅支持 glTF / GLB。",
	"Download": "下载",
	"Shadows": "阴影",
	"Uploading": "上传中",
	"Loading": "加载中",
	"Cancelled": "已取消",
	"Cannot create session cache.": "无法创建会话缓存。",
	"Cannot write model to session cache.": "无法将模型写入会话缓存。",
	"Remote glTF is not valid JSON.": "远程 glTF 不是有效的 JSON。",
	"Remote glTF must declare asset version 2.0.": "远程 glTF 必须声明 asset.version 为 2.0。",
	"Invalid glTF resource list.": "glTF 资源列表无效。",
	"Invalid glTF dependency URI.": "glTF 依赖 URI 无效。",
	"glTF dependencies must use HTTP(S); HTTPS downgrades are rejected.": "glTF 依赖必须使用 HTTP(S)，不允许从 HTTPS 降级。",
	"Remote glTF exceeds 256 dependencies.": "远程 glTF 超过 256 个依赖文件。",
	"Remote model and dependencies exceed the 256 MiB session limit.": "远程模型及其依赖超过 256 MiB 会话限制。",
	"Use a valid HTTP(S) model URL without embedded credentials.": "请使用有效的 HTTP(S) 模型地址，不要在地址中嵌入用户名或密码。",
	"Invalid, insecure or excessive model redirect.": "模型重定向无效、不安全或次数超限。",
	"Remote GLB has an invalid version 2 header.": "远程 GLB 的 2.0 文件头无效。",
	"Remote GLB has an invalid JSON chunk.": "远程 GLB 的 JSON 数据块无效。",
	"Model exceeds the 64 MiB per-content limit.": "模型超过单文件 64 MiB 限制。",
	"glTF scene generation failed.": "glTF 场景生成失败。",
	"Cannot open ZIP.": "无法打开 ZIP。",
	"ZIP must be at most 64 MiB.": "ZIP 最大为 64 MiB。",
	"ZIP central directory is missing.": "ZIP 缺少中央目录。",
	"ZIP64 is not supported.": "不支持 ZIP64。",
	"Multi-volume ZIP is not supported.": "不支持分卷 ZIP。",
	"ZIP directory limits exceeded.": "ZIP 目录超过限制。",
	"Corrupt ZIP directory.": "ZIP 目录已损坏。",
	"ZIP entry is out of bounds.": "ZIP 条目越界。",
	"Unsafe or duplicate ZIP path rejected.": "已拒绝不安全或重复的 ZIP 路径。",
	"ZIP links and special files are not supported.": "不支持 ZIP 中的链接和特殊文件。",
	"Encrypted or unsupported ZIP compression.": "不支持加密 ZIP 或该压缩方式。",
	"ZIP expansion limit exceeded.": "ZIP 解压大小或压缩比超过限制。",
	"Invalid ZIP local header.": "ZIP 本地文件头无效。",
	"ZIP payload is out of bounds.": "ZIP 数据越界。",
	"ZIP entry names disagree.": "ZIP 条目名称不一致。",
	"Unexpected ZIP directory data.": "ZIP 目录含意外数据。",
	"ZIP entry is corrupted or truncated.": "ZIP 条目已损坏或被截断。",
	"ZIP expanded size exceeds the session limit.": "ZIP 解压大小超过会话限制。",
	"Cannot extract ZIP to session cache.": "无法将 ZIP 解压到会话缓存。"
}
