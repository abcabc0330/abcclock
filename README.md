# abcclock

一个 192px 宽的桌面悬浮时钟（Qt5 / X11），设计为始终显示在终端窗口上方：
圆角半透明灰底、模拟表盘、星期、日期、三个城市的时区时间，以及三条天气信息。


## 功能

- **模拟表盘**（192×192）：时针/分针/秒针 + 12 刻度
- **星期行**：`FRI 星期五`（英文缩写大写 + 中文星期，随当天日期变化）
- **日期行**：年-月-日，大字号自动适应宽度
- **三城市时区**：北京 / 纽约 / 洛杉矶，城市名横排 + 当地时间（跟随系统时区换算）
- **三行天气**：城市 / 天气现象 / 温度区间
- **整体圆角 12px + 半透明灰 `rgba(20,20,20,150)`**，四角透明，可透出桌面
- 窗口使用 `override_redirect`，全屏终端（如 ghostty）也不会遮住它

## 构建

依赖：Qt5（`qt5-base`）、C++17 编译器。

```bash
qmake-qt5 abcclock.pro
make -j$(nproc)
```

产物为 `abcclock`。运行：

```bash
./abcclock 北京
```

### 窗口定位（X11 / XWayland）

程序自身不依赖窗口管理器。以 override-redirect 方式定位（示例）：

```bash
abcclock 北京 &
sleep 1
WIN=$(xdotool search --class abcclock | head -1)
xdotool set_window --overrideredirect 1 "$WIN"
xdotool windowmove "$WIN" 1708 50
```

参考 `~/.local/bin/abcclock.sh`（本地启动脚本，含重试等待窗口的完整逻辑）。

## 安装（Arch Linux）

```bash
# 本地构建安装
make install DESTDIR=/tmp/abcclock-pkg   # 或直接 make && sudo make install

# 或使用 AUR 包（见 packaging/PKGBUILD）
```

## 布局

```
y=0    ┌──────────────┐
       │   表盘 192    │  DIAL_SIZE 192
y=192  ├──────────────┤
       │ FRI 星期五    │  WEEKDAY_H 28
y=220  ├──────────────┤
       │  2026-11-06  │  DATE_H 52
y=272  ├──────────────┤
       │ 北京   22:41  │
       │ 纽约   09:41  │  TIME_H 192（3 × 64）
       │ 洛杉   06:41  │
y=464  ├──────────────┤
       │ 庆阳  雾霾 5~13°C │
       │ 成都  小雨 8~15°C │  WEATHER_H 30 × 3
       │ 佛山  雷阵雨 12~20°C │
y=554  └──────────────┘   宽 192
```

## 许可

MIT License，见 [LICENSE](LICENSE)。