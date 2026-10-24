# 小马压缩 · Xiaoma Archive

Windows 10/11 x64 原生压缩与解压工具。提供 ZIP、7z、TAR 创建，以及由独立 7-Zip 内核支持的格式解压、列表、校验、密码和分卷处理。

## 构建

需要 Visual Studio C++ Build Tools、Windows SDK、Python 3 和 Pillow。
从 7-Zip 官方发布获取 Windows x64 的 `7z.exe`、`7z.dll` 及对应许可证，放入 `vendor/`；本仓库不包含第三方二进制。现有版本使用 7-Zip 26.04，其他版本需另行验证。

```powershell
python -m pip install Pillow
.\build.ps1 -OutputDirectory .\dist
# 也可使用已有、可信的官方 7-Zip 文件目录：
.\build.ps1 -OutputDirectory .\dist -EngineDirectory 'C:\path\to\7zip'
```

成品必须保留相邻 `engine/` 目录。普通启动显示管理界面；右键功能需自行运行成品目录内的 `安装右键菜单.ps1`，仅注册当前用户。移除菜单使用 `卸载右键菜单.ps1`。构建不会自动注册右键菜单。

## 代码与测试

- `src/core.cpp`：独立内核调用、压缩包操作、路径检查及临时结果管理。
- `src/quick.cpp`：后台快捷操作。
- `src/main.cpp`：Windows 原生界面。
- `src/tests.cpp`：已有核心回归测试；完整运行还需要独立生成的 fixtures，当前源码包不附带这些测试压缩包，因此不要把缺少 fixtures 的运行视为完整验收。

开源整理仅修改独立副本：构建支持显式指定 7-Zip 目录；修复卸载脚本在扩展名循环外引用 `$ext`，导致密码解压菜单未正确移除的问题。未改动本机已安装软件或菜单。

## 许可

本项目自制代码使用 MIT 许可。7-Zip 是独立的第三方程序，其 LGPL、BSD 与 unRAR 限制见 `vendor/7-Zip-LICENSE.txt`，不属于本项目 MIT 授权范围。官方项目和对应源码入口：

- https://www.7-zip.org/
- https://github.com/ip7z/7zip

应用图标为现有项目的生成图形资产；`tools/make_icon_v4.py` 导出各尺寸图标。
完整功能及边界见 `使用说明.txt`。首轮公开准备不代表所有系统和压缩格式均已实测。
