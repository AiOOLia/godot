# DigitalViewer v1.0 功能与验收清单

> 文档用途：跟踪实现、测试和发布进度。
>
> 最后校准：2026-10-23（多视口与独立窗口重新基线）
>
> 对应产品范围：[FEATURE\_CHECKLIST.md](FEATURE_CHECKLIST.md)

## 2026-10-28 桌面实现记录

本次新增实际桌面运行时，当前是开发版，**不等于下列全部 v1.0 发布门已通过**。范围、验证证据、未实现能力见 [desktop-v1.md](docs/desktop-v1.md)。既有条目包含旧 AI、USD 与移动端规格，本次不根据旧条目重新启用这些产品能力，也不把 Windows 验证勾为 ALL 验收。

| 验证范围 | 本次状态 |
| --- | --- |
| 空视口启动、无演示图层、无内置 AI | Windows 应用测试通过 |
| 独立 World3D/相机、递归 7 视口、分割隔离、全屏恢复、最后视口保护 | Windows 应用测试通过 |
| 视口独立窗口与停靠 | Windows OpenGL 图形测试通过 |
| 结构树名称/显隐/折叠独立、视口高度适配、类型/状态移至属性 | Windows 图形鼠标回归通过；刷新保持折叠/选中/滚动，分屏和浮窗缩放通过 |
| 双视口加载数据后的鼠标旋转/平移/缩放 | 两个视口分别加载实际 OSG 几何；树透明空白区域穿透、树外操作、浮窗导航及相机隔离通过，树行与滚动条仍可操作 |
| glTF 运行时导入、失败、取消、真实三角形拾取 | Windows 应用测试通过 |
| 显式 3D Tiles root 与内容 Resident | 定制引擎应用集成测试通过 |
| B3DM/I3DM/PNTS/CMPT 驻留、PNTS 点编号/Batch ID 拾取 | 当前 release 模板应用测试通过；预置旧编辑器不支持 PNTS/I3DM |
| ContextCapture B3DM 末尾填充兼容 | 新 release 引擎正例及七种损坏文件拒绝通过；武当山本地数据只读验证失败数从 129 降至 0，未穷举全部瓦片 |
| OSG2 ASCII 实际加载、几何驻留与拾取 | 应用测试通过；沿用现有解析库，没有下载/替换库 |
| 传统 OSG 文本、中文路径、纹理与分页 | DotOsgWrapper 注册及非 ASCII 分词修复；UTF-8/Windows 本地编码测试通过，武当山 `.osg` 旋转/缩放后失败内容 0；源数据缺少最低 LOD 的 JPG，`.osgb` 内嵌纹理正常 |
| 空 OSGB 瓦片及 LOD 完成 | 桃园合法空 Group 不再误报损坏；空子瓦片替换、兄弟保留、拉远恢复及截断/不支持图元拒绝回归通过；原文件及所属入口实测失败数 0 |
| 远程 glTF/GLB 外部 buffer/纹理、相对路径、重定向、重试、同源 Header | 本机 HTTP 集成测试通过；真实 TLS/凭据服务未验收 |
| 远程共享无重复下载、删除/重载取消、代次缓存回收 | 应用测试通过 |
| 模型重载保留独立变换/透明度/显隐，原生瓦片重载保持隐藏 | 应用测试通过 |
| 椭球/glTF 共享几何与独立显隐/透明度，最后引用释放 | Windows 应用测试通过；原生瓦片共享未完成 |
| ZIP 预检、模型导入、路径穿越/超限拒绝、最后引用清理 | Windows 应用测试通过 |
| 添加数据新建/导入脚本、结构树实例/内容、多会话同时运行、独立草稿/日志/停止/移除 | Windows 应用测试通过；显式视口、信任门、静态初始化防误执行、跨视口引用及全局重置通过 |
| MCP 初始化/会话/7 工具、权限、Origin 拒绝、持久令牌/导入/重置 | loopback HTTP 与实际 Codex 客户端加载/只读调用通过；关闭只撤销会话，手动重置撤销旧令牌 |
| WGS84/ECEF/ENU 数学 | 数学往返测试通过；不代表共享地理世界与地形流送完成 |
| 脚本分离/停靠/关闭面板和浮窗不中断；已确认原样代码直接重跑，修改后重新确认 | Windows release 图形回归覆盖实例、场景内容、持续回调与文档确认隔离 |
| Windows DPI 与跨屏 | 保持字号；150%/100% 双屏实测主窗口、视口/属性/脚本浮窗、菜单及弹窗，逻辑尺寸、最大化和物理坐标拾取通过 |
| 桌面导出 | Windows release 模板和 EXE/PCK 已生成；同一验证包通过 211 项无头检查、362 项图形检查，含脚本生命周期、结构树、双视口鼠标导航、B3DM 兼容与损坏拒绝、传统 OSG 路径/纹理/分页及空瓦片 LOD；Linux/macOS 未实机验证 |

完整发布仍缺少影像/WMTS/DEM/GeoTIFF、统一地理世界与浮动原点、原生瓦片共享、完整国际化、性能和全平台构建验收。保留这些发布门为未完成。

## 使用说明

- `[ ]`：尚未完成，或尚未完成验收。
- `[x]`：实现完成且满足该条验收条件。
- 本文默认所有条目都需要实际验证后才能勾选；已有引擎代码不自动视为完成。
- `[基础已有]` 表示仓库中已有相关实现，但仍需要集成、补全或回归。
- 优先级：`P0` 为 v1.0 发布阻塞项，`P1` 为 v1.0 正常功能，`P2` 为调试与体验增强。
- 平台：`ALL`、`DESKTOP`、`WIN`、`LINUX`、`MAC`、`ANDROID`、`IOS`、`WEB`。

## 0. 项目骨架与工程配置

- [ ] `DV-FUNC-PROJECT-001` `P0` `ALL` 创建 `project.godot`，名称和版本显示为 DigitalViewer 1.0.0；应用可以进入主场景。
- [ ] `DV-FUNC-PROJECT-002` `P0` `ALL` 建立主场景、应用状态、图层服务、输入路由和错误总线。
- [ ] `DV-FUNC-PROJECT-003` `P0` `ALL` 建立目录约定：`scenes/`、`scripts/`、`ui/`、`themes/`、`i18n/`、`tests/`、`docs/`、`assets/`。
- [ ] `DV-FUNC-PROJECT-004` `P0` `ALL` 配置 Compatibility 渲染器和六端通用项目设置。
- [ ] `DV-FUNC-PROJECT-005` `P1` `ALL` 建立统一的设置资源；只持久化语言、非敏感显示偏好等允许项。
- [ ] `DV-FUNC-PROJECT-006` `P0` `ALL` 启动时不恢复图层、路径、URL、Token、配准或视点。
- [ ] `DV-FUNC-PROJECT-007` `P0` `ALL` 应用退出时依次取消任务、释放图层、清理临时文件并正常退出。
- [ ] `DV-FUNC-PROJECT-008` `P1` `ALL` 建立开发、测试、发布三种日志级别和脱敏过滤器。

## 1. 定制引擎构建

- [ ] `DV-FUNC-ENGINE-001` `P0` `WIN` 构建带测试的 4.7.3-rc 定制编辑器并记录可复现命令。
- [ ] `DV-FUNC-ENGINE-002` `P0` `WIN` 构建匹配提交的 debug/release 导出模板。
- [ ] `DV-FUNC-ENGINE-003` `P0` `LINUX` 构建匹配提交的 debug/release 导出模板。
- [ ] `DV-FUNC-ENGINE-004` `P0` `MAC` 构建 Universal debug/release 导出模板。
- [ ] `DV-FUNC-ENGINE-005` `P0` `ANDROID` 构建 arm64-v8a debug/release 导出模板。
- [ ] `DV-FUNC-ENGINE-006` `P0` `IOS` 构建 arm64 debug/release 导出模板。
- [ ] `DV-FUNC-ENGINE-007` `P0` `WEB` 构建 wasm32 threads debug/release 导出模板。
- [ ] `DV-FUNC-ENGINE-008` `P0` `DESKTOP` 桌面构建启用 Tiled Mesh、3D Tiles、Draco、OSG、Geo。
- [ ] `DV-FUNC-ENGINE-009` `P0` `ANDROID IOS WEB` 移动/Web 构建启用 Tiled Mesh、3D Tiles、Draco、OSG、Geo。
- [ ] `DV-FUNC-ENGINE-010` `P0` `ALL` 导出模板版本和特性标签与编辑器严格匹配，不使用现有 4.7.2 模板。
- [ ] `DV-FUNC-ENGINE-011` `P1` `ALL` 为构建配置生成模块清单和第三方许可证清单。
- [ ] `DV-FUNC-ENGINE-012` `P1` `ALL` 记录未来合并 4.7.3 stable 的步骤和回归范围。

## 2. Tiled Mesh 通用流送框架

