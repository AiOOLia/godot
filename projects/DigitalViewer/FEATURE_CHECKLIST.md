# DigitalViewer v1.0 特性清单

> 文档用途：校准产品范围、平台能力和发布边界。
>
> 最后校准：2026-10-23（多视口与独立窗口重新基线）
>
> 发布基线：定制 Godot 4.7.3-rc；后续单独合并 Godot 4.7.3 stable。

## 使用说明

- `[x]`：产品决策已经确认。
- `[ ]`：仍需产品或技术校准。
- 本文中的勾选表示“规格已确认”，不表示代码已经完成。
- 实现进度、验证结果和验收状态记录在 [FUNCTION\_CHECKLIST.md](FUNCTION_CHECKLIST.md)。
- 平台缩写：`WIN`、`LINUX`、`MAC`、`ANDROID`、`IOS`、`WEB`、`DESKTOP`、`ALL`。

## 1. 产品定义

- [x] `DV-FEAT-PRODUCT-001` 产品正式名称为 **DigitalViewer**。
- [x] `DV-FEAT-PRODUCT-002` 首发版本为 **v1.0.0**。
- [x] `DV-FEAT-PRODUCT-003` 产品定位为跨平台三维与地理空间数据查看器。
- [x] `DV-FEAT-PRODUCT-004` 项目目录为 `projects/DigitalViewer`。
- [x] `DV-FEAT-PRODUCT-005` v1.0 使用当前定制 Godot 4.7.3-rc 发布。
- [x] `DV-FEAT-PRODUCT-006` Godot 4.7.3 stable 发布后，以维护版本形式合并并重新执行完整回归测
- [x] `DV-FEAT-PRODUCT-007 `应用提供简体中文和英文界面，默认跟随系统语言。
- [x] `DV-FEAT-PRODUCT-008 `v1.0 不保存工作空间、最近文件、远程地址、图层列表、配准数据或访问令牌。
- [x] `DV-FEAT-PRODUCT-009 `产品默认包标识使用 `org.digitalviewer.app`，发布前允许替换。
- [ ] `DV-FEAT-PRODUCT-010 `发布主体/公司名称：`待填写`。
- [ ] `DV-FEAT-PRODUCT-011 `最终版权文本：`待填写`。
- [ ] `DV-FEAT-PRODUCT-012 `最终应用图标与启动图：`待提供`。

## 2. 目标平台与兼容范围

### 2.1 发布平台

- [x] `DV-FEAT-PLATFORM-001` Windows x86\_64。
- [x] `DV-FEAT-PLATFORM-002` Linux x86\_64。
- [x] `DV-FEAT-PLATFORM-003` macOS Universal（arm64 + x86\_64）。
- [x] `DV-FEAT-PLATFORM-004` Android arm64-v8a。
- [x] `DV-FEAT-PLATFORM-005` iOS/iPadOS arm64。
- [x] `DV-FEAT-PLATFORM-006` Web wasm32，多线程构建。
- [x] `DV-FEAT-PLATFORM-007` 移动端支持自动旋转，横屏优先优化但不锁定屏幕方向。

### 2.2 最低正式验证系统

> 原则：正式支持 2023 年前后发布、且厂商仍在维护的系统版本及其更新版本；更早版本仅尽力兼容。

| 平台         | 最低正式验证基线                     | 备注                                          |
| ---------- | ---------------------------- | ------------------------------------------- |
| Windows    | Windows 11 23H2              | 更早 Windows 不作为 v1.0 验收条件                    |
| Linux      | Debian 12 / Ubuntu 24.04 LTS | 其他发行版需具备等价 glibc、图形驱动和桌面门户能力                |
| macOS      | macOS 14 Sonoma              | Universal 应用                                |
| Android    | Android 14 / API 34          | arm64-v8a                                   |
| iOS/iPadOS | iOS/iPadOS 17                | arm64                                       |
| Web        | 当前主流浏览器及前两个主要版本              | 必须支持 WebGL 2、Wasm Threads、SharedArrayBuffer |

- [x] `DV-FEAT-PLATFORM-008` Web 部署必须启用 COOP/COEP 跨源隔离。
- [x] `DV-FEAT-PLATFORM-009` Web 远程资源必须满足浏览器 CORS 和 TLS 规则。
- [x] `DV-FEAT-PLATFORM-010` macOS/iOS 正式签名与发布需要 macOS、Xcode、证书和开发者账号。
- [x] `DV-FEAT-PLATFORM-011` Windows、Linux、Android、Web 可以在对应工具链可用时生成发布制品。

## 3. 格式支持矩阵

### 3.1 模型与瓦片格式

| 格式/能力                   | WIN | LINUX | MAC | ANDROID | IOS | WEB |
| ----------------------- | :-: | :---: | :-: | :-----: | :-: | :-: |
| glTF 2.0 `.gltf`        |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| GLB `.glb`              |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| 3D Tiles `tileset.json` |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| B3DM `.b3dm`            |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| I3DM `.i3dm`            |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| PNTS `.pnts`            |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| CMPT `.cmpt`            |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| Draco 压缩 glTF           |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| OSG `.osg`              |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| OSGB `.osgb`            |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| 其它 godot 原生支持模型格式       |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |

