# DigitalViewer 方案 — 完整实施步骤记录

本文档按**实际落地顺序**记录在本 Godot Fork 中实施 `target=digital_viewer` 的每一步：要改的文件、具体改动、目的与验证方式。便于复盘、审计或在干净分支上复现相同方案。

> 关联文档：**[BUILD.md](./BUILD.md)**（面向日常构建与使用说明）。  
> 本文侧重**实施过程**与**变更清单**。

---

## 零、目标与约束（实施前确认）

在实施下列步骤前，应已达成共识：

| 项 | 定案 |
|----|------|
| 新构建目标名 | `digital_viewer` |
| 主编译单元 | `target=digital_viewer` 时仅编译 **`main/DigitalViewerMain.cpp`**（**不**编译 **`main/main.cpp`**）；应用代码直接写在该文件，**不再**使用全局 `DIGITAL_VIEWER_APP` 宏（已从 `SConstruct` 移除）。 |
| 编辑器 | **不**纳入：`target=digital_viewer` 时 `editor_build` 为假，不 `SConscript("editor/SCsub")`，一般不定义 `TOOLS_ENABLED` |
| 产品代码目录 | 仓库根下 **`DigitalViewer/`**，含独立 **`SCsub`** 与 C++ 源文件 |
| 平台入口 | 方式 A：仍由各平台 `godot_*.cpp` 调用 **`Main::setup` / `Main::start` / `Main::cleanup`**（无需改平台文件） |
| `debug_features` | `digital_viewer` 与 `template_debug` 一样视为“带用户向调试特性”的 target（`DEBUG_ENABLED`、`optimize=auto` 行为对齐） |

---

## 第一步：修改 `SConstruct` — 注册 target 与脚本包含

**文件：** [`SConstruct`](../SConstruct)

### 1.1 扩展 `target` 枚举

在 `opts.Add(EnumVariable("target", ...))` 的列表中加入 **`"digital_viewer"`**（与 `editor`、`template_release`、`template_debug` 并列）。

### 1.2 扩展 `debug_features`

将：

- `env.debug_features = env["target"] in ["editor", "template_debug"]`

改为包含 **`"digital_viewer"`**，例如：

- `env.debug_features = env["target"] in ["editor", "template_debug", "digital_viewer"]`

**目的：** 与 `template_debug` 一致启用 `DEBUG_ENABLED` 及相应的默认优化推断（`optimize=auto` → `speed_trace`，除非 `dev_build` 等覆盖）。

**注意：** 勿将 `digital_viewer` 并入 `editor_build`；`TOOLS_ENABLED` 仍仅随 `editor`。

### 1.3 条件引入 `DigitalViewer/SCsub`

在 `SConscript("main/SCsub")` **之后**、`SConscript("platform/" + env["platform"] + "/SCsub")` **之前**，增加：

```python
if env["target"] == "digital_viewer":
    SConscript("DigitalViewer/SCsub")
```

**目的：** 仅构建 `digital_viewer` 时编译应用静态库；**不要**在此 target 下引入 `editor/SCsub`（`editor` 仍由既有 `if env.editor_build:` 控制）。

### 1.4 验证（本步后）

```bash
scons --help
# 或通过 dry-run 确认 target 被接受；完整链接见文末「验收构建」
```

---

## 第二步：修改 `main/SCsub` — 切换 `main` 库源文件

**文件：** [`main/SCsub`](../main/SCsub)

### 2.1 列出 `main` 库固定源码

不要对 `digital_viewer` 使用通配符 `*.cpp`（否则会把 **`DigitalViewerMain.cpp`** 与 **`main.cpp`** 同时编进非 `digital_viewer` target，或混用错误）。

改为显式列举与上游 `main` 目录一致的公共文件，例如（以当前仓库为准）：

- `main_timer_sync.cpp`
- `performance.cpp`
- `steam_tracker.cpp`

### 2.2 按 target 二选一主入口

```python
if env["target"] == "digital_viewer":
    env_main.add_source_files(env.main_sources, "DigitalViewerMain.cpp")
else:
    env_main.add_source_files(env.main_sources, "main.cpp")
```

**目的：** 保证全仓库**仅一处**定义 `Main::` 成员函数。

---

## 第三步：新增 `main/DigitalViewerMain.cpp` — `Main::` 实现

**文件：** [`main/DigitalViewerMain.cpp`](../main/DigitalViewerMain.cpp)（新建）

### 3.1 基线内容

以 **`main/main.cpp`** 为基线复制整文件（或通过脚本 `Copy-Item` / `cp`），作为 `digital_viewer` 专用实现。后续合并上游时，以 **`main/main.cpp` 中非 `TOOLS_ENABLED` 运行时变更** 为主进行 diff 合并。

