#include "ui/commlinks/TalkPopup.h"

#include "game/GameState.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "graphics/Graphics.h"
#include "ui/StandingCaptions.h"
#include "ui/commlinks/TalkScript.h"
#include "ui/style/UiStyle.h"
#include "ui/text/TextPack.h"

#include <functional>
#include <utility>

namespace ac
{

TalkPopup::TalkPopup(GameState& rGameState, const TalkScript& rScript, const TextPack& rText,
                     const StandingCaptions& rCaptions, FactionId_t rivalId, uint32_t seed,
                     WindowLayout_t layout)
    : UIElement(layout)
    , m_rGameState(rGameState)
    , m_rText(rText)
    , m_rCaptions(rCaptions)
    , m_rScript(rScript)
    , m_conversation(rGameState, rScript, rText, rCaptions, rivalId, seed)
{
    OpenMenu_();
}

void TalkPopup::Render(Graphics& rGraphics)
{
    DrawFrame_(rGraphics);
    DrawCaption_(rGraphics);
    DrawStanding_(rGraphics);
    DrawSpeech_(rGraphics);
    DrawDraft_(rGraphics);
    m_pMenu->Render(rGraphics);
}

bool TalkPopup::HandleKey(const KeyEvent_t& rEvent)
{
    (void)m_pMenu->HandleKey(rEvent);
    AfterInput_();
    return true;
}

void TalkPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    m_pMenu->HandleMouseClick(rEvent);
    AfterInput_();
}

void TalkPopup::OpenMenu_()
{
    std::vector<PopupChoice_t> choices = m_conversation.Choices();
    for (PopupChoice_t& rChoice : choices)
    {
        rChoice.onChosen = [this, chosen = std::move(rChoice.onChosen)] {
            m_bChose = true;
            chosen();
        };
    }
    std::string prompt = WithoutHighlights(m_conversation.Prompt());
    const WindowLayout_t menuLayout = ResolveLayout(m_layout, Style().talkPopup.menuLayout);
    m_pMenu = std::make_unique<ListSelectorPopup>(std::move(prompt), "", std::move(choices),
                                                  menuLayout, Style().listSelectorPopup, nullptr);
}

void TalkPopup::AfterInput_()
{
    if (!m_pMenu->ShouldClose())
    {
        return;
    }
    if (!m_bChose || m_conversation.IsOver())
    {
        RequestClose();
        return;
    }
    m_bChose = false;
    OpenMenu_();
}

void TalkPopup::DrawFrame_(Graphics& rGraphics) const
{
    const auto& style = Style().talkPopup;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor,
                       Style().outline.width);
}

void TalkPopup::DrawCaption_(Graphics& rGraphics) const
{
    const auto& style = Style().talkPopup;
    DrawAt_(rGraphics, m_conversation.Bindings().Caption().text, style.padX, style.captionPadY,
            style.captionFontSize, style.captionColor);
}

// SMAC's portrait: the rival's raw mood toward the player and its repute; the InfoWin's line
// with the player's own integrity (terranx-map.md, get_mood).
void TalkPopup::DrawStanding_(Graphics& rGraphics) const
{
    const auto& style = Style().talkPopup;
    const DiplomacyLedger& rLedger = m_rGameState.GetDiplomacyLedger();
    const FactionId_t playerId = m_conversation.PlayerId();
    const FactionId_t rivalId = m_conversation.RivalId();
    const TalkPortrait_t& rPortrait = m_rScript.Portrait();
    const int friction = rLedger.GetFriction(rivalId, playerId);
    const int rivalBlemishes = rLedger.GetBlemishes(rivalId);
    const int playerBlemishes = rLedger.GetBlemishes(playerId);
    const std::pair<int, std::string> lines[] = {
        {rPortrait.mood, m_rCaptions.Mood(friction)},
        {rPortrait.reputation, m_rCaptions.Repute(rivalBlemishes)},
        {rPortrait.integrity, m_rCaptions.Repute(playerBlemishes)},
    };
    const float pads[] = {style.moodPadY, style.reputePadY, style.integrityPadY};
    for (std::size_t i = 0; i < std::size(lines); ++i)
    {
        DrawAt_(rGraphics, m_rText.Label(lines[i].first), style.padX, pads[i], style.textFontSize,
                style.labelColor);
        DrawAt_(rGraphics, lines[i].second, style.valuePadX, pads[i], style.textFontSize,
                style.textColor);
    }
}

void TalkPopup::DrawSpeech_(Graphics& rGraphics) const
{
    DrawLines_(rGraphics, m_conversation.Speech(), Style().talkPopup.speechPadY);
}

void TalkPopup::DrawDraft_(Graphics& rGraphics) const
{
    const std::optional<TalkDraftHeadings_t> headings = m_conversation.DraftHeadings();
    if (!headings)
    {
        return;
    }
    float padY = Style().talkPopup.draftPadY;
    padY = DrawLines_(rGraphics, headings->offer, padY);
    for (const TalkDraftItem_t& rItem : m_conversation.Offer())
    {
        padY = DrawLines_(rGraphics, rItem.label, padY);
    }
    padY = DrawLines_(rGraphics, headings->ask, padY);
    for (const TalkDraftItem_t& rItem : m_conversation.Ask())
    {
        padY = DrawLines_(rGraphics, rItem.label, padY);
    }
}

float TalkPopup::DrawLines_(Graphics& rGraphics, const std::string& rText, float padY) const
{
    const auto& style = Style().talkPopup;
    const float width = m_layout.width * (1.0f - 2.0f * style.padX);
    for (const TextLine_t& rLine : WrapText(rGraphics, rText, style.textFontSize, width))
    {
        DrawLine_(rGraphics, rLine, padY);
        padY += style.lineHeight;
    }
    return padY;
}

void TalkPopup::DrawLine_(Graphics& rGraphics, const TextLine_t& rLine, float padY) const
{
    const auto& style = Style().talkPopup;
    float x = m_layout.x + style.padX * m_layout.width;
    const float y = m_layout.y + padY * m_layout.height;
    for (const TextRun_t& rRun : rLine)
    {
        rGraphics.DrawText(rRun.text, x, y, style.textFontSize,
                           rRun.bHighlight ? style.highlightColor : style.textColor);
        x += rGraphics.MeasureTextWidth(rRun.text, style.textFontSize);
    }
}

void TalkPopup::DrawAt_(Graphics& rGraphics, const std::string& rText, float padX, float padY,
                        unsigned int fontSize, const Color_t& color) const
{
    rGraphics.DrawText(rText, m_layout.x + padX * m_layout.width,
                       m_layout.y + padY * m_layout.height, fontSize, color);
}

} // namespace ac
