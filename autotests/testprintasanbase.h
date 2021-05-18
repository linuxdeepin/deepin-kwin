#ifndef TESTPRINTASANBASE_H
#define TESTPRINTASANBASE_H

#include <QObject>
#if defined(CMAKE_SAFETYTEST_ARG_ON)
#include <sanitizer/asan_interface.h>
#endif
class TestPrintAsanBase : public QObject
{
    Q_OBJECT
public:
    explicit TestPrintAsanBase(QObject *parent = nullptr);
    ~TestPrintAsanBase();

signals:

public Q_SLOTS:
    virtual void testPrintlog();
};

#endif // TESTPRINTASANBASE_H
