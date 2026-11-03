# DigitalViewer 自动化 API / MCP

> 定位：v1.0 不内置 AI；DigitalViewer 只暴露运行时脚本 API 与本机 MCP。Codex、Harness、CLI 和 CI 作为外部 AI 客户端控制场景，不另建第二套运行时。

## 当前桌面实现（2026-10-27）

桌面应用启动 loopback HTTP 服务，支持 MCP `initialize`、`notifications/initialized`、`ping`、`tools/list`、`tools/call`、DELETE 会话关闭。采用 JSON 响应，不提供服务端 SSE 订阅；GET 返回 405。初始化支持协议 `2025-03-26`、`2025-06-18`、`2025-11-25`。

首次启动生成 256 位随机 Bearer 令牌，保存到 `user://mcp/bearer_token`（Windows 为 `%APPDATA%/Godot/app_userdata/DigitalViewer/mcp/bearer_token`）。重启应用、关闭/重新启用 MCP 均复用它；关闭服务只清除连接和 MCP 会话。设置中可“复制令牌”“使用已有令牌”“重置令牌”。使用已有令牌可保留 Codex 已配置的凭据；重置或更换会立即撤销旧令牌和会话。凭据保存失败时不回退为临时令牌。文件留在用户配置目录，不进入安装包、诊断或审计；Unix 文件权限为仅当前用户读写，Windows 使用用户目录继承权限。

客户端发送 `Authorization: Bearer <token>`，不放在 URL。初始化返回 `Mcp-Session-Id`，后续请求需要此头。Bearer 令牌长期有效，MCP 会话 ID 仍是临时的；重启后客户端重新初始化即可，不需要更换配置。默认允许 `scene.read`，`script.run` 和 `capture` 默认关闭。

JSON-RPC 请求 ID 从原始 JSON 中保留整数/字符串类型；`0` 返回 `0`，不再返回导致 Codex `JsonRpcMessage` 反序列化失败的 `0.0`。支持有符号 64 位整数（含大于 2^53 的值），无浮点舍入；拒绝分数、溢出、空值及重复 ID。场景数据中的浮点数不受影响。

`script.propose` 只接收 UTF-8 64 KiB 内的源码，在工作台新增独立脚本文档和图层，保留其它草稿及运行会话；不编译代码。当前 `script.run` 即使有权限也只返回 `manual_run_required`，必须由用户在应用中点击运行。文档首次运行或代码修改后须勾选信任；同一应用会话内已确认的原样代码可由用户直接重跑。该确认不会授予 MCP 自动执行权限，此版本没有自动化预授权执行模式。

`script.inspect` 返回 `scripts`（每份脚本的 ID、名称、运行状态、源码快照哈希及数据 ID）、`running_count` 和 `selected_script`；原有顶层 `running`、`status`、`source_hash` 保留，表示编辑器当前选中的脚本。

截图请求必须显式传 `viewport` ID。无头模式返回渲染不可用。只在已有渲染帧存在且已授权 `capture` 时返回内存中的 PNG。

HTTP 限制：只监听 127.0.0.1；拒绝浏览器 Origin、非 loopback Host、重复头、chunked 请求、超限请求；请求上限 128 KiB、头上限 8 KiB、并发连接上限 16、会话上限 64。响应分帧写入，慢客户端不会无限阻塞 UI。审计上限 250 项，不记录凭据和源码。

验证入口：`tools/Test-Desktop.ps1`。下面章节描述产品目标；尚未实现的受控自动执行、完整检查点与所有审计细节不应视为已交付。

`tools/Test-McpClient.py --codex <codex.exe>` 使用独立验证进程和临时 Codex 配置，检查严格 JSON ID、应用进程重启后的令牌复用，并通过实际 Codex app-server 加载 7 个工具、调用 4 个只读工具；不发起模型请求、不修改用户 Codex 配置。需先导出 Windows Validation 包，Python 3.11+ 标准库即可。测试服务使用 18731，产品服务仍为 8731。

## 架构

```text
DigitalViewer Core（不可变发布产品）
  ├─ Script API（GDScript / viewer 上下文）
  ├─ Automation API（本机）
  │    ├─ MCP transport
  │    ├─ Plugin Bridge transport
  │    ├─ Codex / Harness（外部 AI）
  │    ├─ CLI / CI / 批处理
  │    └─ 第三方集成
  └─ Audit / Checkpoint / Permissions
```

v1.0 没有应用内 AI、聊天、模型管理或 API Key 配置：

- 外部 AI：通过 MCP 或 Plugin Bridge 接入同一运行时；不获得额外场景权限。
- 用户：可在脚本面板手动编辑/运行，或让外部 AI 提交草案。
- 所有客户端都只能操作运行时场景，不接触仓库、代码、构建或测试。

