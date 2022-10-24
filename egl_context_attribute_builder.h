// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "abstract_opengl_context_attribute_builder.h"
#include <kwin_export.h>

namespace KWin
{

class KWIN_EXPORT EglContextAttributeBuilder : public AbstractOpenGLContextAttributeBuilder
{
public:
    std::vector<int> build() const override;
};

class KWIN_EXPORT EglOpenGLESContextAttributeBuilder : public AbstractOpenGLContextAttributeBuilder
{
public:
    std::vector<int> build() const override;
};

}
