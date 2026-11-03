# DigitalViewer Plugin Bridge（替代 MCP 的接入方案）

> 目标：Codex / Harness 通过插件接入 DigitalViewer 运行时场景，不依赖 MCP；产品仍是不可变发布包，插件只能操作运行时场景，不能读仓库、改源码、跑工程测试。

## 1. 当前代码基线

| 现有实现 | 位置 | 复用方式 |
| --- | --- | --- |
| MCP HTTP/JSON-RPC 服务 | `scripts/desktop/mcp_server.gd` | 保留为可选传输；抽出工具与鉴权 |
| 工具清单 | `mcp_server.gd:6` | 迁移到 `automation_service.gd` |
| 工具分发 | `mcp_server.gd:265` | 迁移到 `automation_service.gd` |
| 启动 MCP | `shell.gd:54-61` | 改为按模式启动 |
| 全局 bridge 引用 | `session.gd:17` | 可指向 MCP 或 Plugin Bridge |
| 场景描述/审计 | `session.gd:173,183` | 两个传输共用 |
| 脚本草案 | `mcp_server.gd:278` → `AppState.draft_received` → `shell.gd:803` | 不变 |
| 脚本运行 | `mcp_server.gd:285` | 只打开工作台，不代用户确认 |
| 信任门 | `script_workbench.gd:98` / `script_runner.gd:63` | 不变，插件不得绕过 |
| MCP 设置 | `shell.gd:750-801` | 扩展为 Off / MCP / Plugin |

当前 MCP 已经不是“没有实现”的状态；插件方案应复用同一套 `AppState`、脚本运行器和工作台，而不是重写一套自动化后端。

## 2. 总体架构

```text
Codex / Harness
  └─ DV Agent Plugin
       └─ HTTP JSON-RPC  /bridge
            └─ PluginBridge（DigitalViewer 内置 Node）
                 ├─ BridgeAuth（token / permissions）
                 ├─ AutomationService（7 个工具）
                 ├─ AppState / views / scripts / runner
                 └─ Audit
```

MCP 和 Plugin 是同一层工具的两个传输适配器：

```text
AutomationService
  ├─ MCP transport
  └─ Plugin transport
```

禁止出现第二套场景/脚本执行逻辑。

## 3. 传输选择

推荐 **本机 loopback HTTP + JSON-RPC 2.0**，不实现 MCP 的 `initialize` / `tools/list` / session。

原因：

- 当前 `mcp_server.gd` 已有 HTTP 解析、token、权限、请求大小限制；
- Codex/Harness 插件可用任意 HTTP 客户端；
- 不需要 WebSocket 握手、重连和心跳；
- 事件需求可先用 `script.inspect` 轮询，后续再加 SSE。

端点：

```text
POST http://127.0.0.1:8731/bridge
Authorization: Bearer <token>
Content-Type: application/json
X-DV-Bridge: 1
```

不暴露 `/mcp`；若用户选择 Plugin 模式，则 `mcp_server.gd` 不启动。

## 4. 协议

### 能力发现

```json
{"jsonrpc":"2.0","id":1,"method":"bridge.capabilities","params":{}}
```

响应：

```json
{
  "jsonrpc":"2.0",
  "id":1,
  "result":{
    "protocol":"dv.bridge.v1",
    "tools":["scene.describe","viewport.list","script.inspect","script.propose","script.run","capture.screenshot","audit.list"],
    "permissions":{"scene.read":true,"script.run":false,"capture":false}
  }
}
```

### 提交脚本

```json
{
  "jsonrpc":"2.0",
  "id":2,
  "method":"script.propose",
  "params":{
    "source":"extends Node\n\nfunc run(viewer) -> void:\n\tpass",
    "filename":"external_draft.gd"
  }
}
```

响应：

```json
{"jsonrpc":"2.0","id":2,"result":{"content":[{"type":"text","text":"{\"status\":\"review_required\",\"source_hash\":\"…\",\"executed\":false}"}],"isError":false}}
```

应用侧仍然通过 `AppState.draft_received` 打开工作台；插件不能自动确认信任。

### 请求运行

```json
{"jsonrpc":"2.0","id":3,"method":"script.run","params":{}}
```

响应：

```json
{"jsonrpc":"2.0","id":3,"result":{"content":[{"type":"text","text":"{\"status\":\"manual_run_required\",\"executed\":false}"}],"isError":false}}
```

`script.run` 只 `AppState.workbench.show()`，不按信任按钮、不调用 `run_source`。

