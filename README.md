# abcclock

一个 200px 宽的桌面悬浮时钟（Qt6；Wayland 原生 layer-shell，X11 自动回退），设计为始终显示在终端窗口上方：
圆角半透明灰底、模拟表盘、星期、日期、三个城市的时区时间，以及三条天气信息。


## 功能

- **窗口尺寸**：200×554，内容区 192px 宽，左右各留 4px 背景边
- **模拟表盘**（192×192）：时针/分针/秒针 + 12 刻度
- **星期行**：`FRI 星期五`（英文缩写大写 + 中文星期，随当天日期变化）
- **日期行**：年-月-日，大字号自动适应宽度
- **三城市时区**：北京 / 纽约 / 洛杉矶，城市名横排 + 当地时间（跟随系统时区换算）
- **表盘时区**：表盘、星期、日期按选定城市时区显示（默认北京），可在设置中切换
- **三行天气**：城市 / 天气现象 / 温度区间（城市可在设置中修改，wttr.in 查询）
- **整体圆角 12px + 半透明灰 `rgba(20,20,20,150)`**，四角透明，可透出桌面
- **鼠标交互**：
  - 左键点击窗口任意区域：立即隐藏，10 秒后自动显示
  - 右键点击窗口：弹出菜单，可选 `隐藏 10 秒` / `隐藏 1 分钟` / `隐藏 10 分钟` / `隐藏 30 分钟`
  - 托盘「显示」会取消待执行的自动显示计时；托盘「隐藏」不设定自动显示
- **不抢键盘焦点**：Wayland 下是 `wlr-layer-shell` 的 overlay surface
  （`keyboard-interactivity=none`、`activateOnShow=false`），协议层就不可能获得
  键盘焦点；X11/XWayland 回退使用 override-redirect + `WA_ShowWithoutActivating`
- 浮在所有平铺/全屏窗口之上（overlay 层，sway 全屏时也不被遮挡）
- **系统托盘**（StatusNotifierItem）：
  - 左键单击图标：显示 / 隐藏窗口（图标隐藏时变暗）
  - 右键菜单：`隐藏`（或 `显示`）/ `设置…` / `开机运行（已开启/已关闭）`（复选框）/ `退出`
    复选框的勾选与括号内文字均随 `~/.config/autostart/abcclock.desktop` 是否存在
    实时刷新，每次弹出菜单都会重新读取
- **开机运行**（右键菜单勾选项）：写入 / 删除
  `~/.config/autostart/abcclock.desktop`；勾选状态以该文件是否存在为准，
  重启后保留。若已安装 `abcclock.desktop`（`/usr/share/applications` 或
  `~/.local/share/applications`），自启项沿用它的 `Exec`，从而带上本地启动
  脚本里的窗口定位逻辑。
- **设置面板**（右键菜单 → `设置…`）：
  - 背景不透明度（20–100%）
  - 四区域字号：星期行、日期行、时区时间、天气行
    （日期行 / 时区时间可设为「自动」，自动填满可用宽度）
  - 表盘时间城市：表盘 / 星期 / 日期所用的时区（默认北京）
  - 三个时区城市：下拉选择常用城市（自带 IANA 时区，37 个候选）
  - 三个天气城市：自由输入，按 wttr.in 查询
  - 全部写入 `~/.config/abcclock/abcclock.conf`，重启后保留

> 托盘依赖桌面环境的 StatusNotifier 宿主（waybar 的 `tray` 模块、KDE/GNOME
> 的托盘均可）。应用启动时会依次探测当前 `DBUS_SESSION_BUS_ADDRESS`、
> `/run/user/$UID/bus` 以及 `/tmp/dbus-*`，选择带有
> `org.kde.StatusNotifierWatcher` 的那条总线（自启场景常遇到系统总线没有托盘
> 宿主），探测在 `QApplication` 创建前完成。
>
> Qt 只在 `QApplication` 构造时判定一次“有没有托盘宿主”，之后不会重判。登录
> 自启时 waybar 往往比本程序晚起来，表现为“有窗口、没有托盘图标”。因此托盘
> 初始化失败后每 2s 复查一次总线：一旦探测到 watcher 出现，就用正确的
> `DBUS_SESSION_BUS_ADDRESS` 重启自身一次（环境变量 `ABC_CLOCK_TRAY_REEXEC=1`
> 防止循环），新进程在宿主就绪的前提下初始化，托盘随即出现。

## 构建

依赖：Qt6（Widgets/DBus）+ libdbus-1 头文件（`dbus-1` 的 pkg-config）+ C++17 编译器。
Wayland 原生模式还需要 LayerShellQt（`layer-shell-qt`）的头文件与库；编译时检测不到
就自动回退 X11/XWayland，不会导致构建失败。

