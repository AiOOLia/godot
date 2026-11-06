# DigitalViewer v1.0 桌面实现状态

更新：2026-11-06。当前为可运行桌面开发版，尚未达到完整 v1.0 发布标准。新增 CAD/ALL 构建配置和 [BREP Windows 预览版](brep-preview.md)：直接使用现有 CAD DLL 加载本地 NDS BREP、独立 BREP 几何 JSON、装配 JSON 及 ZIP 依赖。同名 `.geom` 有效时直接加载导出的离散面/线段，跳过 OCC 重建与离散；否则回退 OCC。BREP 结构树从真实数据根节点开始，省去文件名和内部 layer 容器，根节点保留数据显隐和属性操作。支持单击面高亮，属性面板通过新版 DLL 分别导出并显示原始面、OCC 重构结果面的 BREP JSON，可独立复制；结构树选择形体/零件/装配高亮对应部分，Ctrl 多选与视口独立显示，点击空白清除。

## 架构与已实现能力

- `scripts/desktop/shell.gd`：桌面入口，递归分屏、属性、脚本停靠、原生独立窗口、目录/ZIP 入口、设置、诊断。
- `pane.gd`：视口结构树名称选择、独立显隐复选框与折叠箭头；状态/结构刷新保留折叠、选中项和滚动位置。树使用视口可用高度，随分屏与浮窗缩放；类型、状态、引用数移至属性面板，生成节点使用弱引用选择，节点删除后回退到所属图层。
- `viewport_tree.gd`：结构树仅在节点行与可见滚动条接收鼠标，透明空白区域穿透到场景；保留完整可滚动高度，修复双视口加载后空白覆盖层阻断旋转、平移与缩放的问题。
- `dpi_scaling.gd`：每窗口系统缩放、弹窗归属与逻辑尺寸换算，监听 DPI 事件并定期检查显示配置；主窗口、浮动窗口、动态弹窗、菜单共用逻辑。保持主题 14、脚本 14、状态 12 的字号。3D 渲染目标按物理像素更新，拾取从 UI 坐标换算到渲染目标。
- `session.gd`：每视口状态、活动对象、数据引用和应用级脚本会话。启动只创建一个空视口，不恢复历史。
- `view_scene.gd`：独立 World3D/SubViewport、轨道/飞行相机、视点、显示、glTF 三角形和原生瓦片拾取。
- 视口光源面板统一显示和管理默认平行光及新增的平行光、点光源、聚光灯；可编辑颜色、强度、显隐、阴影，以及对应的方向、位置、范围、衰减和锥角。光源属性不随背景类型隐藏，新建局部光源按当前取景设置位置和范围。
- 每个平行光可单独开启“跟随相机”，默认关闭；开启后光线沿所属视口相机的观察方向照射，隐藏手动方向参数，关闭后恢复开启前的手动方向。
- `data_store.gd`：GLTFDocument 后台解析/主线程生成场景，TiledMeshInstance3D 异步流送、本地/远程 glTF/GLB、ZIP 会话缓存、引用计数和释放；重载保留图层变换/显隐/透明度。
- `remote_gltf.gd` / `http_source.gd`：外部 buffer/图片、RFC 3986 相对路径、GLB 内嵌几何与外部纹理、重定向、有限重试、同源请求头、依赖复用、删除/重载取消。单内容 64 MiB、单模型合计 256 MiB、256 个外部依赖。地址仅解析为 HTTP URI，缓存使用生成文件名；跨来源不发送自定义头，不继承根 URL 的查询凭据，不允许 HTTPS 降级。
- `geo.gd`：WGS84/ECEF 标量双精度换算与 East-Up-South 局部基。地球当前为归一化椭球网格。
- `zip_guard.gd`：解压前检查中央目录、路径、重复项、符号链接/特殊文件、压缩方式、大小和压缩比。拒绝 ZIP64、多卷、加密 ZIP；单项 64 MiB、总量 256 MiB、4096 项。
- `mcp_server.gd`：loopback MCP、持久 Bearer 令牌及导入/重置、场景读取、草案、截图授权和审计。`mcp_request_id.gd` 保留 JSON-RPC 整数/字符串 ID，解决 Codex 初始化响应反序列化失败。外部脚本不自动编译或运行。
- `scripts/scripting/`：原生 GDScript、CodeEdit、高亮、双语帮助与信任提示、输出/错误、实际场景树、显式视口 API。`script_library.gd` 管理独立文档与执行器，支持添加数据新建/导入脚本、多会话并行、分别停止/重跑/移除；同名文件保留不同身份，各自保存草稿、日志与运行快照。视口图层树显示实例和生成节点，运行中节点变化自动刷新。分离、停靠、关闭面板或浮窗均保留执行。本次应用会话中按文档记录最近确认的源码哈希，代码未变可直接重跑，首次运行或代码变化须确认；MCP 仍只请求用户手动运行。

