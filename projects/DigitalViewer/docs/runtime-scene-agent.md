# DigitalViewer 外部 AI 脚本桥

> 状态：v1.0 设计基线。v1 不内置 AI；外部 AI 通过本机 MCP 提交 GDScript 草案。**取代**原“运行时场景代理 / SceneProgram/1 JSON”方案；旧 JSON 场景语言与 `scene_run` 不再作为 AI 与场景交互的通道。

## 原则

外部 AI 只通过 **GDScript** 与运行中的 DigitalViewer 场景交互。模型不生成、不读取、不修改 JSON 场景数据；应用不维护第二套 JSON 场景模型，也不把场景序列化后交给模型。

```text
用户请求
  → 外部 AI 生成 GDScript 草案
  → MCP script_propose
  → 编译 / 静态检查
  → 脚本面板（待审查）
  → 用户确认信任
  → 手动运行
  → 输出 / 错误 / 调用栈
```

外部 AI 草案**不会自动运行**。`script_propose` 只提交草案，运行按钮仍由用户点击；v1 没有应用内 AI 聊天。

## 模型接口

| 接口 | 参数 | 用途 |
| --- | --- | --- |
| `script_propose` | `request`、可选 `viewport_id` | 返回 GDScript 草案、目标说明、脚本版本、待审查状态 |
| `script_inspect` | `path` / 空 | 读取脚本会话状态、编译结果、输出摘要；不返回场景 JSON |
| `script_open` | `draft_id` | 把草案放入脚本面板，不执行 |
| `get_script_state` | 无 | 返回脚本版本、运行状态、最近错误和调用 ID |

不存在 `scene_language`、`scene_inspect`、`scene_run` 或 `program_json`。AI 不通过工具调用直接写场景，只提交脚本草案。

## 脚本 API

脚本通过应用级 `viewer` 上下文访问运行时；API 使用注册表、类型/范围校验和错误定位。

```gdscript
extends Node

func _ready() -> void:
    # 显式选择视口；AI 脚本不隐式绑定活动视口
    for vp in viewer.viewports():
        vp.set_grid(false)
        vp.set_background("space")
```

可用对象与能力：

- 视口：枚举、背景、网格、经纬网、渲染模式、显示设置。
- 相机：位置、朝向、FOV、飞行、视点。
- 图层：可见性、透明度、顺序、重载、暂停。
- 场景对象：分组、球体、立方体、圆柱、灯光、材质、变换。
- 环境：背景、环境光、方向光。

脚本 API 不暴露文件、网络、任意反射、进程或应用单例；需要新增能力时先扩展注册表，再由外部 AI 使用。

## 草案进入脚本面板

v1 没有应用内 AI 聊天。外部 AI 通过 `script_propose` 提交草案，脚本面板显示：

- `外部 AI 草案` + `待审查`
- 请求摘要
- 代码预览
- 编译结果、权限、检查点
- `查看差异`

脚本面板提供应用级范围、信任确认、运行/停止、输出和错误；运行前必须由用户确认。

## 安全与边界

- 无 JSON 写通道；无隐藏的场景数据旁路。
- 外部 AI 草案不自动执行；用户审查和信任确认是强制步骤。
- 脚本是应用级会话，切换活动视口不停止、不重置；操作视口时显式指定。
- 脚本在主线程执行；同步死循环无法被停止按钮中断，风险条必须常驻。
- 脚本版本 + 调用 ID 去重；新草案或手动编辑后拒绝旧响应。
- 停止只清理本次脚本会话及其后代；不承诺撤销文件、网络、应用单例等场景外副作用。

## 迁移说明

原 `SceneProgram/1`、`scene_language`、`scene_run`、JSON 检查点和 `native_owned` 回退路径全部废弃。外部 AI 与用户脚本共用同一条 GDScript 运行时会话，不再存在“JSON 接管 / 原生接管”双状态。
