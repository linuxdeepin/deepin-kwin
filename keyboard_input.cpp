/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "keyboard_input.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "keyboard_layout.h"
#include "keyboard_repeat.h"
#include "abstract_client.h"
#include "modifier_only_shortcuts.h"
#include "utils.h"
#include "screenlockerwatcher.h"
#include "toplevel.h"
#include "wayland_server.h"
#include "workspace.h"
// KWayland
#include <KWayland/Server/datadevice_interface.h>
#include <KWayland/Server/seat_interface.h>
//screenlocker
#include <KScreenLocker/KsldApp>
// Frameworks
#include <KGlobalAccel>
// Qt
#include <QKeyEvent>

namespace KWin
{

KeyboardInputRedirection::KeyboardInputRedirection(InputRedirection *parent)
    : QObject(parent)
    , m_input(parent)
    , m_xkb(new Xkb(parent))
{
    connect(m_xkb.data(), &Xkb::ledsChanged, this, &KeyboardInputRedirection::ledsChanged);
    if (waylandServer()) {
        m_xkb->setSeat(waylandServer()->seat());
    }
}

KeyboardInputRedirection::~KeyboardInputRedirection() = default;

class KeyStateChangedSpy : public InputEventSpy
{
public:
    KeyStateChangedSpy(InputRedirection *input)
        : m_input(input)
    {
    }

    void keyEvent(KeyEvent *event) override
    {
        if (event->isAutoRepeat()) {
            return;
        }
        emit m_input->keyStateChanged(event->nativeScanCode(), event->type() == QEvent::KeyPress ? InputRedirection::KeyboardKeyPressed : InputRedirection::KeyboardKeyReleased);
    }

private:
    InputRedirection *m_input;
};

class ModifiersChangedSpy : public InputEventSpy
{
public:
    ModifiersChangedSpy(InputRedirection *input)
        : m_input(input)
        , m_modifiers()
    {
    }

    void keyEvent(KeyEvent *event) override
    {
        if (event->isAutoRepeat()) {
            return;
        }
        updateModifiers(event->modifiers());
    }

    void updateModifiers(Qt::KeyboardModifiers mods)
    {
        if (mods == m_modifiers) {
            return;
        }
        emit m_input->keyboardModifiersChanged(mods, m_modifiers);
        m_modifiers = mods;
    }

private:
    InputRedirection *m_input;
    Qt::KeyboardModifiers m_modifiers;
};

void KeyboardInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;
    const auto config = kwinApp()->kxkbConfig();
    m_xkb->setNumLockConfig(kwinApp()->inputConfig());
    m_xkb->setConfig(config);

    m_input->installInputEventSpy(new KeyStateChangedSpy(m_input));
    m_modifiersChangedSpy = new ModifiersChangedSpy(m_input);
    m_input->installInputEventSpy(m_modifiersChangedSpy);
    m_keyboardLayout = new KeyboardLayout(m_xkb.data());
    m_keyboardLayout->setConfig(config);
    m_keyboardLayout->init();
    m_input->installInputEventSpy(m_keyboardLayout);

    if (waylandServer()->hasGlobalShortcutSupport()) {
        m_input->installInputEventSpy(new ModifierOnlyShortcuts);
    }

    KeyboardRepeat *keyRepeatSpy = new KeyboardRepeat(m_xkb.data());
    connect(keyRepeatSpy, &KeyboardRepeat::keyRepeat, this,
        std::bind(&KeyboardInputRedirection::processKey, this, std::placeholders::_1, InputRedirection::KeyboardKeyAutoRepeat, std::placeholders::_2, nullptr));
    //workaround: disable repeat handling, since clients will do it again.
    //m_input->installInputEventSpy(keyRepeatSpy);

