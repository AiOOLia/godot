# 3D Tiles 模块

通过已有 `TiledMeshInstance3D` 加载 3D Tiles，格式 ID 为 `3dtiles`。模块只依赖
`tiled_mesh_3d`，不依赖 OSG、Cesium SDK 或编辑器 glTF 导入器。

## 构建

```powershell
scons platform=windows target=editor dev_build=yes tests=yes module_3dtiles_enabled=yes accesskit=no d3d12=no -j8
```

SCons 自动发现模块；可用 `module_3dtiles_enabled=no` 禁用。需要线程与 3D，平台条件
与 `tiled_mesh_3d` 一致（Windows、LinuxBSD、Web）。本次验证平台为 Windows x86_64。

## 使用

将以下脚本挂到场景中的 `Node3D`，场景内放置激活的 `Camera3D` 和光源：

```gdscript
extends Node3D

func _ready() -> void:
    var tiles := TiledMeshInstance3D.new()
    tiles.auto_load = false
    tiles.source_uri = "E:/Data/City/tileset.json" # 也可以是 res://、user:// 或 HTTP(S)
    tiles.format_id = &"3dtiles" # 可以省略，根数据可自动识别
    tiles.maximum_screen_space_error = 16.0
    tiles.load_failed.connect(func(uri, message): push_error(uri + ": " + message))
    tiles.tile_failed.connect(func(uri, message): push_error(uri + ": " + message))
    add_child(tiles)
    var result := tiles.open()
    if result != OK:
        push_error(error_string(result))
```

也可以直接在编辑器创建 `TiledMeshInstance3D`，设置 `source_uri`。
`root_loaded` 表示根描述已加载；模型内容继续异步流式加载，使用 `tile_loaded`、
`tile_failed` 和 `get_streaming_stats()` 观察状态。`close()` 取消任务并释放网格。
导出项目时需要将原始 JSON、GLB、glTF、BIN、b3dm 和纹理文件加入非资源导出过滤器，
或使用项目外部数据目录/HTTP 服务。

## Cesium ion

`Tiles3DOptions.ion_asset_id > 0` 启用 ion 并覆盖 `source_uri`；设为 0 恢复本地/普通 HTTP 加载。
通过官方 `https://api.cesium.com/v1/assets/{id}/endpoint` 异步获取托管资产入口，再流式加载：

### 编辑器配置（无需脚本）

1. 使用包含 `3dtiles` 与 `tiled_mesh_3d` 模块的 **editor** 构建打开项目，创建 3D 场景。
   在场景树点击 **添加节点（Ctrl+A）**，搜索并添加 `TiledMeshInstance3D`。这是内置模块，
   无需在项目的插件设置中启用插件。
2. 选中节点，在 **Inspector → Data → Format Options** 的资源下拉菜单选择
   **New Tiles3DOptions**，再点击资源展开属性。设置 **Ion Asset ID** 为实际资产 ID，
   **Ion Access Token** 为具有该资产读取权限的 Token。字段是现有资源属性，无需挂载脚本。
3. **Source URI 留空即可**；非零 Ion Asset ID 会覆盖它。**Format ID** 可留空自动识别，
   或填写 `3dtiles`。坐标通常保留 **Coordinate System = Auto**，
   节点 **Coordinates → World Origin Mode = Format Default**。
4. 节点 **Editor Preview → Enabled** 与 **Data → Auto Load** 默认开启，配置完整后会自动预览；
   如已关闭，配置完成后重新勾选。若想避免填写期间发起请求，可先取消 Enabled。
   也可点击 Inspector 顶部 **Open**（自动启用预览，即使 Auto Load 关闭也能手动打开）。
   不需要运行场景，也不需要为编辑器预览添加 Camera3D：内置插件使用 3D 编辑器视口相机。
5. 根包围体加载后点击 **Frame Dataset** 定位，使用 **Show Bounds** 辅助观察。
   按需添加 DirectionalLight3D，或启用编辑器的预览光照。Inspector 顶部显示 State、Resident、
   Requested、Failed 和内存等统计；**Pause Preview / Resume Preview** 控制流式更新，
   **Reload** 重新获取入口，**Unload** 释放当前预览。
6. 已加载时修改资源属性会自动关闭旧会话并延迟重开，不需要额外点击 Reload。
   未加载时，只有 Auto Load 与编辑器预览都启用才会自动打开；否则点击 Open。
   Unload 不会关闭预览开关，后续属性修改仍可能自动加载；要持续停止预览，请取消 Enabled。
