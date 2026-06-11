#ifdef USE_SFML

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

// ---------------------------------------------------------------------------
// SFMLWorldMap
// ---------------------------------------------------------------------------
class SFMLWorldMap : public UIWorldMap
{
public:
    SFMLWorldMap()
    {
        m_bVisible = true;
    }

    void Draw(Graphics& rGraphics) override
    {
        if (!m_bVisible)
        {
            return;
        }

        // Placeholder: draw a dark green rectangle representing the world map
        // Once sprites are available, this will render actual terrain tiles.
        rGraphics.DrawFilledRect(m_x, m_y, m_width, m_height, 0, 40, 0);
        rGraphics.DrawText("[ World Map ]", m_x + 10.f, m_y + 10.f, 20);
    }

    void Update(float /*deltaTime*/) override
    {
    }
};

// ---------------------------------------------------------------------------
// SFMLInfoPanel
// ---------------------------------------------------------------------------
class SFMLInfoPanel : public UIPanel
{
public:
    SFMLInfoPanel()
    {
        m_title = "Info";
        m_bVisible = true;
    }

    void Draw(Graphics& rGraphics) override
    {
        if (!m_bVisible)
        {
            return;
        }

        // Draw a dark bar at the configured position
        rGraphics.DrawFilledRect(m_x, m_y, m_width, m_height, 20, 20, 40);
        rGraphics.DrawRect(m_x, m_y, m_width, m_height, 100, 100, 160);
        rGraphics.DrawText(m_title, m_x + 10.f, m_y + 5.f, 16);
    }

    void Update(float /*deltaTime*/) override
    {
    }
};

// ---------------------------------------------------------------------------
// SFMLPopup
// ---------------------------------------------------------------------------
class SFMLPopup : public UIPopup
{
public:
    SFMLPopup()
    {
        m_bVisible = false;
    }

    void Draw(Graphics& rGraphics) override
    {
        if (!m_bVisible)
        {
            return;
        }

        // Draw popup background with border
        rGraphics.DrawFilledRect(m_x, m_y, m_width, m_height, 30, 30, 60, 230);
        rGraphics.DrawRect(m_x, m_y, m_width, m_height, 180, 180, 220);

        rGraphics.DrawText(m_text, m_x + 20.f, m_y + 20.f, 18);

        // Dismiss button area
        float btnX = m_x + (m_width - 160.f) / 2.f;
        float btnY = m_y + m_height - 45.f;
        rGraphics.DrawFilledRect(btnX, btnY, 160.f, 30.f, 60, 60, 100);
        rGraphics.DrawRect(btnX, btnY, 160.f, 30.f, 180, 180, 220);
        rGraphics.DrawText("[Enter] Dismiss", btnX + 10.f, btnY + 5.f, 14);
    }

    void Update(float /*deltaTime*/) override
    {
    }

    void Dismiss() override
    {
        m_bVisible = false;
        if (m_onDismiss)
        {
            m_onDismiss();
        }
    }
};

// ---------------------------------------------------------------------------
// SFMLUIManager
// ---------------------------------------------------------------------------
class SFMLUIManager : public UIManager
{
public:
    SFMLUIManager()
        : m_pWorldMap(std::make_unique<SFMLWorldMap>())
        , m_pInfoPanel(std::make_unique<SFMLInfoPanel>())
        , m_pPopup(std::make_unique<SFMLPopup>())
    {
    }

    bool Initialize(Graphics& /*rGraphics*/) override
    {
        // Layout constants (800x600 window)
        const float windowWidth = 800.f;
        const float windowHeight = 600.f;
        const float infoPanelHeight = 80.f;

        // World map fills everything above the info panel
        m_pWorldMap->SetPosition(0.f, 0.f);
        m_pWorldMap->SetSize(windowWidth, windowHeight - infoPanelHeight);

        // Info panel sits at the bottom
        m_pInfoPanel->SetPosition(0.f, windowHeight - infoPanelHeight);
        m_pInfoPanel->SetSize(windowWidth, infoPanelHeight);

        // Popup centered on screen
        const float popupWidth = 400.f;
        const float popupHeight = 200.f;
        m_pPopup->SetPosition((windowWidth - popupWidth) / 2.f, (windowHeight - popupHeight) / 2.f);
        m_pPopup->SetSize(popupWidth, popupHeight);

        std::cout << "[UIManager] SFML UI initialized.\n";
        return true;
    }

    void Draw(Graphics& rGraphics) override
    {
        // Draw in layer order: world map (bottom), info panel, popup (top)
        m_pWorldMap->Draw(rGraphics);
        m_pInfoPanel->Draw(rGraphics);

        if (m_pPopup->IsVisible())
        {
            m_pPopup->Draw(rGraphics);
        }
    }

    void Update(float deltaTime) override
    {
        m_pWorldMap->Update(deltaTime);
        m_pInfoPanel->Update(deltaTime);
        m_pPopup->Update(deltaTime);
    }

    void HandleInput(Input& rInput) override
    {
        if (!m_pPopup->IsVisible())
        {
            return;
        }

        // If a popup is active, consume Enter to dismiss it
        auto key = rInput.CaptureKey();
        if (key.has_value() && *key == Key::Enter)
        {
            m_pPopup->Dismiss();
        }
    }

    UIWorldMap& GetWorldMap() override
    {
        return *m_pWorldMap;
    }

    UIPanel& GetInfoPanel() override
    {
        return *m_pInfoPanel;
    }

    void ShowPopup(const std::string& text, std::function<void()> onDismiss) override
    {
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
    std::unique_ptr<SFMLWorldMap> m_pWorldMap;
    std::unique_ptr<SFMLInfoPanel> m_pInfoPanel;
    std::unique_ptr<SFMLPopup> m_pPopup;
};

std::unique_ptr<UIManager> CreateUIManager()
{
    return std::make_unique<SFMLUIManager>();
}

} // namespace ac

#endif // USE_SFML
