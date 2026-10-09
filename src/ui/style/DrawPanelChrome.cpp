#include "ui/style/DrawPanelChrome.h"

#include "ui/SpriteLibrary.h"

namespace ac
{

void DrawPanelChrome(Graphics& rGraphics,
                     SpriteLibrary* pSprites,
                     const WindowLayout_t& rLayout,
                     const Color_t& rBackgroundColor,
                     const std::string& rBackgroundSprite,
                     const Color_t& rBorderColor,
                     float borderWidth)
{
    const bool bSprite = pSprites && !rBackgroundSprite.empty()
        && pSprites->Ensure(rBackgroundSprite);
    if (bSprite)
    {
        rGraphics.DrawSprite(rBackgroundSprite, rLayout.x, rLayout.y);
    }
    else if (rBackgroundColor.a > 0)
    {
        rGraphics.DrawFilledRect(
            rLayout.x, rLayout.y, rLayout.width, rLayout.height, rBackgroundColor);
    }

    if (borderWidth > 0.0f)
    {
        rGraphics.DrawRect(
            rLayout.x, rLayout.y, rLayout.width, rLayout.height, rBorderColor, borderWidth);
    }
}

} // namespace ac
