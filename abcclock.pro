QT += core gui widgets

TARGET = abcclock
TEMPLATE = app

CONFIG += c++17

SOURCES += main.cpp

# Linux：托盘总线探测依赖 libdbus；X11 回退依赖 libxcb；
# 原生 Wayland 走 LayerShellQt（zwlr_layer_shell_v1，overlay 层、永不抢焦点）
unix:!macx {
    QT += dbus
    CONFIG += link_pkgconfig
    PKGCONFIG += dbus-1 xcb
    SOURCES += platform_linux.cpp

    # 系统没有 layer-shell-qt 时也能编译：主程序自动回退 X11/XWayland。
    LAYERSHELL_HEADER = /usr/include/LayerShellQt/window.h
    exists($$LAYERSHELL_HEADER) {
        DEFINES += ABC_HAVE_LAYER_SHELL
        LIBS += -lLayerShellQtInterface
    }

    # 安装路径（make install 使用）
    target.path = /usr/bin
    INSTALLS += target

    desktop.path = /usr/share/applications
    desktop.files = abcclock.desktop
    INSTALLS += desktop
}

# macOS：ObjC++ 设置 NSWindow 层级；产出 .app bundle（dmg 由 macdeployqt/hdiutil 生成）
macx {
    OBJECTIVE_SOURCES += platform_mac.mm
    LIBS += -framework Cocoa
    ICON = abcclock.icns
    QMAKE_INFO_PLIST = Info.plist
    QMAKE_TARGET_BUNDLE_PREFIX = com.abcabc0330
}