### 3.2 地理影像与高程格式

| 格式/服务                    | WIN | LINUX | MAC | ANDROID | IOS | WEB |
| ------------------------ | :-: | :---: | :-: | :-----: | :-: | :-: |
| XYZ 影像                   |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| WMTS 1.0 URL 模板          |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| WMTS 1.0 GetCapabilities |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| Cesium ion 影像            |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| DigitalViewer 预切片 DEM    |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| GeoTIFF DEM              |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |
| COG DEM                  |  ✓  |   ✓   |  ✓  |    ✓    |  ✓  |  ✓  |

## 4. glTF/GLB 特性

- [x] `DV-FEAT-GLTF-001` 独立打开的 glTF/GLB 使用 Godot 自带 `GLTFDocument` 运行时导入能力。
- [x] `DV-FEAT-GLTF-002` 独立 glTF 的材质、纹理、蒙皮、动画、Morph、灯光和扩展支持范围以当前 Godot 4.7.3-rc glTF 模块为准。
- [x] `DV-FEAT-GLTF-003` 支持本地、ZIP、Web 会话文件系统和远程下载后的 glTF/GLB 导入。
- [x] `DV-FEAT-GLTF-004` 保持 `.gltf` 外部 buffer 与纹理的相对路径关系。
- [x] `DV-FEAT-GLTF-005` 导入错误和不支持的 required extension 必须显示在图层错误面板。
- [x] `DV-FEAT-GLTF-006` 3D Tiles 内嵌 glTF/GLB 使用瓦片适配器的静态高性能解析路径，不播放动画或蒙皮。

## 5. 3D Tiles 特性

- [x] `DV-FEAT-3DTILES-001` 支持 3D Tiles 1.0/1.1 显式层级。
- [x] `DV-FEAT-3DTILES-002` 支持外部 tileset、单 content 和 multiple contents。
- [x] `DV-FEAT-3DTILES-003` 支持 ADD 和 REPLACE refinement。
- [x] `DV-FEAT-3DTILES-004` 支持包围球、定向包围盒和 WGS84 region。
- [x] `DV-FEAT-3DTILES-005` 支持 Screen Space Error LOD、视锥裁剪、请求优先级、取消和 LRU。
- [x] `DV-FEAT-3DTILES-006` 支持 glTF/GLB、b3dm、i3dm、pnts、cmpt 和 Draco。
- [x] `DV-FEAT-3DTILES-007` 支持 Batch Table 属性拾取。
- [x] `DV-FEAT-3DTILES-008` 支持普通本地路径、HTTP(S) 和 Cesium ion 3DTILES Asset。
- [x] `DV-FEAT-3DTILES-009` 不支持 implicit tiling；遇到时提供明确错误。
- [x] `DV-FEAT-3DTILES-010` 不支持未知 required extension；不得以不完整几何静默代替。

## 6. OSG/OSGB 特性

- [x] `DV-FEAT-OSG-001` OSG/OSGB 必须在六个平台上可用。
- [x] `DV-FEAT-OSG-002` 支持 ASCII OSG、二进制 OSGB 和 zlib 压缩 OSGB。
- [x] `DV-FEAT-OSG-003` 支持 PagedLOD、ProxyNode 和外部分页资源。
- [x] `DV-FEAT-OSG-004` 支持距离 LOD、屏幕像素 LOD 和父级回退显示。
- [x] `DV-FEAT-OSG-005` 支持基础材质、透明、Alpha Test、双面、顶点颜色和常用图片纹理。
- [x] `DV-FEAT-OSG-006` 支持 Y-Up/Z-Up 选择。
- [x] `DV-FEAT-OSG-007` 使用解析专用静态构建，不依赖 OSG 动态插件和 OSG 自身渲染后端。
- [x] `DV-FEAT-OSG-008` OSG 解析保持全局串行，跨线程只传递 Godot 通用 CPU 数据。
- [x] `DV-FEAT-OSG-009` OSG 动画、粒子、Shader 和任意 ReaderWriter 插件不属于 v1.0。

## 7. 点云特性

- [x] `DV-FEAT-POINTS-001` 支持 3D Tiles PNTS 点云显示。
- [x] `DV-FEAT-POINTS-002` 支持位置、颜色、法线、Batch ID 和批次属性。
- [x] `DV-FEAT-POINTS-003` 支持点拾取。
- [x] `DV-FEAT-POINTS-004` 使用固定、平台适配的默认点渲染策略。
- [x] `DV-FEAT-POINTS-005` v1.0 不提供点大小、点形状、衰减曲线等用户设置。

## 8. USD 特性

- [x] `DV-FEAT-USD-001` v1.0 不支持 USD。

## 9. WGS84 地球与地理参考

