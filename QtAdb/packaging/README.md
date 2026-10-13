# 打包说明（Linux / RPM）

面向维护者。最终用户不需要看这里 —— 直接装 `.rpm` 即可。

## 一键构建

```bash
packaging/build-rpm.sh            # 产物默认在 packaging/rpmbuild/RPMS/x86_64/
```

脚本做三件事：

1. 确保 `packaging/platform-tools-linux/adb` 存在（否则调用 `fetch-linux-platform-tools.sh` 下载官方 Linux platform-tools 并**校验 SHA-256**）；
2. 用 `git archive HEAD` 生成源码包 —— 因此**打包内容 = 已提交内容**，工作区有未提交改动时会直接报错停下；
3. 调用 `rpmbuild -bb`。

## 这个包为什么是"自包含"的

按作者的选择，包内直接带上运行所需的 Qt 6 库与官方 Linux adb，因此**不依赖**发行版的
`qt6-qtbase`，也**不需要**另装 `android-tools`：

| 内容 | 位置 |
|---|---|
| Qt 运行库（按 soname 复制，来源由 `ldd` 决定） | `/usr/lib64/qtadb-reforged/lib/` |
| Qt 插件（wayland/xcb 平台插件、SVG 图标引擎、常用图片格式、Wayland shell 集成） | `/usr/lib64/qtadb-reforged/plugins/` |
| `qt.conf`（强制只使用包内插件，避免落到系统插件目录导致版本错配） | `/usr/lib64/qtadb-reforged/qt.conf` |
| 官方 Linux `adb`（+ NOTICE、source.properties） | `/usr/lib64/qtadb-reforged/Resources/platform-tools/` |
| 桌面入口、图标、AppStream 元数据 | `/usr/share/...` |
| 许可证（MIT / LGPL-3.0 / GPL-3.0 / adb NOTICE） | `/usr/share/licenses/qtadb-reforged/` |

`/usr/bin/QtAdb` 是指向包目录的相对符号链接；应用通过 `applicationDirPath()` 解析到包目录，
因此能就地找到内置 `adb` 与包内插件。

## Qt 从哪里来

默认使用**本机私有前缀** `~/.local/qt6-prefix/usr`（本项目 CI/开发机上的 Qt 6.11.2）。
若在装有 `qt6-qtbase-devel` 的系统上构建，可以：

```bash
QT_PREFIX=/usr CMAKE_CMD=/usr/bin/cmake packaging/build-rpm.sh
```

此时 `%install` 仍会把 Qt 库打进包（作者的既定选择）；如果希望改为依赖发行版 Qt，
需要去掉 spec 里的库复制步骤与 `__requires_exclude` 两行。

## 已知边界（务必随 Release 说明一起告知用户）

- 运行验证是在 **Fedora 44 + Wayland** 上做的；其他发行版/较老的 glibc **不保证**可运行；
- 包内 Qt 为 6.11.2，与 Fedora 44 系统 Qt 同版本，但**不要**与系统 Qt 混用插件目录（已用 `qt.conf` 隔离）；
- 内置 `adb` 来自 Google 官方 platform-tools（Apache-2.0），版本见包内 `source.properties`。
