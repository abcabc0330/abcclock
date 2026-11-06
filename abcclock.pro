QT += core gui widgets

TARGET = abcclock
TEMPLATE = app

CONFIG += c++17

SOURCES += main.cpp

# Linux：托盘总线探测依赖 libdbus；窗口自定位依赖 override-redirect（X11/XWayland）
unix:!macx {
    QT += dbus
    CONFIG += link_pkgconfig
    PKGCONFIG += dbus-1
    SOURCES += platform_linux.cpp

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