### 3.2 文件头标识

在许可证头上方或紧邻处注明：本文件为 **`target=digital_viewer` 的 `Main::` 实现**，并提示定期与 **`main/main.cpp`** 同步非编辑器逻辑。

### 3.3 头文件

在 `#include "platform/register_platform_apis.h"` 之后直接包含应用头文件（仅此 target 编译本文件，无需 `#ifdef`）：

```cpp
#include "DigitalViewer/digital_viewer_root.h"
#include "DigitalViewer/register_digital_viewer_types.h"
```

### 3.4 `Main::setup2` — 注册类型

在 **`register_platform_apis();`** 之后、**`benchmark_end_measure("Startup", "Platforms")`** 之前调用：

```cpp
	register_digital_viewer_types();
```

**目的：** 平台 API 已注册后再注册应用类型，避免依赖顺序问题。

### 3.5 `Main::start` — 无主场景时挂载应用根节点

在「Load Game」逻辑中，`if (!game_path.is_empty()) { ... }` 的 **`else`** 分支挂载默认根节点：

```cpp
			else {
				DigitalViewerRoot *dv_root = memnew(DigitalViewerRoot);
				sml->add_current_scene(dv_root);
			}
```

**目的：** 命令行未指定主场景时仍有默认根节点，避免空白树。

### 3.6 关闭路径 — `unregister_digital_viewer_types()`

在与 **`unregister_platform_apis()`** 配套的清理流程中（含 `test_cleanup` 等），在 **`unregister_platform_apis()` 之前** 调用：

```cpp
	unregister_digital_viewer_types();
```

**说明：** 若存在**两处**长/短清理路径，两处均需对称添加。

### 3.7 禁止事项

- 勿在本文件引入 **`editor/...`** 或未保护在 `#ifdef TOOLS_ENABLED` 下的编辑器符号。
- 勿在 `digital_viewer` 构建中同时保留 **`main.cpp`** 进入 `main` 库（见第二步）。

---

## 第四步：新增目录 `DigitalViewer/` — 构建与应用源码

### 4.1 `DigitalViewer/SCsub`

**文件：** [`DigitalViewer/SCsub`](./SCsub)

- `Import("env")`
- `env_digital = env.Clone()`
- `env_digital.add_source_files(env_digital_sources, "*.cpp")`
- `lib = env_digital.add_library("digital_viewer_app", env_digital_sources)`
- **`env.Append(LIBS=[lib])`**（放在链接列表**末尾**，以便解析 `main` 中对 `register_digital_viewer_types` 的未定义引用）

### 4.2 类型注册

**文件：**

- [`register_digital_viewer_types.h`](./register_digital_viewer_types.h) — 声明 `register_digital_viewer_types()`、`unregister_digital_viewer_types()`
- [`register_digital_viewer_types.cpp`](./register_digital_viewer_types.cpp) — 实现中 `GDREGISTER_CLASS(DigitalViewerRoot)`（及后续你新增的类）

### 4.3 最小根节点

**文件：**

- [`digital_viewer_root.h`](./digital_viewer_root.h) — `class DigitalViewerRoot : public Node` + `GDCLASS`
- [`digital_viewer_root.cpp`](./digital_viewer_root.cpp) — `_bind_methods()` 与构造函数

### 4.4 后续扩展

新增业务 `.cpp` 放入 `DigitalViewer/` 根目录即可被 `*.cpp` 收录；若子目录增多，可拆分子目录 **`SConscript`**（本记录不展开）。

---

## 第五步：修改 `platform/windows/detect.py` — 链接库与模板对齐

