#include "kwineffects.h"
#include "splitmenu.h"
#include "x11window.h"
#include "waylandwindow.h"
#include "workspace.h"
#include "wayland_server.h"

#include <QCoreApplication>
#include <QEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSvgRenderer>
#include <QToolTip>
#include <QTranslator>


#ifdef BUILD_ON_V25
#define USE_V25_SPLITMENU 1     // 默认使用V25样式
#else
#define USE_V25_SPLITMENU 0     // 默认使用V20样式
#endif

#define LONG_PRESS_TIME 500
#define HIDE_DELAY_TIME 300

namespace KWin
{

struct IconColors {
    QColor normalBg, normalBorder;
    QColor hoverBg, hoverBorder;
    QColor pressBg, pressBorder;
};

struct SplitmenuColors {
    QColor bg;           // A: menu bg      / B: hump+rect fill
    QColor innerBorder;  // A: inner border / B: hump+rect outline
};

struct ThemeColors {
    IconColors icon;
    SplitmenuColors splitmenu;
};

Q_LOGGING_CATEGORY(SPLIT_MENU, "kwin.splitmenu", QtCriticalMsg);

#if USE_V25_SPLITMENU

#define ICON_WIDTH              44  // 分屏图标宽度
#define ICON_HEIGHT             58  // 分屏图标高度
#define LABEL_SPACING           2   // 分屏图标间距
#define OUTER_RADIUS            18  // 大圆角矩形的圆角半径
#define SHADOW_OFFSET_X         0   // 阴影水平偏移
#define SHADOW_OFFSET_Y         6   // 阴影垂直偏移
#define SHADOW_ALPHA           0.15f // 阴影透明度
#define SHADOW_BLUR_RADIUS      24  // 阴影模糊半径
#define INNER_RADIUS            8   // 内边框圆角半径
#define INNER_BORDER_WIDTH      1   // 内边框宽度
#define INNER_BORDER_MARGIN     11  // 小圆角矩形到大圆角矩形的间距
#define CONTENT_MARGIN          13  // 图标容器内边距（layout margins）
#define BACKGROUND_MARGIN       12  // 背景/内边框基准（paintEvent pad）

static const ThemeColors lightTheme = {
    { QColor(0, 0, 0, 0.1 * 255), QColor(203, 203, 203, 255),
      QColor(0, 0, 0, 0.4 * 255), QColor(0, 129, 255, 255),
      QColor(0, 0, 0, 0.4 * 255), QColor(0, 129, 255, 255) },
    { QColor(238, 238, 238, 255), QColor(0, 0, 0, 0.1 * 255) }
};
static const ThemeColors darkTheme = {
    { QColor(255, 255, 255, 0.1 * 255), QColor(255, 255, 255, 0.1 * 255),
      QColor(255, 255, 255, 0.4 * 255), QColor(2, 76, 202, 255),
      QColor(255, 255, 255, 0.4 * 255), QColor(2, 76, 202, 255) },
    { QColor(20, 20, 20, 255), QColor(255, 255, 255, 0.1 * 255) }
};

inline const ThemeColors &getThemeColors(bool isDark)
{
    return isDark ? darkTheme : lightTheme;
}

class SplitIcon : public QWidget
{
    Q_OBJECT
public:
    SplitIcon(QWidget *parent, qreal radius, qreal scale)
        : QWidget(parent), m_radius(radius), m_scale(scale)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedSize(QSize(ICON_WIDTH, ICON_HEIGHT) * m_scale);
    }

Q_SIGNALS:
    void clicked();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override
#else
    void enterEvent(QEvent *event) override
#endif
    {
        m_hovered = true;
        update();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hovered = false;
        update();
        QWidget::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = false;
            update();
            Q_EMIT clicked();
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const auto &theme = getThemeColors(workspace()->isDarkTheme());

        QColor bgColor, borderColor;
        if (m_pressed) {
            bgColor = theme.icon.pressBg;
            borderColor = theme.icon.pressBorder;
        } else if (m_hovered) {
            bgColor = theme.icon.hoverBg;
            borderColor = theme.icon.hoverBorder;
        } else {
            bgColor = theme.icon.normalBg;
            borderColor = theme.icon.normalBorder;
        }

        p.setPen(QPen(borderColor, m_scale));
        p.setBrush(bgColor);
        p.drawRoundedRect(rect(), m_radius, m_radius);
    }

private:
    qreal m_radius;
    qreal m_scale;
    bool m_hovered = false;
    bool m_pressed = false;
};

