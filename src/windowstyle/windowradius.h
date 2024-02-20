#ifndef WINDOWRADIUS_H
#define WINDOWRADIUS_H

#include <QObject>
#include <QPointF>

namespace KWin
{
class Window;

class WindowRadius : public QObject
{
    Q_OBJECT

public:
    explicit WindowRadius(Window *window);
    ~WindowRadius();

    int updateWindowRadius();

    QPointF getWindowRadius();
    QPointF windowRadius() { return m_radius;};
    QString theme() const;

Q_SIGNALS:
    void windowRadiusChanged();
    void validPropertiesChanged(qint64 validProperties);
    void themeChanged();

public Q_SLOTS:
    void onUpdateWindowRadiusChanged();

public:
    Window *m_window;
    quint32 m_atom_deepin_scissor_window;
    QPointF m_radius = QPointF(-1, 0);
    bool    m_isMaximized = false;
    float   m_scale = 1.0;
};
}

#endif