glTF/椭球共享原型、网格和纹理资源，各视口有独立节点、透明度与显隐。流送瓦片当前由每个原生节点管理自己的资源，拖放共享会明确拒绝，不能把再次下载解释成共享。瓦片节点均分合计 1 GiB Resident 与 4 ms 上传目标；这尚不是涵盖所有 glTF/地理资源的全局预算器。非活动视口每隔一帧更新并降低分辨率/LOD，不设产品数量上限。

ZIP/远程缓存只存在本次会话，最后引用移除后删除，正在导入的任务结束后再删除其文件；退出等待导入线程收尾。删除图层立即丢弃过期结果，但 GLTFDocument 的当前同步解析工作不能被线程中途抢占。较大模型生成场景、ZIP 处理和首次构建拾取 BVH 仍可能阻塞主线程。

## 已运行验证

运行时验收使用当前源码构建的 `template_release` 导出的 Windows Validation 包，版本 `4.7.3.rc.custom_build.9c605b09a`。导出器为预置的 `bin/godot.windows.editor.dev.x86_64.console.exe`；该编辑器较旧，仍拒绝 PNTS/I3DM，不能代替发布运行时做全格式验收。

- `tools/Test-Desktop.ps1`：空启动、WGS84 边界、独立世界/相机、递归 7 视口、分屏可见面积/不重叠、全屏恢复、共享/释放、实际 glTF/GLB、显式 3D Tiles、OSG2 ASCII、B3DM/I3DM/PNTS/CMPT Resident、PNTS 点拾取、重载状态、脚本信任/独立窗口会话、HTTP MCP、ZIP、远程依赖/重试/凭据隔离/取消和脱敏。
- 图形模式：Windows Vulkan 1.3 / Forward+ / NVIDIA RTX 3080 Laptop，独立窗口停靠、1366×768 / 1920×1080 布局、真实双视口和截图。
- PCK 导出：生成 `build/desktop/DigitalViewer.pck` 并独立载入启动；旧 AI/移动原型、测试和设计稿未打入桌面包。
- Windows release：从当前工作区成功构建 `template_release`，生成可独立运行的 `build/desktop/DigitalViewer.exe`（约 71 MB）与 PCK。Windows Validation 包在同一 release 引擎下通过 **211 项无头检查、362 项图形检查**，均正常退出。源码工作区、编辑器与模板没有冻结为一致提交，因此仍不是正式签名发布。
- B3DM 兼容回归：ContextCapture 生成的部分瓦片恰好止于完整 GLB 的 4 字节边界，未添加容器末尾的 8 字节填充。原生 `3dtiles` 模块现在接受这种文件，表段对齐、声明长度、GLB chunk 和越界检查仍生效。`tests/desktop_tiles.gd` 在新 release 引擎中通过正常/无尾部填充加载与七种损坏文件拒绝测试；C++ 同等测试已补充，本轮未重建单测 editor。修改已登记到根目录 `GODOT_SOURCE_CHANGES.md` 的 GD-3DT-001。
- 武当山实测：只读加载用户本地 `Scene/Production_2_jd_3dtile.json`，修复前 20 秒出现 129 个失败内容；修复后 30 秒为 Ready、0 个失败、1699 个驻留内容、1313 个带纹理材质及 289 个当前可见实例。报告/截图为 `build/validation/dataset-stationary-report.json` / `dataset-stationary.png`；这些数据来自一次相机位置下的流式加载，不代表穷举全部瓦片和 LOD。
- 武当山 B3DM 导航验证：额外运行 45 秒，分两阶段旋转、拉近再拉远，最终仍为 Ready、0 个失败、285 个可见带纹理实例。报告/截图为 `build/validation/b3dm-navigation-report.json` / `b3dm-navigation.png`。可用 `Test-Desktop.ps1 -Pack -Engine <验证包 console.exe> -Graphics -Capture -Dataset <本地入口> -DatasetSeconds 45 -DatasetNavigate` 重测；此模式只读原数据，测试报告写入 build/validation，未打入桌面产品包。
- 传统 OSG 修复：补接现有本地 OpenSceneGraph 源码中的 DotOsgWrapper 静态对象注册，修正分词器非 ASCII 字节负下标，Windows 非 UTF-8 路径按系统 ANSI 编码兼容。图片与 ProxyNode 引用由异步队列加载。`tests/desktop_osg.gd` 验证无引号中文 DatabasePath 的 UTF-8/本地编码、PagedLOD 子文件、纹理与 ProxyNode；现有 OSG2 与其他格式测试仍通过。新增 C++ 测试尚未通过重建 editor 执行。修改与第三方来源登记在 GD-OSG-001；当前只覆盖静态场景子集。
- 武当山 OSG 实测：原始 `OSGB-LOD-1/1-WuDangShan/Production_2/Production_2.osg` 修复前根加载失败（`file not handled`），修复后 35 秒旋转/缩放验证为 Ready、0 个失败内容、20 个驻留内容、97 个材质（96 个带纹理）；近景阶段 78 个可见实例，拉远后 4 个。报告/截图为 `build/validation/osg-dataset-report.json` / `osg-dataset.png`。原数据缺少 `Production_2_0.jpg`，最低 LOD 因此无贴图并产生警告；同目录 `Production_2.osgb` 的内嵌纹理加载通过。未修改源数据，未穷举整个数据集；此前 B3DM 导航报告另存为 `b3dm-navigation-report.json` / `b3dm-navigation.png`。
- 桃园空瓦片修复：`Tile_314341526340_L10_4261570533.osgb` 为 215 字节的合法空 Group，旧转换器误报没有支持的几何。现在只将解析成功且仅由空 Group/Geode 构成的场景作为空文档驻留；包含不支持图元及截断 OSGB 的反例仍失败。本次仅改 Godot OSG 转换层，未改 FieldReader 或第三方解析库。`desktop_osg.gd` 新增六项检查，覆盖空根/嵌套空节点、截断/POINTS 拒绝、空子瓦片的 LOD 替换、兄弟几何保留和拉远恢复粗层；原生测试已补充但本轮未编译执行。
- 桃园实测：截图中的原始空文件单独加载为 Ready、1 个驻留内容、0 几何/材质、0 失败（`build/validation/taoyuan-empty-report.json`）。其所属入口 `Tile_314341526340.osgb` 30 秒旋转/缩放后为 Ready、0 失败、353 个驻留内容、1861 个带纹理材质，发现 1384 个瓦片；报告/截图为 `taoyuan-parent-report.json` / `taoyuan-parent.png`。这不是全部深层 LOD 的穷举验证。空文件诊断可在 `Test-Desktop.ps1 -Dataset <文件>` 上加 `-DatasetExpectEmpty`，普通数据验证仍要求可见几何。
- 结构树回归：`tests/desktop_tree.gd` 通过窗口物理鼠标事件验证名称/显隐/折叠独立，检查折叠与选中项恢复、动态节点插入后滚动保持、子节点类型/状态属性及删除后的选择回退；覆盖分屏、上下分割、浮窗和窗口缩放的树高度。截图为 `build/validation/desktop-tree.png`。
- 双视口鼠标回归：`tests/desktop_navigation.gd` 在两个视口分别加载 OSG2 ASCII 几何并等待驻留，通过窗口物理鼠标事件验证透明树空白处和树外的左键旋转、右键/中键平移、双向滚轮缩放、活动视口与相机隔离，以及拖动跨过树行后释放和原生浮窗导航。修复前空白区域全部失败，修复后通过；树行滚轮与滚动条操作仍只滚动树。未使用截图中的 Production_2/3.osgb 大型数据进行重测。
- 脚本回归：`tests/desktop_scripts.gd` 从添加数据实际入口新建/导入，同名文档草稿保留、双脚本 `_process` 持续执行、日志隔离、跨视口图层/实例/运行后新增节点、显隐、独立停止/重跑/失败/删除、最后引用关闭、全局重置、MCP 多会话查询、导入静态初始化不执行。关闭停靠面板、浮窗系统按钮和浮窗内部按钮均检查两个实例、生成节点和动画继续；重开保留草稿/快照，未改代码及停止后重跑不重复确认。修改代码、取消确认、确认期间切换/编辑、同名同源码新建/移除后重建各自隔离，MCP 不继承自动执行授权。图形截图：`build/validation/desktop-multiple-scripts.png`。
- 单视口顺序回归：实际 `add_tree.gd` 后运行 `satellite.gd`，修正保存脚本中 `ids[1]` 的越界访问为 `ids[0]`。验证树的 31 个几何节点保留、卫星旋转持续变化、视口归属和渲染像素差异；截图为 `build/validation/desktop-tree-satellite.png`。本次修正是用户脚本的数据目标，桌面运行时代码未变。
- MCP 修复回归：检查初始化 `id: 0` 为整数、64 位整数精确回传、字符串 ID、非法/重复 ID 拒绝；持久令牌、重新启用服务、重建服务、手动重置/导入、写入失败保留凭据。设置界面的导入、掩码输入和取消重置通过图形检查。`tools/Test-McpClient.py` 以 Python 严格 JSON 解码器核验响应类型，并用实际本机 Codex app-server 成功发现 7 个工具、调用 `viewport.list` / `scene.describe` / `script.inspect` / `audit.list`；独立应用进程重启后复用相同令牌通过。
- DPI 实机回归：2560×1600 / 150% 与 1920×1080 / 100% 双屏往返，主窗口、独立视口、属性与脚本窗口独立缩放；菜单、添加数据、嵌套文件选择、脚本确认及已打开弹窗跨屏；最大化切屏、逻辑尺寸保持、物理分辨率和实际输入拾取。`tests/desktop_dpi.gd` 捕获真实 `dpi_changed` 事件，截图为 `build/validation/desktop-dpi-150.png` 与 `desktop-dpi-100.png`。125%/200%、同屏运行中修改系统缩放和显示器热插拔尚未实机验证；Linux/macOS 同样未验收。
- 沙箱无头进程会报告读取 Windows 根证书存储失败；图形沙箱外运行没有此提示。远程真实 TLS 和 ion 服务尚未用凭据验收。

