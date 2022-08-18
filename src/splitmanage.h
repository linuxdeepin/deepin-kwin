/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SPLITMANAGE_H
#define KWIN_SPLITMANAGE_H

#include <QObject>
#include <QRect>
#include <QSet>

#include "abstract_client.h"

namespace KWin {

class SplitGroup
{
public:
    SplitGroup(){};
    ~SplitGroup(){};

    void clearGroup(AbstractClient *client, QString screen);
    void clearCustomGroup(QString screen);
    void cacheGroup(AbstractClient *client, QString screen);
    void clearCustomClient(AbstractClient *client, SplitLocationMode mode);
    void cacheCustomClient(AbstractClient *client, SplitLocationMode mode);
    void cacheActiveClient(AbstractClient *client, QString screen);
    void cacheTmpSplitClient(AbstractClient *client, QString screen);
    void cacheStockSplitClient(QString screen);
    void updateStockSplitClient(AbstractClient *client);
    void setSplitTmpList(AbstractClient *client, QString screen);
    void setSplitLocation(AbstractClient *client, QString screen, bool isNormal, bool isClear = false);
    void setCustomLocation(QString screen, SplitLocationMode mode);
    void updateTmpLocation(QString screen);
    void clearLocation(QString screen, bool isNormal = true);
    bool isManage(AbstractClient *client, QString screen);
    bool isManageActive(AbstractClient *client);

    void setShowRectW(QString screen, int width, bool horizontal = true);
    void setShowRectH(QString screen, int height, bool horizontal = true);
    void setShowRect(QString screen, QRect rect, bool horizontal = true);
    QRect getShowRect(QString screen, bool horizontal = true);
    void getHVArea(QString screen, QRect &hrect, QRect &vrect);
    bool getSplitLineState(QString screen);
    void setSplitLineState(QString screen, bool flag);

    void setSplitScreenMode(QString screen, int mode);
    int getSplitScreenMode(QString screen);

    AbstractClient *getCacheClient(AbstractClient *client, QString screen, SplitLocationMode mode);
    QSet<AbstractClient *> getActiveList(QString screen, bool isTmp = false);
    QSet<AbstractClient *> getStockList(QString screen);
    QList<AbstractClient *> getCacheList(QString screen);
    SplitLocationMode getNormalLocation(QString screen);
    SplitLocationMode getTempLocation(QString screen);

private:
    QHash<QString, SplitLocationMode>       m_splitWinLocation;
    QHash<QString, SplitLocationMode>       m_tmpsplitWinLocation;
    QHash<QString, int>                     m_splitMode;

    QHash<QString, QSet<AbstractClient *>>  m_tmpSplitList;
    QHash<QString, QSet<AbstractClient *>>  m_activeSplitList;
    QHash<QString, QSet<AbstractClient *>>  m_stockSplitList;

    QHash<QString, QList<AbstractClient *>> m_leftTopList;
    QHash<QString, QList<AbstractClient *>> m_leftBottomList;
    QHash<QString, QList<AbstractClient *>> m_rightTopList;
    QHash<QString, QList<AbstractClient *>> m_rightBottomList;

    QHash<QString, QRect> m_hRect;
    QHash<QString, QRect> m_vRect;
    QHash<QString, bool>  m_splitLineState;
};

class SplitManage : public QObject
{
    Q_OBJECT
public:
    SplitManage();
    ~SplitManage();
    static SplitManage *instance();

public:
    void updateSplitRect(int desktop, QString screen, int pos, bool isTopDown = false);
    void addWinSplit(AbstractClient *client, bool isQuickMatch = false);
    void removeWinSplit(AbstractClient *client);
    void updateSplitOutlineRect(int desktop, QString screen);
    void getHVRect(QString screen, QRect &hRect, QRect &vRect);
    bool isExitSplit(AbstractClient *client, QRect oldGeometry);
    void updateSplitGroupMode(AbstractClient *client, int direction);  // 1 上下; 2 左右;
    QSet<AbstractClient *> getMoveSplitGroup(AbstractClient *client, bool isDrag = false);
    void movePartner(AbstractClient *client);
    void resetDirection() {m_direction = 0; m_partnerClient = nullptr; m_initGeometry = QRect(); m_isSwap = false;};
    int getDirection() {return m_direction;};
    void updateSplitGroup(AbstractClient *client);
    void updateCustomGroup(AbstractClient *remvoeClient);
    void switchWorkspaceGroup(int src, int dst);
    void setSplitLineStateEx(int desktop, QString screen, bool flag, bool isRecheck = false);
    bool isShowSplitLine(QString screen);
    bool isHaveAboveWin(int desktop, QString screen);
    void getSplitWinList(QSet<KWin::EffectWindow *> &list, int &location, int desktop, QString screen);
    void getActiveSplitWinList(QSet<KWin::EffectWindow *> &list, int desktop, QString screen);
    void getStockSplitWinList(QSet<KWin::EffectWindow *> &list, int desktop, QString screen);
    bool isIncompleteGroupQuit(AbstractClient *client);
    QRect getQuickTileArea(AbstractClient *client, int desktop, QString screen, QuickTileMode mode, QRect rect, QRect availableArea, bool isTriggerSplit = false, bool isUseTmp = false);

    bool isManageClient(AbstractClient *client);
    AbstractClient *findSecondaryClient(int desktop, QString screen);
    void raiseActiveSplitClient(int desktop, QString screen);
    void updateStockGroupEx(int desktop, QString screen);
    void quickCustomSplit(int desktop, QString screen);
    bool isStartSwap() {return m_isSwap;};
    void startSwapState() {m_isSwap = true;};

    void setSplitMode(int desktop, QString screen, int mode);
    int getSplitMode(int desktop, QString screen);

    QString getScreenWithSplit() {return m_screen;}

private:
    bool isExtensionMode() const;
    bool isSpecialWindowEx(AbstractClient *client);
    int getNumScreens();
    void createSplitGroup(AbstractClient *client, QString screen = "");
    void clearSplitData(AbstractClient *client, bool isClearList = true, QString screen = "");
    void updateStockGroup(AbstractClient *client);
    void checkSplitWindowEx(AbstractClient *client);
    void cacheSplitWin(AbstractClient *client, QString screen = "");
    void checkOcclusion(AbstractClient *client, QString screen = "");
    bool isSplitSplicing(AbstractClient *client, AbstractClient *target);
    void checkSplitMode(int desktop, QString screen, SplitLocationMode location, QSet<AbstractClient *> &list);
    SplitGroup *getObj(int desktop);

private:
    static SplitManage *_instance;

    int             m_direction = 0;     //1 topdown; 2 leftright;
    AbstractClient *m_partnerClient = nullptr;
    bool            m_isSwap = false;
    QString         m_screen;

    QRect           m_initGeometry;
    QHash<int, SplitGroup*> m_splitGroupManage;
};

}

#endif
