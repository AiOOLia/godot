# Tiled Mesh 3D 架构说明

本文档描述 `modules/tiled_mesh_3d` 当前实现的架构、公共 C++ 扩展契约、运行时状态机、线程模型和格式适配方法。它面向维护通用框架或新增格式模块的开发者。

文档以当前源码为准。公共类参考仍以 `doc_classes/` 下的 Godot XML 文档为准；本文重点解释这些接口背后的协作关系和不变量。

## 1. 设计目标

`tiled_mesh_3d` 把“瓦片化网格的流送和渲染”与“具体文件格式的解析”分离。

通用层负责：

- 面向场景的 `TiledMeshInstance3D` 节点。
- URI 规范化、本地文件和 HTTP(S) 字节读取。
- 相机驱动的瓦片选择、视锥裁剪和请求优先级。
- Distance、Projected Pixel 和 SSE 三类 LOD 判定。
- ADD、REPLACE、多内容和动态子树的运行时语义。
- 通用网格数组到 `RenderingServerTypes::SurfaceData` 的独立线程 CPU 打包。
- 已打包 surface 到 `RenderingServer` RID 的主线程提交。
- 外部图片读取与解码。
- 请求取消、generation 隔离、上传预算、LRU 和统计信息。
- 编辑器预览、Inspector、Gizmo 和浮动原点。

格式模块负责：

- 识别自己的数据格式。
- 校验并快照格式专用 options。
- 在适合该格式的线程模型中解析字节。
- 把格式对象转换成通用、纯 CPU 的瓦片与渲染载荷。
- 保证格式解析器对象不会越过适配器边界。

当前 `modules/osgdb` 是第一个格式模块。未来的 `3dtiles` 或自定义格式应复用同一接口，而不修改通用节点、调度器和渲染路径。

## 2. 非目标和当前边界

当前版本明确不提供：

- GDExtension ABI 或脚本实现的格式适配器。
- 3D Tiles JSON、B3DM、I3DM、GLB、隐式索引、region/ECEF 或元数据解析。
- 持久化网络缓存。
- 基于运动方向的推测预加载。
- 把运行时生成的 Mesh、Material 或 Texture 保存到场景。
- 格式解析器对象或 GPU RID 跨越格式接口。

通用材质模型预留了 albedo、normal、ORM 和 emission 图片槽。当前上传器完整消费基础颜色、金属度、粗糙度、发光颜色、透明、双面、Alpha Test 和 albedo 图片；外部异步纹理路径当前只安装 albedo 纹理。其他图片槽是后续扩展点，适配器不能假定它们已经被上传器消费。

## 3. 模块依赖和总体数据流

```text
Godot 场景 / 编辑器
        |
        v
TiledMeshInstance3D
        |
        +------> TiledMeshDataRequestQueue ------> 本地文件线程 / 非阻塞 HTTP
        |
        +------> TiledMeshFormatRegistry
                        |
                        v
                 TiledMeshFormatLoader
                        |
                        v
                 TiledMeshFormatSession ------> 格式专用解析线程
                        |
                        v
                 TiledMeshParseResult
                        |
                        v
                 TiledMeshDocument
                        |
                        v
              TiledMeshMeshPackQueue ------> 独立 CPU 打包线程
                        |
                        v
              TiledMeshPackedDocument
                        |
                        +------> TiledMeshImageDecodeQueue
                        |
                        v
                  RenderingServer
```

依赖方向必须保持单向：

```text
osgdb / 3dtiles / custom_format
                |
                v
         tiled_mesh_3d
                |
                v
       Godot core + scene + servers
```

`tiled_mesh_3d` 的生产代码不得包含 OSG、3D Tiles 或其他格式专用头文件、类型、常量和诊断名称。格式模块通过 `config.py` 声明对 `tiled_mesh_3d` 的模块依赖；例如 `osgdb` 使用：

```python
env.module_add_dependencies("osgdb", ["tiled_mesh_3d"])
```

## 4. 源码布局

| 路径 | 职责 |
| --- | --- |
| `tiled_mesh_format.h` | 原生格式接口和全部跨模块 CPU 数据结构 |
| `tiled_mesh_format_registry.*` | 加载器注册、显式查找、自动识别和扩展名汇总 |
| `tiled_mesh_instance_3d.*` | 场景 API、瓦片调度、上传、显示切换、LRU 和统计 |
| `tiled_mesh_data_source.*` | URI 规范化、相对 URI 解析、本地读取和图片格式解码 |
| `tiled_mesh_data_request_queue.*` | 共享的本地/HTTP 字节请求服务 |
| `tiled_mesh_image_decode_queue.*` | 共享的异步图片解码服务 |
| `tiled_mesh_mesh_pack_queue.*` | 通用网格数组到 CPU-only `SurfaceData` 的优先级打包队列 |
| `tiled_origin_controller_3d.*` | 浮动原点控制器 |
| `editor/` | Inspector 控件和瓦片边界 Gizmo |
| `doc_classes/` | Godot ClassDB 类参考 |
| `tests/test_tiled_mesh_format.h` | 合成格式、注册表和关键瓦片语义测试 |
| `register_types.*` | 模块服务、场景类和编辑器插件的生命周期 |

## 5. 核心设计不变量

维护或扩展实现时必须保持以下不变量：

1. 逻辑瓦片、可请求内容和渲染实例是三个不同对象，不能用一个状态代替三者。
2. 只有主线程创建、修改和释放 Godot Resource、`RenderingServer` RID 及场景对象；打包线程只生成不含有效 RID 的 CPU-only `SurfaceData`。
3. 格式工作线程只接收值类型、不可变快照和字节，只返回通用 CPU 数据。
4. 每个异步结果都携带 `ObjectID` 和 `generation`；旧 generation 的结果不得影响当前数据集。
5. REPLACE 在所需替代内容真正可显示前不能隐藏父内容。
6. 驱逐只移除可重建的 resident 内容，不删除已发现的逻辑拓扑。
7. `provider_key` 在一个数据集会话中必须稳定且唯一。
8. 格式识别的并列最高分是错误，不允许结果依赖模块初始化顺序。
9. adapter 注销前必须停止接收新任务，并保证现有 session 已关闭或不再引用 loader。
10. 格式专用 Resource 只能在创建 session 时读取并复制；解析线程不能直接访问它。