| 发行版 | 依赖 |
|---|---|
| Arch | `qt6-base` `qt6-wayland` `dbus` `libxcb` `layer-shell-qt` `pkgconf` `make` |
| Debian/Ubuntu | `qt6-base-dev` `qt6-wayland-dev` `libdbus-1-dev` `libxcb1-dev` `pkg-config` `build-essential`（可选 `liblayershellqtinterface-dev`） |

```bash
qmake6 abcclock.pro && make -j$(nproc)
```

产物为 `abcclock`。运行：

```bash
./abcclock 北京
```

### 窗口定位

程序**自行定位**，不依赖窗口管理器或任何外部工具。

Wayland（wlroots 系合成器，如 sway）优先：

- 窗口是 `zwlr_layer_surface_v1`（LayerShellQt 按窗口挂载），layer = `overlay`，
  anchor = `top | right`，margin = `top=50, right=20`，由合成器贴右上角摆放
  （1920 宽屏幕 → 窗口右缘 1900）；
- `exclusive_zone = -1`：不挤占其它窗口的可用区；
- `keyboard_interactivity = none` + `activateOnShow = false`：弹出、重显示、
  点击都不会请求键盘焦点，正在输入的程序光标不动；
- 设置对话框是普通 `xdg_toplevel`（layer-shell 只挂主窗口），获得焦点可正常输入。

X11 / XWayland 回退：

- 窗口标志为 `FramelessWindowHint | WindowStaysOnTopHint | X11BypassWindowManagerHint`，
  即 override-redirect，WM 不会接管，全屏终端也盖不住；
- `WA_ShowWithoutActivating` + `WA_X11DoNotAcceptFocus` + `NoFocus`，映射时不抢焦点；
- 启动时按 `POS_RIGHT_MARGIN`(20px) / `POS_TOP`(100px) 贴主屏右上角摆放。

平台选择规则：

- 显式设置 `QT_QPA_PLATFORM` 时不干预；
- 编译时带 layer-shell 支持且 `WAYLAND_DISPLAY` 的 socket 存在 → 原生 Wayland；
- 否则用 `xcb_connect` 试连当前 `DISPLAY`；连不上先试 Xwayland 常用的 `:1`，
  再失败则在 stderr 说明原因并返回 1（Qt 自己遇到这种情况会 `SIGABRT`，不打印原因）。

## macOS（Apple Silicon / Intel）

同一份源码（Qt6）：窗口用 `Tool` + `NSFloatingWindowLevel`（不进 Dock、跨 Space 悬浮），
自启动改用 `~/Library/LaunchAgents/com.abcabc0330.abcclock.plist`（菜单里的「开机运行」会自动写/删它），
Linux 专用的 D-Bus 托盘探测在 macOS 编译时整段剔除。

```bash
# 依赖：Xcode Command Line Tools + Qt 6.12（aqtinstall 装到 ~/Qt，免 sudo）
python3 -m pip install --user aqtinstall
python3 -m aqt install-qt mac desktop 6.12.0 clang_64 -O "$HOME/Qt"

QTDIR=~/Qt/6.12.0/macos ./packaging/macos/build-dmg.sh   # 产出 abcclock-macos.dmg
```

dmg 内含 `abcclock.app` 与 `/Applications` 快捷方式，拖进去即可。构建为 ad-hoc 签名，
**自用**直接打开；若从网上下载时被 Gatekeeper 拦，执行
`xattr -d com.apple.quarantine /Applications/abcclock.app` 或右键「打开」。

CI：`.github/workflows/macos.yml` 在 `macos-latest` 上用 universal Qt 构建
`arm64 + x86_64` 双架构 app，打 tag 时自动附加 dmg 到 GitHub Release。

## 安装

### Arch Linux

```bash
make install DESTDIR=/tmp/abcclock-pkg   # 或直接 make && sudo make install
# 或 AUR 包（packaging/PKGBUILD）
```

### Debian / Ubuntu（deb 打包）

```bash
sudo apt build-dep .            # 或手动安装控制文件中 Build-Depends 各项
dpkg-buildpackage -us -uc -b    # 产物在上一级目录：abcclock_3.1.0-1_amd64.deb
sudo apt install ../abcclock_*.deb
```

运行时依赖自动生成（`libqt6widgets6`/`libqt6gui6`/`libqt6dbus6`/`libdbus-1-3` 等），
另显式依赖 `curl`（天气查询）。

## 布局

```
y=0    ┌──────────────────┐   窗口宽 200（内容 192 + 左右各 4px 背景边）
       │    表盘 192     │  DIAL_SIZE 192
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
y=554  └──────────────────┘   内容宽 192
```

## 许可

MIT License，见 [LICENSE](LICENSE)。