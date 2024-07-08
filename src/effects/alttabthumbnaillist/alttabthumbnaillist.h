#pragma once

#include <kwineffects.h>
#include <kwineffectsex.h>

namespace KWin
{

class ItemView
{
public:
    ItemView(EffectWindow *w, const QRect &r);
    ItemView(ItemView &&info);
    ItemView(const ItemView &info) = delete;
    ~ItemView();

    ItemView &operator=(ItemView &&info);
    ItemView &operator=(const ItemView &info) = delete;

    void setFrameRect(const QRect &r);
    void updateTexture();

    EffectWindow *window() const { return m_window; }
    QRect frameRect() const { return m_frameRect; }
    QRect filledRect() const { return m_frameRect.adjusted(-1, -1, 1, 1); }  // considered border
    QRect thumbnailRect() const { return m_thumbnailRect; }
    GLTexture *windowTexture() const { return m_windowTexture.get(); }
    GLTexture *filledTexture() const { return m_filledTexture.get(); }

private:
    EffectWindow *m_window = nullptr;
    QRect m_frameRect;
    QRect m_thumbnailRect;
    std::unique_ptr<GLFramebuffer> m_fbo = nullptr;
    std::unique_ptr<GLTexture> m_windowTexture = nullptr;
    std::unique_ptr<GLTexture> m_filledTexture = nullptr;
};

class AltTabThumbnailListEffect : public Effect
{
    Q_OBJECT

public:
    AltTabThumbnailListEffect();
    ~AltTabThumbnailListEffect() override;

    bool isActive() const override;

    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;

    static EffectWindow *dockWindow() { return s_dockWindow; }

public Q_SLOTS:
    void slotTabboxAdded(int);
    void slotTabboxClosed();
    void slotTabboxUpdated();

    void slotMouseChanged(const QPoint &pos, const QPoint &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

    void slotWindowRemoved(EffectWindow *w);

private:
    void setActive(bool active);

    void updateWindowList();
    void updateSelected();
    void updateViewRect();
    void updateVisible();

    void renderSelectedFrame(const QRect &rect, ScreenPaintData &data);
    void renderItemView(ItemView &view, ScreenPaintData &data);

    bool windowListInvalid();

private:
    bool m_isActive = false;
    bool m_paintingWaterMark = false;

    QRect m_viewRect;
    QRect m_visibleRect;
    QVector<QPair<EffectWindow *, QRect>> m_windowList;

    EffectWindow *m_selectedWindow = nullptr;
    EffectWindow *m_waterMarkWindow = nullptr;

    std::unique_ptr<GLShader> m_clipShader = nullptr;
    std::unique_ptr<GLShader> m_scissorShader = nullptr;

    std::unique_ptr<GLTexture> m_scissorMask = nullptr;
    std::unique_ptr<GLTexture> m_selectedFrame = nullptr;

    std::unordered_map<EffectWindow *, ItemView> m_itemList;

    static EffectWindow *s_dockWindow;
};

}
