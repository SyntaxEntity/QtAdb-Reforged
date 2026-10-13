#!/usr/bin/env bash
# 组装源码包并调用 rpmbuild 打出 QtAdb-Reforged 的 RPM（自包含：Qt 运行库 + Linux adb）。
#
#   packaging/build-rpm.sh [输出目录]
#
# 会做三件事：
#   1. 确保 packaging/platform-tools-linux/ 里有官方 Linux adb（没有就调用 fetch 脚本）
#   2. 用 git archive 生成 SOURCES/qtadb-reforged-2.0.0.tar.gz（因此打包内容 = 已提交内容）
#   3. rpmbuild -bb，产物打印在最后
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"       # 仓库根（packaging/ 在 QtAdb/ 下）
topdir="${1:-$root/packaging/rpmbuild}"

# 本机的 cmake 与 Qt 都在私有前缀里，且 cmake 需要 LD_LIBRARY_PATH 才能运行
QT_PREFIX="${QT_PREFIX:-$HOME/.local/qt6-prefix/usr}"
CMAKE_CMD="${CMAKE_CMD:-$QT_PREFIX/bin/cmake}"
if [ ! -x "$CMAKE_CMD" ]; then
    CMAKE_CMD="$(command -v cmake || true)"
fi
[ -n "$CMAKE_CMD" ] && [ -x "$CMAKE_CMD" ] || { echo "✗ 找不到 cmake（可用 CMAKE_CMD=... 指定）" >&2; exit 1; }
export LD_LIBRARY_PATH="$QT_PREFIX/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# ① Linux 版 adb
if [ ! -x "$here/platform-tools-linux/adb" ]; then
    echo "→ 取官方 Linux platform-tools"
    "$here/fetch-linux-platform-tools.sh" "$here/platform-tools-linux"
fi

# ② 源码包：只含已提交内容，避免把临时文件打进去
if [ -n "$(git -C "$root" status --porcelain)" ]; then
    echo "✗ 工作区有未提交改动 —— 请先提交（源码包由 git archive HEAD 生成）" >&2
    git -C "$root" status --short >&2
    exit 1
fi
mkdir -p "$topdir"/{SOURCES,SPECS,BUILD,BUILDROOT,RPMS,SRPMS}
ver="$(sed -n 's/^Version:[[:space:]]*//p' "$here/qtadb-reforged.spec" | head -1)"
git -C "$root" archive --prefix="QtAdb-Reforged-$ver/" -o "$topdir/SOURCES/qtadb-reforged-$ver.tar.gz" HEAD
cp "$here/qtadb-reforged.spec" "$topdir/SPECS/"
# platform-tools 被 .gitignore 忽略（不进源码包），按 RPM 惯例作为独立 Source 提供
install -m 0755 "$here/platform-tools-linux/adb"              "$topdir/SOURCES/adb"
install -m 0644 "$here/platform-tools-linux/NOTICE.txt"       "$topdir/SOURCES/NOTICE.txt"
install -m 0644 "$here/platform-tools-linux/source.properties" "$topdir/SOURCES/source.properties"

# ③ 打包
rpmbuild -bb "$topdir/SPECS/qtadb-reforged.spec" \
    --define "_topdir $topdir" \
    --define "_cmake_cmd $CMAKE_CMD" \
    --define "_qt_prefix $QT_PREFIX" \
    --define "_sourcedir $topdir/SOURCES" \
    --define "_builddir $topdir/BUILD" \
    --define "_buildrootdir $topdir/BUILDROOT" \
    --define "_rpmdir $topdir/RPMS" \
    --define "_specdir $topdir/SPECS" \
    --define "_srcrpmdir $topdir/SRPMS" 2>&1 | tail -25

echo
echo "产物："
find "$topdir/RPMS" -name '*.rpm' -printf '  %p (%s bytes)\n'
