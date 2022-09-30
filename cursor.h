// Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_CURSOR_H
#define KWIN_CURSOR_H
// kwin
#include <kwinglobals.h>
// Qt
#include <QHash>
#include <QObject>
#include <QPoint>
// xcb
#include <xcb/xcb.h>

class QTimer;

namespace KWin
{

namespace ExtendedCursor {
enum Shape {
    SizeNorthWest = 0x100 + 0,
    SizeNorth = 0x100 + 1,
    SizeNorthEast = 0x100 + 2,
    SizeEast = 0x100 + 3,
    SizeWest = 0x100 + 4,
    SizeSouthEast = 0x100 + 5,
    SizeSouth = 0x100 + 6,
    SizeSouthWest = 0x100 + 7
};
}
/**
 * Extension of Qt::CursorShape with values not currently present there
 */


/**
 * @brief Wrapper round Qt::CursorShape with extensions enums into a single entity
 */
class KWIN_EXPORT CursorShape {
public:
    CursorShape() = default;
    CursorShape(Qt::CursorShape qtShape) {
        m_shape = qtShape;
    }
    CursorShape(KWin::ExtendedCursor::Shape kwinShape) {
        m_shape = kwinShape;
    }
    bool operator==(const CursorShape &o) const {
        return m_shape == o.m_shape;
    }
    operator int() const {
        return m_shape;
    }
    /**
     * @brief The name of a cursor shape in the theme.
     */
    QByteArray name() const;
private:
    int m_shape = Qt::ArrowCursor;
};

/**
 * @short Replacement for QCursor.
 *
 * This class provides a similar API to QCursor and should be preferred inside KWin. It allows to
 * get the position and warp the mouse cursor with static methods just like QCursor. It also provides
 * the possibility to get an X11 cursor for a Qt::CursorShape - a functionality lost in Qt 5's QCursor
 * implementation.
 *
 * In addition the class provides a mouse polling facility as required by e.g. Effects and ScreenEdges
 * and emits signals when the mouse position changes. In opposite to QCursor this class is a QObject
 * and cannot be constructed. Instead it provides a singleton getter, though the most important
 * methods are wrapped in a static method, just like QCursor.
 *
 * The actual implementation is split into two parts: a system independent interface and a windowing
 * system specific subclass. So far only an X11 backend is implemented which uses query pointer to
 * fetch the position and warp pointer to set the position. It uses a timer based mouse polling and
 * can provide X11 cursors through the XCursor library.
 **/
class KWIN_EXPORT Cursor : public QObject
{
    Q_OBJECT
public:
    virtual ~Cursor();
    void startMousePolling();
    void stopMousePolling();
    /**
     * @brief Enables tracking changes of cursor images.
     *
     * After enabling cursor change tracking the signal cursorChanged will be emitted
     * whenever a change to the cursor image is recognized.
     *
     * Use stopCursorTracking to no longer emit this signal. Note: the signal will be
     * emitted until each call of this method has been matched with a call to stopCursorTracking.
     *
     * This tracking is not about pointer position tracking.
     * @see stopCursorTracking
     * @see cursorChanged
     */
    void startCursorTracking();
    /**
     * @brief Disables tracking changes of cursor images.
     *
     * Only call after using startCursorTracking.
     *
     * @see startCursorTracking
     */
    void stopCursorTracking();

    /**
     * @brief The name of the currently used Cursor theme.
     *
     * @return const QString&
     */
    const QString &themeName() const;
    /**
     * @brief The size of the currently used Cursor theme.
     *
     * @return int
     */
    int themeSize() const;
    /**
     * @return list of alternative names for the cursor with @p name
     **/
    QVector<QByteArray> cursorAlternativeNames(const QByteArray &name) const;

