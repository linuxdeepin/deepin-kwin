#ifndef DOCKRECT_H
#define DOCKRECT_H

#include <QRect>
#include <QDBusArgument>
#include <QDebug>
#include <QDBusMetaType>
#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

struct DockRect
{
public:
    DockRect();
    operator QRect() const;

    friend QDebug operator<<(QDebug debug, const DockRect &rect);
    friend const QDBusArgument &operator>>(const QDBusArgument &arg, DockRect &rect);
    friend QDBusArgument &operator<<(QDBusArgument &arg, const DockRect &rect);

private:
    qint32 x;
    qint32 y;
    quint32 w;
    quint32 h;
};

Q_DECLARE_METATYPE(DockRect)

namespace KWin
{

class DBusDock: public QDBusAbstractInterface
{
    Q_OBJECT

    Q_SLOT void __propertyChanged__(const QDBusMessage& msg)
    {
        QList<QVariant> arguments = msg.arguments();
        if (3 != arguments.count())
            return;
        QString interfaceName = msg.arguments().at(0).toString();
        if (interfaceName !="com.deepin.dde.daemon.Dock")
            return;
        QVariantMap changedProps = qdbus_cast<QVariantMap>(arguments.at(1).value<QDBusArgument>());
        for(const QString &prop : changedProps.keys()) {
        const QMetaObject* self = metaObject();
            for (int i=self->propertyOffset(); i < self->propertyCount(); ++i) {
                QMetaProperty p = self->property(i);
                if (p.name() == prop) {
                    Q_EMIT p.notifySignal().invoke(this);
                }
            }
        }
   }
public:
    static inline const char *staticInterfaceName()
    {
        return "com.deepin.dde.daemon.Dock";
    }

public:
    explicit DBusDock(QObject *parent = 0);

    ~DBusDock();

    Q_PROPERTY(int Position READ position NOTIFY PositionChanged)
    inline int position() const
    {
        return int(qvariant_cast< int >(property("Position")));
    }

    Q_PROPERTY(int HideState READ hideState NOTIFY HideStateChanged)
    inline int hideState() const
    {
        return int(qvariant_cast< int >(property("HideState")));
    }

    Q_PROPERTY(DockRect FrontendWindowRect READ frontendRect NOTIFY FrontendRectChanged)
    inline DockRect frontendRect() const
    {
        return qvariant_cast< DockRect >(property("FrontendWindowRect"));
    }

Q_SIGNALS: // SIGNALS
    void FrontendRectChanged();
    void HideStateChanged();
    void PositionChanged();
};

}

#endif // DOCKRECT_H