    connect(workspace(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(waylandServer(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(workspace(), &Workspace::clientActivated, this,
        [this] {
            disconnect(m_activeClientSurfaceChangedConnection);
            if (auto c = workspace()->activeClient()) {
                m_activeClientSurfaceChangedConnection = connect(c, &Toplevel::surfaceChanged, this, &KeyboardInputRedirection::update);
            } else {
                m_activeClientSurfaceChangedConnection = QMetaObject::Connection();
            }
            update();
        }
    );
    if (waylandServer()->hasScreenLockerIntegration()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &KeyboardInputRedirection::update);
    }
}

void KeyboardInputRedirection::update()
{
    if (!m_inited) {
        return;
    }
    auto seat = waylandServer()->seat();
    // TODO: this needs better integration
    Toplevel *found = nullptr;
    if (waylandServer()->isScreenLocked()) {
        const ToplevelList &stacking = Workspace::self()->stackingOrder();
        if (!stacking.isEmpty()) {
            auto it = stacking.end();
            do {
                --it;
                Toplevel *t = (*it);
                if (t->isDeleted()) {
                    // a deleted window doesn't get mouse events
                    continue;
                }
                if (!t->isLockScreen()) {
                    continue;
                }
                if (!t->readyForPainting()) {
                    continue;
                }
                found = t;
                break;
            } while (it != stacking.begin());
        }
    } else if (!input()->isSelectingWindow()) {
        found = workspace()->activeClient();
    }
    if (found && found->surface()) {
        if (found->surface() != seat->focusedKeyboardSurface()) {
            seat->setFocusedKeyboardSurface(found->surface());
            auto newKeyboard = seat->focusedKeyboard();
            if (newKeyboard && newKeyboard->client() == waylandServer()->xWaylandConnection()) {
                // focus passed to an XWayland surface
                const auto selection = seat->selection();
                auto xclipboard = waylandServer()->xclipboardSyncDataDevice();
                if (xclipboard && selection != xclipboard.data()) {
                    if (selection) {
                        xclipboard->sendSelection(selection);
                    } else {
                        xclipboard->sendClearSelection();
                    }
                }
            }
        }
    } else {
        //NOTE(sonald): clear focus will make qt app lost input, this may be a bug of Qt itself
        //need to fix it later
        //seat->setFocusedKeyboardSurface(nullptr);
    }
}

void KeyboardInputRedirection::processKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time, LibInput::Device *device)
{
    QEvent::Type type;
    bool autoRepeat = false;
    switch (state) {
    case InputRedirection::KeyboardKeyAutoRepeat:
        autoRepeat = true;
        // fall through
    case InputRedirection::KeyboardKeyPressed:
        type = QEvent::KeyPress;
        break;
    case InputRedirection::KeyboardKeyReleased:
        type = QEvent::KeyRelease;
        break;
    default:
        Q_UNREACHABLE();
    }

    if (!autoRepeat) {
        m_xkb->updateKey(key, state);
    }

    const xkb_keysym_t keySym = m_xkb->currentKeysym();

    //During the lock screen, pressing hot keys will start the corresponding function such as show ControlCenter when we press f10, we think this is a bug
    //Fixed: add a qdbus interface, the lock screen program calls this interface to notify kwin to shield the hotkey
    //just return directly without handling hotkey events if found program has called qdbus interface to request kwin to shield the hotkey
    if (workspace()->isDisableHotKeys()) {
        Qt::Key key = m_xkb->toQtKey(keySym);
        switch(key) {
        case Qt::Key_MonBrightnessDown: //f1
        case Qt::Key_MonBrightnessUp: //f2
        case Qt::Key_KeyboardLightOnOff: //f3
        case Qt::Key_VolumeMute: //f4
        case Qt::Key_VolumeDown: //f5
        case Qt::Key_VolumeUp: //f6
        case Qt::Key_MicMute: //f7
        case Qt::Key_Display: //f8
        case Qt::Key_WLAN: //f9
        case Qt::Key_Tools: //f10
        case Qt::Key_Print: //f11
        case Qt::Key_Insert: //f2
            qDebug()<<Q_FUNC_INFO<<"skip key"<<key;
            return;
        default:
            break;
        }
    }

    KeyEvent event(type,
                   m_xkb->toQtKey(keySym),
                   m_xkb->modifiers(),
                   key,
                   keySym,
                   m_xkb->toString(keySym),
                   autoRepeat,
                   time,
                   device);
    event.setModifiersRelevantForGlobalShortcuts(m_xkb->modifiersRelevantForGlobalShortcuts());

    // crtl+alt+f1 will switch to command-line access from GUI
    // we want still show GUI instead of switch to command-line
    // so we need to skip crtl+alt+f1 action
    if (m_xkb->enableCrtlAltShortcuts()) {
        // first we need to find crtl+alt modifiers
        // then skip f1(59)
        if (59 == key) {
            return;
        }
    }

    m_input->processSpies(std::bind(&InputEventSpy::keyEvent, std::placeholders::_1, &event));
    if (!m_inited) {
        return;
    }
    m_input->processFilters(std::bind(&InputEventFilter::keyEvent, std::placeholders::_1, &event));

    m_xkb->forwardModifiers();
}

void KeyboardInputRedirection::processModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!m_inited) {
        return;
    }
    // TODO: send to proper Client and also send when active Client changes
    m_xkb->updateModifiers(modsDepressed, modsLatched, modsLocked, group);
    m_modifiersChangedSpy->updateModifiers(modifiers());
    m_keyboardLayout->checkLayoutChange();
}

void KeyboardInputRedirection::processKeymapChange(int fd, uint32_t size)
{
    if (!m_inited) {
        return;
    }
    // TODO: should we pass the keymap to our Clients? Or only to the currently active one and update
    m_xkb->installKeymap(fd, size);
    m_keyboardLayout->resetLayout();
}

}
