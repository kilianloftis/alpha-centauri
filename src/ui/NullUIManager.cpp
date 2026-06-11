#include "ui/UIManager.h"
#include "ui/UIWorldMap.h"
#include "ui/UIPanel.h"
#include "ui/UIPopup.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include <iostream>
#include <memory>

namespace ac
{

namespace
{

class NullWorldMap : public UIWorldMap
{
public:
    void Draw(Graphics& /*rGraphics*/) override {}
    void Update(float /*deltaTime*/) override {}
};

class NullInfoPanel : public UIPanel
{
public:
    void Draw(Graphics& /*rGraphics*/) override {}
    void Update(float /*deltaTime*/) override {}
};

class NullPopup : public UIPopup
{
public:
    void Draw(Graphics& /*rGraphics*/) override {}
    void Update(float /*deltaTime*/) override {}
    void Dismiss() override
    {
        m_bVisible = false;
        if (m_onDismiss)
        {
            m_onDismiss();
        }
    }
};

class NullUIManager : public UIManager
{
public:
    NullUIManager()
        : m_pWorldMap(std::make_unique<NullWorldMap>())
        , m_pInfoPanel(std::make_unique<NullInfoPanel>())
        , m_pPopup(std::make_unique<NullPopup>())
    {
    }

    bool Initialize(Graphics& /*rGraphics*/) override
    {
        std::cout << "[UIManager] Null UI initialized. No rendering will occur.\n";
        return true;
    }

    void Draw(Graphics& /*rGraphics*/) override {}
    void Update(float /*deltaTime*/) override {}
    void HandleInput(Input& /*rInput*/) override {}

    UIWorldMap& GetWorldMap() override { return *m_pWorldMap; }
    UIPanel& GetInfoPanel() override { return *m_pInfoPanel; }

    void ShowPopup(const std::string& text, std::function<void()> onDismiss) override
    {
        std::cout << "[UIManager] Popup: " << text << "\n";
        m_pPopup->SetText(text);
        m_pPopup->SetOnDismiss(std::move(onDismiss));
        m_pPopup->SetVisible(true);
    }

    void DismissPopup() override
    {
        m_pPopup->Dismiss();
    }

    bool HasActivePopup() const override
    {
        return m_pPopup->IsVisible();
    }

private:
    std::unique_ptr<NullWorldMap> m_pWorldMap;
    std::unique_ptr<NullInfoPanel> m_pInfoPanel;
    std::unique_ptr<NullPopup> m_pPopup;
};

} // namespace

std::unique_ptr<UIManager> CreateUIManager()
{
    return std::make_unique<NullUIManager>();
}

} // namespace ac