- [x] `DV-FEAT-GEO-001` 使用 WGS84 椭球。
- [x] `DV-FEAT-GEO-002` 地理世界内部使用 ECEF 双精度坐标。
- [x] `DV-FEAT-GEO-003` 渲染局部系固定为 X=East、Y=Up、-Z=North。
- [x] `DV-FEAT-GEO-004` 地球、影像、DEM 和地理 3D Tiles 共用同一 ECEF 原点及 ENU 基。
- [x] `DV-FEAT-GEO-005` 支持经纬高、ECEF 和 Godot 局部坐标之间的双向转换。
- [x] `DV-FEAT-GEO-006` 相机离开局部原点阈值后执行无跳动的浮动原点重定位。
- [x] `DV-FEAT-GEO-007` 支持 EPSG:4326 和 EPSG:3857。
- [x] `DV-FEAT-GEO-008` 不引入任意 EPSG/PROJ 数据库。
- [x] `DV-FEAT-GEO-009` 地球使用四叉树、屏幕误差、视锥和地平线遮挡进行瓦片选择。
- [x] `DV-FEAT-GEO-010` 使用裙边或等效方案处理相邻地形 LOD 裂缝。
- [x] `DV-FEAT-GEO-011` 视口默认无数据；添加 WGS84 椭球数据后才创建空白地球，支持三维球以及哥伦布两种渲染方式。

## 10. 经纬网格、坐标飞行和视点

- [x] `DV-FEAT-NAV-001` 提供可开关的经纬网格。
- [x] `DV-FEAT-NAV-002` 经纬网间距随相机高度动态变化。
- [x] `DV-FEAT-NAV-003` 支持经纬度标签，并避免过密重叠。
- [x] `DV-FEAT-NAV-004` 经纬网贴合椭球或当前 DEM 地表，并避免 Z-Fighting。
- [x] `DV-FEAT-NAV-005` 支持输入经度、纬度、观察高度以及可选 Heading/Pitch 后平滑飞往。
- [x] `DV-FEAT-NAV-006` 支持取消正在执行的飞行动画。
- [x] `DV-FEAT-NAV-007` 支持会话级视点的添加、命名、飞往和删除。
- [x] `DV-FEAT-NAV-008` 会话视点记录经纬高、Heading、Pitch 和观察距离。
- [x] `DV-FEAT-NAV-009` 会话视点不持久化，退出应用后清空。

## 11. 影像底图

- [x] `DV-FEAT-IMAGERY-001` 支持多个影像图层同时叠加。
- [x] `DV-FEAT-IMAGERY-002` 每层支持名称、显隐、透明度、排序、状态、错误和 Attribution。
- [x] `DV-FEAT-IMAGERY-003` 使用普通 Alpha Over 混合，不提供额外混合模式。
- [x] `DV-FEAT-IMAGERY-004` 支持 PNG、JPEG、WebP。
- [x] `DV-FEAT-IMAGERY-005` 支持 EPSG:3857 XYZ。
- [x] `DV-FEAT-IMAGERY-006` 支持 EPSG:4326/WorldCRS84Quad。
- [x] `DV-FEAT-IMAGERY-007` 支持 `{z}`、`{x}`、`{y}`、`{s}` URL 模板参数。
- [x] `DV-FEAT-IMAGERY-008` 支持 XYZ/TMS Y 轴选择和层级范围配置。
- [x] `DV-FEAT-IMAGERY-009` 支持 WMTS 1.0 URL 模板。
- [x] `DV-FEAT-IMAGERY-010` 支持 WMTS GetCapabilities，并选择 Layer、Style、Format、TileMatrixSet。
- [x] `DV-FEAT-IMAGERY-011` 支持 WMTS RESTful 与 KVP 请求。
- [x] `DV-FEAT-IMAGERY-012` v1.0 不支持 WMTS 时间等 Dimension。
- [x] `DV-FEAT-IMAGERY-013` 支持 Cesium ion 影像 Asset 和服务 Attribution。

## 12. DEM 与地形

- [x] `DV-FEAT-DEM-001` 支持多个 DEM 同时启用。
- [x] `DV-FEAT-DEM-002` DEM 按图层列表从上到下覆盖。
- [x] `DV-FEAT-DEM-003` 上层 NoData 区域回退到下层。
- [x] `DV-FEAT-DEM-004` 多 DEM 边界采用硬优先级切换，不进行高程平滑融合。
- [x] `DV-FEAT-DEM-005` 每层支持 scale、offset、垂直偏移、NoData、层级和边界。
- [x] `DV-FEAT-DEM-006` 所有 DEM 高程解释为 WGS84 椭球高，不内置 EGM96。
- [x] `DV-FEAT-DEM-007` 支持全局地形夸张，默认 `1.0`。
- [x] `DV-FEAT-DEM-008` 地形夸张 UI 范围为 `0–10`，数值输入允许 `0–100`。
- [x] `DV-FEAT-DEM-009` 夸张应用于合成后的地表高程，不自动变形三维模型。

### 12.1 预切片 DEM

