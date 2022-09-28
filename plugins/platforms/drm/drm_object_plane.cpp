/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Roman Gilg <subdiff@gmail.com>

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
#include "drm_object_plane.h"
#include "drm_buffer.h"
#include "drm_pointer.h"
#include "logging.h"

namespace KWin
{

DrmPlane::DrmPlane(uint32_t plane_id, int fd)
    : DrmObject(plane_id, fd)
{
}

DrmPlane::~DrmPlane()
{
    delete m_current;
    delete m_next;
}

bool DrmPlane::atomicInit()
{
    qCDebug(KWIN_DRM) << "Atomic init for plane:" << m_id;
    ScopedDrmPointer<_drmModePlane, &drmModeFreePlane> p(drmModeGetPlane(fd(), m_id));

    if (!p) {
        qCWarning(KWIN_DRM) << "Failed to get kernel plane" << m_id;
        return false;
    }

    m_possibleCrtcs = p->possible_crtcs;

    int count_formats = p->count_formats;
    m_formats.resize(count_formats);
    for (int i = 0; i < count_formats; i++) {
        m_formats[i] = p->formats[i];
    }

    if (!initProps()) {
        return false;
    }
    initFormatsWithModifiers();
    //dumpFormatsWithModifiers();
    return true;
}

bool DrmPlane::initProps()
{
    setPropertyNames( {
        QByteArrayLiteral("type"),
        QByteArrayLiteral("SRC_X"),
        QByteArrayLiteral("SRC_Y"),
        QByteArrayLiteral("SRC_W"),
        QByteArrayLiteral("SRC_H"),
        QByteArrayLiteral("CRTC_X"),
        QByteArrayLiteral("CRTC_Y"),
        QByteArrayLiteral("CRTC_W"),
        QByteArrayLiteral("CRTC_H"),
        QByteArrayLiteral("FB_ID"),
        QByteArrayLiteral("CRTC_ID"),
        QByteArrayLiteral("rotation")
    });

    QVector<QByteArray> typeNames = {
        QByteArrayLiteral("Primary"),
        QByteArrayLiteral("Cursor"),
        QByteArrayLiteral("Overlay"),
    };

    const QVector<QByteArray> rotationNames{
        QByteArrayLiteral("rotate-0"),
        QByteArrayLiteral("rotate-90"),
        QByteArrayLiteral("rotate-180"),
        QByteArrayLiteral("rotate-270"),
        QByteArrayLiteral("reflect-x"),
        QByteArrayLiteral("reflect-y")
    };

    drmModeObjectProperties *properties = drmModeObjectGetProperties(fd(), m_id, DRM_MODE_OBJECT_PLANE);
    if (!properties){
        qCWarning(KWIN_DRM) << "Failed to get properties for plane " << m_id ;
        return false;
    }

    int propCount = int(PropertyIndex::Count);
    for (int j = 0; j < propCount; ++j) {
        if (j == int(PropertyIndex::Type)) {
            initProp(j, properties, typeNames);
        } else if (j == int(PropertyIndex::Rotation)) {
            initProp(j, properties, rotationNames);
            m_supportedTransformations = Transformations();
            auto testTransform = [j, this] (uint64_t value, Transformation t) {
                if (propHasEnum(j, value)) {
                    m_supportedTransformations |= t;
                }
            };
            testTransform(0, Transformation::Rotate0);
            testTransform(1, Transformation::Rotate90);
            testTransform(2, Transformation::Rotate180);
            testTransform(3, Transformation::Rotate270);
            testTransform(4, Transformation::ReflectX);
            testTransform(5, Transformation::ReflectY);
            qCDebug(KWIN_DRM) << "Supported Transformations: " << m_supportedTransformations << " on plane " << m_id;
        } else {
            initProp(j, properties);
        }
    }

    drmModeFreeObjectProperties(properties);
    return true;
}

void DrmPlane::dumpFormatsWithModifiers()
{
    for (auto it_h = m_formatsWithModifiers.constBegin(); it_h != m_formatsWithModifiers.constEnd(); ++it_h){
        uint32_t format = it_h.key();
        QVector<uint64_t> modifiers = it_h.value();
        qDebug("plane = %d, drm dump format = %d", m_id, format);
        for (auto it_s = modifiers.constBegin(); it_s != modifiers.constEnd(); it_s++ ) {
            qDebug("        ---------- plane = %d, drm dump modifier = %ld", m_id, *it_s);
        }
    }
}

void DrmPlane::initFormatsWithModifiers()
{
    drmModeObjectProperties *props =
            drmModeObjectGetProperties(m_fd, m_id, DRM_MODE_OBJECT_PLANE);

    uint32_t idx = 0;
    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyPtr property = drmModeGetProperty(m_fd, props->props[i]);
        if (!strcmp(property->name, "IN_FORMATS")) {
            idx = i;
        }
        drmModeFreeProperty(property);

        if (idx)
            break;
    }

    uint32_t blob_id = props->prop_values[idx];
    drmModePropertyBlobPtr blob = drmModeGetPropertyBlob(m_fd, blob_id);
    if(blob) {
        struct drm_format_modifier_blob *header = static_cast<drm_format_modifier_blob *>(blob->data);
        uint32_t *formats = (uint32_t *)((char *)header + header->formats_offset);
        struct drm_format_modifier *modifiers = (struct drm_format_modifier *)
                ((char *)header + header->modifiers_offset);

        for (uint32_t m = 0; m < header->count_formats; m++) {
            if (!header->count_modifiers) {
                m_formatsWithModifiers.insert(formats[m], QVector<uint64_t>());
                continue;
            }

            QVector<uint64_t> modifierVector;
            for (uint32_t n = 0; n < header->count_modifiers; n++) {
                modifierVector.append(modifiers[n].modifier);
            }
            m_formatsWithModifiers.insert(formats[m], modifierVector);
        }
    }
    drmModeFreeObjectProperties(props);
}

DrmPlane::TypeIndex DrmPlane::type()
{
    auto property = m_props.at(int(PropertyIndex::Type));
    if (!property) {
        return TypeIndex::Overlay;
    }
    int typeCount = int(TypeIndex::Count);
    for (int i = 0; i < typeCount; i++) {
            if (property->enumMap(i) == property->value()) {
                    return TypeIndex(i);
            }
    }
    return TypeIndex::Overlay;
}

void DrmPlane::setNext(DrmBuffer *b)
{
    if (auto property = m_props.at(int(PropertyIndex::FbId))) {
        property->setValue(b ? b->bufferId() : 0);
    }
    m_next = b;
}

void DrmPlane::setTransformation(Transformations t)
{
    qDebug() << "-----------" << __PRETTY_FUNCTION__ << t;
    if (auto property = m_props.at(int(PropertyIndex::Rotation))) {
        property->setValue(int(t));
    }
}

DrmPlane::Transformations DrmPlane::transformation()
{
    if (auto property = m_props.at(int(PropertyIndex::Rotation))) {
        return Transformations(int(property->value()));
    }
    return Transformations(Transformation::Rotate0);
}

bool DrmPlane::atomicPopulate(drmModeAtomicReq *req)
{
    bool ret = true;

    for (int i = 1; i < m_props.size(); i++) {
        auto property = m_props.at(i);
        if (!property) {
            continue;
        }
        ret &= atomicAddProperty(req, property);
    }

    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to populate atomic plane" << m_id;
        return false;
    }
    return true;
}

void DrmPlane::flipBuffer()
{
    m_current = m_next;
    m_next = nullptr;
}

void DrmPlane::flipBufferWithDelete()
{
    if (m_current != m_next) {
        delete m_current;
    }
    flipBuffer();
}

}
