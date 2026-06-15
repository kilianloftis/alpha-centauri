#pragma once

#include "ui/UIPanel.h"

namespace ac
{

class InfoPanelElement : public UIPanel
{
public:
    void Draw(Graphics& rGraphics) override;
    void Update(float deltaTime) override;
};

} // namespace ac
