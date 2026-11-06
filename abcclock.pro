QT += core gui widgets dbus

CONFIG += link_pkgconfig
PKGCONFIG += dbus-1

TARGET = abcclock
TEMPLATE = app

CONFIG += c++17

SOURCES += main.cpp

# 安装路径（make install 使用）
target.path = /usr/bin
INSTALLS += target

desktop.path = /usr/share/applications
desktop.files = abcclock.desktop
INSTALLS += desktop