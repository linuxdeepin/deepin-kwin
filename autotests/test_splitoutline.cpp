/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
     SplitOutline splitOutline;
//     QFETCH(QRect,rect);
     QRect rect;
     splitOutline.setSplitOutlineRect(rect);
    //  QCOMPARE(splitOutline.getLeftSplitClientRect(),rect);
    //  QFETCH(QRect,rect1);
    //  splitOutline.setSplitOutlineRect(rect1);
    //  QCOMPARE(splitOutline.getRightSplitClientRect(),rect1);
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