- [x] `DV-FEAT-DEM-TILED-001` 使用版本化 JSON 清单。
- [x] `DV-FEAT-DEM-TILED-002` 支持本地目录、ZIP 和 HTTP(S)。
- [x] `DV-FEAT-DEM-TILED-003` 支持 `uint16_png`。
- [x] `DV-FEAT-DEM-TILED-004` 支持 Mapbox 标准 `terrain_rgb`，并允许清单覆盖 scale/offset。
- [x] `DV-FEAT-DEM-TILED-005` 支持小端 `float32_le` RAW。
- [x] `DV-FEAT-DEM-TILED-006` Float RAW 支持未压缩和 Zstandard 压缩。

### 12.2 GeoTIFF/COG

- [x] `DV-FEAT-GEOTIFF-001` 使用 libtiff 实现 DEM 专用子集。
- [x] `DV-FEAT-GEOTIFF-002` 支持经典 TIFF、BigTIFF、条带和瓦片布局。
- [x] `DV-FEAT-GEOTIFF-003` 支持单波段 UInt16、Int16、Float32。
- [x] `DV-FEAT-GEOTIFF-004` 支持无压缩、LZW、DEFLATE。
- [x] `DV-FEAT-GEOTIFF-005` 支持内部概览、NoData、PixelIsArea、PixelIsPoint。
- [x] `DV-FEAT-GEOTIFF-006` 仅支持北向上的 EPSG:4326/3857。
- [x] `DV-FEAT-GEOTIFF-007` 远程 GeoTIFF/COG 允许整文件下载，不强制 HTTP Range。
- [x] `DV-FEAT-GEOTIFF-008` 默认远程文件上限为桌面 512 MiB、移动/Web 128 MiB。
- [x] `DV-FEAT-GEOTIFF-009` 超限时暂停并提示风险，允许用户仅对当前会话确认继续。

## 13. 图层与配准

- [x] `DV-FEAT-LAYER-001` 支持地球、影像、DEM、3D Tiles、OSG/OSGB、glTF/GLB。
- [x] `DV-FEAT-LAYER-002` 支持添加、删除、显示、隐藏、聚焦、暂停、恢复和重载。
- [x] `DV-FEAT-LAYER-003` 支持透明度、加载状态、错误、警告和资源统计。
- [x] `DV-FEAT-LAYER-004` 影像与 DEM 图层支持排序。
- [x] `DV-FEAT-LAYER-005` 删除图层时取消全部任务并释放 GPU 资源。
- [x] `DV-FEAT-LAYER-006` ECEF/region 3D Tiles 自动加入共享地理参考。
- [x] `DV-FEAT-LAYER-007` 大型笛卡尔数据不会仅凭数值自动判断为 ECEF，用户可以显式强制。
- [x] `DV-FEAT-LAYER-008` 非地理模型支持局部场景和经纬高锚点两种配准模式。
- [x] `DV-FEAT-LAYER-009` 手工配准支持经纬高、ENU 偏移、Yaw/Pitch/Roll、源原点和 Y-Up/Z-Up。
- [x] `DV-FEAT-LAYER-010` 支持统一比例与 XYZ 非统一比例。
- [x] `DV-FEAT-LAYER-011` 支持数值面板与场景 Gizmo 调整。

## 14. 相机、显示与拾取

- [x] `DV-FEAT-CAMERA-001` 支持轨道相机与飞行相机。
- [x] `DV-FEAT-CAMERA-002` 桌面/Web 支持鼠标旋转、平移、滚轮缩放和 WASD 飞行。
- [x] `DV-FEAT-CAMERA-003` 移动端支持单指旋转、双指缩放/平移、双击聚焦、长按拾取和虚拟摇杆。
- [x] `DV-FEAT-CAMERA-004` 根据观察距离自动调整移动速度及近远裁剪面。
- [x] `DV-FEAT-CAMERA-005` 防止相机意外进入椭球或地形以下。
- [x] `DV-FEAT-RENDER-001` 支持 Textured、Unlit 和 Wireframe 模式。
- [x] `DV-FEAT-RENDER-002` 支持图层透明度和阴影开关。
- [x] `DV-FEAT-RENDER-003` 使用 Compatibility 渲染器作为六端一致性基线。
- [x] `DV-FEAT-PICK-001` 支持三角形和点云点拾取。
- [x] `DV-FEAT-PICK-002` 拾取结果包含图层、URI、局部位置、ECEF、经纬高、法线、距离和几何标识。
- [x] `DV-FEAT-PICK-003` 3D Tiles 拾取显示 Batch ID 和属性。
- [x] `DV-FEAT-PICK-004` 显示拾取位置标记，并可飞往拾取点。

## 15. 数据入口与网络

