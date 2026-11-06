# Godot 源码修改清单与升级记录

更新：2026-11-06。本文只记录对 **Godot 原有模块和代码**的修改，以及这些修改直接对应的测试、API 文档和升级要求。

独立新增模块、第三方库及其内部修复、应用代码、打包配置不列为源码修改条目；清单和检查工具本身也不列为引擎补丁。新增模块若改动了 Godot 原有代码，只登记原有代码中的接入点。相关外部能力只在说明补丁依赖时简要引用。

配套文件：[路径登记表](GODOT_SOURCE_CHANGES.json)、[只读检查工具](misc/scripts/check_local_engine_changes.py)。

## 1. 基线与记录范围

| 项目 | 盘点结果 |
| --- | --- |
| 引擎版本 | `version.py`：`4.7.3-rc`；最近构建为 `4.7.3.rc.custom_build.a5046bf84`，运行时版本字符串不能代替制品散列 |
| 本地比较基线 | `f6ab5db28b9d989a3ae710d57410a123cf0e4b23`，2026-08-20；用于区分原有 Godot 代码与后续新增内容 |
| 当前 HEAD | `a5046bf849bf62d4bf69a3fa1066c380174ca056`，2026-11-06，`新增cad模式` |
| 已提交修改 | Windows DPI 补丁、原有 glTF 模块的可选压缩数据接入 |
| 工作区修改 | 材质硬件 depth bias 及其文档；尚未提交 |
| 来源边界 | 基线和 HEAD 从本地 Git 历史读取；没有独立核验其与官方远端 / tag 的对应关系 |

登记原有代码的精确文件路径，不使用整个 `modules/`、`scene/`、`servers/` 或 `drivers/` 目录覆盖未知修改。原有模块内部为补丁添加的关联测试文件一并登记。JSON 的 `excluded` 仅声明检查范围，不构成修改条目；不改动比较基线来隐藏遗漏。

## 2. 修改索引

