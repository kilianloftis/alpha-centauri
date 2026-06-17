#pragma once

#include "ui/UIElement.h"
#include <string>

namespace ac
{

class UIPopup : public UIElement
{
public:
    ~UIPopup() override = default;

    const std::string& GetText() const { return m_text; }
    void SetText(const std::string& text) { m_text = text; }

protected:
    std::string m_text;
};

} // namespace ac
