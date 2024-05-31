#include "alttabthumbnaillist.h"

#include <QPainter>

#include <effects.h>
#include <kwinglutils.h>
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "workspace.h"

#define THUMBNAIL_HEIGHT        (160 * effectsEx->getOsScale())
#define THUMBNAIL_HOVER_MARGIN  (7 * effectsEx->getOsScale())
#define THUMBNAIL_ICON_SIZE     (24 * effectsEx->getOsScale())
#define THUMBNAIL_RADIUS        (12 * effectsEx->getOsScale())
#define THUMBNAIL_SPACING       (20 * effectsEx->getOsScale())
#define THUMBNAIL_TITLE_HEIGHT  (38 * effectsEx->getOsScale())

#define THUMBNAIL_VIEW_BOTTOM_MARGIN    (100 * effectsEx->getOsScale())
#define THUMBNAIL_VIEW_HEIGHT           (284 * effectsEx->getOsScale())
#define THUMBNAIL_VIEW_SIDE_MARGIN      (10 * effectsEx->getOsScale())

Q_LOGGING_CATEGORY(KWIN_ALTTABTHUMBNAILLIST, "kwin_effect_alttabthumbnaillist", QtWarningMsg)

static void ensureResources()
{
    Q_INIT_RESOURCE(alttabthumbnaillist);
}

static QRect scaledRect(const QRect &r, const int height)
{
    if (r.height() < height || r.height() <= 0)
        return QRect(0, 0, r.width(), r.height());

    const int width = std::ceil(r.width() * (float(height) / r.height()));
    return QRect(0, 0, width, height);
}

namespace KWin
{

ItemView::ItemView(EffectWindow *w, const QRect &r)
    : m_window(w)
{
    setFrameRect(r);

    m_frame = effectsEx->effectFrameEx("kwin/effects/alttabthumbnaillist/qml/fill.qml", false);
    if (w && m_frame) {
        m_frame->setIconSize(QSize(THUMBNAIL_ICON_SIZE, THUMBNAIL_ICON_SIZE));
        m_frame->setImage(w->icon().pixmap(m_frame->iconSize()));
        m_frame->setText(w->caption());
    } else {
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to load thumbnail fill frame";
    }
}

ItemView::ItemView(ItemView &&info)
{
    *this = std::move(info);
}

ItemView &ItemView::operator=(ItemView &&info)
{
    if (this != &info) {
        m_window = info.m_window;
        m_frameRect = info.m_frameRect;
        m_frame = std::move(info.m_frame);
        m_thumbnailRect = info.m_thumbnailRect;
        m_fbo = std::move(info.m_fbo);
        m_texture = std::move(info.m_texture);
    }
    return *this;
}

ItemView::~ItemView()
{
    effects->makeOpenGLContextCurrent();
    m_frame.reset();
    m_fbo.reset();
    m_texture.reset();
}

void ItemView::setFrameRect(const QRect &r)
{
    m_frameRect = r;
    m_thumbnailRect = scaledRect(m_window->geometry().toRect(), THUMBNAIL_HEIGHT);
    m_thumbnailRect.translate(m_frameRect.x(),
                              m_frameRect.y() + THUMBNAIL_TITLE_HEIGHT + (THUMBNAIL_HEIGHT - m_thumbnailRect.height()) / 2);
}

void ItemView::updateTexture()
{
    if (!m_window || m_thumbnailRect.isEmpty())
        return;

    if (!m_texture || m_texture->size() != m_thumbnailRect.size()) {
        m_texture = std::make_unique<GLTexture>(GL_RGBA8, m_thumbnailRect.size());
        m_texture->setFilter(GL_LINEAR);
        m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_texture->setYInverted(true);
        m_fbo = std::make_unique<GLFramebuffer>(m_texture.get());
    }

    GLFramebuffer::pushFramebuffer(m_fbo.get());

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    ItemRenderer *renderer = static_cast<EffectsHandlerImpl *>(effects)->scene()->renderer();
    const qreal scale = renderer->renderTargetScale();
    QMatrix4x4 projectionMatrix;
    const QRectF geometry = m_window->frameGeometry();
    projectionMatrix.ortho(geometry.x() * scale, (geometry.x() + geometry.width()) * scale,
                           geometry.y() * scale, (geometry.y() + geometry.height()) * scale, -1, 1);
    WindowPaintData data;
    data.setProjectionMatrix(projectionMatrix);
    const int mask = Scene::PAINT_WINDOW_TRANSFORMED;
    renderer->renderItem(static_cast<EffectWindowImpl *>(m_window)->windowItem(), mask, infiniteRegion(), data);

    GLFramebuffer::popFramebuffer();
}

//----------------------------thumbnail list effect-----------------------------

AltTabThumbnailListEffect::AltTabThumbnailListEffect()
{
    ensureResources();

    m_scissorShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                        QByteArray(),
                                                                        ":/effects/alttabthumbnaillist/shaders/scissor.frag");
    if (!m_scissorShader)
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to load thumbnail scissor shader";

