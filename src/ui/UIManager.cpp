#include "ui/UIManager.h"

#include <stdexcept>
#include "ui/IGameView.h"
#include "ui/UIElement.h"
#include "ui/IWorldView.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include <memory>
#include <vector>

namespace ac
{

UIManager::UIManager(Graphics& rGraphics, Input& rInput)
    : m_rGraphics(rGraphics)
    , m_rInput(rInput)
{
}

// Out-of-line so callers that only forward-declare WorldView (per UIManager.h) don't need its
// complete type just to destroy a UIManager.
UIManager::~UIManager() = default;

void UIManager::SetWorldView(std::unique_ptr<IWorldView> pWorldView)
{
    m_pWorldView = std::move(pWorldView);
}

IGameView* UIManager::GetActiveView_()
{
    if (!m_overlayStack.empty())
    {
        return m_overlayStack.back().get();
    }
    return m_pWorldView.get();
}

void UIManager::ProcessInput()
{
    PruneClosedViews_();
    ProcessKeys_();
    ProcessMouse_();
    // A view closed by the last event must not still be active for Update().
    PruneClosedViews_();
}

void UIManager::PruneClosedViews_()
{
    for (int i = static_cast<int>(m_overlayStack.size()) - 1; i >= 0; --i)
    {
        if (m_overlayStack[i]->ShouldClose())
        {
            m_overlayStack[i]->OnPopped();
            m_overlayStack.erase(m_overlayStack.begin() + i);
        }
    }
}

void UIManager::ProcessKeys_()
{
    while (auto event = m_rInput.PollKey())
    {
        // Per event, not per batch: one keystroke can close the top view, and the next event in
        // the same drain must go to whatever is active *now*.
        PruneClosedViews_();

        IGameView* pActive = GetActiveView_();
        if (pActive && pActive->HandleKey(*event))
        {
            continue;
        }
        // Global view shortcuts (F2 research, E social engineering, ...) must not stack a
        // second overlay while one is already active, and must not push while the world view
        // has a blocking in-view modal (same gate as CanAdvanceTurn).
        if (!CanAdvanceTurn())
        {
            continue;
        }
        HandleGlobalShortcut_(event->key);
    }
}

void UIManager::RegisterViewShortcut(Key_t key, ViewFactory_t factory)
{
    m_shortcutMap[key] = std::move(factory);
}

void UIManager::HandleGlobalShortcut_(Key_t key)
{
    auto it = m_shortcutMap.find(key);
    if (it == m_shortcutMap.end())
    {
        return;
    }

    PushView(it->second());
}

void UIManager::ProcessMouse_()
{
    while (auto event = m_rInput.PollMouse())
    {
        PruneClosedViews_();

        IGameView* pActive = GetActiveView_();
        if (pActive)
        {
            pActive->HandleMouse(*event);
        }
    }
}

void UIManager::Update()
{
    if (!m_pWorldView)
    {
        return;
    }

    // Edge scrolling is an input-driven state change, so it belongs here rather than on the
    // render path, where a second pass (screenshot, minimap) would apply it twice.
    m_pWorldView->UpdateCameraInput(m_overlayStack.empty(), m_rInput.GetLastMousePosition());

    // Only the world view queues an out-of-band advance request (auto end-turn). It must stay
    // queued while the turn is gated: ProcessPendingAutoEndTurn clears the flag before calling
    // through, and Engine::ProcessTurn_'s own gate then returns without advancing, so the
    // request would be lost and never re-armed. WorldView already does this for its in-view
    // modal; the overlay stack is the half it cannot see.
    if (CanAdvanceTurn())
    {
        m_pWorldView->ProcessPendingAutoEndTurn();
    }
}

bool UIManager::CanAdvanceTurn() const
{
    if (HasOverlayView())
    {
        return false;
    }
    if (m_pWorldView && m_pWorldView->BlocksTurnAdvance())
    {
        return false;
    }
    return true;
}

void UIManager::Render()
{
    m_rGraphics.Clear();
    if (m_pWorldView)
    {
        m_pWorldView->Render(m_rGraphics);
    }
    for (const auto& pView : m_overlayStack)
    {
        pView->Render(m_rGraphics);
    }
    m_rGraphics.Display();
}

void UIManager::PushView(std::unique_ptr<IGameView> pView)
{
    if (!pView)
    {
        throw std::invalid_argument("UIManager::PushView was given no view");
    }
    pView->OnPushed(m_rGraphics);
    m_overlayStack.push_back(std::move(pView));
}

void UIManager::PopView()
{
    if (m_overlayStack.empty())
    {
        return;
    }
    m_overlayStack.back()->OnPopped();
    m_overlayStack.pop_back();
}

bool UIManager::HasViews() const
{
    return m_pWorldView != nullptr;
}

bool UIManager::HasOverlayView() const
{
    return !m_overlayStack.empty();
}

bool UIManager::ShouldExit() const
{
    return m_bShouldExit;
}

void UIManager::RequestExit()
{
    m_bShouldExit = true;
}

} // namespace ac
