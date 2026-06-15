#include "ui/world/WorldMapElement.h"
#include "ui/world/WorldDisplay.h"
#include "graphics/Graphics.h"

namespace ac
{

void WorldMapElement::SetWorldDisplay(WorldDisplay* pWorldDisplay)
{
    m_pWorldDisplay = pWorldDisplay;
}

void WorldMapElement::Draw(Graphics& rGraphics)
{
    if (m_pWorldDisplay)
    {
        m_pWorldDisplay->Render(m_x, m_y, m_width, m_height);
    }
    else
    {
        rGraphics.DrawFilledRect(m_x, m_y, m_width, m_height, Color{0, 40, 0});
    }
}

void WorldMapElement::Update(float /*deltaTime*/)
{
}

} // namespace ac