- [x] `DV-FEAT-IO-001` Windows/Linux/macOS 支持原生文件和目录选择及拖放。
- [x] `DV-FEAT-IO-002` Android 使用 SAF 选择文件和目录。
- [x] `DV-FEAT-IO-003` iOS 使用 `UIDocumentPicker` 打开单文件和 ZIP。
- [x] `DV-FEAT-IO-004` Web 支持文件、目录输入和拖放，并复制到会话文件系统。
- [x] `DV-FEAT-IO-005` 所有平台支持安全 ZIP 数据包。
- [x] `DV-FEAT-IO-006` ZIP 拒绝路径穿越、绝对路径、符号链接逃逸、异常压缩比及超限数量/体积。
- [x] `DV-FEAT-IO-007` ZIP 内存在多个根文件时显示入口选择器。
- [x] `DV-FEAT-NET-001` 支持 HTTP/HTTPS、相对资源、重定向、超时、重试、取消和相同请求合并。
- [x] `DV-FEAT-NET-002` 默认超时 30 秒，默认重试 2 次。
- [x] `DV-FEAT-NET-003` 不绕过 TLS 错误，不接受自签名证书。
- [x] `DV-FEAT-NET-004` v1.0 不提供应用级代理配置。
- [x] `DV-FEAT-NET-005` 普通远程源支持会话级自定义 Header，敏感值必须从日志中清除。
- [x] `DV-FEAT-NET-006` 支持 Cesium ion Asset ID、Token、端点解析和 Attribution。

## 16. 用户界面与国际化

- [x] `DV-FEAT-UI-001` 桌面布局包含顶部工具栏、每视口图层树、中央视口区域（单视口或分屏）、右侧属性和底部状态/错误面板。
- [x] `DV-FEAT-UI-002` 桌面右栏可关闭、拖出独立窗口、停靠回来并调整尺寸；每视口图层树是无背景内联层级树（顶层分组→图层），只有树按钮能收起，多视口可同时展开；支持全屏活动视口。
- [x] `DV-FEAT-UI-003` 移动端使用精简工具栏、侧边抽屉和底部属性面板。
- [x] `DV-FEAT-UI-004` 移动端适配安全区、刘海、横竖屏和触控尺寸。
- [x] `DV-FEAT-UI-005` 桌面右侧属性面板按选中对象切换：视口（布局/环境/性能）、相机、3D Tiles、OSGB、影像、DEM、椭球、点云、glTF 各自独立字段，不混排；视口不含相机/地形/经纬网，DEM 拥有地形夸张，椭球拥有经纬网；共享数据与“本视口显示”分区。
- [x] `DV-FEAT-UI-006` 桌面/Web 属性 + 脚本：右侧属性栏，脚本在视口底部；每个面板可关闭；桌面可拖出独立窗口并停靠回来，Web 只能停靠；右栏和底部关闭后自动释放空间；顶栏按钮可重开。v1 没有 AI 面板；脚本为应用级，不绑定活动视口。
- [x] `DV-FEAT-UI-007` 平台边界：多视口版本用于桌面 + Web，Web 隐藏面板/视口的独立窗口入口；单视口版本只用于移动端，移动端不提供脚本与 AI。
- [x] `DV-FEAT-I18N-001` 支持简体中文和英文。
- [x] `DV-FEAT-I18N-002` 默认跟随系统语言，允许运行时即时切换。
- [x] `DV-FEAT-I18N-003` 未匹配语言回退英文。
- [x] `DV-FEAT-I18N-004` 界面、帮助、平台限制和错误均进入翻译表。

## 17. 运行时脚本与外部 AI

> **v1.0 范围：不内置 AI。** 本节的 `DV-FEAT-AI-*` 是 v1.1 候选，不进入 v1.0 交付；v1.0 只交付脚本工作台、运行时脚本 API 和外部 AI 的本机 MCP。外部 AI 只能操作运行时场景，不能修改产品。
>
> v1.0 的 AI 与脚本面向 DigitalViewer 应用运行时，采用单进程方案，不依赖外部 Godot Editor 或工程文件修改；发布包仍包含 Godot 引擎运行时。AI 只通过 GDScript 脚本与场景交互，不生成/读取 JSON 场景数据；AI 草案进入脚本面板后必须由用户审查、确认信任并手动运行。现有验证基于 Windows 普通应用运行模式，正式导出包和其他平台仍待验收。

### 17.1 AI 聊天与模型服务

- [ ] `DV-FEAT-AI-001` 提供运行时 AI 场景助手，支持本地规则演示和真实模型两种模式。
- [ ] `DV-FEAT-AI-002` 支持 OpenAI Chat Completions 兼容接口；基础地址可自动补全 `/v1/chat/completions`，模型名称由用户配置。
- [ ] `DV-FEAT-AI-003` 支持可选访问凭据、异步非流式请求、连接测试、请求进度、超时、取消和协议错误提示。
- [ ] `DV-FEAT-AI-004` 访问凭据、连接设置和聊天内容仅保留在当前会话内，不写入项目或发布包。
- [ ] `DV-FEAT-AI-005` 支持工具调用续轮，要求服务支持 `messages`、`tools`、`tool_choice`、`tool_calls` 和 `role=tool`。
- [ ] `DV-FEAT-AI-006` 聊天上下文按轮次、UTF-8 字节数和摘要条数进行有界管理；新聊天和切换连接可清空当前上下文。
- [ ] `DV-FEAT-AI-007` 每轮模型工具调用有次数、请求体和结果配对限制；迟到响应、重复调用 ID 和过期脚本版本不得覆盖新状态。
- [ ] `DV-FEAT-AI-008` 模型请求包含聊天消息及必要的场景摘要、相机状态和执行事实；应用不自动附带源文件、原始资源 URL 或截图。访问凭据仅用于服务认证，不写入提示词；用户自行输入的敏感信息仍会随聊天发送。

