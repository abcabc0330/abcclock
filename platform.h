// 平台相关的窗口修饰（Linux 为空操作）
#pragma once

class QWidget;

// 在窗口首次显示（NSWindow/X 窗口已创建）后调用：
// macOS 把窗口层级提到 floating、加入所有 Space 且不随全屏应用消失；
// Linux 无需处理（override-redirect 已由 Qt::X11BypassWindowManagerHint 完成）。
void platformSetupWindow(QWidget* w);
