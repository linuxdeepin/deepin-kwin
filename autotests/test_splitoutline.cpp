// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "testprintasanbase.h"
#include "../splitoutline.h"
using KWin::SplitOutline;

class SplitOutlineTest : public TestPrintAsanBase
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testEnabled();
    void testRequestEnabled_data();
    void testRequestEnabled();
};


void SplitOutlineTest::initTestCase()
{

}

void SplitOutlineTest::testEnabled()
{
    SplitOutline splitOutline();
}

void SplitOutlineTest::testRequestEnabled_data()
{

}

void SplitOutlineTest::testRequestEnabled()
{
    testPrintlog();
}

QTEST_MAIN(SplitOutlineTest)
#include "test_splitoutline.moc"
