#include "kwineffectsex.h"

namespace KWin
{

EffectsHandlerEx::EffectsHandlerEx(CompositingType type)
    : EffectsHandler (type)
{
    KWin::effectsEx = this;
}

EffectsHandlerEx::~EffectsHandlerEx()
{
    KWin::effectsEx = nullptr;
}

EffectsHandlerEx* effectsEx = nullptr;

EffectFrameEx::EffectFrameEx()
    : EffectFrame()
    // , QObject(nullptr)
{

}

EffectFrameEx::~EffectFrameEx()
{

}

}