- [ ] `DV-FUNC-TILED-001` `P0` `ALL` `[基础已有]` `TiledMeshInstance3D` 能异步打开、关闭、重载和暂停数据集。
- [ ] `DV-FUNC-TILED-002` `P0` `ALL` `[基础已有]` 本地与 HTTP 数据请求支持优先级、取消、重试和相同请求合并。
- [ ] `DV-FUNC-TILED-003` `P0` `ALL` `[基础已有]` 解析、图片解码和 CPU 网格打包不阻塞主线程。
- [ ] `DV-FUNC-TILED-004` `P0` `ALL` `[基础已有]` 所有异步结果使用 owner、generation、request ID 隔离旧结果。
- [ ] `DV-FUNC-TILED-005` `P0` `ALL` `[基础已有]` REPLACE 内容未可显示前保持父级回退可见。
- [ ] `DV-FUNC-TILED-006` `P0` `ALL` `[基础已有]` LRU 只驱逐可重建且当前不需要的 Resident 内容。
- [ ] `DV-FUNC-TILED-007` `P0` `ALL` 扩展模块平台声明并修复 macOS、Android、iOS 编译与线程差异。
- [ ] `DV-FUNC-TILED-008` `P0` `ALL` 增加共享双精度源原点和共享 `source_to_local` 基接口。
- [ ] `DV-FUNC-TILED-009` `P0` `ALL` 共享原点平移时不重解析网格，并同步全部 RenderingServer 实例。
- [ ] `DV-FUNC-TILED-010` `P0` `ALL` 共享坐标基改变时安全取消并重载当前会话。
- [ ] `DV-FUNC-TILED-011` `P1` `ALL` 增加全局透明度并正确应用到新旧材质。
- [ ] `DV-FUNC-TILED-012` `P1` `ALL` 增加 Textured、Unlit、Wireframe 显示模式。
- [ ] `DV-FUNC-TILED-013` `P1` `ALL` 将格式 warning 通过信号发送给应用，不只写控制台。
- [ ] `DV-FUNC-TILED-014` `P0` `ALL` 关闭节点后没有数据、解析、打包、图片或 GPU 资源泄漏。
- [ ] `DV-FUNC-TILED-015` `P1` `ALL` ClassDB 文档与新增属性、方法和信号一致。

## 3. WGS84/ECEF 地理参考

- [ ] `DV-FUNC-GEOREF-001` `P0` `ALL` 新增 `GeoReference3D` 并完成 ClassDB 注册及文档。
- [ ] `DV-FUNC-GEOREF-002` `P0` `ALL` 实现 WGS84 经纬高到 ECEF 的双精度转换。
- [ ] `DV-FUNC-GEOREF-003` `P0` `ALL` 实现 ECEF 到 WGS84 经纬高的稳定迭代转换。
- [ ] `DV-FUNC-GEOREF-004` `P0` `ALL` 实现 ECEF 与局部 East-Up-South Godot 坐标互转。
- [ ] `DV-FUNC-GEOREF-005` `P0` `ALL` 经度规范化覆盖日期变更线，纬度限制覆盖极点。
- [ ] `DV-FUNC-GEOREF-006` `P0` `ALL` 以 double 保存共享 ECEF 原点，以 float 局部坐标提交渲染。
- [ ] `DV-FUNC-GEOREF-007` `P0` `ALL` 相机超过阈值后触发浮动原点平移。
- [ ] `DV-FUNC-GEOREF-008` `P0` `ALL` 原点平移同步地球、地形、影像、3D Tiles、锚定模型和相机目标。
- [ ] `DV-FUNC-GEOREF-009` `P0` `ALL` 原点平移前后屏幕位置连续，没有一帧错位。
- [ ] `DV-FUNC-GEOREF-010` `P1` `ALL` 暴露 `origin_shifted`、坐标转换和当前原点查询接口。
- [ ] `DV-FUNC-GEOREF-011` `P0` `ALL` 多个地理 3D Tiles 使用同一 ENU 基并保持相对精确对齐。

## 4. WGS84 地球与瓦片调度

- [ ] `DV-FUNC-GLOBE-001` `P0` `ALL` 新增 `GeoGlobe3D` 并完成 ClassDB 注册及文档。
- [ ] `DV-FUNC-GLOBE-002` `P0` `ALL` 生成 WGS84 椭球根瓦片和四叉树拓扑。
- [ ] `DV-FUNC-GLOBE-003` `P0` `ALL` 使用屏幕空间误差选择地球/地形 LOD。
- [ ] `DV-FUNC-GLOBE-004` `P0` `ALL` 实现视锥裁剪。
- [ ] `DV-FUNC-GLOBE-005` `P0` `ALL` 实现地平线遮挡裁剪。
- [ ] `DV-FUNC-GLOBE-006` `P0` `ALL` 后台生成地形网格，主线程按预算创建资源。
- [ ] `DV-FUNC-GLOBE-007` `P0` `ALL` 相邻不同 LOD 瓦片使用裙边或等效方法隐藏裂缝。
- [ ] `DV-FUNC-GLOBE-008` `P0` `ALL` 实现可见瓦片优先队列、取消、generation 和 LRU。
- [ ] `DV-FUNC-GLOBE-009` `P0` `ALL` 无影像和 DEM 时显示空白椭球材质。
- [ ] `DV-FUNC-GLOBE-010` `P0` `ALL` 支持三维球、哥伦布显示模式切换。
- [ ] `DV-FUNC-GLOBE-011` `P1` `ALL` 提供地球显隐、地形显隐、线框和瓦片边界调试开关。
- [ ] `DV-FUNC-GLOBE-012` `P1` `ALL` 提供瓦片、请求、缓存、网格和纹理统计。
- [ ] `DV-FUNC-GLOBE-013` `P0` `ALL` 地球瓦片正确响应共享原点变化。

## 5. 经纬网格

- [ ] `DV-FUNC-GRID-001` `P1` `ALL` 实现可开关的经线与纬线渲染。
- [ ] `DV-FUNC-GRID-002` `P1` `ALL` 根据相机高度自动选择合理网格间距。
- [ ] `DV-FUNC-GRID-003` `P1` `ALL` 跨越日期变更线时网格连续。
- [ ] `DV-FUNC-GRID-004` `P1` `ALL` 极区网格不会产生无效几何或严重密集闪烁。
- [ ] `DV-FUNC-GRID-005` `P1` `ALL` 经纬标签显示度数和东/西/南/北方向。
- [ ] `DV-FUNC-GRID-006` `P1` `ALL` 标签根据屏幕密度隐藏重叠项。
- [ ] `DV-FUNC-GRID-007` `P1` `ALL` 网格贴合椭球或当前 DEM 地表。
- [ ] `DV-FUNC-GRID-008` `P1` `ALL` 网格具有可配置颜色、透明度和标签开关。
- [ ] `DV-FUNC-GRID-009` `P1` `ALL` 网格使用偏移或深度策略避免 Z-Fighting。

## 6. 影像数据源

- [ ] `DV-FUNC-IMG-001` `P0` `ALL` 新增 `GeoImagerySource` 资源、配置校验和 ClassDB 文档。
- [ ] `DV-FUNC-IMG-002` `P0` `ALL` 支持 EPSG:3857 XYZ URL 模板。
- [ ] `DV-FUNC-IMG-003` `P0` `ALL` 支持 EPSG:4326/WorldCRS84Quad URL 模板。
- [ ] `DV-FUNC-IMG-004` `P1` `ALL` 支持 `{z}`、`{x}`、`{y}`、`{s}` 替换。
- [ ] `DV-FUNC-IMG-005` `P1` `ALL` 支持 XYZ 和 TMS Y 轴规则。
- [ ] `DV-FUNC-IMG-006` `P0` `ALL` 支持 PNG、JPEG、WebP 下载和解码。
- [ ] `DV-FUNC-IMG-007` `P0` `ALL` 影像请求只针对可见或即将可见瓦片。
- [ ] `DV-FUNC-IMG-008` `P0` `ALL` 影像层支持名称、显隐、透明度、上下排序和移除。
- [ ] `DV-FUNC-IMG-009` `P0` `ALL` 多影像按图层顺序使用 Alpha Over 合成。
- [ ] `DV-FUNC-IMG-010` `P1` `ALL` 显示数据源 Attribution，并在图层关闭后移除。
- [ ] `DV-FUNC-IMG-011` `P0` `ALL` 图层取消后停止未完成下载和解码。
- [ ] `DV-FUNC-IMG-012` `P0` `WEB` 跨域失败显示 CORS 专用诊断。

## 7. WMTS

- [ ] `DV-FUNC-WMTS-001` `P0` `ALL` 支持直接配置 WMTS RESTful URL 模板。
- [ ] `DV-FUNC-WMTS-002` `P0` `ALL` 支持 WMTS KVP GetTile 请求。
- [ ] `DV-FUNC-WMTS-003` `P0` `ALL` 下载并解析 WMTS 1.0 GetCapabilities。
- [ ] `DV-FUNC-WMTS-004` `P0` `ALL` 从 Capabilities 列出 Layer、Style、Format、TileMatrixSet。
- [ ] `DV-FUNC-WMTS-005` `P0` `ALL` 识别 EPSG:4326、CRS84 和 EPSG:3857 矩阵集。
- [ ] `DV-FUNC-WMTS-006` `P0` `ALL` 对其他 CRS 给出明确的不支持错误。
- [ ] `DV-FUNC-WMTS-007` `P1` `ALL` 正确处理矩阵层级标识、比例尺、TopLeftCorner、TileWidth/Height 和 MatrixWidth/Height。
- [ ] `DV-FUNC-WMTS-008` `P1` `ALL` 从服务元数据读取并显示 Attribution/Provider 信息。
- [ ] `DV-FUNC-WMTS-009` `P1` `ALL` 不提供 Dimension UI；存在必需 Dimension 时提示不支持。

## 8. Cesium ion

