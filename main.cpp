// abcclock：模拟表盘 + 星期 + 日期 + 三城市时区 + 天气（Qt5，200px 宽悬浮窗）
// 布局（从上到下）：
//   1. tzclock 表盘（192x192，含 12 时点数字刻度，同心）
//   2. 年-月-日（日期行，宽=表盘直径 192，大字体）
//   3. 城市竖排（2 汉字）+ 时:分:秒（大字体，缩小间距）
//   4. 底部 3 城市天气（庆阳/成都/佛山，各 1 行：城市名 + 天气 + 温度）
// 总宽 200 = 内容宽(192，表盘/日期/时区/天气均按此排布) + 左右各 4px 背景边。
//
// 样式来源：
//   - 表盘：tzclock（圆盘 #141414、刻度 #000000、时分秒指针 #6e6e6e）
//   - 时间：hoverclockqt（白色文字）
//   - 城市竖排：2 个汉字垂直排列（如"北京"）
//   - 天气：wttr.in（城市名 + 天气 + 温度）
#include <QApplication>
#include <QScreen>
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QDateTime>
#include <QTime>
#include <QFont>
#include <QFontMetrics>
#include <QString>
#include <QProcess>
#include <QtMath>
#include <QRegularExpression>
#include <QTimeZone>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QSettings>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSlider>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QUrl>
#include <QCursor>
#include <QStandardPaths>
#include <QDir>
#include <QMouseEvent>
#include <QFile>
#include <QFileInfo>
#include <cstdio>
#include "platform.h"
#if defined(Q_OS_LINUX)
#include <dbus/dbus.h>
#include <xcb/xcb.h>
#include <unistd.h>
#elif defined(Q_OS_MACOS)
#include <unistd.h>
#endif

// 尺寸（像素）
static const int DIAL_SIZE = 192;       // 表盘直径（复刻 tzclock）
static const int CITY_W    = 32;        // 城市竖排宽度（2汉字）
static const int WEEKDAY_H = 28;       // 星期行高（表盘与日期之间：SUN 星期五）
static const int DATE_H    = 52;        // 年-月-日行高（需求1增大，容纳大字号）
static const int TZ_N      = 3;         // 时区城市数（北京/纽约/洛杉矶）
static const int TZ_ROW_H  = 64;        // 每时区城市行高（时间字体更大）
static const int TIME_H    = TZ_ROW_H * TZ_N;  // 时间区总高（3 城市时区）
static const int WEATHER_H = 30;        // 每行天气高（需求4：3 行）
static const int WEATHER_N = 3;         // 3 个城市天气
static const int PAD_H     = 4;                     // 左右各留 4px 背景边（整体宽 +8）
static const int WIN_W     = DIAL_SIZE + PAD_H * 2;  // 总宽 200 = 表盘宽 192 + 左右各 4
static const int POS_RIGHT_MARGIN = 20;              // 默认距屏幕右缘
static const int POS_TOP          = 50;              // 默认距屏幕顶
static const int WIN_H     = DIAL_SIZE + WEEKDAY_H + DATE_H + TIME_H + WEATHER_H * WEATHER_N;
static const int CORNER_R   = 12;       // 整窗圆角半径

// 颜色（复刻 tzclock 实测样式色）
static const QColor DIAL_BG   (0x14, 0x14, 0x14);  // 表盘背景 #141414
static const QColor DIAL_RING (0x00, 0x00, 0x00);  // 刻度 #000000
static const QColor HAND_COL  (0x6e, 0x6e, 0x6e);  // 指针 #6e6e6e
static const QColor SEC_COL   (0x8a, 0x8a, 0x8a);  // 秒针
static const QColor TEXT_COL  (0xff, 0xff, 0xff);  // 文字白色（hoverclockqt 样式）
static const QColor NUM_COL   (0x9a, 0x9a, 0x9a);  // 时点数字
static const QColor MUTED_COL (0x8a, 0x8a, 0x8a);  // 天气文字灰色

// 底部天气城市：拼音名（wttr.in 查询）+ 显示中文名
// 时区城市候选（设置面板下拉框）：显示名 + IANA 时区
struct CityTz {
    const char* name;
    const char* tz;
};
static const CityTz kCityTz[] = {
    {"北京",       "Asia/Shanghai"},
    {"上海",       "Asia/Shanghai"},
    {"香港",       "Asia/Hong_Kong"},
    {"台北",       "Asia/Taipei"},
    {"东京",       "Asia/Tokyo"},
    {"首尔",       "Asia/Seoul"},
    {"新加坡",     "Asia/Singapore"},
    {"曼谷",       "Asia/Bangkok"},
    {"雅加达",     "Asia/Jakarta"},
    {"孟买",       "Asia/Kolkata"},
    {"德里",       "Asia/Kolkata"},
    {"迪拜",       "Asia/Dubai"},
    {"莫斯科",     "Europe/Moscow"},
    {"伊斯坦布尔", "Europe/Istanbul"},
    {"柏林",       "Europe/Berlin"},
    {"巴黎",       "Europe/Paris"},
    {"伦敦",       "Europe/London"},
    {"里斯本",     "Europe/Lisbon"},
    {"开罗",       "Africa/Cairo"},
    {"约翰内斯堡", "Africa/Johannesburg"},
    {"拉各斯",     "Africa/Lagos"},
    {"纽约",       "America/New_York"},
    {"芝加哥",     "America/Chicago"},
    {"丹佛",       "America/Denver"},
    {"洛杉矶",     "America/Los_Angeles"},
    {"温哥华",     "America/Vancouver"},
    {"多伦多",     "America/Toronto"},
    {"墨西哥城",   "America/Mexico_City"},
    {"圣保罗",     "America/Sao_Paulo"},
    {"布宜诺斯艾利斯", "America/Argentina/Buenos_Aires"},
    {"悉尼",       "Australia/Sydney"},
    {"墨尔本",     "Australia/Melbourne"},
    {"珀斯",       "Australia/Perth"},
    {"奥克兰",     "Pacific/Auckland"},
    {"檀香山",     "Pacific/Honolulu"},
    {"安克雷奇",   "America/Anchorage"},
    {"UTC",        "UTC"},
};

