#pragma once

#include "ui/UIElement.h"

namespace ac
{

class WorldDisplay;

class UIWorldMap : public UIElement
{
public:
    explicit UIWorldMap(WindowLayout_t layout)
        : UIElement(layout)
    {}
    ~UIWorldMap() override = default;

    virtual void SetWorldDisplay(WorldDisplay* pWorldDisplay) {}
};

} // namespace ac
