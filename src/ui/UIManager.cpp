#include "ui/UIManager.h"
#include "ui/IGameView.h"
#include "ui/UIElement.h"
#include "ui/world/WorldView.h"
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

void UIManager::SetWorldView(std::unique_ptr<WorldView> pWorldView)
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
    if (GetActiveView_())
    {
        ProcessKeys_();
        ProcessMouse_();
    }
}

void UIManager::ProcessKeys_()
{
    m_rInput.CaptureKeyAsync([this](KeyEvent_t event)
    {
        IGameView* pActive = GetActiveView_();
        if (pActive && pActive->HandleKey(event))
        {
            return;
        }
        HandleGlobalShortcut_(event.key);
    });
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

    if (auto pView = it->second())
    {
        PushView(std::move(pView));
    }
}

void UIManager::ProcessMouse_()
{
    while (auto event = m_rInput.CaptureMouse())
    {
        IGameView* pActive = GetActiveView_();
        if (pActive)
        {
            pActive->HandleMouse(*event);
        }
    }
}

void UIManager::Render()
{
    m_rGraphics.Clear();
    if (m_pWorldView)
    {
        m_pWorldView->UpdateCameraInput(m_overlayStack.empty());
        m_pWorldView->Render(m_rGraphics);
    }
    for (int i = static_cast<int>(m_overlayStack.size()) - 1; i >= 0; --i)
    {
        if (m_overlayStack[i]->ShouldClose())
        {
            m_overlayStack[i]->OnPopped();
            m_overlayStack.erase(m_overlayStack.begin() + i);
        }
    }
    for (const auto& pView : m_overlayStack)
    {
        pView->Render(m_rGraphics);
    }
    m_rGraphics.Display();
}

void UIManager::PushView(std::unique_ptr<IGameView> pView)
{
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