    connect(effects, &EffectsHandler::tabBoxAdded, this, &AltTabThumbnailListEffect::slotTabboxAdded);
    connect(effects, &EffectsHandler::tabBoxClosed, this, &AltTabThumbnailListEffect::slotTabboxClosed);
    connect(effects, &EffectsHandler::tabBoxUpdated, this, &AltTabThumbnailListEffect::slotTabboxUpdated);

    connect(effects, &EffectsHandler::mouseChanged, this, &AltTabThumbnailListEffect::slotMouseChanged);
}

AltTabThumbnailListEffect::~AltTabThumbnailListEffect()
{
    setActive(false);

    m_scissorShader.reset();
    m_scissorMask.reset();
}

bool AltTabThumbnailListEffect::isActive() const
{
    return m_isActive;
}

void AltTabThumbnailListEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    workspace()->forceDisableRadius(true);

    for (auto &item : m_itemList) {
        EffectWindow *w = item.first;
        ItemView &view = item.second;
        const QRect thumbnail_rect = view.thumbnailRect();
        const QRect frame_rect = view.frameRect();
        const QRect trans_visible = m_visibleRect.translated(m_viewRect.left() - m_visibleRect.left(),
                                                             m_viewRect.top());

        int clip = 0;
        if (thumbnail_rect.left() < trans_visible.left()) {
            clip = thumbnail_rect.left() - trans_visible.left();
        } else if (thumbnail_rect.right() > trans_visible.right()) {
            clip = thumbnail_rect.right() - trans_visible.right();
        }

        // render hover frame
        if (w == m_selectedWindow && m_hoverFrame) {
            const int margin = THUMBNAIL_HOVER_MARGIN;
            m_hoverFrame->setGeometry(frame_rect.adjusted(-margin, -margin, margin, margin));
            m_hoverFrame->render(infiniteRegion(), 1, 0);
        }

        // render fill frame
        EffectFrameEx *frame = view.frame();
        if (frame) {
            frame->setClipOffset(QPoint(clip, 0));
            frame->setGeometry(frame_rect.translated(-clip, 0)
                                         .adjusted(-1, -1, 1, 1)  // adjust border
                                         .intersected(trans_visible));
            frame->render(infiniteRegion(), 1, thumbnail_rect.height() < THUMBNAIL_HEIGHT);
        }

        // build scissor mask
        if (!m_scissorMask) {
            QImage img(QSize(THUMBNAIL_RADIUS * 2, THUMBNAIL_RADIUS * 2), QImage::Format_RGBA8888);
            img.fill(Qt::transparent);
            QPainter painter(&img);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 255));
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawEllipse(0, 0, THUMBNAIL_RADIUS * 2, THUMBNAIL_RADIUS * 2);
            painter.end();

            m_scissorMask = std::make_unique<GLTexture>(img.copy(0, 0, THUMBNAIL_RADIUS, THUMBNAIL_RADIUS));
            m_scissorMask->setFilter(GL_LINEAR);
            m_scissorMask->setWrapMode(GL_CLAMP_TO_EDGE);
        }

        // update texture and render
        view.updateTexture();
        GLTexture *texture = view.texture();
        if (!texture) {
            qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to load thumbnail texture of " << w->caption();
        } else {
            ShaderManager::instance()->pushShader(m_scissorShader.get());

            const float width_factor = float(thumbnail_rect.width()) / w->width();
            const float height_factor = float(thumbnail_rect.height()) / w->height();
            const float clip_factor = float(clip) / w->width() / width_factor;
            const qreal scale = effects->renderTargetScale();

            QMatrix4x4 mvp(data.projectionMatrix());
            mvp.translate(thumbnail_rect.x() * scale, thumbnail_rect.y() * scale);

            m_scissorShader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
            m_scissorShader->setUniform("sampler", 0);
            m_scissorShader->setUniform("msk1", 1);
            m_scissorShader->setUniform("k",
                                        QVector2D(w->width() * width_factor / THUMBNAIL_RADIUS,
                                                  w->height() * height_factor / THUMBNAIL_RADIUS));
            m_scissorShader->setUniform("clip_left", clip_factor < 0 ? -clip_factor : 0);
            m_scissorShader->setUniform("clip_right", clip_factor > 0 ? 1 - clip_factor : 1);

            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

            glActiveTexture(GL_TEXTURE1);
            m_scissorMask->bind();
            glActiveTexture(GL_TEXTURE0);
            texture->bind();
            texture->render(thumbnail_rect, scale);
            glActiveTexture(GL_TEXTURE1);
            m_scissorMask->unbind();
            glActiveTexture(GL_TEXTURE0);
            texture->unbind();

            glDisable(GL_BLEND);

            ShaderManager::instance()->popShader();
        }
    }

    workspace()->forceDisableRadius(false);
}

