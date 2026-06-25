// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "meetingprivacy.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(MeetingPrivacy,
                               "metadata.json.stripped",
                               return MeetingPrivacy::supported();)

} // namespace KWin

#include "main.moc"