#else

#define ICON_WIDTH              37  // 分屏图标宽度
#define ICON_HEIGHT             37  // 分屏图标高度
#define LABEL_SPACING           5   // 分屏图标间距
#define OUTER_RADIUS            14  // 大圆角矩形的圆角半径
#define SHADOW_OFFSET_X         0   // 阴影水平偏移
#define SHADOW_OFFSET_Y         6   // 阴影垂直偏移
#define SHADOW_ALPHA           0.15f // 阴影透明度
#define SHADOW_BLUR_RADIUS      20  // 阴影模糊半径
#define BUTTON_OFFSET           5   // 菜单与按钮的垂直间距
#define HUMP_WIDTH              50  // 驼峰宽度
#define HUMP_HEIGHT             20  // 驼峰高度
#define INNER_BORDER_WIDTH      0   // 无内边框
#define INNER_RADIUS            0   // 内边框圆角半径
#define INNER_BORDER_MARGIN     0   // 小圆角矩形到大圆角矩形的间距
#define CONTENT_MARGIN          0   // 图标容器内边距（layout margins）
#define BACKGROUND_MARGIN       0   // 背景/内边框基准（paintEvent pad）

#define LEFT_HOVER      "left_split_hover.svg"
#define LEFT_NORMAL     "left_split_normal.svg"
#define RIGHT_HOVER     "right_split_hover.svg"
#define RIGHT_NORMAL    "right_split_normal.svg"
#define MAX_HOVER       "max_split_hover.svg"
#define MAX_NORMAL      "max_split_normal.svg"
#define RESTORE_HOVER   "restore_split_hover.svg"
#define RESTORE_NORMAL  "restore_split_normal.svg"

static const ThemeColors lightTheme = {
    { QColor(), QColor(), QColor(), QColor(), QColor(), QColor() },
    { QColor(255, 255, 255), QColor(185, 185, 185, 0.7 * 255) }
};
static const ThemeColors darkTheme = {
    { QColor(), QColor(), QColor(), QColor(), QColor(), QColor() },
    { QColor(0, 0, 0), QColor(55, 55, 55, 0.7 * 255) }
};

inline const ThemeColors &getThemeColors(bool isDark)
{
    return isDark ? darkTheme : lightTheme;
}

class SplitIcon : public QLabel
{
    Q_OBJECT
public:
    SplitIcon(QWidget *parent, qreal radius, qreal scale,
              const QString &normalFile = QString(),
              const QString &hoverFile  = QString())
        : QLabel(parent), m_radius(radius), m_scale(scale),
          m_normalFile(normalFile), m_hoverFile(hoverFile)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedSize(QSize(ICON_WIDTH, ICON_HEIGHT) * m_scale);
        setPixmap();
    }

    void setTooltipText(const QString &text)
    {
        m_tooltipText = text;
    }

    void setMaximized(bool maximized)
    {
        if (m_maximized == maximized)
            return;
        m_maximized = maximized;
        setPixmap();
    }

    void setPixmap()
    {
        QString file = m_hovered ? m_hoverFile : m_normalFile;
        if (file.isEmpty())
            return;

        if (file.contains("max") || file.contains("restore")) {
            file = m_hovered
                ? (m_maximized ? RESTORE_HOVER : MAX_HOVER)
                : (m_maximized ? RESTORE_NORMAL : MAX_NORMAL);
        }

        QLabel::setPixmap(getPixmap(file, size()));
    }

Q_SIGNALS:
    void clicked();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override
#else
    void enterEvent(QEvent *event) override
#endif
    {
        m_hovered = true;
        setPixmap();
        if (!m_tooltipText.isEmpty()) {
            int yOffset = ICON_HEIGHT * m_scale - 2 * LABEL_SPACING * m_scale;
            QToolTip::showText(mapToGlobal(QPoint(width() / 2, yOffset)), m_tooltipText, this);
        }
        QLabel::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hovered = false;
        setPixmap();
        QToolTip::hideText();
        QLabel::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            setPixmap();
        }
        QLabel::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = false;
            setPixmap();
            Q_EMIT clicked();
        }
        QLabel::mouseReleaseEvent(event);
    }

