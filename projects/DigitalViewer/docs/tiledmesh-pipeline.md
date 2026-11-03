# TiledMesh 数据加载、解析与渲染流程

本文描述当前 DigitalViewer 中 `TiledMeshInstance3D` 从打开数据到提交 GPU 渲染的完整流程，覆盖 OSGB/OSG 与 3D Tiles 两条格式路径。

## 1. 参与模块

| 模块 | 主要文件 | 职责 |
|---|---|---|
| 图层/数据管理 | `scripts/desktop/data_store.gd` | 创建 TiledMesh 实例，设置预算，轮询状态 |
| TiledMesh 运行时 | `modules/tiled_mesh_3d/tiled_mesh_instance_3d.cpp` | 流式选择、请求、上传、RenderingServer 实例管理 |
| 数据请求队列 | `modules/tiled_mesh_3d/tiled_mesh_data_request_queue.cpp` | 本地文件读取、HTTP 请求与轮询 |
| Mesh Pack 队列 | `modules/tiled_mesh_3d/tiled_mesh_mesh_pack_queue.cpp` | 将解析结果打包成 GPU 可提交的表面数据 |
| 图片解码队列 | `modules/tiled_mesh_3d/tiled_mesh_image_decode_queue.cpp` | 外部纹理解码 |
| OSG 格式 | `modules/osgdb/osg_tiled_mesh_format.cpp` | OSG/OSGB 解析会话 |
| OSG 解析队列 | `modules/osgdb/osg_parse_queue.cpp` | 单线程 OSG 解析 |
| 3D Tiles 格式 | `modules/3dtiles/tiles_3d_format.cpp` | tileset/glTF/b3dm/i3dm/pnts/cmpt 解析会话 |
| 3D Tiles 解析队列 | `modules/3dtiles/tiles_3d_format.cpp` | 单线程 3D Tiles 解析 |

## 2. 总体流水线

```text
Layer / DataStore
    |
    v
TiledMeshInstance3D.open()
    |
    v
[Data Queue] 本地读文件 / HTTP 下载
    |
    v
[Format Parse Queue] OSG 或 3D Tiles 解析
    |
    v
[TiledMeshMeshPackQueue] 打包 Surface / 索引 / 拾取数据
    |
    v
[Main Thread Upload] 创建 RenderingServer mesh / material / instance
    |
    v
RenderingServer / Vulkan Forward+ 渲染
```

纹理还有一条并行支路：

```text
解析结果发现外部纹理
    |
    v
[TiledMeshImageDecodeQueue] 后台解码
    |
    v
[Main Thread] 创建 ImageTexture / 绑定到 StandardMaterial3D
```

## 3. 打开阶段

`DataStore._instantiate_tiled()` 创建 `TiledMeshInstance3D`，并设置：

```gdscript
source_uri
max_tile_size_mb = 64
max_concurrent_requests = 8
upload_budget_ms = 4.0
world_origin_mode = EXPLICIT
format_options
```

随后调用：

```gdscript
node.open()
```

`TiledMeshInstance3D::open()`：

1. 递增 generation，清理旧流状态和旧渲染实例。
2. 创建 `__root__` 根瓦片。
3. 将根 URI 作为第一个 `TiledMeshContentRequest` 入 Data Queue。
4. 根请求优先级设为 `1e15`。
5. 设置 `open_state = true`、`streaming_topology_dirty = true`。
6. 发出初始流式统计。

根请求完成之前没有可渲染内容。

## 4. 数据请求

`TiledMeshDataRequestQueue` 负责字节获取。

### 本地文件

- 由独立 `TiledMesh Data` 线程处理。
- 单线程按优先级读取本地字节。
- 读取结果放回结果队列，由主线程 `poll()` 取走。

### HTTP

- 使用 `HTTPClient`。
- 最多 `max_active_requests` 个活跃请求，默认 8；Web 默认 4。
- HTTP `poll()` 在主线程推进。
- 有超时、重试、取消和优先级更新。

### 结果

数据结果包含：

```text
request_id
owner
generation
uri
bytes
error
error_message
```

