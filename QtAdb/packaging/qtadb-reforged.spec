# QtAdb-Reforged 的 RPM 打包描述（自包含包：内含 Qt 库与官方 Linux adb）。
#
# 构建方式（详见 packaging/README.md）：
#   packaging/fetch-linux-platform-tools.sh        # 先取 Linux 版 adb（校验 SHA-256）
#   packaging/build-rpm.sh                         # 组装源码包并调用 rpmbuild
#
# Qt 从哪个前缀构建：默认用本机私有前缀；若系统装有 qt6-qtbase-devel，
# 可以 -Dqt_prefix=/usr 用发行版 Qt（那样就不必打包 Qt 库，见 %install 里的说明）。

%global qt_prefix %{?_qt_prefix}%{!?_qt_prefix:%{getenv:HOME}/.local/qt6-prefix/usr}
%global appdir %{_libdir}/qtadb-reforged

# 包内 Qt 库是随包分发的（不依赖发行版的 qt6-qtbase），因此必须屏蔽自动生成的
# Requires/Provides —— 否则 RPM 会去依赖系统 Qt，bundling 就失去意义了。
%global __requires_exclude ^libQt6.*\.so.*$
%global __provides_exclude ^libQt6.*\.so.*$

Name:           qtadb-reforged
Version:        2.0.0
Release:        1%{?dist}
Summary:        Graphical front-end for adb (Android Debug Bridge)
Summary(zh_CN): adb（Android 调试桥）的图形前端

License:        MIT AND LGPL-3.0-or-later AND Apache-2.0
URL:            https://github.com/SyntaxEntity/QtAdb-Reforged
Source0:        %{name}-%{version}.tar.gz
# 官方 Linux platform-tools 的三个文件由 packaging/fetch-linux-platform-tools.sh 下载
# 并校验 SHA-256 后放进 SOURCES/（二进制不入库，避免每次 clone 都付代价）
Source1:        adb
Source2:        NOTICE.txt
Source3:        source.properties

# cmake/ninja/gcc 由构建脚本确保在 PATH 上；这里不声明 qt6-qtbase-devel，
# 因为默认走私有 Qt 前缀（若改用系统 Qt 构建，请自行加上该 BuildRequires）。
BuildRequires:  ninja-build
BuildRequires:  gcc-c++

%description
QtAdb Reforged 是同一作者对 QtAdb 的续作：一个 Qt 6 桌面工具，把常用 adb 操作
（设备信息、软件包管理、权限列表、激活辅助、按键模拟等）做成图形界面。

本包是自包含的：内含 Qt 6 运行库与 Google 官方 Linux 版 adb，
因此不依赖发行版的 qt6-qtbase，也不需要另装 adb。

%description -l zh_CN
QtAdb Reforged 是同一作者对 QtAdb 的续作：一个 Qt 6 桌面工具，把常用 adb 操作
（设备信息、软件包管理、权限列表、激活辅助、按键模拟等）做成图形界面。

本包是自包含的：内含 Qt 6 运行库与 Google 官方 Linux 版 adb。

%prep
%autosetup -n QtAdb-Reforged-%{version}

# cmake 的路径由构建脚本传入（本机的 cmake 装在 Qt 私有前缀里、且需要 LD_LIBRARY_PATH）。
# 若在装有 cmake 的系统上构建，可直接用系统的 cmake。
%global cmake_cmd %{?_cmake_cmd}%{!?_cmake_cmd:cmake}

# RPM 6 起，%build/%install 的工作目录与源码目录是分开的（构建目录多一层 "-build"），
# 所以这里显式定位源码树，两种布局都兼容。
%global srcdir_new %{_builddir}/%{name}-%{version}-build/QtAdb-Reforged-%{version}
%global srcdir_old %{_builddir}/QtAdb-Reforged-%{version}

%build
cd "$([ -d %{srcdir_new} ] && echo %{srcdir_new} || echo %{srcdir_old})"
"%{cmake_cmd}" -S QtAdb -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=%{qt_prefix} \
    -DCMAKE_INSTALL_PREFIX=%{appdir} \
    -DCMAKE_INSTALL_RPATH=\$ORIGIN/lib \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
"%{cmake_cmd}" --build build

%install
cd "$([ -d %{srcdir_new} ] && echo %{srcdir_new} || echo %{srcdir_old})"

# ① 应用本体（CMake 的安装规则把它放到 %{appdir}/bin/QtAdb，这里挪到包根，
#    使 applicationDirPath() 正好等于 %{appdir} —— 应用据此找 Resources/ 与 lib/）
DESTDIR=%{buildroot} "%{cmake_cmd}" --install build
mv %{buildroot}%{appdir}/bin/QtAdb %{buildroot}%{appdir}/QtAdb
rmdir %{buildroot}%{appdir}/bin

