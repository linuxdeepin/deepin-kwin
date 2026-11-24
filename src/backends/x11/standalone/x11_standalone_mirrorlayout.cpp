/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_mirrorlayout.h"

#include "main.h"

#include "utils/xcbutils.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QString>


namespace KWin
{

static const char s_snapshotGroup[] = "MirrorMode";
static const char s_snapshotKey[] = "Layout";

X11MirrorModeLayout::X11MirrorModeLayout(xcb_connection_t *connection)
    : m_connection(connection)
{
}

QVector<X11MirrorModeLayout::CrtcState> X11MirrorModeLayout::readCrtcs() const
{
    QVector<CrtcState> crtcs;
    if (!m_connection) {
        return crtcs;
    }

    Xcb::RandR::CurrentResources resources(kwinApp()->x11RootWindow());
    if (resources.isNull()) {
        return crtcs;
    }

    crtcs.reserve(resources->num_crtcs);
    for (int i = 0; i < resources->num_crtcs; ++i) {
        CrtcState state;
        state.crtc = resources.crtcs()[i];

        Xcb::RandR::CrtcInfo info(state.crtc, resources->config_timestamp);
        if (!info.isNull() && info->mode != XCB_NONE) {
            state.mode = info->mode;
            state.rotation = info->rotation;
            state.geometry = QRect(info->x, info->y, info->width, info->height);
            state.outputs.reserve(info->num_outputs);
            for (int j = 0; j < info->num_outputs; ++j) {
                state.outputs.append(info.outputs()[j]);
            }
        }
        crtcs.append(state);
    }
    return crtcs;
}

bool X11MirrorModeLayout::writeCrtcs(const QVector<CrtcState> &crtcs) const
{
    if (!m_connection) {
        return false;
    }

    QRect bounds;
    for (const CrtcState &state : crtcs) {
        if (state.mode != XCB_NONE) {
            bounds = bounds.united(state.geometry);
        }
    }
    if (bounds.isEmpty()) {
        return false;
    }

    // The screen has to be large enough for the new positions before the CRTCs move:
    // the X server rejects a CRTC that ends up outside of it.
    xcb_randr_set_screen_size(m_connection, kwinApp()->x11RootWindow(), bounds.width(), bounds.height(), 0, 0);

    for (const CrtcState &state : crtcs) {
        if (state.mode == XCB_NONE) {
            continue;
        }
        // A panned CRTC scans out a window of the framebuffer that is not the view
        // rectangle, so the two would disagree again. Clearing the panning means
        // zeroing the view rectangle, the tracking area and the border.
        xcb_randr_set_panning(m_connection, state.crtc, XCB_CURRENT_TIME,
                              0, 0, 0, 0,
                              0, 0, 0, 0,
                              0, 0, 0, 0);
        xcb_randr_set_crtc_config(m_connection, state.crtc, XCB_CURRENT_TIME, XCB_CURRENT_TIME,
                                  state.geometry.x(), state.geometry.y(), state.mode, state.rotation,
                                  state.outputs.size(), state.outputs.constData());
    }
    xcb_flush(m_connection);
    return true;
}

bool X11MirrorModeLayout::apply()
{
    if (!m_snapshot.isEmpty()) {
        return true;
    }

    const QVector<CrtcState> current = readCrtcs();
    if (current.isEmpty()) {
        return false;
    }

    // The X server anchors on the primary output, KWin on preferredOutput(). Both have
    // to pick the same display, otherwise one of them moves a screen the other does
    // not care about.
    xcb_randr_output_t primaryOutput = XCB_NONE;
    const xcb_randr_get_output_primary_cookie_t primaryCookie = xcb_randr_get_output_primary(m_connection, kwinApp()->x11RootWindow());
    if (xcb_randr_get_output_primary_reply_t *reply = xcb_randr_get_output_primary_reply(m_connection, primaryCookie, nullptr)) {
        primaryOutput = reply->output;
        free(reply);
    }

    QVector<MirrorModeDisplay> displays;
    displays.reserve(current.size());
    for (const CrtcState &state : current) {
        MirrorModeDisplay display;
        display.enabled = state.mode != XCB_NONE;
        display.primary = display.enabled && state.outputs.contains(primaryOutput);
        display.geometry = state.geometry;
        displays.append(display);
    }

    const QVector<QRect> geometries = MirrorModeLayout::normalize(displays);
    QVector<CrtcState> target = current;
    for (int i = 0; i < target.size(); ++i) {
        target[i].geometry = geometries[i];
    }

    if (!writeCrtcs(target)) {
        return false;
    }

    m_snapshot = current;
    KConfigGroup group(kwinApp()->config(), QString::fromLatin1(s_snapshotGroup));
    group.writeEntry(QString::fromLatin1(s_snapshotKey), serialize(m_snapshot));
    kwinApp()->config()->sync();
    return true;
}

void X11MirrorModeLayout::restore()
{
    if (m_snapshot.isEmpty()) {
        // Nothing was applied in this run, but a previous one may have ended while the
        // mode was on and left the real layout where the mode put it.
        restoreSnapshot(m_connection);
        return;
    }

    writeCrtcs(m_snapshot);
    m_snapshot.clear();

    KConfigGroup group(kwinApp()->config(), QString::fromLatin1(s_snapshotGroup));
    group.deleteEntry(QString::fromLatin1(s_snapshotKey));
    kwinApp()->config()->sync();
}


void X11MirrorModeLayout::restoreSnapshot(xcb_connection_t *connection)
{
    if (!connection) {
        return;
    }
    const KConfigGroup group(kwinApp()->config(), QString::fromLatin1(s_snapshotGroup));
    const QVector<CrtcState> snapshot = deserialize(group.readEntry<QStringList>(QString::fromLatin1(s_snapshotKey), QStringList()));
    if (snapshot.isEmpty()) {
        return;
    }

    X11MirrorModeLayout layout(connection);
    layout.writeCrtcs(snapshot);

    KConfigGroup writableGroup(kwinApp()->config(), QString::fromLatin1(s_snapshotGroup));
    writableGroup.deleteEntry(QString::fromLatin1(s_snapshotKey));
    kwinApp()->config()->sync();
}

QStringList X11MirrorModeLayout::serialize(const QVector<CrtcState> &crtcs)
{
    QStringList entries;
    entries.reserve(crtcs.size());
    for (const CrtcState &state : crtcs) {
        QStringList outputs;
        outputs.reserve(state.outputs.size());
        for (const xcb_randr_output_t output : state.outputs) {
            outputs.append(QString::number(output));
        }
        entries.append(QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8")
                           .arg(state.crtc)
                           .arg(state.mode)
                           .arg(state.rotation)
                           .arg(state.geometry.x())
                           .arg(state.geometry.y())
                           .arg(state.geometry.width())
                           .arg(state.geometry.height())
                           .arg(outputs.join(QLatin1Char(';'))));
    }
    return entries;
}

QVector<X11MirrorModeLayout::CrtcState> X11MirrorModeLayout::deserialize(const QStringList &entries)
{
    QVector<CrtcState> crtcs;
    crtcs.reserve(entries.size());
    for (const QString &entry : entries) {
        const QStringList fields = entry.split(QLatin1Char(','));
        if (fields.size() != 8) {
            return QVector<CrtcState>();
        }
        CrtcState state;
        state.crtc = fields.at(0).toUInt();
        state.mode = fields.at(1).toUInt();
        state.rotation = fields.at(2).toUShort();
        state.geometry = QRect(fields.at(3).toInt(), fields.at(4).toInt(), fields.at(5).toInt(), fields.at(6).toInt());
        const QStringList outputs = fields.at(7).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        state.outputs.reserve(outputs.size());
        for (const QString &output : outputs) {
            state.outputs.append(output.toUInt());
        }
        crtcs.append(state);
    }
    return crtcs;
}

} // namespace KWin
