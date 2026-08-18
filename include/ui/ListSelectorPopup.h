#pragma once

#include "input/Input.h"
#include "ui/UIElement.h"
#include "ui/style/UiStyle.h"

#include <functional>
#include <string>
#include <vector>

namespace ac
{

class Graphics;

// One row: what it reads as, and what it does. Callers that pick from a payload vector bind
// the payload into the action rather than keeping a parallel array and an index.
struct PopupChoice_t
{
    std::string label;
    std::function<void()> onChosen;
};

// The one modal list-selector: a title, a column of choices, click or Escape.
//
// Every popup that offers "pick one of these" uses this, which is what keeps dismiss,
// hit-testing, clipping and the empty-list message from drifting apart.
class ListSelectorPopup : public UIElement
{
public:
    // rStyle lets a screen keep its own colours and metrics without a second widget; it must
    // outlive the popup (they are all members of the loaded UiStyle).
    // Throws if any choice has no action: a row whose click does nothing is a programmer error,
    // and silently swallowing the click leaves the popup open with no feedback.
    ListSelectorPopup(std::string title,
                      std::string emptyMessage,
                      std::vector<PopupChoice_t> choices,
                      WindowLayout_t layout,
                      const ListSelectorPopupStyle_t& rStyle);

    ~ListSelectorPopup() override = default;

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    // Rows that fit the content area below the header. Bounded, so a long list cannot paint
    // past the chrome into space that Contains() does not cover.
    size_t VisibleRowCount_() const;
    void CacheEntryRects_();

    std::string m_title;
    std::string m_emptyMessage;
    std::vector<PopupChoice_t> m_choices;
    const ListSelectorPopupStyle_t& m_rStyle;

    // Index of the first row drawn; the arrow keys move it.
    size_t m_scrollOffset = 0;
    std::vector<Rectangle_t> m_entryRects;
};

} // namespace ac
