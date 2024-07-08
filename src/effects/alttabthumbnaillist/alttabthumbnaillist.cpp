#include "alttabthumbnaillist.h"

#include <QPainter>

#include <effects.h>
#include <kwinglutils.h>
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "workspace.h"

#define THUMBNAIL_HEIGHT        (118 * effectsEx->getOsScale())
#define THUMBNAIL_HOVER_WIDTH   (4 * effectsEx->getOsScale())
#define THUMBNAIL_HOVER_MARGIN  (7 * effectsEx->getOsScale())
#define THUMBNAIL_ICON_SIZE     (24 * effectsEx->getOsScale())
#define THUMBNAIL_RADIUS        (12 * effectsEx->getOsScale())
#define THUMBNAIL_SPACING       (14 * effectsEx->getOsScale())
#define THUMBNAIL_SIDE_MARGIN   (20 * effectsEx->getOsScale())
#define THUMBNAIL_TITLE_HEIGHT  (40 * effectsEx->getOsScale())
#define THUMBNAIL_TITLE_MARGIN  (8 * effectsEx->getOsScale())
#define THUMBNAIL_TEXT_SIZE     (14 * effectsEx->getOsScale())

#define THUMBNAIL_VIEW_BOTTOM_MARGIN    (100 * effectsEx->getOsScale())
#define THUMBNAIL_VIEW_HEIGHT           (198 * effectsEx->getOsScale())
#define THUMBNAIL_VIEW_SIDE_MARGIN      (10 * effectsEx->getOsScale())

Q_LOGGING_CATEGORY(KWIN_ALTTABTHUMBNAILLIST, "kwin_effect_alttabthumbnaillist", QtWarningMsg)

static void ensureResources()
{
    Q_INIT_RESOURCE(alttabthumbnaillist);
}

static QRect heightScaledRect(const QRect &r, const int height)
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
        m_thumbnailRect = info.m_thumbnailRect;
        m_fbo = std::move(info.m_fbo);
        m_windowTexture = std::move(info.m_windowTexture);
        m_filledTexture = std::move(info.m_filledTexture);
    }
    return *this;
}

ItemView::~ItemView()
{
    effects->makeOpenGLContextCurrent();
    m_fbo.reset();
    m_windowTexture.reset();
    m_filledTexture.reset();
}

void ItemView::setFrameRect(const QRect &r)
{
    m_frameRect = r;
    m_thumbnailRect = heightScaledRect(m_window->geometry().toRect(), THUMBNAIL_HEIGHT);
    m_thumbnailRect.translate(m_frameRect.x(),
                              m_frameRect.y() + THUMBNAIL_TITLE_HEIGHT + (THUMBNAIL_HEIGHT - m_thumbnailRect.height()) / 2);
}

