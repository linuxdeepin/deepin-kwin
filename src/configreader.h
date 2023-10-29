#ifndef KWIN_CONFIGREADER_H
#define KWIN_CONFIGREADER_H

#include <kwinglobals.h>
#include <QDBusInterface>
#include <QDBusConnectionInterface>
#include <QDBusArgument>
#include <QThread>
#include <QDebug>

#include "wayland_server.h"

namespace KWin
{
class ConfigReaderThread : public QThread
{
    Q_OBJECT
public:
    ConfigReaderThread(const QString &service, const QString &path, const QString &interface, const QString &propertyName);
    ~ConfigReaderThread();

Q_SIGNALS:
    void propertyFound(QVariant);

protected:
    void run() override;

private:
    QString m_service;
    QString m_path;
    QString m_interface;
    QString m_propertyName;
};


class KWIN_EXPORT ConfigReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant m_property READ getProperty WRITE setProperty)
public:
    ConfigReader(const QString &service, const QString &path, const QString &interface, const QString &propertyName);
    ~ConfigReader();

    QVariant getProperty() const {
        return m_property;
    }

    void setProperty(const QVariant &property) {
        m_property = property;
    }

public Q_SLOTS:
    void slotUpdateProperty(QDBusMessage msg);
    void slotSetProperty(QVariant property);

private:
    QString m_interface;
    QString m_propertyName;
    QVariant m_property;
    ConfigReaderThread *m_thread = nullptr;
};

}

#endif