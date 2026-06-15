#pragma once

#include "ui/base/IBasePanel.h"
#include <string>

namespace ac
{

class BaseManager;
class Graphics;

class BaseDisplay : public IBasePanel
{
public:
    BaseDisplay(const BaseManager& rBase, Graphics& rGraphics);

    void SetLastClickedTileText(const std::string& text);

    void Render(Graphics& rGraphics) override;

private:
    static constexpr float kTextX = 20.f;
    static constexpr float kNameY = 40.f;
    static constexpr float kNutrientsY = 70.f;
    static constexpr float kMineralsY = 90.f;
    static constexpr float kEnergyY = 110.f;
    static constexpr float kStatusY = 570.f;

    const BaseManager& m_rBase;
    Graphics& m_rGraphics;
    std::string m_lastClickedTileText;
};

} // namespace ac
