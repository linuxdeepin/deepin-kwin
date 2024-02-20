#ifndef WINDOWSTYLEMANAGER_H
#define WINDOWSTYLEMANAGER_H

#include <QObject>

namespace KWin
{
class Window;
class Unmanaged;
class ConfigReader;

class WindowStyleManager : public QObject
{
    Q_OBJECT
public:
    explicit WindowStyleManager();
    ~WindowStyleManager();

public:
    float getOsRadius();
    float getOsScale();

Q_SIGNALS:
    void sigRadiusChanged(float &);

public Q_SLOTS:
    void onRadiusChange(QVariant);
    void onWindowAdded(Window*);
    void onWindowMaxiChanged(Window *, bool, bool);
    void onWindowActiveChanged();
    void onGeometryShapeChanged(Window *, QRectF);
    void onCompositingChanged(bool);

private:
    ConfigReader *m_configReader = nullptr;
    float        m_osRadius = -1.0;
    float        m_scale = 1.0;
};
}

#endif