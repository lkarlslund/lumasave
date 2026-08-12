// SPDX-License-Identifier: MIT
#include "lumasaveeffect.h"

namespace KWin
{
KWIN_EFFECT_FACTORY_SUPPORTED(LumaSaveEffect, "metadata.json", return LumaSaveEffect::supported();)
}

#include "main.moc"