private:
    qreal m_radius;
    qreal m_scale;
    bool m_hovered = false;
    bool m_pressed = false;
    QString m_normalFile;
    QString m_hoverFile;
    bool m_maximized = false;
    QString m_tooltipText;

    static QPixmap getPixmap(const QString &file, const QSize &size)
    {
        const bool isDark = workspace()->isDarkTheme();
        const QString theme = isDark ? QStringLiteral("dark") : QStringLiteral("light");

        static QMap<QString, QPixmap> cache;
        const QString key = theme + file + QStringLiteral("%1%2")
                                        .arg(size.width()).arg(size.height());
        if (cache.contains(key))
            return cache[key];

        QSvgRenderer svg(QStringLiteral(":/splitmenu/themes/%1/icons/%2")
                            .arg(theme, file));
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing);
        svg.render(&painter);

        cache.insert(key, pixmap);
        return pixmap;
    }
};

#endif // USE_V25_SPLITMENU

SplitMenu *SplitMenu::instance()
{
    static SplitMenu menu;
    return &menu;
}

SplitMenu::SplitMenu()
{
    if (waylandServer()) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint);
    } else {
        setWindowFlags(Qt::X11BypassWindowManagerHint);
    }
    setAttribute(Qt::WA_TranslucentBackground);

    m_scale = QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96;
    setWindowTitle("splitmenu");

    const qreal icon_radius = INNER_RADIUS * m_scale;

#if USE_V25_SPLITMENU
    auto *leftIcon = new SplitIcon(this, icon_radius, m_scale);
    auto *rightIcon = new SplitIcon(this, icon_radius, m_scale);
#else
    auto *leftIcon = new SplitIcon(this, icon_radius, m_scale, LEFT_NORMAL, LEFT_HOVER);
    auto *rightIcon = new SplitIcon(this, icon_radius, m_scale, RIGHT_NORMAL, RIGHT_HOVER);
    auto *centerIcon = new SplitIcon(this, icon_radius, m_scale, MAX_NORMAL, MAX_HOVER);
#endif

    llabel = leftIcon;
    rlabel = rightIcon;
#if USE_V25_SPLITMENU
    clabel = nullptr;
#else
    clabel = centerIcon;
#endif

    layout = new QHBoxLayout();
    layout->setSpacing(LABEL_SPACING * m_scale);

    const int margin = (BACKGROUND_MARGIN + CONTENT_MARGIN) * m_scale;
    layout->setContentsMargins(margin, margin, margin, margin);

    layout->addWidget(llabel);
    layout->addWidget(rlabel);
#if !USE_V25_SPLITMENU
    layout->addWidget(clabel);
#endif
    setLayout(layout);

    connect(llabel, &SplitIcon::clicked, this, [this] {
        if (m_client)
            m_client->setQuickTileFromMenu(QuickTileFlag::Left);
        Hide(false, true);
    });
    connect(rlabel, &SplitIcon::clicked, this, [this] {
        if (m_client)
            m_client->setQuickTileFromMenu(QuickTileFlag::Right);
        Hide(false, true);
    });

#if !USE_V25_SPLITMENU
    connect(clabel, &SplitIcon::clicked, this, [this] {
        if (m_client)
            m_client->setQuickTileFromMenu(QuickTileFlag::Maximize, false);
        bool maximized = m_client && m_client->maximizeMode() == MaximizeMode::MaximizeFull;
        clabel->setMaximized(maximized);
        clabel->setTooltipText(maximized ? tr("Unmaximize") : tr("Maximize"));
        Hide(false, true);
    });
#endif

    const qreal shadow_blur_radius = SHADOW_BLUR_RADIUS * m_scale;
    shadow = new QGraphicsDropShadowEffect(this);
    shadow->setOffset(SHADOW_OFFSET_X * m_scale, SHADOW_OFFSET_Y * m_scale);
    shadow->setColor(QColor(0, 0, 0, SHADOW_ALPHA * 255));
    shadow->setBlurRadius(shadow_blur_radius);
    this->setGraphicsEffect(shadow);

