#include "ui/UIManager.h"
#include "ui/IGameView.h"
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

void UIManager::Update(float deltaTime)
{
    if (m_viewStack.empty())
    {
        return;
    }
    m_viewStack.back()->Update(deltaTime);
}

void UIManager::ProcessInput()
{
    if (m_viewStack.empty())
    {
        return;
    }

    m_pInput->CaptureKeyAsync([this](KeyEvent_t event)
    {
        if (m_viewStack.empty())
        {
            return;
        }
        m_viewStack.back()->HandleKey(event);
    });

    m_pInput->CaptureMouseAsync([this](MouseEvent_t event)
    {
        if (m_viewStack.empty())
        {
            return;
        }
        if (event.button == MouseButton_t::None)
        {
            return;
        }

        IGameView& rTopView = *m_viewStack.back();

        for (UIElement* pElement : rTopView.GetElements())
        {
            if (!pElement || !pElement->IsVisible())
            {
                continue;
            }
            if (pElement->Contains(static_cast<float>(event.x), static_cast<float>(event.y)))
            {
                if (pElement->HandleMouse(event))
                {
                    return;
                }
                break;
            }
        }

        rTopView.HandleMouse(event);
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
        pView->Render(*m_pGraphics);
    }
    m_pGraphics->Display();
}

void UIManager::PushView(std::unique_ptr<IGameView> pView)
{
    pView->OnPushed();
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

std::unique_ptr<UIManager> CreateUIManager()
{
    return std::make_unique<UIManager>();
}

} // namespace ac
