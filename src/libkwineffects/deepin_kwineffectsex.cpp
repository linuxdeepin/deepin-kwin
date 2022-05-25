#include "deepin_kwineffectsex.h"

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
}