- [ ] `DV-FUNC-ION-001` `P0` `ALL` 提供 Asset ID 和用户 Access Token 输入界面。
- [ ] `DV-FUNC-ION-002` `P0` `ALL` `[基础已有]` 解析 3DTILES endpoint 并加载返回的 tileset。
- [ ] `DV-FUNC-ION-003` `P0` `ALL` 解析影像 endpoint 并创建影像图层。
- [ ] `DV-FUNC-ION-004` `P0` `ALL` 用户 Token 只发送到官方 endpoint。
- [ ] `DV-FUNC-ION-005` `P0` `ALL` 资产 Token 只发送到允许的 HTTPS 同源资产资源。
- [ ] `DV-FUNC-ION-006` `P0` `ALL` Token 不写入日志、诊断、配置、场景或 URL 历史。
- [ ] `DV-FUNC-ION-007` `P1` `ALL` 显示 ion 返回的 Attribution。
- [ ] `DV-FUNC-ION-008` `P1` `ALL` Token 过期后提供重新加载和更新 Token 的可读提示。

## 9. 预切片 DEM

- [ ] `DV-FUNC-DEM-MANIFEST-001` `P0` `ALL` 新增 `GeoDemSource` 资源和 DigitalViewer DEM 清单解析器。
- [ ] `DV-FUNC-DEM-MANIFEST-002` `P0` `ALL` 校验清单版本、CRS、scheme、tile size、zoom、bounds、URL 和 encoding。
- [ ] `DV-FUNC-DEM-MANIFEST-003` `P0` `ALL` 支持本地目录中的相对瓦片 URL。
- [ ] `DV-FUNC-DEM-MANIFEST-004` `P0` `ALL` 支持 ZIP 会话目录中的相对瓦片 URL。
- [ ] `DV-FUNC-DEM-MANIFEST-005` `P0` `ALL` 支持 HTTP(S) URL 模板。
- [ ] `DV-FUNC-DEM-CODEC-001` `P0` `ALL` 无精度损失地读取 16 位灰度 PNG 高程。
- [ ] `DV-FUNC-DEM-CODEC-002` `P0` `ALL` 按 Mapbox 标准公式解码 Terrain-RGB。
- [ ] `DV-FUNC-DEM-CODEC-003` `P1` `ALL` Terrain-RGB 允许清单覆盖 scale/offset。
- [ ] `DV-FUNC-DEM-CODEC-004` `P0` `ALL` 解码小端 Float32 RAW。
- [ ] `DV-FUNC-DEM-CODEC-005` `P1` `ALL` 解码 Zstandard 压缩 Float32 RAW。
- [ ] `DV-FUNC-DEM-CODEC-006` `P0` `ALL` 所有编码正确应用 scale、offset 和 NoData。
- [ ] `DV-FUNC-DEM-CODEC-007` `P0` `ALL` 损坏、尺寸错误或非有限高程不会产生部分地形。

## 10. GeoTIFF/COG DEM

- [ ] `DV-FUNC-TIFF-001` `P0` `ALL` 引入 libtiff 并登记版本、来源、补丁和许可证。
- [ ] `DV-FUNC-TIFF-002` `P0` `ALL` libtiff 可在六个平台静态构建。
- [ ] `DV-FUNC-TIFF-003` `P0` `ALL` 读取经典 TIFF 和 BigTIFF。
- [ ] `DV-FUNC-TIFF-004` `P0` `ALL` 读取条带和瓦片布局。
- [ ] `DV-FUNC-TIFF-005` `P0` `ALL` 读取单波段 UInt16、Int16、Float32。
- [ ] `DV-FUNC-TIFF-006` `P0` `ALL` 读取无压缩、LZW、DEFLATE 数据。
- [ ] `DV-FUNC-TIFF-007` `P1` `ALL` 选择合适的内部 Overview，避免总是读取最高分辨率。
- [ ] `DV-FUNC-TIFF-008` `P0` `ALL` 读取 NoData、ModelPixelScale、ModelTiepoint 和 GeoKey。
- [ ] `DV-FUNC-TIFF-009` `P0` `ALL` 区分 PixelIsArea 和 PixelIsPoint。
- [ ] `DV-FUNC-TIFF-010` `P0` `ALL` 接受北向上的 EPSG:4326/3857。
- [ ] `DV-FUNC-TIFF-011` `P0` `ALL` 拒绝旋转/剪切仿射、多波段和未知投影，并显示原因。
- [ ] `DV-FUNC-TIFF-012` `P0` `ALL` 本地 TIFF 在后台读取和采样。
- [ ] `DV-FUNC-TIFF-013` `P0` `ALL` 远程 TIFF 整文件下载到会话缓存后再读取。
- [ ] `DV-FUNC-TIFF-014` `P0` `DESKTOP` 远程默认上限为 512 MiB。
- [ ] `DV-FUNC-TIFF-015` `P0` `ANDROID IOS WEB` 远程默认上限为 128 MiB。
- [ ] `DV-FUNC-TIFF-016` `P0` `ALL` 超限后暂停，显示大小/内存风险，并允许当前会话确认继续。
- [ ] `DV-FUNC-TIFF-017` `P0` `ALL` 下载失败、磁盘不足或解码失败时清除不完整缓存。

## 11. 多 DEM 合成与地形夸张

- [ ] `DV-FUNC-DEM-LAYER-001` `P0` `ALL` 同时管理多个启用的 DEM 数据源。
- [ ] `DV-FUNC-DEM-LAYER-002` `P0` `ALL` 列表上方 DEM 覆盖下方 DEM。
- [ ] `DV-FUNC-DEM-LAYER-003` `P0` `ALL` 上层 NoData 样本回退到底层有效值。
- [ ] `DV-FUNC-DEM-LAYER-004` `P0` `ALL` 不同分辨率和投影的 DEM 重采样到目标地形网格。
- [ ] `DV-FUNC-DEM-LAYER-005` `P0` `ALL` EPSG:4326 与 EPSG:3857 的范围和采样转换正确。
- [ ] `DV-FUNC-DEM-LAYER-006` `P0` `ALL` 每层先应用 scale、offset 和垂直偏移，再参与覆盖。
- [ ] `DV-FUNC-DEM-LAYER-007` `P0` `ALL` 图层排序变化只重建受影响的可见/预取瓦片。
- [ ] `DV-FUNC-DEM-EXAG-001` `P1` `ALL` 全局地形夸张默认值为 1.0。
- [ ] `DV-FUNC-DEM-EXAG-002` `P1` `ALL` 滑块范围 0–10，数值输入允许 0–100。
- [ ] `DV-FUNC-DEM-EXAG-003` `P1` `ALL` 夸张应用到 DEM 合成结果。
- [ ] `DV-FUNC-DEM-EXAG-004` `P1` `ALL` 修改夸张时异步重建可见地形，不冻结 UI。
- [ ] `DV-FUNC-DEM-EXAG-005` `P1` `ALL` 三维模型锚点和真实经纬高不随夸张改变。

## 12. 3D Tiles 适配器

- [ ] `DV-FUNC-3DT-001` `P0` `ALL` `[基础已有]` 自动识别 tileset JSON 与二进制容器。
- [ ] `DV-FUNC-3DT-002` `P0` `ALL` `[基础已有]` 解析显式 1.0/1.1 层级、外部 tileset 和多内容。
- [ ] `DV-FUNC-3DT-003` `P0` `ALL` `[基础已有]` 正确实现 ADD/REPLACE 和父级回退。
- [ ] `DV-FUNC-3DT-004` `P0` `ALL` `[基础已有]` 支持 GLB/glTF、b3dm、i3dm、pnts、cmpt。
- [ ] `DV-FUNC-3DT-005` `P0` `ALL` `[基础已有]` Draco 网格在六平台正确解码。
- [ ] `DV-FUNC-3DT-006` `P0` `ALL` `[基础已有]` pnts 点、颜色、法线和 Batch 属性正确显示。
- [ ] `DV-FUNC-3DT-007` `P0` `ALL` `[基础已有]` i3dm 嵌入/外部模型和实例变换正确。
- [ ] `DV-FUNC-3DT-008` `P0` `ALL` ECEF/region 数据使用共享 `GeoReference3D`，而不是每层独立 ENU。
- [ ] `DV-FUNC-3DT-009` `P0` `ALL` 笛卡尔 tileset 保持本地坐标，并允许用户强制 ECEF。
- [ ] `DV-FUNC-3DT-010` `P0` `ALL` 隐式切片和未知 required extension 显示明确错误。
- [ ] `DV-FUNC-3DT-011` `P1` `ALL` Batch Table 属性进入统一拾取面板。

## 13. OSG/OSGB 适配器

