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

bool UIManager::Initialize(Graphics& rGraphics, Input& rInput)
{
    m_pGraphics = &rGraphics;
    m_pInput = &rInput;
    return true;
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
    m_pInput->CaptureKeyAsync([this](KeyEvent_t event)
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
    m_pInput->CaptureMouseAsync([this](MouseEvent_t event)
    {
        IGameView* pActive = GetActiveView_();
        if (pActive)
        {
            pActive->HandleMouse(event);
        }
    });
}

void UIManager::Render()
{
    if (!m_pGraphics)
    {
        return;
    }
    m_pGraphics->Clear();
    if (m_pWorldView)
    {
        m_pWorldView->Render(*m_pGraphics);
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
        pView->Render(*m_pGraphics);
    }
    m_pGraphics->Display();
}

void UIManager::PushView(std::unique_ptr<IGameView> pView)
{
    pView->OnPushed(*m_pGraphics);
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

bool UIManager::ShouldExit() const
{
    return m_bShouldExit;
}

void UIManager::RequestExit()
{
    m_bShouldExit = true;
}

} // namespace ac
