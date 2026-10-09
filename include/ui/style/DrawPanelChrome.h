#pragma once

#include "graphics/Graphics.h"
#include "ui/UIElement.h"

#include <string>

namespace ac
{

class SpriteLibrary;

// Panel backdrop: native-size background sprite at the layout origin when the path loads,
// otherwise a filled rect. Optional colour border is drawn afterward (width 0 skips it).
void DrawPanelChrome(Graphics& rGraphics,
                     SpriteLibrary* pSprites,
                     const WindowLayout_t& rLayout,
                     const Color_t& rBackgroundColor,
                     const std::string& rBackgroundSprite,
                     const Color_t& rBorderColor,
                     float borderWidth = 1.0f);

} // namespace ac