#if !USE_V25_SPLITMENU
    // 驼峰
    const QSize hump_size(QSize(HUMP_WIDTH, HUMP_HEIGHT) * m_scale);
    m_hump.setSize(hump_size);
    m_hump.moveLeft(shadow->blurRadius() + 4 * layout->spacing()
                    + 1.5 * leftIcon->width());
    m_rect.setSize(QSize(8 * layout->spacing() + 3 * leftIcon->width(),
                            4 * layout->spacing() + leftIcon->height()));
    m_rect.moveLeft(shadow->blurRadius());
    setFixedSize(m_rect.width() + 2 * shadow->blurRadius(),
                    m_rect.height() + m_hump.height() + shadow->blurRadius());

    llabel->setTooltipText(tr("Tile window to left of screen"));
    rlabel->setTooltipText(tr("Tile window to right of screen"));
    clabel->setTooltipText(tr("Maximize"));
#endif

    //翻译
    QString qm = QString(":/splitmenu/translations/splitmenu_%1.qm")
                    .arg(QLocale::system().name());
    auto *tran = new QTranslator(this);
    if (tran->load(qm)) {
        qApp->installTranslator(tran);
    } else {
        qCDebug(SPLIT_MENU) << "load" << qm << "failed";
    }

    show_timer.setSingleShot(true);
    connect(&show_timer, &QTimer::timeout, this, [this] {
        m_isShow = true;
        adjustSize();
        show();
    });

    hide_timer.setSingleShot(true);
    connect(&hide_timer, &QTimer::timeout, this, [this] {
        m_isShow = false;
        m_entered = false;
        m_client = nullptr;
        hide();
    });
}

SplitMenu::~SplitMenu()
{
    if (shadow)
        delete shadow;
}

void SplitMenu::Show(const QRect &button_rect, uint32_t client_id)
{
    if (m_isShow)
        return;
    findClient(client_id);
    if (!m_client)
        return;
    stopTime();

#if !USE_V25_SPLITMENU
    bool maximized = m_client && m_client->maximizeMode() == MaximizeMode::MaximizeFull;
    clabel->setMaximized(maximized);
#endif

    checkArea(button_rect);

#if !USE_V25_SPLITMENU
    llabel->setTooltipText(tr("Tile window to left of screen"));
    rlabel->setTooltipText(tr("Tile window to right of screen"));
    clabel->setTooltipText(maximized ? tr("Unmaximize") : tr("Maximize"));
#endif

    show_timer.start(LONG_PRESS_TIME);
}

void SplitMenu::Hide(bool delay, bool internal)
{
    if ((m_entered && !internal) || m_keepShowing)
        return;

    stopTime();
    hide_timer.start((!delay || !m_isShow) ? 0 : HIDE_DELAY_TIME);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void SplitMenu::enterEvent(QEnterEvent *event)
#else
void SplitMenu::enterEvent(QEvent *event)
#endif
{
    m_entered = true;
    QWidget::enterEvent(event);
    stopTime();
}

void SplitMenu::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    Hide(true, true);
}

void SplitMenu::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing, true);

    const int pad = BACKGROUND_MARGIN * m_scale;
    const QRectF contentRect = rect().adjusted(pad, pad, -pad, -pad);
    const qreal radius = OUTER_RADIUS * m_scale;

    // 背景圆角矩形
    const auto &theme = getThemeColors(workspace()->isDarkTheme());
#if USE_V25_SPLITMENU
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme.splitmenu.bg);
    painter.drawRoundedRect(contentRect, radius, radius);
#endif

    // 内边框（V20 width=0 跳过）
    if (INNER_BORDER_WIDTH > 0 && theme.splitmenu.innerBorder.isValid()) {
        const qreal bw = INNER_BORDER_WIDTH * m_scale;
        const qreal innerRadius = INNER_RADIUS * m_scale;
        const int innerMargin = INNER_BORDER_MARGIN * m_scale;

        QRectF borderOuter = contentRect.adjusted(innerMargin - bw,
                                                   innerMargin - bw,
                                                  -innerMargin + bw,
                                                  -innerMargin + bw);
        QRectF borderInner = contentRect.adjusted(innerMargin, innerMargin,
                                                  -innerMargin, -innerMargin);

        painter.setBrush(theme.splitmenu.innerBorder);
        painter.drawRoundedRect(borderOuter, innerRadius + bw, innerRadius + bw);
        painter.setBrush(theme.splitmenu.bg);
        painter.drawRoundedRect(borderInner, innerRadius, innerRadius);
    }