### 其他工具

| 方法 | 结果 |
| --- | --- |
| `scene.describe` | `AppState.describe()` |
| `viewport.list` | 视口/活动视口摘要 |
| `script.inspect` | 脚本文档、运行状态、source hash |
| `capture.screenshot` | base64 PNG |
| `audit.list` | 审计条目 |

错误约定：

- HTTP 401：token 无效
- HTTP 403：Origin/非 loopback
- HTTP 413：请求体超限
- JSON-RPC `-32600/-32601/-32602`：协议错误
- 工具错误放在 `result.error`，带 `isError` 语义

## 5. 应用侧目标重构（稳定化，后续）

当前最小实现通过继承 `mcp_server.gd` 复用工具和鉴权；为避免长期耦合，下一阶段应拆成以下模块。

### 5.1 新增 `scripts/desktop/automation_service.gd`

从 `mcp_server.gd` 抽出：

```gdscript
extends RefCounted
const TOOL_NAMES := [...]
var permissions: Dictionary

func call_tool(name: String, args: Dictionary) -> Dictionary:
	# 现有 mcp_server.call_tool 的逻辑
```

依赖：

- `AppState.describe()`
- `AppState.scripts.describe()`
- `AppState.runner`
- `AppState.draft_received`
- `AppState.workbench`
- `AppState.views`
- `AppState.audit_event`

MCP 与 Plugin 都调用它。

### 5.2 新增 `scripts/desktop/bridge_auth.gd`

把 `mcp_server.gd` 的 token 逻辑抽出：

- `ensure_token()`
- `replace_token()`
- `reset_token()`
- `token_path`
- `permissions`
- `audit_event`

MCP/Plugin 共用 `user://bridge/bearer_token`；保留对旧 `user://mcp/bearer_token` 的迁移读取。

### 5.3 新增 `scripts/desktop/bridge_http.gd`

从 `mcp_server.gd` 抽出 HTTP 解析/响应：

- Header/Content-Length 限制
- loopback 检查
- `/bridge` 路由
- JSON 解析
- 401/403/413/415
- 64 KiB/128 KiB 限制

`mcp_server.gd` 和 `plugin_bridge.gd` 复用，不改现有 MCP 行为。

### 5.4 新增 `scripts/desktop/plugin_bridge.gd`

```gdscript
extends Node
var enabled := false
var token := ""
var permissions := {...}
var port := 8731

func start() -> int
func stop() -> void
func _process(delta: float) -> void
```

路由：

- `POST /bridge`
- `GET /bridge/health`（可选）
- 不暴露 `/mcp`

内部流程：

```text
auth -> parse -> bridge.capabilities or automation_service.call_tool -> JSON response
```

### 5.5 `shell.gd`

当前 `_ready()`：

```gdscript
AppState.bridge = Bridge.new()
add_child(AppState.bridge)
AppState.bridge.start()
```

改为：

```gdscript
var mode := settings automation_mode  # "off" | "mcp" | "plugin"
if mode == "mcp":
	AppState.bridge = McpBridge.new()
elif mode == "plugin":
	AppState.bridge = PluginBridge.new()
if AppState.bridge:
	add_child(AppState.bridge)
	AppState.bridge.start()
```

兼容 `--dv-plugin-check`、`--dv-mcp-check`。

### 5.6 `session.gd`

`AppState.bridge` 类型保持 Node；`shutdown()` 已调用 `bridge.stop()`，无需改。

### 5.7 设置页

当前 `shell.gd:750` 只配置 MCP；改为：

```text
自动化模式：Off / MCP / Plugin
Endpoint: 127.0.0.1:8731/bridge 或 /mcp
权限：scene.read / script.run / capture
Token: Copy / Use existing / Reset
说明：Plugin 模式不启动 MCP；外部 AI 不能修改产品
```

MCP 和 Plugin 互斥，避免两个端口同时开放。

## 6. Agent 插件

目录：

```text
plugins/
  dv_bridge_client/          # 共享 HTTP/JSON-RPC 客户端
  codex-digitalviewer/
    manifest.json
    tools.json
    src/index.ts
  harness-digitalviewer/
    manifest.json
    tools.json
    src/index.ts
```

### 工具映射

| Agent 工具 | Bridge 方法 |
| --- | --- |
| `dv_scene_describe` | `scene.describe` |
| `dv_viewport_list` | `viewport.list` |
| `dv_script_inspect` | `script.inspect` |
| `dv_script_propose` | `script.propose` |
| `dv_script_request_run` | `script.run` |
| `dv_capture` | `capture.screenshot` |
| `dv_audit` | `audit.list` |

