#include "configreader.h"

#define MAXTIMES 256
namespace KWin
{

ConfigReaderThread::ConfigReaderThread(const QString &service, const QString &path, const QString &interface, const QString &propertyName)
    : m_service(service), m_path(path), m_interface(interface), m_propertyName(propertyName)
{
    connect(this, &ConfigReaderThread::finished, this, [=](){
        this->deleteLater();
    });
}

ConfigReaderThread::~ConfigReaderThread()
{
    requestInterruption();
    wait();
}

void ConfigReaderThread::run()
{
    bool found = false;
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    for (int i = 1; i < MAXTIMES; i *= 2) {
        if (isInterruptionRequested())
            return;
        auto sleepfunc = [&i]{
            QThread::sleep(i);
        };
        if (!sessionBus.interface()->isServiceRegistered(m_service)) {
            sleepfunc();
            continue;
        }
        QDBusInterface dbus(m_service, m_path, m_interface);
        if (!dbus.isValid()) {
            sleepfunc();
            continue;
        }
        QVariant property = dbus.property(m_propertyName.toStdString().c_str());

        if (property.isValid()) {
            Q_EMIT propertyFound(property);
            found = true;
            break;
        }

        sleepfunc();
    }

    if (!found) {
        qWarning() << "cannot get dbus property";
    }
}

ConfigReader::ConfigReader(const QString &service, const QString &path, const QString &interface, const QString &propertyName)
    : m_interface(interface), m_propertyName(propertyName)
{
    m_thread = new ConfigReaderThread(service, path, interface, propertyName);
    connect(m_thread, SIGNAL(propertyFound(QVariant)), this, SLOT(slotSetProperty(QVariant)));

    m_thread->start();

    QDBusConnection::sessionBus().connect(service, path, "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                          "sa{sv}as", this, SLOT(slotUpdateProperty(QDBusMessage)));
}

ConfigReader::~ConfigReader()
{
    if (m_thread) {
        m_thread->requestInterruption();
        m_thread->wait();
        m_thread = nullptr;
    }
}

void ConfigReader::slotUpdateProperty(QDBusMessage msg)
{
    QList<QVariant> arguments = msg.arguments();
    if (3 != arguments.count()) {
        return;
    }

    QString interfaceName = msg.arguments().at(0).toString();
    if (interfaceName == m_interface) {
        QVariantMap changedProps = qdbus_cast<QVariantMap>(arguments.at(1).value<QDBusArgument>());
        QStringList keys = changedProps.keys();
        for (int i = 0; i < keys.size(); ++i) {
            if (keys.at(i) == m_propertyName) {
                m_property = changedProps.value(keys.at(i));
                break;
            }
        }
    }
}

void ConfigReader::slotSetProperty(QVariant property)
{
    m_property = property;
}

}