主线程 `_poll_data_results()` 把结果交给解析阶段。

## 5. 解析阶段

解析由格式会话负责：

```text
TiledMeshFormatSession
  -> OsgTiledMeshSession
  -> Tiles3DSession
```

### OSG / OSGB

`OsgTiledMeshSession::enqueue_parse()`：

- 将字节、URI、优先级送入 `OsgParseQueue`。
- 解析在独立的 `OSGDB Parse` 线程执行。
- 解析结果包含：
  - 几何表面
  - 材质
  - 外部瓦片引用
  - LOD/PagedLOD 信息
  - 局部 AABB
  - 源坐标原点

### 3D Tiles

`Tiles3DSession`：

- 解析 `tileset.json`。
- 解析 b3dm/i3dm/pnts/cmpt。
- 提取内嵌 glTF。
- 解析 glTF 材质、纹理、网格和子瓦片。
- `KHR_materials_unlit` 被记录为材质 `unlit` 标记。

两种格式最终都产出统一的 `TiledMeshDocument`。

## 6. Mesh Pack 阶段

解析结果经过 `TiledMeshMeshPackQueue`，把文档转换为适合上传的：

- Surface 顶点/索引
- 法线/UV/顶点色
- 材质索引
- 拾取用三角形数据
- 表面 AABB
- 外部纹理引用

这个阶段在独立的 `TiledMesh Mesh Pack` 线程执行。

## 7. 图片解码

发现外部纹理后，`_update_texture_requests()` 将纹理请求送入 `TiledMeshImageDecodeQueue`。

- 独立 `TiledMesh Images` 线程解码。
- 解码结果由主线程 `_poll_texture_results()` 取回。
- 主线程创建 `ImageTexture` 并设置到对应材质。

## 8. 主线程上传

`_upload_ready_contents()` 是 GPU 资源创建阶段，运行在主线程。

每个待上传内容包含一个 `TiledMeshPackedDocument`。

上传流程：

1. 选优先级最高的 ready content。
2. 检查时间预算和字节预算。
3. `_upload_document()`：
   - 遍历 render group/surface。
   - 提取 `SurfaceData`。
   - 创建/复用 `StandardMaterial3D`。
   - 设置 albedo、metallic、roughness、emission、透明度、双面、alpha scissor。
   - 绑定外部纹理。
   - 调用 `RenderingServer.mesh_create_from_surfaces()`。
   - 创建 `RenderingServer` instance。
   - 设置 mesh、transform、layer mask、可见性和阴影模式。
   - 保存拾取表面。
4. 注册该文档产生的子瓦片。
5. 更新流式可见性和纹理请求。

预算参数：

```text
upload_budget_ms
byte_budget = 16 MiB
max_tile_size_mb = 64 MiB
```

注意：预算检查发生在内容块之间，单个 `_upload_document()` 内部仍可能超过时间预算。

## 9. 渲染实例

每个 tile/render group 通常对应一个 `RID instance`。

`_sync_render_instance()` 设置：

```text
scenario
global transform
layer mask
visible
cast shadow
```

`_sync_render_instances()` 在所有场景/可见性变化后同步。

材质由 `StandardMaterial3D` 创建，`KHR_materials_unlit` 或 `receive_lighting_mode = Unlit` 时使用：

```cpp
BaseMaterial3D::SHADING_MODE_UNSHADED
```

否则使用：

```cpp
BaseMaterial3D::SHADING_MODE_PER_PIXEL
```

阴影由独立 `cast_shadow` 控制，和 shading mode 无关。

## 10. 每帧流式循环

`TiledMeshInstance3D` 在 `NOTIFICATION_INTERNAL_PROCESS` 中执行：

```text
_update_streaming(camera)
_poll_data_results()
_poll_texture_results()
_poll_parse_results()
_poll_mesh_pack_results()
_upload_ready_contents()
[拓扑变化时再次 _update_streaming]
_sync_render_instances()  // 仅 layer mask/visible 变化时
```

### `_update_streaming`