7. 使用 **Ctrl+S** 保存 `.tscn`，选项默认作为场景子资源保存；也可通过资源菜单 **Save As…**
   保存为 `.tres` 再引用。预览开关也会保存；如果不希望下次打开场景自动联网，保存前关闭预览。
   **Token 会以明文保存在场景/资源中，密码框不等于加密**，请仅使用最小权限 Token，
   不要将含 Token 的文件提交版本库或公开分发。运行场景时需另设激活的 Camera3D 和光照。

未填写数据源或 ion 配置不完整时，场景树节点显示配置警告；填好 ID 与 Token 后，
空 Source URI 不会产生该警告。HTTP 401/403、无权访问资产或不支持的格式可能在异步加载后
表现为 Failed；先检查资产类型、Token 权限与网络，修正后点击 Reload。当前面板只显示状态统计，
并未提供完整的服务端错误详情或 ion 登录/资产浏览器。

### 脚本配置（可选）

```gdscript
var tiles := TiledMeshInstance3D.new()
tiles.auto_load = false
var options := Tiles3DOptions.new()
options.ion_asset_id = 123456 # 替换为你的资产 ID
options.ion_access_token = OS.get_environment("CESIUM_ION_ACCESS_TOKEN")
tiles.format_options = options
add_child(tiles)
var error := tiles.open()
```

将示例中的 `123456` 替换为实际资产 ID。Token 需要该资产的读取权限，建议限定资产与最小权限。
密码属性只隐藏编辑器显示，不加密场景/资源；不要将真实 Token 提交到仓库，推荐运行时注入。
`open()` 仅提交异步任务；使用 `load_failed` / `tile_failed` 观察 HTTP、权限与格式错误，
`close()` 取消 endpoint/tileset/子资源任务；已打开节点修改选项会自动重载，也可手动调用 `reload()`。

仅支持 ion **托管的 `3DTILES`**，不是 Cesium SDK 的完整能力：不支持 Terrain、Imagery、
Google 等 external provider，也不支持下文列出的隐式切片等格式。
Endpoint 必须返回无查询参数的 HTTPS URL；返回的资产 Token 仅通过 Bearer 请求头传递给
同源、同端口、资产目录下的 tileset、瓦片、glTF buffer 和纹理；跨源/降级重定向不携带凭据。
编码路径采取保守策略，不发送凭据。用户 Token 不进入 URL，也不会作为资产 Token 使用。
暂不自动刷新过期的资产 Token；401/403 会失败，可调用 `reload()` 重新解析 endpoint。
Endpoint JSON 限制为 1 MiB，解析后的根 tileset 与额外 buffer 使用 256 MiB、30 秒、2 次重试。
应用需自行遵守资产许可并展示要求的署名（本模块尚未渲染 ion attribution）。

## 支持范围

- 3D Tiles 1.0 / 1.1 **显式层级**，`content.uri`、旧版 `content.url`、`contents`、
  无内容中间瓦片、外部 tileset；外部引用携带变换上下文，并检测引用环。
- `box`、`sphere`、WGS84 `region`（包括跨日期变更线区域）。Region 不应用 tile transform。
- 列主序 `transform`、继承 `refine`、ADD / REPLACE、父级几何误差驱动的屏幕空间误差 LOD。
- GLB 2.0、静态 glTF 2.0；外部/内嵌 buffer、交错 bufferView、规范化整数属性、稀疏 accessor、
  `KHR_draco_mesh_compression` 解码、节点层级与 TRS/matrix、三角形/三角带/三角扇、顶点颜色和 UV。
- b3dm v1 的标准 28 字节头、JSON/二进制 `RTC_CENTER`、传统 Batch Table（JSON 数组及
  `BYTE`/`UNSIGNED_BYTE`/`SHORT`/`UNSIGNED_SHORT`/`INT`/`UNSIGNED_INT`/`FLOAT`/`DOUBLE`
  的 `SCALAR`/`VEC2`/`VEC3`/`VEC4` 二进制属性）、普通 accessor 与 Draco 映射的 `_BATCHID`/`BATCHID`，
  以及包含支持内容的 cmpt。`BATCH_LENGTH == 1` 且无批次属性时，整个 primitive 合理归入批次 0。
- 兼容 ContextCapture 输出的 B3DM：允许文件在完整 GLB 的 4 字节边界结束而缺少末尾 8 字节填充；
  表段边界仍须 8 字节对齐，容器/GLB 长度、chunk 与越界校验保留，不放宽其他容器格式。
- 驻留、当前 LOD 可见三角形的 CPU 精确射线/屏幕点拾取；返回瓦片、内容、primitive、triangle、
  batch ID、传统 Batch Table 属性、世界位置、法线和距离，不创建逐三角形物理节点。
