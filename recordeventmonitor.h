#ifndef RECORDEVENTMONITOR_H
#define RECORDEVENTMONITOR_H

#include <QObject>
#include <QThread>
#include <X11/Xlib.h>
#include <X11/extensions/record.h>

class RecordEventMonitor : public QThread
{
    Q_OBJECT
public:
    explicit RecordEventMonitor(QObject *parent = nullptr);

signals:
    void touchDown();
    void touchMotion();
    void touchUp();

public slots:

protected:
    static void callback(XPointer trash, XRecordInterceptData *data);
    void handleRecordEvent(XRecordInterceptData *);
    void run();
};

#endif // RECORDEVENTMONITOR_H
