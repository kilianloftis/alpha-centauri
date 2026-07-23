#pragma once

#include "ui/UIElement.h"
#include <string>

namespace ac
{

class Graphics;

// Labeled chrome panel for UI regions that are not implemented yet.
class PlaceholderPanel : public UIElement
{
public:
    PlaceholderPanel(std::string label, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    std::string m_label;
};

} // namespace ac
