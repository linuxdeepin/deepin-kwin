/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <Qt>

namespace KWin
{

enum class NavActionType {
    None = 0,
    Consume,
    Exit,
    SwitchDesktop,
    SwitchDesktopStep,
    MoveHoverToDesktop,
    HoverFirst,
    HoverLast,
    CycleHover,
    CycleHoverOrFirst,
    CycleSameType,
    AddDesktop,
    RemoveDesktop,
    ActivateHover,
    CloseHover,
    Lock,
};

struct NavAction
{
    NavActionType type = NavActionType::None;
    int arg = 0;
};

/**
 * @brief Maps a key press and modifiers to a navigation action.
 * @param key Qt key code.
 * @param mods Active keyboard modifiers.
 * @return The resolved navigation action, or a default action when unhandled.
 */
inline NavAction navActionFor(int key, Qt::KeyboardModifiers mods)
{
    const bool meta = mods == Qt::MetaModifier || mods == (Qt::MetaModifier | Qt::KeypadModifier);
    const bool shiftMeta = mods == (Qt::ShiftModifier | Qt::MetaModifier);
    const bool shiftMetaKeypad = mods == (Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier);
    const bool none = mods == Qt::NoModifier;
    const bool alt = mods == Qt::AltModifier;
    const bool altKeypad = mods == (Qt::AltModifier | Qt::KeypadModifier);
    const bool altShift = mods == (Qt::AltModifier | Qt::ShiftModifier);
    const bool shift = mods == Qt::ShiftModifier;

    switch (key) {
    case Qt::Key_Escape:
        return {NavActionType::Exit, 0};
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
    case Qt::Key_5:
    case Qt::Key_6:
        if (meta) {
            return {NavActionType::SwitchDesktop, key - Qt::Key_0};
        }
        break;
    case Qt::Key_Exclam:
        return shiftMeta ? NavAction{NavActionType::MoveHoverToDesktop, 1} : NavAction{};
    case Qt::Key_At:
        return shiftMeta ? NavAction{NavActionType::MoveHoverToDesktop, 2} : NavAction{};
    case Qt::Key_NumberSign:
        return shiftMeta ? NavAction{NavActionType::MoveHoverToDesktop, 3} : NavAction{};
    case Qt::Key_Dollar:
        return shiftMeta ? NavAction{NavActionType::MoveHoverToDesktop, 4} : NavAction{};
    case Qt::Key_Percent:
        return shiftMeta ? NavAction{NavActionType::MoveHoverToDesktop, 5} : NavAction{};
    case Qt::Key_AsciiCircum:
        return shiftMeta ? NavAction{NavActionType::MoveHoverToDesktop, 6} : NavAction{};
    case Qt::Key_Home:
        return none ? NavAction{NavActionType::HoverFirst, 0} : NavAction{};
    case Qt::Key_End:
        if (shiftMetaKeypad) {
            return {NavActionType::MoveHoverToDesktop, 1};
        }
        return none ? NavAction{NavActionType::HoverLast, 0} : NavAction{};
    case Qt::Key_PageDown:
        return shiftMetaKeypad ? NavAction{NavActionType::MoveHoverToDesktop, 3} : NavAction{};
    case Qt::Key_Clear:
        return shiftMetaKeypad ? NavAction{NavActionType::MoveHoverToDesktop, 5} : NavAction{};
    case Qt::Key_Down:
        if (shiftMetaKeypad) {
            return {NavActionType::MoveHoverToDesktop, 2};
        }
        return none ? NavAction{NavActionType::CycleHover, +1} : NavAction{};
    case Qt::Key_Up:
        return none ? NavAction{NavActionType::CycleHover, -1} : NavAction{};
    case Qt::Key_Equal:
        return alt ? NavAction{NavActionType::AddDesktop, 0} : NavAction{};
    case Qt::Key_Plus:
        return (altKeypad || altShift) ? NavAction{NavActionType::AddDesktop, 0} : NavAction{};
    case Qt::Key_Minus:
        return (alt || altKeypad) ? NavAction{NavActionType::RemoveDesktop, 0} : NavAction{};
    case Qt::Key_Tab:
        return none ? NavAction{NavActionType::CycleHoverOrFirst, +1} : NavAction{};
    case Qt::Key_Backtab:
        return shift ? NavAction{NavActionType::CycleHover, -1} : NavAction{};
    case Qt::Key_Right:
        if (meta) {
            return {NavActionType::SwitchDesktopStep, +1};
        }
        if (shiftMetaKeypad) {
            return {NavActionType::MoveHoverToDesktop, 6};
        }
        return none ? NavAction{NavActionType::CycleHover, +1} : NavAction{};
    case Qt::Key_Left:
        if (meta) {
            return {NavActionType::SwitchDesktopStep, -1};
        }
        if (shiftMetaKeypad) {
            return {NavActionType::MoveHoverToDesktop, 4};
        }
        return none ? NavAction{NavActionType::CycleHover, -1} : NavAction{};
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return {NavActionType::ActivateHover, 0};
    case Qt::Key_Delete:
        return none ? NavAction{NavActionType::CloseHover, 0} : NavAction{};
    case Qt::Key_L:
        return meta ? NavAction{NavActionType::Lock, 0} : NavAction{};
    case Qt::Key_QuoteLeft:
        return alt ? NavAction{NavActionType::CycleSameType, +1} : NavAction{};
    case Qt::Key_AsciiTilde:
        return altShift ? NavAction{NavActionType::CycleSameType, -1} : NavAction{};
    case Qt::Key_Space:
        return alt ? NavAction{NavActionType::Consume, 0} : NavAction{};
    default:
        break;
    }
    return {};
}

}
