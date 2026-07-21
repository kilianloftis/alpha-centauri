#pragma once

#include "ui/UIElement.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include <string>
#include <vector>

namespace ac
{

class InfoPanelElement : public UIElement
{
public:
    explicit InfoPanelElement(WindowLayout_t layout)
        : UIElement(layout)
    {
    }

    struct InfoLine
    {
        std::string text;
        Color_t color = Style().infoPanel.defaultLineColor;
    };

    void Render(Graphics& rGraphics) override;

    void SetInfoLines(const std::vector<InfoLine>& lines) { m_infoLines = lines; }
    const std::vector<InfoLine>& GetInfoLines() const { return m_infoLines; }

private:
    std::vector<InfoLine> m_infoLines;
};

} // namespace ac