### 17.2 AI 脚本桥（GDScript，取代 JSON 场景代理）

- [ ] `DV-FEAT-AI-009` AI 通过 GDScript 与场景交互，不生成/读取 JSON 场景数据，不引入 SceneProgram/1 或等价 JSON 场景模型。
- [ ] `DV-FEAT-AI-010` AI 的每次修改先生成 GDScript 草案卡片（请求摘要、代码、待审查状态）；查看/审查只把代码放入脚本面板，不自动运行。
- [ ] `DV-FEAT-AI-011` AI 生成的脚本通过应用级 `viewer` API 访问视口、相机、图层、背景、环境光、方向光和对象；API 有注册表、类型/范围校验和错误定位。
- [ ] `DV-FEAT-AI-012` AI 草案必须先编译/静态检查；运行前由用户确认信任；运行输出、错误、调用栈和脚本版本在脚本面板可见。
- [ ] `DV-FEAT-AI-013` AI 脚本草案携带脚本版本和调用 ID 去重；手动修改或新草案后拒绝旧响应，避免覆盖新代码。
- [ ] `DV-FEAT-AI-014` AI 与脚本是应用级，不隐式绑定当前活动视口；需要操作视口时由脚本显式获取；每个视口的场景、相机、图层与设置相互独立。
- [ ] `DV-FEAT-AI-015` AI 可通过脚本 API 控制当前已开放的地球、经纬网、标记、背景、环境光、方向光和相机对象，并创建受支持的分组、球体、立方体、圆柱和灯光对象。
- [ ] `DV-FEAT-AI-016` 提供多模型管理：可添加模型并配置显示名称、提供方、Base URL、模型 ID、凭据和上下文窗口，可在 AI 面板选择当前模型；凭据只保留在当前会话内存。
- [ ] `DV-FEAT-AI-017` 应用内 AI 是场景 Copilot：只修改当前运行时场景，不修改工程、文件、依赖或构建，不提供终端/仓库能力。
- [ ] `DV-FEAT-AI-018` AI 脚本运行前必须通过信任门：编译/静态检查、权限、差异、检查点、同步死循环警告和用户确认；草案不自动执行。
- [ ] `DV-FEAT-AI-019` 移动端 AI 只读：不执行脚本、不修改场景，脚本修改指向桌面/Web 脚本面板。

### 17.3 原生 GDScript 工作台

- [x] `DV-FEAT-SCRIPT-001` 在桌面/Web 提供应用内原生 GDScript 编辑、保存、打开和运行入口；移动端不提供脚本面板；不要求用户安装或打开 Godot Editor。
- [x] `DV-FEAT-SCRIPT-002` 使用运行时 `CodeEdit` 和高亮器提供行号、折叠、括号补全、查找、常用关键字提示和基础语法高亮。
- [x] `DV-FEAT-SCRIPT-003` 原生脚本必须继承 `Node` 或其子类并提供 `run(viewer)` 入口；支持 `_process`、信号、材质、相机、环境和创建网格等 Godot 运行时能力。
- [x] `DV-FEAT-SCRIPT-004` 脚本可以通过运行时上下文访问真实场景根、地球、经纬网、相机、环境和本次 `ScriptSession`。
- [x] `DV-FEAT-SCRIPT-005` 提供首次运行及代码变化后的信任确认；本次应用会话内同一文档已确认且代码未变时直接重跑。提供编译/运行错误、源码行号、可用调用栈、输出日志、只读场景树和只读属性检查。
- [x] `DV-FEAT-SCRIPT-006` 每个视口拥有独立的 `World3D`；桌面/Web Shell 显示当前活动视口，脚本创建的对象、材质和相机状态不会因窗口或布局切换而分裂。
- [x] `DV-FEAT-SCRIPT-007` 原生脚本是应用级会话，不隐式接管活动视口；需要时显式指定视口。切换活动视口、关闭脚本面板或浮窗不停止/重置脚本；停止或移除脚本结束对应会话，重置场景结束全部会话。
- [x] `DV-FEAT-SCRIPT-008` 脚本文件保存在应用用户目录 `user://scripts`，默认不覆盖已有文件；当前不保存工程文件和发布包内容。
- [x] `DV-FEAT-SCRIPT-009` v1.0 明确不承诺完整 Godot ScriptEditor、原生断点、单步、暂停时局部变量和安全沙箱；AI 不自动执行原生 GDScript。
- [x] `DV-FEAT-SCRIPT-010` 原生脚本在主线程执行，不提供同步死循环强制终止、执行预算或看门狗；代码阻塞主线程时，工作台停止按钮也无法响应。
- [x] `DV-FEAT-SCRIPT-011` 停止只清理本次脚本会话及其后代，重置只重建当前 3D 场景；不承诺撤销文件、网络、应用单例或场景外副作用，原生脚本不具备事务回滚保证。
- [x] `DV-FEAT-SCRIPT-013` “添加数据”支持 GDScript 脚本来源：校验 `.gd` 的 `extends Node` 与 `run(viewer)` 入口，创建“脚本”图层并打开脚本面板；脚本是运行时行为，不改产品。

