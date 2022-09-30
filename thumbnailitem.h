// Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_THUMBNAILITEM_H
#define KWIN_THUMBNAILITEM_H

#include <QPointer>
#include <QWeakPointer>
#include <QQuickPaintedItem>

namespace KWin
{

class AbstractClient;
class EffectWindow;
class EffectWindowImpl;

class AbstractThumbnailItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
    Q_PROPERTY(QQuickItem *clipTo READ clipTo WRITE setClipTo NOTIFY clipToChanged)
public:
    virtual ~AbstractThumbnailItem();
    qreal brightness() const;
    qreal saturation() const;
    QQuickItem *clipTo() const;

public Q_SLOTS:
    void setBrightness(qreal brightness);
    void setSaturation(qreal saturation);
    void setClipTo(QQuickItem *clip);

Q_SIGNALS:
    void brightnessChanged();
    void saturationChanged();
    void clipToChanged();

protected:
    explicit AbstractThumbnailItem(QQuickItem *parent = 0);

protected Q_SLOTS:
    virtual void repaint(KWin::EffectWindow* w) = 0;

private Q_SLOTS:
    void init();
    void effectWindowAdded();
    void compositingToggled();

private:
    void findParentEffectWindow();
    QWeakPointer<EffectWindowImpl> m_parent;
    qreal m_brightness;
    qreal m_saturation;
    QPointer<QQuickItem> m_clipToItem;
};

class WindowThumbnailItem : public AbstractThumbnailItem
{
    Q_OBJECT
    Q_PROPERTY(qulonglong wId READ wId WRITE setWId NOTIFY wIdChanged SCRIPTABLE true)
    Q_PROPERTY(KWin::AbstractClient *client READ client WRITE setClient NOTIFY clientChanged)
public:
    explicit WindowThumbnailItem(QQuickItem *parent = 0);
    virtual ~WindowThumbnailItem();

    qulonglong wId() const {
        return m_wId;
    }
    void setWId(qulonglong wId);
    AbstractClient *client() const;
    void setClient(AbstractClient *client);
    virtual void paint(QPainter *painter);
Q_SIGNALS:
    void wIdChanged(qulonglong wid);
    void clientChanged();
protected Q_SLOTS:
    virtual void repaint(KWin::EffectWindow* w);
private:
    qulonglong m_wId;
    AbstractClient *m_client;
};

class DesktopThumbnailItem : public AbstractThumbnailItem
{
    Q_OBJECT
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)
public:
    DesktopThumbnailItem(QQuickItem *parent = 0);
    virtual ~DesktopThumbnailItem();

    int desktop() const {
        return m_desktop;
    }
    void setDesktop(int desktop);
    virtual void paint(QPainter *painter);
Q_SIGNALS:
    void desktopChanged(int desktop);
protected Q_SLOTS:
    virtual void repaint(KWin::EffectWindow* w);
private:
    int m_desktop;
};

inline
qreal AbstractThumbnailItem::brightness() const
{
    return m_brightness;
}

inline
qreal AbstractThumbnailItem::saturation() const
{
    return m_saturation;
}

inline
QQuickItem* AbstractThumbnailItem::clipTo() const
{
    return m_clipToItem.data();
}

inline
AbstractClient *WindowThumbnailItem::client() const
{
    return m_client;
}

} // KWin

#endif // KWIN_THUMBNAILITEM_H