#if defined(Q_OS_LINUX)
static QString findTrayBus();   // 定义见文件末尾：探测带 StatusNotifierWatcher 的总线
#endif

class HoverTzClock : public QWidget {
public:
    explicit HoverTzClock(const QString& city, QWidget* parent = nullptr)
        : QWidget(parent), m_city(city)
    {
        setFixedSize(WIN_W, WIN_H);
        // 无边框 + 置顶 + 背景透明（半透明灰色，不遮挡桌面）
#if defined(Q_OS_MACOS)
        // macOS：Tool 窗口不进 Dock、不抢前台；层级由 platformSetupWindow 提到 floating
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
#else
        // X11：X11BypassWindowManagerHint => override-redirect，窗口不被 WM 管理，
        // 因而可以自行定位、也不会被全屏终端盖住
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                       | Qt::X11BypassWindowManagerHint);
#endif
        setAttribute(Qt::WA_TranslucentBackground);
        // 自定位：默认贴屏幕右上（右缘 20px、顶部 50px），不再依赖外部 xdotool
        placeAtDefaultPosition();
        // 每秒更新（秒针每秒动）
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() { update(); });
        timer->start(1000);
        // 持久化设置（背景不透明度 + 天气城市）
        loadSettings();
        // 系统托盘：左键切换显示，右键菜单（切换/设置/退出）
        setupTray();
        // 天气：启动时 + 每 10 分钟刷新
        fetchWeather();
        QTimer* wtimer = new QTimer(this);
        connect(wtimer, &QTimer::timeout, this, [this]() { fetchWeather(); });
        wtimer->start(10 * 60 * 1000);
    }

protected:
    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        platformSetupWindow(this);
#if defined(Q_OS_MACOS)
        // macOS 首次 show 时系统会按菜单栏把窗口上移；层级设置完再定位一次才是最终位置
        if (!m_macPlaced) {
            m_macPlaced = true;
            // 首次 show 期间 AppKit 会把窗口吸到菜单栏下（y 变成 30）；
            // 延到下一轮事件循环定位才是最终位置
            QTimer::singleShot(0, this, [this]() { placeAtDefaultPosition(); });
        }
#endif
    }

    // 左键点窗口任意位置：隐藏 10 秒后自动显示
    // 右键：弹出隐藏时长菜单
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            hideFor(10 * 1000);
        } else if (e->button() == Qt::RightButton) {
            showHideMenu(e->globalPos());
        }
        e->accept();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        // 背景半透明灰色 + 整体圆角（rgba(20,20,20,150)，四角透明）
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 20, 20, m_alpha));
        p.drawRoundedRect(QRectF(rect()), CORNER_R, CORNER_R);

        // 内容整体右移 PAD_H：可视宽度仍为 192，左右各多 2px 背景
        p.translate(PAD_H, 0);

        // 表盘时间：按设置中选定的城市时区（默认北京），星期/日期同步
        const QDateTime nowSystem = QDateTime::currentDateTime();
        QTimeZone dialZone(m_dialCity.zone.toUtf8());
        QDateTime now = dialZone.isValid() ? nowSystem.toTimeZone(dialZone) : nowSystem;
        QTime t = now.time();

        // ---- 上方：tzclock 表盘（192x192，居左，同心）----
        drawDial(&p, 0, 0, DIAL_SIZE, t);

        // ---- 表盘与日期之间：星期行（SUN 星期五）----
        drawWeekday(&p, 0, DIAL_SIZE, DIAL_SIZE, WEEKDAY_H, now);

        // ---- 中部：年-月-日（宽=表盘直径，大字体）----
        drawDate(&p, 0, DIAL_SIZE + WEEKDAY_H, DIAL_SIZE, DATE_H, now);

        // ---- 下方：3 城市时区（北京/纽约/洛杉矶），各城市横排 + 该时区时间 ----
        int timeY = DIAL_SIZE + WEEKDAY_H + DATE_H;
        for (int i = 0; i < TZ_N; ++i) {
            int ry = timeY + i * TZ_ROW_H;
            // 该城市时区当前时间
            QTimeZone tz(m_tzCity[i].zone.toUtf8());
            QDateTime local = tz.isValid() ? now.toTimeZone(tz) : now;
            drawCity(&p, 0, ry, CITY_W, TZ_ROW_H, m_tzCity[i].name);
            drawTime(&p, CITY_W, ry, DIAL_SIZE - CITY_W, TZ_ROW_H, local);
        }

        // ---- 底部：3 城市天气 ----
        int wy = timeY + TIME_H;
        for (int i = 0; i < WEATHER_N; ++i) {
            drawWeather(&p, 0, wy + i * WEATHER_H, DIAL_SIZE, WEATHER_H, i);
        }
    }

