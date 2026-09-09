/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "blackscreen.h"

#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QFile>
#include <QLoggingCategory>

#include <limits>

Q_LOGGING_CATEGORY(KWIN_BLACKSCREEN, "kwin_effect_blackscreen", QtInfoMsg)

namespace KWin
{

bool BlackScreenEffect::supported()
{
    return KWin::effects->isOpenGLCompositing();
}

BlackScreenEffect::BlackScreenEffect(QObject *, const QVariantList &)
    : Effect()
{
    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/BlackScreen"), this, QDBusConnection::ExportScriptableContents)) {
        qCWarning(KWIN_BLACKSCREEN) << "failed to register D-Bus object /BlackScreen";
    }

    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &BlackScreenEffect::slotTimerTimeout);
}

BlackScreenEffect::~BlackScreenEffect()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/BlackScreen"));
}

/**
 * Intercepts per-window rendering. While the black screen is active every window's
 * draw is replaced by a full-screen black quad; otherwise the window is drawn normally.
 */
void BlackScreenEffect::drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    if (!m_activated) {
        effects->drawWindow(w, mask, region, data);
        return;
    }

    const QRect screen = effects->virtualScreenGeometry();
    QColor color(0, 0, 0);
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    ShaderBinder binder(ShaderTrait::UniformColor);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());
    vbo->setColor(color);

    QVector<float> verts;
    verts.reserve(12);

    verts << screen.x() + screen.width() << screen.y();
    verts << screen.x() << screen.y();
    verts << screen.x() << screen.y() + screen.height();
    verts << screen.x() << screen.y() + screen.height();
    verts << screen.x() + screen.width() << screen.y() + screen.height();
    verts << screen.x() + screen.width() << screen.y();

    vbo->setData(verts.count() / 2, 2, verts.data(), nullptr);
    vbo->render(GL_TRIANGLES);
}

bool BlackScreenEffect::isActive() const
{
    return m_activated;
}

/**
 * Returns a description of the D-Bus caller for logging purposes. For a call made
 * over D-Bus this resolves the peer's service name, PID and process name; local
 * (non-D-Bus) callers are reported as "internal".
 */
QString BlackScreenEffect::dbusCaller() const
{
    if (!calledFromDBus()) {
        return QStringLiteral("internal");
    }

    const QDBusMessage &m = message();
    const QString service = m.service();
    QString caller = QStringLiteral("%1 (path:%2)").arg(service, m.path());

    const QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    if (iface) {
        const QDBusReply<uint> pidReply = iface->servicePid(service);
        if (pidReply.isValid()) {
            const uint pid = pidReply.value();
            QFile commFile(QStringLiteral("/proc/%1/comm").arg(pid));
            if (commFile.open(QIODevice::ReadOnly)) {
                const QString processName = QString::fromUtf8(commFile.readAll()).trimmed();
                commFile.close();
                caller = QStringLiteral("%1 (pid:%2 process:%3)").arg(service).arg(pid).arg(processName);
            } else {
                caller = QStringLiteral("%1 (pid:%2)").arg(service).arg(pid);
            }
        }
    }
    return caller;
}

/**
 * Activates or deactivates the black screen. Activation hides the cursor and
 * schedules a full repaint; deactivation stops any pending auto-exit timer.
 */
void BlackScreenEffect::setActive(bool active)
{
    if (m_activated == active) {
        return;
    }

    m_activated = active;

    if (m_activated) {
        effects->hideCursor();
        qCInfo(KWIN_BLACKSCREEN) << "black screen activated by" << dbusCaller();
    } else {
        m_timer.stop();
        effects->showCursor();
        qCInfo(KWIN_BLACKSCREEN) << "black screen deactivated by" << dbusCaller();
    }
    effects->addRepaintFull();
}

bool BlackScreenEffect::getActive() const
{
    return m_activated;
}

/**
 * Starts the black screen for @p ms milliseconds. A positive value schedules an
 * automatic deactivation after the given duration; @p ms == 0 keeps the black
 * screen active until setActive(false) is called explicitly.
 */
void BlackScreenEffect::start(quint32 ms)
{
    qCInfo(KWIN_BLACKSCREEN) << "start black screen, duration =" << ms << "ms, called by" << dbusCaller();

    setActive(true);

    m_timer.stop();
    if (ms > 0) {
        // Clamp to a value QTimer can represent to avoid int truncation overflow.
        m_timer.start(int(qMin(ms, quint32(std::numeric_limits<int>::max()))));
    }
}

void BlackScreenEffect::slotTimerTimeout()
{
    qCInfo(KWIN_BLACKSCREEN) << "black screen auto exit after duration";
    setActive(false);
}

} // namespace KWin
