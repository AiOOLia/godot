# BREP Windows 预览版

CAD 和 ALL 均已支持本地 NDS `.brep`、独立的 BREP 几何 `.json`、包含 `brep.file` 与 `root` 的装配 JSON，以及 ZIP 中的这些入口。几何 JSON 使用 brepexpoter 的 `version/unitToMillimeters/vertices/edges/wires/faces/shells/solids` 格式，可直接加载属性面板复制出的原始或 OCC 面 JSON。装配 `brep.file` 可引用二进制 BREP 或几何 JSON。JSON 按内容识别，3D Tiles JSON 仍仅允许 ALL；混合格式标记、缺失字段和错误导入器均会被拒绝。装配引用的几何不重复列入 ZIP 模型候选。

BREP 二进制、几何 JSON 和 JSON 对象树均由现有 **BRepCore.dll** 解析，几何/拓扑重建由现有 **BRepToTopoDS.dll** 执行。几何 JSON 调用 `BRepFile::parse(const char*, ParsedBRepData&, ...)`，支持 UTF-8 BOM；字段和拓扑索引由 DLL 校验。同名 `.geom` 存在时优先通过 `BRepFile::read(path, tessellations, error)` 读取导出的顶点、法线、三角形和线段，直接向 Godot 提交网格；没有调用 OCC 重建或离散。离散数据缺失、损坏或拓扑覆盖不完整时，记录原因并回退现有 OCC 流程。没有重写解析器，也没有重编 NDS 或 OCCT DLL。

## 使用与构建

解压完整包后运行 `DigitalViewer.exe`，保留相邻 PCK、DLL 和 `BREP-LICENSES/`。开发工程的 DLL 位于 `addons/brep/bin/`，Godot 导出后会将它们放在 EXE 同目录。添加本地 `.brep`、几何/装配 `.json`，或将文件拖入窗口。装配 JSON 引用的二进制 BREP 或几何 JSON 必须保留相对目录关系；当前允许同目录或子目录中的相对引用。缺失依赖、越界路径、未知或冲突 JSON 都会显示明确错误。

开发构建使用 MSVC x64 C++20 Release `/MD`、`_ITERATOR_DEBUG_LEVEL=0`：

```powershell
.\projects\DigitalViewer\tools\Build-BRep.ps1 `
  -BRepSdkPath E:\WorkSpace\NDS\GitLab\ndsbrepexpoter `
  -GodotCppPath E:\WorkSpace\Gitea\DigitalViewer\ThirdPart\godot-cpp
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Profile CAD
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Profile ALL
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Profile CAD -BRepOnly
```

`Build-Desktop.ps1 -BuildBRep` 可先构建扩展。`Build-BRep.ps1` 校验固定头文件、DLL、导入库 SHA256 后，只编译扩展并复制依赖。当前 SDK 对应 NDS 提交 `ecf121e`，使用用户提供的新版 BRepCore/BRepToTopoDS DLL（包含 `.geom` 读取及两个 `separateFaceBrepsToJson` 接口）；OCCT DLL 来自 `ThirdPart/OCCT/windows/bin/`。固定清单位于 `addons/brep/native/sdk/manifest.json`，其中 `runtime_files` 记录每个运行库的源路径。更新 SDK 时必须成套更新头文件、导入库、DLL 与清单。

godot-cpp 使用已有 Godot 4.1.4 单精度 Release 库，已在当前 Godot 4.7.3 验证。扩展面向单精度 Windows x64 模板，不适用于 double 引擎或其他平台。Godot 主程序保持原有 CRT 配置。

## 已实现

