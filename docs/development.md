# LiteTodo 构建、数据与验证说明

## 数据、备份与升级

Windows 默认 `%LOCALAPPDATA%\LiteTodoPrototype`；macOS 默认 `~/Library/Application Support/LiteTodoPrototype`；Linux 默认 `$XDG_DATA_HOME/LiteTodoPrototype`，未设置时用 `~/.local/share/LiteTodoPrototype`。`--data-dir PATH` 指定独立目录，支持中文路径。

项目已命名为 LiteTodo，历史数据目录名保持兼容，现有任务可继续使用。

- v1/v2/v3/v4 升级前生成 `tasks.sqlite3.pre-v5-时间戳.sqlite3`，再升级到 v5，新增搁置字段。原任务、子任务、附件、状态和提醒保留；交付日期不从提醒推断。
- 开启自动备份后，程序运行期间每天一份 `backups/auto-YYYY-MM-DD.sqlite3`，仅保留最近 7 份该命名的自动快照；手工与导入前备份另外保留。
- 「立即备份」创建独立快照；导出输入完整文件路径，同名文件拒绝覆盖。导入在原文件的副本上校验结构及关联，确认后先备份当前库再替换，失败保留原数据。
- 数据目录更改会复制数据库、记住启动位置、切换连接，原目录保留。新目录不能已有任务库或位置指向文件。启动位置记在原目录 `.litetodo-location`；迁移后不要删除这个文件。
- 附件目录更改只影响新附件，已有附件保留原路径。数据库备份仅包含附件引用，换机时还要复制附件文件；删除任务不清理附件实体。

## 5 MB 口径

每个平台、每种架构，**解压后的程序及全部随包文件合计不超过 5,000,000 字节**，包含说明、许可、脚本和 `SIZE.json`。用户生成的数据、验证截图、源码和编译缓存不计入程序发布目录。

`scripts/package.py` 从新建空目录打包，按最终总大小执行硬门槛；`SIZE.json` 提供随包文件清单、大小和 SHA256。重新打包将旧目录改名为 `dist/<平台>-previous-<标识>` 留存，旧便携数据不会收录进新 ZIP。5 MB 为磁盘体积，不是内存上限。

## 项目结构

```text
src/       SQLite 核心、日期规则、窗口导航、控件、矢量图标、平台适配
assets/    原创 SVG、PNG、多尺寸 ICO、ICNS 及 Linux 图标像素
tests/     真实文件存储、升级、父子关系和打包隔离检查
scripts/   依赖准备、构建、图标生成、窗口验证及发布打包
docs/      交互对照、改版设计、Windows 验证和许可
vendor/    固定版本依赖，下载后校验 SHA256
build-*/   编译缓存，不进入发布包
dist/      便携程序目录及 ZIP
```

UI、数据存储和系统托盘/通知适配分层；本软件不连接嵌入式硬件，不包含串口或设备通信协议。

## 从源码构建

需要 CMake 3.21+、C++17 编译器、Ninja、Python 3.12+。Python 仅用于依赖准备和打包；最终用户无需这些工具。先执行 `python scripts/fetch-deps.py`。图标资源已经生成并随源码提供，重新生成可执行 `python scripts/render-icon.py`，该步骤额外需要 Pillow。

```sh
git clone https://github.com/WSxuqinrui/LiteTodo.git
cd LiteTodo
```

Windows（PowerShell）：

```powershell
./scripts/build-windows.ps1 -CMake '你的 cmake.exe 路径' -Ninja '你的 ninja.exe 路径' -Toolchain 'MinGW-w64 的 bin 目录'
```

macOS（Xcode Command Line Tools；分别编译 arm64/x86_64）：

```sh
cmake -S . -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos --target LiteTodo store_tests
ctest --test-dir build-macos --output-on-failure
python scripts/package.py --binary build-macos/LiteTodo.app --platform macos-arm64
```

macOS 通知需运行 `.app`；正式分发还需签名及公证。GitHub Actions 已通过 macOS Intel / Apple Silicon 的构建、测试与打包；尚未实机验收或签名、公证发布。

Linux（X11；Wayland 会话需 XWayland）：

```sh
sudo apt install build-essential cmake ninja-build python3 libx11-dev libxext-dev libxinerama-dev libxfixes-dev libxft-dev libxcursor-dev libxrender-dev libpango1.0-dev libdbus-1-dev fonts-noto-cjk
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-linux --target LiteTodo store_tests
ctest --test-dir build-linux --output-on-failure
python scripts/package.py --binary build-linux/LiteTodo --platform linux-x64
```

Linux 使用系统图形及通知库，需在目标发行版执行 `ldd` 与运行检查；XEmbed 托盘和置顶能力由桌面环境决定。GitHub Actions 已通过 Linux 构建、测试与打包；尚未实机验收。

## 检查方法

- CTest：Windows 65 项存储/路径检查，含 v5 搁置升级与持久化、暂停/恢复提醒、超过20项到期任务的排队排除、升级前备份、导入结构校验、批量整理、附件复制、目录迁移、自动备份保留、子任务关联与状态约束、提醒改期确认保护、中文/特殊字符搜索及一致备份。非 Windows 不执行专用的系统命令行解析检查。
- `python tests/package_tests.py`：重复打包时排除旧便携数据并保留旧发布目录。
- `LiteTodo --ui-smoke --data-dir 独立空目录`：用虚构任务检查单窗口导航、卡片更多菜单全部动作及删除取消、直接开始、自由浮球悬停和草稿保留、表情优先级、原生圆形窗口区域、应用内提醒确认/延后及草稿保存不撤销延后、旧通知身份回执隔离；保留五种尺寸、子任务、附件、导入、合成 Ctrl+Enter 和正常退出检查。Windows 本机通知总开关关闭，实际测试路径使用应用内兜底；未把合成回执当成系统通知目视验收。
- `scripts/verify-windows.py` 保存上述结果、实际控件渲染 PNG 和本次资源采样；控件离屏渲染不等于操作系统截图或真实输入验收。
- 人工复核仍包括真实鼠标、中文输入法、混合 DPI 多屏、浮球拖动、睡眠唤醒和通知展示。Windows 结果不能替代其他系统验收。

## 第三方与参考

本程序基于 FLTK 项目的部分工作（https://www.fltk.org）。完整许可见 `第三方声明.md` / `THIRD_PARTY_NOTICES.md` 与 `licenses/`。

交互参考 Pometodo 公开源码及用户提供的界面截图。代码及清单图标独立编写，未使用 Pometodo 的商标、源码或图片资源。对应关系见 `docs/interaction-reference.md`。
