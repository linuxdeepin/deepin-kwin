#ifndef TESTPRINTASANBASE_H
#define TESTPRINTASANBASE_H

#include <QObject>
#ifdef NOTSUPPORTASAN
/*nothing*/
#else
#if defined(CMAKE_SAFETYTEST_ARG_ON)
#include <sanitizer/asan_interface.h>
#endif
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