1. 收集相机状态：transform、projection、viewport size。
2. 如果相机和拓扑都没变，只补发待处理请求，跳过全量选择。
3. 遍历全部 streaming tiles：
   - 判断父节点是否 desired。
   - 判断距离/屏幕空间误差 LOD 规则。
   - 判断相机视锥相交。
   - 计算请求优先级。
   - 更新 desired/display_desired。
4. 对不需要的内容取消 Data/Parse/Pack 请求。
5. 请求新内容。
6. 更新可见性。
7. 更新纹理请求。
8. 执行内存预算淘汰。

### 可见性

`_update_streaming_visibility()`：

- 根据 `display_desired`、LOD 和内容状态选择可见 render instance。
- 更新 `RenderingServer.instance_set_visible()`。

## 11. 状态机

流式内容大致经历：

```text
REQUESTED
  -> DOWNLOADED
  -> PARSING
  -> CPU_READY
  -> PACKING
  -> UPLOADING
  -> RESIDENT
```

异常/取消：

```text
FAILED
CANCELLED
```

根节点状态会通过 `streaming_stats_changed` 和 `root_loaded` 通知外部。

## 12. 内存与淘汰

`_enforce_memory_budget()` 按 `memory_budget_mb` 淘汰不再需要的 resident 内容。

当前预算由 DataStore 动态设置：

```gdscript
memory_budget_mb = 1024 / tiled_layer_count
upload_budget_ms = 4.0 / tiled_layer_count
```

多个 TiledMesh 图层时，每个图层独立拥有预算，没有全局统一调度。

## 13. 线程模型

| 工作 | 执行线程 |
|---|---|
| 本地文件读取 | 单个 `TiledMesh Data` 线程 |
| HTTP 轮询 | 主线程 |
| OSG 解析 | 单个 `OSGDB Parse` 线程 |
| 3D Tiles 解析 | 单个 `3D Tiles Parse` 线程 |
| Mesh Pack | 单个 `TiledMesh Mesh Pack` 线程 |
| 图片解码 | 单个 `TiledMesh Images` 线程 |
| 材质/mesh/instance 创建 | 主线程 |
| LOD/可见性/预算 | 主线程 |
| GPU 渲染提交 | 主线程（Godot 默认 Safe 渲染线程模型） |

## 14. 当前性能特征

桃园市政府 OSGB 实测：

```text
GPU 总计：平均 2.34ms，最大 8.10ms
主线程流式阶段：平均 22.41ms，P95 42.50ms
显存峰值：约 2.49GB
Draw Calls：平均 1116，最大 2576
```

结论：

- GPU 不是主要瓶颈。
- 主线程的流式选择、上传、材质创建是主要帧时间来源。
- 高瓦片数和纹理驻留导致显存压力。
- 单个文档上传可能突破 4ms 预算。
- 没有全局上传预算和并行解析线程池。

## 15. 已知限制

1. 每个 TiledMesh 独立预算，多图层总预算不可控。
2. `_upload_document()` 内部没有细粒度时间切片。
3. LOD 选择在拓扑或相机变化时全量遍历。
4. OSG/3D Tiles/Mesh Pack/Image Decode 都是单工作线程。
5. HTTP 轮询在主线程。
6. 纹理驻留和显存预算不是全局统一。
7. 无持久化磁盘瓦片缓存，重复打开依赖 OS 文件缓存。

## 16. 关键函数索引

```text
TiledMeshInstance3D::open
TiledMeshInstance3D::_update_streaming
TiledMeshInstance3D::_request_streaming_contents
TiledMeshInstance3D::_poll_data_results
TiledMeshInstance3D::_poll_texture_results
TiledMeshInstance3D::_poll_parse_results
TiledMeshInstance3D::_poll_mesh_pack_results
TiledMeshInstance3D::_upload_ready_contents
TiledMeshInstance3D::_upload_document
TiledMeshInstance3D::_register_tiles
TiledMeshInstance3D::_update_streaming_visibility
TiledMeshInstance3D::_enforce_memory_budget
TiledMeshInstance3D::_sync_render_instances
```