## 6. 面向场景的公共 API

### 6.1 `TiledMeshInstance3D`

场景只需要一个通用节点。主要属性分为以下几组。

数据源和格式：

- `source_uri`：根数据 URI，支持普通路径、`file://`、`res://`、`user://` 和 HTTP(S)。
- `format_id`：显式格式 ID；空值启用自动识别。
- `format_options`：可选的格式专用配置资源。
- `active_format_id`：只读，本次会话实际使用的 loader ID。
- `auto_load`：进入场景树后是否自动打开。

坐标和原点：

- `unit_scale`：一个源坐标单位对应的 Godot 单位数。
- `world_origin_mode`：使用格式默认原点或显式 double 原点。
- `world_origin_x/y/z`：double 精度源坐标原点。
- `world_to_local()` / `local_to_world()`：源坐标与 Godot 局部坐标互转。

LOD 和资源预算：

- `camera_path`：运行时相机；为空时使用 viewport 当前相机。
- `lod_bias`、`lod_hysteresis`：LOD 偏置和边界滞回。
- `maximum_screen_space_error`：SSE 模式的像素误差阈值。
- `max_concurrent_requests`：流送前沿允许的并发内容请求数；`0` 使用平台默认值。
- `max_tile_size_mb`：单个源响应的大小上限。
- `request_timeout_sec`、`retry_count`：HTTP 请求控制。
- `upload_budget_ms`：每帧主线程网格上传时间预算。
- `memory_budget_mb`：resident 渲染内容预算；`0` 使用平台默认值。
- `cast_shadow`：应用到所有生成实例的阴影模式。

编辑器预览：

- `editor_preview_enabled`
- `editor_preview_lod_bias`
- `editor_preview_memory_mb`
- `editor_preview_max_requests`
- `editor_preview_show_bounds`

生命周期方法：

- `open()`：建立根内容并排队读取。返回 `OK` 只表示请求成功排队，不表示异步解析完成。
- `close()`：递增 generation，取消请求，关闭 session，释放 RID 并清空当前数据集。
- `reload()`：执行 `close()` 后重新 `open()`。
- `set_streaming_paused()`：暂停选择和新上传；已 resident 内容保留。

信号：

- `root_loaded`：根文档已成功上传。
- `load_failed(uri, message)`：根读取、格式选择、根解析或根上传失败。
- `tile_loaded(uri)`：一个内容已经上传为 resident。
- `tile_failed(uri, message)`：非根内容失败。
- `streaming_stats_changed(stats)`：根内容和节点生命周期状态变化。高频子内容完成通过 `tile_loaded`/`tile_failed` 报告；调用方需要完整统计时再调用 `get_streaming_stats()`，避免大型动态树在每个子内容阶段变化时反复全量构造统计字典。
- `world_origin_changed(origin)`：double 原点发生变化。

修改 `source_uri`、`format_id` 或 `format_options` 时，已打开节点会先关闭并延迟重开。修改 `unit_scale` 或 `world_origin_mode` 会重新加载，因为它们影响解析生成的顶点和坐标框架。

### 6.2 动态文件过滤器

Inspector 中 `source_uri` 的文件过滤器不是硬编码的。`_validate_property()` 从 `TiledMeshFormatRegistry::get_recognized_extensions()` 收集所有 loader 扩展名，去点号、转小写、去重、排序后生成过滤字符串。

因此新增格式模块只需注册 loader，文件选择器会自动出现对应扩展名。

### 6.3 `TiledOriginController3D`

浮动原点控制器在运行时监控相机。当相机全局位置长度超过 `rebase_threshold_m` 时：

1. 把 `managed_world_root` 沿相反方向平移。
2. 枚举 `_tiled_mesh_instances_3d` 组中的所有 `TiledMeshInstance3D`。
3. 使用每个节点自己的坐标变换，把 Godot 世界位移换算回源坐标位移。
4. 推进节点的 double 原点。
5. 发出 `origin_shifted(delta, new_origin)`。

控制器不会在编辑器 hint 模式中重定位场景。

## 7. 原生格式扩展接口

格式接口是 Godot 模块之间的 C++ 接口，不是稳定的 GDExtension ABI。

### 7.1 `TiledMeshFormatOptions`

格式模块可以派生一个 Godot `Resource` 暴露专用配置，并重写：

```cpp
virtual StringName get_format_id() const;
```

节点没有显式 `format_id` 时，会优先采用 options 声明的格式 ID。loader 的 `supports_options()` 仍必须验证实际类型。

`create_session()` 在主线程调用。实现应在这里把 Resource 字段复制到普通结构或 session 成员，此后工作线程不再读取 Resource。

### 7.2 `TiledMeshFormatLoader`

loader 是格式级、长生命周期对象，职责是描述格式并创建数据集级 session：

```cpp
class TiledMeshFormatLoader {
public:
    virtual StringName get_format_id() const = 0;
    virtual Vector<String> get_recognized_extensions() const = 0;
    virtual int recognize(const String &uri, const PackedByteArray &bytes) const = 0;
    virtual bool supports_options(const Ref<TiledMeshFormatOptions> &options) const = 0;
    virtual TiledMeshFormatSession *create_session(
            const Ref<TiledMeshFormatOptions> &options,
            const TiledMeshSessionOptions &session_options,
            String &error) = 0;
};
```

约定：

- `format_id` 必须非空，并在注册表中唯一。
- `recognize()` 返回正数表示识别；数值越大置信度越高，`0` 表示不识别。
- 识别可以综合 URI、扩展名和根字节特征。
- `recognize()` 不应保存传入字节的指针，也不应启动后台任务。
- 每次打开数据集都创建独立 session，不能把数据集状态放进全局 loader。
- 创建失败返回 `nullptr` 并填写可读错误。

### 7.3 `TiledMeshFormatSession`

session 隔离一个打开的数据集，并提供异步解析控制：

