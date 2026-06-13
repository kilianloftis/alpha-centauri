#pragma once

#include "ui/UIElement.h"
#include "graphics/Graphics.h"
#include <string>
#include <vector>

namespace ac
{

class UIPanel : public UIElement
{
public:
    struct InfoLine
    {
        std::string text;
        Color color = Color::White();
    };

    ~UIPanel() override = default;

    const std::string& GetTitle() const { return m_title; }
    void SetTitle(const std::string& title) { m_title = title; }

    void SetInfoLines(const std::vector<InfoLine>& lines) { m_infoLines = lines; }
    const std::vector<InfoLine>& GetInfoLines() const { return m_infoLines; }

protected:
    std::string m_title;
    std::vector<InfoLine> m_infoLines;
};

} // namespace ac
