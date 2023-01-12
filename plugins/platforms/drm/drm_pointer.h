// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_DRM_POINTER_H
#define KWIN_DRM_POINTER_H

#include <QScopedPointer>

#include <xf86drmMode.h>

namespace KWin
{

template <typename Pointer, void (*cleanupFunc)(Pointer*)>
struct DrmCleanup
{
    static inline void cleanup(Pointer *ptr)
    {
        cleanupFunc(ptr);
    }
};
template <typename T, void (*cleanupFunc)(T*)> using ScopedDrmPointer = QScopedPointer<T, DrmCleanup<T, cleanupFunc>>;

template <typename T>
struct DrmDeleter;

template <>
struct DrmDeleter<drmModeAtomicReq>
{
    static void cleanup(drmModeAtomicReq *req)
    {
        drmModeAtomicFree(req);
    }
};

template <>
struct DrmDeleter<drmModeConnector>
{
    static void cleanup(drmModeConnector *connector)
    {
        drmModeFreeConnector(connector);
    }
};

template <>
struct DrmDeleter<drmModeCrtc>
{
    static void cleanup(drmModeCrtc *crtc)
    {
        drmModeFreeCrtc(crtc);
    }
};

template <>
struct DrmDeleter<drmModeFB>
{
    static void cleanup(drmModeFB *fb)
    {
        drmModeFreeFB(fb);
    }
};

template <>
struct DrmDeleter<drmModeEncoder>
{
    static void cleanup(drmModeEncoder *encoder)
    {
        drmModeFreeEncoder(encoder);
    }
};

template <>
struct DrmDeleter<drmModeModeInfo>
{
    static void cleanup(drmModeModeInfo *info)
    {
        drmModeFreeModeInfo(info);
    }
};

template <>
struct DrmDeleter<drmModeObjectProperties>
{
    static void cleanup(drmModeObjectProperties *properties)
    {
        drmModeFreeObjectProperties(properties);
    }
};

template <>
struct DrmDeleter<drmModePlane>
{
    static void cleanup(drmModePlane *plane)
    {
        drmModeFreePlane(plane);
    }
};

template <>
struct DrmDeleter<drmModePlaneRes>
{
    static void cleanup(drmModePlaneRes *resources)
    {
        drmModeFreePlaneResources(resources);
    }
};

template <>
struct DrmDeleter<drmModePropertyRes>
{
    static void cleanup(drmModePropertyRes *property)
    {
        drmModeFreeProperty(property);
    }
};

template <>
struct DrmDeleter<drmModePropertyBlobRes>
{
    static void cleanup(drmModePropertyBlobRes *blob)
    {
        drmModeFreePropertyBlob(blob);
    }
};

template <>
struct DrmDeleter<drmModeRes>
{
    static void cleanup(drmModeRes *resources)
    {
        drmModeFreeResources(resources);
    }
};

template <typename T>
using DrmScopedPointer = QScopedPointer<T, DrmDeleter<T>>;

}

#endif

