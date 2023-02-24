// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CHAMELEONBUTTON_H
#define CHAMELEONBUTTON_H

#include <KDecoration2/DecorationButton>
#include "chameleonsplitmenu.h"

class ChameleonButton : KDecoration2::DecorationButton
{
    Q_OBJECT
public:
    explicit ChameleonButton(KDecoration2::DecorationButtonType type, const QPointer<KDecoration2::Decoration> &decoration, QObject *parent = nullptr);
    virtual ~ChameleonButton();

    static DecorationButton *create(KDecoration2::DecorationButtonType type, KDecoration2::Decoration *decoration, QObject *parent);

    virtual void hoverEnterEvent(QHoverEvent *event) override;
    virtual void hoverLeaveEvent(QHoverEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

protected Q_SLOTS:
    void onCompositorChanged(bool);
    void onClientAreaUpdate();

protected:
    void paint(QPainter *painter, const QRect &repaintRegion) override;
    void showSplitMenu();
    KDecoration2::DecorationButtonType m_type;

    ChameleonSplitMenu *m_pSplitMenu = nullptr;

    QTimer *max_hover_timer = nullptr;
    int m_mousePosX;

    QColor m_backgroundColor;
    KWin::EffectWindow *effect = nullptr;
    QTimer *max_timer = nullptr;
    bool m_isMaxAvailble = true;
    bool m_wlHoverStatus = false;
};

#endif // CHAMELEONBUTTON_H
