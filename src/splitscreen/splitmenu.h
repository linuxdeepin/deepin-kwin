#ifndef SPLITMENU_H
#define SPLITMENU_H

#include "window.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QSharedPointer>

namespace KWin
{

class SplitMenu : public QWidget
{
    Q_OBJECT
public:
    static SplitMenu *instance();

    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

    bool eventFilter(QObject *obj, QEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

    void Show(const QRect &button_rect, uint32_t client_id);
    void Hide(bool delay, bool internal = false);

    void setKeepShowing(bool keep) { m_keepShowing = keep; }

private:
    explicit SplitMenu();
    ~SplitMenu() override;

    void findClient(uint32_t client_id);

    void checkTheme();
    void checkArea(const QRect &button_rect, const QSize &size);

    void stopTime();

    bool m_isShow = false;
    bool m_keepShowing = false;
    bool m_isDark = false;
    bool entered = false;
    bool upside = false;
    Window *m_client = nullptr;

    QPoint m_pos;
    QColor m_color;
    QHBoxLayout *layout;
    QLabel *llabel;
    QLabel *clabel;
    QLabel *rlabel;
    float   m_scale = 1.0;

    QTimer *show_timer = nullptr;
    QTimer *hide_timer = nullptr;

    QGraphicsDropShadowEffect *shadow = nullptr;
};

}

#endif // SPLITMENU_H