void AltTabThumbnailListEffect::slotTabboxAdded(int)
{
    updateWindowList();
    if (windowListInvalid()) {
        effects->closeTabBox();
        return;
    }

    // init visible rect
    const int list_width = m_windowList.back().second.right() + THUMBNAIL_SPACING;
    m_visibleRect = QRect(0, 0, m_viewRect.width(), m_viewRect.height());
    if (m_visibleRect.width() < list_width) {
        for (const auto &w : m_windowList) {
            int offset = w.second.width() < 40 ? w.second.width() / 2 : 20;
            if (w.second.right() < m_visibleRect.right() + offset)
                continue;
            if (w.second.left() > m_visibleRect.right() - offset) {
                // ensure the last visible thumbnail is truncated
                m_visibleRect.translate(w.second.left() - (m_visibleRect.right() - offset), 0);
                break;
            } else {
                break;
            }
        }
    }

    updateSelected();
    setActive(true);
}

void AltTabThumbnailListEffect::slotTabboxClosed()
{
    if (!isActive())
        return;

    setActive(false);
}

void AltTabThumbnailListEffect::slotTabboxUpdated()
{
    if (!isActive())
        return;

    updateWindowList();
    updateSelected();
    updateVisible();
}

void AltTabThumbnailListEffect::slotMouseChanged(const QPoint &pos, const QPoint &old,
                                           Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                                           Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers)
{
    if (!isActive() || buttons != Qt::MouseButton::LeftButton)
        return;

    for (auto &item : m_itemList) {
        if (item.second.frameRect().contains(pos)) {
            effects->setTabBoxWindow(item.first);
            updateSelected();
            return;
        }
    }
}

void AltTabThumbnailListEffect::setActive(bool active)
{
    m_isActive = active;

    // inactive
    if (!active) {
        m_selectedWindow = nullptr;
        m_hoverFrame.reset();
        m_itemList.clear();
        m_windowList.clear();
        return;
    }

    // active
    if (!m_hoverFrame) {
        m_hoverFrame = effectsEx->effectFrameEx("kwin/effects/alttabthumbnaillist/qml/hover.qml", false);
        if (m_hoverFrame) {
            m_hoverFrame->setColor(effectsEx->getActiveColor());
            m_hoverFrame->setRadius((THUMBNAIL_RADIUS + THUMBNAIL_HOVER_MARGIN));
        } else {
            qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to load thumbnail hover frame";
        }
    }
}

void AltTabThumbnailListEffect::updateWindowList()
{
    EffectWindowList list = effects->currentTabBoxWindowList();
    bool changed = false;
    if (m_windowList.size() != list.size()) {
        changed = true;
    } else {
        for (int i = 0; i < list.size(); ++i) {
            if (m_windowList.at(i).first != list.at(i)) {
                changed = true;
                break;
            }
        }
    }
    if (!changed)
        return;

    m_windowList.clear();
    int x = 0;
    for (EffectWindow *w : list) {
        if (Q_UNLIKELY(!w))
            continue;
        x += THUMBNAIL_SPACING;
        QRect r = scaledRect(w->geometry().toRect(), THUMBNAIL_HEIGHT).translated(x, 0);
        m_windowList << QPair<EffectWindow *, QRect>(w, r);
        x += r.width();
    }
    if (m_windowList.empty()) {
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Empty thumbnail window list";
        return;
    }

    updateViewRect();
    updateVisible();
}