# ② qt.conf：让 Qt 只看包内的插件，绝不落到系统插件目录（版本错配会直接崩）
cat > %{buildroot}%{appdir}/qt.conf <<'QTCONF'
[Paths]
Prefix=.
Plugins=plugins
QTCONF

# ③ 内置 Qt 运行库 + 插件。用 ldd 的实际依赖决定要带哪些库 ——
#    既覆盖应用自身，也覆盖插件（例如 wayland 插件需要 libQt6WaylandClient）。
mkdir -p %{buildroot}%{appdir}/lib %{buildroot}%{appdir}/plugins
plugin_src=%{qt_prefix}/lib64/qt6/plugins
for p in platforms/libqwayland.so platforms/libqxcb.so \
         iconengines/libqsvgicon.so \
         imageformats/libqsvg.so imageformats/libqjpeg.so imageformats/libqgif.so imageformats/libqico.so; do
    install -D -m 0755 "$plugin_src/$p" "%{buildroot}%{appdir}/plugins/$p"
done
cp -a "$plugin_src/wayland-shell-integration/." "%{buildroot}%{appdir}/plugins/wayland-shell-integration/"

# 每个二进制/插件都跑一遍 ldd，把其中的 Qt 库按 soname 复制进来
targets="%{buildroot}%{appdir}/QtAdb"
for so in $(find "%{buildroot}%{appdir}/plugins" -name '*.so'); do targets="$targets $so"; done
for t in $targets; do
    LD_LIBRARY_PATH=%{qt_prefix}/lib64 ldd "$t" 2>/dev/null | awk '/=> \//{print $1, $3}' | while read -r soname realpath; do
        case "$soname" in
            libQt6*) install -m 0755 "$realpath" "%{buildroot}%{appdir}/lib/$soname" ;;
        esac
    done
done

# ④ 内置官方 Linux adb（应用会优先使用 可执行文件目录/Resources/platform-tools/adb）
install -D -m 0755 %{SOURCE1} "%{buildroot}%{appdir}/Resources/platform-tools/adb"
install -D -m 0644 %{SOURCE2} "%{buildroot}%{appdir}/Resources/platform-tools/NOTICE.txt"
install -D -m 0644 %{SOURCE3} "%{buildroot}%{appdir}/Resources/platform-tools/source.properties"

# ⑤ 桌面集成
install -D -m 0644 QtAdb/packaging/qtadb-reforged.desktop \
    "%{buildroot}%{_datadir}/applications/qtadb-reforged.desktop"
install -D -m 0644 QtAdb/packaging/io.github.syntaxentity.QtAdbReforged.metainfo.xml \
    "%{buildroot}%{_datadir}/metainfo/io.github.syntaxentity.QtAdbReforged.metainfo.xml"
install -D -m 0644 QtAdb/image/ico/logo.svg \
    "%{buildroot}%{_datadir}/icons/hicolor/scalable/apps/qtadb-reforged.svg"

# ⑥ /usr/bin 下的入口（相对符号链接；applicationDirPath() 会解析到包目录，
#    因此内置 adb 与 Qt 插件都能被找到）
mkdir -p "%{buildroot}%{_bindir}"
ln -s %{appdir}/QtAdb "%{buildroot}%{_bindir}/QtAdb"

# ⑦ 许可证与文档：交给 %license / %doc 处理（见 %files
%license %{_datadir}/licenses/%{name}/LICENSE
%license %{_datadir}/licenses/%{name}/LICENSE.LGPL-3.0
%license %{_datadir}/licenses/%{name}/LICENSE.GPL-3.0
%license %{_datadir}/licenses/%{name}/NOTICE-platform-tools.txt
%doc %{_datadir}/doc/%{name}/README.md
%doc %{_datadir}/doc/%{name}/AI-REFACTOR-AUDIT.md
%{_bindir}/QtAdb
%{appdir}/QtAdb
%{appdir}/qt.conf
%{appdir}/lib/
%{appdir}/plugins/
%{appdir}/Resources/
%{_datadir}/applications/qtadb-reforged.desktop
%{_datadir}/metainfo/io.github.syntaxentity.QtAdbReforged.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/qtadb-reforged.svg

%changelog
* Mon Oct 13 2025 SyntaxEntity <SyntaxEntity@users.noreply.github.com> - 2.0.0-1
- 首个 Linux 打包版本：自包含 Qt 6 运行库 + Google 官方 Linux 版 adb。
- 代码为 91 轮 AI 辅助重构后的结果（不新增功能）：异步化、平台解耦、i18n、
  缺陷修复 20 余项；详见包内 AI-REFACTOR-AUDIT.md。