    /**
     * Returns the current cursor position. This method does an update of the mouse position if
     * needed. It's save to call it multiple times.
     *
     * Implementing subclasses should prefer to use currentPos which is not performing a check
     * for update.
     **/
    static QPoint pos();
    /**
     * Warps the mouse cursor to new @p pos.
     **/
    static void setPos(const QPoint &pos);
    static void setPos(int x, int y);
    static xcb_cursor_t x11Cursor(CursorShape shape);
    /**
     * Notice: if available always use the CursorShape variant to avoid cache duplicates for
     * ambiguous cursor names in the non existing cursor name specification
     **/
    static xcb_cursor_t x11Cursor(const QByteArray &name);

Q_SIGNALS:
    void posChanged(QPoint pos);
    void mouseChanged(const QPoint& pos, const QPoint& oldpos,
                      Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                      Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    /**
     * @brief Signal emitted when the cursor image changes.
     *
     * To enable these signals use startCursorTracking.
     *
     * @see startCursorTracking
     * @see stopCursorTracking
     */
    void cursorChanged();
    void themeChanged();

protected:
    /**
     * Called from x11Cursor to actually retrieve the X11 cursor. Base implementation returns
     * a null cursor, an implementing subclass should implement this method if it can provide X11
     * mouse cursors.
     **/
    virtual xcb_cursor_t getX11Cursor(CursorShape shape);
    /**
     * Called from x11Cursor to actually retrieve the X11 cursor. Base implementation returns
     * a null cursor, an implementing subclass should implement this method if it can provide X11
     * mouse cursors.
     **/
    virtual xcb_cursor_t getX11Cursor(const QByteArray &name);
    /**
     * Performs the actual warping of the cursor.
     **/
    virtual void doSetPos();
    /**
     * Called from @ref pos() to allow syncing the internal position with the underlying
     * system's cursor position.
     **/
    virtual void doGetPos();
    /**
     * Called from startMousePolling when the mouse polling gets activated. Base implementation
     * does nothing, inheriting classes can overwrite to e.g. start a timer.
     **/
    virtual void doStartMousePolling();
    /**
     * Called from stopMousePolling when the mouse polling gets deactivated. Base implementation
     * does nothing, inheriting classes can overwrite to e.g. stop a timer.
     **/
    virtual void doStopMousePolling();
    /**
     * Called from startCursorTracking when cursor image tracking gets activated. Inheriting class needs
     * to overwrite to enable platform specific code for the tracking.
     */
    virtual void doStartCursorTracking();
    /**
     * Called from stopCursorTracking when cursor image tracking gets deactivated. Inheriting class needs
     * to overwrite to disable platform specific code for the tracking.
     */
    virtual void doStopCursorTracking();
    bool isCursorTracking() const;
    /**
     * Provides the actual internal cursor position to inheriting classes. If an inheriting class needs
     * access to the cursor position this method should be used instead of the static @ref pos, as
     * the static method syncs with the underlying system's cursor.
     **/
    const QPoint &currentPos() const;
    /**
     * Updates the internal position to @p pos without warping the pointer as
     * setPos does.
     **/
    void updatePos(const QPoint &pos);
    void updatePos(int x, int y);

private Q_SLOTS:
    void loadThemeSettings();
    void slotKGlobalSettingsNotifyChange(int type, int arg);

private:
    void updateTheme(const QString &name, int size);
    void loadThemeFromKConfig();
    QPoint m_pos;
    int m_mousePollingCounter;
    int m_cursorTrackingCounter;
    QString m_themeName;
    int m_themeSize;

    KWIN_SINGLETON(Cursor)
};

/**
 * @brief Implementation using the InputRedirection framework to get pointer positions.
 *
 * Does not support warping of cursor.
 *
 */
class InputRedirectionCursor : public Cursor
{
    Q_OBJECT
public:
    explicit InputRedirectionCursor(QObject *parent);
    virtual ~InputRedirectionCursor();
protected:
    virtual void doSetPos();
    virtual void doStartCursorTracking();
    virtual void doStopCursorTracking();
private Q_SLOTS:
    void slotPosChanged(const QPointF &pos);
    void slotPointerButtonChanged();
    void slotModifiersChanged(Qt::KeyboardModifiers mods, Qt::KeyboardModifiers oldMods);
private:
    Qt::MouseButtons m_currentButtons;
    friend class Cursor;
};

inline const QPoint &Cursor::currentPos() const
{
    return m_pos;
}

inline void Cursor::updatePos(int x, int y)
{
    updatePos(QPoint(x, y));
}

inline const QString& Cursor::themeName() const
{
    return m_themeName;
}

inline int Cursor::themeSize() const
{
    return m_themeSize;
}

inline bool Cursor::isCursorTracking() const
{
    return m_cursorTrackingCounter > 0;
}

}

Q_DECLARE_METATYPE(KWin::CursorShape)

#endif // KWIN_CURSOR_H