实现细节见 [原生脚本工作台](docs/native-script-workbench.md)；AI 通过脚本桥接入，不经过 JSON 场景代理。

### 17.4 自动化 API / 外部 AI

- [x] `DV-FEAT-API-001` 提供本机 MCP `/mcp` 与 Plugin Bridge `/bridge`，默认只监听 loopback，权限分为 `scene.read` / `script.run` / `capture`。
- [x] `DV-FEAT-API-002` Codex、Harness、CLI 和 CI 通过 MCP 或 Plugin Bridge 使用同一套脚本 API、权限、检查点和审计；不建立第二套运行时。
- [x] `DV-FEAT-API-003` 自动化工具只包含 `scene.describe`、`viewport.list`、`script.inspect`、`script.propose`、`script.run`、`capture.screenshot`、`audit.list`；不提供 shell、文件系统、网络或 JSON 场景写接口。
- [x] `DV-FEAT-API-004` 设置页可在 MCP / Plugin Bridge 间切换（互斥），显示端点、权限、复制令牌和限制说明；Web 不暴露本机桥接。

## 18. 性能与资源预算

- [x] `DV-FEAT-PERF-001` 全平台单视口目标帧率为 60 FPS；多视口时活动视口目标 60 FPS，非活动视口允许 30 FPS 或按需更新。
- [x] `DV-FEAT-PERF-002` 稳定浏览时单视口目标帧时间为 16.7 ms；验收参考为 95% 帧不超过 20 ms。多视口场景单独记录降级后的基线。
- [x] `DV-FEAT-PERF-003` 负载过高时优先降低地球、地形和 3D Tiles LOD，不持续阻塞主线程。
- [x] `DV-FEAT-PERF-004` 桌面默认总 Resident 预算为 1 GiB。
- [x] `DV-FEAT-PERF-005` Android/iOS/Web 默认总 Resident 预算为 384 MiB。
- [x] `DV-FEAT-PERF-006` 单内容默认大小上限为 64 MiB。
- [x] `DV-FEAT-PERF-007` 桌面/移动默认网络并发为 8，Web 为 4。
- [x] `DV-FEAT-PERF-008` 每帧主线程资源上传预算默认为 4 ms。
- [x] `DV-FEAT-PERF-009` 文件读取、网络、解析、图片解码、网格打包和地形构网在后台执行。
- [x] `DV-FEAT-PERF-010` Godot Resource、场景对象和 RenderingServer RID 只在主线程创建或修改。
- [x] `DV-FEAT-PERF-011` 所有异步任务携带 owner、generation 和 request ID，过期结果必须丢弃。

## 19. 安全、隐私与诊断

- [x] `DV-FEAT-SEC-001` 不收集遥测，不集成第三方分析。
- [x] `DV-FEAT-SEC-002` 不保存 Token、URL 历史、本地路径历史、图层、配准或视点。
- [x] `DV-FEAT-SEC-003` Token、Authorization 和敏感查询参数不得进入日志或诊断报告。
- [x] `DV-FEAT-SEC-004` Cesium 资产 Token 只发送给允许的 HTTPS 资产源。
- [x] `DV-FEAT-SEC-005` 退出时清理会话临时文件。
- [x] `DV-FEAT-DIAG-001` 每层显示排队、下载、解析、打包、上传、Resident、暂停和失败状态。
- [x] `DV-FEAT-DIAG-002` 显示瓦片、实例、请求、CPU/GPU 估算内存和失败数量。
- [x] `DV-FEAT-DIAG-003` 支持复制和导出脱敏后的会话诊断报告。

## 20. 明确不属于 v1.0

- [x] `DV-FEAT-OUT-001` 不支持 USD。
- [x] `DV-FEAT-OUT-002` 不支持 IFC、STEP、IGES 等 CAD/BIM 格式。
- [x] `DV-FEAT-OUT-003` 不支持 LAS/LAZ、Shapefile、CityGML、CityJSON。
- [x] `DV-FEAT-OUT-004` 不提供通用任意 EPSG 重投影。
- [x] `DV-FEAT-OUT-005` 不提供 EGM96 或其他 geoid 自动转换。
- [x] `DV-FEAT-OUT-006` 不提供模型、属性或源文件编辑与回写。
- [x] `DV-FEAT-OUT-007` 不提供工作空间持久化和云同步。
- [x] `DV-FEAT-OUT-008` 不提供持久化网络瓦片缓存。
- [x] `DV-FEAT-OUT-009` 不提供大气散射、云层、海洋模拟和天文行星系统。
- [x] `DV-FEAT-OUT-010` 不提供路径追踪、完整材质编辑器和模型剖切。

## 21. 交付物

