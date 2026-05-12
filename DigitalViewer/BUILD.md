# DigitalViewer 目标构建与开发说明

本文档描述在本 Fork 中通过 **`target=digital_viewer`** 构建 **DigitalViewer 应用程序**（无官方编辑器 UI、运行时向 Godot 二进制）的完整方案，便于团队成员复现环境、理解目录职责与排查问题。

**分步实施记录（每一步改了什么、如何验收、附录变更清单）：** 请参阅 **[IMPLEMENTATION_STEPS.md](./IMPLEMENTATION_STEPS.md)**。

---

## 1. 概述

| 项目 | 说明 |
|------|------|
| **产品形态** | 使用 Godot 引擎核心（渲染、GUI、`SceneTree`、各 Server 等），**不编译** `editor/` 目录，行为接近导出模板类运行时。 |
| **SCons 目标名** | `digital_viewer`（在 `SConstruct` 的 `EnumVariable("target", ...)` 中注册）。 |
| **入口实现** | **`main/DigitalViewerMain.cpp`**：提供与 `main.h` 一致的 **`Main::`** 静态实现，替代 **`main/main.cpp`**（仅 `digital_viewer` 编译此文件；应用逻辑直接写在该 cpp 中，无需再包 `#ifdef`）。 |
| **产品代码** | **`DigitalViewer/`**：应用层 C++（自定义 `Node`/`Control`、业务、扩展注册函数等）。 |
| **`editor` 构建** | `env.editor_build = (env["target"] == "editor")`，因此 **`digital_viewer` 不会**定义 `TOOLS_ENABLED`，也 **不会** `SConscript("editor/SCsub")`。 |

---

## 2. 与 editor / 模板目标的对比

| 维度 | `editor` | `template_debug` / `template_release` | `digital_viewer` |
|------|----------|--------------------------------------|------------------|
| `TOOLS_ENABLED` | 是 | 否 | 否 |
| 编译 `editor/` | 是 | 否 | 否 |
| `main` 库主源文件 | `main.cpp` | `main.cpp` | **`DigitalViewerMain.cpp`** |
| 编译 `DigitalViewer/` | 否 | 否 | **是** |
| 默认 `DEBUG_ENABLED`（非 `dev_build` 时由 target 推导） | 是（`editor`） | 仅 `template_debug` | **是**（与 `template_debug` 一样纳入 `debug_features`） |

说明：`digital_viewer` 被放入 `env.debug_features` 列表，因而在**未**强制 `dev_build` 时，`optimize=auto` 会采用与调试向 target 一致的优化等级（`speed_trace`），并定义 `DEBUG_ENABLED`。

---

## 3. 环境前置条件