```cpp
class TiledMeshFormatSession {
public:
    virtual uint64_t enqueue_parse(const TiledMeshParseRequest &, double priority) = 0;
    virtual void update_priority(uint64_t request_id, double priority) = 0;
    virtual void cancel(uint64_t request_id) = 0;
    virtual bool take_result(uint64_t request_id, TiledMeshParseResult &result) = 0;
    virtual void shutdown() = 0;
};
```

session 可以拥有独立线程，也可以转发给格式模块共享的串行队列。接口只规定异步语义，不强制具体并发模型。

`enqueue_parse()` 返回 `0` 表示无法排队。非零 ID 在 session 内唯一，并用于调优先级、取消和提取结果。`take_result()` 是主线程轮询入口，尚未完成时返回 `false`。

`shutdown()` 必须幂等，并做到：

- 不再接收新任务。
- 取消 session 拥有的待处理任务。
- 丢弃或排空不再需要的结果。
- 确保析构后不会有回调访问 session。

### 7.4 格式选择规则

根字节读取完成后，节点按固定顺序选择格式：

1. 若 `format_id` 非空，按该 ID 查找 loader。
2. 否则，若 `format_options->get_format_id()` 非空，按 options 的 ID 查找。
3. 否则调用所有 loader 的 `recognize(uri, bytes)`。
4. 自动识别选择唯一的最高正分 loader。
5. 没有正分或最高分并列时失败。
6. 对选中 loader 调用 `supports_options()`。
7. 创建 session，设置 `active_format_id`，再提交根解析。

显式 ID 不存在、options 不兼容、自动识别歧义和 session 创建失败都会进入根失败状态并发出 `load_failed`。

### 7.5 注册表生命周期

格式模块通常在 `MODULE_INITIALIZATION_LEVEL_SCENE` 注册 loader：

```cpp
loader = memnew(MyTiledMeshFormatLoader);
TiledMeshFormatRegistry::register_loader(loader);
```

反初始化时注销并删除：

```cpp
TiledMeshFormatRegistry::unregister_loader(loader);
memdelete(loader);
loader = nullptr;
```

注册表拒绝空 ID、同一对象重复注册和重复 ID。内部 loader 容器使用函数静态存储，使容器本身在模块反初始化阶段仍有效。

格式模块仍需保证注销顺序安全：先让使用它的节点/session 停止，再注销 loader，最后销毁格式解析队列和静态解析环境。

## 8. 通用 CPU 数据契约

所有跨适配器边界的数据定义在 `tiled_mesh_format.h`。它们只包含 Godot 值类型、容器、字符串和字节数组。

### 8.1 坐标框架

`TiledMeshVector3d` 保存 double 精度源坐标。

`TiledMeshCoordinateFrame` 包含：

- `origin`：源坐标原点元数据。
- `source_to_local`：源坐标轴到 Godot 局部坐标轴的基变换。
- `unit_scale`：源单位到 Godot 单位的比例元数据。

`TiledMeshDocument::source_origin` 是该文档网格载荷的源坐标锚点，也是根文档在 `WORLD_ORIGIN_FORMAT_DEFAULT` 下建议给节点使用的原点。

当前载荷约定如下：

- bounding volume 和 selection 的中心保留为 double 源坐标。
- surface 顶点已经由 adapter 转换为 Godot 局部轴，并相对该文档的 `source_origin` 保存。
- adapter 应使用 session 收到的 `unit_scale` 生成顶点；运行时不会再次缩放顶点数组。
- 根文档的 `coordinate_frame.source_to_local` 会成为节点进行源坐标互转和瓦片定位时使用的基变换。
- `local_aabb` 和 surface `aabb` 使用已经转换后的局部坐标。

节点的源坐标转换可概括为：

```text
local = source_to_local_basis * ((source - active_world_origin) * unit_scale)
```

每个渲染载荷再以其文档 `source_origin` 对应的 local 位置作为实例平移。这让大坐标数据仍能用靠近零点的 float 顶点表达。

### 8.2 包围体

`TiledMeshBoundingVolume` 支持：

- `TYPE_SPHERE`：double 中心和半径。
- `TYPE_ORIENTED_BOX`：double 中心与三个 half-axis 向量。

球体直接用于视锥测试。OBB 当前在运行时转换八个角点得到 Godot 世界 AABB，再执行视锥测试。未来 region/ECEF 应由格式适配器转换成这两种通用表达之一。

### 8.3 选择规则

`TiledMeshSelectionRule::Mode`：

| 模式 | 数据 | 匹配条件 |
| --- | --- | --- |
| `MODE_ALWAYS` | 无 | 始终匹配 |
| `MODE_DISTANCE_RANGE` | `center`, `minimum`, `maximum` | 相机距离位于半开区间 `[minimum, maximum)` |
| `MODE_PROJECTED_PIXEL_RANGE` | `center`, `radius`, range | 投影直径位于半开区间 |
| `MODE_SCREEN_SPACE_ERROR` | `center`, `radius` 或 `half_axes`, `volume_type`, `geometric_error` | 投影几何误差大于节点阈值 |

透视 SSE 使用相机到父瓦片包围体的最近距离：

```text
sse = geometric_error
    * unit_scale
    * max_global_scale
    * focal_length_pixels_y
    / max(distance_to_bounding_volume, 0.000001)
    / lod_bias
```

球体距离为 `max(length(camera - center) - radius, 0)`；盒体使用变换后盒体表面的最近距离，相机在包围体内时为 0。`volume_type` 默认球体，3D Tiles 适配器会保留父包围体的类型和 half axes。`lod_bias < 1` 增加细节，运行时和编辑器保持同一方向。

当 `sse > maximum_screen_space_error` 时选择细节。正交相机不使用透视深度衰减。已经匹配的分支会应用 `lod_hysteresis`，降低边界附近反复装卸。

### 8.4 网格、材质和图片载荷

`TiledMeshMeshSurfacePayload` 使用 Godot Mesh 数组语义：

- `vertices`
- `normals`
- `uv0`、`uv1`
- `colors`
- `indices`
- `material`
- `aabb`
- `estimated_bytes`

运行时只上传非空顶点和索引的三角形 surface。法线、UV 和颜色只有在数量等于顶点数时才安装。