private:
    // 贴屏幕右上：右缘留 POS_RIGHT_MARGIN、顶部留 POS_TOP
    void placeAtDefaultPosition() {
        if (QScreen* scr = QGuiApplication::primaryScreen()) {
            const QRect g = scr->geometry();
            move(g.right() + 1 - WIN_W - POS_RIGHT_MARGIN, g.top() + POS_TOP);
        }
    }

    // 表盘：圆盘 + 12 时点数字刻度 + 时分秒指针（复刻 tzclock getHandPositions 角度公式）
    void drawDial(QPainter* p, int x, int y, int size, const QTime& t) {
        int cx = x + size / 2, cy = y + size / 2;
        int r = size / 2;

        // 表盘背景圆
        p->setPen(Qt::NoPen);
        p->setBrush(DIAL_BG);
        p->drawEllipse(cx - r, cy - r, size, size);
        // 外圈
        p->setPen(QPen(DIAL_RING, 2));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(cx - r + 1, cy - r + 1, size - 2, size - 2);

        // 12 个时点刻度（短线）
        p->setPen(QPen(DIAL_RING, 2));
        for (int i = 0; i < 12; ++i) {
            double ang = qDegreesToRadians(i * 30.0);
            int x1 = cx + (int)((r - 12) * std::sin(ang));
            int y1 = cy - (int)((r - 12) * std::cos(ang));
            int x2 = cx + (int)((r - 5) * std::sin(ang));
            int y2 = cy - (int)((r - 5) * std::cos(ang));
            p->drawLine(x1, y1, x2, y2);
        }

        // 12 个时点数字（1-12，在刻度内侧，精确居中 = 同心）
        p->setPen(NUM_COL);
        QFont nf = p->font();
        nf.setPointSize(9);
        nf.setBold(true);
        p->setFont(nf);
        QFontMetrics fm(nf);
        for (int i = 1; i <= 12; ++i) {
            double ang = qDegreesToRadians(i * 30.0);
            // 数字中心位置（在 r-24 半径处）
            int dnx = cx + (int)((r - 24) * std::sin(ang));
            int dny = cy - (int)((r - 24) * std::cos(ang));
            // 用字体度量精确居中（左上角 = 中心 - 半宽/半高）
            QString num = QString::number(i);
            int tx = dnx - fm.horizontalAdvance(num) / 2;
            int ty = dny - fm.height() / 2 + fm.ascent();
            p->drawText(tx, ty, num);
        }

        // 指针角度（tzclock 公式：单位 0-1200，x0.3=角度）
        double secAng = (t.second() * 20) * 0.3;
        double minAng = (t.minute() * 20 + t.second() / 3) * 0.3;
        double hourAng = (t.hour() * 100 + (t.minute() * 60 + t.second()) / 36) * 0.3;

        // 时针（短粗）
        drawHand(p, cx, cy, hourAng, r * 0.55, 5, HAND_COL);
        // 分针（中长中粗）
        drawHand(p, cx, cy, minAng, r * 0.75, 3, HAND_COL);
        // 秒针（细长）
        drawHand(p, cx, cy, secAng, r * 0.85, 1, SEC_COL);
        // 中心点
        p->setPen(Qt::NoPen);
        p->setBrush(HAND_COL);
        p->drawEllipse(cx - 3, cy - 3, 6, 6);
    }

    void drawHand(QPainter* p, int cx, int cy, double angleDeg, int len, int width, const QColor& col) {
        double rad = qDegreesToRadians(angleDeg);
        int ex = cx + (int)(len * std::sin(rad));
        int ey = cy - (int)(len * std::cos(rad));
        p->setPen(QPen(col, width));
        p->drawLine(cx, cy, ex, ey);
    }

    // 年-月-日（日期行，宽=表盘直径 192，大字体——字号自动增大到填满表盘宽）
    // 星期行：SUN 星期五（英文缩写大写 + 中文星期）
    void drawWeekday(QPainter* p, int x, int y, int w, int h, const QDateTime& now) {
        QDate d = now.date();
        QString en = QLocale(QLocale::English, QLocale::UnitedStates)
                         .toString(d, "ddd").toUpper();
        static const char* kZh[7] = {
            "星期一", "星期二", "星期三", "星期四", "星期五", "星期六", "星期日"
        };
        QString zh = QString::fromUtf8(kZh[d.dayOfWeek() - 1]);
        QFont f = p->font();
        f.setPointSize(m_fsWeekday);
        f.setBold(true);
        p->setFont(f);
        p->setPen(TEXT_COL);
        p->drawText(QRect(x, y, w, h), Qt::AlignCenter, en + "  " + zh);
    }

    void drawDate(QPainter* p, int x, int y, int w, int h, const QDateTime& now) {
        p->setPen(TEXT_COL);
        QString dateStr = now.toString("yyyy-MM-dd");
        // 自动增大字号直到文字宽≈表盘宽(192)，填满宽度
        QFont f = p->font();
        f.setBold(true);
        if (m_fsDate > 0) {
            f.setPointSize(m_fsDate);
        } else {
            for (int pt = 10; pt <= 40; ++pt) {
                f.setPointSize(pt);
                QFontMetrics fm(f);
                if (fm.horizontalAdvance(dateStr) >= w - 4) {
                    break;
                }
            }
        }
        p->setFont(f);
        p->drawText(QRect(x, y, w, h), Qt::AlignCenter, dateStr);
    }

    // 城市名横排：只显示前 2 字（3 字城市只保留前 2 字）
    void drawCity(QPainter* p, int x, int y, int w, int h, const QString& city) {
        p->setPen(TEXT_COL);
        QFont f = p->font();
        f.setPointSize(11);
        f.setBold(true);
        p->setFont(f);
        // 横排，只显示前 2 字（洛杉矶→洛杉）
        p->drawText(QRect(x, y, w, h), Qt::AlignCenter, city.left(2));
    }

    // 时间：时:分:秒（大字体，自动增大填满时间区宽）
    void drawTime(QPainter* p, int x, int y, int w, int h, const QDateTime& now) {
        p->setPen(TEXT_COL);
        QString timeStr = now.toString("hh:mm:ss");
        // 自动增大字号直到填满宽（但不超过行高，避免溢出）
        QFont f = p->font();
        f.setBold(true);
        if (m_fsTz > 0) {
            f.setPointSize(m_fsTz);
        } else {
            int maxPt = 24;  // 初始 24pt，受行高 56 限制约 38pt
            for (int pt = 16; pt <= 40; ++pt) {
                f.setPointSize(pt);
                QFontMetrics fm(f);
                if (fm.height() > h - 6 || fm.horizontalAdvance(timeStr) >= w - 2) {
                    break;
                }
                maxPt = pt;
            }
            f.setPointSize(maxPt);
        }
        p->setFont(f);
        p->drawText(QRect(x, y, w, h), Qt::AlignCenter, timeStr);
    }

    // 底部城市天气：城市名(左) + 天气情况(4字宽) + 温度区间(右) = 表盘宽 192
    void drawWeather(QPainter* p, int x, int y, int w, int h, int idx) {
        // 城市名（左，横排小字）
        p->setPen(TEXT_COL);
        QFont cf = p->font();
        cf.setPointSize(m_fsWeather);
        cf.setBold(true);
        p->setFont(cf);
        p->drawText(QRect(x, y, CITY_W, h), Qt::AlignCenter, m_wcityName[idx]);

        // 拆分 "天气描述 最低~最高°C"（天气描述无空格，故按第一个空格拆）
        QString wStr = m_weather[idx].isEmpty() ? "…" : m_weather[idx];
        QString desc, temp;
        int sp = wStr.indexOf(' ');
        if (sp > 0) {
            desc = wStr.left(sp);
            temp = wStr.mid(sp + 1).trimmed();
        } else {
            desc = wStr;
        }

        // 天气情况：固定 4 字宽区域，居中
        QFont wf = p->font();
        wf.setPointSize(m_fsWeather + 2);
        wf.setBold(false);
        p->setFont(wf);
        QFontMetrics wm(wf);
        int descW = wm.horizontalAdvance(QString::fromUtf8("国")) * 4;
        int wx = x + CITY_W;
        int tw = wm.horizontalAdvance(desc);
        p->drawText(QRect(wx + (descW - tw) / 2, y, tw, h),
                    Qt::AlignVCenter | Qt::AlignLeft, desc);

        // 温度区间：剩余宽度右对齐（清晰大字号）
        if (!temp.isEmpty()) {
            QFont tf = wf;
            tf.setPointSize(m_fsWeather + 3);
            p->setFont(tf);
            p->drawText(QRect(wx + descW, y, w - CITY_W - descW, h),
                        Qt::AlignVCenter | Qt::AlignRight, temp);
        }
    }

    // 英文天气 → 中文翻译（wttr.in weatherDesc 常见值）
    static QString translateWeather(const QString& en) {
        static const QMap<QString, QString> m = {
            {"Sunny", "晴"}, {"Clear", "晴"}, {"Partly cloudy", "多云"},
            {"Cloudy", "多云"}, {"Overcast", "阴"}, {"Mist", "雾"},
            {"Fog", "雾"}, {"Patchy rain nearby", "小雨"},
            {"Patchy rain possible", "小雨"}, {"Light rain", "小雨"},
            {"Light drizzle", "毛毛雨"}, {"Moderate rain", "中雨"},
            {"Heavy rain", "大雨"}, {"Light rain shower", "小阵雨"},
            {"Moderate rain at times", "中雨"}, {"Heavy rain at times", "大雨"},
            {"Light snow", "小雪"}, {"Moderate snow", "中雪"},
            {"Heavy snow", "大雪"}, {"Blizzard", "暴雪"},
            {"Thundery outbreaks possible", "雷阵雨"},
            {"Thunderstorm", "雷暴"}, {"Smoky haze", "雾霾"},
            {"Smoke", "烟霾"}, {"Haze", "霾"}, {"Dust", "沙尘"},
            {"Patchy snow possible", "小雪"}, {"Freezing fog", "冻雾"},
            {"Light freezing rain", "冻雨"},
        };
        // 精确匹配 + 前缀匹配（去掉尾部空格）
        QString e = en.trimmed();
        if (m.contains(e)) return m[e];
        // 尝试前缀匹配（如 "Patchy rain nearby" 已精确，其他含空格）
        for (auto it = m.begin(); it != m.end(); ++it) {
            if (e.startsWith(it.key())) return it.value();
        }
        return e;  // 未匹配：原样显示
    }

    // 获取天气（wttr.in j1：中文天气 + 温度区间 min~max°C）
    void fetchWeather() {
        for (int i = 0; i < WEATHER_N; ++i) {
            QString q = m_wcityName[i];
            QProcess* proc = new QProcess(this);
            connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc, i](int, QProcess::ExitStatus) {
                QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
                if (!out.isEmpty()) {
                    // j1 JSON: weather[0].maxtempC/mintempC + current_condition[0].weatherDesc
                    QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8());
                    QJsonObject root = doc.object();
                    QJsonArray weather = root.value("weather").toArray();
                    if (!weather.isEmpty()) {
                        QJsonObject today = weather.at(0).toObject();
                        QString maxT = today.value("maxtempC").toString();
                        QString minT = today.value("mintempC").toString();
                        // 天气描述（英文→中文）
                        QString descEn;
                        QJsonArray cur = root.value("current_condition").toArray();
                        if (!cur.isEmpty()) {
                            QJsonArray desc = cur.at(0).toObject().value("weatherDesc").toArray();
                            if (!desc.isEmpty()) descEn = desc.at(0).toObject().value("value").toString();
                        }
                        QString descZh = translateWeather(descEn);
                        // "中文天气 最低~最高°C"
                        m_weather[i] = descZh + " " + minT + "~" + maxT + "°C";
                        update();
                    }
                }
                proc->deleteLater();
            });
            QStringList args;
            args << "-s" << "--max-time" << "10" << "https://wttr.in/" + QString::fromLatin1(QUrl::toPercentEncoding(q)) + "?format=j1";
            proc->start("curl", args);
        }
    }

    // ---- 系统托盘 ----
    // Qt 在 QApplication 构造时就决定了托盘后端是否可用，之后即使 waybar 的
    // StatusNotifierWatcher 才注册，isSystemTrayAvailable() 也不会变成 true。
    // 登录自启时正好是这种顺序（sway 先拉起 app，waybar 稍后才起来），
    // 所以这里用 libdbus 自己盯着总线：一等出现 watcher 就重启自己一次，
    // 让新的 QApplication 在“有托盘宿主”的前提下初始化。
    void retryTray() {
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            setupTray();
            return;
        }
