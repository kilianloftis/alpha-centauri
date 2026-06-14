#pragma once

#include "ui/UIWorldMap.h"

namespace ac
{

class WorldDisplay;

class WorldMapElement : public UIWorldMap
{
public:
    void SetWorldDisplay(WorldDisplay* pWorldDisplay) override;
    void Draw(Graphics& rGraphics) override;
    void Update(float deltaTime) override;

private:
    WorldDisplay* m_pWorldDisplay = nullptr;
};

} // namespace ac
