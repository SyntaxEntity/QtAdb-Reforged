# QtAdb-Reforged

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?style=for-the-badge&logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6.11.2-blue?style=for-the-badge&logo=qt)
![Platform](https://img.shields.io/badge/Platform-Fedora%2044%20%7C%20Wayland-blue?style=for-the-badge&logo=fedora)
![license](https://img.shields.io/badge/license-MIT-blue?style=for-the-badge)

adb 的 Qt 6 图形前端：把常用 adb 操作做成界面，并内置 adb 查找，面向不想背命令行的 Android 用户与开发者。
本仓库是同一作者旧仓库 [LapplandSp/QtAdb](https://github.com/LapplandSP/QtAdb) 的续作（旧账号 `@LapplandSP` → 现账号 `@SyntaxEntity`）。

> 版本 `2.0.0` ｜ 状态：开发中 ｜ 已知限制见 [已知限制](#已知限制)

## 功能

QtAdb 把 adb 的常见操作图形化，主要能力：

- 已**内置 adb 环境**，并可在运行时自行配置，使用者无需关心环境变量。
- 支持 Windows 11 中的 Android 子系统（**WSA**）。*仅可在 WSA 运行时连接 WSA。*
- 支持 Android R (11) 及以上版本的**无线调试**。
- 提供命令模板，使用者可直接复制命令并自行执行。
- 支持多种状态显示及使用：开机、Recovery 及 adb sideload。

### 设备信息

- 获取设备型号、屏幕分辨率、屏幕密度（DPI）、Android Id、安卓版本以及处理器型号
- 修改 DPI、修改屏幕分辨率
- 实时监控 CPU 使用率、内存使用率

### 激活

- 提供主流工具应用的激活：[黑阈](https://brevent.jianyv.com/)、[冰箱](https://iceboxdoc.catchingnow.com/)、[小黑屋](https://stopapp.https.gs/)、[Shizuku](https://shizuku.rikka.app/zh-hans/)、*黑洞*、[太极](https://taichi.cool/)、[Island](https://island.oasisfeng.com/)、Scene、[看帧数+](https://www.coolapk.com/apk/com.watchfps)、[权限狗](https://www.coolapk.com/apk/com.web1n.permissiondog)、[雹](https://github.com/aistra0528/Hail)、[安装狮](https://www.coolapk.com/apk/com.modosa.apkinstaller)、[Dhizuku](https://github.com/iamr0s/Dhizuku)
- 为应用授予其所需的权限：BatteryGuru（使用情况访问、修改安全系统设置、读取 DUMP）
- 更多应用正在添加中

### 设备控制

- 电源控制：关机、重启到系统、重启到 Recovery、重启到 Fastboot
- 按键模拟：
  - 系统导航：返回键、主页键、菜单键、任务键
  - 实体按键：电源键、音量 +、音量 −、静音、拍照（处于相机应用中时）、唤起助理应用
  - 媒体控制：播放音乐、停止播放、上一曲、继续、暂停、下一曲
  - 其他指令：亮度 −、亮度 +、系统休眠、点亮屏幕、挂断电话
  - 拉起系统应用：设置、通讯录、拨号、浏览器、音乐、日历、计算器
- 命令行：运行自定义的 adb 命令；在 adb 环境中打开命令行窗口

### 软件包管理器

- 安装应用
- 列出已知的权限
  - 可单独打开大窗口查看列表
  - 可按以下要求输出：按权限组整理 / 输出所有信息 / 输出简短摘要 / 仅列出危险权限 / 仅列出用户将看到的权限
- 列出已知的权限组
- 列出软件包：卸载、停用、启用、清除数据；获取包名、安装者、关联文件等信息
- 列出系统的所有功能
- 列出当前设备支持的所有库
- 用户：列出当前设备中的所有用户；删除选中用户

### Recovery 功能

- 侧载
  - 当设备处于 adb sideload 模式时，可使用侧载刷入 zip 刷机包
  - **这是危险功能，即使正确地使用也可能导致设备故障，请确保您有足够的排障能力后使用**
  - **使用侧载功能时请不要关闭程序**

### 其它功能

- 账户：查看设备中各个用户已登录的账户
- 去除叹号：修改验证服务器，提供多个预设选项（MIUI、EMUI 等国内验证服务器）
- 修改过渡动画：动画时长、过渡动画、窗口动画
- 状态栏与导航栏：隐藏状态栏图标（静音/震动、定位、麦克风、录屏、WIFI、热点、飞行模式、耳机、闹钟、蓝牙、NFC）、重置状态栏图标、全局隐藏状态栏与导航栏
- 修改系统震感强度

## 安装

**Linux（RPM，需自行构建）**：本仓库目前**没有公开的预编译包**，需要从源码构建并打成本地 RPM：

```bash
cd QtAdb
packaging/build-rpm.sh
sudo dnf install packaging/rpmbuild/RPMS/x86_64/qtadb-reforged-2.0.0-1.fc44.x86_64.rpm
```

打出的包是**自包含**的：内含 Qt 6 运行库与 Google 官方 Linux 版 adb，因此不需要另装 `qt6-qtbase` 或 `android-tools`。细节与边界见 [QtAdb/packaging/README.md](QtAdb/packaging/README.md)。

**Windows**：本仓库尚未提供 Windows 构建，请使用上游 1.7 的发布包。

**依赖**：程序优先使用可执行文件同目录下的 `Resources/platform-tools/adb`；找不到时回退到 `PATH` 上的 `adb`。系统里没有 adb 且未随程序分发时，所有 adb 功能都会失败。

## 使用

1. 打开设备的 USB 调试，并在手机上允许本机调试授权；
2. 启动后左侧为功能页，顶部显示当前设备；
3. 未检测到设备时界面仍可浏览，各页会显示 adb 的原始输出或报错。

### 常见问题

- **Q：这个分支加了 AI 功能吗？**

  A：没有。AI 用在**重构代码这个过程**里（91 轮、带可复现的验证矩阵），产品本身没有对话、
  自动化之类的 AI 功能——这也是仓库名用 `-Reforged`（强调"重做"）而不是 `-AI` 的原因。

- **Q：这和 `LapplandSp/QtAdb` 是什么关系？**

  A：**同一个作者的续作**。旧账号 `@LapplandSP` 已无法登录，因此迁移到 `@SyntaxEntity`
  继续维护，仓库名加上 `-Reforged` 以免与同名项目混淆。功能范围与 1.7 一致，
  此外做了大量重构与缺陷修复（见上文与 [`docs/AI-REFACTOR-AUDIT.md`](docs/AI-REFACTOR-AUDIT.md)）。

- **Q：为什么还叫 QtAdb？**

  A：保持连续性。可执行文件仍为 `QtAdb`，因此 `~/.config/QtAdb/` 下的设备别名等数据不受影响。

- **Q：不是有环境吗？怎么还是跑不了🥺**

  A：您下载的是压缩包，请全部解压后再使用；请确保您的设备已打开 USB 调试并正确连接。

- **Q：我已经解压，仍然无法使用**

  A：部分 ADB 工具（如秋之盒、搞机工具箱等）在退出后不会杀死其拉起的 ADB 服务端；
  QtAdb 异常退出后也可能来不及杀死 adb 进程。请进入任务管理器结束 `adb.exe` 后重试，
  若仍无法使用请尝试重启系统。

- **Q：怎么这么多类似乱码的报错😱**

  A：已经尽可能减少报错。若弹出的"未知错误"对话框中的报错无法构成一个完整的句子，
  可以根据内容视情况忽略，并查看另一条报错。

  > Tips：Qt 封装的进程类在读取 adb 错误输出时，有时会把一条错误随机分成多段，
  > 每段都被报告为一条独立错误；用于识别报错类型的关键字可能只出现在其中一段，
  > 甚至被拦腰截断。此时无法分辨这些不含关键字的信息，于是弹出"未知错误"并显示原文。
  > 这个问题目前没有彻底的解决办法。

- **Q：应用无法安装🙁**

  A：请排查是否为降级安装、在不兼容的安卓版本上安装等情况，并根据报错内容自行判断。

- **Q：侧载未响应**

  A：侧载与应用安装类似，功能暂未完全实现，为不稳定功能。若侧载包过大可能导致等待时间较长，
  请不要进行操作，具体以 recovery 输出为准。**请具备一定容错能力后再使用。**

- **Q：按键模拟不生效🤨**

  A：此功能因设备而异，部分 ROM 中并没有提供相关功能（比如播放音乐、拉起计算器等）。
  若弹出错误提示，可以进行反馈。

- **Q：连接无线调试卡死**

  A：分情况讨论：若更换另一部手机可以连接，则可能是手机端 ROM 的问题，
  请重启并再次尝试；若设备没有问题则可能是路由器的问题，请更换其它 WI-FI 网络再试。

- **Q：无法连接 WSA**

  A：请确保 **WSA 已安装并正在运行**：仅仅安装完成是不够的，还需要在开始菜单中打开
  "适用于 Android™ 的 Windows 子系统设置"应用，选择"开发人员"，并打开"开发人员模式"。
  在此之后，可能还需要选择"系统"、打开"文件"以确保 WSA 正在运行。

- **Q：闪退**

  A：请提交反馈，并描述发生闪退之前您进行的操作，谢谢！

- **Q：我可以催更吗**🥺

  A：当然可以催更，它现在仍处于开发中，并没有正式发布，仍需要长时间的调试。

## 配置

| 项 | 位置 / 行为 |
| --- | --- |
| 设备别名 | `~/.config/QtAdb/device-names.ini`（按设备地址保存，界面优先显示别名） |
| adb 查找顺序 | 可执行文件同目录 `Resources/platform-tools/adb` → 否则回退到 `PATH` 上的 `adb` |
| 界面文案语言 | 跟随系统语言；英文翻译尚未覆盖全部词条（见 [已知限制](#已知限制)） |

## 构建（开发）

工程同时提供 **CMake** 与 **qmake** 两套等价的构建描述（`QtAdb/CMakeLists.txt` 与 `QtAdb/QtAdb.pro`）。
**两份描述里的源文件列表与翻译文件列表必须保持一致**——新增或删除文件时两边都要改。

### CMake（推荐，Windows / Linux 通用）

要求：Qt **6.2+**（C++17）、CMake **3.21+**、Ninja 或 Make。

```bash
cmake -S QtAdb -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
cmake --build build -j
```

- 仅依赖 `Qt6::Widgets`；**嵌入**资源（`.qrc` 列出的 128 个文件）合计约 **2.7 MiB**，已使用 `qt_add_big_resources` 处理。
- `QtAdb/Resources`（约 14 MB，含 `platform-tools`）**不会被嵌入**，而是在 Windows 构建时复制到可执行文件目录，与 `MainWindow::initEnvironmentPATH()` 的查找约定一致。
- 英文翻译由 `lrelease` 编译为 `QtAdb_en.qm` 并嵌入到 `:/i18n/`（找不到 `lrelease` 时该步骤会被跳过，程序仍可构建）。

### qmake（原有方式）

- 建议使用 Qt 6.2.4，Kit 选择 MinGW。
- 运行环境会在 `MainWindow` 构造时被更改至 `./Resources/platform-tools`，请确保 debug 或 release 时 `Resources` 目录位置正确。
- 翻译：`QtAdb.pro` 里的 `TRANSLATIONS`、`CONFIG += lrelease`、`CONFIG += embed_translations` 与 CMake 侧的 `QTADB_TS_FILES` 指向**同一个** `.ts` 文件。

> 说明：两套构建描述都已固定为 **C++17**（`QtAdb.pro` 的 `CONFIG += c++17` 与 `CMakeLists.txt` 的 `CMAKE_CXX_STANDARD 17`），无需再手工改动。

回归验证设施**不随仓库发布**（见 [已知限制](#已知限制)），因此这里不提供可复制的测试命令。

## 项目结构

```
QtAdb-Reforged/                    仓库根：与"项目"有关的东西
├── README.md
├── LICENSE                        MIT（原始版权归 Github@LapplandSP）
├── privacy.md
├── AGENTS.md                      维护者与 agent 的上手契约
├── docs/AI-REFACTOR-AUDIT.md      本次重构的逐轮记录
└── QtAdb/                         应用本体：与"构成程序"有关的东西
    ├── CMakeLists.txt             CMake 构建描述
    ├── QtAdb.pro                  qmake 构建描述（与上面那份必须同步）
    ├── packaging/                 Linux RPM 打包材料
    ├── translations/              英文翻译（.ts）
    └── Resources/                 随程序分发的 platform-tools（Windows 版）
```

一条规则：**仓库根放"关于项目"的，`QtAdb/` 放"构成程序"的**。所以构建时源码目录是 `QtAdb/`（`cmake -S QtAdb -B build`），打包材料在 `QtAdb/packaging/` 下。

## 技术栈

| 层 | 选择 |
| --- | --- |
| 界面 | Qt 6.11.2（Widgets），C++17 |
| 构建 | CMake ≥ 3.21 或 qmake；Ninja 或 Make |
| 打包 | rpmbuild（Fedora 44）；自包含 Qt 运行库 + Google 官方 Linux adb |
| 验证 | AddressSanitizer / UndefinedBehaviorSanitizer + 静态检查脚本 + 桩程序驱动的运行场景（设施在仓库外） |

## 已知限制

- **平台**：只在 **Fedora 44 + Wayland** 上验证过；其他发行版不保证可运行（glibc 较新）。
- **Windows**：未构建、未运行。只验证了两份构建描述等价、代码不再依赖 Windows 专有头文件。
- **qmake**：未实际构建，只做了与 CMake 的源文件列表与翻译列表一致性校验。
- **回归矩阵**：验证设施（检查脚本与探针）位于仓库之外，**未随仓库发布**；文档里提到的 32 项矩阵需要该设施才能复现。
- **adb 版本**：RPM 内置 Google 官方 adb 37.0.1。若系统里已有不同版本的 adb server 在运行，启动时可能触发 adb 重启该 server（本机安装已改为使用系统 adb 以避免这一点）。
- **翻译**：英文翻译共 436 条词条，已译 345 条，其余 91 条回退显示中文原文。
- **侧载 / Recovery**：上游标注为"暂未完全实现、为不稳定功能"，以 recovery 输出为准。
- **无界面截图**：仓库内只有应用图标，没有界面截图。
- **旧仓库无法更新**：旧账号已无法登录，旧仓库 `LapplandSp/QtAdb` 无法挂上"已迁移"说明，迁移信息只能写在本仓库。

## 路线图

上游遗留计划（来自旧仓库）：

- 跨平台、文件管理、集成 scrcpy
- 命令：更改屏幕边距
- 控制台：优化 err 处理

本仓库待办：

- 补齐英文翻译剩余 91 条词条
- 为 Linux 发布提供 CI 构建产物
- 决定验证设施是否随仓库发布（当前"32 项矩阵"无法被读者自证）

## 项目文档

重构摘要（本仓库相对上游 1.7 的变化，均为可复核的计数）：

| 指标 | 上游 1.7 | 现在 |
| --- | ---: | ---: |
| `qDebug()` 调试输出 | 278 | 0 |
| 注释掉的代码 | 474 | 0 |
| 可翻译文案（`tr()`） | 0 | 213 |
| 最大源文件 | `mainwindow.cpp` 911 行 | 714 行 |
| 函数规模 | — | 342 个，中位数 5 行 |

| 文档 | 内容 |
| --- | --- |
| [docs/AI-REFACTOR-AUDIT.md](docs/AI-REFACTOR-AUDIT.md) | 重构过程的逐轮记录（唯一真相）：做了什么、为什么、以及为什么不做什么 |
| [AGENTS.md](AGENTS.md) | 维护者与 agent 的上手契约、硬性约束、已知边界 |
| [QtAdb/packaging/README.md](QtAdb/packaging/README.md) | 打包方式、包内布局、必须随发布告知的边界 |

## 贡献 / 反馈

- 请在本仓库提交 issue（功能建议、缺陷、重构意见都欢迎）。若您有想要添加的可激活应用，请附上它的命令与文档/官网。
- 旧仓库与其社群渠道属于**同一位作者**；由于旧账号无法登录，那里的内容不会再更新，请以本仓库为准。

## 许可证

[MIT](LICENSE)，原始版权归 `Github@LapplandSP`（同一作者的旧账号）；本仓库的修改同样以 MIT 发布。

**第三方组件**：分发的 RPM 内含 Qt 6（LGPL-3.0-only）与 Google platform-tools 中的 adb（Apache-2.0），相应许可证文本随包安装在 `/usr/share/licenses/qtadb-reforged/` 下。

**第三方资源**：图标来自 [RemixIcon](https://github.com/Remix-Design/RemixIcon) 与 [IconPark](https://github.com/bytedance/IconPark)；字体为 [LXGW WenKai Lite](https://github.com/lxgw/LxgwWenKai-Lite)。
