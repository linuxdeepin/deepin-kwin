/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "abstract_opengl_context_attribute_builder.h"
#include "kwin_export.h"

namespace KWin
{

class DEEPIN_KWIN_EXPORT EglContextAttributeBuilder : public AbstractOpenGLContextAttributeBuilder
{
public:
    std::vector<int> build() const override;
};

class DEEPIN_KWIN_EXPORT EglOpenGLESContextAttributeBuilder : public AbstractOpenGLContextAttributeBuilder
{
public:
    std::vector<int> build() const override;
};

}