#if !USE_V25_SPLITMENU
    // 驼峰
    painter.setBrush(Qt::NoBrush);
    QPen pen = QPen(theme.splitmenu.innerBorder);
    pen.setWidth(1);
    painter.setPen(pen);

    double base, sign;
    if (m_upside) {
        base = m_hump.top();
        sign = -1;
    } else {
        base = m_hump.y() + m_hump.height();
        sign = 1;
    }

    QPainterPath hump_path;
    hump_path.moveTo(m_hump.left(), base);
    const double step = m_hump.width() / 50.0,
                 f = 2 * M_PI / m_hump.width(),
                 half = m_hump.height() / 2.0;
    for (double x = step; x < (double)m_hump.width(); x += step) {
        double y = half * sin(f * x - M_PI / 2) + half;
        hump_path.lineTo(m_hump.left() + x, base - sign * y);
    }
    hump_path.lineTo(m_hump.right(), base);
    painter.drawPath(hump_path);

    QPainterPath rect_path;
    rect_path.addRoundedRect(m_rect, radius, radius);
    painter.drawPath(rect_path);

    painter.setPen(Qt::NoPen);
    painter.setBrush(theme.splitmenu.bg);
    painter.drawPath(hump_path);
    painter.drawPath(rect_path);
#endif
}

void SplitMenu::checkArea(const QRect &button_rect)
{
    QRect area = workspace()->clientArea(MaximizeArea, m_client,
                                          button_rect.center()).toRect();

#if USE_V25_SPLITMENU
    //V25：简单居中
    adjustSize();
    {
        const int pad = BACKGROUND_MARGIN * m_scale;
        QPoint pos(button_rect.center().x() - width() / 2,
                   button_rect.bottom() - pad);
        if (pos.x() < area.left()) {
            pos.setX(area.left());
        } else if (pos.x() + width() > area.right()) {
            pos.setX(area.right() - width());
        }
        if (pos.y() + height() > area.bottom()) {
            pos.setY(button_rect.top() - height() + pad);
        }
        if (pos.y() < area.top()) {
            pos.setY(area.top());
        }
        move(pos);
    }
#else
    // V20：驼峰对齐 + upside 翻转
    QPoint pos(button_rect.center().x() - m_hump.center().x(),
               button_rect.bottom() + BUTTON_OFFSET);

    m_upside = false;
    QMargins margins(shadow->blurRadius(), m_hump.height(),
                     shadow->blurRadius(), shadow->blurRadius());
    m_hump.moveTop(1);
    m_rect.moveTop(m_hump.y() + m_hump.height());
    if (pos.x() < area.left()) {
        pos.setX(area.left());
    } else if (pos.x() + width() > area.right()) {
        pos.setX(area.right() - width());
    }
    if (pos.y() < area.top()) {
        pos.setY(area.top());
    } else if (pos.y() + height() > area.bottom()) {
        m_upside = true;
        margins = QMargins(shadow->blurRadius(), shadow->blurRadius(),
                           shadow->blurRadius(), m_hump.height());
        m_rect.moveTop(shadow->blurRadius());
        m_hump.moveTop(m_rect.y() + m_rect.height());
        pos.setY(button_rect.top() - height() + 1);
    }

    layout->setContentsMargins(margins);
    setLayout(layout);
    move(pos);
#endif
}

void SplitMenu::findClient(uint32_t client_id)
{
    if (waylandServer()) {
        m_client = workspace()->findWaylandWindow(client_id);
        if (m_client)
            return;
    }

    m_client = workspace()->findClient(Predicate::WindowMatch, client_id);
}

void SplitMenu::stopTime()
{
    show_timer.stop();
    hide_timer.stop();
}

} // namespace KWin

#include "splitmenu.moc"
