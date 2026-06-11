#pragma once

#include "ui/UIElement.h"
#include <functional>
#include <string>

namespace ac
{

class UIPopup : public UIElement
{
public:
    ~UIPopup() override = default;

    const std::string& GetText() const { return m_text; }
    void SetText(const std::string& text) { m_text = text; }

    void SetOnDismiss(std::function<void()> callback) { m_onDismiss = std::move(callback); }

    virtual void Dismiss() = 0;

protected:
    std::string m_text;
    std::function<void()> m_onDismiss;
};

} // namespace ac
