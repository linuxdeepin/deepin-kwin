#pragma once

#include <QDBusMessage>
#include <QMutex>
#include <kwinglobals.h>

#define LOG_EMERG   0   /* system is unusable */
#define LOG_ALERT   1   /* action must be taken immediately */
#define LOG_CRIT    2   /* critical conditions */
#define LOG_ERR     3   /* error conditions */
#define LOG_WARNING 4   /* warning conditions */
#define LOG_NOTICE  5   /* normal but significant condition */
#define LOG_INFO    6   /* informational */
#define LOG_DEBUG   7   /* debug-level messages */

namespace KWin
{

class KWIN_EXPORT Logger : public QObject
{
    Q_OBJECT
public:
    static Logger *instance();

    void installMessageHandler();

private Q_SLOTS:
    void onValueChanged(const QString &msg);

private:
    Logger(QObject *parent = nullptr);
    ~Logger() = default;

    void setLoggingRules(const QString &rules = QString());

    void appendRules(const QString &rules);

    void initDBus();
    void setLogRulesFromDBus();

    QMutex m_mutex;
    QString m_rules;
    QString m_configManagerPath;
};

}