- [x] `DV-FEAT-DELIVERY-001` 完整 DigitalViewer Godot 项目。
- [x] `DV-FEAT-DELIVERY-002` 定制引擎源码、第三方依赖登记和模块文档。
- [x] `DV-FEAT-DELIVERY-003` 与当前 4.7.3-rc 源码提交匹配的编辑器与导出模板构建配置。
- [x] `DV-FEAT-DELIVERY-004` Windows、Linux、macOS、Android、iOS、Web 导出预设。
- [x] `DV-FEAT-DELIVERY-005` Windows 便携包配置。
- [x] `DV-FEAT-DELIVERY-006` Linux 便携包/AppImage 打包说明。
- [x] `DV-FEAT-DELIVERY-007` macOS `.app`/DMG、签名和公证说明。
- [x] `DV-FEAT-DELIVERY-008` Android APK/AAB 配置。
- [x] `DV-FEAT-DELIVERY-009` iOS Xcode、签名和 Archive 说明。
- [x] `DV-FEAT-DELIVERY-010` Web PWA、COOP/COEP、CORS、Nginx 和静态托管示例。
- [x] `DV-FEAT-DELIVERY-011` 中英文用户说明、格式限制和诊断指南。

## 22. 待填写的发布资料

- [ ] `DV-CALIBRATE-001` 公司/组织名称。
- [ ] `DV-CALIBRATE-002` 版权年份和版权持有人。
- [ ] `DV-CALIBRATE-003` 正式包 ID（若不使用 `org.digitalviewer.app`）。
- [ ] `DV-CALIBRATE-004` 应用图标、移动端自适应图标和启动图。
- [ ] `DV-CALIBRATE-005` 隐私政策和第三方服务声明文本。
- [ ] `DV-CALIBRATE-006` Apple/Android 商店账号、签名证书与描述资料。

## 23. 多视口与独立窗口（v1.0 重新基线）

> 2026-10-23 起纳入 v1.0。移动端保持单视口；桌面/Web 支持分屏；桌面支持独立窗口。添加流程始终创建独立数据；只有从视口图层树拖放到另一个视口时才建立共享数据引用。桌面不再使用全局左侧图层面板。

- [x] `DV-FEAT-VIEWPORT-001` 桌面与 Web 从单视口开始，支持左右（水平）/上下（垂直）递归分割；移动端保持单视口。
- [x] `DV-FEAT-VIEWPORT-002` 不设可见视口数量上限和单视口最小尺寸；分割只作用于目标视口，其他视口布局与尺寸不变；性能由全局预算、LOD 和自适应降级管理，不阻止继续分割。
- [x] `DV-FEAT-VIEWPORT-003` 每个视口拥有独立的 `World3D`、相机、图层栈、选中图层、显示模式、经纬网/地形开关、地形夸张、背景和光照。
- [x] `DV-FEAT-VIEWPORT-004` 提供活动视口；点击视口或 `Ctrl+1..9` 切换；活动视口用 accent 边框和居中视口名标识。
- [x] `DV-FEAT-VIEWPORT-005` 添加数据始终进入活动视口并创建独立数据与图层，不影响其他视口。
- [x] `DV-FEAT-VIEWPORT-006` 从活动视口图层树拖放图层到另一视口时创建共享数据引用；底层数据只下载/解析一次；各视口的显示设置相互独立。
- [ ] `DV-FEAT-VIEWPORT-007` 共享数据使用引用计数；移除单个引用不释放数据，最后一个引用移除时释放；重载共享数据影响全部引用并需确认。
- [x] `DV-FEAT-VIEWPORT-008` 每个视口独立设置相机投影/FOV、渲染模式、经纬网、地形、地形夸张、背景、光照预设、图层顺序与透明度。
- [x] `DV-FEAT-VIEWPORT-009` 桌面端支持把视口分离为独立窗口；独立窗口拥有精简视口工具栏，可添加数据、查看图层、修改设置、停靠回主窗口或关闭；独立窗口数量不设产品上限。
- [ ] `DV-FEAT-VIEWPORT-010` 独立窗口关闭时默认停靠回主窗口；显式“关闭视口”才销毁；关闭含会话数据的视口需确认。
- [x] `DV-FEAT-VIEWPORT-011` 全屏视口仅最大化活动视口，不改变操作系统窗口；退出全屏恢复原分屏树。
- [x] `DV-FEAT-VIEWPORT-012` AI 与原生脚本是应用级，不隐式绑定活动视口；需要操作某视口时显式指定；同一时间只有一个运行会话，切换活动视口不停止脚本。
- [x] `DV-FEAT-VIEWPORT-013` 诊断按视口归集，并显示共享数据的引用数；全局 Resident 预算跨视口统一分配。
- [x] `DV-FEAT-VIEWPORT-014` 视口数量增加时自适应降低非活动视口帧率、LOD 和渲染比例；性能提示不阻止继续分割，也不强制关闭视口。
- [x] `DV-FEAT-VIEWPORT-015` 分屏布局、活动视口、独立窗口和共享引用均为会话状态，不持久化。
- [x] `DV-FEAT-VIEWPORT-016` Web 只支持页内分屏，不支持独立窗口；Android/iOS 不提供分屏与独立窗口入口。
