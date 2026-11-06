# 按产品配置构建 DigitalViewer

`profiles/ALL.json` 和 `profiles/CAD.json` 统一管理产品名称、能力、主题、配色、图标和原生模板后缀。默认配置是 ALL。

| 配置 | OSG / OSGB | 3D Tiles / Cesium ion | GIS 椭球、经纬网、坐标飞往 | 外观 |
| --- | --- | --- | --- | --- |
| ALL | 开启 | 开启 | 开启 | 原有青绿色主题、地球图标 |
| CAD | 关闭 | 关闭 | 关闭 | 蓝灰色 Blueprint 主题、工程立方体图标 |

两种配置都保留现有 glTF/GLB、本地和远程模型、ZIP、分屏、拾取、属性编辑、GDScript 工作台和本机自动化。CAD 是现有模型查看能力的产品配置，不会新增 STEP、IGES、DWG 等尚未实现的导入器。ALL 仍受现有 GIS 实现范围限制，不代表未实现的功能已经完成。

## Windows 打包

在仓库根目录使用 PowerShell 7；需要定制的 Godot 4.7.3-rc 编辑器以及配置好 Visual Studio 工具链的 SCons。

```powershell
# 首次编译 CAD 专用模板并导出程序。
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Profile CAD -BuildTemplate

# 后续资源更新可复用模板。
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Profile CAD
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Profile ALL

# 单独构建模板；可通过 -Jobs 限制编译并发。
.\projects\DigitalViewer\tools\Build-Template.ps1 -Profile CAD -Jobs 8
.\projects\DigitalViewer\tools\Build-Template.ps1 -Profile ALL -Jobs 8
```

模板分别输出为 `bin/godot.windows.template_release.x86_64.dv_cad.exe` 和 `bin/godot.windows.template_release.x86_64.dv_all.exe`。ALL 也兼容现有的无后缀定制模板，但导出后必须通过原生模块检查。可用 `-Template <绝对路径>` 指定模板。构建脚本从 JSON 能力生成 SCons 模块开关：CAD 不编译 `osgdb`、`3dtiles`、`tiled_mesh_3d`；保留 glTF 使用的 Draco。ANGLE、AccessKit 和 D3D12 构建依赖默认关闭，保留 OpenGL 和 Vulkan 渲染。

成品目录分别为 `build/desktop/CAD/`、`build/desktop/ALL/`；ZIP 为 `build/DigitalViewer-CAD-1.0.0-windows-x86_64-preview.zip` 和 `build/DigitalViewer-ALL-1.0.0-windows-x86_64-preview.zip`。包内 `build-profile.json` 是生效配置，`build-manifest.json` 记录配置、功能开关、模板和制品 SHA256。CAD 包不携带独立的 OSG 许可证文件；通用第三方许可证汇总仍随包保留。

每次导出在 `build/staging/<配置>-<随机 ID>/` 创建独立工程，写入锁定的配置、应用名称、图标、启动图、主题和导出选项，不修改源工程的配置或导出预设。窗口图标、标题栏品牌、EXE 图标与启动图都由所选图标资源派生。构建结束后可按需清理 `build/staging/`。

只输出资源包可使用 `-PackOnly`，此时不会验证或包含原生模板，运行时仍需配套模板。Linux/macOS 可生成配置对应的资源包；完整 CAD 导出要求显式提供关闭对应模块的平台模板。本脚本目前仅在 Windows 上构建并探测原生模板。

## 开发预览与验证

```powershell
# 源工程默认 ALL；仅编辑器运行允许命令行预览 CAD。
.\bin\godot.windows.editor.dev.x86_64.console.exe --path projects\DigitalViewer -- --dv-profile=CAD

# 分别构建包含配置回归测试的验证程序。
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Profile CAD -Preset 'Windows Validation'
.\projects\DigitalViewer\tools\Build-Desktop.ps1 -Profile ALL -Preset 'Windows Validation'
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Profile CAD -ProfileOnly -Pack -Engine .\projects\DigitalViewer\build\validation\CAD\DigitalViewer.console.exe
.\projects\DigitalViewer\tools\Test-Desktop.ps1 -Profile ALL -ProfileOnly -Pack -Engine .\projects\DigitalViewer\build\validation\ALL\DigitalViewer.console.exe
```

配置测试覆盖主题与图标、可用入口、文件过滤、原生模块、禁用格式拒绝、显式导入类型和 ZIP 绕过、正常 glTF 加载、多视口共享及 GDScript 执行。ALL 可继续运行不带 `-ProfileOnly` 的完整桌面回归。CAD 的测试入口使用 `-ProfileOnly`。导出程序锁定打包配置，传入 `--dv-profile=ALL` 不能将 CAD 包切换为 ALL。

应用层的能力限制同时作用于文件拖入、脚本/API 调用、ZIP 候选、重载和下载之前的数据加载；禁用格式会给出当前配置的错误。原生模板检查会拒绝把包含 OSG / 3D Tiles 的全量引擎当作 CAD 模板。产品配置不构成 GDScript 安全沙箱。

## 自定义外观

修改对应 JSON 的 `icon`、`theme`、`theme_name` 和 `palette`。主题资源负责输入框、弹窗、滚动条等默认控件；`palette` 负责动态生成的桌面控件。当前提供 ALL 青绿与 CAD 蓝灰两套深色主题。能力开关变化后重新编译相应模板。

## 本次验证（2026-11-03）

- CAD 专用 Windows release 模板完成编译；最终 CAD、ALL 的 EXE/PCK 和 ZIP 均已导出并通过启动检查。
- 导出验证包的配置回归：CAD 图形模式 33 项通过，ALL 无头模式 24 项通过，覆盖原生模块、模型/ZIP、跨视口共享及 GDScript 执行；CAD 图形截图确认蓝灰主题和立方体品牌图标。
- Windows EXE 图标资源不同；两种 ZIP 内的 EXE/PCK SHA256 与各自清单一致。CAD ZIP 不包含独立 OSG 许可证文件。
- CAD 收到 `--dv-profile=ALL` 后仍保持 CAD；CAD 的 PCK 与 ALL 引擎错误配对时会被拒绝。
- ALL 完整桌面回归运行 211 项，其中 5 项业务断言失败：树刷新保留折叠/选中、停止脚本后选中回退、OSG 三角形拾取、MCP 无效会话拒绝、MCP 重启会话失效。使用同一发布模板并还原本次修改的 6 个桌面脚本后，对照包出现相同失败；没有新增失败。完整回归尚未全绿。