`replacement_group` 表示 surface 属于哪个独立替换区域。`lod_branch` 指向 `TiledMeshDocument::lod_ranges`，允许同一文档中的 inline LOD 几何保留嵌套范围链。

`TiledMeshMaterialPayload` 当前包含基础 PBR 参数和四类图片槽。`TiledMeshImagePayload` 可以携带：

- 已解码的 RGBA8 像素；或
- 需要通用层异步读取的外部 URI。

### 8.5 内容请求

`TiledMeshContentRequest` 描述一个可独立读取和解析的内容：

- `uri`：绝对或已由 adapter 解析后的规范 URI。
- `content_type`：给 session 的格式内提示。
- `context`：adapter 私有的纯字节上下文。
- `required_for_display`：此内容是否是瓦片可显示的必要条件。

同一逻辑瓦片可有多个内容。所有内容独立拥有状态和内存归属；只有标记为 required 的内容会阻塞 REPLACE 完成。可选内容失败不会阻止瓦片用已经 resident 的必要内容参与显示切换。

### 8.6 瓦片定义

`TiledMeshTileDefinition` 描述逻辑拓扑，而不是一个已加载 Mesh：

- `provider_key`：跨增量解析稳定的唯一键。
- `parent`：当前返回文档内的局部父索引。
- `replacement_group`：独立替换区域编号。
- `refinement`：`REFINE_ADD` 或 `REFINE_REPLACE`。
- `bounding_volume`
- `selection`
- `contents`

`parent == -1` 时，新瓦片挂到“产生当前文档的内容所属瓦片”。非负 parent 必须引用同一 `document.tiles` 中更早出现且已经有效的定义。这样一次结果可以按父在前的顺序追加任意深度子树。

`provider_key` 的要求比局部 parent 更强：它应在整个数据集 session 内唯一，且同一真实瓦片在后续解析中必须返回同一个 key。若省略，运行时依次尝试首个内容 URI和内部生成键；生产 adapter 应显式提供 key，避免拓扑因返回顺序变化而不稳定。

### 8.7 文档和解析结果

`TiledMeshDocument` 可以同时提交：

- 当前内容的 surface 和 material。
- 新发现的逻辑瓦片子树。
- inline LOD 范围。
- 警告。
- 数据集局部 AABB。
- CPU 内存估算。

根解析、普通内容解析和动态子树解析使用同一种 document。任意后续内容都可以继续返回 `tiles`，因此拓扑不要求在根文件中一次性完整出现。

`TiledMeshParseRequest` 携带请求归属、generation、URI、类型提示、私有上下文、字节和 `root` 标记。`TiledMeshParseResult` 原样返回归属信息、错误和通用 document。

## 9. 运行时内部模型

### 9.1 三层对象

`TiledMeshInstance3D` 内部将状态拆成三层：

| 内部对象 | 表示什么 | 主要生命周期 |
| --- | --- | --- |
| `StreamingTile` | 稳定逻辑拓扑、选择和细化规则 | 数据集打开期间保留 |
| `StreamingContent` | 一个 URI 的读取、解析、CPU/GPU resident 状态 | 可取消、失败、驱逐和重载 |
| `TileRenderInstance` | 一组实际 Mesh RID、Instance RID 和 Material | 上传时创建，驱逐/关闭时释放 |

一个 tile 可以没有内容，只作为拓扑分支；也可以有多个 content。一个 content 返回的文档又可以生成多个 render instance，因为 surface 会按 `(replacement_group, lod_branch)` 分组。

### 9.2 内容状态机

```text
UNLOADED ──选择并排队──> REQUESTED
                              |
                              v
                         DOWNLOADED
                              |
                              v
                           PARSING
                              |
                              v
                      CPU PACKING（内部）
                              |
                              v
                         CPU_READY
                              |
                              v
                         UPLOADING
                              |
                              v
                          RESIDENT
                              |
                       LRU 驱逐 v
                          UNLOADED

任一读取/解析/打包/上传阶段 ──错误──> FAILED
不再需要的待处理内容   ──取消──> CANCELLED ──再次需要──> REQUESTED
```

`FAILED` 不会被当作可显示。根失败发出 `load_failed` 并令节点退出 open 状态；非根失败发出 `tile_failed`，父级 fallback 保持可见。

## 10. 打开数据集的完整流程

`open()` 的时序如下：

1. 递增 generation，清理旧流送和渲染状态。
2. 创建内部逻辑根瓦片 `__root__`。
3. 把规范化后的 `source_uri` 建成根内容。
4. 通过共享数据队列异步读取根字节。
5. 根字节返回后执行格式选择和 options 校验。
6. loader 创建数据集专用 session。
7. 根字节以最高优先级提交给 session。
8. 主线程轮询到 parse result，并按当前视觉优先级提交给通用 mesh pack queue。
9. 独立打包线程把 Godot Mesh 数组转换成 CPU-only `SurfaceData`；完成后内容进入 `CPU_READY`。
10. 上传器在预算内创建材质、Mesh RID 和 Instance RID。
11. 把 document 中的新 tile 注册到稳定拓扑。
12. 根内容进入 `RESIDENT`，发出 `root_loaded` 和 `tile_loaded`。
13. 相机选择发现所需子内容，重复“读取—解析—打包—上传—追加拓扑”。

调用 `open()` 后发生的格式错误通常是异步错误，因此调用方必须监听 `load_failed`，不能只依赖 `open()` 返回值。

## 11. 每帧调度顺序

节点在 `NOTIFICATION_INTERNAL_PROCESS` 中依次执行：

1. `_update_streaming()`：运行时先根据最新相机更新需要集合，重排或取消读取、解析和 mesh pack 工作。
2. `_poll_data_results()`：推进本地/HTTP 请求并取得字节结果。
3. `_poll_texture_results()`：推进外部纹理读取、解码和安装。
4. `_poll_parse_results()`：从格式 session 取得 CPU document，并提交 mesh pack 请求。
5. `_poll_mesh_pack_results()`：取得已打包的 CPU-only `SurfaceData`。
6. `_upload_ready_contents()`：按优先级和预算创建 RID 并提交已打包 surface。
7. 上传若发现动态子树，再执行一次 dirty-only `_update_streaming()`，让新内容在同一帧进入请求流水线。
8. 必要时同步 layer mask 和节点可见性到 RID。

