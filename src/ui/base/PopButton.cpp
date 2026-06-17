#include "ui/base/PopButton.h"
#include "game/population/pop-types/Pop.h"
#include "graphics/Graphics.h"

namespace ac
{

PopButton::PopButton(const Graphics& rGraphics, const Pop& rPop, ResolvedLayout_t layout, OnClickCallback_t onClick)
    : UIElement(layout, rGraphics)
    , m_rPop(rPop)
    , m_onClick(std::move(onClick))
{}

void PopButton::Render()
{
    // TODO: Draw button background/highlight when hovered/selected
    m_rGraphics.DrawText(m_rPop.GetPopType(), m_layout.x, m_layout.y, static_cast<unsigned int>(m_layout.height * 0.6f));
}

void PopButton::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    if (m_onClick)
    {
        m_onClick(m_rPop);
    }
}

} // namespace ac