**文件：** [`platform/windows/detect.py`](../platform/windows/detect.py）

在 MSVC 配置中，将：

```python
if env["target"] in ["editor", "template_debug"]:
    LIBS += ["psapi", "dbghelp"]
```

扩展为包含 **`"digital_viewer"`**：

```python
if env["target"] in ["editor", "template_debug", "digital_viewer"]:
    LIBS += ["psapi", "dbghelp"]
```

**目的：** `digital_viewer` 与 `template_debug` 同属面向调试/诊断链路的 Windows 构建习惯。

---

## 第六步：修改各平台 `msvs.py` — Visual Studio 配置名下拉

VS 解决方案中的「配置」名来自各平台目录下的 **`get_configurations()`**。

**文件：**

- [`platform/windows/msvs.py`](../platform/windows/msvs.py)
- [`platform/linuxbsd/msvs.py`](../platform/linuxbsd/msvs.py)
- [`platform/macos/msvs.py`](../platform/macos/msvs.py)

将 `get_configurations()` 返回值由：

```python
return ["editor", "template_debug", "template_release"]
```

改为：

```python
return ["editor", "template_debug", "template_release", "digital_viewer"]
```

**目的：** `scons vsproj=yes target=digital_viewer ...` 生成工程后，VS 中可选 **`digital_viewer | x64`** 等组合。

---

## 第七步（可选）：`methods.py` 注释

**文件：** [`methods.py`](../methods.py)

在 `generate_vs_project` 附近，将注释中的配置示例从「editor/template_debug/template_release」更新为包含 **`digital_viewer`**，避免后续维护者误解。

---

## 第八步：撰写与维护文档

**文件：**

- [`DigitalViewer/BUILD.md`](./BUILD.md) — 构建命令、VS、架构说明（面向使用者）
- **本文** [`DigitalViewer/IMPLEMENTATION_STEPS.md`](./IMPLEMENTATION_STEPS.md) — 实施步骤全记录（面向复现与审计）

---

## 第九步：验收构建与检查清单

### 9.1 命令行完整链接（Windows 示例）

```bash
scons target=digital_viewer platform=windows accesskit=no d3d12=no angle=no -j8
```

（按本机是否安装 AccessKit / D3D12 / ANGLE 决定是否省略这些 `=no`。）

### 9.2 预期产物

- `bin/` 下出现带 **`digital_viewer`** 与当前 **`arch`** 段的可执行文件（名称规则与上游 `PROGSUFFIX` 一致）。
- `bin/obj/DigitalViewer/` 下存在 **`digital_viewer_app`** 相关 `.lib` / `.obj`。

### 9.3 检查清单

- [ ] `scons target=editor` 仍能使用 **`main.cpp`** 而非 `DigitalViewerMain.cpp`
- [ ] `digital_viewer` 构建**不包含** `editor/` 编译单元（体积/链接符号侧面印证）
- [ ] 未出现 **`Main::` 重复定义** 链接错误
- [ ] 试运行无主场景参数时进程能启动并挂有 `DigitalViewerRoot`（可按调试器或日志确认）

### 9.4 VS 工程（若使用）

```bash
scons target=digital_viewer platform=windows vsproj=yes vsproj_gen_only=yes accesskit=no d3d12=no angle=no
```

打开 `godot.sln`，确认配置中有 **`digital_viewer`**，并选择对应 **`godot.<platform>.digital_viewer.<arch>.generated.props`** 已生成。

---

## 第十步：明确未纳入本方案的「可选项」（避免误以为漏做）

以下项在方案中**可**做但**当前记录不要求**为必需：

| 可选项 | 说明 |
|--------|------|
| 平台改调 `DigitalViewerMain::` | 不通过 `Main::` 而改每个 `godot_*.cpp`；维护成本高 |
| 可执行文件改名 | 在各平台 `platform/*/SCsub` 的 `add_program` 中修改输出名 |
| 上游流程自动化 | CI 中对 `main/main.cpp` 与 `DigitalViewerMain.cpp` 做定期 diff 或合并策略文档化 |

---

## 附录 A：变更文件一览（实施完成后应对齐）

| 路径 | 变更性质 |
|------|----------|
| `SConstruct` | 修改 |
| `main/SCsub` | 修改 |
| `main/DigitalViewerMain.cpp` | **新增**（大块） |
| `DigitalViewer/SCsub` | **新增** |
| `DigitalViewer/register_digital_viewer_types.{h,cpp}` | **新增** |
| `DigitalViewer/digital_viewer_root.{h,cpp}` | **新增** |
| `platform/windows/detect.py` | 修改 |
| `platform/windows/msvs.py` | 修改 |
| `platform/linuxbsd/msvs.py` | 修改 |
| `platform/macos/msvs.py` | 修改 |
| `methods.py` | 修改（注释，可选） |
| `DigitalViewer/BUILD.md` | **新增** |
| `DigitalViewer/IMPLEMENTATION_STEPS.md` | **新增**（本文） |

**刻意不修改（推荐）：**

- `main/main.cpp` — 尽量保持与上游一致，仅作对照；运行时定制放在 `DigitalViewerMain.cpp` 与 `DigitalViewer/`。

---

## 附录 B：回滚顺序（若需撤销方案）

建议按相反顺序删除/还原：`DigitalViewer/` 与 `DigitalViewerMain.cpp` → 还原 `main/SCsub` → 还原 `SConstruct` 中 `digital_viewer` 相关段落 → 还原 `detect.py` / `msvs.py` / `methods.py` 中与 DigitalViewer 相关的改动。回滚后应用 `scons -c` 或清理 `bin/obj` 再全量构建，避免陈旧对象文件干扰。

---

*文档版本与引擎 `version.py` 及本 Fork 的 `DigitalViewer` 提交保持同步为佳。*