编辑器模式由插件收集一个或多个 3D viewport 相机并调用 `editor_update_cameras()`。请求集合取所有编辑器相机的并集，而最终显示切换使用活动显示相机，避免多个 viewport 同时工作时漏载需要的内容。

为避免每帧重复遍历，运行时缓存相机 ObjectID、Transform、Projection 和 viewport 尺寸；只有相机状态或拓扑变化时才重新计算选择。

`max_concurrent_requests` 只约束读取、下载和格式解析阶段。解析完成并进入 mesh pack queue 后会释放该准入槽，使下一内容可以与当前打包任务流水并行。为防止解析器长期快于单打包线程，通用层在全局等待打包数达到请求上限，或打包在途原数组估算达到 resident 预算的一半（最少 64 MiB）时暂停准入新的读取；已有读取和解析仍可完成，因此这是软背压而不是单文档硬大小限制。

## 12. 相机选择、裁剪和优先级

每个逻辑瓦片维护两个标记：

- `desired`：任一参与流送的相机是否需要它，用于请求和 LRU 活跃性。
- `display_desired`：活动显示相机是否需要它，用于 ADD/REPLACE 可见性。

子瓦片只有在父瓦片 desired 且自己的选择规则匹配时才进入 desired。除 `MODE_ALWAYS` 外，还要通过球体、OBB 或替换区域 AABB 的视锥测试。

请求优先级综合：

- 包围体在 viewport 中的投影面积。
- 距离范围、像素范围或 SSE 的细化紧迫度。
- 是否来自活动显示相机。
- 是否拥有从父级 fallback 推导出的替换 AABB。

数据队列、格式 session 和 mesh pack queue 都接收优先级更新。外部纹理也继承所属内容的视觉优先级并动态更新读取、解码和主线程安装顺序。瓦片离开 desired 后，处于读取、下载、解析、打包或 CPU ready 的内容会被取消；打包线程至少在每个 surface 之间检查取消。已经 resident 的内容保留到 LRU 决定是否驱逐。resident 内容的渲染分支被细化隐藏或移出活动显示视锥时，其尚未完成的外部纹理暂停，重新可见时恢复。

## 13. ADD、REPLACE 和无空洞切换

### 13.1 ADD

`REFINE_ADD` 子瓦片只增加内容，不参与隐藏父内容。父级几何在子级加载、失败或驱逐时都保持显示。

### 13.2 REPLACE

`REFINE_REPLACE` 子瓦片只有在“可显示”后才能替换父内容。一个瓦片可显示需要：

- 所有 `required_for_display` 内容均为 `RESIDENT`。
- 对其当前显示所需的内部替换分组，存在可显示的 fallback 或完整子替代。
- 当前 inline LOD 条件允许某个有效渲染实例，或者它是由可显示后代构成的纯拓扑节点。

父 surface 与 REPLACE 子瓦片通过相同的非负 `replacement_group` 关联。运行时只有在该组所有 `display_desired` REPLACE 子分支都可显示后，才隐藏父级同组 surface。

可显示性汇总只要求当前显示相机视锥相交的独立 replacement group 提供覆盖。一个文档中的屏幕外分组不会阻止同一文档覆盖更宽泛的祖先 fallback，否则已驻留的屏幕内精细几何会与仍可见的祖先粗模重叠。运行时优先使用实际 fallback mesh AABB 判断分组覆盖范围，其次使用子瓦片的球体或 OBB；缺少可靠空间范围时保持保守，不隐藏父级以避免空洞。

“可显示”是递归覆盖概念，不要求中间层自己的 fallback 仍然可见。如果中间瓦片的每个 replacement group 已经由更深层后代完整覆盖，该中间瓦片仍然是其父级的完整替代；否则多级 REPLACE 会错误保留最上层粗模。

这有两个重要结果：

- 一个空间区域完成细化，不会错误隐藏父节点中其他独立区域。
- 任一必要内容失败、取消或被驱逐时，完整细化条件立即失效，父 fallback 重新显示。

格式适配器若需要替换父级具体几何，必须给父 surface 和对应 REPLACE 子瓦片分配一致的非负 group。仅设置 `REFINE_REPLACE` 而不建立 group，不能表达独立 surface 区域替换。

显示判定包含递归，运行时用 tile、parent 和 group 缓存避免同一帧重复计算，并用访问中标记防止异常拓扑造成无限递归。

## 14. 动态子树和拓扑保留

每个成功上传的 document 都会调用 `_register_tiles()`。新定义按局部 parent 顺序映射到运行时索引，并通过 `provider_key` 去重。

拓扑和内容描述符在 LRU 驱逐后继续存在。驱逐只会：

- 释放内容生成的 Mesh/Instance RID。
- 取消属于这些实例的纹理请求。
- 把内容状态恢复为 `UNLOADED`。

相机再次需要它时，可直接重新请求已知内容，不必从祖先重新发现整棵子树。

## 15. 字节请求服务

`TiledMeshDataRequestQueue` 是模块级共享服务。

### 15.1 URI 处理

`TiledMeshDataSource` 提供：

- 去除空白、统一路径分隔符。
- `file://` 解码和 Windows drive path 处理。
- 本地路径 `simplify_path()`。
- HTTP 相对路径、绝对路径和 `..` 规范化。
- `res://`、`user://` 和显式绝对 URI 处理。

adapter 应使用 `resolve_uri(parent_uri, reference)` 解析外部内容和纹理引用，避免自行拼接 URL。

### 15.2 本地读取

本地文件请求进入共享工作线程。线程每次选择当前最高优先级请求，检查空文件、大小限制、短读和 `INT32_MAX` 内存限制，然后把纯字节结果送回主线程。

若工作线程无法启动，会退化为调用线程同步读取并给出警告。

### 15.3 HTTP

HTTP 使用主线程轮询的非阻塞 `HTTPClient`：