#if defined(Q_OS_LINUX)
        if (qEnvironmentVariableIsSet("ABC_CLOCK_TRAY_REEXEC"))
            return;   // 已经重启过一次，不再循环
        const QString bus = findTrayBus();
        if (bus.isEmpty())
            return;   // 还没有任何总线带 watcher，继续等
        qputenv("ABC_CLOCK_TRAY_REEXEC", "1");
        if (bus != qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"))
            qputenv("DBUS_SESSION_BUS_ADDRESS", bus.toUtf8());
        QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                QCoreApplication::arguments().mid(1));
        QCoreApplication::quit();
#endif
    }

    void setupTray() {
        if (m_tray) return;
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            // 登录自启时托盘宿主（waybar 的 StatusNotifierWatcher）往往还没注册，
            // 一次性判断会让自启实例永远没有托盘图标；每 2s 复查一次。
            if (!m_trayRetry) {
                m_trayRetry = new QTimer(this);
                m_trayRetry->setInterval(2000);
                connect(m_trayRetry, &QTimer::timeout, this, [this]() { retryTray(); });
                m_trayRetry->start();
            }
            return;
        }
        if (m_trayRetry) {
            m_trayRetry->stop();
            m_trayRetry->deleteLater();
            m_trayRetry = nullptr;
        }

        m_menu = new QMenu(this);
        m_toggleAct = m_menu->addAction(QString::fromUtf8("隐藏"));
        m_settingsAct = m_menu->addAction(QString::fromUtf8("设置…"));
        m_autostartAct = m_menu->addAction(QString::fromUtf8("开机运行"));
        m_autostartAct->setCheckable(true);
        m_exitAct = m_menu->addAction(QString::fromUtf8("退出"));

        m_tray = new QSystemTrayIcon(makeTrayIcon(true), this);
        m_tray->setToolTip(QStringLiteral("abcclock"));
        m_tray->setContextMenu(m_menu);

        connect(m_toggleAct, &QAction::triggered, this, [this]() { toggleVisible(); });
        connect(m_settingsAct, &QAction::triggered, this, [this]() { openSettings(); });
        connect(m_exitAct, &QAction::triggered, qApp, &QCoreApplication::quit);
        // 每次弹出菜单都用文件系统状态刷新勾选与文字——外部增删自启项也能反映
        auto refreshAutostart = [this]() {
            const bool on = isAutostartEnabled();
            m_autostartAct->setChecked(on);
            m_autostartAct->setText(on ? QString::fromUtf8("开机运行（已开启）")
                                       : QString::fromUtf8("开机运行（已关闭）"));
        };
        refreshAutostart();
        connect(m_menu, &QMenu::aboutToShow, this, refreshAutostart);
        connect(m_autostartAct, &QAction::triggered, this, [this, refreshAutostart](bool on) {
            if (setAutostartEnabled(on)) refreshAutostart();
            else m_autostartAct->setChecked(!on);
        });
        // 左键单击（Trigger）切换显示/隐藏；右键由 setContextMenu 弹出菜单
        connect(m_tray, &QSystemTrayIcon::activated, this,
                [this](QSystemTrayIcon::ActivationReason r) {
                    if (r == QSystemTrayIcon::Trigger) toggleVisible();
                    else if (r == QSystemTrayIcon::Context && m_menu)
                        m_menu->popup(QCursor::pos());
                });
        m_tray->show();
    }

    // ---- 开机自启 ----
