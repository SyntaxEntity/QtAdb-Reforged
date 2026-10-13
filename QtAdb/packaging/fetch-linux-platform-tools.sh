#!/usr/bin/env bash
# 取得 Google 官方 Linux 版 platform-tools，并把需要的文件放到 packaging/platform-tools-linux/。
#
# 为什么用脚本而不是把二进制提交进仓库：adb 约 7 MB，提交进去会让每次 clone 都付出代价；
# 而固定 SHA-256 之后，构建同样是可复现的（校验不过就中止）。
# 用法：packaging/fetch-linux-platform-tools.sh [输出目录]
set -euo pipefail

URL="https://dl.google.com/android/repository/platform-tools-latest-linux.zip"
SHA256="d230f13842f60f782a8645f9c813f8f845bf36089ea7289f28c48f17979313f1"
OUT="${1:-$(cd "$(dirname "$0")" && pwd)/platform-tools-linux}"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
echo "下载 $URL"
curl -fsSL -o "$tmp/pt.zip" "$URL"

actual="$(sha256sum "$tmp/pt.zip" | awk '{print $1}')"
if [ "$actual" != "$SHA256" ]; then
    echo "✗ SHA-256 不符：期望 $SHA256，实际 $actual" >&2
    echo "  上游可能已更新 platform-tools；确认无误后再更新本脚本里的校验值。" >&2
    exit 1
fi
echo "SHA-256 校验通过 ✓"

unzip -q "$tmp/pt.zip" -d "$tmp"
mkdir -p "$OUT"
install -m 0755 "$tmp/platform-tools/adb" "$OUT/adb"
install -m 0644 "$tmp/platform-tools/NOTICE.txt" "$OUT/NOTICE.txt"
install -m 0644 "$tmp/platform-tools/source.properties" "$OUT/source.properties"
echo "已放入 $OUT："
ls -l "$OUT" | sed 's/^/  /'
