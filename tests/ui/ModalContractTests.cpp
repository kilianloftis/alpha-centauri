#include "ui/IGameView.h"
#include "ui/UIElement.h"
#include "input/Input.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

using namespace ac;

namespace
{

class StubModalElement_ : public UIElement
{
public:
    StubModalElement_(WindowLayout_t layout, bool bModal)
        : UIElement(layout)
        , m_bModal(bModal)
    {}

    void Render(Graphics&) override {}
    bool IsModal() const override { return m_bModal; }

    bool HandleKey(const KeyEvent_t& rEvent) override
    {
        lastKey = rEvent.key;
        ++keyCount;
        if (rEvent.key == Key_t::Escape)
        {
            RequestClose();
            return true;
        }
        return false;
    }

    void HandleMouseClick(const MouseEvent_t& rEvent) override
    {
        lastClickX = rEvent.x;
        lastClickY = rEvent.y;
        ++clickCount;
        if (!Contains(static_cast<float>(rEvent.x), static_cast<float>(rEvent.y)))
        {
            RequestClose();
        }
    }

    bool m_bModal = false;
    int clickCount = 0;
    int keyCount = 0;
    int lastClickX = -1;
    int lastClickY = -1;
    Key_t lastKey = Key_t::Unknown;
};

class StubPanel_ : public UIElement
{
public:
    using UIElement::UIElement;
    void Render(Graphics&) override {}
    void HandleMouseClick(const MouseEvent_t&) override { ++clickCount; }
    int clickCount = 0;
};

class TestView_ : public IGameView
{
public:
    TestView_() : IGameView(WindowLayout_t{0, 0, 1000, 1000}) {}

    using IGameView::DismissOpenModals_;
    using IGameView::m_elements;

    StubModalElement_& PushModal_(WindowLayout_t layout)
    {
        auto p = std::make_unique<StubModalElement_>(layout, true);
        StubModalElement_& r = *p;
        m_elements.push_back(std::move(p));
        return r;
    }

    StubPanel_& PushPanel_(WindowLayout_t layout)
    {
        auto p = std::make_unique<StubPanel_>(layout);
        StubPanel_& r = *p;
        m_elements.push_back(std::move(p));
        return r;
    }

    int CountOpenModals_() const
    {
        int count = 0;
        for (const auto& pElement : m_elements)
        {
            if (pElement->IsModal() && !pElement->ShouldClose())
            {
                ++count;
            }
        }
        return count;
    }
};

MouseEvent_t PressAt_(int x, int y)
{
    MouseEvent_t event{};
    event.x = x;
    event.y = y;
    event.bPressed = true;
    event.button = MouseButton_t::Left;
    return event;
}

KeyEvent_t Key_(Key_t key)
{
    KeyEvent_t event{};
    event.key = key;
    return event;
}

} // namespace

TEST_CASE("IGameView routes outside-chrome clicks to the top modal", "[ui][modal]")
{
    TestView_ view;
    StubPanel_& rPanel = view.PushPanel_(WindowLayout_t{0, 0, 1000, 1000});
    StubModalElement_& rModal = view.PushModal_(WindowLayout_t{100, 100, 50, 50});

    REQUIRE(view.HasModalElement());
    REQUIRE(view.BlocksTurnAdvance());

    view.HandleMouse(PressAt_(10, 10)); // outside modal chrome
    CHECK(rModal.clickCount == 1);
    CHECK(rModal.lastClickX == 10);
    CHECK(rPanel.clickCount == 0);
    CHECK(rModal.ShouldClose()); // stub outside-click dismiss
}

TEST_CASE("IGameView exclusive key routing does not fall through under a modal",
          "[ui][modal]")
{
    TestView_ view;
    StubModalElement_& rModal = view.PushModal_(WindowLayout_t{0, 0, 100, 100});

    // Enter is not handled by the stub modal (returns false) — view still reports handled so
    // UIManager will not fall through to global shortcuts.
    CHECK(view.HandleKey(Key_(Key_t::Enter)));
    CHECK(rModal.keyCount == 1);
    CHECK(rModal.lastKey == Key_t::Enter);

    CHECK(view.HandleKey(Key_(Key_t::Escape)));
    CHECK(rModal.ShouldClose());
}

TEST_CASE("DismissOpenModals closes prior modal before a replacement push", "[ui][modal]")
{
    TestView_ view;
    StubModalElement_& rFirst = view.PushModal_(WindowLayout_t{0, 0, 40, 40});
    REQUIRE(view.CountOpenModals_() == 1);

    view.DismissOpenModals_();
    CHECK(rFirst.ShouldClose());
    CHECK(view.CountOpenModals_() == 0);

    view.PushModal_(WindowLayout_t{10, 10, 40, 40});
    CHECK(view.CountOpenModals_() == 1);
}

TEST_CASE("Non-modal elements do not block turn advance", "[ui][modal]")
{
    TestView_ view;
    view.PushPanel_(WindowLayout_t{0, 0, 100, 100});
    CHECK_FALSE(view.HasModalElement());
    CHECK_FALSE(view.BlocksTurnAdvance());
}
