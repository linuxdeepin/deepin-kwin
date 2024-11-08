/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/colorlut.h"
#include "core/output.h"
#include "drm_object.h"
#include "drm_blob.h"

#include <QPoint>
#include <memory>

namespace KWin
{

class DrmBackend;
class DrmFramebuffer;
class DrmDumbBuffer;
class DrmGpu;
class DrmPlane;

class DrmCrtc : public DrmObject
{
public:
    DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex, DrmPlane *primaryPlane, DrmPlane *cursorPlane);

    enum class PropertyIndex : uint32_t {
        ModeId = 0,
        Active,
        VrrEnabled,
        Gamma_LUT,
        Gamma_LUT_Size,
        CTM,
        Count
    };

    bool init() override;
    void disable() override;

    int pipeIndex() const;
    int gammaRampSize() const;
    DrmPlane *primaryPlane() const;
    DrmPlane *cursorPlane() const;
    drmModeModeInfo queryCurrentMode();

    std::shared_ptr<DrmFramebuffer> current() const;
    std::shared_ptr<DrmFramebuffer> next() const;
    void setCurrent(const std::shared_ptr<DrmFramebuffer> &buffer);
    void setNext(const std::shared_ptr<DrmFramebuffer> &buffer);
    void flipBuffer();
    void releaseBuffers();

    bool hasCTM() const;
    bool hasColorMode() const;

private:
    DrmUniquePtr<drmModeCrtc> m_crtc;
    std::shared_ptr<DrmFramebuffer> m_currentBuffer;
    std::shared_ptr<DrmFramebuffer> m_nextBuffer;
    int m_pipeIndex;
    DrmPlane *m_primaryPlane;
    DrmPlane *m_cursorPlane;
    bool m_ctmEnabled = false;
};

class DrmGammaRamp : public DrmBlob<DrmCrtc, DrmCrtc::PropertyIndex::Gamma_LUT>
{
public:
    DrmGammaRamp(DrmCrtc *crtc, const std::shared_ptr<ColorTransformation> &transformation);
    DrmGammaRamp(DrmCrtc *crtc, const Output::ColorCurves &colorCurves);

    const ColorLUT &lut() const;

private:
    void init(DrmCrtc *crtc);

    const ColorLUT m_lut;
};

class DrmCTM : public DrmBlob<DrmCrtc, DrmCrtc::PropertyIndex::CTM>
{
public:
    DrmCTM(DrmCrtc *crtc, const Output::CtmValue &ctmValue);

    const Output::CtmValue &ctmValue() const;

private:
    const Output::CtmValue m_ctmValue;
};

class DrmColorMode : public DrmBlob<DrmCrtc, DrmCrtc::PropertyIndex::Gamma_LUT>
{
public:
    DrmColorMode(DrmCrtc *crtc, const Output::ColorMode &colorMode);

    const Output::ColorMode &colorModeValue() const;

private:
    const Output::ColorMode m_colorModeValue;
};

}
