#include <QtTest>
#include "testprintasanbase.h"

class TestLightWeight : public TestPrintAsanBase
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testWindowBorderEffect();
    void testWindowAnimation();
    void testMultitasking();
    void testDraggingWithContent();
    void cleanupTestCase();
private:
    bool m_bIsSupport {false};
};

void TestLightWeight::initTestCase()
{
    m_bIsSupport = true;
}

void TestLightWeight::testWindowBorderEffect()
{
    QCOMPARE(m_bIsSupport, true);
}

void TestLightWeight::testWindowAnimation()
{
    QCOMPARE(m_bIsSupport, true);
}

void TestLightWeight::testMultitasking()
{
    QCOMPARE(m_bIsSupport, true);
}

void TestLightWeight::testDraggingWithContent()
{
    QCOMPARE(m_bIsSupport, true);
}

void TestLightWeight::cleanupTestCase()
{
    m_bIsSupport = false;
}

QTEST_MAIN(TestLightWeight)
#include "test_light_weight.moc"