- [ ] `DV-FUNC-OSG-001` `P0` `WIN LINUX WEB` `[基础已有]` 现有 OSG 测试全部通过。
- [ ] `DV-FUNC-OSG-002` `P0` `MAC` OSG 静态解析器编译、启动并解析样例。
- [ ] `DV-FUNC-OSG-003` `P0` `ANDROID` OSG 静态解析器编译、启动并解析样例。
- [ ] `DV-FUNC-OSG-004` `P0` `IOS` OSG 静态解析器编译、启动并解析样例。
- [ ] `DV-FUNC-OSG-005` `P0` `ALL` 排除动态插件加载和不使用的 OSG 图形上下文代码。
- [ ] `DV-FUNC-OSG-006` `P0` `ALL` ASCII OSG、OSGB、zlib OSGB 正确识别。
- [ ] `DV-FUNC-OSG-007` `P0` `ALL` PagedLOD 和 ProxyNode 相对路径在本地、ZIP、HTTP 下正确解析。
- [ ] `DV-FUNC-OSG-008` `P0` `ALL` 距离 LOD 和像素 LOD 显示正确。
- [ ] `DV-FUNC-OSG-009` `P0` `ALL` 基础材质、顶点色、透明、Alpha Test、双面和纹理正确转换。
- [ ] `DV-FUNC-OSG-010` `P0` `ALL` PNG/JPEG/BMP/TGA 由 Godot 图片解码路径处理。
- [ ] `DV-FUNC-OSG-011` `P0` `ALL` 全局串行解析队列支持优先级、取消、shutdown 和旧结果隔离。
- [ ] `DV-FUNC-OSG-012` `P1` `ALL` 不支持的 Drawable/Primitive 进入图层 warning 面板。

## 14. 独立 glTF/GLB

- [ ] `DV-FUNC-GLTF-001` `P0` `ALL` 通过 Godot `GLTFDocument.append_from_file()` 导入本地 glTF/GLB。
- [ ] `DV-FUNC-GLTF-002` `P0` `ALL` 通过 `append_from_buffer()` 或会话文件加载 ZIP/Web/远程 GLB。
- [ ] `DV-FUNC-GLTF-003` `P0` `ALL` `.gltf` 的 base path 正确解析外部 buffer 与纹理。
- [ ] `DV-FUNC-GLTF-004` `P0` `ALL` `generate_scene()` 生成的节点完整加入图层根节点。
- [ ] `DV-FUNC-GLTF-005` `P0` `ALL` Godot 支持的材质、纹理、Skin、Animation、Morph 和灯光行为不被应用层削减。
- [ ] `DV-FUNC-GLTF-006` `P1` `ALL` 图层面板显示导入警告、缺失依赖和 required extension 错误。
- [ ] `DV-FUNC-GLTF-007` `P0` `ALL` 删除图层后释放生成的节点、资源和动画引用。
- [ ] `DV-FUNC-GLTF-008` `P1` `ALL` 动画存在时提供最小播放/暂停和时间轴选择；无动画时不显示相关控件。

## 15. 图层模型与生命周期

- [ ] `DV-FUNC-LAYER-001` `P0` `ALL` 建立稳定的会话级图层 ID、名称、类型和状态模型。
- [ ] `DV-FUNC-LAYER-002` `P0` `ALL` 支持添加模型、影像和 DEM 图层。
- [ ] `DV-FUNC-LAYER-003` `P0` `ALL` 支持显隐、删除、聚焦、暂停、恢复和重载。
- [ ] `DV-FUNC-LAYER-004` `P0` `ALL` 模型层支持 0–100% 透明度。
- [ ] `DV-FUNC-LAYER-005` `P0` `ALL` 影像层支持排序和透明度。
- [ ] `DV-FUNC-LAYER-006` `P0` `ALL` DEM 层支持排序、启用和参数修改。
- [ ] `DV-FUNC-LAYER-007` `P0` `ALL` 每层单独汇总下载、解析、Resident、失败和内存统计。
- [ ] `DV-FUNC-LAYER-008` `P0` `ALL` 图层删除后所有后台任务停止且结果不可重新进入场景。
- [ ] `DV-FUNC-LAYER-009` `P0` `ALL` 图层切换可见性不丢失可恢复的加载拓扑。
- [ ] `DV-FUNC-LAYER-010` `P1` `ALL` 图层错误状态允许复制信息、重试和移除。
- [ ] `DV-FUNC-LAYER-011` `P0` `ALL` 应用退出不保存图层列表。

## 16. 模型配准

- [ ] `DV-FUNC-ALIGN-001` `P0` `ALL` 提供“局部场景”和“经纬高锚点”配准模式。
- [ ] `DV-FUNC-ALIGN-002` `P0` `ALL` 经纬高锚点转换为共享 ECEF/ENU 坐标。
- [ ] `DV-FUNC-ALIGN-003` `P0` `ALL` 支持 East/North/Up 米制偏移。
- [ ] `DV-FUNC-ALIGN-004` `P0` `ALL` 支持 Yaw/Pitch/Roll。
- [ ] `DV-FUNC-ALIGN-005` `P0` `ALL` 支持 Y-Up、Z-Up。
- [ ] `DV-FUNC-ALIGN-006` `P0` `ALL` 支持源原点。
- [ ] `DV-FUNC-ALIGN-007` `P1` `ALL` 支持统一比例与 XYZ 非统一比例。
- [ ] `DV-FUNC-ALIGN-008` `P1` `DESKTOP WEB` 提供场景 Gizmo 平移、旋转和缩放。
- [ ] `DV-FUNC-ALIGN-009` `P0` `ALL` 数值面板修改后立即更新，不需要重新导入普通模型。
- [ ] `DV-FUNC-ALIGN-010` `P0` `ALL` 配准状态只存在于当前会话。

## 17. 相机与输入

- [ ] `DV-FUNC-CAMERA-001` `P0` `ALL` 实现轨道相机模式。
- [ ] `DV-FUNC-CAMERA-002` `P0` `ALL` 实现飞行相机模式。
- [ ] `DV-FUNC-CAMERA-003` `P0` `DESKTOP WEB` 支持鼠标旋转、平移和滚轮缩放。
- [ ] `DV-FUNC-CAMERA-004` `P0` `DESKTOP WEB` 支持 WASD、Shift 加速和 Ctrl 减速。
- [ ] `DV-FUNC-CAMERA-005` `P0` `ANDROID IOS` 支持单指旋转。
- [ ] `DV-FUNC-CAMERA-006` `P0` `ANDROID IOS` 支持双指缩放和平移。
- [ ] `DV-FUNC-CAMERA-007` `P1` `ANDROID IOS` 支持双击聚焦和长按拾取。
- [ ] `DV-FUNC-CAMERA-008` `P1` `ANDROID IOS` 提供飞行虚拟摇杆。
- [ ] `DV-FUNC-CAMERA-009` `P0` `ALL` 相机速度根据观察距离自动缩放。
- [ ] `DV-FUNC-CAMERA-010` `P0` `ALL` 近远裁剪面根据尺度自动调整。
- [ ] `DV-FUNC-CAMERA-011` `P0` `ALL` 防止相机意外进入当前地表以下。
- [ ] `DV-FUNC-CAMERA-012` `P0` `ANDROID IOS` 横竖屏旋转后手势、相机和 UI 状态连续。

## 18. 飞往坐标与会话视点

- [ ] `DV-FUNC-FLYTO-001` `P1` `ALL` 提供经度、纬度、观察高度输入。
- [ ] `DV-FUNC-FLYTO-002` `P1` `ALL` 提供可选 Heading 和 Pitch 输入。
- [ ] `DV-FUNC-FLYTO-003` `P1` `ALL` 校验并规范化经纬度、极点和日期变更线。
- [ ] `DV-FUNC-FLYTO-004` `P1` `ALL` 相机沿平滑路径飞往目标。
- [ ] `DV-FUNC-FLYTO-005` `P1` `ALL` 飞行时长随距离调整，避免超长或瞬移。
- [ ] `DV-FUNC-FLYTO-006` `P1` `ALL` 用户输入可取消当前飞行动画。
- [ ] `DV-FUNC-VIEWPOINT-001` `P1` `ALL` 显示当前经纬高、Heading、Pitch 和观察距离。
- [ ] `DV-FUNC-VIEWPOINT-002` `P1` `ALL` 添加和命名当前视点。
- [ ] `DV-FUNC-VIEWPOINT-003` `P1` `ALL` 飞往和删除会话视点。
- [ ] `DV-FUNC-VIEWPOINT-004` `P0` `ALL` 退出应用后视点不恢复、不写入磁盘。

## 19. 显示模式与材质覆盖

- [ ] `DV-FUNC-RENDER-001` `P1` `ALL` Textured 模式保持源材质和纹理。
- [ ] `DV-FUNC-RENDER-002` `P1` `ALL` Unlit 模式保持基础颜色并忽略光照。
- [ ] `DV-FUNC-RENDER-003` `P1` `ALL` Wireframe 模式在六平台可用或提供等效线框 Shader。
- [ ] `DV-FUNC-RENDER-004` `P1` `ALL` 阴影开关同步当前和后续创建的实例。
- [ ] `DV-FUNC-RENDER-005` `P1` `ALL` 图层透明度同步当前和后续创建的材质。
- [ ] `DV-FUNC-RENDER-006` `P0` `ALL` 透明图层深度和排序行为没有严重闪烁。
- [ ] `DV-FUNC-RENDER-007` `P1` `ALL` 添加 WGS84 椭球后，空白地球、环境光和方向光提供可读的默认外观。

## 20. 拾取与属性面板

