#ifndef KWIN_SPLITSCREEN_H
#define KWIN_SPLITSCREEN_H

#include <kwineffects.h>
#include <abstract_client.h>
#include <QHash>

#include <utils.h>

namespace KWin
{

class SplitScreenEffect : public Effect
{
    Q_OBJECT
public:
    SplitScreenEffect();
    ~SplitScreenEffect() override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void paintScreen(int mask, QRegion region, ScreenPaintData &data) override;
    void postPaintScreen() override;

    virtual void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    virtual void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    virtual void windowInputMouseEvent(QEvent* e) override;
    virtual void grabbedKeyboardEvent(QKeyEvent* e) override;

    bool isActive() const override;
    void setActive(bool active);

private Q_SLOTS:
    void slotWindowStartUserMovedResized(EffectWindow *w);
    void slotWindowQuickTileModeChanged(EffectWindow *w);
    void slotWindowFinishUserMovedResized(EffectWindow *w);

private:
    bool isEnterSplitMode(QuickTileMode mode);
    QRect getPreviewWindowsGeometry();

    void cleanup();

    bool isRelevantWithPresentWindows(EffectWindow *w) const;
    void calculateWindowTransformations(EffectWindowList windows, WindowMotionManager& wmm);
    void calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen,
            WindowMotionManager& motionManager);

private:
    struct GridSize {
        int columns {0};
        int rows {0};
    };

private:
    QRect m_geometry;
    EffectWindow *m_window = nullptr;

    AbstractClient* m_cacheClient = nullptr;
    AbstractClient* m_enterSplitClient = nullptr;
    AbstractClient* m_previewPopClient = nullptr;

    bool m_activated = false;
    bool m_hasKeyboardGrab = false;

    QuickTileMode m_quickTileMode = QuickTileMode(QuickTileFlag::None);

    QVector<WindowMotionManager> m_motionManagers;
    QRect m_backgroundRect;

    QHash<int, GridSize> m_gridSizes;
    QHash<int, QVector<EffectWindow*>> m_takenSlots;
};

} // namespace KWin

#endif
