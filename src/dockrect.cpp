#include "dockrect.h"

DockRect::DockRect()
    : x(0),
      y(0),
      w(0),
      h(0)
{
}

QDebug operator<<(QDebug debug, const DockRect &rect)
{
    debug << QString("ScreenRect(%1, %2, %3, %4)").arg(rect.x).arg(rect.y).arg(rect.w).arg(rect.h);
    return debug;
}

DockRect::operator QRect() const
{
    return QRect(x, y, w, h);
}

QDBusArgument &operator<<(QDBusArgument &arg, const DockRect &rect)
{
    arg.beginStructure();
    arg << rect.x << rect.y << rect.w << rect.h;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, DockRect &rect)
{
    arg.beginStructure();
    arg >> rect.x >> rect.y >> rect.w >> rect.h;
    arg.endStructure();
    return arg;
}

namespace KWin
{

DBusDock::DBusDock(QObject *parent)
    : QDBusAbstractInterface("com.deepin.dde.daemon.Dock", "/com/deepin/dde/daemon/Dock", staticInterfaceName(), QDBusConnection::sessionBus(), parent)
{
    qRegisterMetaType<DockRect>("DockRect");
    qDBusRegisterMetaType<DockRect>();
    QDBusConnection::sessionBus().connect(this->service(), this->path(), "org.freedesktop.DBus.Properties",  "PropertiesChanged","sa{sv}as", this, SLOT(__propertyChanged__(QDBusMessage)));
}

DBusDock::~DBusDock()
{
    QDBusConnection::sessionBus().disconnect(service(), path(), "org.freedesktop.DBus.Properties",  "PropertiesChanged",  "sa{sv}as", this, SLOT(propertyChanged(QDBusMessage)));
}

}
