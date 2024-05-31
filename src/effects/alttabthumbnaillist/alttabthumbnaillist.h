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
    EffectFrameEx *frame() const { return m_frame.get(); }
    QRect thumbnailRect() const { return m_thumbnailRect; }
    GLTexture *texture() const { return m_texture.get(); }

private:
    EffectWindow *m_window = nullptr;
    QRect m_frameRect;
    std::unique_ptr<EffectFrameEx> m_frame = nullptr;
    QRect m_thumbnailRect;
    std::unique_ptr<GLFramebuffer> m_fbo = nullptr;
    std::unique_ptr<GLTexture> m_texture = nullptr;
};

class AltTabThumbnailListEffect : public Effect
{
    Q_OBJECT

public:
    AltTabThumbnailListEffect();
    ~AltTabThumbnailListEffect() override;

    bool isActive() const override;

    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;

public Q_SLOTS:
    void slotTabboxAdded(int);
    void slotTabboxClosed();
    void slotTabboxUpdated();

    void slotMouseChanged(const QPoint &pos, const QPoint &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

private:
    void setActive(bool active);

    void updateWindowList();
    void updateSelected();
    void updateViewRect();
    void updateVisible();

    bool windowListInvalid();

private:
    bool m_isActive = false;

    QRect m_viewRect;
    QRect m_visibleRect;
    QVector<QPair<EffectWindow *, QRect>> m_windowList;

    EffectWindow *m_selectedWindow = nullptr;
    std::unique_ptr<GLShader> m_scissorShader = nullptr;
    std::unique_ptr<GLTexture> m_scissorMask = nullptr;
    std::unique_ptr<EffectFrameEx> m_hoverFrame = nullptr;
    std::unordered_map<EffectWindow *, ItemView> m_itemList;
};

}
