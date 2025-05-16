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
    QByteArray logRules = qgetenv("QT_LOGGING_RULES");
    qunsetenv("QT_LOGGING_RULES");

    // set env
    setLoggingRules(logRules);
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

void Logger::setLoggingRules(const QString &rules)
{
    if (!rules.isEmpty()) {
        QString tmpRules = rules;
        m_rules = tmpRules.replace(";", "\n");
    }
    qCInfo(KWIN_LOGGER) << "Setting logging rules: " << m_rules;
    QLoggingCategory::setFilterRules(m_rules);
}

void Logger::appendRules(const QString &rules)
{
    // Convert rules string to QMap for processing
    QMap<QString, QString> rulesMap;

    // Process existing rules
    if (!m_rules.isEmpty()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        const QStringList currentRules = m_rules.split('\n', QString::SkipEmptyParts);
#else
        const QStringList currentRules = m_rules.split('\n', Qt::SkipEmptyParts);
#endif
        for (const QString &rule : currentRules) {
            const int equalPos = rule.indexOf('=');
            if (equalPos > 0) {
                QString key = rule.left(equalPos).trimmed();
                QString value = rule.mid(equalPos + 1).trimmed();
                rulesMap[key] = value;
            }
        }
    }

    // Process new rules
    QString tmpRules = rules;
    tmpRules.replace(";", "\n");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const QStringList newRules = tmpRules.split('\n', QString::SkipEmptyParts);
#else
    const QStringList newRules = tmpRules.split('\n', Qt::SkipEmptyParts);
#endif
    bool hasNewRules = false;

    for (const QString &rule : newRules) {
        const int equalPos = rule.indexOf('=');
        if (equalPos > 0) {
            QString key = rule.left(equalPos).trimmed();
            QString value = rule.mid(equalPos + 1).trimmed();

            // Update if it's a new rule or the value has changed
            if (!rulesMap.contains(key) || rulesMap[key] != value) {
                rulesMap[key] = value;
                hasNewRules = true;
            }
        }
    }

    if (!hasNewRules) {
        return;
    }

    // Convert QMap back to rules string
    QStringList resultRules;
    for (auto it = rulesMap.constBegin(); it != rulesMap.constEnd(); ++it) {
        resultRules.append(QString("%1=%2").arg(it.key(), it.value()));
    }

    m_rules = resultRules.join("\n");
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
            appendRules(reply.value().toString());
            setLoggingRules();
        }
        watcher->deleteLater();
    });
}

}