脚本测试额外捕获运行时错误，出现脚本/引擎错误即判为失败。旧 `ai_smoke`、`scripting_smoke` 和 `runtime_checks` 对应旧原型 API，不作为这版桌面验收。

Windows 跨屏支持需要本次更新的原生模板：项目启用 `display/window/dpi/per_monitor_dpi_aware`，Windows 后端启用 Per-Monitor V2（旧系统回退 Per-Monitor），按目标 DPI 计算窗口边框，并把 `WM_DPICHANGED` 转为 `Window.dpi_changed`。原生层应用 Windows 建议的窗口矩形，应用层只改变内容缩放，避免窗口重复放大。该引擎开关默认关闭，DigitalViewer 显式启用；旧预置编辑器未包含此修改。应用包需同步更新 EXE/PCK。

## 未完成的发布阻塞项

1. 影像 XYZ/WMTS/ion imagery、动态地球四叉树与地平线裁剪、DEM 合成/夸张、GeoTIFF/COG/libtiff。没有把这些能力伪装成已加载图层。
2. 统一 ECEF 原点、地球/DEM/3D Tiles 地理对齐、浮动原点、地理锚点/Gizmo。当前椭球是归一化展示，原生地理瓦片使用自身坐标基。
3. 原生流送瓦片共享 CPU/GPU 资源与一次下载/解析、瓦片透明度；完整多视口全局预算器。
4. 动态经纬网间距、标签/地形贴合、哥伦布显示、Heading/Pitch 飞往、地形下方相机约束。
5. 跨独立数据源的一般请求复用、瓦片自定义 HTTP Header 和完整引擎错误本地化；当前模型依赖和共享图层内部已复用下载。远程导入仍可能在场景生成阶段阻塞主线程。
6. 全量双语 UI/错误审阅、完整场景重置事务、面板拖出手势；工作台帮助/信任/状态已双语，分离按钮已实现。
7. 参考大数据 60 FPS/95% 帧时间与内存基线，Windows 全格式回归，Linux/macOS 运行及签名、公证。
8. 发布主体、版权、最终图标和正式隐私文本。

