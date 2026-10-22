#pragma once

#include "ui/UIElement.h"

#include <functional>
#include <memory>
#include <string>

namespace ac
{

// TODO: move SatelliteLabeledButton to ui/ with its own style section, so this stops reaching
// into the satellite screen.
class SatelliteLabeledButton;
class SpriteLibrary;

// A modal title, message and OK.
class NoticePopup : public UIElement
{
public:
    NoticePopup(WindowLayout_t layout,
                std::string title,
                std::string message,
                std::function<void()> onOk,
                SpriteLibrary* pSprites);
    // Out of line: m_pOkButton's type is only forward-declared here.
    ~NoticePopup() override;

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void Close_();

    std::string m_title;
    std::string m_message;
    std::function<void()> m_onOk;
    SpriteLibrary* m_pSprites = nullptr;
    std::unique_ptr<SatelliteLabeledButton> m_pOkButton;
};

} // namespace ac