- 按 [官方文档：为 Windows 编译 Godot](https://docs.godotengine.org/en/latest/engine_details/development/compiling/compiling_for_windows.html) 准备 **Python 3**、**SCons**、**Visual Studio / MSVC**（或你选择的平台工具链）。
- 本仓库若未安装 **AccessKit**、**Direct3D 12 SDK**、**ANGLE** 等可选驱动依赖，需在命令行显式关闭对应选项（见下文示例），否则会像上游一样在配置阶段报错退出。

---

## 4. 命令行构建

### 4.1 Windows（MSVC）示例

在仓库根目录执行：

```bat
scons target=digital_viewer platform=windows accesskit=no d3d12=no angle=no
```

常用并行与开发向选项：

```bat
scons target=digital_viewer platform=windows accesskit=no d3d12=no angle=no -j12 dev_build=yes
```

指定 MSVC 工具集版本（与团队统一）：

```bat
scons target=digital_viewer platform=windows msvc_version=14.5 accesskit=no d3d12=no angle=no
```

### 4.2 产物位置与命名

- 可执行文件通常在 **`bin/`** 下，名称仍沿用上游规则，后缀包含 **`windows`**、**`digital_viewer`**、**`x86_64`**（或当前 `arch`）等段，例如：  
  `godot.windows.digital_viewer.x86_64.exe`  
  以及可能生成的 **console** 变体（取决于平台 `SCsub`）。
- 对象目录与静态库在 **`bin/obj/`** 下按子目录分流；应用库名为 **`digital_viewer_app`**（见 `DigitalViewer/SCsub`）。

### 4.3 与 `digital_viewer` 相关的 Windows 链接库

在 `platform/windows/detect.py` 中，**`digital_viewer`** 与 **`template_debug`** 一样会链接 **`psapi` / `dbghelp`**，便于调试与栈符号等场景。

### 4.4 无工程运行（普通 exe）

- 通用模板二进制会在当前目录查找 **`project.godot`** 或同名 **`.pck`**。
- 本 Fork 在 **`Main::setup`**（`DigitalViewerMain.cpp`）中，当**未**指定 **`--path`**、**`--main-pack`** 且工程路径为默认 **`"."`** 时，调用 **`ProjectSettings::setup_standalone_application(可执行文件目录)`**（见 `core/config/project_settings.cpp`）：
  - **不读取**磁盘上的 `project.godot`；
  - 除 **GLOBAL_DEF** 外，仅设置应用名；显示与拉伸见 **`DigitalViewer/project.godot`**（桌面壳默认 **`stretch/mode=disabled`**，字号不随窗口缩放；HiDPI 由 Main 对 **`content_scale_factor` × `screen_get_scale()`** 处理）。
  - 将 **`res://`** 映射到**可执行文件所在目录**（便于随 exe 携带资源；无文件亦可启动）。
- 开发调试可：`your.exe --path /path/to/repo/DigitalViewer`（使用仓库内 **`DigitalViewer/project.godot`**）。
- 若你需要其它 Godot 工程（脚本、`tscn` 等），仍可用：  
  `your.exe --path D:\MyGame`
- 界面壳在 **`DigitalViewerRoot`**（`digital_viewer_root.cpp`）中搭建，可按产品需求扩展。

---

## 5. Visual Studio 工程生成

上游通过 **`vsproj=yes`** 生成可在 VS 中选配置来调用 SCons 的解决方案。本 Fork 在以下文件的 **`get_configurations()`** 中增加了 **`digital_viewer`**，使配置下拉中可出现该目标：

- `platform/windows/msvs.py`
- `platform/linuxbsd/msvs.py`
- `platform/macos/msvs.py`

### 5.1 仅生成工程、不执行完整编译

```bat
scons target=digital_viewer platform=windows vsproj=yes msvc_version=14.5 dev_build=yes accesskit=no d3d12=no angle=no vsproj_gen_only=yes
```

### 5.2 生成后使用说明

- 打开生成的 **`godot.sln`**（或 `vsproj_name` 指定的名称），在工具栏将配置选为 **`digital_viewer | x64`**（或所需平台）。
- 每个「配置 | 平台」组合可能对应一个 **`godot.<platform>.<target>.<arch>.generated.props`**；若某组合尚未用对应参数生成过，`.props` 可能不存在——`vcxproj` 中的 `Import` 通常带 `Exists(...)` 条件，不会直接导致生成失败，但要获得正确的 IntelliSense 与构建命令，建议至少用 **目标 `target` + `arch`** 成功执行过一次 `vsproj=yes` 或在该配置下从 VS 触发构建。

若下拉仍无 **`digital_viewer`**，请确认已保存上述 `msvs.py` 修改并**重新运行**带 `vsproj=yes` 的 SCons。

---

## 6. 源码与构建系统布局

### 6.1 `SConstruct`（摘要）

- **`target`** 合法值包含 **`digital_viewer`**。
- **`env.editor_build`**：仅 `target == "editor"` 时为真。
- **`env.debug_features`**：`editor`、`template_debug`、**`digital_viewer`** 为真 → 定义 **`DEBUG_ENABLED`**（在 `dev_build` 未覆盖优化逻辑时与模板调试向一致）。
- 脚本顺序：**`main/SCsub`** 之后，仅当 **`target == "digital_viewer"`** 时 **`SConscript("DigitalViewer/SCsub")`**；**从不**在 `digital_viewer` 下引入 **`editor/SCsub`**。

### 6.2 `main/SCsub`

- 共通：`main_timer_sync.cpp`、`performance.cpp`、`steam_tracker.cpp`。
- **`target == "digital_viewer"`**：额外编译 **`DigitalViewerMain.cpp`**，**不**编译 **`main.cpp`**。
- 其它 target：编译 **`main.cpp`** 与上述三个文件（**不**编译 `DigitalViewerMain.cpp`，避免重复定义 `Main::`）。

### 6.3 `DigitalViewer/SCsub`

- 将本目录下 **`*.cpp`** 编译为静态库 **`digital_viewer_app`**。
- 使用 **`env.Append(LIBS=[lib])`** 把该库加入链接，放在 **`main`** 之后，以便解析 `main` 中对 `register_digital_viewer_types` 等符号的引用。

### 6.4 应用目录文件（可扩展）

| 文件 | 用途 |
|------|------|
| `register_digital_viewer_types.h` / `.cpp` | `register_digital_viewer_types()` / `unregister_digital_viewer_types()`，内用 `GDREGISTER_CLASS` 等注册自定义类。 |
| `digital_viewer_root.h` / `.cpp` | 示例根节点（继承 `Node`）；可在 `Main::start` 中作为默认场景挂载。 |

新增 `.cpp` 时放入 **`DigitalViewer/`** 即可被 **`SCsub`** 自动收录（若子目录增多，可再拆分子 `SConscript`）。

---

## 7. 运行时集成要点（`DigitalViewerMain.cpp`）

1. **类型注册**  
   在 **`register_platform_apis()` 之后**（`Main::setup2` 流程中）调用 **`register_digital_viewer_types()`**；在 **`test_cleanup` / `Main::cleanup`** 等关闭路径中调用 **`unregister_digital_viewer_types()`**。本文件仅由 `digital_viewer` 编译，**无需** `#ifdef`。

2. **默认场景**  
   在 **`Main::start`** 的「Load Game」中，当 **`game_path` 为空** 时 **`memnew(DigitalViewerRoot)`** 并 **`SceneTree::add_current_scene`**，避免无主场景时黑屏无根节点。

3. **与上游 `main/main.cpp` 的关系**  
   `DigitalViewerMain.cpp` 由 `main.cpp` 派生维护；合并上游时请以 **`main.cpp` 中非 `TOOLS_ENABLED` 运行时逻辑** 为准，将必要修改同步到 `DigitalViewerMain.cpp`，**尽量**不往 `main.cpp` 堆业务分支。文件头注释中有简要提示。

---

## 8. 模块、GDExtension 与 `TOOLS_ENABLED`

- **`digital_viewer`** 不定义 **`TOOLS_ENABLED`**：任何仅在 `#ifdef TOOLS_ENABLED` 下存在、或 **`config.py` 声明依赖 editor** 的模块，可能无法链接或需在本 Fork 中调整/关闭。
- 验证标准：在目标平台上 **`scons target=digital_viewer platform=<platform> ...`** **完整链接通过**；若失败，按链接错误检查模块依赖或裁剪 `module_*_enabled`。

---

## 9. 常见问题（排查）

| 现象 | 可能原因 | 处理方向 |
|------|----------|----------|
| 配置阶段提示 AccessKit / D3D12 / ANGLE 缺失 | 未安装对应第三方或脚本依赖 | 按文档安装，或 **`accesskit=no` `d3d12=no` `angle=no`** |
| 链接 undefined：`register_digital_viewer_types` | `DigitalViewer/SCsub` 未编入或未链接 | 确认 **`target=digital_viewer`** 且 **`DigitalViewer/`** 源文件在 `SCsub` 中 |
| 重复定义 `Main::` | 同时编译了 `main.cpp` 与 `DigitalViewerMain.cpp` | 检查 **`main/SCsub`** 分支是否仅对 `digital_viewer` 使用 `DigitalViewerMain.cpp` |
| VS 无 `digital_viewer` 配置 | `msvs.py` 未更新或未重新 `vsproj=yes` | 更新 **`platform/*/msvs.py`** 后重新生成解决方案 |

---

## 10. 可选后续定制（未默认实现）

- **可执行文件名**：可在各平台 **`platform/<platform>/SCsub`** 的 `add_program` 中改输出名（例如改为 `digital_viewer`）。
- **平台入口**：保持 **`Main::`** 时无需改 `godot_windows.cpp`；若改为独立命名空间入口，则需在多平台 `godot_*.cpp` 中加分支（维护成本更高）。

---

## 11. 文档与仓库

- 构建行为以仓库内 **`**/SCsub`、`SConstruct`、`platform/**/detect.py`** 及 **`DigitalViewerMain.cpp`** 为准；上游版本升级后请复查本文件所列位置是否仍然一致。

如需对外部协作者说明「这是基于 Godot 的定制运行时」，可一并引用官方编译文档与引擎版本（见仓库 `version.py`）。