- 相同“规范 URI + 大小上限”的请求合并为一个 job。
- 每个调用方仍保留独立 subscriber request ID、owner、generation 和优先级。
- job 优先级取当前 subscriber 的最大值。
- 支持最多 5 次重定向。
- 4xx 直接失败，5xx 和连接错误按配置重试。
- 前两档重试等待分别为 0.5 秒和 1 秒，后续维持 1 秒。
- 同时检查 `Content-Length` 和实际累计字节，防止越过 tile 大小限制。
- 默认全局活动请求上限为桌面 8、Web 4。

取消一个 subscriber 不影响同一合并 job 中的其他 subscriber；最后一个 subscriber 被取消时才关闭并移除 job。

队列没有磁盘缓存，也不会在结果被消费后保留响应。

## 16. 格式解析线程模型

通用层不创建统一解析线程，因为第三方解析库经常有自己的线程约束。每个格式模块自行实现 session 和解析队列。

以 OSG 为例：

- `OsgTiledMeshSession` 保存 session options 的普通值快照和所属 request ID 集合。
- 所有 session 把任务提交到模块共享的 `OsgParseQueue`。
- 队列串行解析，保证 OSG 对象在同一工作线程创建和析构。
- session 取消和 shutdown 转发到该队列。
- 返回结果已经是 `TiledMeshDocument`，不包含 `osg::Object`。

新格式若解析库可安全并行，可以实现每 session 线程池，但仍需保证 `take_result()`、取消、shutdown 和模块卸载安全。

## 17. 图片管线

图片有两条路径：

1. adapter 直接返回 RGBA8 像素，上传时立即创建 `ImageTexture`。
2. adapter 返回外部图片 URI，通用数据队列先读取字节，图片队列再异步解码。

图片工作线程每次选择当前视觉优先级最高的等待任务，解码 PNG、JPEG、BMP 或 TGA。等待中的任务可随相机移动更新优先级；正在执行的单次解码不抢占，但被取消后的结果不会安装。扩展名缺失或错误时，会依次尝试这些 signature-capable 解码器，并统一转换成 RGBA8。

外部纹理的读取优先级使用低于几何内容的独立数值区间，因此重要瓦片的纹理可以超越次要瓦片纹理，但不会让根或粗层纹理阻塞新发现的精细几何文件。所属瓦片离开所有流送相机的 desired 集合、渲染分支被 REPLACE/LOD 隐藏，或实际 Mesh AABB 离开活动显示视锥后，尚未完成的读取/解码会被取消，保留可恢复的纹理描述；重新可见时按新的视觉优先级重新提交。

主线程每次内部处理最多安装两个已解码外部纹理，并从已完成图片中优先选择视觉贡献最大的所属瓦片。纹理请求也携带 owner 和 generation；目标 render instance 已驱逐或 generation 过期时，结果直接丢弃。

## 18. 网格 CPU 打包

`TiledMeshMeshPackQueue` 是格式无关的模块级服务，使用一个受控工作线程。它接收 `TiledMeshDocument`，每次从等待列表中选择当前视觉优先级最高的请求，并完成以下纯 CPU 工作：

1. 忽略没有有效顶点或索引的 surface。
2. 按 `(replacement_group, lod_branch)` 建立 render group。
3. 按 Godot Mesh 数组语义组装 vertex、normal、UV、color 和 index 数组。
4. 调用 `mesh_create_surface_data_from_arrays()` 生成 `RenderingServerTypes::SurfaceData` 的顶点、属性和索引字节。
5. 保留原材质索引、顶点色标记、AABB、LOD 分支和内存估算，移除已经打包的原始 surface 数组。

结果 `TiledMeshPackedDocument` 仍是普通 CPU 数据；其 `SurfaceData.material` 在工作线程保持无效 RID。工作线程不得创建 Mesh、Material、Texture、Instance RID，不得访问 SceneTree，也不得接收 OSG 或其他格式解析对象。

这里有意不复用本地读取线程或 OSG 解析线程：读取线程需要尽快为后续内容提供字节；OSG 线程因第三方对象生命周期约束必须全局串行。独立队列可让下一份 OSGB 继续解析，同时上一份 document 在另一个线程打包。

等待中的请求支持动态调优先级和直接移除。正在打包的请求不能中断单个 `mesh_create_surface_data_from_arrays()` 调用，但会在 surface 之间检查取消；取消或旧 generation 的结果不会进入主线程上传。队列统计同时记录等待数、活动数和原始数组的在途估算字节，用于软背压并观察原数组与 packed buffer 短暂共存造成的峰值。

当前实现依据 Godot 源码中该 helper 只读取输入数组并生成 CPU 字节数组、不创建 RID 或访问渲染后端的性质在线程中调用它。由于公开 API 没有承诺该 helper 是跨线程 ABI，升级 RenderingServer 实现时必须重新审查，并保留并发关闭、取消和大网格压力测试。

## 19. 主线程上传和 RenderingServer 生命周期

packed-ready 内容按请求优先级上传。当前每次处理同时受以下约束：

- `upload_budget_ms` 时间预算。
- 16 MiB 估算 CPU 字节预算。

为保证前进，第一个候选即使超过预算也允许上传；预算用于阻止同一帧继续上传更多候选。CPU 数组打包已经不再消耗这个主线程预算。

打包阶段已经把 surface 按 `(replacement_group, lod_branch)` 分组。主线程配置材质 RID 后，每组通过一次 `mesh_create_from_surfaces()` 创建 Mesh RID，再创建一个 Instance RID，并保存：

- 所属逻辑 tile 和 content。
- 材质引用。
- source origin。
- AABB、估算网格和纹理字节。
- inline LOD 范围链。
- 当前可见状态。

创建过程中若任一步失败，会释放该 document 已创建的 RID，避免半上传内容进入 resident。

节点变换、可见性、layer mask、scenario 和阴影设置直接同步到 `RenderingServer` instance。退出 world 时先移除 scenario；关闭、驱逐或析构时释放 instance 和 mesh RID。

## 20. 内存预算和 LRU

