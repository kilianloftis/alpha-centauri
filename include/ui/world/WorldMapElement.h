#pragma once

#include "ui/UIWorldMap.h"

namespace ac
{

class WorldDisplay;

class WorldMapElement : public UIWorldMap
{
public:
    explicit WorldMapElement(WindowLayout_t layout)
        : UIWorldMap(layout)
    {}

    void SetWorldDisplay(WorldDisplay* pWorldDisplay) override;
    void Render(Graphics& rGraphics) override;

private:
    WorldDisplay* m_pWorldDisplay = nullptr;
};

} // namespace ac
