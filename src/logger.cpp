#include "logger.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QLoggingCategory>

#include <systemd/sd-journal.h>

#define CONFIGMANAGER_SERVICE           "org.desktopspec.ConfigManager"
#define CONFIGMANAGER_INTERFACE         "org.desktopspec.ConfigManager"
#define CONFIGMANAGER_MANGER_INTERFACE  "org.desktopspec.ConfigManager.Manager"

Q_LOGGING_CATEGORY(KWIN_LOGGER, "kwin_logger")

namespace KWin
{

static void qtLoggerMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString &msg)
{
    int logLevel = LOG_INFO;
    switch (type) {
    case QtDebugMsg:
        logLevel = LOG_DEBUG;
        break;
    case QtInfoMsg:
        logLevel = LOG_INFO;
        break;
    case QtWarningMsg:
        logLevel = LOG_WARNING;
        break;
    case QtCriticalMsg:
        logLevel = LOG_ERR;
        break;
    case QtFatalMsg:
        logLevel = LOG_CRIT;
        break;
    default:
        logLevel = LOG_INFO;
        break;
    }

    sd_journal_send("MESSAGE=%s",
                    msg.toStdString().c_str(),
                    "PRIORITY=%d",
                    logLevel,
                    "CODE_FILE=%s",
                    context.file,
                    "CODE_LINE=%d",
                    context.line,
                    "CODE_FUNC=%s",
                    context.function,
                    "CODE_CATEGORY=%s",
                    context.category,
                    NULL);
}

Logger::Logger(QObject *parent)
    : QObject(parent)
{
    m_envRules = QString::fromLocal8Bit(qgetenv("QT_LOGGING_RULES"));
    m_envRules.replace(";", "\n");
    qunsetenv("QT_LOGGING_RULES");

    // set env
    setLoggingRules();
    initDBus();
}

Logger *Logger::instance()
{
    static Logger logger;
    return &logger;
}

void Logger::installMessageHandler()
{
    qInstallMessageHandler(qtLoggerMessageHandler);
}

void Logger::setLoggingRules()
{
    // env rules first, dconfig rules second — Qt filter rules are
    // last-wins, so dconfig values take precedence over the env.
    QString rules = m_envRules;
    if (!m_dconfigRules.isEmpty()) {
        if (!rules.isEmpty()) {
            rules += QLatin1Char('\n');
        }
        rules += m_dconfigRules;
    }
    qCInfo(KWIN_LOGGER) << "Setting logging rules: " << rules;
    QLoggingCategory::setFilterRules(rules);
}

void Logger::initDBus()
{
    auto qdbus = QDBusConnection::systemBus();

    // set dconfig asynchronously
    QDBusInterface interfaceRequire(CONFIGMANAGER_SERVICE, "/", CONFIGMANAGER_INTERFACE, qdbus);
    QDBusMessage acquireCall = QDBusMessage::createMethodCall(CONFIGMANAGER_SERVICE,
                                                              "/",
                                                              CONFIGMANAGER_INTERFACE,
                                                              "acquireManager");
    acquireCall << "org.kde.kwin" << "org.kde.kwin.logging" << "";

    QDBusPendingCall acquirePending = qdbus.asyncCall(acquireCall);
    QDBusPendingCallWatcher *acquireWatcher = new QDBusPendingCallWatcher(acquirePending, this);

    connect(acquireWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qCWarning(KWIN_LOGGER) << "Failed to acquire ConfigManager.Manager interface: " << reply.error().message();
            return;
        }
        if (reply.isValid()) {
            m_configManagerPath = reply.value().path();
            qCInfo(KWIN_LOGGER) << "Get ConfigManager.Manager value: " << m_configManagerPath;
            setLogRulesFromDBus();
            auto ret = QDBusConnection::systemBus().connect(CONFIGMANAGER_SERVICE,
                                                            m_configManagerPath,
                                                            CONFIGMANAGER_MANGER_INTERFACE,
                                                            "valueChanged",
                                                            "s",
                                                            this,
                                                            SLOT(onValueChanged(QString)));
            if (!ret) {
                qCWarning(KWIN_LOGGER) << "Failed to connect valueChanged signal: " << QDBusConnection::systemBus().lastError().message();
            }
        }
        watcher->deleteLater();
    });
}

void Logger::onValueChanged(const QString &msg)
{
    qCInfo(KWIN_LOGGER) << "dsg value changed: " << msg;
    if (msg == "log_rules") {
        setLogRulesFromDBus();
    }
}

void Logger::setLogRulesFromDBus()
{
    if (m_configManagerPath.isEmpty())
        return;
    auto qdbus = QDBusConnection::systemBus();
    QDBusMessage valueCall = QDBusMessage::createMethodCall(CONFIGMANAGER_SERVICE,
                                                            m_configManagerPath,
                                                            CONFIGMANAGER_MANGER_INTERFACE,
                                                            "value");
    valueCall << "log_rules";

    QDBusPendingCall valuePending = qdbus.asyncCall(valueCall);
    QDBusPendingCallWatcher *valueWatcher = new QDBusPendingCallWatcher(valuePending, this);

    connect(valueWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QVariant> reply = *watcher;
        if (reply.isError()) {
            qCWarning(KWIN_LOGGER) << "Failed to get log_rules: " << reply.error().message();
            return;
        }
        if (reply.isValid()) {
            qCInfo(KWIN_LOGGER) << "Get log_rules value from dconfig: " << reply.value().toString();
            m_dconfigRules = reply.value().toString();
            m_dconfigRules.replace(";", "\n");
            setLoggingRules();
        }
        watcher->deleteLater();
    });
}

}
