/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"
#include "core/gbmloader.h"
#include "dmabuftexture.h"
#include <libdrm/drm_fourcc.h>

#include <gbm.h>

#include <QDebug>

namespace KWin
{

inline DmaBufAttributes dmaBufAttributesForBo(gbm_bo *bo)
{
    DmaBufAttributes attributes;
    attributes.planeCount = gbm_bo_get_plane_count(bo);
    attributes.width = gbm_bo_get_width(bo);
    attributes.height = gbm_bo_get_height(bo);
    attributes.format = gbm_bo_get_format(bo);
    attributes.modifier = gbm_bo_get_modifier(bo);

#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    for (int i = 0; i < attributes.planeCount; ++i) {
        if (GbmLoader::loader() && GbmLoader::loader()->gbmBoGetFdForPlane) {
            attributes.fd[i] = FileDescriptor{GbmLoader::loader()->gbmBoGetFdForPlane(bo, i)};
        }
        attributes.offset[i] = gbm_bo_get_offset(bo, i);
        attributes.pitch[i] = gbm_bo_get_stride_for_plane(bo, i);
        if (attributes.pitch[i] == 0) {
            attributes.pitch[i] = gbm_bo_get_width(bo) * 4;
        }
    }
#else
    if (attributes.planeCount > 1) {
        return attributes;
    }

    attributes.fd[0] = FileDescriptor{gbm_bo_get_fd(bo)};
    attributes.offset[0] = gbm_bo_get_offset(bo, 0);
    attributes.pitch[0] = gbm_bo_get_stride_for_plane(bo, 0);
    if (attributes.pitch[0] == 0) {
        attributes.pitch[0] = gbm_bo_get_width(bo) * 4;
    }
#endif

    return attributes;
}

inline DmaBufParams dmaBufParamsForBo(gbm_bo *bo)
{
    DmaBufParams attributes;
    attributes.planeCount = gbm_bo_get_plane_count(bo);
    attributes.width = gbm_bo_get_width(bo);
    attributes.height = gbm_bo_get_height(bo);
    attributes.format = gbm_bo_get_format(bo);
    attributes.modifier = gbm_bo_get_modifier(bo);
    return attributes;
}

inline gbm_bo *createGbmBo(gbm_device *device, const QSize &size, quint32 format, const QVector<uint64_t> &modifiers)
{
    gbm_bo *bo = nullptr;
    if (modifiers.count() > 0 && !(modifiers.count() == 1 && modifiers[0] == DRM_FORMAT_MOD_INVALID)) {
#if HAVE_GBM_BO_CREATE_WITH_MODIFIERS2
        if (GbmLoader::loader() && GbmLoader::loader()->createWithModifiers2) {
            qDebug() << "createWithModifiers2 " << device << size << format << modifiers;
            bo = GbmLoader::loader()->createWithModifiers2(device,
                                           size.width(),
                                           size.height(),
                                           format,
                                           modifiers.constData(), modifiers.count(),
                                           GBM_BO_USE_RENDERING);
        }
#else
        if (GbmLoader::loader() && GbmLoader::loader()->createWithModifiers) {
            qDebug() << "createWithModifiers " << device << size << format << modifiers;
            bo = GbmLoader::loader()->createWithModifiers(device,
                                          size.width(),
                                          size.height(),
                                          format,
                                          modifiers.constData(), modifiers.count());
        }
#endif
    }

    if (!bo && (modifiers.isEmpty() || modifiers.contains(DRM_FORMAT_MOD_INVALID))) {
        bo = gbm_bo_create(device,
                           size.width(),
                           size.height(),
                           format,
                           GBM_BO_USE_LINEAR);
    }
    return bo;
}

} // namespace KWin
