#include "ui/world/WorldMapElement.h"
#include "ui/world/WorldDisplay.h"
#include "graphics/Graphics.h"

namespace ac
{

void WorldMapElement::SetWorldDisplay(WorldDisplay* pWorldDisplay)
{
    m_pWorldDisplay = pWorldDisplay;
}

void WorldMapElement::Render(Graphics& rGraphics)
{
    if (m_pWorldDisplay)
    {
        m_pWorldDisplay->Render(rGraphics, m_layout.x, m_layout.y, m_layout.width, m_layout.height);
    }
    else
    {
        rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{0, 40, 0});
    }
}

} // namespace ac
