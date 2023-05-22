#ifndef CHAMELEONSPLITMENU_H
#define CHAMELEONSPLITMENU_H

#include "kwineffects.h"

#include <QWidget>
#include <QMouseEvent>
#include <QLabel>
#include <QGraphicsDropShadowEffect>

enum class QuickTileFlag {
    None        = 0,
    Left        = 1 << 0,
    Right       = 1 << 1,
    Top         = 1 << 2,
    Bottom      = 1 << 3,
    Horizontal  = Left | Right,
    Vertical    = Top | Bottom,
    Maximize    = Left | Right | Top | Bottom,
};

class ChameleonSplitMenu : public QWidget
{
    Q_OBJECT
public:
    explicit ChameleonSplitMenu(QWidget *parent = nullptr, bool isSupportFourSplit = false);
    ~ChameleonSplitMenu();

    void Show(const QRect& screenGeom, QPoint pos, QColor color);
    void Hide();
    bool isShow();
    void setShowSt(bool flag);
    bool getMenuSt(){ return m_MenuSt;};
    void setEffect(QObject *client);
    void setEffect(WId id);
    void setSupportFourSplit(bool flag);
    bool getSupportFourSplit() {return m_isSupportFourSplit;}

    void startTime(int tm = 300);
    void stopTime();

    void CheckTheme();

    void enterEvent(QEvent *e) override;
    void leaveEvent(QEvent *e) override;

    bool eventFilter(QObject *obj, QEvent *event) override;
    void paintEvent(QPaintEvent *e) override;

private:
    bool m_isShow = false;
    bool m_MenuSt = false;
    bool m_isDark = false;
    QPoint m_pos;
    QColor m_color;
    QLabel *llabel;
    QLabel *clabel;
    QLabel *rlabel;
    QLabel *blabel;

    QLabel *threeLabelLeft;
    QLabel *threeLabelTopRight;
    QLabel *threeLabelBottomRight;

    QLabel *threeLabelTopLeft;
    QLabel *threeLabelBottomLeft;
    QLabel *threeLabelRight;

    QLabel *fourLabelTopLeft;
    QLabel *fourLabelBottomLeft;
    QLabel *fourLabelTopRight;
    QLabel *fourLabelBottomRight;
    bool m_isSupportFourSplit = false;

    QObject *m_client = nullptr;
    QTimer *tip_timer = nullptr;

    QGraphicsDropShadowEffect *shadow = nullptr;
};

#endif