## 构建

```powershell
scons platform=windows target=template_release arch=x86_64 module_osgdb_enabled=yes module_3dtiles_enabled=yes module_tiled_mesh_3d_enabled=yes module_draco_enabled=yes module_usd_enabled=no debug_symbols=no accesskit=no d3d12=no vulkan=yes -j8
.\projects\DigitalViewer\tools\Build-Desktop.ps1
```

使用 Vulkan/Forward+；本地未安装 AccessKit/D3D12 SDK，因此本次模板不包含这些驱动。不把现存 `godot.windows.digital_viewer.dev.x86_64` 当作模板，其报告版本为旧 `4.7.beta.custom_build.e3cf4f0fc`。Linux x86_64、macOS Universal 导出预设指向待构建的定制模板，没有使用 4.7.2 官方模板替代。

`Build-Desktop.ps1` 写入制品散列、运行时代码逐文件散列、编辑器版本、源码 HEAD/dirty 状态和第三方许可副本（含 `OSG-LICENSE.txt`）。工作区当前包含已有未提交引擎改动，编辑器与新模板的模块内容确实不同；最终发布必须从冻结的同一源码构建编辑器和模板，重新回归。2026-10-28 从本机已有 OSG 源码补接传统对象 wrappers，并修正分词器索引；未下载或替换解析库。上述武当山 OSG/OSGB 是实际分页数据的有限视角验证，不能推广为全部数据与性能验收。

导出环境回归：

```powershell
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Preset 'Windows Validation'
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Pack -Engine E:/WorkSpace/GitHub/godot/projects/DigitalViewer/build/validation/DigitalViewer.console.exe
```

验证包与用户包使用相同源码和 release 模板；验证包额外包含测试源码和原始数据夹具。用户包仅包含运行时。`disable_path_overrides` 使用引擎默认安全配置，release 模板不接受编辑器式 `--path` 启动，应运行打包后的 EXE。