- 导出的 `.geom` 与对应 `.brep` 或独立几何 `.json` 同名、同目录；从装配 JSON 打开时，按 `brep.file` 的位置查找 `.geom`。ZIP 保留三文件相对目录即可。装配通过 `faceTessIds` / `edgeTessIds` 选取三角形/线段；裸 BREP 按源拓扑分组。分组索引直接对应原始面，支持面拾取、树选择和实例共享；线段不阻挡面拾取。仅在导出未提供法线时从三角形缓冲计算法线，保留导出的顶点与三角形。状态统计 `geometry_source` 为 `geom` 或 `occ`，`occ_reconstructed=false` 表示模型加载跳过重建。
- 后台解析、重建、三角化和阶段状态；删除/重载取消及 generation 检查；退出等待导入线程结束。
- 装配节点、名称、persistentId、局部矩阵及实例编号；相同形体只三角化一次；跨视口共享网格、独立显隐与变换。
- BREP 结构树从源数据根节点开始，直接列在“三维数据”下，不显示文件名行和内部 `layer_*` 容器；源装配层级保持原样。源根节点行承接整份数据的显隐复选框和数据属性/操作，文件名保留在悬浮提示中。裸几何有多个根形体时并列显示，加载中或失败时保留文件行以查看状态与错误。
- OCC 回退路径的装配 `repBodyIndex` 按源 solid/shell 索引查询修复结果；支持一个源形体拆成多个结果实体或壳，以及源实体修复后只剩开壳。每个实例包含全部结果，保留源形体身份并共享结果网格。
- 每个 body 一个网格 surface；离散路径分别创建面和线段网格；孔洞、反向面和 TopLoc 变换；保留面/壳/实体及三角形区间。
- 单击 BREP 网格高亮命中的完整面（保留孔洞），同步展开并定位结构树中的对应形体，属性显示面索引和持久 ID。选择树中的形体、零件、装配或图层时高亮全部对应网格，支持 Ctrl 多选；点击空白清除。高亮按视口和实例独立，跟随镜像/非均匀变换、显隐和透明度，保留深度遮挡；不修改共享网格、材质、拾取缓存或模型包围盒。`hit.brep`、`object_persistent_id` 与 `occurrence` 保持可用。
- CAD/ALL 导出携带扩展、现有 CAD/OCCT DLL、MSVC 动态运行库及 SHA256 清单，启动检查扩展注册。
- 选中面后，属性面板顶部同时显示“原始 BREP JSON”和“OCC 重构后 BREP JSON”，各自提供只读滚动预览与“复制 JSON”。二进制源的原始数据直接调用 `ModelTransfer::BRepFile::separateFaceBrepsToJson`，JSON 源按下述方式调用 SDK 解析器和 JsonWriter；重构结果直接调用 `BRepToTopoDS::separateFaceBrepsToJson`。保留 SDK 返回的完整字符串、格式、字段、double 精度与单位，不再由 DigitalViewer 手动组织曲面/拓扑 JSON。两个 JSON 均采用 brepexpoter 的 `version/unitToMillimeters/vertices/edges/wires/faces/shells/solids` 结构；装配矩阵、显示缩放与 Up 轴不改写定义。
- 修复拆分后的面按持久 ID 关联完整源面，不将结果面索引用作源面索引；不同源壳复用 ID 时结合源壳关联消歧。原始导出 API 按独立壳后实体枚举源面，扩展将解析索引转换为该枚举索引。无法唯一关联时，明确提示并在原始 JSON 导出全部候选面，重构 JSON 始终只导出当前结果面；一侧导出失败不会隐藏另一侧。单击其他面更新，树节点或空白选择清除两块内容。
- 离散路径选中面后才从源快照提取对应面，调用 DLL 按需重建并输出 OCC JSON，不调用离散器；如果源面修复后拆成多个面，OCC JSON 包含这些结果并在属性中说明。重建失败仍显示原始 JSON，不影响已有网格。OCC 回退路径保留原有结果面语义。
- 原始二进制或 JSON 快照及 OCC 转换器（如已创建）随共享网格保留，ZIP 临时文件或外部文件变化不会改变已加载模型的导出数据。几何 JSON 输入选面时，扩展从 DLL 解析对象选取对应面和依赖拓扑并重排引用索引，曲面/曲线参数保持原样，最终由 DLL 的 `JsonWriter::write` 输出原始面 JSON。面 JSON 在后台按需生成，单个工作线程只发布最新选择；文档内缓存最多 16 个结果。超长 JSON 预览限制为 65536 字符并提示截断，复制包含全文，退出等待当前导出线程结束。

## 当前限制