#if defined(Q_OS_MACOS)
    // macOS：~/Library/LaunchAgents/com.abcabc0330.abcclock.plist（RunAtLoad）
    // 状态同样以文件系统为准，外部增删可即时反映
    static QString autostartPath() {
        return QDir::homePath()
               + QStringLiteral("/Library/LaunchAgents/com.abcabc0330.abcclock.plist");
    }

    static bool isAutostartEnabled() { return QFile::exists(autostartPath()); }

    bool setAutostartEnabled(bool on) {
        const QString path = autostartPath();
        const QString label = QStringLiteral("com.abcabc0330.abcclock");
        const QString domain = QStringLiteral("gui/%1").arg(getuid());
        QProcess lc;

        if (!on) {
            lc.start(QStringLiteral("launchctl"),
                     {QStringLiteral("bootout"), domain + QLatin1Char('/') + label});
            lc.waitForFinished(3000);
            return QFile::remove(path) || !QFile::exists(path);
        }

        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        QStringList args{QCoreApplication::applicationFilePath()};
        if (!m_city.isEmpty()) args << m_city;
        QString arr;
        for (const QString& a : args)
            arr += QStringLiteral("    <string>%1</string>\n").arg(a.toHtmlEscaped());
        const QString content = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n<dict>\n"
            "  <key>Label</key><string>%1</string>\n"
            "  <key>ProgramArguments</key>\n  <array>\n%2  </array>\n"
            "  <key>RunAtLoad</key><true/>\n"
            "  <key>ProcessType</key><string>Interactive</string>\n"
            "</dict>\n</plist>\n").arg(label, arr);
        const bool ok = f.write(content.toUtf8()) > 0;
        f.close();
        if (!ok) { f.remove(); return false; }

        lc.start(QStringLiteral("launchctl"), {QStringLiteral("bootstrap"), domain, path});
        lc.waitForFinished(3000);
        return true;
    }