| 编号 | 修改 | 状态 | 升级关注点 |
| --- | --- | --- | --- |
| [GD-DPI-001](#gd-dpi-001) | Windows 每显示器 DPI、跨屏窗口与事件 | 已提交；项目开关默认关闭；Windows 已验证 | 设置注册、DisplayServer、原生窗口矩形、Window 信号 |
| [GD-DRACO-001](#gd-draco-001) | 原有 glTF 模块接入可选压缩图元解码 | 已提交；只登记 `modules/gltf/` 中的 4 个文件 | 可选依赖、accessor 类型与索引校验、关闭依赖时的行为 |
| [GD-DEPTH-BIAS-001](#gd-depth-bias-001) | 材质硬件深度偏移 | 未提交；默认关闭；Windows 三种渲染器已验证 | 延迟 RID、材质存储、管线缓存、深度预绘制、GL 状态恢复 |

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

**提交 / 上游状态：**DPI 补丁已随本地 `16501e75df`（`多显示器 dpi 适配`）提交，未记录上游 PR；尚未认定任何上游版本已覆盖它。

## GD-DRACO-001

**目的：**修改 Godot 原有 `GLTFDocument` 导入流程，支持可选的 `KHR_draco_mesh_compression` 图元解码。本条保留既有编号，只记录原有 glTF 模块的接入改动，不登记独立解码模块或第三方库实现。

| 原有模块中的文件 | 修改内容 |
| --- | --- |
| [modules/gltf/config.py](modules/gltf/config.py) | 为 glTF 声明可选解码依赖，保持可在依赖关闭时编译 |
| [modules/gltf/gltf_document.cpp](modules/gltf/gltf_document.cpp) | `_parse_meshes` 在模块开关下调用解码接口，按属性 unique ID 映射到 glTF 语义；校验 accessor 的类型、分量数、normalized、count 和索引，转换为原有 accessor 流程；`get_supported_gltf_extensions_hashset` 仅在依赖开启时声明扩展支持 |
| [modules/gltf/tests/test_gltf_draco.h](modules/gltf/tests/test_gltf_draco.h) | 原有模块内新增的关联测试，通过压缩 GLB fixture 验证 GLTFDocument 接入路径 |
| [modules/gltf/README.md](modules/gltf/README.md) | 说明原有 glTF 导入器在依赖启用时支持该压缩扩展 |

**兼容行为：**开启 `MODULE_DRACO_ENABLED` 时接通解码；关闭时明确拒绝相关压缩内容，普通未压缩 glTF 保持原导入路径。

**升级处理：**优先检查目标 Godot 是否已提供等价扩展支持。迁移时核对 accessor 编解码 API、属性类型与 unique ID 映射、索引和限额校验，并覆盖 `module_draco_enabled=yes/no` 两种构建。若由上游替代，保留等价的开启 / 关闭行为和压缩图元回归后记录替代版本。

**验证边界：**上述 glTF 测试已存在；本轮清单整理未重建或运行 C++ 原生测试，不以普通 glTF 加载成功代替压缩格式验收。

## GD-DEPTH-BIAS-001

**日期 / 状态：**2026-11-06；本地保留的核心补丁，基于 `a5046bf849bf62d4bf69a3fa1066c380174ca056` 后的工作区，尚未提交，未记录上游 issue / PR。本次没有升级 Godot。

**触发问题 → 改后行为：**CAD 离散面和独立边线共面时，深度竞争使边线断续。RenderingDevice 已有硬件 depth bias 光栅化字段，但场景材质没有对应设置；OpenGL 的 `glPolygonOffset` 只影响填充三角面，不能直接偏移独立线段。本补丁从材质接通硬件管线参数，供应用将面深度稍微推远，边线保留原始坐标和正常深度测试。

**公共 API 与默认值：**

- `Material.depth_bias_enabled = false`、`depth_bias_constant_factor = 0.0`、`depth_bias_slope_factor = 0.0`。提供 setter/getter，支持 Inspector、GDScript、资源保存和复制；面向 `BaseMaterial3D` 及 spatial `ShaderMaterial`，其他材质类型不显示 Inspector 选项。
- `RenderingServer.material_set_depth_bias(material, enabled, constant_factor, slope_factor)` 同步更新后端存储，拒绝非有限参数。材质 RID 延迟创建、运行时切换及 shader data 重建均保留设置。
- 正值将填充三角面推远，负值拉近；各渲染器按 reverse Z 转换符号。默认关闭保持原材质行为。相机颜色、深度预绘制和运动矢量使用一致设置；阴影贴图、材质烘焙、SDF 生成、点、独立线段和 wireframe 不应用该偏移。

**引擎文件：**JSON 按下表逐文件登记，共 24 个精确路径；没有扩大目录覆盖范围。

| 文件 | 关键符号 / 修改及迁移要求 |
| --- | --- |
| `scene/resources/material.h`、`scene/resources/material.cpp` | `Material::_set_material`、`_update_depth_bias`、`_bind_methods`、`_validate_property`；公共参数、默认值、Inspector、保存、有限值校验和惰性 RID 初始化时同步 |
| `servers/rendering/rendering_server.h`、`rendering_server.cpp`、`rendering_server_default.h`（同目录） | `material_set_depth_bias` 抽象接口、ClassDB 绑定及 `FUNC4` 命令队列转发；脚本与渲染线程契约需一起迁移 |
| `servers/rendering/storage/material_storage.h`、`servers/rendering/dummy/storage/material_storage.h` | `RendererMaterialStorage::DepthBias` 与参数 hash、存储抽象及 dummy 空实现；无头构建仍须可用 |
| `servers/rendering/renderer_rd/storage_rd/material_storage.h`、`material_storage.cpp`（同目录） | `Material` / `MaterialData` 保存 bias，shader data 创建及重建时复制，setter 发出材质依赖变更 |
| `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.h`、`render_forward_clustered.cpp`、`scene_shader_forward_clustered.h`、`scene_shader_forward_clustered.cpp`（同目录） | Forward+ 的 `SurfacePipelineData`、`PipelineKey::hash`、`_create_pipeline`、绘制和预编译；bias 纳入缓存与 ubershader，深度预绘制即使用共享 depth shader 也取原始材质参数，阴影与烘焙分支排除偏移 |
| `servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.h`、`render_forward_mobile.cpp`、`scene_shader_forward_mobile.h`、`scene_shader_forward_mobile.cpp`（同目录） | Mobile 同步接通材质参数、管线 key/hash、预编译、ubershader 和 camera / shadow pass 区分 |
| `drivers/gles3/storage/material_storage.h`、`material_storage.cpp`（同目录） | GLES3 材质 / material data 保存与同步，shader data 重建后恢复参数并通知依赖 |
| `drivers/gles3/rasterizer_scene_gles3.h`、`rasterizer_scene_gles3.cpp`（同目录） | `SceneState::set_gl_depth_bias`、`reset_gl_state`、`_render_list_template`；缓存 `glPolygonOffset` 参数，控制 `GL_POLYGON_OFFSET_FILL`，阴影禁用，每个 render list 结束恢复，避免泄漏到后续材质、天空、画布和 blit |
| `drivers/d3d12/rendering_device_driver_d3d12.cpp` | `render_pipeline_create` 补回 `else`，修正原先设置 bias 后立即无条件清零的问题；本次 D3D12 未编入交付程序，也未实机验收 |
| `doc/classes/Material.xml`、`doc/classes/RenderingServer.xml` | 公共属性、方法、符号方向、影响范围及精度限制说明 |

**应用配合：**[brep_scene.gd](projects/DigitalViewer/scripts/desktop/brep_scene.gd) 在导入结果含有效边线时，仅给三角面材质开启 `constant_factor = 0.5`、`slope_factor = 0.5`；没有边线时保持关闭。边线仍使用 `ArrayMesh / PRIMITIVE_LINES + StandardMaterial3D`。桌面视口在 `view_scene.gd` 创建相机时统一设置 `near = 0.2`，已移除按导航距离、图层类型和显隐切换近裁剪面的逻辑；远裁剪策略保持原值。未修改网格坐标或 BREP/OCC 解析。专项 [material_depth_bias.gd](projects/DigitalViewer/tests/material_depth_bias.gd) 验证硬件偏移，`brep_smoke.gd` 检查实际相机的固定近裁剪面；`Test-Desktop.ps1 -RenderingMethod` 可选择三种渲染器。应用文件归入 `projects/` 排除项，详细记录见 [brep-preview.md](projects/DigitalViewer/docs/brep-preview.md#面与边线的硬件深度偏移)。

**已执行验证与制品：**Windows x86_64、NVIDIA RTX 3080 Laptop，OpenGL 3.3 Compatibility 与 Vulkan 1.3 Forward+ / Mobile；重建 dev editor、CAD/ALL `template_release`，重新导出正式包和验证包。

- 三种渲染器在开启 / 关闭深度预绘制时分别通过 30 项专项检查；覆盖资源往返、延迟 RID、动态开关、共享 shader 材质隔离、RenderingServer 更新、shader data 重建、独立线段不受影响及阴影不变。透视 / 正交、1 / 10000 倍尺度的共面线连续率为 100%，遮挡样本无透线，90 个阴影采样不变。合成模型面宽 8，遮挡间隔 0.05；这不保证任意更薄结构。
- CAD 源码及导出 OpenGL、CAD 导出 Forward+、ALL 源码及导出 Forward+ 的真实 CNC BREP 回归各通过 390 项；数据仍使用 `.geom`，232,383 个三角形、281,013 条线，`missing_faces = 0`、`occ_reconstructed = false`。CAD/ALL 导出配置分别通过 68/59 项；EXE/PCK 与应用脚本散列已核对。
- `git diff --check`、C++ clang-format 检查及 Material / RenderingServer XML 文档生成检查通过。本次没有构建或运行 C++ 原生单元测试。执行日志在 `.tmp_tests/depth-bias-*.log`，该临时目录不作为升级移植源。
- CNC 透线修正的动态裁剪版本曾通过三种渲染器各 70 项和 CAD/ALL CNC 各 431 项回归；该动态策略现已按要求移除，统一固定 `near = 0.2`。这些历史结果不能代表当前固定近裁剪面在任意尺度下的遮挡保证。应用修改不新增 Godot 原有源码路径。
- 额外运行的 ALL 完整桌面套件未全部通过，包含属性面板旧断言、OSG 拾取、HTTP 会话和 DPI 测试访问旧 `_menu`；原有未修改编辑器也复现这些失败，不能将专项通过表述为完整业务套件全部通过。

复测导出程序（先使用新引擎和对应模板生成 `Windows Validation` 包）：

```powershell
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Profile CAD -BRepOnly -Graphics -RenderingMethod gl_compatibility -Pack -Engine ./projects/DigitalViewer/build/validation/CAD/DigitalViewer.exe
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Profile ALL -BRepOnly -Graphics -RenderingMethod forward_plus -Pack -Engine ./projects/DigitalViewer/build/validation/ALL/DigitalViewer.exe
# 将 RenderingMethod 改为 mobile 可运行 Mobile 路径；加入 -Dataset <model.json> 可加载真实数据。
```

**升级冲突点与迁移 / 移除条件：**优先检查目标上游是否已有材质级硬件偏移接口；如有，保留 API 默认关闭及符号语义，调整应用调用，验证等价行为后记录替代版本。重点核对 Material RID 初始化、RenderingServer 转发、dummy 与两个 material storage、管线 key/hash / 预编译 / ubershader 的参数完整性、共享 depth shader 的原材质参数、camera 与 shadow/bake pass 区分及 GL 状态恢复。RD 底层字段已存在，未增加 Vulkan / Metal 驱动接口；不要将场景偏移误写成共享 shader 状态，也不要把独立线段当作 OpenGL polygon offset 的作用对象。必须同步重编 editor / 模板并导出 EXE/PCK，不能只更换 PCK。

**未验证与限制：**D3D12 修复仅作源码检查，当前构建 `d3d12=no`；Metal、其他平台 / 显卡及所有 MSAA 组合未实机验证。硬件偏移并非世界距离，受深度格式及投影影响；过大的常量、坡度和掠射角仍可能让极薄结构后的线透出。shader 主动写 `DEPTH` 可能覆盖光栅化深度，面 / 边离散数据不重合也需另行处理。

## 3. 清单维护与检查

每次修改 Godot 原有代码时，同步更新对应条目与 JSON 精确路径，记录公共 API / 默认值、修改锚点、提交、升级冲突点、实际执行的验证和未验证项。应用配合和验收证据可以引用，不将应用文件登记为引擎补丁。

独立新增模块及 `thirdparty/` 使用明确的目录排除规则；原有 `modules/gltf/` 继续逐文件检查。清单、工具、导航文档和许可证登记也列为排除项，避免将维护材料计入引擎代码修改数量。

在仓库根目录执行：

```powershell
& 'D:/Program Files/Python312/python.exe' misc/scripts/check_local_engine_changes.py
& 'D:/Program Files/Python312/python.exe' misc/scripts/check_local_engine_changes.py --json
```

检查工具只读 Git，比较基线到完整工作区的已提交、暂存和未暂存差异，再加入未被忽略的未跟踪文件。退出码 `0` 表示路径均已登记或明确排除，`1` 表示有漏记，`2` 表示配置 / Git 错误；`--base <commit>` 可临时选择比较基线。工具只检查路径归属，不判断修改语义、测试结果或制品一致性；被 Git ignore 的内容不在扫描范围内。

本次清理后，修改条目仅保留 DPI 的 4 个文件、glTF 的 4 个文件及 depth bias 的 24 个文件，共 32 个文件。清理不改变引擎实现和构建产物。

## 4. 升级处理

- [ ] 保存已提交、未提交和未跟踪的修改，记录旧 / 新完整 commit、编译器 / SDK、构建参数及制品散列。
- [ ] 对每项原有代码补丁判断“保留 / 重写 / 上游已覆盖 / 停用”；按符号和行为迁移，不按旧行号贴补丁。
- [ ] 核对公共 API、默认值、线程与资源生命周期、各渲染后端和功能开关关闭路径。
- [ ] 使用目标源码重建 editor 与实际交付的导出模板，运行对应原生 / 应用测试，分别记录通过项和剩余失败。
- [ ] 用新模板重新导出 EXE/PCK，核对制品清单和应用调用；引擎补丁不能仅靠更换 PCK 分发。
- [ ] 更新日期、HEAD、路径登记与升级记录；变更比较基线前保留旧记录，重新运行覆盖检查。

## 5. 记录

| 日期 | 条目 / 范围 | 处理与验证 |
| --- | --- | --- |
| 2026-10-27 | 首次建账；原有代码补丁 | 建立本地比较基线，记录 Windows DPI 与原有 glTF 接入；未升级 Godot |
| 2026-11-06 | GD-DEPTH-BIAS-001 | 补录 24 个引擎文件；重建 dev editor、CAD/ALL release 模板与导出包；三种渲染器 × 开关深度预绘制各 30 项通过，CNC BREP 各 390 项、配置 68/59 项通过；完整桌面套件有旧版可复现失败，D3D12 / Metal 未实机验证 |
| 2026-11-06 | 清单范围清理 | 移除独立新增模块、第三方库及其内部修复记录；glTF 条目收敛为原有模块的接入修改；清单维护文件改为排除项；保留 3 项、32 个引擎补丁文件 |
