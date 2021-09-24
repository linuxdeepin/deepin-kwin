#ifndef SPLITOUTLINE_H
#define SPLITOUTLINE_H

#include <QMouseEvent>
#include <QWidget>

#include "abstract_client.h"

namespace KWin {
class AbstractClient;
class SplitOutline : public QWidget
{
public:

    explicit SplitOutline();

    void mousePressEvent(QMouseEvent* e);

    void mouseMoveEvent(QMouseEvent*e);

    void mouseReleaseEvent(QMouseEvent* e);

    void enterEvent(QEvent *);

    void leaveEvent(QEvent *);

    void setLeftSplitClient(AbstractClient* client);

    void setRightSplitClient(AbstractClient* client);
    
    void updateSplitOutlinePosition();

    AbstractClient* getLeftSplitClient();

    AbstractClient* getRightSplitClient();

    QRect getLeftSplitClientRect();

    QRect getRightSplitClientRect();
  
    void swapClientLocation();
    
    void setSplitOutlineRect(QRect rect);
     ~SplitOutline();

private:
    bool m_mainWindowPress = false;
    AbstractClient* m_leftSplitClient = nullptr;
    AbstractClient* m_rightSplitClient = nullptr;
    QRect m_workspaceRect;
    QRect m_leftSplitClientRect;
    QRect m_rightSplitClientRect;
    QWidget* widgetLine;
};
}
#endif // SPLITOUTLINE_H
