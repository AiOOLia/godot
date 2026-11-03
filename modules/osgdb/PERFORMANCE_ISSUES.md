# OSGDB 模块性能问题清单

本文记录当前 `modules/osgdb` 已确认或需要通过性能分析验证的性能问题，作为后续优化 backlog。本文不表示对应功能存在正确性缺陷。

## 优化约束

- 不在 OSG 原始 LOD 范围之外提前预加载 Tile。
- 始终优先处理当前视口中视觉贡献最高的已知 Tile。
- 不以牺牲父级 fallback、无空洞切换、取消安全或编辑器预览正确性换取性能。
- 当前裁剪版 OSG 的引用计数不是原子操作。在完成线程安全改造和验证前，OSG 解析及 OSG 对象析构必须继续位于同一个串行工作线程。
- 所有优化都需要在 Release/生产构建和典型 GIS 数据集上用 profiler 验证；单元测试通过不能证明性能最优。

## 优先级汇总

| ID | 优先级 | 问题 | 主要影响 |
| --- | --- | --- | --- |
| PERF-000 | P0 | 缺少统一性能基线和分阶段耗时指标 | 无法准确判断真正瓶颈和优化收益 |
| PERF-001 | P0 | 完成结果依赖每帧全量轮询和线性查找 | Tile 数量增大后主线程空转成本持续上升 |
| PERF-002 | P0 | 调度、请求准入和上传选优使用重复线性扫描 | 相机移动和细节集中更新时帧率下降 |
| PERF-003 | P0/P1 | 已完成纹理动态优先级和离屏暂停；尚无跨 Tile 缓存与 mipmap | 重要 Tile 的纹理顺序已改善，重复纹理和远距采样仍待优化 |
| PERF-004 | P1 | HTTP 请求调度存在重复扫描 | 大量 HTTP 请求等待时调度成本偏高 |
| PERF-005 | P1 | SurfaceData 已移出主线程；OSGB 解析前后仍存在可减少的数据复制和数组扩容 | 大 Tile 的主线程停顿已降低，峰值内存和解析分配仍需优化 |
| PERF-006 | P1 | 本地读取和图片解码各只有一个工作线程 | SSD 和多核 CPU 利用不足，纹理完成速度受限 |
| PERF-007 | P1 | GPU 材质、纹理、surface、阴影和透明渲染尚未系统优化 | draw call、shader 切换、overdraw 和显存占用偏高 |
| PERF-008 | P2 | 内存预算、统计信息和可见性维护仍有全量扫描 | 超大数据集及编辑器长时间预览时产生额外 CPU 开销 |
| PERF-009 | P2/研究 | 单个 OSG 解析线程构成最终吞吐上限 | 深层级连续细化的最短完成时间受限 |
| PERF-010 | 约束 | 无预加载模式下，后代 Tile 必须等待父文件被解析后才能发现 | 深层 PagedLOD 存在不可消除的串行依赖延迟 |

## 详细问题

### PERF-000：建立可重复的性能基线

现状：

- 当前测试主要验证解析、LOD、生命周期和渲染切换正确性。
- 缺少读取、排队、解析、转换、图片解码、主线程上传和 GPU 渲染的分阶段统计。
- Dev 编辑器的耗时不能代表最终 Release 构建性能。

后续处理：

- 为每个请求记录 `queued/read/parse/cpu_ready/upload/resident/texture_ready` 时间戳。
- 增加队列深度、取消数、过期结果数、上传字节、可见 draw call、纹理缓存命中率等计数器。
- 使用固定相机轨迹分别测试本地 SSD、HTTP、Windows/Linux、编辑器和运行时。
- 至少保留武当山、青菱桥和一个大规模城市数据集的可重复基准结果。

完成标准：能够区分 I/O、解析、主线程上传和 GPU 四类瓶颈，并能量化后续每项优化的收益及回归。

### PERF-001：改为事件驱动的完成结果分发

现状：

- `_poll_data_results()` 每帧遍历全部 `streaming_tiles`。
- `_poll_streaming_results()` 每帧再次遍历全部 `streaming_tiles`。
- `_poll_texture_results()` 每帧遍历全部纹理请求。
- `OsgDataRequestQueue::take_result()`、`OsgParseQueue::take_result()` 和 `OsgImageDecodeQueue::take_result()` 都在线性链表中按 request ID 查找结果。

问题：没有任务完成时仍会扫描；同时完成任务较多时形成“Tile 扫描 × 结果链表查找”的组合成本。

后续处理：

- 工作队列发布完成事件，由主线程一次取出当前 owner 的全部完成项。
- 维护 `request_id -> tile/stage` 映射，直接定位目标 Tile。
- 事件必须携带 owner、generation 和 request ID，继续丢弃旧 generation 结果。
- 节点关闭和销毁时清理映射，并保持取消及析构安全。

完成标准：没有完成事件时轮询成本接近 O(1)；有完成事件时成本与本帧完成数量相关，而不是与全部历史已知 Tile 数量相关。

