#pragma once

#include "ui/UIElement.h"

namespace ac
{

class Faction;

class SocialEngineeringBottomPanel : public UIElement
{
public:
    SocialEngineeringBottomPanel(const Faction& rFaction, WindowLayout_t layout);
    ~SocialEngineeringBottomPanel() override = default;

    void Render(Graphics& rGraphics) override;

private:
    const Faction& m_rFaction;
};

} // namespace ac
