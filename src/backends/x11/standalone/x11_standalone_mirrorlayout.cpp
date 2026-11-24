/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_mirrorlayout.h"

#include "main.h"
#include "mirrormode_logging.h"
#include "utils/xcbutils.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDebug>
#include <QString>

#include <cmath>


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

        // Ask for the state as it is now: the callers write a layout and then read it
        // back, so a config timestamp from before the write would hand them the old one.
        Xcb::RandR::CrtcInfo info(state.crtc, XCB_CURRENT_TIME);
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

// RandR rejects a zero physical size, and a layout change is only about pixels: scale the
// millimetres per pixel the screen reports onto the requested size, so the reported DPI does
// not jump when the mode packs the outputs tighter than the layout it replaces.
static QSize physicalSizeFor(xcb_connection_t *connection, const QSize &pixels)
{
    const xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    if (!screen || screen->width_in_pixels == 0 || screen->height_in_pixels == 0
        || screen->width_in_millimeters == 0 || screen->height_in_millimeters == 0) {
        return pixels;
    }
    return QSize(qMax(1, int(std::lround(double(screen->width_in_millimeters) * pixels.width() / screen->width_in_pixels))),
                 qMax(1, int(std::lround(double(screen->height_in_millimeters) * pixels.height() / screen->height_in_pixels))));
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

    // The screen has to cover the CRTCs both at their current positions, which is where
    // they still are, and at the new ones: the X server rejects a CRTC that ends up
    // outside of it, and it rejects a size that leaves an existing CRTC outside too.
    // Going straight to bounds therefore fails whenever the mode packs the outputs
    // smaller than the layout it replaces, and the server silently keeps the old, larger
    // screen. Stage through the union and shrink to the packed layout once they moved.
    QRect staging = bounds;
    for (const CrtcState &state : readCrtcs()) {
        if (state.mode != XCB_NONE) {
            staging = staging.united(state.geometry);
        }
    }

    // The requests are asynchronous and a rejected one leaves the layout where it was.
    // They are not collected through cookies: RandR hands back a typed cookie for the CRTC
    // request and rebuilding that as an xcb_void_cookie_t drops the wide request sequence,
    // which asserts inside xcb instead of reporting the error. Read the result back from
    // the server, which is the same answer without depending on cookie bookkeeping.
    const QSize stagingPhysical = physicalSizeFor(m_connection, staging.size());
    const QSize boundsPhysical = physicalSizeFor(m_connection, bounds.size());

    xcb_randr_set_screen_size(m_connection, kwinApp()->x11RootWindow(), staging.width(), staging.height(),
                              stagingPhysical.width(), stagingPhysical.height());

    for (const CrtcState &state : crtcs) {
        if (state.mode == XCB_NONE) {
            continue;
        }
        // A panned CRTC scans out a window of the framebuffer that is not the view
        // rectangle, so the two would disagree again. Clearing the panning means
        // zeroing the view rectangle, the tracking area and the border. Panning is
        // optional for a driver, so a failure here is not a failed move.
        xcb_randr_set_panning(m_connection, state.crtc, XCB_CURRENT_TIME,
                              0, 0, 0, 0,
                              0, 0, 0, 0,
                              0, 0, 0, 0);
        xcb_randr_set_crtc_config(m_connection, state.crtc, XCB_CURRENT_TIME, XCB_CURRENT_TIME,
                                   state.geometry.x(), state.geometry.y(), state.mode, state.rotation,
                                   state.outputs.size(), state.outputs.constData());
    }

    xcb_randr_set_screen_size(m_connection, kwinApp()->x11RootWindow(), bounds.width(), bounds.height(),
                              boundsPhysical.width(), boundsPhysical.height());
    xcb_flush(m_connection);

    const QVector<CrtcState> applied = readCrtcs();
    if (applied.size() != crtcs.size()) {
        return false;
    }
    for (int i = 0; i < crtcs.size(); ++i) {
        if (crtcs[i].mode == XCB_NONE) {
            continue;
        }
        if (applied[i].crtc != crtcs[i].crtc || applied[i].mode != crtcs[i].mode || applied[i].geometry != crtcs[i].geometry) {
            qCWarning(KWIN_MIRRORMODE) << "layout write did not take for crtc" << crtcs[i].crtc
                               << "wanted" << crtcs[i].geometry << "have" << applied[i].geometry;
            return false;
        }
    }
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

    for (const QRect &geometry : geometries) {
        qCDebug(KWIN_MIRRORMODE) << "normalized geometry" << geometry;
    }
    if (!writeCrtcs(target)) {
        // Hand the layout back before saying the mode stays off: the failed write may
        // have moved some of the CRTCs already, and the mode is only honest while the
        // screens the clients are shown and the ones they are scanned out on agree.
        writeCrtcs(current);
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

    const bool restored = writeCrtcs(m_snapshot);
    // Drop the in-memory copy either way so a later apply() is not blocked by it.
    m_snapshot.clear();
    if (!restored) {
        // The screens did not go back. Leave the record in the config so the next start
        // can try again instead of losing the only way back.
        qCWarning(KWIN_MIRRORMODE) << "The layout cannot be restored, keeping the record for the next start";
        return;
    }
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
    if (!layout.writeCrtcs(snapshot)) {
        // The layout a previous run captured no longer fits this hardware, so the screens
        // stay where the mode left them. Keep the record instead of throwing away the only
        // way back, and say so.
        qCWarning(KWIN_MIRRORMODE) << "The layout captured before the mode cannot be restored";
        return;
    }

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