当前 resident 预算累加 surface 的 `estimated_bytes` 和运行时记录的 `texture_bytes`。现有实现会把异步外部 albedo 纹理按 RGBA8 大小计入 `texture_bytes`；直接随 document 返回的内嵌图片尚未加入 resident 统计，这是扩展上传器时需要补齐的已知限制。默认预算：

- 桌面运行时：512 MiB。
- Web 运行时：256 MiB。
- 编辑器预览：`editor_preview_memory_mb`，默认 256 MiB。

LRU 只选择：

- 当前是 `RESIDENT`；
- 不是根内容；
- 所属瓦片当前不 desired；
- 实际拥有渲染实例；
- resident 至少 2 秒；
- `last_used_usec` 最老的内容。

驱逐后重新计算 resident AABB 和显示切换。若被驱逐内容曾完成 REPLACE，父级会在同一次可见性更新中恢复。

逻辑拓扑、内容 URI 和 adapter 上下文不计入可驱逐对象，因此不会因 GPU LRU 丢失发现结果。

## 21. 异步安全、取消和关闭

### 21.1 generation

每次 `open()` 和 `close()` 都递增 generation。数据、解析、mesh 打包和图片结果均携带提交时的：

- 节点 `ObjectID`
- generation
- request ID

轮询结果只有在 owner 和 generation 同时匹配时才生效。这是快速切换 URI、reload、删除节点和场景退出时防止过期回调的主要机制。

### 21.2 `close()` 顺序

节点关闭时：

1. 递增 generation，使所有在途结果失效。
2. 取消根数据请求和所有内容数据请求。
3. 取消所有 session parse request。
4. 取消所有 mesh pack request；活动请求会在当前 surface 结束或下一个 surface 边界停止。
5. 调用 session `shutdown()` 并析构 session。
6. 清空内容和逻辑拓扑。
7. 取消纹理请求并释放全部 RID。
8. 清空 AABB 和编辑器 Gizmo 状态。

节点在 `NOTIFICATION_EXIT_TREE` 中自动 `close()`，析构函数也执行资源清理。

### 21.3 模块反初始化

`tiled_mesh_3d` 在 scene 初始化级创建数据请求队列、图片解码队列和 mesh pack queue。在 scene 反初始化级先关闭并销毁 mesh pack queue，再销毁图片队列和数据队列。各自析构会 shutdown 并等待工作线程结束。

格式模块必须在销毁自己的队列前注销 loader，并确保格式任务已经停止。第三方库要求单线程析构时，队列 shutdown 必须在对应线程退出前清理其对象和结果。

## 22. 统计与诊断

`get_streaming_stats()` 返回：

| 键 | 含义 |
| --- | --- |
| `open`, `paused`, `generation` | 节点会话状态 |
| `active_format_id` | 当前 adapter |
| `logical_tiles` | 已发现逻辑瓦片数 |
| `contents` | 可请求内容数 |
| `root_state` | 聚合根状态字符串 |
| `requested_contents` | 读取、下载或解析中的内容数 |
| `packing_contents` | 当前节点等待或正在 CPU mesh 打包的内容数 |
| `packed_ready_contents` | 已生成 `SurfaceData`、等待主线程上传的内容数 |
| `cpu_ready_contents` | `packed_ready_contents` 的兼容名称 |
| `resident_contents` | resident 内容数 |
| `failed_contents` | 失败内容数 |
| `source_bytes` | 当前解析阶段持有的源字节估算 |
| `packed_ready_bytes` | 等待主线程上传的 packed buffer 实际字节估算 |
| `cpu_ready_bytes` | `packed_ready_bytes` 的兼容名称 |
| `mesh_pack_queue_pending` | 全局 mesh pack queue 的等待请求数 |
| `mesh_pack_queue_active` | 全局 mesh pack queue 的活动请求数，当前为 0 或 1 |
| `mesh_pack_in_flight_bytes` | 全局等待和活动打包请求所持原始数组的估算字节 |
| `estimated_resident_bytes` | resident surface 与已计入的外部纹理估算；不含当前未计数的内嵌图片 |
| `resident_render_instances` | 有效渲染实例数 |
| `visible_render_instances` | 当前可见实例数 |
| `inline_lod_render_instances` | 带 inline LOD 范围链的有效渲染实例数 |
| `visible_inline_lod_render_instances` | 当前可见的 inline LOD 渲染实例数 |
| `visible_tiles` | 拥有可见实例的逻辑瓦片数 |
| `texture_requests` | 尚未安装的外部纹理总数，包含活动、暂停和已解码等待安装的请求 |
| `active_texture_requests` | 正在读取或解码的外部纹理数 |
| `paused_texture_requests` | 因所属瓦片/渲染分支当前不需要显示而暂停的外部纹理数 |
| `decoded_texture_requests` | 已解码、等待主线程安装的外部纹理数 |
| `replacement_surfaces` | 带替换 group 的 resident 实例数 |
| `hidden_replacement_surfaces` | 已被完整细化隐藏的实例数 |

`known_tiles`、`requested_tiles`、`cpu_ready_tiles`、`resident_tiles`、`failed_tiles`、`fallback_surfaces` 和 `hidden_fallback_surfaces` 是为旧诊断调用保留的兼容别名。新工具应优先使用 tile/content 区分明确的新键。

## 23. 编辑器集成

编辑器插件包含三部分：

- Inspector：打开/重载、暂停、卸载、聚焦数据集、切换 bounds，并显示流送统计。
- Gizmo：显示数据集和 resident 边界。
- 多 viewport 相机收集：向节点提供所有编辑器相机和活动相机。

这些工具只依赖通用节点，不知道当前格式。格式模块不应再注册自己的 `TiledMeshInstance3D` Inspector 或 Gizmo。

## 24. OSG 适配映射示例

`modules/osgdb` 展示了一个完整 adapter：

