<div align=center><img src="QtAdb/image/ico/logo.svg" width="120" alt="QtAdb-Reforged"></div>

# QtAdb-Reforged

QtAdb 是一个基于 [Qt](https://www.qt.io/) 的 Android 工具集合：内置 adb 环境，把常用 adb 操作做成图形化界面。

> **本仓库是 [LapplandSp/QtAdb](https://github.com/LapplandSP/QtAdb) 的续作**，同一个作者：
> 旧账号 `@LapplandSP` 已无法登录，现由 `@SyntaxEntity` 继续维护。
> 在原有代码之上做了 91 轮 AI 辅助重构，目标是让代码"健壮 / 可维护 / 可拓展 / 易读"，
> **不新增功能**。可执行文件仍叫 `QtAdb`，配置目录（`~/.config/QtAdb/`）保持不变。

<p align="center">
  <em>同一个工具，重做了一遍代码。</em>
</p>

---

## 📌 项目沿革（同一作者的续作）

| 项目 | 说明 |
| --- | --- |
| 旧仓库 | [LapplandSp/QtAdb](https://github.com/LapplandSP/QtAdb)（版本 1.7） |
| 旧官网 | https://lapplandsp.github.io/QtAdb-Pages/ |
| 作者 | 两个仓库是**同一个作者**：旧账号 `@LapplandSP` → 现账号 `@SyntaxEntity` |
| 本仓库定位 | **正统续作**：功能范围与 1.7 一致（不新增功能），另外做了大量重构与缺陷修复 |
| 为什么迁移 | 旧账号已无法登录，原仓库无法继续更新 |
| 许可 | MIT，保留原始版权声明 `Copyright (c) 2023 Github@LapplandSP`（见 [LICENSE](LICENSE)） |
| 提问去处 | 请在本仓库提 issue —— 旧仓库已无法再接收改动 |

> ⚠️ 因为旧账号登不上，**旧仓库自己没法挂上"已迁移"的说明**，所以这段沿革只能写在这里。
> 如果你是从旧仓库找过来的：以本仓库为准，界面与使用方式与 1.7 一致。

仓库名为什么加 `-Reforged`：`QtAdb` 这个名字此前已被多个**无关**项目使用（旧仓库的 FAQ 里也提到过），
加后缀既保留连续性，也让检索时能一眼分清"正统续作"与同名项目。

## 🔍 这个分支改了什么（都能复现）

| 指标 | 上游 1.7 | 现在 |
| --- | ---: | ---: |
| `qDebug()` 调试输出 | 278 | **0** |
| 注释掉的代码 | 474 | **0** |
| 只声明不引用的函数 / 只写不读的成员 | — | **0 / 0** |
| 直接 `new` | 253 | **130** |
| 可翻译文案（`tr()` 调用） | 0 | **213** |
| 最大源文件 | `mainwindow.cpp` 911 行 | **714 行** |
| 函数规模 | — | 342 个函数，中位数 **5 行**；超过 60 行的只有 1 个（数据表） |

具体做了什么：

- **异步化**：删除同步的 `adbProcess`（21 处会让界面卡住的调用），改为单队列 + 超时 + 取消 + 退出收口的 `AdbClient`；设备挂起时界面不再无响应。
- **平台解耦**：移除 22 个编译单元里无条件包含的 `Windows.h`；Linux 上以定时器轮询兜底热插拔通知，走与 Windows 原生通知**相同**的下游路径。
- **国际化**：界面文案全部包 `tr()`，新增英文翻译（`translations/QtAdb_en.ts`，436 词条 / 已译 345），构建时嵌入 `:/i18n/`。
- **修复缺陷 20 余项**：畸形 adb 输出越界解析、`QList` 越界断言并 abort、阴影句柄变空指针、输入框验证器无父对象泄漏、页面/对话框/通知控件泄漏、表格模型累积、动画缺父对象、堆上的 `QMetaObject::Connection`、退出时在途请求、子页监视命令缺 `-s`、设备别名被覆盖、`QSettings` 写入失败不可见、未登记的解释键静默失效……
- **结构**：页面工厂与列表条目改为数据表（**新增子页 = 加一行**）、重复样板收成具名助手、重复样式串集中到 `uistyles.h`、无线调试与 Magisk 窗口拆成独立文件。

**怎么验证这些说法**：仓库之外的验证设施提供 **32 项回归矩阵**——完整应用场景下 LSan 报
**0 字节 / 0 次分配**，UBSan 下 **0 处未定义行为**，另有 8 个运行场景、6 个探针、9 项静态检查。
逐轮的决策过程、以及**"为什么不做某些事"**，记录在 [`docs/AI-REFACTOR-AUDIT.md`](docs/AI-REFACTOR-AUDIT.md)。

> ⚠️ **诚实边界**：运行验证是在 **Linux** 上完成的。Windows 侧只验证了"两份构建描述等价、
> 代码不再依赖 Windows 专有头文件"，**没有在 Windows 上实际构建与运行**。
> qmake 侧只做了与 CMake 的列表一致性校验，未实际构建。英文翻译的词条完整性经过检查，
> 但"特定系统语言下运行时是否加载到对应 `.qm`"未做端到端验证。

---

## 🔩特性

- 已**内置 adb 环境**，并可在运行时自行配置，使用者无需关心环境变量。
- 支持 Windows 11 中的 Android 子系统（**WSA**）。*仅可在 WSA 运行时连接 WSA。*
- 支持 Android R (11) 及以上版本的**无线调试**。
- 提供命令模板，使用者可直接复制命令并自行执行。
- 支持多种状态显示及使用：开机、Recovery 及 adb sideload。

*注意：暂不支持比 Windows 10 更早的 Windows 系统。*

---

## 🛠️功能

### ℹ️ 设备信息

- 获取设备型号、屏幕分辨率、屏幕密度（DPI）、Android Id、安卓版本以及处理器型号
- 修改 DPI、修改屏幕分辨率
- 实时监控 CPU 使用率、内存使用率

### ✔️ 激活

- 提供主流工具应用的激活：[黑阈](https://brevent.jianyv.com/)、[冰箱](https://iceboxdoc.catchingnow.com/)、[小黑屋](https://stopapp.https.gs/)、[Shizuku](https://shizuku.rikka.app/zh-hans/)、*黑洞*、[太极](https://taichi.cool/)、[Island](https://island.oasisfeng.com/)、Scene、[看帧数+](https://www.coolapk.com/apk/com.watchfps)、[权限狗](https://www.coolapk.com/apk/com.web1n.permissiondog)、[雹](https://github.com/aistra0528/Hail)、[安装狮](https://www.coolapk.com/apk/com.modosa.apkinstaller)、[Dhizuku](https://github.com/iamr0s/Dhizuku)
- 为应用授予其所需的权限：BatteryGuru（使用情况访问、修改安全系统设置、读取 DUMP）
- 更多应用正在添加中

### 🎮 设备控制

- 电源控制：关机、重启到系统、重启到 Recovery、重启到 Fastboot
- 按键模拟：
  - 系统导航：返回键、主页键、菜单键、任务键
  - 实体按键：电源键、音量 +、音量 −、静音、拍照（处于相机应用中时）、唤起助理应用
  - 媒体控制：播放音乐、停止播放、上一曲、继续、暂停、下一曲
  - 其他指令：亮度 −、亮度 +、系统休眠、点亮屏幕、挂断电话
  - 拉起系统应用：设置、通讯录、拨号、浏览器、音乐、日历、计算器
- 命令行：运行自定义的 adb 命令；在 adb 环境中打开命令行窗口

### 📦 软件包管理器

- 安装应用
- 列出已知的权限
  - 可单独打开大窗口查看列表
  - 可按以下要求输出：按权限组整理 / 输出所有信息 / 输出简短摘要 / 仅列出危险权限 / 仅列出用户将看到的权限
- 列出已知的权限组
- 列出软件包：卸载、停用、启用、清除数据；获取包名、安装者、关联文件等信息
- 列出系统的所有功能
- 列出当前设备支持的所有库
- 用户：列出当前设备中的所有用户；删除选中用户

### 🔧 Recovery 功能

- 侧载
  - 当设备处于 adb sideload 模式时，可使用侧载刷入 zip 刷机包
  - **这是危险功能，即使正确地使用也可能导致设备故障，请确保您有足够的排障能力后使用**
  - **使用侧载功能时请不要关闭程序**

### 🔮 其它功能

- 账户：查看设备中各个用户已登录的账户
- 去除叹号：修改验证服务器，提供多个预设选项（MIUI、EMUI 等国内验证服务器）
- 修改过渡动画：动画时长、过渡动画、窗口动画
- 状态栏与导航栏：隐藏状态栏图标（静音/震动、定位、麦克风、录屏、WIFI、热点、飞行模式、耳机、闹钟、蓝牙、NFC）、重置状态栏图标、全局隐藏状态栏与导航栏
- 修改系统震感强度

---

## 💕Q&A

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

### 🍘 项目计划（上游）

> Q：为什么刷机相关命令这么少？为什么没有修改系统文件衍生的玩法？
>
> A：危险性较高，防小白。不过也不是不能加。

- 跨平台
- 文件管理
- 集成 scrcpy
- 命令：更改屏幕边距
- 控制台：优化 err 处理

---

## 目录结构

```
QtAdb-Reforged/                    仓库根：与"项目"有关的东西
├── README.md
├── LICENSE                        MIT（原始版权归 Github@LapplandSP）
├── privacy.md
├── docs/AI-REFACTOR-AUDIT.md      本次重构的逐轮记录
└── QtAdb/                         应用本体：与"构成程序"有关的东西
    ├── CMakeLists.txt             CMake 构建描述
    ├── QtAdb.pro                  qmake 构建描述（与上面那份必须同步）
    ├── packaging/                 Linux RPM 打包材料
    ├── translations/              英文翻译（.ts）
    └── Resources/                 随程序分发的 platform-tools（Windows 版）
```

一条规则：**仓库根放"关于项目"的，`QtAdb/` 放"构成程序"的**。
所以构建时源码目录是 `QtAdb/`（`cmake -S QtAdb -B build`），打包材料在 `QtAdb/packaging/` 下。

## 构建

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
- `QtAdb/Resources`（约 14 MB，含 `platform-tools`）**不会被嵌入**，而是在 Windows 构建时复制到可执行文件目录，
  与 `MainWindow::initEnvironmentPATH()` 的查找约定一致。
- 英文翻译由 `lrelease` 编译为 `QtAdb_en.qm` 并嵌入到 `:/i18n/`（找不到 `lrelease` 时该步骤会被跳过，程序仍可构建）。

### qmake（原有方式）

- 建议使用 Qt 6.2.4，Kit 选择 MinGW。
- 运行环境会在 `MainWindow` 构造时被更改至 `./Resources/platform-tools`，
  请确保 debug 或 release 时 `Resources` 目录位置正确。
- 翻译：`QtAdb.pro` 里的 `TRANSLATIONS`、`CONFIG += lrelease`、`CONFIG += embed_translations` 与 CMake 侧的
  `QTADB_TS_FILES` 指向**同一个** `.ts` 文件。

> 说明：两套构建描述都已固定为 **C++17**（`QtAdb.pro` 的 `CONFIG += c++17` 与
> `CMakeLists.txt` 的 `CMAKE_CXX_STANDARD 17`），无需再手工改动。

---

## 🧬 资源与致谢

### 图标

- 主要：[RemixIcon](https://github.com/Remix-Design/RemixIcon)
- 补充：[IconPark](https://github.com/bytedance/IconPark)

### 字体

- [lxgw/LxgwWenKai-Lite](https://github.com/lxgw/LxgwWenKai-Lite)

### 代码

- 原始项目：[LapplandSp/QtAdb](https://github.com/LapplandSP/QtAdb)（MIT，`Copyright (c) 2023 Github@LapplandSP`）
- 本分支的修改同样以 **MIT** 发布；重构过程与验证记录见 [`docs/AI-REFACTOR-AUDIT.md`](docs/AI-REFACTOR-AUDIT.md)

---

## 🙋 反馈

- 请在本仓库提交 issue（功能建议、缺陷、重构意见都欢迎）。
  若您有想要添加的可激活应用，请附上它的命令与文档/官网。
- 旧仓库与其社群渠道属于**同一位作者**；由于旧账号无法登录，那里的内容不会再更新，请以本仓库为准。

十分感谢你的建议！