#else
    // Linux：XDG autostart（~/.config/autostart/abcclock.desktop）
    static QString autostartPath() {
        return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
               + QStringLiteral("/autostart/abcclock.desktop");
    }

    static bool isAutostartEnabled() { return QFile::exists(autostartPath()); }

    QString autostartExecLine() const {
        // 优先复用已安装的 .desktop 的 Exec（它可能带定位脚本、环境变量等）
        QString cmd;
        const QString desktop = QStandardPaths::locate(
            QStandardPaths::ApplicationsLocation, QStringLiteral("abcclock.desktop"));
        if (!desktop.isEmpty()) {
            QSettings ds(desktop, QSettings::IniFormat);
            cmd = ds.value(QStringLiteral("Desktop Entry/Exec")).toString().trimmed();
        }
        if (cmd.isEmpty()) {
            cmd = QCoreApplication::applicationFilePath();
            if (!m_city.isEmpty()) cmd += QLatin1Char(' ') + m_city;
        }
        if (cmd.contains(QLatin1Char(' '))) cmd = QLatin1Char('"') + cmd + QLatin1Char('"');
        return cmd;
    }

    bool setAutostartEnabled(bool on) {
        const QString path = autostartPath();
        if (!on) return QFile::remove(path) || !QFile::exists(path);

        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const QString content =
            QStringLiteral(
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=abcclock\n"
                "Comment=Hover desktop clock: dial, weekday, date, 3 timezones, weather\n"
                "Exec=%1\n"
                "Icon=clock\n"
                "Terminal=false\n"
                "X-GNOME-Autostart-enabled=true\n")
                .arg(autostartExecLine());
        const bool ok = f.write(content.toUtf8()) > 0;
        f.close();
        if (!ok) f.remove();
        return ok;
    }
