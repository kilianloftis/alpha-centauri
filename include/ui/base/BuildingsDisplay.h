#pragma once

#include "game/buildings/BuildingConfig.h"
#include "graphics/Graphics.h"
#include "ui/UIElement.h"

#include <functional>
#include <vector>

namespace ac
{

class BaseManager;
class Graphics;

using BuildingClickCallback_t = std::function<void(const BuildingConfig_t&)>;

// Vertical list of this base's constructed buildings, plus buildings granted here by a
// continuous GrantBuilding effect. Granted rows use a darker colour; a constructed copy
// that is also granted is prefixed with "*". Every listed row is clickable when a callback
// is supplied; a granted-only copy is not constructed, so scrap quoting denies it.
class BuildingsDisplay : public UIElement
{
public:
    BuildingsDisplay(const BaseManager& rBase,
                     WindowLayout_t layout,
                     BuildingClickCallback_t onBuildingClick = {});

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    struct BuildingRow_t
    {
        const BuildingConfig_t* pConfig = nullptr;
        std::string label;
        Color_t color{};
        WindowLayout_t bounds{};
    };

    std::vector<BuildingRow_t> Rows_() const;

    const BaseManager& m_rBase;
    BuildingClickCallback_t m_onBuildingClick;
};

} // namespace ac