### PERF-002：active frontier、dirty 集合和优先堆

现状：

- 相机变化时 `_update_streaming_from_cameras()` 对所有已知 Tile 和所有相机重新计算范围及投影贡献。
- `_request_streaming_tiles()` 每准入一个请求都会重新扫描全部 Tile，并再次统计较高优先级的 pending Tile。
- `_upload_streaming_tiles()` 先收集全部 CPU_READY Tile，再反复线性寻找最高优先级项。
- 解析和本地读取队列也通过扫描链表选择最高优先级请求。

后续处理：

- 只遍历根到当前活动 LOD frontier 的可达分支，不投影已经脱离活动层级的全部历史 Tile。
- 用 dirty Tile 集合处理拓扑变化、相机跨阈值和状态完成事件。
- 请求、解析和 CPU_READY 上传阶段使用优先堆。
- 优先堆条目保存 priority revision/generation，使用惰性失效处理相机移动后的优先级更新。
- 保留当前“投影可见面积 + LOD 紧迫度 + fallback + 活动视口”的视觉贡献模型。
- 不扩大 LOD 请求范围，不引入提前预加载。

完成标准：静止相机无拓扑变化时不重建队列；相机移动成本主要与 active frontier 大小相关；相同测试相机轨迹下的 Tile 选择和父子切换行为不变。

### PERF-003：纹理优先级、缓存和 mipmap

已完成：

- 外部纹理继承所属内容的动态视觉优先级；读取、解码和主线程安装都会重排。
- 纹理读取使用低于几何内容的独立优先级区间，避免粗层纹理阻塞新发现的精细几何。
- 图片解码队列每次选择当前最高优先级等待任务。
- Tile 离开 desired 集合、渲染分支被细化隐藏或移出活动视锥后，取消未完成纹理工作并保留描述，重新可见时按新优先级恢复。

剩余问题：

- 相同 URI 的纹理只有并发下载合并，没有持久的解码图像或 GPU 纹理缓存。
- 每个 Tile 独立创建 `ImageTexture`；当前上传路径不主动生成 mipmap。

后续处理：

- 增加规范化 URI 级缓存，区分 encoded bytes、decoded image 和 GPU texture，并纳入统一内存预算。
- 多个材质共享同一个纹理资源；最后一个引用释放后再进入 LRU。
- 根据目标平台评估 mipmap、GPU 纹理压缩和后台压缩策略。
- 缓存键需要包含颜色空间、采样设置及会影响结果的解码选项。

完成标准：当前画面贡献最高 Tile 的纹理优先完成；重复纹理只读取、解码和上传一次；远距离纹理采样稳定且内存统计准确。

### PERF-004：HTTP 队列调度复杂度

现状：

- `poll()` 遍历全部 HTTP Job。
- 每个未启动 Job 会重新统计 active Job，并扫描是否存在更高优先级 Job。
- 更新或取消 subscriber 时还会重新扫描 Job 和 subscriber。

后续处理：

- 单独维护 active request 计数。
- 未启动 HTTP Job 使用支持动态优先级的 pending heap。
- 维护 request ID 和规范化请求键索引，继续支持相同 URI 请求合并。
- 已经开始传输的请求原则上不抢占；是否中断低价值大响应需要由基准数据决定。

完成标准：大量等待请求时，单帧 HTTP 调度不再出现 Job 数量平方级增长。

### PERF-005：减少解析和转换阶段的数据复制

已完成：

- 通用层新增独立、按动态视觉优先级调度的 `TiledMeshMeshPackQueue`。
- `mesh_create_surface_data_from_arrays()` 不再由主线程逐 surface 调用；工作线程生成 CPU-only `SurfaceData`，主线程按 render group 一次提交 `mesh_create_from_surfaces()`。
- 离开 desired 的等待/活动打包请求会取消，旧 generation 的结果不会上传；OSG 解析线程不承担通用 mesh 打包工作。
- 增加 packing、packed-ready、队列深度和在途原数组字节统计。

现状：

- `PackedByteArray` 会先复制到 `std::string`，再构造输入流交给 OSG reader。
- OSG Geometry 先写入 Raw surface，之后再转换到 Godot PackedArray。
- 合并兼容 surface 时多次 `resize()` 并逐元素追加，可能反复重新分配和复制已有数组。
- `TiledMeshDocument` 在格式结果和 mesh pack 请求之间按值传递；其中 PackedArray/Vector 通常使用写时复制，但原数组和 packed buffer 会在单个请求打包期间短暂共存，仍需验证实际峰值。

后续处理：

- 为 `PackedByteArray` 实现只读 memory stream buffer，避免构造完整 `std::string` 副本。
- 预统计兼容 surface 的总顶点和索引数量，一次分配目标 PackedArray。
- 审查能够安全使用移动语义或写时复制的 document/result 转移路径。
- 用 profiler 和内存峰值数据确认 Raw 中间层是否需要重构，不能让 OSG 对象逃出解析线程。

完成标准：不改变转换结果及 OSG 单线程生命周期；降低大 Tile 的总分配次数、峰值内存和 parse-to-CPU_READY 时间。