#endif

    // 隐藏 ms 毫秒后自动重新显示；重复调用会重置计时
    void hideFor(int ms) {
        hide();
        updateTray();
        if (!m_showTimer) {
            m_showTimer = new QTimer(this);
            m_showTimer->setSingleShot(true);
            connect(m_showTimer, &QTimer::timeout, this, [this]() {
                show();
                updateTray();
            });
        }
        m_showTimer->start(ms);
    }

    // 右键菜单：按固定时长隐藏
    void showHideMenu(const QPoint& globalPos) {
        if (!m_hideMenu) {
            m_hideMenu = new QMenu(this);
            struct Item { const char* label; int ms; };
            static const Item items[] = {
                { "隐藏 10 秒",   10 * 1000 },
                { "隐藏 1 分钟",  60 * 1000 },
                { "隐藏 10 分钟", 10 * 60 * 1000 },
                { "隐藏 30 分钟", 30 * 60 * 1000 },
            };
            for (const Item& it : items) {
                QAction* a = m_hideMenu->addAction(QString::fromUtf8(it.label));
                const int ms = it.ms;
                connect(a, &QAction::triggered, this, [this, ms]() { hideFor(ms); });
            }
        }
        m_hideMenu->popup(globalPos);
    }

    void toggleVisible() {
        if (m_showTimer) m_showTimer->stop();
        if (isVisible()) hide();
        else show();
        updateTray();
    }

    void updateTray() {
        if (!m_tray) return;
        m_tray->setIcon(makeTrayIcon(isVisible()));
        if (m_toggleAct)
            m_toggleAct->setText(isVisible() ? QString::fromUtf8("隐藏")
                                             : QString::fromUtf8("显示"));
    }

    // 托盘图标：小表盘 + 当前时分针；隐藏时整体压暗（同 hoverclock 行为）
    static QIcon makeTrayIcon(bool active) {
        const int S = 64;
        QPixmap pm(S, S);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        if (!active) p.setOpacity(0.4);

        p.setPen(QPen(QColor(0x00, 0x00, 0x00), 2));
        p.setBrush(DIAL_BG);
        p.drawEllipse(3, 3, S - 6, S - 6);

        QTime t = QTime::currentTime();
        double cx = S / 2.0, cy = S / 2.0;
        auto hand = [&](double deg, double len, const QColor& c, double width) {
            double rad = qDegreesToRadians(deg);
            p.setPen(QPen(c, width, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(cx, cy),
                       QPointF(cx + len * std::sin(rad), cy - len * std::cos(rad)));
        };
        hand((t.hour() % 12 + t.minute() / 60.0) * 30.0, 15, HAND_COL, 3.5);
        hand(t.minute() * 6.0, 20, HAND_COL, 2.5);
        hand(t.second() * 6.0, 22, SEC_COL, 1.5);
        p.setPen(Qt::NoPen);
        p.setBrush(TEXT_COL);
        p.drawEllipse(QPointF(cx, cy), 2.5, 2.5);
        return QIcon(pm);
    }

    // ---- 设置（不透明度 / 4 区域字号 / 时区城市 / 天气城市；QSettings 持久化）----
    void openSettings() {
        QDialog dlg(this);
        dlg.setWindowTitle(QString::fromUtf8("abcclock 设置"));
        QFormLayout* form = new QFormLayout(&dlg);

        QSlider* alpha = new QSlider(Qt::Horizontal);
        alpha->setRange(20, 100);
        alpha->setValue(qBound(20, m_alpha * 100 / 255, 100));
        alpha->setMinimumWidth(220);
        form->addRow(QString::fromUtf8("背景不透明度"), alpha);

        auto mkSpin = [&](int value, int lo, int hi) {
            QSpinBox* sb = new QSpinBox;
            sb->setRange(lo, hi);
            sb->setValue(value);
            if (lo == 0) {
                sb->setSpecialValueText(QString::fromUtf8("自动"));
                sb->setSuffix(QStringLiteral(" pt"));
            } else {
                sb->setSuffix(QStringLiteral(" pt"));
            }
            return sb;
        };
        QSpinBox* fsW = mkSpin(m_fsWeekday, 6, 40);
        QSpinBox* fsD = mkSpin(m_fsDate, 0, 48);
        QSpinBox* fsT = mkSpin(m_fsTz, 0, 48);
        QSpinBox* fsWe = mkSpin(m_fsWeather, 6, 30);
        form->addRow(QString::fromUtf8("星期行字号"), fsW);
        form->addRow(QString::fromUtf8("日期行字号"), fsD);
        form->addRow(QString::fromUtf8("时区时间字号"), fsT);
        form->addRow(QString::fromUtf8("天气行字号"), fsWe);

        auto fillCityCombo = [](QComboBox* cb, const QString& name, const QString& zone) {
            for (const CityTz& c : kCityTz)
                cb->addItem(QString::fromUtf8(c.name), QString::fromLatin1(c.tz));
            int idx = cb->findData(zone);
            if (idx < 0) {
                cb->insertItem(0, name, zone);
                idx = 0;
            }
            cb->setCurrentIndex(idx);
        };

        QComboBox* dialBox = new QComboBox;
        fillCityCombo(dialBox, m_dialCity.name, m_dialCity.zone);
        form->addRow(QString::fromUtf8("表盘时间城市"), dialBox);

        QComboBox* tzBox[TZ_N];
        for (int i = 0; i < TZ_N; ++i) {
            tzBox[i] = new QComboBox;
            fillCityCombo(tzBox[i], m_tzCity[i].name, m_tzCity[i].zone);
            form->addRow(QString::fromUtf8("时区城市 %1").arg(i + 1), tzBox[i]);
        }

        QLineEdit* edits[WEATHER_N];
        for (int i = 0; i < WEATHER_N; ++i) {
            edits[i] = new QLineEdit(m_wcityName[i]);
            form->addRow(QString::fromUtf8("天气城市 %1").arg(i + 1), edits[i]);
        }

        QDialogButtonBox* bb =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(bb);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() == QDialog::Accepted) {
            m_alpha = alpha->value() * 255 / 100;
            m_fsWeekday = fsW->value();
            m_fsDate = fsD->value();
            m_fsTz = fsT->value();
            m_fsWeather = fsWe->value();
            m_dialCity.name = dialBox->currentText();
            m_dialCity.zone = dialBox->currentData().toString();
            for (int i = 0; i < TZ_N; ++i) {
                m_tzCity[i].name = tzBox[i]->currentText();
                m_tzCity[i].zone = tzBox[i]->currentData().toString();
            }
            for (int i = 0; i < WEATHER_N; ++i) {
                QString c = edits[i]->text().trimmed();
                if (!c.isEmpty()) m_wcityName[i] = c;
            }
            saveSettings();
            fetchWeather();
            update();
        }
    }

    void loadSettings() {
        QSettings s(QStringLiteral("abcclock"), QStringLiteral("abcclock"));
        m_alpha = qBound(20, s.value(QStringLiteral("alpha"), 150).toInt(), 255);
        m_fsWeekday = qBound(6, s.value(QStringLiteral("fsWeekday"), 14).toInt(), 40);
        m_fsDate = qBound(0, s.value(QStringLiteral("fsDate"), 0).toInt(), 48);
        m_fsTz = qBound(0, s.value(QStringLiteral("fsTz"), 0).toInt(), 48);
        m_fsWeather = qBound(6, s.value(QStringLiteral("fsWeather"), 11).toInt(), 30);
        static const char* wdefs[WEATHER_N] = {"庆阳", "成都", "佛山"};
        for (int i = 0; i < WEATHER_N; ++i) {
            m_wcityName[i] = s.value(QStringLiteral("weather%1").arg(i),
                                     QString::fromUtf8(wdefs[i])).toString();
        }
        m_dialCity.name = s.value(QStringLiteral("dialName"),
                                  QString::fromUtf8("北京")).toString();
        m_dialCity.zone = s.value(QStringLiteral("dialZone"),
                                  QStringLiteral("Asia/Shanghai")).toString();
        static const char* tdefs[TZ_N] = {"北京", "纽约", "洛杉矶"};
        static const char* zdefs[TZ_N] = {"Asia/Shanghai", "America/New_York",
                                          "America/Los_Angeles"};
        for (int i = 0; i < TZ_N; ++i) {
            m_tzCity[i].name = s.value(QStringLiteral("tzName%1").arg(i),
                                       QString::fromUtf8(tdefs[i])).toString();
            m_tzCity[i].zone = s.value(QStringLiteral("tzZone%1").arg(i),
                                       QString::fromLatin1(zdefs[i])).toString();
        }
    }

    void saveSettings() {
        QSettings s(QStringLiteral("abcclock"), QStringLiteral("abcclock"));
        s.setValue(QStringLiteral("alpha"), m_alpha);
        s.setValue(QStringLiteral("fsWeekday"), m_fsWeekday);
        s.setValue(QStringLiteral("fsDate"), m_fsDate);
        s.setValue(QStringLiteral("fsTz"), m_fsTz);
        s.setValue(QStringLiteral("fsWeather"), m_fsWeather);
        for (int i = 0; i < WEATHER_N; ++i)
            s.setValue(QStringLiteral("weather%1").arg(i), m_wcityName[i]);
        s.setValue(QStringLiteral("dialName"), m_dialCity.name);
        s.setValue(QStringLiteral("dialZone"), m_dialCity.zone);
        for (int i = 0; i < TZ_N; ++i) {
            s.setValue(QStringLiteral("tzName%1").arg(i), m_tzCity[i].name);
            s.setValue(QStringLiteral("tzZone%1").arg(i), m_tzCity[i].zone);
        }
    }

    QString m_city;
    QString m_weather[WEATHER_N];
    QString m_wcityName[WEATHER_N] = { QString::fromUtf8("庆阳"),
                                       QString::fromUtf8("成都"),
                                       QString::fromUtf8("佛山") };
    struct TzCitySel { QString name; QString zone; };
    TzCitySel m_dialCity = { QString::fromUtf8("北京"),
                             QStringLiteral("Asia/Shanghai") };
    TzCitySel m_tzCity[TZ_N] = {
        { QString::fromUtf8("北京"),   QStringLiteral("Asia/Shanghai") },
        { QString::fromUtf8("纽约"),   QStringLiteral("America/New_York") },
        { QString::fromUtf8("洛杉矶"), QStringLiteral("America/Los_Angeles") },
    };
    int m_fsWeekday = 14;   // 星期行字号
    int m_fsDate = 0;       // 日期行字号（0 = 自动填满宽度）
    int m_fsTz = 0;         // 时区时间字号（0 = 自动填满行）
    int m_fsWeather = 11;   // 天气行基准字号（描述 +2，温度 +3）
    int m_alpha = 150;                 // 背景不透明度（0-255），设置面板可调
    QSystemTrayIcon* m_tray = nullptr;
    QTimer* m_trayRetry = nullptr;   // 托盘宿主未就绪时的重试定时器
    QMenu* m_menu = nullptr;
    QMenu* m_hideMenu = nullptr;     // 窗口右键：按时长隐藏
    QTimer* m_showTimer = nullptr;   // 自动显示定时器
    QAction* m_toggleAct = nullptr;
    QAction* m_settingsAct = nullptr;
    QAction* m_autostartAct = nullptr;
    QAction* m_exitAct = nullptr;