void ItemView::updateTexture()
{
    if (!window() || thumbnailRect().isEmpty())
        return;

    if (!m_windowTexture || m_windowTexture->size() != thumbnailRect().size()) {
        m_windowTexture = std::make_unique<GLTexture>(GL_RGBA8, thumbnailRect().size());
        m_windowTexture->setFilter(GL_LINEAR);
        m_windowTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_windowTexture->setYInverted(true);
        m_fbo = std::make_unique<GLFramebuffer>(m_windowTexture.get());
    }

    // update window thumbnail
    if (m_fbo) {
        GLFramebuffer::pushFramebuffer(m_fbo.get());

        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        ItemRenderer *renderer = static_cast<EffectsHandlerImpl *>(effects)->scene()->renderer();
        const qreal scale = renderer->renderTargetScale();
        QMatrix4x4 projectionMatrix;
        const QRectF geometry = window()->frameGeometry();
        projectionMatrix.ortho(geometry.x() * scale, (geometry.x() + geometry.width()) * scale,
                               geometry.y() * scale, (geometry.y() + geometry.height()) * scale, -1, 1);
        WindowPaintData data;
        data.setProjectionMatrix(projectionMatrix);
        const int mask = Scene::PAINT_WINDOW_TRANSFORMED;
        renderer->renderItem(static_cast<EffectWindowImpl *>(window())->windowItem(), mask, infiniteRegion(), data);

        if (window()->isDesktop()) {
            EffectWindow *dock = AltTabThumbnailListEffect::dockWindow();
            if (dock && geometry.contains(dock->frameGeometry()))
                renderer->renderItem(static_cast<EffectWindowImpl *>(dock)->windowItem(), mask, infiniteRegion(), data);
        }

        GLFramebuffer::popFramebuffer();
    }

    // update filled
    if (!m_filledTexture || m_filledTexture->size() != filledRect().size()) {
        const QRect filled_rect(QPoint(0, 0), filledRect().size());
        QPixmap pixmap(filled_rect.size());
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

        const float radius = THUMBNAIL_RADIUS;
        // inner rect
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#CCFFFFFF"));
        painter.drawRoundedRect(filled_rect.adjusted(1, 1, -1, -1), radius, radius);
        if (window()->height() >= THUMBNAIL_HEIGHT) {
            QPainter::CompositionMode mode = painter.compositionMode();
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.setBrush(Qt::transparent);
            painter.drawRect(0, filled_rect.bottom() - radius, filled_rect.width(), radius);
            painter.setCompositionMode(mode);
        }
        // border
        painter.setPen(QPen(QColor("#19000000"), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(filled_rect, radius + 1, radius + 1);
        // split line
        painter.setPen(QPen(QColor("#0D000000"), 1));
        painter.drawLine(QPointF(0, THUMBNAIL_TITLE_HEIGHT),
                         QPointF(filled_rect.width(), THUMBNAIL_TITLE_HEIGHT));
        // icon
        const qreal verical_margin = (THUMBNAIL_TITLE_HEIGHT - THUMBNAIL_ICON_SIZE) / 2;
        const QRectF icon_rect(QPointF(THUMBNAIL_TITLE_MARGIN, verical_margin),
                               QSizeF(THUMBNAIL_ICON_SIZE, THUMBNAIL_ICON_SIZE));
        const QIcon icon = window()->isDesktop() ? QIcon::fromTheme(QStringLiteral("deepin-toggle-desktop"))
                                                 : window()->icon();
        painter.drawPixmap(icon_rect.topLeft(), icon.pixmap(THUMBNAIL_ICON_SIZE, THUMBNAIL_ICON_SIZE));
        // title
        QFont font;
        font.setPixelSize(THUMBNAIL_TEXT_SIZE);
        const QFontMetricsF metrics(font);
        const QRectF text_rect(icon_rect.x() + icon_rect.width() + THUMBNAIL_TITLE_MARGIN, 0,
                               filled_rect.width() - icon_rect.width() - 3 * THUMBNAIL_TITLE_MARGIN,
                               THUMBNAIL_TITLE_HEIGHT);
        const QString elided = metrics.elidedText(window()->caption(), Qt::ElideRight, text_rect.width());
        painter.setPen(Qt::black);
        painter.setFont(font);
        painter.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, elided);

        m_filledTexture = std::make_unique<GLTexture>(pixmap);
    }
}

//----------------------------thumbnail list effect-----------------------------

EffectWindow *AltTabThumbnailListEffect::s_dockWindow = nullptr;

AltTabThumbnailListEffect::AltTabThumbnailListEffect()
{
    ensureResources();

    m_clipShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                     QByteArray(),
                                                                     ":/effects/alttabthumbnaillist/shaders/clip.frag");
    if (!m_clipShader)
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to load filled clip shader";

    m_scissorShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                        QByteArray(),
                                                                        ":/effects/alttabthumbnaillist/shaders/scissor.frag");
    if (!m_scissorShader)
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to load thumbnail scissor shader";

    connect(effects, &EffectsHandler::tabBoxAdded, this, &AltTabThumbnailListEffect::slotTabboxAdded);
    connect(effects, &EffectsHandler::tabBoxClosed, this, &AltTabThumbnailListEffect::slotTabboxClosed);
    connect(effects, &EffectsHandler::tabBoxUpdated, this, &AltTabThumbnailListEffect::slotTabboxUpdated);

    connect(effects, &EffectsHandler::mouseChanged, this, &AltTabThumbnailListEffect::slotMouseChanged);

    connect(effects, &EffectsHandler::windowClosed, this, &AltTabThumbnailListEffect::slotWindowRemoved);
    connect(effects, &EffectsHandler::windowDeleted, this, &AltTabThumbnailListEffect::slotWindowRemoved);
}

AltTabThumbnailListEffect::~AltTabThumbnailListEffect()
{
    setActive(false);

    m_scissorShader.reset();
    m_clipShader.reset();
    m_scissorMask.reset();
    m_selectedFrame.reset();
}

bool AltTabThumbnailListEffect::isActive() const
{
    return m_isActive;
}

void AltTabThumbnailListEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (w->isDock()) {
        s_dockWindow = w;
    } else if (w->isDesktop()) {
        data.setBrightness(0.4);
    } else if (w->isWaterMark()) {
        // paint later
        if (!m_paintingWaterMark) {
            m_waterMarkWindow = w;
            return;
        }
    }

    effects->paintWindow(w, mask, region, data);
}

void AltTabThumbnailListEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    workspace()->forceDisableRadius(true);

    for (auto &item : m_itemList) {
        EffectWindow *w = item.first;
        ItemView &view = item.second;

        if (w == m_selectedWindow) {
            const QRect frame_rect = view.frameRect();
            const int margin = THUMBNAIL_HOVER_MARGIN;
            renderSelectedFrame(frame_rect.adjusted(-margin, -margin, margin, margin), data);
        }

        renderItemView(view, data);
    }

    workspace()->forceDisableRadius(false);

    // paint watermark
    if (m_waterMarkWindow) {
        m_paintingWaterMark = true;
        ItemRenderer *renderer = static_cast<EffectsHandlerImpl *>(effects)->scene()->renderer();
        WindowPaintData window_data(renderer->renderTargetProjectionMatrix());
        effects->paintWindow(m_waterMarkWindow, 0, region, window_data);
        m_paintingWaterMark = false;
    }
}

