#pragma once

#include "ui/UIPanel.h"
#include "graphics/Graphics.h"
#include <string>
#include <vector>

namespace ac
{

class InfoPanelElement : public UIPanel
{
public:
    InfoPanelElement()
        : UIPanel(kCenterPanelLayout)
    {
    }

    struct InfoLine
    {
        std::string text;
        Color color = Color::White();
    };

    void Draw(Graphics& rGraphics) override;
    void Update(float deltaTime) override;

    void SetInfoLines(const std::vector<InfoLine>& lines) { m_infoLines = lines; }
    const std::vector<InfoLine>& GetInfoLines() const { return m_infoLines; }

private:
    std::vector<InfoLine> m_infoLines;
};

} // namespace ac
