/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "splitmanage.h"
#include "workspace.h"
#include "screens.h"
#include "abstract_output.h"

#define SPLITOUTLINE_WIDTH 14
namespace KWin {

void SplitGroup::clearGroup(AbstractClient *client, QString cscreen)
{
    QList<EffectScreen *> list = effects->screens();
    for (int i = 0; i < list.size(); i++) {
        QString screen = list[i]->name();
        bool isClearLocation = false;
        if (m_activeSplitList.contains(screen) && m_activeSplitList[screen].contains(client)) {
            m_activeSplitList[screen].remove(client);
            isClearLocation = true;
        }

        if ((m_leftTopList.contains(screen) && m_leftTopList[screen].contains(client))
            || (m_leftBottomList.contains(screen) && m_leftBottomList[screen].contains(client))) {
            if (m_leftTopList[screen].removeOne(client) && isClearLocation) {
                m_splitWinLocation[screen] &= ~SplitLocationMode(SplitLocation::leftTop);
            }
            if (m_leftBottomList[screen].removeOne(client) && isClearLocation) {
                m_splitWinLocation[screen] &= ~SplitLocationMode(SplitLocation::leftBottom);
            }
        } else if ((m_rightTopList.contains(screen) && m_rightTopList[screen].contains(client))
            || (m_rightBottomList.contains(screen) && m_rightBottomList[screen].contains(client))) {
            if (m_rightTopList[screen].removeOne(client) && isClearLocation) {
                m_splitWinLocation[screen] &= ~SplitLocationMode(SplitLocation::rightTop);
            }
            if (m_rightBottomList[screen].removeOne(client) && isClearLocation) {
                m_splitWinLocation[screen] &= ~SplitLocationMode(SplitLocation::rightBottom);
            }
        }
    }
}

void SplitGroup::clearCustomGroup(QString screen)
{
    m_tmpSplitList[screen].clear();
    m_activeSplitList[screen].clear();
    m_splitWinLocation[screen] = SplitLocation::None;
}

void SplitGroup::cacheGroup(AbstractClient *client, QString screen)
{
    if (screen.isEmpty())
        screen = client->output()->name();
    if (client->quickTileMode() & QuickTileFlag::Left) {
        if (client->quickTileMode() & QuickTileFlag::Top) {
            m_leftTopList[screen].push_back(client);
        } else if (client->quickTileMode() & QuickTileFlag::Bottom) {
            m_leftBottomList[screen].push_back(client);
        } else {
            m_leftTopList[screen].push_back(client);
            m_leftBottomList[screen].push_back(client);
        }
    } else if (client->quickTileMode() & QuickTileFlag::Right) {
        if (client->quickTileMode() & QuickTileFlag::Top) {
            m_rightTopList[screen].push_back(client);
        } else if (client->quickTileMode() & QuickTileFlag::Bottom) {
            m_rightBottomList[screen].push_back(client);
        } else {
            m_rightTopList[screen].push_back(client);
            m_rightBottomList[screen].push_back(client);
        }
    }
}

void SplitGroup::clearCustomClient(AbstractClient *client, SplitLocationMode mode)
{
    QString screen = client->output()->name();
    if (mode & SplitLocation::leftTop && m_leftTopList.contains(screen)) {
        m_leftTopList[screen].removeOne(client);
    }
    if (mode & SplitLocation::leftBottom && m_leftBottomList.contains(screen)) {
        m_leftBottomList[screen].removeOne(client);
    }

    if (mode & SplitLocation::rightTop && m_rightTopList.contains(screen)) {
        m_rightTopList[screen].removeOne(client);
    }
    if (mode & SplitLocation::rightBottom && m_rightBottomList.contains(screen)) {
        m_rightBottomList[screen].removeOne(client);
    }
}

void SplitGroup::cacheCustomClient(AbstractClient *client, SplitLocationMode mode)
{
    QString screen = client->output()->name();
    if (mode & SplitLocation::leftTop) {
        m_leftTopList[screen].push_back(client);
    }
    if (mode & SplitLocation::leftBottom) {
        m_leftBottomList[screen].push_back(client);
    }

    if (mode & SplitLocation::rightTop) {
        m_rightTopList[screen].push_back(client);
    }
    if (mode & SplitLocation::rightBottom) {
        m_rightBottomList[screen].push_back(client);
    }
}

AbstractClient *SplitGroup::getCacheClient(AbstractClient *client, QString screen, SplitLocationMode mode)
{
    if (screen.isEmpty())
        screen = client->output()->name();
    switch (mode)
    {
    case SplitLocationMode(SplitLocation::leftTop):
        if (m_leftTopList[screen].size() == 0)
            return nullptr;
        return m_leftTopList[screen][m_leftTopList[screen].size() - 1];
    case SplitLocationMode(SplitLocation::leftBottom):
        if (m_leftBottomList[screen].size() == 0)
            return nullptr;
        return m_leftBottomList[screen][m_leftBottomList[screen].size() - 1];
    case SplitLocationMode(SplitLocation::rightTop):
        if (m_rightTopList[screen].size() == 0)
            return nullptr;
        return m_rightTopList[screen][m_rightTopList[screen].size() - 1];
    case SplitLocationMode(SplitLocation::rightBottom):
        if (m_rightBottomList[screen].size() == 0)
            return nullptr;
        return m_rightBottomList[screen][m_rightBottomList[screen].size() - 1];
    default:
        break;
    }
    return nullptr;
}

void SplitGroup::cacheActiveClient(AbstractClient *client, QString screen)
{
    if (screen.isEmpty())
        screen = client->output()->name();
    m_activeSplitList[screen].insert(client);
}

void SplitGroup::cacheTmpSplitClient(AbstractClient *client, QString screen)
{
    if (screen.isEmpty())
        screen = client->output()->name();
    m_tmpSplitList[screen].insert(client);
}

void SplitGroup::cacheStockSplitClient(QString screen)
{
    m_stockSplitList[screen] = m_activeSplitList[screen];
}

void SplitGroup::updateStockSplitClient(AbstractClient *client)
{
    QList<EffectScreen *> list = effects->screens();
    for (int i = 0; i < list.size(); i++) {
        QString screen = list[i]->name();
        if (m_stockSplitList[screen].contains(client)) {
            m_stockSplitList[screen].remove(client);
        }
    }
}

void SplitGroup::setSplitLocation(AbstractClient *client, QString screen, bool isNormal, bool isClear)
{
    QuickTileMode m = client->quickTileMode();
    if (screen.isEmpty())
        screen = client->output()->name();
    SplitLocationMode location = SplitLocation::None;
    if (isNormal)
        location = m_splitWinLocation[screen];
    else
        location = m_tmpsplitWinLocation[screen];
    if (m & QuickTileFlag::Left) {
        if (m & QuickTileFlag::Top) {
            location = isClear ? location & ~SplitLocationMode(SplitLocation::leftTop)
                                : location | SplitLocation::leftTop;
        } else if (m & QuickTileFlag::Bottom) {
            location = isClear ? location & ~SplitLocationMode(SplitLocation::leftBottom)
                                : location | SplitLocation::leftBottom;
        } else {
            location = isClear ? location & ~SplitLocationMode(SplitLocation::leftTop)
                                : location | SplitLocation::leftTop;
            location = isClear ? location & ~SplitLocationMode(SplitLocation::leftBottom)
                                : location | SplitLocation::leftBottom;
        }
    } else if (m & QuickTileFlag::Right) {
        if (m & QuickTileFlag::Top) {
            location = isClear ? location & ~SplitLocationMode(SplitLocation::rightTop)
                                : location | SplitLocation::rightTop;
        } else if (m & QuickTileFlag::Bottom) {
            location = isClear ? location & ~SplitLocationMode(SplitLocation::rightBottom)
                                : location | SplitLocation::rightBottom;
        } else {
            location = isClear ? location & ~SplitLocationMode(SplitLocation::rightTop)
                                : location | SplitLocation::rightTop;
            location = isClear ? location & ~SplitLocationMode(SplitLocation::rightBottom)
                                : location | SplitLocation::rightBottom;
        }
    }
    if (isNormal)
        m_splitWinLocation[screen] = location;
    else
        m_tmpsplitWinLocation[screen] = location;
}

void SplitGroup::setCustomLocation(QString screen, SplitLocationMode mode)
{
    m_splitWinLocation[screen] |= mode;
}

QSet<AbstractClient *> SplitGroup::getActiveList(QString screen, bool isTmp)
{
    QSet<AbstractClient *> list;
    if (isTmp && m_tmpSplitList.contains(screen))
        return m_tmpSplitList[screen];
    else if (!isTmp && m_activeSplitList.contains(screen))
        return m_activeSplitList[screen];
    return list;
}

QSet<AbstractClient *> SplitGroup::getStockList(QString screen)
{
    return m_stockSplitList[screen];
}

void SplitGroup::updateTmpLocation(QString screen)
{
    m_tmpsplitWinLocation[screen] = m_splitWinLocation[screen];
}

SplitLocationMode SplitGroup::getNormalLocation(QString screen)
{
    return m_splitWinLocation[screen];
}

SplitLocationMode SplitGroup::getTempLocation(QString screen)
{
    if (m_tmpsplitWinLocation.contains(screen))
        return m_tmpsplitWinLocation[screen];
    return 0;
}

void SplitGroup::setSplitTmpList(AbstractClient *client, QString screen)
{
    m_tmpSplitList[screen].insert(client);
}

bool SplitGroup::isManage(AbstractClient *client, QString screen)
{
    if ((m_leftTopList.contains(screen) && m_leftTopList[screen].contains(client)) ||
        (m_leftBottomList.contains(screen) && m_leftBottomList[screen].contains(client)) ||
        (m_rightTopList.contains(screen) && m_rightTopList[screen].contains(client)) ||
        (m_rightBottomList.contains(screen) && m_rightBottomList[screen].contains(client)))
        return true;
    return false;
}

bool SplitGroup::isManageActive(AbstractClient *client)
{
    return m_activeSplitList[client->output()->name()].contains(client);
}

void SplitGroup::clearLocation(QString screen, bool isNormal)
{
    if (isNormal) {
        m_splitWinLocation[screen] = SplitLocationMode(SplitLocation::None);
    } else {
        m_tmpsplitWinLocation[screen] = SplitLocationMode(SplitLocation::None);
    }
}

void SplitGroup::setShowRectW(QString screen, int width, bool horizontal)
{
    if (horizontal)
        m_hRect[screen].setWidth(width);
    else
        m_vRect[screen].setWidth(width);
}

void SplitGroup::setShowRectH(QString screen, int height, bool horizontal)
{
    if (horizontal)
        m_hRect[screen].setHeight(height);
    else
        m_vRect[screen].setHeight(height);
}

void SplitGroup::setShowRect(QString screen, QRect rect, bool horizontal)
{
    if (horizontal)
        m_hRect[screen] = rect;
    else
        m_vRect[screen] = rect;
}

QRect SplitGroup::getShowRect(QString screen, bool horizontal)
{
    if (horizontal && m_hRect.contains(screen))
        return m_hRect[screen];
    else if (m_vRect.contains(screen))
        return m_vRect[screen];
    return QRect();
}

void SplitGroup::getHVArea(QString screen, QRect &hrect, QRect &vrect)
{
    if (m_hRect.contains(screen))
        hrect = m_hRect[screen];
    if (m_vRect.contains(screen))
        vrect = m_vRect[screen];
}

bool SplitGroup::getSplitLineState(QString screen)
{
    if (m_splitLineState.contains(screen))
        return m_splitLineState[screen];
    return false;
}

void SplitGroup::setSplitLineState(QString screen, bool flag)
{
    m_splitLineState[screen] = flag;
}

void SplitGroup::setSplitScreenMode(QString screen, int mode)
{
    m_splitMode[screen] = mode;
}

int SplitGroup::getSplitScreenMode(QString screen)
{
    if (m_splitMode.contains(screen))
        return m_splitMode[screen];
    return 0;
}

QList<AbstractClient *> SplitGroup::getCacheList(QString screen)
{
    QList<AbstractClient *> list;
    if (m_leftTopList.contains(screen)) {
        list += m_leftTopList[screen];
    }
    if (m_leftBottomList.contains(screen)) {
        list += m_leftBottomList[screen];
    }
    if (m_rightTopList.contains(screen)) {
        list += m_rightTopList[screen];
    }
    if (m_rightBottomList.contains(screen)) {
        list += m_rightBottomList[screen];
    }
    return list;
}

/*****************************************************/

SplitManage *SplitManage::_instance = nullptr;
SplitManage *SplitManage::instance()
{
    if (_instance == nullptr) {
        _instance = new SplitManage();
    }
    return _instance;
}

SplitManage::SplitManage()
{
}

SplitManage::~SplitManage()
{
}

SplitGroup *SplitManage::getObj(int desktop)
{
    if (m_splitGroupManage.contains(desktop))
        return m_splitGroupManage[desktop];
    return nullptr;
}

bool SplitManage::isExtensionMode() const
{
    if (screens()->count() >= 2) {
        QRect rect = Workspace::self()->clientArea(FullScreenArea, 0, effects->currentDesktop());
        if (screens()->geometry() != rect) {
            return true;
        }
    }
    return false;
}

int SplitManage::getNumScreens()
{
    int num = 1;
    if (isExtensionMode()) {
        num = screens()->count();
    } else if (screens()->count() <= 0) {
        num = 0;
    }
    return num;
}

bool SplitManage::isManageClient(AbstractClient *client)
{
    QString screen = client->output()->name();
    if (getObj(client->desktop()) && getObj(client->desktop())->isManage(client, screen))
        return true;
    return false;
}

void SplitManage::getHVRect(QString screen, QRect &hRect, QRect &vRect)
{
    if (getObj(effects->currentDesktop())) {
        getObj(effects->currentDesktop())->getHVArea(screen, hRect, vRect);
    }
}

bool SplitManage::isShowSplitLine(QString screen)
{
    if (getObj(effects->currentDesktop()))
        return getObj(effects->currentDesktop())->getSplitLineState(screen);
    return false;
}

void SplitManage::setSplitLineStateEx(int desktop, QString screen, bool flag, bool isRecheck)
{
    if (!getObj(desktop)) {
        SplitGroup *sg = new SplitGroup();
        m_splitGroupManage[desktop] = sg;
    }
    if (!screen.isEmpty()) {
        if (isRecheck)
            getObj(desktop)->setSplitLineState(screen, !isHaveAboveWin(desktop, screen));
        else
            getObj(desktop)->setSplitLineState(screen, flag);
    } else {
        QList<EffectScreen *> list = effects->screens();
        for (int i = 0; i < list.size(); i++) {
            screen = list[i]->name();
            if (isRecheck)
                getObj(desktop)->setSplitLineState(screen, !isHaveAboveWin(desktop, screen));
            else
                getObj(desktop)->setSplitLineState(screen, flag);
        }
    }
}

bool SplitManage::isHaveAboveWin(int desktop, QString screen)
{
    QSet<AbstractClient *> list = getObj(desktop) ? getObj(desktop)->getActiveList(screen) : QSet<AbstractClient *>();
    QList<Toplevel *> stacklist = Workspace::self()->stackingOrder();
    for (int i = stacklist.size() - 1; i >=0 ; i--) {
        AbstractClient *c = qobject_cast<AbstractClient*>(stacklist[i]);
        if (!c || isSpecialWindowEx(c) || c->isMinimized() || c->desktop() != desktop || c->output()->name() != screen)
            continue;
        if (list.contains(c))
            return false;
        else if (c->keepAbove())
            return true;
    }
    return false;
}

void SplitManage::updateSplitRect(int desktop, QString screen, int pos, bool isTopDown)
{
    if (!getObj(desktop))
        return;

    QRect hrect = getObj(desktop)->getShowRect(screen);
    QRect vrect = getObj(desktop)->getShowRect(screen, false);
    QRect rect = workspace()->clientArea(MaximizeArea, Cursors::self()->mouse()->pos(), desktop);
    if (isTopDown) {
        hrect.moveTop(pos - 7);
        getObj(desktop)->setShowRect(screen, hrect);
        if (!vrect.isEmpty() && abs(rect.height() - vrect.height()) > 4) {
            if (vrect.y() == 0)
                getObj(desktop)->setShowRectH(screen, pos, false);
            else {
                vrect.setY(pos);
                getObj(desktop)->setShowRect(screen, vrect, false);
            }
        }
    } else {
        vrect.moveLeft(pos - 7);
        getObj(desktop)->setShowRect(screen, vrect, false);
        if (!hrect.isEmpty() &&
            abs(hrect.width() - rect.width()) > 4) {
            if (hrect.x() == 0)
                getObj(desktop)->setShowRectW(screen, pos);
            else {
                hrect.setX(pos);
                getObj(desktop)->setShowRect(screen, hrect);
            }
        }
    }
}

void SplitManage::checkSplitMode(int desktop, QString screen, SplitLocationMode location, QSet<AbstractClient *> &list)
{
    switch (list.size())
    {
    case 1:
        if (location == SplitLocationMode(SplitLocation::leftTop) ||
            location == SplitLocationMode(SplitLocation::leftBottom) ||
            location == SplitLocationMode(SplitLocation::rightTop) ||
            location == SplitLocationMode(SplitLocation::rightBottom))
            getObj(desktop)->setSplitScreenMode(screen, int(SplitMode::Four));
        else
            getObj(desktop)->setSplitScreenMode(screen, int(SplitMode::Two));
        break;
    case 2:
        if (location == SplitLocationMode(SplitLocation::AllShow))
            getObj(desktop)->setSplitScreenMode(screen, int(SplitMode::Two));
        else {
            location = ~location;
            location &= 0xF;
            if (location == SplitLocationMode(SplitLocation::leftTop) ||
                location == SplitLocationMode(SplitLocation::leftBottom) ||
                location == SplitLocationMode(SplitLocation::rightTop) ||
                location == SplitLocationMode(SplitLocation::rightBottom))
                getObj(desktop)->setSplitScreenMode(screen, int(SplitMode::Three));
            else
                getObj(desktop)->setSplitScreenMode(screen, int(SplitMode::Four));
        }
        break;
    case 3:
        if (location == SplitLocationMode(SplitLocation::AllShow))
            getObj(desktop)->setSplitScreenMode(screen, int(SplitMode::Three));
        else
            getObj(desktop)->setSplitScreenMode(screen, int(SplitMode::Four));
        break;
    case 4:
        getObj(desktop)->setSplitScreenMode(screen, int(SplitMode::Four));
        break;
    default:
        break;
    }
}

QRect SplitManage::getQuickTileArea(AbstractClient *client, int desktop, QString screen, QuickTileMode mode, QRect rect, QRect availableArea, bool isTriggerSplit, bool isUseTmp)
{
    QSet<AbstractClient *> activeSplitList = getObj(desktop) ? getObj(desktop)->getActiveList(screen, isUseTmp) : QSet<AbstractClient *>();

    // single 1/2 split window resize height, triggle another window to one side
    if (isTriggerSplit && activeSplitList.size() == 1) {
        for (auto activeClient : activeSplitList) {
            QuickTileMode activeClientMode = activeClient->quickTileMode();
            if (activeClientMode & QuickTileFlag::Left && mode & QuickTileFlag::Left
                || (activeClientMode & QuickTileFlag::Right && mode & QuickTileFlag::Right)) {
                if ((activeClientMode & QuickTileFlag::Top && mode & QuickTileFlag::Bottom)
                    || (activeClientMode & QuickTileFlag::Bottom && mode & QuickTileFlag::Top)) {
                    break;
                } else {
                    return rect;
                }
            }
        }
    }
    int adjustWidth = rect.width();
    int adjustHeight = rect.height();
    for (auto activeClient : activeSplitList) {
        if (activeClient == client)
            continue;

        QuickTileMode activeClientMode = activeClient->quickTileMode();
        QRect activeClientRect = activeClient->moveResizeGeometry();
        int width = activeClientRect.width();
        int height = activeClientRect.height();
        if (activeClientMode == mode) {
            if (isTriggerSplit) {
                if ((activeClientMode == (QuickTileMode)QuickTileFlag::Left && mode == (QuickTileMode)QuickTileFlag::Left)
                || (activeClientMode == (QuickTileMode)QuickTileFlag::Right && mode == (QuickTileMode)QuickTileFlag::Right)) {
                    activeClientRect.setHeight(rect.height());
                }
            }
            return activeClientRect;
        }
        if (activeClientMode & QuickTileFlag::Left) {
            if (mode & QuickTileFlag::Left) {
                adjustWidth = width;
                adjustHeight = availableArea.height() - height - 2;
            } else if (mode & QuickTileFlag::Right) {
                if (activeClientMode & QuickTileFlag::Top) {
                    if (mode & QuickTileFlag::Top) {
                        adjustHeight = height;
                        adjustWidth = availableArea.width() - width - 2;
                    } else if (mode & QuickTileFlag::Bottom) {
                        adjustHeight = availableArea.height() - height - 2;
                        adjustWidth = availableArea.width() - width - 2;
                    } else {
                        adjustWidth = availableArea.width() - width - 2;
                    }
                } else if (activeClientMode & QuickTileFlag::Bottom) {
                    if (mode & QuickTileFlag::Top) {
                        adjustHeight = availableArea.height() - height - 2;
                        adjustWidth = availableArea.width() - width - 2;
                    } else if (mode & QuickTileFlag::Bottom) {
                        adjustHeight = height;
                        adjustWidth = availableArea.width() - width - 2;
                    } else {
                        adjustWidth = availableArea.width() - width - 2;
                    }
                } else {
                    adjustWidth = availableArea.width() - width - 2;
                }
            }

        } else if (activeClientMode & QuickTileFlag::Right) {
            if (mode & QuickTileFlag::Right) {
                adjustWidth = width;
                adjustHeight = availableArea.height() - height - 2;
            } else if (mode & QuickTileFlag::Left) {
                if (activeClientMode & QuickTileFlag::Top) {
                    if (mode & QuickTileFlag::Top) {
                        adjustHeight = height;
                        adjustWidth = availableArea.width() - width - 2;
                    } else if (mode & QuickTileFlag::Bottom) {
                        adjustHeight = availableArea.height() - height - 2;
                        adjustWidth = availableArea.width() - width - 2;
                    } else {
                        adjustWidth = availableArea.width() - width - 2;
                    }
                } else if (activeClientMode & QuickTileFlag::Bottom) {
                    if (mode & QuickTileFlag::Top) {
                        adjustHeight = availableArea.height() - height - 2;
                        adjustWidth = availableArea.width() - width - 2;
                    } else if (mode & QuickTileFlag::Bottom) {
                        adjustHeight = height;
                        adjustWidth = availableArea.width() - width - 2;
                    } else {
                        adjustWidth = availableArea.width() - width - 2;
                    }
                } else {
                    adjustWidth = availableArea.width() - width - 2;
                }
            }
        }
    }
    if (adjustWidth == rect.width() && adjustHeight == rect.height()) {
        return rect;
    }

    if (mode & QuickTileFlag::Left && mode & QuickTileFlag::Top) {
        rect = QRect(rect.x(), rect.y(), adjustWidth, adjustHeight);
    } else if (mode & QuickTileFlag::Left && mode & QuickTileFlag::Bottom) {
        rect = QRect(rect.x(), rect.y() + (rect.height() - adjustHeight), adjustWidth, adjustHeight);
    }else if (mode & QuickTileFlag::Right && mode & QuickTileFlag::Top) {
        rect = QRect(rect.x() + (rect.width() - adjustWidth), rect.y(), adjustWidth, adjustHeight);
    } else if (mode & QuickTileFlag::Right && mode & QuickTileFlag::Bottom) {
        rect = QRect(rect.x() + (rect.width() - adjustWidth), rect.y() + (rect.height() - adjustHeight), adjustWidth, adjustHeight);
    } else if (mode & QuickTileFlag::Left) {
        rect.setWidth(adjustWidth);
    } else if (mode & QuickTileFlag::Right) {
        rect = QRect(rect.x() + (rect.width() - adjustWidth), rect.y(), adjustWidth, rect.height());
    }
    return rect;
}

void SplitManage::clearSplitData(AbstractClient *client, bool isClearList, QString screen)
{
    if (screen.isEmpty())
        screen = client->output()->name();
    for (auto iter = m_splitGroupManage.begin(); iter != m_splitGroupManage.end(); iter++) {
        if (iter.value()) {
            iter.value()->clearGroup(client, screen);
        }
    }
    if (getObj(client->desktop()) && isClearList) {
        getObj(client->desktop())->clearCustomGroup(screen);
    }
}

void SplitManage::updateStockGroup(AbstractClient *client)
{
    for (auto iter = m_splitGroupManage.begin(); iter != m_splitGroupManage.end(); iter++) {
        if (iter.value()) {
            iter.value()->updateStockSplitClient(client);
        }
    }
    updateStockGroupEx(client->desktop(), client->output()->name());
}

void SplitManage::updateStockGroupEx(int desktop, QString screen)
{
    if (getObj(desktop)) {
        getObj(desktop)->cacheStockSplitClient(screen);
    }
}

void SplitManage::addWinSplit(AbstractClient *client, bool isQuickMatch)
{
    if (!client->isSplitWindow())
        return;

    m_screen = effects->screenAt(Cursors::self()->mouse()->pos())->name();

    clearSplitData(client, true, m_screen);
    cacheSplitWin(client, m_screen);
    createSplitGroup(client, m_screen);
    QSet<AbstractClient *> list;
    if (isQuickMatch) {
        checkOcclusion(client, m_screen);
        list = getObj(client->desktop()) ? getObj(client->desktop())->getActiveList(m_screen, true) : QSet<AbstractClient *>();
        SplitLocationMode location = getObj(client->desktop()) ? getObj(client->desktop())->getTempLocation(m_screen) : SplitLocation::None;
        checkSplitMode(client->desktop(), m_screen, location, list);
    }

    updateSplitOutlineRect(client->desktop(), m_screen);
    if (client->keepAbove())
        getObj(client->desktop())->setSplitLineState(m_screen, true);
    else
        getObj(client->desktop())->setSplitLineState(m_screen, !isHaveAboveWin(client->desktop(), m_screen));
}

void SplitManage::removeWinSplit(AbstractClient *client)
{
    if (!getObj(client->desktop()))
        return;
    updateCustomGroup(client);

    QSet<AbstractClient *> activeSplitList = getObj(client->desktop())->getActiveList(client->output()->name());
    AbstractClient *topClient = Workspace::self()->activeClient();
    if ((topClient && !activeSplitList.contains(topClient)) || activeSplitList.size() < 2)
        getObj(client->desktop())->setSplitLineState(client->output()->name(), false);
}

void SplitManage::updateSplitGroup(AbstractClient *client)
{
    QString screen = client->output()->name();
    if (!client->isSplitWindow()) {
        if (getObj(client->desktop()))
            getObj(client->desktop())->setSplitLineState(screen, false);
        return;
    }
    workspace()->setShowSplitLine(false, QRect());

    clearSplitData(client);
    cacheSplitWin(client);

    createSplitGroup(client);
    QSet<AbstractClient *> list = getObj(client->desktop()) ? getObj(client->desktop())->getActiveList(screen) : QSet<AbstractClient *>();
    SplitLocationMode location = getObj(client->desktop()) ? getObj(client->desktop())->getNormalLocation(screen) : SplitLocation::None;
    checkSplitMode(client->desktop(), screen, location, list);
    updateSplitOutlineRect(client->desktop(), screen);
    SplitOutline::instance()->resetPresStatus();
    if (client->keepAbove())
        getObj(client->desktop())->setSplitLineState(screen, true);
    else
        getObj(client->desktop())->setSplitLineState(screen, !isHaveAboveWin(client->desktop(), screen));
}

void SplitManage::updateCustomGroup(AbstractClient *remvoeClient)
{
    // if (!remvoeClient->isSplitWindow()) {
    //     if (getObj(remvoeClient->desktop()))
    //         getObj(remvoeClient->desktop())->setSplitLineState(remvoeClient->output()->name(), false);
    //     return;
    // }
    workspace()->setShowSplitLine(false, QRect());
    if (getObj(remvoeClient->desktop()))
        getObj(remvoeClient->desktop())->setSplitLineState(remvoeClient->output()->name(), true);

    clearSplitData(remvoeClient, false);
    QSet<AbstractClient *> list = getObj(remvoeClient->desktop()) ? getObj(remvoeClient->desktop())->getActiveList(remvoeClient->output()->name()) : QSet<AbstractClient *>();
    if (list.size() > 0) {
        AbstractClient *client = *(list.begin());
        createSplitGroup(client);
        SplitLocationMode location = getObj(client->desktop()) ? getObj(client->desktop())->getNormalLocation(client->output()->name()) : SplitLocation::None;
        checkSplitMode(client->desktop(), client->output()->name(), location, list);
        updateSplitOutlineRect(client->desktop(), client->output()->name());
    }
}

void SplitManage::switchWorkspaceGroup(int src, int dst)
{
    SplitGroup *srcsg = nullptr;
    SplitGroup *dstsg = nullptr;
    if (getObj(src)) {
        srcsg = m_splitGroupManage[src];
    } else {
        srcsg = new SplitGroup();
    }
    if (getObj(dst)) {
        dstsg = m_splitGroupManage[dst];
    } else {
        dstsg = new SplitGroup();
    }
    m_splitGroupManage[src] = dstsg;
    m_splitGroupManage[dst] = srcsg;
}

void SplitManage::checkOcclusion(AbstractClient *client, QString screen)
{
    if (screen.isEmpty())
        screen = client->output()->name();
    if (getObj(client->desktop())) {
        getObj(client->desktop())->updateTmpLocation(screen);
        getObj(client->desktop())->cacheTmpSplitClient(client, screen);
    }

    QSet<AbstractClient *> tmpList = getObj(client->desktop()) ? getObj(client->desktop())->getActiveList(screen) : QSet<AbstractClient *>();
    tmpList.remove(client);

    QList<Toplevel *> list = Workspace::self()->stackingOrder();
    for (int i = list.size() - 1; i >=0 ; i--) {
        AbstractClient *c = qobject_cast<AbstractClient*>(list[i]);
        if (!c || client == c || isSpecialWindowEx(c) || c->isMinimized() || c->desktop() != client->desktop() || c->output()->name() != screen)
            continue;
        if (tmpList.size() == 0)
                break;
        if (tmpList.contains(c)) {
            tmpList.remove(c);
            if (getObj(client->desktop()))
                getObj(c->desktop())->cacheTmpSplitClient(c, screen);
        } else if (c->quickTileMode() == QuickTileMode(QuickTileFlag::None)) {
            QRect rect = c->frameGeometry();
            QSetIterator<AbstractClient *> tmpit(tmpList);
            while(tmpit.hasNext()) {
                AbstractClient *tmpc = tmpit.next();
                if (tmpc->moveResizeGeometry().intersects(rect)) {
                    tmpList.remove(tmpc);
                    if (getObj(client->desktop()))
                        getObj(tmpc->desktop())->setSplitLocation(tmpc, screen, false, true);
                }
            }
        }
    }
}

void SplitManage::createSplitGroup(AbstractClient *client, QString screen)
{
    if (screen.isEmpty())
        screen = client->output()->name();
    int desktop = client->desktop();
    if (!getObj(desktop))
        return;

    AbstractClient *lefttop = nullptr;
    AbstractClient *leftbottom = nullptr;
    AbstractClient *righttop = nullptr;
    AbstractClient *rightbottom = nullptr;

    SplitLocationMode  location = SplitLocation::None;
    if ((lefttop = getObj(desktop)->getCacheClient(client, screen, SplitLocation::leftTop)) == client) {
        location |= SplitLocation::leftTop;
    }
    if ((leftbottom = getObj(desktop)->getCacheClient(client, screen, SplitLocation::leftBottom)) == client) {
        location |= SplitLocation::leftBottom;
    }
    if ((righttop = getObj(desktop)->getCacheClient(client, screen, SplitLocation::rightTop)) == client) {
        location |= SplitLocation::rightTop;
    }
    if ((rightbottom = getObj(desktop)->getCacheClient(client, screen, SplitLocation::rightBottom)) == client) {
        location |= SplitLocation::rightBottom;
    }

    getObj(desktop)->cacheActiveClient(client, screen);
    getObj(desktop)->setSplitLocation(client, screen, true);

    // QRect maxiArea = workspace()->clientArea(MaximizeArea, client);
    // QRect rect = client->frameGeometry();
    location = ~location;
    location &= 0xF;
    QuickTileMode m = QuickTileFlag::Top | QuickTileFlag::Bottom;
    if (lefttop && location & SplitLocation::leftTop) {
        if (!(lefttop->quickTileMode() & m) && lefttop != leftbottom)
            lefttop = nullptr;
        if (isSplitSplicing(client, lefttop)) {
            getObj(desktop)->cacheActiveClient(lefttop, screen);
            getObj(desktop)->setSplitLocation(lefttop, screen, true);
        }
    }
    if (leftbottom && location & SplitLocation::leftBottom) {
        if (!(leftbottom->quickTileMode() & m) && lefttop != leftbottom)
            leftbottom = nullptr;
        if (isSplitSplicing(client, leftbottom)) {
            getObj(desktop)->cacheActiveClient(leftbottom, screen);
            getObj(desktop)->setSplitLocation(leftbottom, screen, true);
        }
    }
    if (righttop && location & SplitLocation::rightTop) {
        if (!(righttop->quickTileMode() & m) && righttop != rightbottom)
            righttop = nullptr;
        if (isSplitSplicing(client, righttop)) {
            getObj(desktop)->cacheActiveClient(righttop, screen);
            getObj(desktop)->setSplitLocation(righttop, screen, true);
        }
    }
    if (rightbottom && location & SplitLocation::rightBottom) {
        if (!(rightbottom->quickTileMode() & m) && righttop != rightbottom)
            rightbottom = nullptr;
        if (isSplitSplicing(client, rightbottom)) {
            getObj(desktop)->cacheActiveClient(rightbottom, screen);
            getObj(desktop)->setSplitLocation(rightbottom, screen, true);
        }
    }
    //qCritical() << "---activesplit==-------";// << m_activeSplitList[screen].size() << m_splitWinLocation;
}

void SplitManage::updateSplitOutlineRect(int desktop, QString screen)
{
    SplitOutline::instance()->hideOutline();
    if (!getObj(desktop))
        return;

    QSet<AbstractClient *> activeSplitList = getObj(desktop)->getActiveList(screen);
    if (activeSplitList.size() < 2) {
        getObj(desktop)->setShowRect(screen, QRect());
        getObj(desktop)->setShowRect(screen, QRect(), false);
        return;
    }

    int num = 0;
    int left2len = 0, right2len = 0, top2len = 0, bottom2len = 0;
    int miny = 10000, minx = 100000;
    QSetIterator<AbstractClient *>tmpit(activeSplitList);
    while(tmpit.hasNext()) {
        AbstractClient *client = tmpit.next();

        QRect rt = client->moveResizeGeometry();
        miny = std::min(miny, rt.y());
        int offset = SPLITOUTLINE_WIDTH / 2;
        if (client->quickTileMode() & QuickTileFlag::Left) {
            if (left2len)
                left2len += 2;
            left2len += rt.height();
            getObj(desktop)->setShowRect(screen, QRect(rt.right() - offset, miny, SPLITOUTLINE_WIDTH, left2len), false);
        } else if (client->quickTileMode() & QuickTileFlag::Right) {
            if (right2len)
                right2len += 2;
            right2len += rt.height();
            getObj(desktop)->setShowRect(screen, QRect(rt.left() - offset, miny, SPLITOUTLINE_WIDTH, right2len), false);
        }

        if (client->quickTileMode() & QuickTileFlag::Top) {
            num ++;
            if (top2len)
                top2len += 2;
            top2len += rt.width();
            minx = std::min(minx, rt.x());
            getObj(desktop)->setShowRect(screen, QRect(minx, rt.bottom() - offset, top2len, SPLITOUTLINE_WIDTH));
        } else if (client->quickTileMode() & QuickTileFlag::Bottom) {
            num ++;
            if (bottom2len)
                bottom2len += 2;
            bottom2len += rt.width();
            minx = std::min(minx, rt.x());
            getObj(desktop)->setShowRect(screen, QRect(minx, rt.y() - offset, bottom2len, SPLITOUTLINE_WIDTH));
        }
    }

    if ((left2len != 0 && right2len != 0) || activeSplitList.size() >= 2) {
        getObj(desktop)->setShowRectH(screen, std::max(left2len, right2len), false);
    } else {
        getObj(desktop)->setShowRect(screen, QRect(), false);
    }

    if ((top2len != 0 && bottom2len != 0) || num > 1) {
        getObj(desktop)->setShowRectW(screen, std::max(top2len, bottom2len));
    } else {
        getObj(desktop)->setShowRect(screen, QRect());
    }

    if (left2len != 0 && right2len != 0 &&
        top2len != 0 && bottom2len != 0 &&
        activeSplitList.size() == 2) {
        getObj(desktop)->setShowRectH(screen, left2len + right2len, false);
        getObj(desktop)->setShowRectW(screen, top2len + bottom2len);
    }

    QRect hRect, vRect;
    getObj(desktop)->getHVArea(screen, hRect, vRect);
    qCritical() << ">>>>>>>>>>>>>>>>" << __LINE__ << hRect << vRect;
}

void SplitManage::cacheSplitWin(AbstractClient *client, QString screen)
{
    if (!getObj(client->desktop())) {
        SplitGroup *sg = new SplitGroup();
        m_splitGroupManage[client->desktop()] = sg;
    }

    getObj(client->desktop())->cacheGroup(client, screen);
}

bool SplitManage::isExitSplit(AbstractClient *client, QRect oldGeometry)
{
    if (!getObj(client->desktop()))
        return true;

    QString screen = client->output()->name();
    QSet<AbstractClient *> activeSplitList = getObj(client->desktop())->getActiveList(screen);
    if (!activeSplitList.contains(client) ||
        activeSplitList.size() <= 1) {
        return true;
    }

    if (m_initGeometry.isEmpty()) {
        m_initGeometry = oldGeometry;
    }

    QRect screenArea = workspace()->clientArea(MaximizeArea, client->screen(), client->desktop());

    int y = client->y() - m_initGeometry.y();
    int x = client->x() - m_initGeometry.x();

    QuickTileMode mode = QuickTileFlag::Top | QuickTileFlag::Bottom;

    if (m_direction == 0 || !(client->quickTileMode() & mode)) {
        if (abs(x) > 10) {
            m_direction = 2;
        } else if (abs(y) > 10) {
            m_direction = 1;
        }

        if (m_direction != 0) {
            if (isIncompleteGroupQuit(client)) {
                return true;
            }
            QSet<AbstractClient *> list = getMoveSplitGroup(client, true);
            if (list.size() != 0) {
                m_partnerClient = *list.begin();
                workspace()->raiseClient(m_partnerClient);
            } else {
                m_partnerClient = nullptr;
            }
        }
    }

    if (!getObj(client->desktop())->getShowRect(screen).isEmpty() && m_direction == 1) {
        if (client->quickTileMode() & QuickTileFlag::Top
            && client->y() - screenArea.y() < -10) {
            return true;
        } else if (client->quickTileMode() & QuickTileFlag::Bottom
            && client->y() + client->height() - screenArea.bottom() > 10) {
            return true;
        }
    }
    if (!getObj(client->desktop())->getShowRect(screen, false).isEmpty() && m_direction == 2) {
        if (client->quickTileMode() & QuickTileFlag::Left
            && client->x() - screenArea.x() < -10) {
            return true;
        } else if (client->quickTileMode() & QuickTileFlag::Right
            && client->x() + client->width() - screenArea.right() > 10) {
            return true;
        }
    }

    if (m_direction != 0)
        movePartner(client);

    return false;
}

void SplitManage::updateSplitGroupMode(AbstractClient *client, int direction)
{
    int desktop = client->desktop();
    if (!getObj(desktop))
        return;

    QString screen = client->output()->name();
    getObj(desktop)->clearLocation(screen);
    QSetIterator<AbstractClient *>tmpit(getObj(desktop)->getActiveList(screen)); //(m_activeSplitList[screen]);
    while(tmpit.hasNext()) {
        AbstractClient *c = tmpit.next();

        QuickTileMode mode = c->quickTileMode();
        if (c->quickTileMode() & QuickTileFlag::Left) {
            if (c->quickTileMode() & QuickTileFlag::Top) {
                getObj(desktop)->clearCustomClient(c, SplitLocation::leftTop);
                if (direction == 1) {
                    mode = QuickTileFlag::Left | QuickTileFlag::Bottom;
                    getObj(desktop)->setCustomLocation(screen, SplitLocation::leftBottom);
                    getObj(desktop)->cacheCustomClient(c, SplitLocation::leftBottom);
                } else {
                    mode = QuickTileFlag::Right | QuickTileFlag::Top;
                    getObj(desktop)->setCustomLocation(screen, SplitLocation::rightTop);
                    getObj(desktop)->cacheCustomClient(c, SplitLocation::rightTop);
                }
            } else if (c->quickTileMode() & QuickTileFlag::Bottom) {
                getObj(desktop)->clearCustomClient(c, SplitLocation::leftBottom);
                if (direction == 1) {
                    mode = QuickTileFlag::Left | QuickTileFlag::Top;
                    getObj(desktop)->setCustomLocation(screen, SplitLocation::leftTop);
                    getObj(desktop)->cacheCustomClient(c, SplitLocation::leftTop);
                } else {
                    mode = QuickTileFlag::Right | QuickTileFlag::Bottom;
                    getObj(desktop)->setCustomLocation(screen, SplitLocation::rightBottom);
                    getObj(desktop)->cacheCustomClient(c, SplitLocation::rightBottom);
                }
            } else {
                if (direction == 2) {
                    mode = QuickTileFlag::Right;
                    getObj(desktop)->setCustomLocation(screen, SplitLocationMode(SplitLocation::rightBottom) | SplitLocationMode(SplitLocation::rightTop));
                    getObj(desktop)->clearCustomClient(c, SplitLocationMode(SplitLocation::leftTop) | SplitLocationMode(SplitLocation::leftBottom));
                    getObj(desktop)->cacheCustomClient(c, SplitLocationMode(SplitLocation::rightBottom) | SplitLocationMode(SplitLocation::rightTop));
                }
            }
        } else if (c->quickTileMode() & QuickTileFlag::Right) {
            if (c->quickTileMode() & QuickTileFlag::Top) {
                getObj(desktop)->clearCustomClient(c, SplitLocation::rightTop);
                if (direction == 1) {
                    mode = QuickTileFlag::Right | QuickTileFlag::Bottom;
                    getObj(desktop)->setCustomLocation(screen, SplitLocation::rightBottom);
                    getObj(desktop)->cacheCustomClient(c, SplitLocation::rightBottom);
                } else {
                    mode = QuickTileFlag::Left | QuickTileFlag::Top;
                    getObj(desktop)->setCustomLocation(screen, SplitLocation::leftTop);
                    getObj(desktop)->cacheCustomClient(c, SplitLocation::leftTop);
                }
            } else if (c->quickTileMode() & QuickTileFlag::Bottom) {
                getObj(desktop)->clearCustomClient(c, SplitLocation::rightBottom);
                if (direction == 1) {
                    mode = QuickTileFlag::Right | QuickTileFlag::Top;
                    getObj(desktop)->setCustomLocation(screen, SplitLocation::rightTop);
                    getObj(desktop)->cacheCustomClient(c, SplitLocation::rightTop);
                } else {
                    mode = QuickTileFlag::Left | QuickTileFlag::Bottom;
                    getObj(desktop)->setCustomLocation(screen, SplitLocation::leftBottom);
                    getObj(desktop)->cacheCustomClient(c, SplitLocation::leftBottom);
                }
            } else {
                if (direction == 2) {
                    mode = QuickTileFlag::Left;
                    getObj(desktop)->setCustomLocation(screen, SplitLocationMode(SplitLocation::leftBottom) | SplitLocationMode(SplitLocation::leftTop));
                    getObj(desktop)->clearCustomClient(c, SplitLocationMode(SplitLocation::rightTop) | SplitLocationMode(SplitLocation::rightBottom));
                    getObj(desktop)->cacheCustomClient(c, SplitLocationMode(SplitLocation::leftBottom) | SplitLocationMode(SplitLocation::leftTop));
                }
            }
        }
        c->updateQuickTileMode(mode);
        c->setElectricBorderMode(mode);
    }

}

void SplitManage::movePartner(AbstractClient *client)
{
    if (m_partnerClient) {
        QPoint pos = client->pos();
        if (client->quickTileMode() & QuickTileFlag::Top && m_partnerClient->quickTileMode() & QuickTileFlag::Bottom) {
            pos.setY(client->y() + client->height());
        } else if (client->quickTileMode() & QuickTileFlag::Bottom && m_partnerClient->quickTileMode() & QuickTileFlag::Top) {
            pos.setY(client->y() - m_partnerClient->height());
        } else if (client->quickTileMode() & QuickTileFlag::Left && m_partnerClient->quickTileMode() & QuickTileFlag::Right) {
            pos.setX(client->x() + client->width());
        } else if (client->quickTileMode() & QuickTileFlag::Right && m_partnerClient->quickTileMode() & QuickTileFlag::Left) {
            pos.setX(client->x() - m_partnerClient->width());
        }

        m_partnerClient->move(pos);
    }
}

bool SplitManage::isIncompleteGroupQuit(AbstractClient *client)
{
    if (!getObj(client->desktop()) || !getObj(client->desktop())->getActiveList(client->output()->name()).contains(client))
        return true;

    QuickTileMode m = client->quickTileMode();
    if (!(m & QuickTileFlag::Top) && !(m & QuickTileFlag::Bottom)) {
        return false;
    }

    bool flag = true;
    QSet<AbstractClient *> list = getMoveSplitGroup(client);
    QSetIterator<AbstractClient *>tmpit(list);
    while(tmpit.hasNext()) {
        AbstractClient *c = tmpit.next();
        if (c == client)
            continue;
        if (m_direction == 1) {
            QuickTileMode mode = m & c->quickTileMode();
            if (mode == QuickTileMode(QuickTileFlag::Left) ||
                mode == QuickTileMode(QuickTileFlag::Right)) {
                return false;
            }
        } else if (m_direction == 2) {
            QuickTileMode mode = m & c->quickTileMode();
            if (c->quickTileMode() == QuickTileMode(QuickTileFlag::Left) ||
                c->quickTileMode() == QuickTileMode(QuickTileFlag::Right)) {
                return false;
            } else if (mode == QuickTileMode(QuickTileFlag::Top) ||
                       mode == QuickTileMode(QuickTileFlag::Bottom)) {
                return false;
            }
        }
    }
    return flag;
}

QSet<AbstractClient *> SplitManage::getMoveSplitGroup(AbstractClient *client, bool isDrag)
{
    QString screen = client->output()->name();
    QSet<AbstractClient *> activeSplitList = getObj(client->desktop()) ? getObj(client->desktop())->getActiveList(screen) : QSet<AbstractClient *>();
    QSet<AbstractClient *> list;
    if (!activeSplitList.contains(client))
        return list;

    QuickTileMode m = client->quickTileMode();

    QSetIterator<AbstractClient *>tmpit(activeSplitList);
    while(tmpit.hasNext()) {
        AbstractClient *c = tmpit.next();
        if (c == client)
            continue;

        if (m_direction == 1) {
            if (client->quickTileMode() & QuickTileFlag::Top) {
                if (!isDrag && c->quickTileMode() & QuickTileFlag::Bottom) {
                    list.insert(c);
                } else if (isDrag && c->quickTileMode() & QuickTileFlag::Top) {
                    list.insert(c);
                }
            } else if (client->quickTileMode() & QuickTileFlag::Bottom) {
                if (!isDrag && c->quickTileMode() & QuickTileFlag::Top) {
                    list.insert(c);
                } else if (isDrag && c->quickTileMode() & QuickTileFlag::Bottom) {
                    list.insert(c);
                }
            }
        } else {
            if (client->quickTileMode() & QuickTileFlag::Left) {
                if (!isDrag && c->quickTileMode() & QuickTileFlag::Right) {
                    list.insert(c);
                } else if (isDrag && c->quickTileMode() & QuickTileFlag::Left) {
                    list.insert(c);
                }
            } else if (client->quickTileMode() & QuickTileFlag::Right) {
                if (!isDrag && c->quickTileMode() & QuickTileFlag::Left) {
                    list.insert(c);
                } else if (isDrag && c->quickTileMode() & QuickTileFlag::Right) {
                    list.insert(c);
                }
            }
        }
    }

    return list;
}

void SplitManage::getSplitWinList(QSet<KWin::EffectWindow *> &list, int &location, int desktop, QString screen)
{
    location = getObj(desktop) ? getObj(desktop)->getTempLocation(screen) : SplitLocation::None;//m_tmpsplitWinLocation;
    QSet<AbstractClient *> tmpSplitList = getObj(desktop) ? getObj(desktop)->getActiveList(screen, true) : QSet<AbstractClient *>();
    if (tmpSplitList.size() < 2)
        return;

    QSetIterator<AbstractClient *> iter(tmpSplitList);
    while(iter.hasNext()) {
        AbstractClient *c = iter.next();
        list.insert(c->effectWindow());
    }
}

void SplitManage::getActiveSplitWinList(QSet<KWin::EffectWindow *> &list, int desktop, QString screen)
{
    QSet<AbstractClient *> activeSplitList = getObj(desktop) ? getObj(desktop)->getActiveList(screen) : QSet<AbstractClient *>();
    QSetIterator<AbstractClient *> iter(activeSplitList);
    while(iter.hasNext()) {
        AbstractClient *c = iter.next();
        list.insert(c->effectWindow());
    }
}

void SplitManage::getStockSplitWinList(QSet<KWin::EffectWindow *> &list, int desktop, QString screen)
{
    QSet<AbstractClient *> activeSplitList = getObj(desktop) ? getObj(desktop)->getStockList(screen) : QSet<AbstractClient *>();
    QSetIterator<AbstractClient *> iter(activeSplitList);
    while(iter.hasNext()) {
        AbstractClient *c = iter.next();
        list.insert(c->effectWindow());
    }
}

bool SplitManage::isSplitSplicing(AbstractClient *client, AbstractClient *target)
{
    bool flag = false;
    if (!client || !target)
        return flag;

    QRect maxiArea = workspace()->clientArea(MaximizeArea, Cursors::self()->mouse()->pos(), client->desktop());
    int w = client->moveResizeGeometry().width();
    int h = client->moveResizeGeometry().height();
    int tw = target->moveResizeGeometry().width();
    int th = target->moveResizeGeometry().height();

    if (!target->moveResizeGeometry().intersects(client->moveResizeGeometry())) {
        QuickTileMode mode = client->quickTileMode() & target->quickTileMode();
        switch (mode) {
        case QuickTileMode(QuickTileFlag::Left):
        case QuickTileMode(QuickTileFlag::Right):
            if (abs(w - tw) <= 2 &&
                abs(h + th - maxiArea.height()) <= 2)
                flag = true;
            break;
        case QuickTileMode(QuickTileFlag::Top):
        case QuickTileMode(QuickTileFlag::Bottom):
            if (abs(h - th) <= 2 &&
                abs(w + tw - maxiArea.width()) <= 4)
                flag = true;
            break;
        case QuickTileMode(QuickTileFlag::None):
            if ((!(client->quickTileMode() & QuickTileFlag::Top) && !(client->quickTileMode() & QuickTileFlag::Bottom)) ||
                (!(target->quickTileMode() & QuickTileFlag::Top) && !(target->quickTileMode() & QuickTileFlag::Bottom))) {
                if (abs(w + tw - maxiArea.width()) <= 4)
                    flag = true;
            } else if (abs(h + th - maxiArea.height()) <= 4 &&
                    abs(w + tw - maxiArea.width()) <= 4)
                flag = true;
            break;
        }
    }
    return flag;
}

bool SplitManage::isSpecialWindowEx(AbstractClient *client)
{
    return client->isSpecialWindow() || client->isTooltip() || client->isPopupMenu();
}

void SplitManage::setSplitMode(int desktop, QString screen, int mode)
{
    if (!getObj(desktop)) {
        SplitGroup *sg = new SplitGroup();
        m_splitGroupManage[desktop] = sg;
    }
    getObj(desktop)->setSplitScreenMode(screen, mode);
}

int SplitManage::getSplitMode(int desktop, QString screen)
{
    if (getObj(desktop))
        return getObj(desktop)->getSplitScreenMode(screen);
    return 0;
}

AbstractClient *SplitManage::findSecondaryClient(int desktop, QString screen)
{
    if (!getObj(desktop))
        return nullptr;

    QSet<AbstractClient *> list = getObj(desktop)->getActiveList(screen);

    AbstractClient *topClient = Workspace::self()->activeClient();
    AbstractClient *secondaryClient = nullptr;
    QList<KWin::Toplevel*> stacking_order = Workspace::self()->stackingOrder();

    for (int i = stacking_order.size() - 1; i >= 0; i--) {
        AbstractClient *client = qobject_cast<AbstractClient *>(stacking_order.at(i));
        if (!client || client == topClient)
            continue;
        if (list.contains(client)) {
            secondaryClient = client;
            break;
        }
    }

    return secondaryClient;
}

void SplitManage::raiseActiveSplitClient(int desktop, QString screen)
{
    if (!getObj(desktop))
        return;

    AbstractClient *topClient = Workspace::self()->activeClient();
    QSet<AbstractClient *> list = getObj(desktop)->getActiveList(screen);
    for (auto client : list) {
        if (topClient != client)
            workspace()->raiseClient(client);
    }
    workspace()->raiseClient(topClient);
}

void SplitManage::quickCustomSplit(int desktop, QString screen)
{
    if (!getObj(desktop) || screen.isEmpty())
        return;
    QList<AbstractClient *> list = getObj(desktop)->getCacheList(screen);
    for (auto client : list) {
        if (client) {
            getObj(desktop)->clearGroup(client, screen);
            client->setQuickTileMode(QuickTileFlag::None);
        }
    }
    updateSplitOutlineRect(desktop, screen);
    workspace()->setShowSplitLine(false, QRect());
    // updateStockGroupEx(desktop, screen);
}

}