### PERF-006：本地读取与图片解码并发度

现状：

- 所有本地 Tile 和纹理文件由一个本地读取线程处理。
- 所有外部图片由一个图片解码线程 FIFO 处理。
- OSG 解析线程与二者可以流水并行，但各阶段内部吞吐仍是单任务。

后续处理：

- 本地读取和图片解码分别使用有上限的工作线程池，桌面和 Web 使用不同默认值。
- 保持动态优先级、取消、generation 检查和总内存上限。
- HDD、小文件、SSD 和 Web 环境分别基准测试，避免并发过高造成随机 I/O 或内存峰值恶化。
- OSG parser 不包含在此并行化范围内。

完成标准：多核/SSD 环境吞吐提升，同时帧时间、内存峰值和取消延迟不发生明显回归。

### PERF-007：GPU 资源和 draw call 优化

现状：

- 材质仅在单个上传组内复用，没有跨 Tile 材质缓存。
- 每个 fallback/LOD render group 创建独立 Mesh 和 RenderingServer instance；每个 surface 仍产生独立 mesh surface/draw call。
- 默认开启阴影；双面材质、透明 depth pre-pass 和大面积粗细 Tile 过渡可能增加 GPU overdraw。
- 缺少可见 draw call、三角形数量、阴影 pass 和纹理显存的模块级统计。

后续处理：

- 先用 RenderingServer/GPU profiler 分离 CPU submit、顶点处理、像素 overdraw 和阴影成本。
- 增加兼容材质的跨 Tile缓存，评估材质参数表/纹理数组等批处理方案。
- 在不破坏 fallback group 和 inline LOD 独立显隐的前提下减少 surface 和 instance 数量。
- 为大型 GIS 场景提供明确的阴影策略和推荐默认值；评估远距离关闭阴影。
- 优化透明材质分类，避免把可使用 Alpha Scissor 的材质放入代价更高的透明路径。

完成标准：在固定视角下减少 draw call、材质切换、阴影开销或 overdraw，并保持 LOD 切换和材质视觉结果正确。

### PERF-008：增量内存、统计与可见性维护

现状：

- `_enforce_memory_budget()` 计算 resident bytes 和选择每个 LRU 驱逐对象时会全量扫描。
- `get_streaming_stats()` 扫描全部 streaming/render Tile；编辑器 Inspector 定期调用。
- 拓扑或 resident 状态变化时，可见性更新会扫描全部 render instance 并重建若干缓存。

后续处理：

- 状态迁移时增量维护 requested、resident、CPU/GPU bytes 等计数器。
- 不可见 resident Tile 使用 LRU 结构维护，避免每次驱逐重新寻找最老项。
- 将可见性更新限制在受影响的父子/分组子树。
- 调试统计和详细 Tile 数据按需采样，避免发布构建承担不必要开销。

完成标准：统计查询接近 O(1)；驱逐成本与实际驱逐数量相关；可见性变化只访问受影响分支。

### PERF-009：OSG 解析并行化研究

现状：当前裁剪版 OSG 的引用计数不是原子操作，因此全进程只使用一个 OSG parser worker。这是安全约束，不是可以直接删除的保守逻辑。

可研究方向：

- 验证并改造裁剪 OSG 的引用计数、Registry、插件静态状态和 reader 全局状态线程安全。
- 研究完全独立 parser context，或者桌面平台使用隔离进程解析。
- 先完成 PERF-001、PERF-002 和 PERF-005，并用 profiler 确认 parser 确实是主瓶颈。

完成标准：通过并发、取消、退出和压力测试证明无竞态、无跨线程 OSG 析构、无 Registry 状态竞争后，才允许增加 parser worker 数量。

### PERF-010：无预加载条件下的层级发现延迟

现状：PagedLOD 后代 URI 只有在父 OSGB 下载并解析后才会被发现。因此从粗糙层级到多级精细层级至少需要依次经历每一级的读取、解析和注册。

结论：优先队列只能在“已经发现且进入原始 LOD 范围”的 Tile 之间排序，无法消除尚未知晓的后代依赖。为遵守不提前预加载的要求，当前不把扩大请求范围作为解决方案。

后续可选方向：如果将来仍需降低深层级首达延迟，可研究离线生成仅包含层级、边界和 URI 的轻量索引。该索引只提前提供元数据，不提前读取 Tile 几何；是否接受这种方式需要单独评审。

## 推荐实施顺序

1. PERF-000：建立 Release 性能基线。
2. PERF-001：事件驱动完成队列和 request ID 直接映射。
3. PERF-002：active frontier、dirty 集合和各阶段优先堆。
4. PERF-003：纹理视觉优先级、共享缓存和 mipmap。
5. 根据 profiler 在 PERF-004、PERF-005、PERF-006、PERF-007 中选择真实主瓶颈。
6. PERF-008：面向超大数据集消除剩余全量维护路径。
7. 只有解析线程成为主导瓶颈时，才开展 PERF-009。
