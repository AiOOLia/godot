# 在 DeepSeek Harness 安装 DigitalViewer

当前 Harness 使用原生 MCP 客户端插件 `@deepseek-ai/dsh-mcp-client` 接入 DigitalViewer；这是 Harness 侧支持的插件方式，不需要修改 DigitalViewer 产品。

## 前置条件

- DigitalViewer 桌面版正在运行。
- 设置中启用 MCP（默认端口 `8731`）。
- 从 DigitalViewer 设置复制 Bearer token，或读取：

```text
%APPDATA%\Godot\app_userdata\DigitalViewer\mcp\bearer_token
```

## 用插件方式安装

插件包位于：

```text
E:\WorkSpace\GitHub\godot\projects\DigitalViewer\build\dsh-plugin-digitalviewer
```

在 Harness 中：

```text
插件 → 添加插件 → 安装第三方插件 → 选择本地目录
→ E:\WorkSpace\GitHub\godot\projects\DigitalViewer\build\dsh-plugin-digitalviewer
→ 安装
```

该包是 bundle 插件，自身没有运行代码；`cordis.patch.yml` 注册 `@deepseek-ai/dsh-mcp-client`。

当前 Desktop profile 已安装：

```text
node_modules/dsh-plugin-digitalviewer
```

并已加入：

```json
"bundles": [
  "...",
  "dsh-plugin-digitalviewer"
]
```

## 手动配置（备用）

如果不使用插件管理器，也可以编辑：

```text
%USERPROFILE%\.dsh\profiles\desktop\cordis.patch.yml
```

追加：

```yaml
- insert:
    - id: mcp-digitalviewer
      name: '@deepseek-ai/dsh-mcp-client'
      config:
        serverName: digitalviewer
        transport: streamable-http
        url: http://127.0.0.1:8731/mcp
        headers:
          Authorization: 'Bearer <token>'
        failOnStartupError: false
        toolCallTimeoutMs: 60000
```

不要同时使用插件包和手动 patch，避免同一个 `mcp-digitalviewer` 注册两次。

## 工具

安装后 Harness 工具列表中会出现：

```text
mcp__digitalviewer__scene.describe
mcp__digitalviewer__viewport.list
mcp__digitalviewer__script.inspect
mcp__digitalviewer__script.propose
mcp__digitalviewer__script.run
mcp__digitalviewer__capture.screenshot
mcp__digitalviewer__audit.list
```

实际名称可能带 Harness 的稳定 hash 后缀。

## 安全边界

- `script.run` 默认权限关闭；调用返回 `Permission denied`。
- `capture` 默认权限关闭。
- 即使打开 `script.run`，工具仍只返回 `manual_run_required` 并打开脚本面板，不能代替用户确认信任。
- MCP 不提供 shell、文件系统、网络或 JSON 场景写接口。
- token 存在本地 `cordis.patch.yml`，不要提交到仓库或分享。

## 自定义 `/bridge`

`plugin_bridge.gd` 提供的 `/bridge` 用于不支持 MCP 的外部插件/客户端。当前 Harness 使用原生 MCP，因此没有使用 `/bridge`；两种传输共用同一套工具和权限。

## 验证

当前 Harness 已实际调用：

- `scene.describe`：返回 `view_1`
- `viewport.list`：返回 `view_1`
- `script.inspect`：返回脚本列表
- `script.propose`：`review_required`
- `script.run`：`Permission denied`（权限关闭）
- `capture.screenshot`：`Permission denied`（权限关闭）
- `audit.list`：包含以上调用记录