- [ ] `DV-FUNC-PICK-001` `P0` `ALL` 在全部可见模型层中选取最近命中。
- [ ] `DV-FUNC-PICK-002` `P0` `ALL` 3D Tiles/OSG 流式网格使用 `intersect_screen_point()`。
- [ ] `DV-FUNC-PICK-003` `P0` `ALL` glTF/USD 静态 Mesh 提供三角形拾取。
- [ ] `DV-FUNC-PICK-004` `P0` `ALL` pnts 提供点拾取。
- [ ] `DV-FUNC-PICK-005` `P1` `ALL` 返回图层、URI、局部位置、ECEF、经纬高、法线和距离。
- [ ] `DV-FUNC-PICK-006` `P1` `ALL` 返回 Tile、Content、Surface、Triangle/Point 和 Batch ID。
- [ ] `DV-FUNC-PICK-007` `P1` `ALL` 显示 Batch Table 属性和可用格式元数据。
- [ ] `DV-FUNC-PICK-008` `P1` `ALL` 在命中点显示可清除的高亮标记。
- [ ] `DV-FUNC-PICK-009` `P1` `ALL` 支持飞往命中点。
- [ ] `DV-FUNC-PICK-010` `P0` `ALL` 删除或隐藏图层后清除无效拾取结果。

## 21. 本地文件、目录和 ZIP

- [ ] `DV-FUNC-FILE-001` `P0` `WIN LINUX MAC` 使用原生文件选择器打开支持格式。
- [ ] `DV-FUNC-FILE-002` `P0` `WIN LINUX MAC` 使用原生目录选择器打开目录数据集。
- [ ] `DV-FUNC-FILE-003` `P1` `WIN LINUX MAC` 支持文件和目录拖放。
- [ ] `DV-FUNC-FILE-004` `P0` `ANDROID` 使用 SAF 选择文件。
- [ ] `DV-FUNC-FILE-005` `P0` `ANDROID` 使用 SAF 选择目录并访问相对依赖。
- [ ] `DV-FUNC-FILE-006` `P0` `IOS` 使用 `UIDocumentPicker` 打开单文件和 ZIP。
- [ ] `DV-FUNC-FILE-007` `P0` `WEB` 支持文件输入并立即复制到会话文件系统。
- [ ] `DV-FUNC-FILE-008` `P0` `WEB` 支持目录输入/拖放并保持相对路径。
- [ ] `DV-FUNC-ZIP-001` `P0` `ALL` ZIP 解压到唯一会话目录。
- [ ] `DV-FUNC-ZIP-002` `P0` `ALL` 拒绝 `..`、绝对路径、盘符、UNC 和目标目录逃逸。
- [ ] `DV-FUNC-ZIP-003` `P0` `ALL` 拒绝符号链接和其他特殊文件。
- [ ] `DV-FUNC-ZIP-004` `P0` `ALL` 限制文件数量、单文件大小、总展开大小和压缩比。
- [ ] `DV-FUNC-ZIP-005` `P1` `ALL` 多个候选根文件时显示格式、路径和大小供用户选择。
- [ ] `DV-FUNC-ZIP-006` `P0` `ALL` 关闭图层或应用后安全清理会话目录。

## 22. HTTP、认证与错误

- [ ] `DV-FUNC-HTTP-001` `P0` `ALL` 支持 HTTP 和 HTTPS 根资源及相对依赖。
- [ ] `DV-FUNC-HTTP-002` `P0` `ALL` 重定向限制次数并正确解析相对 Location。
- [ ] `DV-FUNC-HTTP-003` `P0` `ALL` 4xx 直接失败，5xx/连接错误按策略重试。
- [ ] `DV-FUNC-HTTP-004` `P0` `ALL` 默认超时 30 秒、重试 2 次，可在当前图层调整。
- [ ] `DV-FUNC-HTTP-005` `P0` `ALL` Content-Length 和实际接收量都受大小上限约束。
- [ ] `DV-FUNC-HTTP-006` `P0` `ALL` 相同 URL 请求合并，取消一个订阅者不影响其他订阅者。
- [ ] `DV-FUNC-HTTP-007` `P1` `ALL` 支持会话级自定义 Header。
- [ ] `DV-FUNC-HTTP-008` `P0` `ALL` Authorization、Cookie、Token 和敏感查询参数从日志中移除。
- [ ] `DV-FUNC-HTTP-009` `P0` `ALL` 不提供跳过证书校验功能。
- [ ] `DV-FUNC-HTTP-010` `P0` `WEB` CORS、COEP、COOP、TLS 错误分别给出可操作提示。

## 23. 运行时脚本与外部 AI

