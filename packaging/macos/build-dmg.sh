#!/usr/bin/env bash
# 在 macOS 上本地构建 abcclock.app 并打包 dmg（与 CI 流程等价）
#   QTDIR=~/Qt/6.12.0/macos ./packaging/macos/build-dmg.sh
set -euo pipefail
cd "$(dirname "$0")/../.."

QTDIR="${QTDIR:-$HOME/Qt/6.12.0/macos}"
[ -x "$QTDIR/bin/qmake" ] || { echo "未找到 $QTDIR/bin/qmake，可用 aqtinstall 安装 Qt 6.12" >&2; exit 1; }

"$QTDIR/bin/qmake" abcclock.pro CONFIG+=release CONFIG+=sdk_no_version_check \
    QMAKE_APPLE_DEVICE_ARCHS="arm64 x86_64"
make -j"$(sysctl -n hw.ncpu)"
"$QTDIR/bin/macdeployqt" abcclock.app -always-overwrite
codesign --force --deep --sign - abcclock.app

rm -rf dist abcclock-macos.dmg
mkdir -p dist
cp -R abcclock.app dist/
ln -s /Applications dist/Applications
hdiutil create -volname abcclock -srcfolder dist -ov -format UDZO abcclock-macos.dmg
echo "完成：$(pwd)/abcclock-macos.dmg"