- loader ID 为 `osg`。
- 扩展名为 `.osg`、`.osgb`。
- 扩展名识别得分为 80，`#Ascii` 或 XML 根特征得分为 100。
- `OsgTiledMeshOptions` 提供 Z-up 到 Godot Y-up 的默认转换，也可选择保持 Y-up。
- `OsgSceneConverter` 输出 `TiledMeshDocument`。
- 普通几何写入当前 document surface。
- PagedLOD 和 ProxyNode 外部文件变成逻辑 tile 的 content request。
- OSG distance 或 pixel range 映射到对应 selection mode。
- inline LOD 映射到 `lod_ranges`。
- 独立 PagedLOD fallback 使用不同 `replacement_group`，防止一个区域细化时隐藏其他区域。
- 漫反射、透明、双面、Alpha Test、顶点颜色和漫反射纹理映射到通用材质载荷。

OSG adapter 是使用示例，不是通用层的特例。`tiled_mesh_3d` 在关闭 `osgdb` 时仍可独立构建；打开 OSG URI 会通过普通的“没有 adapter 识别”路径明确失败。

## 25. 新增格式适配器的步骤

### 25.1 建立模块依赖

在格式模块 `config.py` 中声明：

```python
def can_build(env, platform):
    env.module_add_dependencies("my_format", ["tiled_mesh_3d"])
    return env["threads"] and not env["disable_3d"]
```

根据实际第三方库继续限制平台，但不要让通用模块反向依赖格式模块。

### 25.2 可选：定义 options

```cpp
class MyTiledMeshOptions : public TiledMeshFormatOptions {
    GDCLASS(MyTiledMeshOptions, TiledMeshFormatOptions);

public:
    virtual StringName get_format_id() const override { return SNAME("my_format"); }
};
```

在 `create_session()` 中读取字段并构造不可变普通快照。

### 25.3 实现 loader

loader 至少需要：

- 唯一 ID。
- 规范化、不带点号的扩展名列表。
- 稳定的识别评分策略。
- 严格的 options 类型校验。
- 每数据集 session 创建。

识别评分应留出层级，例如“仅扩展名”低于“已验证 magic/version”。不要用模块注册先后来解决冲突。

### 25.4 实现 session

session 必须维护自己提交的 request ID，并实现优先级、取消、结果提取和幂等 shutdown。若使用共享解析队列，结果仍应按 request ID 和 session 所有权隔离。

工作线程的最终输出必须已经转换成 `TiledMeshDocument`。不要返回第三方 AST、DOM、scene graph、图片对象或指向解析缓存的裸指针。

### 25.5 生成通用 document

adapter 应逐项检查：

- 根和子内容 URI 是否用 `resolve_uri()` 正确解析。
- source origin、轴变换、单位比例和局部顶点是否满足坐标契约。
- selection center 和 bounding volume 是否仍是 double 源坐标。
- 每个 `provider_key` 是否全局稳定唯一。
- 局部 parent 是否只指向更早定义。
- ADD/REPLACE 是否正确。
- 需要独立替换的空间区域是否有独立 group。
- 多内容的 `required_for_display` 是否正确。
- CPU 字节估算和 AABB 是否有效。
- 不支持但可跳过的内容是否写入 warning。
- 数据不合法时是否返回明确 Error 和 error message。

### 25.6 注册和卸载

在 scene 初始化级注册 Resource 类、创建格式队列并注册 loader。反初始化时按“停止 session/任务—注销 loader—销毁队列—释放第三方静态状态”的安全顺序执行。

### 25.7 测试

新 adapter 至少应覆盖：

- 显式选择和自动识别。
- 错误扩展名但有效 magic，以及正确扩展名但损坏数据。
- options 默认值、不兼容 options 和快照线程安全。
- 根几何、外部内容和动态追加子树。
- 每种格式提供的 selection 和 refinement 语义。
- 快速 reload、取消、节点删除和引擎退出。
- 大坐标、轴变换和单位比例。
- adapter 禁用时通用模块仍可构建和明确失败。

可参考 `tests/test_tiled_mesh_format.h` 中的 `SyntheticLoader`/`SyntheticSession`，以及 `modules/osgdb/tests/test_osg_scene_converter.h`。

## 26. 构建和验证

通用模块要求启用线程和 3D，当前声明支持 Windows、LinuxBSD 和 Web。

Windows 示例：

```powershell
scons platform=windows target=editor dev_build=yes tests=yes `
    module_tiled_mesh_3d_enabled=yes module_osgdb_enabled=yes `
    accesskit=no d3d12=no -j6
```

验证通用模块独立构建：

```powershell
scons platform=windows target=editor dev_build=yes tests=yes `
    module_tiled_mesh_3d_enabled=yes module_osgdb_enabled=no `
    accesskit=no d3d12=no -j6
```

测试过滤器：

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe `
    --headless --test '--test-case=*TiledMesh3D*' '--no-version'

.\bin\godot.windows.editor.dev.x86_64.console.exe `
    --headless --test '--test-case=*OSGDB*' '--no-version'
```

修改 ClassDB 绑定后还应运行 `--doctool`，确认 `doc_classes` 与生成 API 一致。

## 27. 演进原则

后续扩展优先遵循以下判断：

- 如果能力与具体数据格式无关，并且多个 adapter 都会需要，应放在 `tiled_mesh_3d`。
- 如果能力解释某种容器、元数据、压缩或坐标参考系统，应放在对应格式 adapter。
- 如果新增数据可以用现有 `TiledMeshDocument` 表达，不应给节点增加格式专用属性。
- 如果公共模型确实无法表达新语义，应先用合成 adapter 测试证明通用语义，再扩展数据结构和运行时。
- 任何新增异步阶段都必须继续携带 owner、generation 和可取消 request ID。
- 任何新增 GPU 资源都必须明确主线程创建位置、内存统计方式和关闭/驱逐释放路径。

架构验收标准保持不变：新增一种合成或真实格式，只需要实现并注册 adapter，不修改通用节点、调度器、渲染器或编辑器插件。

### 相机选择缓存与请求补充

相机、投影、视口和拓扑未改变时可复用 desired/priority 结果。若仍有 desired 内容因并发限制或 mesh pack 背压等待，请求调度必须继续运行，直至所有候选进入请求流程。`streaming_requests_pending` 记录该状态，不要求相机移动才能补充请求。统计中的 `waiting_contents` 表示已选中但尚未开始请求的内容，不能仅凭 `requested_contents == 0` 判断加载完成。
