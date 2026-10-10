# LiteTodo · 轻待办

当前版本：**0.4.1**。轻量、离线、免账户的桌面待办软件。

![LiteTodo 图标](assets/app.png)

源码仓库：[WSxuqinrui/LiteTodo](https://github.com/WSxuqinrui/LiteTodo)。Windows 便携版见 [Releases](https://github.com/WSxuqinrui/LiteTodo/releases)。

C++17 + FLTK + SQLite 的轻量桌面原型。Windows 已本机编译、检查和打包；macOS/Linux 有共享源码及构建配置，尚未实机验收。每个平台发布包以解压后总计 5,000,000 字节为上限。

## 双击使用

Windows 解压 ZIP，双击 `LiteTodo.exe`；免安装、免账户，启动不联网。`便携数据模式.cmd` 使用程序旁的 `data` 文件夹。

![主页面子任务清单](docs/screenshots/main.png)

- 左上角图标切换快捷面板与控制台。控制台展示状态看板/列表、来源、分类、备注及子任务进度，窄窗口自动改为单列。
- 新增、编辑、月历、提醒、设置、整理预览和删除/导入确认都在主窗口内跳转。上方「设置」按钮选中时填色，「置顶」变为「已置顶」，再次点击取消。
- 任务标题加大加粗，长中文标题最多三行。卡片右下常显开始/暂停与更多按钮；更多菜单包含完成/恢复、加急/取消、搁置/恢复待办、删除。点击正文进入编辑。删除在主窗口确认，取消保留任务；没有任务时显示新增提示。
- 「搁置」单独筛选，暂停提醒并移出今日/进行中。开始或完成会结束搁置；子任务与附件保留。快捷面板窄窗口用两行状态标签，菜单会自动避开窗口边缘。
- 交付时间提供今天、明天、后天、+3天、+7天和内嵌月历；提醒时间独立设置。「今日」包含已逾期的未完成任务。
- 点击主页面卡片中的「子任务」进度展开清单，勾选框或子任务名称均可直接完成/恢复，立即保存并更新进度；快捷面板、控制台看板和列表均可使用。所有子项勾完后仍需主动完成父任务；恢复已完成任务的子项会同步恢复父任务。
- 子任务支持添加、改名、删除和完成，最多 200 条一级子项。编辑页保存父任务时统一提交，取消不修改数据库；未完成子项会阻止父任务完成。存在未保存草稿时，主页面状态操作先返回草稿，防止覆盖。
- `Ctrl+N` 新增，`Ctrl+F` 搜索，`Ctrl+Enter` 保存任务，`Esc` 返回/取消编辑。设置之间切换保留任务草稿。
- 多行输入使用独立内边距，光标及文字保持一致；编辑与设置页使用细圆角滚动条，支持滚轮、拖动滑块及点击轨道。设置包含通用、外观、浮球、提醒、数据与备份、本地整理。
- 外观支持跟随系统/浅色/深色、五种配色、45%—100% 透明度和紧凑卡片。Linux 跟随系统目前仅读取 `GTK_THEME`；透明度取决于桌面合成器。
- Windows 自启动只在主动开启开关时注册实际数据目录；迁移数据后尝试更新注册。移动程序后应重新设置。macOS/Linux 此项禁用。
- 浮球默认使用「小团子」：没有任务时开心，8 项及以上时疲惫，有加急或逾期任务时冒火，进行中时认真。搁置/完成任务不计入表情。仍可切换数量样式、配色和线框；图标、开关及配色按钮使用平滑绘制。
- 圆形浮球在首次显示、隐藏恢复、主窗口关闭恢复及应用内缩放后重新应用遮罩，裁掉四角。自由浮球和贴边浮球都支持停留约 280 毫秒展开主窗口，快速掠过不触发；已经显示的编辑页保持原状。支持双击开合、位置重置、本地提示、右键新增和拖动记住位置。
- 拖文件到浮球，或在编辑页输入完整文件路径，生成附件草稿；保存时复制文件，取消不复制、不删除原件。每任务最多 40 个附件，单文件不超过 50 MB；点击附件显示路径，叉号只移除草稿引用。移除引用/删除任务不自动删除附件实体。
- 本地整理：设置 → 本地整理 → 快速整理。粘贴文字，按行识别任务及今天/明天/后天/加急，先查看预览再保存。修改文字后需重新预览；规则保留原文，不进行语义推理、OCR 或云调用。
- Windows 可录制 Ctrl/Alt 与字母/数字组合的全局快捷键；默认关闭。先复制文字，再按组合进入整理页，文字仍需点击预览并确认。冲突时保留原有效组合。其他平台暂禁用全局快捷键。
- 设置 → 提醒 → 测试通知：Windows 检查通知总开关和当前通知状态，等待实际显示回执；未显示时最多 10 秒后改用应用内提醒。总开关关闭时立即兜底，并提供「打开系统通知设置」。本应用不会修改系统通知开关。
- 应用内提醒可点「知道了」或稍后 5 分钟；没有显示回执或用户确认时不标记已发送。有草稿或正在确认删除时保留当前页，顶部蓝色提醒按钮可查看。macOS/Linux 目前只能确认通知服务接收，未实机验收实际显示。
- 关闭按钮可隐藏到托盘；设置 → 通用 → 退出会彻底结束程序，有草稿时页内确认。退出后不再提醒，重开处理未完成且尚未确认的过期提醒。

「贴在桌面」暂未开放，需要完善 Explorer 重启后的窗口恢复。云端接口、微信绑定、重复任务、回收站、剪贴板图片与 OCR 暂未实现；不展示虚假的账号绑定状态。查询/计数最多 2000 条任务。

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

macOS 通知需运行 `.app`；正式分发还需签名及公证。当前未执行该平台构建或发布。

Linux（X11；Wayland 会话需 XWayland）：

```sh
sudo apt install build-essential cmake ninja-build python3 libx11-dev libxext-dev libxinerama-dev libxfixes-dev libxft-dev libxcursor-dev libxrender-dev libpango1.0-dev libdbus-1-dev fonts-noto-cjk
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-linux --target LiteTodo store_tests
ctest --test-dir build-linux --output-on-failure
python scripts/package.py --binary build-linux/LiteTodo --platform linux-x64
```

Linux 使用系统图形及通知库，需在目标发行版执行 `ldd` 与运行检查；XEmbed 托盘和置顶能力由桌面环境决定。当前未执行该平台构建或运行。

## 检查方法

- CTest：Windows 65 项存储/路径检查，含 v5 搁置升级与持久化、暂停/恢复提醒、超过20项到期任务的排队排除、升级前备份、导入结构校验、批量整理、附件复制、目录迁移、自动备份保留、子任务关联与状态约束、提醒改期确认保护、中文/特殊字符搜索及一致备份。非 Windows 不执行专用的系统命令行解析检查。
- `python tests/package_tests.py`：重复打包时排除旧便携数据并保留旧发布目录。
- `LiteTodo --ui-smoke --data-dir 独立空目录`：用虚构任务检查单窗口导航、卡片更多菜单全部动作及删除取消、直接开始、自由浮球悬停和草稿保留、表情优先级、原生圆形窗口区域、应用内提醒确认/延后及草稿保存不撤销延后、旧通知身份回执隔离；保留五种尺寸、子任务、附件、导入、合成 Ctrl+Enter 和正常退出检查。Windows 本机通知总开关关闭，实际测试路径使用应用内兜底；未把合成回执当成系统通知目视验收。
- `scripts/verify-windows.py` 保存上述结果、实际控件渲染 PNG 和本次资源采样；控件离屏渲染不等于操作系统截图或真实输入验收。
- 人工复核仍包括真实鼠标、中文输入法、混合 DPI 多屏、浮球拖动、睡眠唤醒和通知展示。Windows 结果不能替代其他系统验收。

## 第三方与参考

本程序基于 FLTK 项目的部分工作（https://www.fltk.org）。完整许可见 `第三方声明.md` / `THIRD_PARTY_NOTICES.md` 与 `licenses/`。

交互参考 Pometodo 公开源码及用户提供的界面截图。代码及清单图标独立编写，未使用 Pometodo 的商标、源码或图片资源。对应关系见 `docs/interaction-reference.md`。
