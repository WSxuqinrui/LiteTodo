# 第三方组件说明

- **FLTK 1.4.5**：https://www.fltk.org 。本程序基于 FLTK 项目的部分工作。使用其未修改的核心静态库。LGPL v2 附静态链接例外；完整原文随包保留。
- **SQLite 3.54.0**：https://www.sqlite.org 。使用官方 amalgamation；SQLite 核心为公共领域，https://www.sqlite.org/copyright.html 。
- **GCC / libstdc++ / libgcc**：Windows 本地验证使用 MinGW-w64 GCC 13.1.0，运行库静态链接。GPL v3 与 GCC Runtime Library Exception 3.1 原文随包保留。
- **MinGW-w64 / winpthreads**：静态运行库包含其相关组件，许可原文见随包文件。
- Windows 系统库、macOS 系统框架以及 Linux 系统图形/通知库由目标系统提供，按各自许可使用。

未链接 FLTK 的 PNG/JPEG/SVG/OpenGL 附加库。构建依赖下载由 `scripts/fetch-deps.py` 的固定版本和 SHA256 校验约束。
Windows 的许可清单不能自动替代 macOS/Linux 构建后对实际链接组件的复核。
