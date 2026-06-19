#include "ui/UIManager.h"
#include "ui/UIGroup.h"
#include "ui/UIElement.h"
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

void UIManager::ProcessInput()
{
    if (!m_viewStack.empty())
    {
        ProcessKeys_();
        ProcessMouse_();
    }
}

void UIManager::ProcessKeys_()
{
    m_pInput->CaptureKeyAsync([this](KeyEvent_t event)
    {
        m_viewStack.back()->HandleKey(event);
    });
}

void UIManager::ProcessMouse_()
{
    m_pInput->CaptureMouseAsync([this](MouseEvent_t event)
    {
        m_viewStack.back()->HandleMouse(event);
    });
}

void UIManager::Render()
{
    if (!m_pGraphics)
    {
        return;
    }
    m_pGraphics->Clear();
    for (auto& pView : m_viewStack)
    {
        if (pView->ShouldClose())
        {
            m_viewStack.pop_back();
            continue;
        }
        pView->Render(*m_pGraphics);
    }
    m_pGraphics->Display();
}

void UIManager::PushView(std::unique_ptr<UIGroup> pView)
{
    pView->OnPushed(*m_pGraphics);
    m_viewStack.push_back(std::move(pView));
}

void UIManager::PopView()
{
    if (m_viewStack.empty())
    {
        return;
    }
    m_viewStack.back()->OnPopped();
    m_viewStack.pop_back();
}

bool UIManager::HasViews() const
{
    return !m_viewStack.empty();
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
