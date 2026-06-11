#pragma once

#include "ui/UIElement.h"
#include <string>

namespace ac
{

class UIPanel : public UIElement
{
public:
    ~UIPanel() override = default;

    const std::string& GetTitle() const { return m_title; }
    void SetTitle(const std::string& title) { m_title = title; }

protected:
    std::string m_title;
};

} // namespace ac