void AltTabThumbnailListEffect::slotTabboxAdded(int)
{
    updateWindowList();
    if (windowListInvalid()) {
        effects->closeTabBox();
        return;
    }

    // init visible rect
    const int list_width = m_windowList.back().second.right() + THUMBNAIL_SIDE_MARGIN;
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

void AltTabThumbnailListEffect::slotWindowRemoved(EffectWindow *w)
{
    if (!isActive())
        return;

    if (s_dockWindow == w) {
        s_dockWindow = nullptr;
    } else if (m_waterMarkWindow == w) {
        m_waterMarkWindow = nullptr;
        m_paintingWaterMark = false;
    }
}

void AltTabThumbnailListEffect::setActive(bool active)
{
    m_isActive = active;

    effects->addRepaintFull();

    // inactive
    if (!active) {
        m_paintingWaterMark = false;

        m_selectedWindow = nullptr;
        m_waterMarkWindow = nullptr;
        s_dockWindow = nullptr;

        m_itemList.clear();
        m_windowList.clear();
        return;
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
    int x = THUMBNAIL_SIDE_MARGIN;
    for (EffectWindow *w : list) {
        if (Q_UNLIKELY(!w))
            continue;
        QRect r = heightScaledRect(w->geometry().toRect(), THUMBNAIL_HEIGHT).translated(x, 0);
        m_windowList << QPair<EffectWindow *, QRect>(w, r);
        x += r.width() + THUMBNAIL_SPACING;
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
        const int spacing = (i == 0 || i == m_windowList.size() - 1 ? THUMBNAIL_SIDE_MARGIN : THUMBNAIL_SPACING);
        // scroll left
        if (selected_rect.left() - spacing < m_visibleRect.left()) {
            if (i == 0) {
                m_visibleRect.moveTo(0, m_visibleRect.top());
            } else {
                m_visibleRect.moveTo(m_windowList.at(i - 1).second.center().x(), m_visibleRect.top());
            }
        // scroll right
        } else if (selected_rect.right() + spacing > m_visibleRect.right()) {
            if (i == m_windowList.size() - 1) {
                m_visibleRect.moveTo(m_windowList.at(i).second.right() + THUMBNAIL_SIDE_MARGIN - m_visibleRect.width(), m_visibleRect.top());
            } else {
                m_visibleRect.moveTo(m_windowList.at(i + 1).second.center().x() - m_visibleRect.width(), m_visibleRect.top());
            }
        }

        updateVisible();
    }
}

void AltTabThumbnailListEffect::updateViewRect()
{
    QRect screen_geometry = effects->clientArea(
            clientAreaOption::PlacementArea, effects->activeScreen(), effects->currentDesktop()).toRect();
    screen_geometry.setHeight(effects->activeScreen()->geometry().height());

    // top and bottom of view are fixed
    m_viewRect.setBottom(screen_geometry.bottom() - THUMBNAIL_VIEW_BOTTOM_MARGIN);
    m_viewRect.setTop(m_viewRect.bottom() - THUMBNAIL_VIEW_HEIGHT + 1);

    if (m_windowList.empty())
        return;
    const int list_width = m_windowList.last().second.right() + THUMBNAIL_SIDE_MARGIN;
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

void AltTabThumbnailListEffect::renderSelectedFrame(const QRect &rect, ScreenPaintData &data)
{
    // update texture
    if (!m_selectedFrame || m_selectedFrame->size() != rect.size()) {
        const QRect r(QPoint(0, 0), rect.size());
        QPixmap pixmap(r.size());
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);

        const float radius = THUMBNAIL_RADIUS + THUMBNAIL_HOVER_MARGIN;
        painter.setBrush(QColor(workspace()->ActiveColor()));
        painter.drawRoundedRect(r, radius, radius);

        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setBrush(Qt::transparent);
        const float width = THUMBNAIL_HOVER_WIDTH;
        painter.drawRoundedRect(r.adjusted(width, width, -width, -width), radius - width, radius - width);

        m_selectedFrame = std::make_unique<GLTexture>(pixmap);
    }

    const QRectF scaled = scaledRect(rect, effects->renderTargetScale());

    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

    QMatrix4x4 mvp(data.projectionMatrix());
    mvp.translate(scaled.x(), scaled.y());
    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_selectedFrame->bind();
    m_selectedFrame->render(scaled.toRect(), effects->renderTargetScale());
    m_selectedFrame->unbind();

    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();
}

void AltTabThumbnailListEffect::renderItemView(ItemView &view, ScreenPaintData &data)
{
    const QRect trans_visible = m_visibleRect.translated(m_viewRect.left() - m_visibleRect.left(),
                                                         m_viewRect.top());

    EffectWindow *w = view.window();
    view.updateTexture();

    // render filled
    GLTexture *texture = view.filledTexture();
    if (!texture) {
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to render filled texture of " << w->caption();
    } else {
        ShaderManager::instance()->pushShader(m_clipShader.get());

        const QRect filled_rect = view.filledRect();
        int clip = 0;
        if (filled_rect.left() < trans_visible.left()) {
            clip = filled_rect.left() - trans_visible.left();
        } else if (filled_rect.right() > trans_visible.right()) {
            clip = filled_rect.right() - trans_visible.right();
        }
        const float clip_factor = float(clip) / filled_rect.width();

        QMatrix4x4 mvp(data.projectionMatrix());
        mvp.translate(filled_rect.x() * effects->renderTargetScale(),
                      filled_rect.y() * effects->renderTargetScale());

        m_clipShader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
        m_clipShader->setUniform("clip_left", clip_factor < 0 ? -clip_factor : 0);
        m_clipShader->setUniform("clip_right", clip_factor > 0 ? 1 - clip_factor : 1);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        texture->bind();
        texture->render(filled_rect, effects->renderTargetScale());
        texture->unbind();

        glDisable(GL_BLEND);

        ShaderManager::instance()->popShader();
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

    // render thumbnail
    texture = view.windowTexture();
    if (!texture) {
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to load thumbnail texture of " << w->caption();
    } else {
        ShaderManager::instance()->pushShader(m_scissorShader.get());

        const QRect thumbnail_rect = view.thumbnailRect();
        const float width_factor = float(thumbnail_rect.width()) / w->width();
        const float height_factor = float(thumbnail_rect.height()) / w->height();
        int clip = 0;
        if (thumbnail_rect.left() < trans_visible.left()) {
            clip = thumbnail_rect.left() - trans_visible.left();
        } else if (thumbnail_rect.right() > trans_visible.right()) {
            clip = thumbnail_rect.right() - trans_visible.right();
        }
        const float clip_factor = float(clip) / thumbnail_rect.width();

        QMatrix4x4 mvp(data.projectionMatrix());
        mvp.translate(thumbnail_rect.x() * effects->renderTargetScale(),
                      thumbnail_rect.y() * effects->renderTargetScale());

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
        texture->render(thumbnail_rect, effects->renderTargetScale());
        glActiveTexture(GL_TEXTURE1);
        m_scissorMask->unbind();
        glActiveTexture(GL_TEXTURE0);
        texture->unbind();

        glDisable(GL_BLEND);

        ShaderManager::instance()->popShader();
    }
}

bool AltTabThumbnailListEffect::windowListInvalid()
{
    return m_windowList.empty() || (m_windowList.size() == 1 && m_windowList.front().first->isDesktop());
}

}
