#pragma once

#include "ui/ListSelectorPopup.h"
#include "ui/TextWrap.h"
#include "ui/UIElement.h"
#include "ui/commlinks/TalkConversation.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ac
{

class GameState;
class StandingCaptions;
class TextPack;
class TalkScript;
struct Color_t;

// Face-to-face talk with one rival: its caption and standing toward the player, its last line,
// the trade being built, and the scene's menu.
class TalkPopup : public UIElement
{
public:
    TalkPopup(GameState& rGameState, const TalkScript& rScript, const TextPack& rText,
              const StandingCaptions& rCaptions, FactionId_t rivalId, uint32_t seed,
              WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void OpenMenu_();
    // A chosen row leads to the next menu; a dismissed menu or the end scene closes the talk.
    // Runs after the menu's own handler has returned, since that handler is still on the stack
    // while a choice runs.
    void AfterInput_();

    void DrawFrame_(Graphics& rGraphics) const;
    void DrawCaption_(Graphics& rGraphics) const;
    void DrawStanding_(Graphics& rGraphics) const;
    void DrawSpeech_(Graphics& rGraphics) const;
    void DrawDraft_(Graphics& rGraphics) const;
    // Returns the pad below the lines drawn.
    float DrawLines_(Graphics& rGraphics, const std::string& rText, float padY) const;
    void DrawLine_(Graphics& rGraphics, const TextLine_t& rLine, float padY) const;
    // padX and padY are fractions of the popup's width and height.
    void DrawAt_(Graphics& rGraphics, const std::string& rText, float padX, float padY,
                 unsigned int fontSize, const Color_t& color) const;

    const GameState& m_rGameState;
    const TextPack& m_rText;
    const StandingCaptions& m_rCaptions;
    const TalkScript& m_rScript;
    TalkConversation m_conversation;
    std::unique_ptr<ListSelectorPopup> m_pMenu;
    bool m_bChose = false;
};

} // namespace ac