- 支持 NDSBREP 二进制，不支持 OCCT 文本 BREP，不新增 STEP/IGES/DWG 导入。
- 默认 CAD 材质，未恢复源材质。支持格式 1.3 的 `unitToMillimeters`（每源坐标单位的毫米数）；旧文件或字段为 0 时显示未知。几何和装配平移保留源坐标，不自动换算；脚本/API 可传 `unit_scale`，在根节点统一缩放一次。CAD/ALL 添加数据界面的 Up 轴只有 `Y-UP`、`Z-UP` 两项，每次打开默认选中 `Y-UP`。BREP 默认不附加旋转，选择 `Z-UP` 时在根节点绕 X 轴转 −90° 一次。文件、装配 JSON 和 ZIP 使用相同设置，未新增单位/精度面板。
- 网格主线程提交尚未按帧分批，大模型上传时仍可能短暂阻塞。单个二进制 BREP / GEOM 最大 256 MiB、几何/装配 JSON 最大 16 MiB、唯一网格最多 200 万三角形；GEOM 最多 600 万显示顶点、400 万线段，装配最多 10 万节点和 128 层；这些限制不是重建阶段的严格内存上限。
- OCC 回退路径可传 `deflection_scale`（0.1～10）控制三角化；GEOM 路径保持导出的离散精度。OCCT 单次调用期间不能立即中断，取消在阶段和面之间生效；内核导入串行执行。
- 超大坐标浮动原点、远程 BREP/装配依赖下载、OCC 路径的纯线体显示和测量待后续版本（GEOM 线段已支持）。远程 JSON 仅探测格式，识别为 BREP 后提示下载到本地打开。
- ZIP 沿用现有 64 MiB 单项、256 MiB 总展开及 4096 项限制。

## 验证

`tests/brep_smoke.gd` 使用由 BRepCore DLL 写出的带孔平面及拓扑拆分夹具；生成源为 `addons/brep/native/make_fixture.cpp`，`Build-BRep.ps1 -GenerateTestFixtures` 可重新生成新版夹具。检查面积 84、射线穿孔、源面 ID、镜像非均匀实例、中文目录、重复实例、双视口共享、重载、移除期间取消、JSON 分派与 ZIP 依赖。新增 1.3 英寸单位、旧文件未知单位、显式显示缩放、源壳拆分、开壳源实体、拆分后的共享与源 ID、源索引越界验证。已增加面点击与树节点高亮回归：同一 body 中两个独立面、命中面范围、实例隔离、树多选与刷新、变换、显隐、透明度、删除/重载清理和真实装配面选择；图形模式还验证高亮像素与前景遮挡。

新版真实装配 `output/热压机总装图.STEP/model.json` 验证得到 2834 个面、42 个独立 body、81326 个三角形、0 个缺失面，格式 1.3、每源坐标单位 1 mm。开发机测试尚不等于干净机器验收；读取器声明支持 1.0～1.3，实际回归覆盖旧夹具与新版 1.3 数据。

2026-11-03 导出模板验证：CAD/ALL 的 BREP 专项各 65 项通过，配置检查分别 34/25 项通过，CAD 热压机装配图形渲染通过。两个 ZIP 在新的解压目录中校验了全部 15 个运行库及 EXE/PCK 散列，并在精简 PATH 下通过模板启动检查。ALL 全量回归的此前基线有 5 项业务断言失败：树刷新选择保持、停止脚本后的选择回退、OSG 三角形拾取、HTTP 无效会话和 MCP 会话恢复；汇总断言随之失败。高亮功能完成后已用 ALL 导出模板重跑全量套件（211 项），仍为上述 5 项业务断言与汇总失败；没有新增业务失败，不能将本版描述为全量测试通过。

CNC 装配崩溃修复（2026-11-03）：转储确认旧 `addons/brep/bin/DigitalViewerBRep.dll` 与 EXE 相邻的新版 BRepCore 混用，故障为访问冲突。构建现会备份移走旧目录，启动会拒绝残留的旧布局。使用转储对应的旧适配 DLL 验证了安全拒绝，恢复正常布局后启动通过。`output/CNC3轴雕刻机.STEP/model.json` 在修复后的 CAD/ALL 导出模板各通过 65 项 BREP 检查，结果为 62 个形体、2663 个面、109634 个三角形、0 缺失面；CAD 图形渲染通过。没有修改模型数据或重编 CAD/OCCT DLL。

面/树高亮交付验证：CAD 导出包的 CNC 专项 100 项通过（含真实窗口 Ctrl 多选/取消、选中面与邻面像素对比、前景遮挡），ALL 导出包无图形专项 93 项通过；两配置检查分别 34/25 项通过。兼容渲染器也通过图形高亮验证；ZIP 中的 EXE/PCK、15 个 DLL 及运行脚本散列均与当前构建一致。

