#pragma once

#include "ui/UIElement.h"

namespace ac
{

class BaseManager;
class Faction;
class Graphics;

class GrowthDisplay : public UIElement
{
public:
    // pFaction supplies the faction-wide effect pool so the displayed growth threshold
    // includes GrowthRate modifiers (e.g. the social engineering Growth rating), matching
    // what ApplyGrowth will actually require at turn end.
    GrowthDisplay(
        const BaseManager* pBase,
        const Faction* pFaction,
        WindowLayout_t layout
    );
    ~GrowthDisplay() override = default;

    void Render(Graphics& rGraphics) override;

private:
    const BaseManager* m_pBase = nullptr;
    const Faction* m_pFaction = nullptr;
};

} // namespace ac