## 本机端点

| 项 | 设计 |
| --- | --- |
| 端点 | `127.0.0.1:8731/mcp`（MCP）或 `127.0.0.1:8731/bridge`（Plugin） |
| 传输 | 本机 loopback；MCP 与 Plugin 二选一 |
| 认证 | 持久本机 Bearer 令牌，可手动导入/重置；不把令牌写进 URL |
| 权限 | `scene.read` / `script.run` / `capture` |
| 审计 | 每次调用记录客户端、脚本版本、检查点、结果 |
| 关闭 | 设置中可关闭；关闭后立即拒绝新会话 |

Web 版本不暴露本机 MCP/Plugin；企业 Web 通过受控网关接入，默认不开放任意本地命令。

### Plugin Bridge

Codex / Harness 可通过 Plugin Bridge 接入，不使用 MCP：

- 端点 `/bridge`，不执行 MCP `initialize` / `tools/list` 会话握手；
- `bridge.capabilities` 返回 `dv.bridge.v1`、7 个工具和权限；
- 工具结果沿用 `content[0].text` JSON 包装；
- 与 MCP 共用 `mcp_server.gd` 的工具分发、token、权限和 `script_runner`；
- `script.run` 只返回 `manual_run_required` 并打开工作台，不代用户确认；
- 设置中可在 MCP 和 Plugin 之间切换，两者互斥。

实现与测试：[Plugin Bridge](plugin-bridge.md)、[Test-BridgeClients.py](../tools/Test-BridgeClients.py)。

## 工具

| 工具 | 权限 | 用途 |
| --- | --- | --- |
| `scene.describe` | `scene.read` | 返回当前场景结构、视口、图层、相机摘要 |
| `viewport.list` | `scene.read` | 列出视口和活动视口 |
| `script.inspect` | `scene.read` | 查询所有脚本 ID、运行状态和源码快照哈希，以及所选脚本和运行数 |
| `script.propose` | `scene.read` | 提交 GDScript 草案；不执行 |
| `script.run` | `script.run` | 在用户确认/权限允许后运行脚本 |
| `capture.screenshot` | `capture` | 返回视口截图 |
| `audit.list` | `scene.read` | 查询脚本运行与权限审计 |

不提供：

- 任意 shell / 文件系统 / 网络工具；
- JSON 场景写接口；
- 绕过用户审查的自动执行；
- 修改工程、依赖、构建或安装包。

## 发布产品不可被 AI 修改

DigitalViewer 作为发布产品是**不可变运行时**：

- AI 不能修改 DigitalViewer 源码、二进制、插件、安装包、依赖或构建配置。
- AI 不能安装插件、注入原生代码或改项目文件。
- 应用更新只来自官方签名发布，不来自 AI。
- AI 生成的 GDScript 是**运行时脚本**，不是产品代码；停止/关闭会话后清理，不写回安装目录。

## v1 没有内部 AI

v1.0 **不内置 AI 助手**，没有聊天、Composer、模型管理或 API Key 配置。唯一 AI 入口是外部 AI（Codex / Harness / CLI）通过本机 MCP 连接运行时场景。

- 外部 AI 是**运行时场景客户端**，通过 MCP 或 Plugin Bridge 使用同一脚本 API。
- 外部 AI 的差异只在客户端位置、模型、认证和交互方式，不在权限范围。
- 外部 AI 只能：读取场景、提交脚本草案、在授权后运行脚本、截图、查看审计。
- 外部 AI 不能：读仓库、改代码、跑测试、访问文件系统或终端、修改产品。
- 外部 AI 与用户手动脚本共享：脚本 API、权限、检查点、审计、错误格式。

“工程开发 / 改源码 / 跑测试”不属于 DigitalViewer 产品内能力，也不在本设计范围内。需要修改产品本身时，走正常研发流程和官方发布，不通过 AI 控制面。

## 脚本执行边界

- 外部 AI/用户提交的是 GDScript 草案；默认不执行。
- 执行前必须编译/静态检查，并显示权限、差异、检查点。
- 运行必须由用户确认，或在受控自动化会话中由显式 `script.run` 权限授权。
- 脚本拥有应用权限；同步死循环可能无法停止；不是不可信代码沙箱。
- 停止只清理本次脚本会话；文件、网络、应用单例等场景外副作用不承诺回滚。

## 非目标

- 不在应用内实现通用 coding agent。
- 不提供仓库、终端、测试和 CI 能力；这些不属于发布产品内的 AI 控制面。
- 不把模型供应商密钥分发给每个终端用户；企业使用统一网关。
- 不允许移动端执行脚本；移动端 AI 只读。
