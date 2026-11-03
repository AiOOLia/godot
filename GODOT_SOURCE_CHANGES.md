# Godot 源码修改清单与升级记录

更新：2026-10-28。本文记录这个仓库相对本地历史基线的引擎定制，用于后续升级、冲突处理和验收。**保留现有 Godot 源码实现的 Windows DPI 适配，不迁移为 GDExtension。**

配套文件：[路径登记表](GODOT_SOURCE_CHANGES.json)、[只读检查工具](misc/scripts/check_local_engine_changes.py)、[DigitalViewer 实现及验证状态](projects/DigitalViewer/docs/desktop-v1.md)。

## 1. 基线与记录范围

| 项目 | 本次盘点结果 |
| --- | --- |
| 引擎版本 | `version.py`：`4.7.3-rc`；运行时显示 `4.7.3.rc.custom_build.9c605b09a` |
| 本地比较基线 | `f6ab5db28b9d989a3ae710d57410a123cf0e4b23`，2026-08-20；OSG 接入提交的父提交 |
| 当前 HEAD | `9c605b09a54f08c28548d8b018ffe078761e1ec0`，2026-09-04，`支持 osg 数据加载` |
| 已提交定制 | HEAD 中的 `modules/osgdb/`、`thirdparty/osgdb/` 和 `thirdparty/README.md` |
| HEAD 之后的工作区 | DPI、通用瓦片、OSG 适配、3D Tiles、Draco/glTF、USD 等；包含修改、删除和未跟踪文件，尚未形成对应提交 |
| 来源核实边界 | 上述哈希从本地 Git 历史读取；没有独立核验其与官方远端/tag 的对应关系。不能将当前 HEAD 当作干净官方基线 |

本文按当前源码归类，不把既有模块的修改归功于本次清单整理，也不把源码存在等同于已发布。新增模块具体引入日期、提交号和历史测试结果无法从 Git 确认时，标为待补录。

`projects/` 下的应用、GDScript、测试、打包配置单独维护，不算引擎核心补丁；它们与引擎的配合点仍在对应条目中记录。临时 `.tmp_tests/` 和历史计划 `TiledMeshInstance_PLAN.md` 不作为升级移植依据。Git 忽略的 SDK、缓存、预编译库和构建产物也不由路径检查工具备份或验收。

## 2. 修改索引

