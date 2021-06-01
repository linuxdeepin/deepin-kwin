#include "testprintasanbase.h"

TestPrintAsanBase::TestPrintAsanBase(QObject *parent) : QObject(parent)
{

}

TestPrintAsanBase::~TestPrintAsanBase()
{

}

void TestPrintAsanBase::testPrintlog()
{
#ifdef NOTSUPPORTASAN
    /*nothing*/
#else
#if defined(CMAKE_SAFETYTEST_ARG_ON)
    __sanitizer_set_report_path("asan.log");
#endif
#endif
}