void AltTabThumbnailListEffect::updateSelected()
{
    m_selectedWindow = effects->currentTabBoxWindow();
    int i = 0;
    while (i < m_windowList.size()) {
        if (m_windowList.at(i).first == m_selectedWindow)
            break;
        ++i;
    }
    if (i >= m_windowList.size()) {
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to find selected client in list";
    } else {
        const QRect selected_rect = m_windowList.at(i).second;
        // scroll left
        if (selected_rect.left() - THUMBNAIL_SPACING < m_visibleRect.left()) {
            if (i == 0) {
                m_visibleRect.moveTo(0, m_visibleRect.top());
            } else {
                m_visibleRect.moveTo(m_windowList.at(i - 1).second.center().x(), m_visibleRect.top());
            }
        // scroll right
        } else if (selected_rect.right() + THUMBNAIL_SPACING > m_visibleRect.right()) {
            if (i == m_windowList.size() - 1) {
                m_visibleRect.moveTo(m_windowList.at(i).second.right() + THUMBNAIL_SPACING - m_visibleRect.width(), m_visibleRect.top());
            } else {
                m_visibleRect.moveTo(m_windowList.at(i + 1).second.center().x() - m_visibleRect.width(), m_visibleRect.top());
            }
        }

        updateVisible();
    }
}

void AltTabThumbnailListEffect::updateViewRect()
{
    const QRect screen_geometry = workspace()->activeOutput()->geometry();

    // top and bottom of view are fixed
    m_viewRect.setBottom(screen_geometry.bottom() - THUMBNAIL_VIEW_BOTTOM_MARGIN);
    m_viewRect.setTop(m_viewRect.bottom() - THUMBNAIL_VIEW_HEIGHT + 1);

    if (m_windowList.empty())
        return;
    const int list_width = m_windowList.last().second.right() + THUMBNAIL_SPACING;
    if (list_width <= screen_geometry.width() - THUMBNAIL_VIEW_SIDE_MARGIN * 2) {
        const int left = screen_geometry.left() + (screen_geometry.width() - list_width) / 2;
        m_viewRect.setLeft(left);
        m_viewRect.setWidth(list_width);
    } else {
        m_viewRect.setLeft(screen_geometry.left() + THUMBNAIL_VIEW_SIDE_MARGIN);
        m_viewRect.setRight(screen_geometry.right() - THUMBNAIL_VIEW_SIDE_MARGIN);
    }

    effects->setTabBoxViewRect(m_viewRect.adjusted(-1, -1, 1, 1));  // adjust border

    m_visibleRect.setWidth(m_viewRect.width());
    if (m_visibleRect.right() > list_width)
        m_visibleRect.translate(list_width - m_visibleRect.right(), 0);
}

void AltTabThumbnailListEffect::updateVisible()
{
    const int item_top_margin = (m_viewRect.height() - THUMBNAIL_TITLE_HEIGHT - THUMBNAIL_HEIGHT) / 2;
    const int item_y = m_viewRect.top() + item_top_margin;

    QHash<EffectWindow *, QRect> list;
    for (const auto &p : m_windowList) {
        if (p.second.intersects(m_visibleRect))
            list.insert(p.first, p.second.translated(-m_visibleRect.left() + m_viewRect.left(), item_y));
    }

    // remove invisible
    for (auto it = m_itemList.begin(), end = m_itemList.end(); it != end;) {
        if (!list.contains(it->first)) {
            it = m_itemList.erase(it);
        } else {
            ++it;
        }
    }

    // add new visible and update existing
    for (auto it = list.begin(), end = list.end(); it != end; ++it) {
        auto item = m_itemList.find(it.key());
        const QRect frame_rect(it.value().x(), it.value().y(),
                               it.value().width(), THUMBNAIL_TITLE_HEIGHT + THUMBNAIL_HEIGHT);
        if (item == m_itemList.end()) {
            m_itemList.emplace(it.key(), ItemView(it.key(), frame_rect));
        } else {
            item->second.setFrameRect(frame_rect);
        }
    }
}

bool AltTabThumbnailListEffect::windowListInvalid()
{
    return m_windowList.empty() || (m_windowList.size() == 1 && m_windowList.front().first->isDesktop());
}

}