### 插件配置

```json
{
  "endpoint": "http://127.0.0.1:8731/bridge",
  "token": "…",
  "protocol": "dv.bridge.v1"
}
```

Token 不写入日志；插件不把 token 放进 URL。

### 运行语义

- `propose` 只返回 `review_required`；
- `request_run` 只返回 `manual_run_required`；
- 不做自动确认；
- 轮询 `script.inspect` 读取运行/错误；
- 不能访问仓库、源码、构建、终端。

## 7. 实现状态（2026-10-28）

已落地最小可用实现：

- `scripts/desktop/mcp_server.gd`：增加 `is_plugin()` 和 `/bridge` 分支；MCP 的 `/mcp` 行为保持不变。
- `scripts/desktop/plugin_bridge.gd`：继承同一 HTTP 服务，只把协议切到 `/bridge`。
- `scripts/desktop/shell.gd`：支持 `--dv-plugin-check`、MCP/Plugin 设置切换和独立 token 路径。
- `tools/Test-BridgeClients.py`：同一进程外测试插件与 MCP 两个端点。

已验证：

```text
PLUGIN_BRIDGE_OK tools=7 propose=review permission_denied=1 audit=1
MCP_WIRE_OK tools=7 initialize=1 scene_describe=1
```

工具调用结果沿用现有 MCP 的 `content` 包装；`bridge.capabilities` 直接返回能力对象。Agent 插件应解析 `content[0].text` 中的 JSON。

## 8. 分阶段实现


### 阶段 1：抽取服务（不改行为）
- 新增 `automation_service.gd`、`bridge_auth.gd`、`bridge_http.gd`。
- `mcp_server.gd` 改为委托。
- 现有 MCP 测试全部通过。

### 阶段 2：Plugin Bridge
- 新增 `plugin_bridge.gd`。
- `shell.gd` 支持 `plugin` 模式。
- 设置页模式切换。
- 测试 `/bridge`、token、权限、工具、草案、手动运行。

### 阶段 3：Agent 插件
- 实现 `dv_bridge_client`。
- 先接 Codex 或 Harness 其中一个。
- 完成 `scene.describe` / `script.propose` / `script.inspect`。
- 再接 capture/audit。

### 阶段 4：打包与回归
- Plugin node 随应用发布；不动态加载用户插件。
- 增加 `--dv-plugin-check` headless 测试。
- 验证 Windows 导出、Web/移动不启动。
- 文档：插件安装、token、权限、手动运行。

## 9. 测试清单

- `bridge.capabilities` 返回 7 个工具和权限。
- 无 token / 错 token → 401。
- 非 loopback / Origin → 403。
- 非法 JSON / 超限 → 协议/413。
- `script.propose` → `draft_received` → 工作台打开。
- `script.run` → `manual_run_required`，不自动执行。
- 权限关闭 `script.run` → 拒绝。
- `capture.screenshot` 返回 PNG；headless 返回错误。
- `audit.list` 记录调用。
- 插件断开不影响运行中的脚本。
- Web/mobile 不启动。
- 旧 MCP 测试不回归。

## 10. 风险与取舍

- **插件安装/版本矩阵**：Agent 插件必须适配 Codex/Harness 的插件 API；需要明确 manifest/tool schema。
- **双传输维护**：MCP 与 Plugin 共存时，需共享 `AutomationService`，否则会出现行为分叉。
- **安全**：插件不是沙箱；仍依赖 token、permissions、工作台信任门。
- **Web/移动**：无本机 loopback，插件模式不适用。
- **产品不可变**：PluginBridge 随官方包发布或企业签名包；不允许 AI 安装插件。
- **事件**：v1 用 `script.inspect` 轮询；v1.1 再加 SSE。

## 11. 结论

如果 Codex/Harness 必须使用插件：

1. 保留 `AppState`、`script_runner`、`script_workbench` 不动。
2. 把 `mcp_server.gd` 的工具分发抽成 `AutomationService`。
3. 用 `PluginBridge` 提供 `/bridge` HTTP JSON-RPC。
4. 用共享客户端实现 Codex/Harness 插件。
5. Plugin 模式关闭 MCP，避免双入口。
6. 插件只能提议脚本；运行仍由用户在应用内确认。

这不是“另做一套自动化”，而是给现有 MCP 工具层换一个传输适配器。