| 编号 | 修改 | 类型与状态 | 依赖 / 升级关注点 |
| --- | --- | --- | --- |
| [GD-DPI-001](#gd-dpi-001) | Windows 每显示器 DPI、跨屏窗口与事件 | 核心补丁；项目可选开启；Windows 已验证 | `Main` 设置注册、Windows `DisplayServer`、Window 信号；优先检查上游是否已有等价实现 |
| [GD-TILE-001](#gd-tile-001) | 通用瓦片流送与渲染框架 | 新增原生模块；已供应用使用；未提交 | 格式注册、后台任务、RenderingServer、编辑器 API |
| [GD-OSG-001](#gd-osg-001) | 现有 OSG 接入转为通用框架适配器 | 已提交基础 + 工作区重构 | 依赖 GD-TILE-001；保留现有解析库 |
| [GD-3DT-001](#gd-3dt-001) | 3D Tiles 格式、内容与 ion 接入 | 新增原生模块；部分格式已验证；未提交 | 依赖 GD-TILE-001，Draco 为可选依赖 |
| [GD-DRACO-001](#gd-draco-001) | Draco 解码与 glTF 接入 | 新模块 + 原有 glTF 修改 + 第三方库；未提交 | glTF accessor API、属性类型、模块关闭分支、许可证 |
| [GD-USD-001](#gd-usd-001) | USD 导入与 OpenUSD SDK 准备 | 既有可选模块；默认关闭；未作为本版应用验收 | SDK ABI、异常、共享库部署；不得因升级自动启用 |
| [GD-MAINT-001](#gd-maint-001) | 本清单、登记表和检查命令 | 维护工具；不改变运行时 | 更新登记基线与条目，防止漏记 |

## GD-DPI-001

**目的：**主窗口、独立视口、属性/脚本浮窗和弹窗在不同 Windows 系统缩放屏幕之间移动时，保持合理的逻辑尺寸、清晰度与输入坐标。此次不调整主题或字体字号。

**原始行为：**这份基线的 Windows 后端使用 `PROCESS_SYSTEM_DPI_AWARE`；没有处理 `WM_DPICHANGED` 并转发 `Window.dpi_changed`。Godot 已有 `content_scale_factor`，但仅调应用缩放不能补齐这些原生窗口行为。

| 文件 | 修改锚点 | 修改内容 |
| --- | --- | --- |
| [main/main.cpp](main/main.cpp) | `Main::setup`、`GLOBAL_DEF_RST` | 注册 `display/window/dpi/per_monitor_dpi_aware`，默认 `false`，需重启 |
| [platform/windows/display_server_windows.cpp](platform/windows/display_server_windows.cpp) | `DisplayServerWindows::DisplayServerWindows` | 项目开启该设置且允许 HiDPI 时，优先设置 `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`，失败回退 `PROCESS_PER_MONITOR_DPI_AWARE`；编辑器与项目管理器保留原进程感知选择 |
| 同上 | `WndProc` 的 `WM_DPICHANGED` | 对普通顶层窗口应用 Windows 建议矩形，再发送 `WINDOW_EVENT_DPI_CHANGE`；全屏、最大化、最小化和嵌入窗口跳过建议矩形处理 |
| 同上 | `adjust_window_rect_for_dpi`、`_create_window`、`window_set_position` | 开启设置时使用 `AdjustWindowRectExForDpi`，不可用时回退；计算目标 DPI 下窗口边框 |
| [doc/classes/ProjectSettings.xml](doc/classes/ProjectSettings.xml) | 新项目设置 | 记录开关、重启要求和应用仍须更新内容缩放 |
| [doc/classes/Window.xml](doc/classes/Window.xml) | `dpi_changed` | 补充 Windows 开启该设置后的信号支持 |

**应用配合：**[project.godot](projects/DigitalViewer/project.godot) 显式开启设置；[dpi_scaling.gd](projects/DigitalViewer/scripts/desktop/dpi_scaling.gd) 设置每个窗口的 `content_scale_factor`、弹窗归属和尺寸，响应 DPI 信号并定期检查。Windows 已调整原生窗口矩形，应用层不能再次按比例放大窗口。`pane.gd` 按物理像素设置 3D 渲染目标并换算输入。主题 14、代码 14、状态 12 的字号保持原值。

**验证证据：**`tests/desktop_dpi.gd` 已在 Windows 2560×1600 / 150% 与 1920×1080 / 100% 双屏往返验证主窗口、视口/属性/脚本浮窗、菜单、添加数据、嵌套文件选择、已打开弹窗、最大化切屏、物理分辨率及输入拾取。截图见 `projects/DigitalViewer/build/validation/desktop-dpi-150.png`、`desktop-dpi-100.png`。125%/200%、运行中修改系统缩放、显示器热插拔、旧 Windows 回退路径尚未实机验收。

**升级处理：**

- 先检查新版本 Windows 后端是否已原生支持 Per-Monitor V2、`WM_DPICHANGED` 和 DPI 事件；若已有，使用上游实现并调整应用设置，避免双重窗口缩放。
- 按函数/符号迁移，不按旧行号贴补丁；检查新窗口创建、定位、边框、全屏、嵌入及初始化时序。
- 保留项目开关的默认关闭语义；记录上游是否更改默认行为。开关关闭、HiDPI 关闭、编辑器和项目管理器需补做回归，不能从 DigitalViewer 开启路径的测试推断通过。
- 如果删除该补丁，同步删除自定义设置与文档，并调整应用对 Windows 自动调整矩形的假设；先通过等价跨屏测试再标记“被上游替代”。
- 重建 editor 与导出模板，并重新导出应用；不能只更新 PCK 来分发引擎改动。

**提交 / 上游状态：**当前为工作区补丁，未记录上游 PR；尚未认定任何上游版本已覆盖它。

## GD-TILE-001

**目的与范围：**新增 [modules/tiled_mesh_3d/](modules/tiled_mesh_3d/)，从 OSG 专用实现抽出格式无关的 `TiledMeshInstance3D`、`TiledOriginController3D`、选项/格式注册、请求队列、图片解码、CPU mesh 打包、主线程上传、LOD、取消与代次隔离、预算统计和编辑器预览。公共接口说明见 [ARCHITECTURE.md](modules/tiled_mesh_3d/ARCHITECTURE.md)。

**重点文件：**`tiled_mesh_format.{h,cpp}`、`tiled_mesh_format_registry.{h,cpp}`、`tiled_mesh_instance_3d.{h,cpp}`、`tiled_mesh_data_request_queue.*`、`tiled_mesh_data_source.*`、`tiled_mesh_image_decode_queue.*`、`tiled_mesh_mesh_pack_queue.*`、`tiled_origin_controller_3d.*`、`editor/`、`register_types.cpp`、`config.py`、`SCsub`、`doc_classes/`。

**构建边界：**当前 `config.py` 要求线程、启用 3D，允许 Windows / LinuxBSD / Web。这只是编译条件，不是三个平台均已验收。必须与 OSG 适配迁移一起处理，避免同名类重复注册。

**升级处理：**核对 `RenderingServerTypes::SurfaceData` / RID、Mesh 数组布局、主线程资源生命周期、线程退出、HTTPClient、ClassDB、编辑器插件与 Gizmo 接口。继续维持 CPU 数据在工作线程、GPU 提交在主线程，以及旧 generation 不得发布结果的约束。多视口的原生流送资源共享仍未完成，不能仅因编译成功认定已解决。

**验证：**`tests/test_tiled_mesh_format.h`、`tests/test_tiled_mesh_http.h` 覆盖注册、SurfaceData、点几何、异步队列、流送和 HTTP。应用集成覆盖加载、显隐、重载、拾取和释放；本次盘点未重新运行所有 C++ 测试。`TiledMeshInstance_PLAN.md` 是历史计划，不代表已完成事项。

## GD-OSG-001

**基础：**现有 OSG 解析库已完整提供，继续沿用 `thirdparty/osgdb/`，不下载、替换或重写解析库。基础接入来自已提交的 `9c605b09a54f08c28548d8b018ffe078761e1ec0`。`thirdparty/README.md` 记录为 OpenSceneGraph 3.7.x 源码快照，未给出精确上游 commit；升级前应补齐来源信息或保留当前 Git tree 作为可复现依据。

**本地重构：**`modules/osgdb/` 依赖 GD-TILE-001，新增 `osg_tiled_mesh_format.{h,cpp}` 和 `OsgTiledMeshOptions`，`register_types.cpp` 改为注册格式加载器。转换器、图片读取器和解析队列改用通用文档/数据源；保留 LOD / replacement group 归属；新增基于 `PackedByteArray` 的输入流以减少解析前复制。`SCsub` 不再编译旧 OSG 专用编辑器目录。

以下是相对当前 HEAD 的删除/迁移关系，**不是丢弃对应能力，也不是逐字节重命名**：

| 原 `modules/osgdb/` 路径 | 当前对应实现 |
| --- | --- |
| `tiled_mesh_instance_3d.{h,cpp}` | `modules/tiled_mesh_3d/tiled_mesh_instance_3d.{h,cpp}` |
| `tiled_origin_controller_3d.{h,cpp}` | `modules/tiled_mesh_3d/tiled_origin_controller_3d.{h,cpp}` |
| `osg_data_request_queue.{h,cpp}` | `modules/tiled_mesh_3d/tiled_mesh_data_request_queue.{h,cpp}` |
| `osg_data_source.{h,cpp}` | `modules/tiled_mesh_3d/tiled_mesh_data_source.{h,cpp}` |
| `osg_image_decode_queue.{h,cpp}` | `modules/tiled_mesh_3d/tiled_mesh_image_decode_queue.{h,cpp}` |
| `osg_tile_document.h` | `modules/tiled_mesh_3d/tiled_mesh_format.h` 的通用数据结构 |
| `editor/tiled_mesh_instance_3d_editor_plugin.{h,cpp}` | `modules/tiled_mesh_3d/editor/` |
| `doc_classes/TiledMeshInstance3D.xml`、`TiledOriginController3D.xml` | `modules/tiled_mesh_3d/doc_classes/` |

**升级处理：**共同移植 OSG 适配器与通用框架；保留静态 reader/serializer 注册、`OSG_LIBRARY_STATIC` / `OSGDB_LIBRARY_STATIC` / zlib 配置；只使用源码构建，不把目录内预编译库作为输入。OSG 对象解析/析构继续留在串行解析线程，不能未经验证改成多线程解析。性能说明见 [PERFORMANCE_ISSUES.md](modules/osgdb/PERFORMANCE_ISSUES.md)，它包含历史命名和待办，不是性能验收证据。

**验证：**`modules/osgdb/tests/test_osg_scene_converter.h` 中的 `[OSGDB]` 测试含 ASCII、OSGB、zlib、PagedLOD、fallback、取消和 HTTP；DigitalViewer 已验证 OSG2 ASCII 几何驻留与拾取。未重新验收所有 OSG 格式。

**2026-10-28 传统 OSG 文本修复：**武当山 `Production_2.osg` 使用 `PagedLOD { ... }` 传统语法，既有构建只接入 OSG2 对象 serializers，传统 reader 的 DotOsgWrapper 注册为空，因此入口报错。另有未加引号的中文 `DatabasePath` 被分词器截断，以及 Windows ANSI/GBK 路径误按 UTF-8 解码的问题。

| 文件 / 锚点 | 修改与升级保留要求 |
| --- | --- |
| `thirdparty/osgdb/src/osgDB/deprecated-dotosg/osg/` | 从本机已有 OpenSceneGraph 3.7.0-rc1 源码补接 22 个传统对象 wrapper/helper `.cpp` 与 `Matrix.h`；未下载或替换现有库。`SOURCE.json` 记录原文件 SHA256、来源和构建适配，`LICENSE.txt` 保留上游许可证。上游 commit 未知。仅静态网格/材质/纹理/变换/LOD/ProxyNode 子集 |
| `modules/osgdb/SCsub`、`osg_static_init.cpp` | 编译上述源码并显式引用 `USE_DOTOSGWRAPPER`，避免静态链接裁掉注册；OSG2 注册保留 |
| `thirdparty/osgdb/src/osgDB/FieldReader.cpp` / `_readField` | 两个分隔符表索引转为 unsigned byte，消除非 ASCII 字节引起的负下标越界和路径截断；UTF-8 与 ANSI 均需回归 |
| `modules/osgdb/osg_scene_converter.cpp` | Windows 路径先严格验证 UTF-8，仅非法时按系统 ANSI code page 解码（中文系统为 GBK）；`DeferredOsgResources` 保留图片 URI、延迟 ProxyNode 外部引用，文件访问仍走异步队列；失败时保留首选 reader 的具体错误，不被备用 reader 的 `file not handled` 覆盖 |
| `modules/osgdb/tests/test_osg_scene_converter.h` | 增加传统 reader/writer 和 ProxyNode 延迟引用测试；本轮未重建原生单测 editor，应用正例使用新 release 引擎验证 |

应用新增 `tests/desktop_osg.gd` 和生成式三角形 fixture，验证无引号中文路径的 UTF-8/本地编码、纹理队列、PagedLOD 与 ProxyNode。打包脚本随包提供 `OSG-LICENSE.txt`。实际数据仍缺少其引用的 `Production_2_0.jpg`，这是最低 LOD 的源纹理缺失，不能将几何加载成功表述为数据依赖完整；同目录 `.osgb` 含内嵌纹理且已验证。原始数据未修改。升级时保留来源记录、静态注册、串行解析及延迟资源约束，并同步重建 EXE/PCK。

**2026-10-28 空 OSGB 瓦片修复：**桃园 `Tile_314341526340_L10_4261570533.osgb` 只有 215 字节，读取成功后为不含子节点/几何的 `osg::Group`，仅含渲染状态。`modules/osgdb/osg_scene_converter.cpp` 原来将所有无几何/外链场景作为损坏文件拒绝；现在通过 `is_empty_container_tree` 识别仅由空 Group/Geode 构成的节点树，返回合法空文档。仍经原 reader 完整读取，不将存在但不支持的 drawable/primitive 当作空瓦片；截断 OSGB 的读取错误仍失败。通用流送已有空文档驻留与 LOD 完成语义，本次不修改通用调度器、`FieldReader.cpp` 或第三方解析库。

升级保留此区分，并回归空子瓦片替换自身粗层、相邻几何保留、拉远恢复粗层和截断/不支持图元拒绝。新增原生单测覆盖 OSG2 文本、普通及压缩 OSGB 空节点，尚未重建单测 editor；实际执行的是新 release 引擎的应用回归（生成式 OSGB v91 和传统 OSG）。截图原文件已为 Resident、0 渲染实例、0 失败；其所属入口 30 秒旋转缩放验证失败内容为 0，报告见应用文档。原数据未修改。

## GD-3DT-001

**范围：**新增 [modules/3dtiles/](modules/3dtiles/)，通过 `Tiles3DFormatLoader` / `Tiles3DOptions` 注册到通用框架。关键文件为 `tiles_3d_format.*`、`tiles_3d_parser.h`、`parser/`、`register_types.*`、`SCsub`、`config.py` 和 `doc_classes/Tiles3DOptions.xml`。

**现有实现：**显式 tileset、变换/包围体、GLB/glTF、B3DM、CMPT、I3DM、PNTS，批次信息及拾取所需数据、ion endpoint 获取与凭据作用域。`parser/i3dm/`、`parser/pnts/` 和相应测试已存在，不能被旧 README 中“拒绝 i3dm/pnts”的文字覆盖；该 README 有尚待同步的旧限制描述。

**依赖与边界：**强依赖 GD-TILE-001，可选依赖 GD-DRACO-001；实际可编译平台受两者 `config.py` 的交集约束，不能只看本模块列出的六平台。不是完整 Cesium SDK；隐式切片、影像、地形与真实 ion/TLS 服务不因这些解析器存在而视为已验收。

**升级处理：**检查 WorkerThread / 容器 API、Image 解码、网络取消与认证重定向范围、double 源坐标到局部 Vector3 转换、可选 Draco 编译开关。保留子内容相对 URI、批次编号、点几何和模型实例语义。

**验证：**`modules/3dtiles/tests/test_tiles_3d.h` 的 `[3DTiles]`；应用 `projects/DigitalViewer/tests/desktop_tiles.gd` 已验证 B3DM/I3DM/PNTS/CMPT 驻留及点拾取。真实 ion 凭据服务未验收。模块 README 中示例测试命令应使用新编译的测试引擎，不应复用旧预置 editor。

**2026-10-28 B3DM 兼容修复：**ContextCapture Center edition Update 16 的武当山数据部分文件止于完整 GLB 的 4 字节边界，未补齐 B3DM 最后的 8 字节填充，旧解析器报 `Invalid standard version 1 tile header or alignment.`。

| 文件 / 锚点 | 修改与升级保留要求 |
| --- | --- |
| `parser/tile_table.h` / `LegacyTile::load` | 增加默认关闭的 `p_allow_unpadded_glb`；仅允许调用方放宽容器末尾到 4 字节，表段仍要求 8 字节边界，长度一致与越界检查保留 |
| `parser/b3dm/b3dm_parser.cpp` / `Decoder::decode_b3dm` | 仅 B3DM 开启兼容；非 8 字节文件必须恰好止于完整 GLB 声明长度，继续验证 GLB v2 与 chunk；I3DM/PNTS/CMPT 不放宽 |
| `tests/test_tiles_3d.h` | 新增缺少尾部填充的正例及七种长度/截断/表段/GLB 损坏反例；本轮未重建 C++ 单测 editor，等价正反例已通过新 release 引擎的应用测试 |

路径相对 `modules/3dtiles/`，归入现有 GD-3DT-001，无新增核心补丁。升级需迁移此兼容行为及边界回归，并重新编译模板、同步导出 EXE/PCK；不能只替换应用脚本。实际数据验证与报告见 [desktop-v1.md](projects/DigitalViewer/docs/desktop-v1.md)。原始数据未修改。

## GD-DRACO-001

**目的：**给通用 3D Tiles 路径与原生 `GLTFDocument` 提供 `KHR_draco_mesh_compression` 解码。

| 路径 | 修改内容 |
| --- | --- |
| `modules/draco/` | `DracoDecoder` 封装、限额/错误检查、构建与测试；`is_enabled()` 默认开启 |
| `thirdparty/draco/` | Draco 1.5.7，来源 commit `8786740086a9f4d83f44aa83badfbea4dce7a1b5`；构建只列出解码依赖，目录中的其它源码不代表已启用 |
| `modules/gltf/config.py` | 添加可选 Draco 依赖 |
| `modules/gltf/gltf_document.cpp` | `_parse_meshes` 解压 primitive，按 unique ID 映射属性、校验 accessor、转换索引；开启模块时声明支持扩展，关闭时明确拒绝相关压缩内容 |
| `modules/gltf/tests/test_gltf_draco.h` | 通过官方压缩 GLB fixture 验证 GLTFDocument 路径 |
| `modules/gltf/README.md`、`thirdparty/README.md`、`COPYRIGHT.txt` | 能力说明、版本来源、Apache-2.0 版权登记 |

**升级处理：**优先确认上游是否已支持 Draco；若替换实现，保留等价属性/索引/限额回归。重点核对 accessor 的组件类型、normalized/count 校验和编码 API，不能只合并 include 与扩展名列表。测试 `module_draco_enabled=yes/no` 两种构建，确认未压缩 glTF 不受影响；升级第三方版本时同步 `SCsub` 源文件列表、feature header 和许可证。

**验证：**`modules/draco/tests/test_draco_decoder.h` 的 `[Draco]`、glTF 的 `*KHR_draco*` 测试及 3D Tiles Draco case 已提供；本次清单整理未重新执行这些 C++ 测试，不能用普通 glTF 应用检查代替压缩格式验收。

## GD-USD-001

**范围与状态：**工作区已有 `modules/usd/` 和 `thirdparty/openusd/` 准备脚本，当前 `module_usd_enabled` 默认关闭。包括 `USDDocument`、编辑器导入器、静态网格测试及 SDK 检查；不属于 DigitalViewer 本版已接通能力。本次清单只登记，不启用、不下载 SDK。

**依赖：**OpenUSD `v24.08`，commit `59992d2178afcebd89273759f2bddfe730e59aa8`，记录于 `modules/usd/config.py` 与 `thirdparty/openusd/build.py`。SDK 获取/预构建需显式执行；SCons 不联网、不自动构建依赖。源代码、install 和 probe-build 目录由模块自己的 `.gitignore` 排除。

**升级处理：**核对编辑器 importer API、`USDDocument` 与 OpenUSD ABI；Windows 目前要求 MSVC、`use_static_cpp=no`、`debug_crt=no`、匹配 `/MD` 的共享 monolithic SDK。异常配置限定在模块构建环境；最终应用还需部署 SDK 共享库。Web 不支持。保留默认关闭，除非另行完成启用与平台验收。

**验证入口：**`modules/usd/tests/test_config.py`、`thirdparty/openusd/test_build.py`、`modules/usd/tests/test_usd_document.h`。没有将它们计入当前 DigitalViewer 的通过数量。

## GD-MAINT-001

本次新增本文、`GODOT_SOURCE_CHANGES.json`、`misc/scripts/check_local_engine_changes.py`，在根 README 与 DigitalViewer README 增加入口。它们不改变引擎或应用运行行为。

在仓库根目录执行（Python 3；本机可用 `D:/Program Files/Python312/python.exe`）：

```powershell
& 'D:/Program Files/Python312/python.exe' misc/scripts/check_local_engine_changes.py
& 'D:/Program Files/Python312/python.exe' misc/scripts/check_local_engine_changes.py --json
```

工具只读 Git：比较登记基线到完整工作区的已跟踪差异，并加入未被忽略的未跟踪文件。输出每项的路径数量；`--json` 输出完整文件归属。退出码 `0` 表示所有路径已登记或明确排除，`1` 表示有漏记路径，`2` 表示配置/Git 错误。`--base <commit>` 可临时更换比较基线，不修改登记表。

**限制：**它检查路径归属，不检查修改含义、同一文件新增功能是否已写入说明、二进制是否匹配或代码正确性。自定义模块按目录归类，新增模块内部文件不会触发漏记，但仍须更新说明。`main/main.cpp` 等上游文件按精确路径登记，不用整个 `main/` 或 `platform/` 目录掩盖新补丁。`--json` 可以核查显式排除项。被 Git ignore 的文件不在扫描范围内。

**本次工具验证：**实际工作区覆盖检查通过，登记 1,220 个差异路径、明确排除 258 个应用/临时路径；共享文件可归属多个条目，分项数量不能直接相加。另用内存中的登记表副本移除 DPI 条目，成功检出四个漏记文件；验证了目录边界匹配、无效基线报错和文档本地链接。数量仅代表本次盘点，后续以命令输出为准。

## 3. 后续修改的维护规则

每次引擎修改都与清单更新一起提交：

1. 复用或新增稳定编号，写清“触发问题 → 改后行为”，列出文件、关键符号、设置默认值、依赖与兼容影响。
2. 更新 JSON 路径登记，执行检查工具。应用代码留在应用文档，但涉及引擎契约的配合项需交叉引用。
3. 写明实际执行的测试、平台/构建类型、结果及尚未验证项；测试存在不等于测试通过。
4. 提交完成后补录 commit / PR。按功能拆分补丁，避免将 DPI、第三方更新和瓦片架构混在一个升级提交中；同一文件可属于多个条目。
5. 上游已有等价能力时记录替代版本、移除的本地实现和等价回归结果，保留历史编号，不直接删除历史记录。

新增条目模板：

```text
编号 / 标题：GD-XXX-001
状态：待实现 / 本地保留 / 待迁移 / 被上游替代 / 停用
比较基线与引入 commit：
问题与期望行为：
文件、关键符号、公共 API / 默认值：
依赖及应用配合：
升级冲突点与迁移/移除条件：
已执行验证（平台、构建、命令、结果）：
未验证项 / 已知限制：
上游 issue / PR / 替代版本：
```

## 4. 升级 Godot 的执行清单

- [ ] 冻结并提交本地定制源码、第三方源码和清单，记录当前 commit、编译器/SDK、构建参数及制品散列。**当前有大量未跟踪新模块，`git diff` 或 `format-patch` 不会自动保存它们。**不能只备份 tracked diff 就开始迁移。
- [ ] 在独立分支/工作区对齐目标 Godot commit，核实官方来源；记录旧/新基线。不要把当前包含 OSG 的 HEAD 当成上游原版。
- [ ] 逐项判断“保留 / 重写 / 上游已覆盖 / 暂停”，填写下方升级记录。一般先迁移通用框架，再迁移 OSG / 3D Tiles；Draco 与 glTF 一起处理；DPI 独立处理；USD 保持关闭。
- [ ] 检查所有删除/迁移文件，避免同时保留旧 OSG 节点与新通用节点；同步 ClassDB、依赖、SCsub、XML、许可证。
- [ ] 使用目标源码重新编译带测试的 editor 和实际交付的 template；相同版本字符串不代表二进制内容一致。当前预置 editor 曾落后于 release 模板，不能用它替代发布运行时验收。
- [ ] 按启用功能运行原生测试、关闭可选模块的编译检查，再运行打包应用的无头/图形/DPI 回归。检查窗口开关关闭路径与默认值；必要时补做此前未测的平台/缩放组合。
- [ ] 用新模板重新导出 EXE/PCK，核查 `build-manifest.json` 和应用源码散列；发布文档说明自定义设置、依赖与剩余限制。
- [ ] 审核完成后更新 JSON 的 `comparison_base`、盘点日期和 HEAD，将旧基线/测试结果留在升级记录中；不要仅为消除检查报错而把基线改成定制 HEAD。

原生验证命令示例（使用**刚编译、启用了 tests 和相应模块**的 editor；本次仅登记命令，不声称已重跑）：

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --headless --test '--test-case=*TiledMesh3D*,*OSGDB*,*3DTiles*,*Draco*,*KHR_draco*' --no-version
```

打包应用验证入口：

```powershell
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Preset 'Windows Validation'
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Pack -Engine ./projects/DigitalViewer/build/validation/DigitalViewer.console.exe
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Pack -Engine ./projects/DigitalViewer/build/validation/DigitalViewer.console.exe -Graphics -Capture
.\projects\DigitalViewer\tools\Build-Desktop.ps1
```

上述脚本负责导出与应用验证，**不编译 Godot C++**。需先完成新源码的 editor/template 构建。最近一轮应用证据为 211 项无头检查、362 项图形检查；范围与限制见应用实现状态，不能推广为全部 C++ 模块或全部平台通过。脚本面板关闭、确认复用、结构树交互/布局及透明覆盖层鼠标穿透修复仅涉及 DigitalViewer 应用代码；B3DM 与 OSG 修复分别涉及 GD-3DT-001、GD-OSG-001。

## 5. 升级记录

| 日期 | 起点 → 目标 | 条目处理 | 验证与产物 |
| --- | --- | --- | --- |
| 2026-10-27 | 本地 `f6ab5db28b9…` → `9c605b09a54…` + 工作区 | 首次建账；保留 DPI 源码方案；登记既有模块；并未升级引擎 | 路径覆盖检查；功能验证引用已有记录，本次不重建引擎 |
| 2026-10-28 | 同一 HEAD + 工作区；未升级引擎 | GD-3DT-001：兼容 ContextCapture B3DM 缺少末尾 8 字节填充 | 重建 release 模板；应用正反例、200 项无头及 351 项图形检查通过；武当山原始数据加载失败数降为 0 |
| 2026-10-28 | 同一 HEAD + 工作区；未升级引擎 | GD-OSG-001：传统 OSG wrapper 注册、中文分词与 Windows ANSI 路径、延迟资源读取 | 重建 release 模板；205 项无头及 356 项图形检查通过；武当山 OSG 导航失败内容 0，源 JPG 缺失仍有警告；新增 wrappers 来源/许可已记录 |
| 2026-10-28 | 同一 HEAD + 工作区；未升级引擎 | GD-OSG-001：合法空 Group/Geode 瓦片返回空文档，不再误报损坏 | 重建 release 模板；211 项无头及 362 项图形检查通过；桃园原空瓦片和所属入口验证通过；未改第三方库 |

下一次升级在这里追加目标完整 commit、每项保留/替代决定、对应提交和新测试证据。