- 基础颜色纹理（外部 URI、data URI 或 GLB bufferView）、金属度/粗糙度/自发光常量、双面与透明裁剪。
- `KHR_materials_unlit`（包括 required 声明）映射为 Godot 无光照材质，保留基础颜色、纹理、顶点颜色、透明与双面设置；普通材质仍按原有光照方式渲染。
  `get_streaming_stats()` 的 `resident_materials`、`resident_unlit_materials`、`resident_textured_materials`
  分别统计驻留实例持有的实际材质、无光照材质及已绑定基础颜色纹理的材质数量（非唯一纹理数）。

当前明确拒绝隐式切片、i3dm、pnts、glTF 1.x、蒙皮及不支持的 required extension。
动画、morph target、viewerRequestVolume 暂不参与显示/调度，并产生警告。3D Tiles 1.1
`EXT_structural_metadata` 不在本轮支持范围。通用渲染器当前仅上传基础颜色纹理，其余 PBR 贴图会警告并忽略；
不提供样式、动画或碰撞体。
纹理采样器使用通用渲染器默认设置，不支持 KHR_texture_transform。适合静态建筑与倾斜摄影网格。

## 坐标

默认选择根包围体中心作为浮动原点。`Tiles3DOptions.coordinate_system` 为 AUTO 时，
WGS84 region 或距离源坐标零点超过 1,000,000 的中心按 ECEF 处理；局部 X 向东、Y 向上、-Z 向北。
大坐标工程数据可以显式指定笛卡尔坐标：

```gdscript
var options := Tiles3DOptions.new()
options.coordinate_system = Tiles3DOptions.COORDINATE_SYSTEM_CARTESIAN
# 在 open() 之前设置；加载后修改需要 reload()
tiles.format_options = options

# 鼠标/触摸坐标或自定义世界射线均只测试当前驻留且可见的 LOD 三角形。
var hit := tiles.intersect_screen_point(get_viewport().get_camera_3d(), get_viewport().get_mouse_position())
# 或：var hit := tiles.intersect_ray(ray_origin, ray_direction)
if not hit.is_empty():
    print("batch=", hit.batch_id, " keys=", hit.properties.keys())
```

`intersect_ray()` / `intersect_screen_point()` 必须在主线程调用；拾取 CPU 数据计入驻留内存，随内容卸载释放。
`visible_only=false` 可查询驻留但因 LOD 替换暂时隐藏的几何。

`world_origin_mode`、`set_world_origin()`、`unit_scale` 和 `TiledOriginController3D`
沿用通用模块行为。顶点在 double 源坐标中累积变换，减去每份内容的局部原点后才转换为 Vector3。

## 实现与验证

`Tiles3DFormatLoader` 注册格式并复制选项；每个 session 使用一个工作线程解析 CPU 数据。
缺失的 glTF buffer 由主线程通过 `TiledMeshDataRequestQueue` 异步获取后重新解析。
这些额外 buffer 使用每个 256 MiB、30 秒超时、2 次重试的限制。
最终输出 `TiledMeshDocument`，通用模块完成网格打包、纹理加载、上传和缓存淘汰。

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --headless --test '--test-case=*3DTiles*' --no-version
.\bin\godot.windows.editor.dev.x86_64.console.exe --headless --test '--test-case=*TiledMesh3D*,*OSGDB*' --no-version
```

测试使用生成的三角形 GLB/b3dm、tileset 和临时外部 buffer，覆盖解析错误、坐标、
LOD、多个内容、外部 tileset、取消和实际 SceneTree 网格加载，不需要外部样例数据。
编辑器回归测试另覆盖属性绑定、空 Source URI 的 ion 配置警告、预览开关、资源修改与替换后的
自动重开、旧资源信号断开，以及 PackedScene 打包/实例化后的配置保留；HTTP 使用模拟客户端。

2026-10-28：新增 B3DM 缺少末尾填充的正例，以及容器长度不符、截断、表段错位/越界、
GLB chunk 损坏、GLB 边界不符和非 4 字节长度的反例。本轮通过新编译 release 模板的
DigitalViewer `tests/desktop_tiles.gd` 运行同等正反例，未重建原生单测 editor。
用户提供的武当山 ContextCapture 数据实测从 129 个加载失败降为 0；只读测试，未修改源数据。

## LOD 判定

子瓦片用父级 geometricError 和父级包围体计算 SSE。透视相机使用到球面/盒体的最近距离，进入包围体后继续细化非零误差；正交相机只使用像素比例。SSE 除以 `lod_bias`（编辑器使用 `editor_preview_lod_bias`），小于 1 时提高细节。相机静止时，完成一批请求后会继续加载其余已选中的瓦片。诊断时同时检查 `waiting_contents`、`requested_contents` 和打包/上传队列，避免把暂时没有在途请求误认为细化已完成。
