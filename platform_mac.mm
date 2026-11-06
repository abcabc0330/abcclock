#include "platform.h"

#include <QWidget>

#import <Cocoa/Cocoa.h>

void platformSetupWindow(QWidget* w)
{
    if (!w)
        return;
    NSView* view = reinterpret_cast<NSView*>(w->winId());
    NSWindow* win = [view window];
    if (!win)
        return;

    // 悬浮在所有普通窗口之上，且出现在每个 Space（含全屏应用）里
    [win setLevel:NSFloatingWindowLevel];
    [win setCollectionBehavior:(NSWindowCollectionBehaviorCanJoinAllSpaces
                                | NSWindowCollectionBehaviorFullScreenAuxiliary
                                | NSWindowCollectionBehaviorStationary)];
    [win setHidesOnDeactivate:NO];
    // 兜底：即使未打包运行也不进 Dock、不抢前台
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