2026-11-04 面定义验证：增加由原 BRepCore DLL 生成的 NURBS、零 ID、同壳重复 ID、跨壳重复 ID 夹具；检查双精度 JSON、边界环/边/顶点参数、旧单位未知、修复拆分关联、歧义回退、属性切换和复制。CNC 的 2663 个面全部唯一关联源定义，覆盖 cylinder、plane、cone、torus、nurbs、sphere 六类曲面。CAD 导出验证包图形专项 152 项、ALL 导出验证包无图形专项 144 项通过，包含 CNC 数据、单击高亮、实际剪贴板内容（Windows CRLF 标准化）、双 Up 轴回归。CAD/ALL 桌面 ZIP 中的 EXE、PCK、15 个 DLL 与清单散列全部一致，运行脚本散列与当前源码一致。

2026-11-04 新版 DLL 双 JSON 接口验证：使用用户重新生成的 `efa4f03` BRepCore/BRepToTopoDS，DigitalViewer 只重编适配扩展。CAD 导出验证包 CNC 图形专项 186 项、ALL 导出验证包 CNC 无图形专项 175 项通过。覆盖两个 SDK JSON 的直接显示与复制、超长预览与完整剪贴板、原始双区域和 OCC 单结果面的差异、NURBS 双精度、单位、零/重复 ID、快速切面仅发布最新结果、文件删除后仍使用加载快照、树选择清除内容与两项 Up 轴。CAD/ALL 桌面 ZIP 的 EXE/PCK、15 个 DLL、清单及运行脚本散列一致。

2026-11-04 独立 BREP JSON 输入验证：CAD 导出验证包 CNC 图形专项 232 项、ALL 导出验证包 CNC 无图形专项 221 项通过。覆盖属性面板原始/OCC JSON 保存后加载、UTF-8 BOM、NURBS 双精度与单位、带孔拓扑、JSON 选面双定义、重新加载、独立 JSON ZIP、装配引用几何 JSON、正反斜杠路径候选去重、无效拓扑报错及 Y-UP/Z-UP。继续调用既有 BRepCore JSON 解析器、BRepToTopoDS 和 JsonWriter，仅重编适配扩展。图形回归对 Windows 剪贴板临时占用作有限重试，最终完整复制检查通过。CAD/ALL 桌面 ZIP 的 EXE、PCK、15 个 DLL、清单及 36 个运行文件散列均与当前构建一致；本次未重跑 ALL 全业务套件，既有全量限制仍见上文。

2026-11-06 离散几何验证：适配 `ecf121e` SDK，读取与 BREP 同名的 `.geom`，只重编 DigitalViewer 适配扩展。CAD 导出验证包图形专项 281 项、ALL 导出验证包无图形专项 270 项通过。CAD 实测 `GSX-0.6-DN200.step_tess/model.json` 为 155 个面、15552 个三角形、41760 条线段、0 缺失面，`geometry_source=geom`、`occ_reconstructed=false`；ALL 实测旧 CNC 装配为 2663 个面、109634 个三角形、0 缺失面，`geometry_source=occ`。新增夹具由 BRepCore DLL 直接写出，覆盖无提供法线的缓冲生成、孔洞、面索引、原始/OCC 面 JSON、实例共享和矩阵、二进制与独立 JSON 直读、ZIP、重载、缺失 tessellation 引用、截断 GEOM 及不完整面覆盖回退。CAD/ALL 桌面 ZIP 的 EXE/PCK、15 个 DLL、SDK 清单及运行脚本散列与当前构建一致。本轮未重跑 ALL 全业务套件。

2026-11-06 源结构树根节点验证：省去文件名行和运行时 layer 容器，源根节点直接位于“三维数据”下；选中源根节点显示真实节点属性并保留数据操作。CAD 源码及 OpenGL 导出图形专项各 361 项、ALL 导出无头 BREP 专项 333 项通过。覆盖裸 BREP、装配 JSON、GEOM、ZIP、真实 CNC、多根形体并列、源节点身份和层级、根节点/分组显隐、折叠和刷新保持、根属性的重载/移除操作、选面定位与高亮。原数据属性面板代码原样提取复用，未改变已有操作实现。本轮未重跑 ALL 全业务套件。
