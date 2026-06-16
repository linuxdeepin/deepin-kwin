#ifndef SPLITMENU_H
#define SPLITMENU_H

#include "window.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QSharedPointer>
#include <QPointer>

namespace KWin
{

class SplitIcon;

class SplitMenu : public QWidget
{
    Q_OBJECT
public:
    static SplitMenu *instance();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEvent *event) override;
#else
    void enterEvent(QEnterEvent *event) override;
#endif

    void leaveEvent(QEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

    void Show(const QRect &button_rect, uint32_t client_id);
    void Hide(bool delay, bool internal = false);

    void setKeepShowing(bool keep) { m_keepShowing = keep; }

private:
    explicit SplitMenu();
    ~SplitMenu() override;

    void findClient(uint32_t client_id);

    void checkArea(const QRect &button_rect);

    void stopTime();

    bool m_isShow = false;
    bool m_keepShowing = false;
    bool m_entered = false;
    bool m_upside = false;
    QPointer<Window> m_client = nullptr;

    QHBoxLayout *layout;
    SplitIcon *llabel;
    SplitIcon *clabel;
    SplitIcon *rlabel;
    QRect m_hump, m_rect;  // relative rectangle
    float   m_scale = 1.0;

    QTimer show_timer, hide_timer;

    QGraphicsDropShadowEffect *shadow = nullptr;
};

}

#endif // SPLITMENU_H