> **v1.0 范围：不内置 AI。** 23.1/23.2 的 AI 条目是 v1.1 候选，不进入 v1.0 交付；v1.0 只交付脚本工作台、运行时脚本 API 和外部 AI 的本机 MCP。外部 AI 只能操作运行时场景，不能修改产品。
>
> 本章记录当前实现及已有验证范围。标记“Windows 已验证”的条目仅代表 Windows 普通应用运行模式或布局模拟验证，不代表所有目标平台验收通过；仅确认有实现、尚未注明验证结果的条目保持未勾选。正式导出、真实模型生成效果和其他平台另行验收。本次清单更新未重新运行测试。
>
> 实现依据：[AI 脚本桥设计](#232-ai-脚本桥gdscript取代-json-场景代理)、[原生脚本工作台](docs/native-script-workbench.md)。完整调试器和死循环强制中止按当前范围暂缓，不作为本版新增开发任务。

### 23.1 AI 聊天与模型连接

- [ ] `DV-FUNC-AI-001` `P0` `DESKTOP ANDROID IOS WEB` `[已实现，Windows 桌面/移动布局已验证]` 提供 AI 场景助手入口、消息列表、输入框、发送、停止、新聊天和上下文查看。
- [ ] `DV-FUNC-AI-002` `P0` `ALL` `[已实现，待验收]` 未连接模型时使用本地规则演示；本地规则也只生成 GDScript 草案，不直接写 JSON 场景。
- [ ] `DV-FUNC-AI-003` `P0` `ALL` `[已实现，待验收]` 连接页支持 Chat Completions 基础地址或完整地址、模型名、访问凭据、超时和输出预算参数配置。
- [ ] `DV-FUNC-AI-004` `P0` `ALL` `[已实现，待验收]` 基础地址自动补全 `/v1/chat/completions`；拒绝缺少协议、查询参数、凭据嵌入和不兼容 Responses/Messages 地址。
- [ ] `DV-FUNC-AI-005` `P0` `ALL` `[已实现，待验收]` 连接测试使用真实异步请求，显示解析、连接、接收、成功、协议错误、鉴权错误、限流和超时原因。
- [ ] `DV-FUNC-AI-006` `P0` `ALL` `[已实现，待验收]` 模型请求支持取消；停止时取消请求、停止飞行和持续行为，并丢弃迟到响应。
- [ ] `DV-FUNC-AI-007` `P0` `ALL` `[已实现，待验收]` 支持 tools/tool_calls/role=tool 的多轮工具调用，单轮限制工具轮数、调用数和请求体大小。
- [ ] `DV-FUNC-AI-008` `P0` `ALL` `[已实现，待验收]` 聊天上下文按完整轮次淘汰，最多保留 8 轮、18,000 UTF-8 字节、6 条旧请求摘录和 12 条执行事实。
- [ ] `DV-FUNC-AI-009` `P0` `ALL` `[已实现，待验收]` 新聊天或切换连接时清空消息、摘要、指代和本聊天脚本草案，但保留当前场景与会话视点。
- [ ] `DV-FUNC-AI-010` `P0` `ALL` `[已实现，待验收]` 请求包含聊天消息和必要的场景摘要、相机状态及工具事实；应用不自动附带源文件、原始资源 URL 或截图，访问凭据仅用于服务认证而不写入提示词。用户输入的敏感信息仍会发送给模型服务。

### 23.2 AI 脚本桥（GDScript，取代 JSON 场景代理）

- [ ] `DV-FUNC-AI-011` `P0` `ALL` `[已实现，Windows 回归已验证]` 注册 `script_propose`、`script_open`、`script_inspect`、`get_script_state` 和脚本会话工具；不注册 JSON 场景命令。
- [ ] `DV-FUNC-AI-012` `P0` `ALL` `[已实现，Windows 回归已验证]` `script_propose` 返回 GDScript 草案、目标说明、脚本版本和待审查状态；不返回 SceneProgram/JSON。
- [ ] `DV-FUNC-AI-013` `P0` `ALL` `[已实现，Windows 回归已验证]` AI 草案只能放入脚本面板；查看/审查不自动运行，必须由用户确认信任后手动运行。
- [ ] `DV-FUNC-AI-014` `P0` `ALL` `[已实现，Windows 回归已验证]` 支持通过脚本 API 设置/创建/删除对象、背景、环境、相机、经纬网和受支持会话对象；无 JSON 写入通道。
- [ ] `DV-FUNC-AI-015` `P0` `ALL` `[已实现，Windows 回归已验证]` 脚本 API 使用注册表、类型/范围校验和脚本版本；未知对象/方法返回可修正错误。
- [ ] `DV-FUNC-AI-016` `P0` `ALL` `[已实现，Windows 回归已验证]` AI 草案先编译/静态检查；错误定位到源码行；运行输出、错误、调用栈和脚本版本可见。
- [ ] `DV-FUNC-AI-017` `P0` `ALL` `[已实现，Windows 回归已验证]` 脚本版本和调用 ID 去重；手动修改或新草案后拒绝旧响应；过期草案不覆盖当前编辑内容。
- [ ] `DV-FUNC-AI-018` `P0` `ALL` `[已实现，Windows 回归已验证]` AI、脚本会话和桌面/移动视图共享同一运行时场景；布局切换不复制或丢失对象。
- [ ] `DV-FUNC-AI-019` `P0` `ALL` `[已实现，待验收]` 旧 JSON/SceneProgram 场景工具已移除，不再有 `native_owned` 或 JSON 回退路径；AI 只能通过脚本桥交互。
- [ ] `DV-FUNC-AI-024` `P0` `DESKTOP WEB` 应用内 AI 定位为场景 Copilot：只修改运行时场景，不读取仓库、不调用终端、不修改工程/依赖/构建。
- [ ] `DV-FUNC-AI-025` `P0` `DESKTOP WEB` 脚本运行前显示信任门：编译结果、权限、差异、检查点、同步死循环警告；未勾选确认时运行按钮禁用。
- [ ] `DV-FUNC-AI-026` `P0` `ANDROID IOS` 移动端 AI 只读：不执行脚本、不修改场景，提示脚本修改在桌面/Web 完成。

### 23.3 原生 GDScript 工作台

- [x] `DV-FUNC-SCRIPT-001` `P0` `DESKTOP WEB` `[已实现，Windows 桌面/Web 布局已验证]` 在桌面/Web 提供“脚本”入口；移动端不提供脚本面板；运行时不依赖 Godot Editor 或外部工程修改。
- [ ] `DV-FUNC-SCRIPT-002` `P0` `ALL` `[已实现，待验收]` 使用 CodeEdit 提供行号、折叠、自动缩进、括号补全、查找、基础 GDScript 语法高亮和常用词补全。
- [ ] `DV-FUNC-SCRIPT-003` `P0` `ALL` `[已实现，待验收]` 支持示例脚本、代码编辑、保存到 `user://scripts`、打开已有 `.gd` 文件和覆盖确认。
- [x] `DV-FUNC-SCRIPT-004` `P0` `ALL` `[已实现，Windows 回归已验证]` 每份文档首次运行或代码变化时显示信任确认；本次应用会话内已确认且代码未变时直接重跑。拒绝空脚本、超过 64 KiB 脚本和不符合入口契约的脚本。
- [x] `DV-FUNC-SCRIPT-005` `P0` `ALL` `[已实现，Windows 回归已验证]` 编译并实例化继承 Node/Node3D 的脚本，调用单参数 `run(viewer)` 入口。
- [x] `DV-FUNC-SCRIPT-006` `P0` `ALL` `[已实现，Windows 回归已验证]` 脚本通过 viewer 上下文访问真实场景根、地球/经纬网、相机、环境和 ScriptSession，可创建网格、材质、灯光及自定义节点。
- [x] `DV-FUNC-SCRIPT-007` `P0` `ALL` `[已实现，Windows 回归已验证]` 脚本实例保留在场景树中，支持 Godot 正常 `_process`、信号和生命周期回调。
- [x] `DV-FUNC-SCRIPT-008` `P1` `ALL` `[已实现，Windows 回归已验证]` 捕获脚本 print、编译/运行错误、行号和可用调用栈；源码改变后可打开只读运行快照定位旧错误。
- [x] `DV-FUNC-SCRIPT-009` `P1` `ALL` `[已实现，Windows 回归已验证]` 提供实际场景树和只读属性检查，最多限制显示节点数和层级，避免面板无限增长。
- [ ] `DV-FUNC-SCRIPT-010` `P0` `ALL` `[已实现，待验收]` 停止会话节点和脚本实例，重置场景时重建接管前场景、恢复导航快照并重新允许 AI 脚本桥控制。
- [x] `DV-FUNC-SCRIPT-011` `P0` `ALL` `[已实现，Windows 回归已验证]` 每个视口使用独立的 World3D/SubViewport；桌面与移动 Shell 显示同一活动视口，脚本修改不会因窗口或布局切换分裂。
- [ ] `DV-FUNC-SCRIPT-012` `P0` `ALL` `[已实现，待验收]` 明确原生脚本拥有应用权限，文件、网络和进程能力受平台限制；编译前必须由用户确认信任。提示同步死循环无法通过停止按钮中断，停止/重置不保证清理场景外副作用。
- [ ] `DV-FUNC-SCRIPT-013` `P0` `DESKTOP WEB` 添加数据支持 GDScript 来源：选择 `.gd`，校验 `extends Node` 与 `run(viewer)`，创建脚本图层并打开脚本面板；支持脚本属性、运行/停止和移除。

### 23.4 自动化 API / 外部 AI

- [ ] `DV-FUNC-API-001` `P0` `DESKTOP` 提供本机 MCP `/mcp` 与 Plugin Bridge `/bridge`，默认只监听 loopback；权限分为 `scene.read` / `script.run` / `capture`。
- [ ] `DV-FUNC-API-002` `P0` `DESKTOP` Codex、Harness、CLI、CI 通过 MCP 或 Plugin Bridge 使用同一脚本 API、权限、检查点和审计；不建立第二套运行时。
- [ ] `DV-FUNC-API-003` `P0` `DESKTOP` 自动化工具限定为 `scene.describe`、`viewport.list`、`script.inspect`、`script.propose`、`script.run`、`capture.screenshot`、`audit.list`；不提供 shell、文件系统、网络或 JSON 场景写接口。
- [ ] `DV-FUNC-API-004` `P0` `DESKTOP WEB` 设置页可在 MCP / Plugin Bridge 间切换（互斥），显示端点、权限、复制令牌和限制说明；Web 不暴露本机桥接。

### 23.5 发布与跨平台待验收

- [ ] `DV-FUNC-AI-021` `P0` `ALL` 在正式导出包验证模型连接、聊天上下文、工具执行、取消、错误反馈和场景所有权切换；使用真实兼容模型验证“读取场景 → 执行 → 根据错误修正 → 核对结果”，记录服务、模型及验证范围，不记录密钥。
- [ ] `DV-FUNC-AI-022` `P0` `ANDROID IOS WEB` 验证移动端网络策略及 Web 的 HTTPS、混合内容和 CORS 限制，给出可操作错误提示；不把 Windows 移动布局模拟当作实机验收。
- [ ] `DV-FUNC-AI-023` `P0` `DESKTOP WEB` AI 模型库支持添加多个模型并选择当前模型；添加表单包含显示名称、提供方、Base URL、模型 ID、凭据、上下文窗口；凭据仅当前会话内存，不写入 localStorage。
- [ ] `DV-FUNC-SCRIPT-013` `P0` `DESKTOP ANDROID IOS WEB` 完成正式导出包的 CodeEdit、GDScript/Logger 保留、脚本保存路径和运行时兼容性验收；单独记录 Web 存储限制及 iOS 等分发渠道对动态脚本的政策评估结果。

### 23.6 后续候选（不计入 v1.0 验收）

- [ ] `DV-FUNC-AI-020` `后续候选` `ALL` 将 AI 接入原生脚本工作台，支持生成/修改草稿、用户审阅与确认运行、按授权反馈输出和场景信息；当前未实现，不默认自动执行任意原生 GDScript。
- [ ] `DV-FUNC-SCRIPT-014` `暂缓` `ALL` 集成完整 Godot ScriptEditor 语义补全、原生断点、单步及暂停时局部变量；执行沙箱另行设计。当前版本仅提供基础高亮、日志和错误定位。
- [ ] `DV-FUNC-SCRIPT-015` `暂缓` `ALL` 为同步死循环提供 VM 级强制中止、执行预算、看门狗或线程安全恢复；不属于当前 v1.0 实现，不以现有“停止”按钮代替此能力。

## 24. 桌面用户界面

- [ ] `DV-FUNC-UI-DESKTOP-001` `P0` `DESKTOP WEB` 顶部工具栏只包含品牌应用菜单、属性、AI、脚本、FPS、设置；相机模式在每视口相机对象中，不占全局顶栏。飞往、视点、显示、经纬网在每个视口标题栏左侧（树/添加之后），添加/新建视口/全屏/视口切换也在视口标题栏。
- [ ] `DV-FUNC-UI-DESKTOP-002` `P0` `DESKTOP WEB` 每个视口标题栏左侧提供图层树按钮和添加按钮；图层树无背景内联在视口左上，按“顶层分组→图层”显示层级、状态、显隐、共享徽标和排序，只有树按钮能收起，随视口分割/缩放移动。
- [ ] `DV-FUNC-UI-DESKTOP-003` `P0` `DESKTOP WEB` 中央视口区域（单视口或分屏）占用剩余空间并正确获得输入焦点；同一时间只有一个活动视口。
- [ ] `DV-FUNC-UI-DESKTOP-004` `P0` `DESKTOP WEB` 右侧属性面板按选中对象切换：点视口/标题栏显示布局、环境、性能；点树顶部相机显示相机模式/投影/FOV/视点；点图层树行显示图层属性；不混排。
- [ ] `DV-FUNC-UI-DESKTOP-005` `P1` `DESKTOP WEB` 底部区域显示总体状态、性能摘要和错误列表。
- [ ] `DV-FUNC-UI-DESKTOP-006` `P1` `DESKTOP WEB` 右栏可关闭、拖出独立窗口、停靠回来，调整宽度并恢复默认；顶栏属性/脚本入口可重开；每视口图层树只有树按钮能收起，不改变其他视口布局。
- [ ] `DV-FUNC-UI-DESKTOP-007` `P1` `DESKTOP WEB` 支持全屏活动视口模式，退出后恢复原分屏。
- [ ] `DV-FUNC-UI-DESKTOP-008` `P0` `DESKTOP WEB` 1920×1080 和 1366×768 下无关键控件遮挡。
- [ ] `DV-FUNC-UI-DESKTOP-009` `P0` `DESKTOP WEB` 属性面板按对象类型提供专属字段：相机、3D Tiles/OSGB/影像/DEM/椭球/点云/glTF；DEM 含参与地形/地形夸张，椭球含经纬网，视口不含相机/地形/经纬网；共享数据影响 N 个视口，本视口显示属性互不影响；不设返回按钮，点视口/标题栏回到视口属性。
- [ ] `DV-FUNC-UI-DESKTOP-010` `P0` `DESKTOP WEB` 属性 + 脚本停靠：右侧属性栏，脚本视口底部；每个面板可关闭；桌面可拖出独立窗口并停靠回来，Web 只能停靠；顶栏属性/脚本按钮重开；全部关闭后右栏消失、脚本底栏消失，顶栏入口保留。v1 没有 AI 面板；脚本为应用级，不绑定活动视口；切换视口、关闭面板或浮窗均不停止脚本；停止按钮或移除脚本结束对应会话，重置结束全部会话。
- [ ] `DV-FUNC-UI-DESKTOP-011` `P0` `DESKTOP WEB MOBILE` 平台边界：Web 下隐藏面板与视口的“独立/分离”入口；移动端单视口不显示脚本/AI 入口；桌面保留独立窗口。

## 25. 移动用户界面

- [ ] `DV-FUNC-UI-MOBILE-001` `P0` `ANDROID IOS` 使用精简顶部工具栏。
- [ ] `DV-FUNC-UI-MOBILE-002` `P0` `ANDROID IOS` 图层列表使用侧边抽屉。
- [ ] `DV-FUNC-UI-MOBILE-003` `P0` `ANDROID IOS` 属性、配准和错误使用底部面板。
- [ ] `DV-FUNC-UI-MOBILE-004` `P0` `ANDROID IOS` 适配安全区域和屏幕 cutout。
- [ ] `DV-FUNC-UI-MOBILE-005` `P0` `ANDROID IOS` 横竖屏自动重排，不丢失正在输入的数据。
- [ ] `DV-FUNC-UI-MOBILE-006` `P0` `ANDROID IOS` 所有主要按钮达到合理触控尺寸。
- [ ] `DV-FUNC-UI-MOBILE-007` `P1` `ANDROID IOS` 虚拟摇杆不遮挡图层和属性操作。
- [ ] `DV-FUNC-UI-MOBILE-008` `P0` `ANDROID IOS` 系统键盘弹出时输入框和确认按钮保持可见。

## 26. 国际化

- [ ] `DV-FUNC-I18N-001` `P0` `ALL` 建立英文源文案与简体中文翻译表。
- [ ] `DV-FUNC-I18N-002` `P0` `ALL` 启动时读取系统语言并选择中/英。
- [ ] `DV-FUNC-I18N-003` `P1` `ALL` 设置中切换语言后界面即时刷新。
- [ ] `DV-FUNC-I18N-004` `P0` `ALL` 未支持语言回退英文。
- [ ] `DV-FUNC-I18N-005` `P0` `ALL` 菜单、按钮、对话框、状态、错误和帮助均不含遗漏的硬编码用户文案。
- [ ] `DV-FUNC-I18N-006` `P1` `ALL` 中英文长文本在桌面和移动布局中不被截断。

## 27. 诊断与隐私

- [ ] `DV-FUNC-DIAG-001` `P1` `ALL` 显示图层状态：未加载、排队、下载、解析、打包、上传、就绪、暂停、部分失败、失败、取消。
- [ ] `DV-FUNC-DIAG-002` `P1` `ALL` 显示请求、等待、Resident、可见、失败内容数量。
- [ ] `DV-FUNC-DIAG-003` `P1` `ALL` 显示 CPU、GPU、源字节、纹理和临时缓存估算。
- [ ] `DV-FUNC-DIAG-004` `P1` `ALL` 显示 FPS、CPU 帧时间、GPU 帧时间和网络吞吐摘要。
- [ ] `DV-FUNC-DIAG-005` `P1` `ALL` 错误项包含图层、阶段、脱敏 URI、错误码和可读建议。
- [ ] `DV-FUNC-DIAG-006` `P1` `ALL` 支持复制单条错误和清空会话日志。
- [ ] `DV-FUNC-DIAG-007` `P1` `DESKTOP ANDROID IOS WEB` 导出脱敏诊断报告。
- [ ] `DV-FUNC-DIAG-008` `P0` `ALL` 自动测试确认报告不包含 Token、Authorization、Cookie 或敏感查询值。
- [ ] `DV-FUNC-PRIVACY-001` `P0` `ALL` 不初始化遥测、分析或广告 SDK。
- [ ] `DV-FUNC-PRIVACY-002` `P0` `ALL` 不保存 URL、本地路径、Token、图层、配准和视点历史。

## 28. 性能与内存控制

- [ ] `DV-FUNC-PERF-001` `P0` `ALL` 全局资源预算器在多个模型、影像和 DEM 图层间分配预算。
- [ ] `DV-FUNC-PERF-002` `P0` `DESKTOP` 默认总 Resident 预算为 1 GiB。
- [ ] `DV-FUNC-PERF-003` `P0` `ANDROID IOS WEB` 默认总 Resident 预算为 384 MiB。
- [ ] `DV-FUNC-PERF-004` `P0` `ALL` 单内容默认大小上限为 64 MiB。
- [ ] `DV-FUNC-PERF-005` `P0` `WIN LINUX MAC ANDROID IOS` 默认网络并发为 8。
- [ ] `DV-FUNC-PERF-006` `P0` `WEB` 默认网络并发为 4。
- [ ] `DV-FUNC-PERF-007` `P0` `ALL` 主线程上传预算默认为每帧 4 ms。
- [ ] `DV-FUNC-PERF-008` `P0` `ALL` 首个超预算候选允许前进，避免永久饥饿。
- [ ] `DV-FUNC-PERF-009` `P0` `ALL` 超预算时自动提高屏幕误差或降低地球/地形细节。
- [ ] `DV-FUNC-PERF-010` `P0` `ALL` 稳态参考场景目标为 60 FPS，95% 帧不超过 20 ms。
- [ ] `DV-FUNC-PERF-011` `P0` `ALL` 大数据首开期间 UI、取消和图层操作保持响应。
- [ ] `DV-FUNC-PERF-012` `P0` `ALL` 反复打开/关闭数据集后内存回到可接受基线。

## 29. 自动化原生测试

- [ ] `DV-TEST-NATIVE-001` `P0` `ALL` WGS84 ↔ ECEF 往返覆盖赤道、中纬度、极点和负高程。
- [ ] `DV-TEST-NATIVE-002` `P0` `ALL` ECEF ↔ ENU 往返及共享原点平移精度。
- [ ] `DV-TEST-NATIVE-003` `P0` `ALL` 日期变更线、极点和非有限输入验证。
- [ ] `DV-TEST-NATIVE-004` `P0` `ALL` 多地理图层共享基后相对位置一致。
- [ ] `DV-TEST-NATIVE-005` `P0` `ALL` 地球 LOD、视锥、地平线和裂缝处理。
- [ ] `DV-TEST-NATIVE-006` `P0` `ALL` XYZ/WMTS 地址、矩阵和边界计算。
- [ ] `DV-TEST-NATIVE-007` `P0` `ALL` WMTS Capabilities 有效、缺失和不支持 CRS 用例。
- [ ] `DV-TEST-NATIVE-008` `P0` `ALL` ion Token 同源限制、重定向和日志脱敏。
- [ ] `DV-TEST-NATIVE-009` `P0` `ALL` UInt16 PNG、Terrain-RGB、Float RAW/Zstd DEM 解码。
- [ ] `DV-TEST-NATIVE-010` `P0` `ALL` GeoTIFF 类型、压缩、Overview、NoData 和非法子集。
- [ ] `DV-TEST-NATIVE-011` `P0` `ALL` 多 DEM 优先级、NoData 回退和夸张。
- [ ] `DV-TEST-NATIVE-012` `P0` `ALL` ZIP 路径穿越、特殊文件、炸弹和入口选择。
- [ ] `DV-TEST-NATIVE-013` `P0` `ALL` 快速重载、取消、删除节点和关闭引擎。
- [ ] `DV-TEST-NATIVE-014` `P0` `ALL` `[基础已有]` Tiled Mesh、3D Tiles、OSG、Draco、USD 现有测试全部通过。
- [ ] `DV-TEST-NATIVE-015` `P0` `ALL` Godot 原生 glTF 运行时导入和外部依赖测试。

## 30. 应用集成测试

- [ ] `DV-TEST-APP-001` `P0` `ALL` 离线启动空视口且无第三方网络请求；添加 WGS84 椭球后可显示空白地球。
- [ ] `DV-TEST-APP-002` `P0` `ALL` 添加、隐藏、显示、暂停、恢复、重载和删除各种图层。
- [ ] `DV-TEST-APP-003` `P0` `ALL` 多个 ECEF 3D Tiles 与地球/影像/DEM 正确对齐。
- [ ] `DV-TEST-APP-004` `P0` `ALL` OSG/glTF/USD 经地理锚点和手工变换正确配准。
- [ ] `DV-TEST-APP-005` `P0` `ALL` 多影像排序、透明度和 Attribution 正确。
- [ ] `DV-TEST-APP-006` `P0` `ALL` 多 DEM 优先级、NoData 回退和夸张正确。
- [ ] `DV-TEST-APP-007` `P1` `ALL` 经纬网、飞往坐标和会话视点正确。
- [ ] `DV-TEST-APP-008` `P0` `ALL` 拾取、Batch 属性和飞往拾取点正确。
- [ ] `DV-TEST-APP-009` `P0` `ALL` 本地目录、ZIP、URL、WMTS 和 ion 入口正确。
- [ ] `DV-TEST-APP-010` `P0` `ALL` 网络、权限、格式、超限和 CORS 错误可理解且可恢复。
- [ ] `DV-TEST-APP-011` `P0` `ALL` 中英文即时切换且布局正常。
- [ ] `DV-TEST-APP-012` `P0` `ANDROID IOS` 自动旋转后图层、输入、相机和对话框状态正常。
- [ ] `DV-TEST-APP-013` `P0` `ALL` 加载中删除图层后无残留任务、临时文件或 GPU 资源。

## 31. 平台冒烟测试

- [ ] `DV-TEST-WIN-001` `P0` `WIN` Windows 11 23H2 完成启动、文件/目录/ZIP、远程加载、USD 和退出测试。
- [ ] `DV-TEST-LINUX-001` `P0` `LINUX` Debian 12 完成 X11/Wayland、Portal、格式和退出测试。
- [ ] `DV-TEST-LINUX-002` `P0` `LINUX` Ubuntu 24.04 LTS 完成同等冒烟测试。
- [ ] `DV-TEST-MAC-001` `P0` `MAC` macOS 14 Intel/Apple Silicon 或等价覆盖完成格式、USD、签名前运行测试。
- [ ] `DV-TEST-ANDROID-001` `P0` `ANDROID` Android 14 真机完成 SAF、触控、旋转、远程和内存测试。
- [ ] `DV-TEST-IOS-001` `P0` `IOS` iOS/iPadOS 17 真机完成 Document Picker、触控、旋转和内存测试。
- [ ] `DV-TEST-WEB-001` `P0` `WEB` Chrome/Edge 完成 threads、文件/目录、CORS 和格式测试。
- [ ] `DV-TEST-WEB-002` `P0` `WEB` Firefox 完成同等测试。
- [ ] `DV-TEST-WEB-003` `P0` `WEB` Safari 完成支持能力检测和可用功能测试。
- [ ] `DV-TEST-PERF-001` `P0` `DESKTOP WEB` 2023 年级六核 CPU、16 GiB RAM、6 GiB GPU、1080p 参考机达到目标。
- [ ] `DV-TEST-PERF-002` `P0` `ANDROID IOS` 2023 年旗舰级、至少 6 GiB RAM 设备达到目标。

## 32. 导出与发布配置

- [ ] `DV-RELEASE-WIN-001` `P0` `WIN` 配置 x86\_64 便携发布，版本、图标和文件信息正确。
- [ ] `DV-RELEASE-LINUX-001` `P0` `LINUX` 配置 x86\_64 发布并提供 AppImage/便携包说明。
- [ ] `DV-RELEASE-MAC-001` `P0` `MAC` 配置 Universal `.app`、依赖 Bundle、签名、公证和 DMG 说明。
- [ ] `DV-RELEASE-ANDROID-001` `P0` `ANDROID` 配置 arm64-v8a APK/AAB、API 34、网络权限和图标。
- [ ] `DV-RELEASE-IOS-001` `P0` `IOS` 配置 arm64 Xcode 导出、iOS 17、自动旋转、Document Picker 和签名说明。
- [ ] `DV-RELEASE-WEB-001` `P0` `WEB` 配置 wasm32 threads、PWA 和 SharedArrayBuffer。
- [ ] `DV-RELEASE-WEB-002` `P0` `WEB` 提供 COOP/COEP Nginx 和静态托管示例。
- [ ] `DV-RELEASE-WEB-003` `P0` `WEB` 部署页在缺少跨源隔离时显示明确诊断。
- [ ] `DV-RELEASE-DOC-001` `P1` `ALL` 完成中英文用户指南。
- [ ] `DV-RELEASE-DOC-002` `P1` `ALL` 完成支持格式、限制、DEM 清单和部署文档。
- [ ] `DV-RELEASE-DOC-003` `P1` `ALL` 完成第三方许可证和隐私声明。

## 33. 多视口与独立窗口

> v1.0 重新基线：移动端单视口；桌面/Web 支持递归分屏；桌面支持独立窗口。添加流程创建独立数据；从视口图层树拖放到另一视口才建立共享数据引用。桌面不再使用全局左侧图层面板。

- [ ] `DV-FUNC-VIEWPORT-001` `P0` `DESKTOP WEB` 从单视口启动，支持左右/上下递归分割、拖动分隔条、关闭视口；不设可见视口数量上限和单视口最小尺寸，性能由预算与自适应降级管理。
- [ ] `DV-FUNC-VIEWPORT-002` `P0` `DESKTOP WEB` 分割只替换目标视口为新的分割节点；相邻视口的位置、尺寸和比例不变；活动视口切换、输入路由、键盘焦点和 `Ctrl+1..9` 正确。
- [ ] `DV-FUNC-VIEWPORT-003` `P0` `ALL` 每个视口拥有独立 World3D、相机、图层栈、选中图层和显示设置；分屏/窗口/移动布局切换不复制或丢失状态。
- [ ] `DV-FUNC-VIEWPORT-004` `P0` `ALL` 添加数据进入活动视口并创建独立数据与图层；对话框显示目标视口；其他视口图层与统计不变。
- [ ] `DV-FUNC-VIEWPORT-005` `P0` `DESKTOP WEB` 图层行可拖放到另一视口建立共享引用；共享数据只下载/解析一次；各视口透明度、顺序、显隐和渲染模式独立；图层显示共享徽标。
- [ ] `DV-FUNC-VIEWPORT-006` `P0` `ALL` 共享数据引用计数正确；移除单个引用不释放，最后一个引用释放 CPU/GPU 资源；重载共享数据前确认并影响全部引用。
- [ ] `DV-FUNC-VIEWPORT-007` `P0` `ALL` 每视口设置完整：相机投影/FOV、渲染模式、经纬网、地形、地形夸张、背景、光照预设、图层顺序/透明度。
- [ ] `DV-FUNC-VIEWPORT-008` `P0` `DESKTOP` 视口可分离为独立窗口；窗口内可添加数据、查看图层、修改设置、停靠回主窗口；关闭/停靠后状态不丢失；窗口数量不设产品上限。
- [ ] `DV-FUNC-VIEWPORT-009` `P1` `DESKTOP WEB` 全屏活动视口只隐藏面板和其他视口，不改变操作系统窗口；退出后恢复原分屏树。
- [ ] `DV-FUNC-VIEWPORT-010` `P0` `ALL` AI 脚本桥/原生脚本是应用级，不隐式绑定活动视口；需要操作某视口时显式指定；切换活动视口不停止脚本；面板不显示目标视口。
- [ ] `DV-FUNC-VIEWPORT-011` `P0` `ALL` 诊断按视口归集并显示共享引用数；错误包含视口标识；全局预算跨视口统一分配。
- [ ] `DV-FUNC-VIEWPORT-012` `P0` `DESKTOP WEB` 视口数量增加时自适应降低非活动视口帧率/LOD/渲染比例；共享数据不重复加载；不因数量或尺寸强制阻止分割。
- [ ] `DV-FUNC-VIEWPORT-013` `P1` `DESKTOP WEB` 极小视口进入紧凑标题栏模式（隐藏次要覆盖层、收拢操作到菜单），仍可分割/关闭/切换；不设最小尺寸。
- [ ] `DV-FUNC-VIEWPORT-014` `P0` `DESKTOP WEB` 自动化/手动覆盖 2/4/6+ 视口、共享/独立数据、拖放、独立窗口停靠、输入路由、分割隔离与资源释放。
- [ ] `DV-FUNC-VIEWPORT-015` `P0` `ANDROID IOS` 移动端保持单视口，不显示分屏/独立窗口入口；横竖屏切换不丢失相机、图层和设置。
- [ ] `DV-FUNC-VIEWPORT-016` `P0` `DESKTOP WEB` 关闭分割节点时只合并该节点的空间；相邻子树不变；最后一个视口不能关闭，只能重置。

## 34. v1.0 发布门槛

- [ ] `DV-GATE-001` 所有 `P0` 实现条目完成。
- [ ] `DV-GATE-002` 六个平台对应的 `P0` 构建和冒烟测试完成。
- [ ] `DV-GATE-003` 没有已知崩溃、数据越界、Token 泄露或不可回收的重大资源泄漏。
- [ ] `DV-GATE-004` 参考硬件达到单视口 60 FPS，且有明确、可接受的多视口 LOD/降帧记录。
- [ ] `DV-GATE-005` 中英文 UI 和错误信息完成审阅。
- [ ] `DV-GATE-006` 4.7.3-rc 编辑器与所有导出模板来自同一源码提交。
- [ ] `DV-GATE-007` 产品图标、公司名、版权、包 ID、隐私和签名资料已经补齐。
- [ ] `DV-GATE-008` 发布说明明确列出格式边界、平台限制和后续 4.7.3 stable 合并策略。
- [ ] `DV-GATE-009` 多视口 `P0` 条目完成；2/4/6+ 视口、分割隔离、共享/独立数据、拖放和独立窗口停靠回归通过。
- [ ] `DV-GATE-010` 多视口性能基线、资源预算和降级策略记录在发布说明中。