#if defined(Q_OS_MACOS)
    bool m_macPlaced = false;   // macOS 首次显示后重新定位（首次 show 会被系统微调）
#endif
};

// 选择带 StatusNotifierWatcher 的会话总线：自启（systemd-xdg-autostart 等）
// 拿到的 DBUS_SESSION_BUS_ADDRESS 常是没有托盘宿主的系统总线。
// 必须在 QApplication 之前执行——Qt 在构造 app 时就会连上会话总线。
#if defined(Q_OS_LINUX)
static bool busHasWatcher(const QString& addr) {
    DBusError err;
    dbus_error_init(&err);
    DBusConnection* c = dbus_connection_open_private(addr.toUtf8().constData(), &err);
    if (!c) {
        dbus_error_free(&err);
        return false;
    }
    bool has = false;
    if (dbus_bus_register(c, &err))
        has = dbus_bus_name_has_owner(c, "org.kde.StatusNotifierWatcher", &err);
    dbus_error_free(&err);
    dbus_connection_close(c);
    dbus_connection_unref(c);
    return has;
}

static QString findTrayBus() {
    QStringList cands;
    const QString cur = qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS");
    if (!cur.isEmpty()) cands << cur;
    cands << QStringLiteral("env:unix:path=/run/user/%1/bus").arg(getuid());
    for (const QFileInfo& fi : QDir(QStringLiteral("/tmp"))
                                   .entryInfoList({QStringLiteral("dbus-*")},
                                                  QDir::Files | QDir::System)) {
        if (fi.ownerId() == getuid())
            cands << QStringLiteral("env:unix:path=") + fi.absoluteFilePath();
    }
    for (QString addr : cands) {
        addr.remove(QStringLiteral("env:"));
        if (busHasWatcher(addr)) return addr;
    }
    return QString();
}
#endif

#if defined(Q_OS_LINUX)
// 试连一个 X 显示（含认证）。失败时 xcb_connect 不打印任何东西，由调用方决定提示。
static bool xDisplayConnectable(const QByteArray& display) {
    int screen = 0;
    xcb_connection_t* conn =
        xcb_connect(display.isEmpty() ? nullptr : display.constData(), &screen);
    const bool ok = conn && xcb_connection_has_error(conn) == 0;
    if (conn)
        xcb_disconnect(conn);
    return ok;
}

static bool xDisplayReachable() {
    const QByteArray display = qgetenv("DISPLAY");
    return !display.isEmpty() && xDisplayConnectable(display);
}

// Wayland 会话：本程序依赖 override-redirect 固定悬浮，需走 XWayland。
// 有些启动器（或无图形 shell / SSH）不导出 DISPLAY，此时回落 Xwayland 常用的 :1。
// 返回 false 表示按自动选择走不通（用户显式指定 QT_QPA_PLATFORM 时不干预，一律 true）。
static bool prepareX11Display() {
    if (!qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        return true;
    if (qEnvironmentVariableIsEmpty("DISPLAY")
        && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
        && xDisplayConnectable(":1"))
        qputenv("DISPLAY", ":1");
    if (!xDisplayReachable())
        return false;
    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "xcb");
    return true;
}
#endif

int main(int argc, char* argv[]) {
#if defined(Q_OS_LINUX)
    // 没有可用 X 时 Qt 的 xcb 插件会直接 abort（SIGABRT，几乎不打印原因），
    // 这里提前试连一次并给出可读提示、返回非零码。
    if (!prepareX11Display()) {
        std::fprintf(stderr,
                     "abcclock: 无法启动：DISPLAY=\"%s\" 连不上（WAYLAND_DISPLAY=\"%s\"）。\n"
                     "本程序是 X11 悬浮时钟：Wayland 会话需要 Xwayland（通常 DISPLAY=:1，"
                     "必要时设置 XAUTHORITY），或显式导出 QT_QPA_PLATFORM 指定平台。\n",
                     qgetenv("DISPLAY").constData(), qgetenv("WAYLAND_DISPLAY").constData());
        return 1;
    }
    // 托盘总线自愈（必须在 QApplication 之前设置环境变量）
    const QString bus = findTrayBus();
    if (!bus.isEmpty() && bus != qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"))
        qputenv("DBUS_SESSION_BUS_ADDRESS", bus.toUtf8());
#endif
    QApplication app(argc, argv);
#if defined(Q_OS_MACOS)
    // 菜单栏常驻应用：不占 Dock、不抢前台（打包后 Info.plist 的 LSUIElement 亦生效）
    app.setQuitOnLastWindowClosed(false);
#endif
    // 城市名：命令行参数或默认"北京"
    QString city = argc > 1 ? QString::fromUtf8(argv[1]) : QString::fromUtf8("北京");
    HoverTzClock clock(city);
    clock.show();
    return app.exec();
}
