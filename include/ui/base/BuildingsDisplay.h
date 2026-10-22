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
class SpriteLibrary;

using BuildingClickCallback_t = std::function<void(const BuildingConfig_t&)>;

// The base's built and granted buildings. Granted rows are darker, a built copy that is also
// granted gets "*", and granted-only copies cannot be scrapped.
class BuildingsDisplay : public UIElement
{
public:
    BuildingsDisplay(const BaseManager& rBase,
                     SpriteLibrary& rSprites,
                     WindowLayout_t layout,
                     BuildingClickCallback_t onBuildingClick);

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
    SpriteLibrary& m_rSprites;
    BuildingClickCallback_t m_onBuildingClick;
};

} // namespace ac
