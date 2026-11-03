# DigitalViewer v1.0 桌面开发版

基于定制 Godot 4.7.3-rc。当前已经从演示 UI 骨架切换为真实桌面运行时；**尚未满足完整 v1.0 发布门槛**。具体已实现能力、限制与验证记录见 [桌面实现状态](docs/desktop-v1.md)。产品范围仍以 [FEATURE_CHECKLIST.md](FEATURE_CHECKLIST.md) 为准。

引擎 DPI 补丁、OSG / 瓦片 / Draco 等定制及升级迁移要求统一记录在 [Godot 源码修改清单](../../GODOT_SOURCE_CHANGES.md)。

## 运行

Windows 可直接解压 `build/DigitalViewer-1.0.0-windows-x86_64-preview.zip`，双击同目录的 `DigitalViewer.exe`，保留旁边的 PCK。包内含使用说明、许可证和源码/制品散列。

在仓库根目录执行：

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --path projects\DigitalViewer
```

进入应用后，从每个视口左上角“＋”添加数据。启动时只有一个空视口，不创建演示图层，不请求第三方服务。

## 已接通的桌面功能

- 递归左右/上下分屏，独立 World3D、相机、环境、图层栈和会话视点；极小视口保留操作菜单。
- 视口、属性和脚本面板可分离到原生窗口并停靠；全屏视口退出后恢复原分屏树。
- Windows 主窗口、浮动窗口和弹窗按所在屏幕的系统 DPI 缩放，支持跨屏更新；保持原有字号，3D 视口按实际像素渲染。
- 本地/远程 glTF/GLB、目录/ZIP 入口选择，以及通过定制引擎流送 3D Tiles、OSG/OSGB；Cesium ion 3D Tiles 提供会话凭据输入。
- 远程模型的外部 buffer/纹理、相对路径、重定向、有限重试、同源自定义 HTTP 请求头、重复依赖下载复用和取消清理。
- glTF/GLB 和椭球图层可拖到另一视口，共享解析结果与网格资源，各自控制显隐和透明度；原生流送瓦片的跨视口共享仍待实现。
- 图层显隐、排序、定位、重载、暂停、移除、手工变换、glTF 动画、错误反馈和脱敏诊断；重载保留各引用的变换和显示状态。
- 结构树名称只负责选择，独立复选框控制显隐，箭头控制折叠；刷新保留折叠、选中项和滚动位置。树高度随视口变化，透明空白区域可直接操作场景；节点类型、状态和共享引用数在属性面板查看。
- 显式添加 WGS84 椭球、经纬网、坐标飞往和会话视点；轨道/飞行相机、三角形/PNTS 点拾取与位置标记、Textured/Unlit/Wireframe 显示。
- 应用级 GDScript 工作台：添加数据可新建/导入脚本图层，多脚本同时运行；每份脚本独立保留草稿、输出、运行快照，结构树显示实例与生成节点，可分别停止、重跑和移除。
- 本机 MCP 或 Plugin Bridge（二选一）、持久 Bearer 令牌（可导入/重置）、权限开关、草案审阅和审计；没有内置 AI 或模型配置。
- Plugin Bridge 供 Codex / Harness 等支持本机插件的客户端使用；与 MCP 共用 7 个工具和权限模型，不提供仓库、源码、文件或终端能力。

影像/WMTS/DEM、统一地理流送世界、完整浮动原点、瓦片共享、完整双语与正式多平台导出仍未完成。当前椭球使用归一化尺寸，不能当作已与 ECEF 3D Tiles 对齐的生产地理世界。

## 脚本 / MCP / Plugin Bridge

脚本必须显式选择视口：

```gdscript
extends Node
func run(viewer) -> void:
    var view = viewer.viewport("view_1")
    if view == null:
        return
    var content = viewer.content("view_1")
    var mesh = MeshInstance3D.new()
    mesh.mesh = BoxMesh.new()
    content.add_child(mesh)
```

关闭脚本面板或浮窗后，脚本继续运行。首次运行和代码修改后须审查确认；本次应用会话内，同一文档已确认且代码未变时直接重跑。脚本拥有应用权限，编译也可能执行静态初始化；同步死循环不能被停止按钮强制中断。详情见 [脚本工作台](docs/native-script-workbench.md)。

MCP 地址为 `http://127.0.0.1:8731/mcp`。从设置复制本次会话的令牌，通过 `Authorization: Bearer <token>` 发送；先 `initialize`，再携带响应中的 `Mcp-Session-Id`。`script.propose` 只提交草案，`script.run` 仍要求用户在应用中手动运行。端口不可用会在状态栏显示错误。

Plugin Bridge 地址为 `http://127.0.0.1:8731/bridge`，不使用 MCP 的 `initialize`/`tools/list` 会话握手；直接调用 `bridge.capabilities` 或 7 个工具方法，结果沿用 `content[0].text` JSON 包装。设置中可在 `MCP / Plugin bridge` 之间切换；两者互斥。Codex/Harness 插件只需实现本机 HTTP 客户端和手动运行提示。

验证脚本：[Test-BridgeClients.py](tools/Test-BridgeClients.py)；设计细节：[Plugin Bridge](docs/plugin-bridge.md)、[自动化 API](docs/automation-api.md)、[Harness 安装](docs/harness-plugin-install.md)。

## 验证与构建

```powershell
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Preset 'Windows Validation'
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Pack -Engine E:/WorkSpace/GitHub/godot/projects/DigitalViewer/build/validation/DigitalViewer.console.exe
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Pack -Engine E:/WorkSpace/GitHub/godot/projects/DigitalViewer/build/validation/DigitalViewer.console.exe -Graphics -Capture
.\projects\DigitalViewer\tools\Build-Desktop.ps1
py -3 .\projects\DigitalViewer\tools\Test-BridgeClients.py --engine .\bin\godot.windows.editor.dev.x86_64.console.exe --project .\projects\DigitalViewer
```

完整导出需要 `export_presets.cfg` 指定的同版定制模板；不使用仓库中旧版 `digital_viewer` 或 4.7.2 官方模板。生成制品位于 `build/desktop/`。Linux/macOS 只有预设，未在本机验收。当前预置开发编辑器的模块内容早于发布模板，PNTS/I3DM 验证必须使用新模板生成的验证包；版本字符串相同不代表模块相同。

跨屏 DPI 同样需要新构建的 Windows 模板。预置编辑器没有新增的 `display/window/dpi/per_monitor_dpi_aware` 支持，不能代替导出包验证跨屏缩放；升级包应同时替换 EXE/PCK。

## 快捷键

- `Ctrl+1..9`：活动视口；`Ctrl+L`：该视口图层树。
- `Ctrl+P`：属性；`Ctrl+Shift+F`：全屏活动视口；`Esc`：退出全屏/取消飞行。
- 拖动：轨道旋转；右键/中键/Shift 拖动：平移；滚轮：缩放。
- 飞行模式：视口获得焦点后用 `WASD`、`Q/E` 移动，`Shift` 加速。
- 脚本工作台：`Ctrl+Enter` 运行、`Ctrl+S` 保存、`Ctrl+F` 查找。

桌面入口为 `scripts/desktop/shell.gd`，数据生命周期在 `session.gd` / `data_store.gd`。旧桌面/移动/AI 原型代码保留作迁移参考，桌面导出不打包这些旧入口。UI 设计依据见 [design/ui-v1](design/ui-v1/README.md)。
