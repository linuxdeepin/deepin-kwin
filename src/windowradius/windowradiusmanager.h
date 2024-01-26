#ifndef WINDOWRADIUSMANAGER_H
#define WINDOWRADIUSMANAGER_H

#include <QObject>

namespace KWin
{
class Window;
class Unmanaged;
class ConfigReader;

class WindowRadiusManager : public QObject
{
    Q_OBJECT
public:
    explicit WindowRadiusManager(/* args */);
    ~WindowRadiusManager();

public:
    float getOsRadius();
    float getOsScale();

Q_SIGNALS:
    void sigRadiusChanged(float &);

public Q_SLOTS:
    void onRadiusChange(QVariant);
    void onWindowAdded(Window*);
    void onWindowMaxiChanged(Window *, bool, bool);

private:
    ConfigReader *m_configReader = nullptr;
    float        m_radius = -1.0;
    float        m_scale = 1.0;
};
}

#endif