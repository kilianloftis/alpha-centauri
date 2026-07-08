#ifdef USE_SFML

#include "graphics/Graphics.h"
#include "input/KeyMapping.h"
#include "input/KeyEventQueue.h"
#include "input/MouseEventQueue.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>

namespace ac
{

static const std::string k_fontPath1 = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
static const std::string k_fontPath2 = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";

class SFMLGraphics : public Graphics
{
public:
    SFMLGraphics()
        : m_window(sf::VideoMode(sf::Vector2u(1280, 900)), "Alpha Centauri")
    {
        std::cout << "[SFMLGraphics] Creating window...\n";
        m_window.setFramerateLimit(60);
        m_window.setKeyRepeatEnabled(false);
        m_window.requestFocus();
        std::cout << "[SFMLGraphics] Window created, isOpen=" << m_window.isOpen() << "\n";
        if (!m_font.openFromFile(k_fontPath1))
        {
            if (!m_font.openFromFile(k_fontPath2))
            {
                std::cerr << "[SFMLGraphics] Font loading failed.\n";
            }
        }
    }

    bool Initialize() override
    {
        std::cout << "[SFMLGraphics] Initialize() called, isOpen=" << m_window.isOpen() << "\n";
        if (!m_window.isOpen())
        {
            std::cerr << "[Graphics] Failed to create SFML render window.\n";
            return false;
        }
        return true;
    }

    void Clear() override
    {
        m_window.clear(sf::Color::Black);
    }

    void Display() override
    {
        ProcessEvents_();
        m_window.display();
    }

    bool LoadTexture(const std::string& id, const std::string& path) override
    {
        sf::Texture texture;
        if (!texture.loadFromFile(path))
        {
            std::cerr << "[Graphics] Failed to load texture '" << path << "'.\n";
            return false;
        }
        m_textures.emplace(id, std::move(texture));
        return true;
    }

    bool DrawSprite(const std::string& textureId, float x, float y) override
    {
        auto it = m_textures.find(textureId);
        if (it == m_textures.end())
        {
            std::cerr << "[Graphics] Texture '" << textureId << "' is not loaded.\n";
            return false;
        }

        sf::Sprite sprite(it->second);
        sprite.setPosition({x, y});
        m_window.draw(sprite);
        return true;
    }

    void DrawText(const std::string& text, float x, float y, unsigned int size = 24, const Color& color = Color::White()) override
    {
        if (m_font.getInfo().family.empty())
        {
            return;
        }
        sf::Text drawable(m_font, text, size);
        drawable.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
        drawable.setPosition({x, y});
        m_window.draw(drawable);
    }

    void DrawRect(float x, float y, float width, float height, const Color& color, float thickness) override
    {
        sf::RectangleShape rect(sf::Vector2f(width, height));
        rect.setPosition(sf::Vector2f(x, y));
        rect.setFillColor(sf::Color::Transparent);
        rect.setOutlineColor(sf::Color(color.r, color.g, color.b, color.a));
        rect.setOutlineThickness(thickness);
        m_window.draw(rect);
    }

    void DrawFilledRect(float x, float y, float width, float height, const Color& color) override
    {
        sf::RectangleShape rect(sf::Vector2f(width, height));
        rect.setPosition(sf::Vector2f(x, y));
        rect.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
        m_window.draw(rect);
    }

    unsigned int GetWindowWidth() const override
    {
        return m_window.getSize().x;
    }

    unsigned int GetWindowHeight() const override
    {
        return m_window.getSize().y;
    }

private:
    void ProcessEvents_()
    {
        while (auto event = m_window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                // Ignore the close button: only Enter should close the window.
                continue;
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                m_window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)})));
            }

            if (auto KeyEvent_t = event->getIf<sf::Event::KeyPressed>())
            {
                if (auto mapped = KeyFromSfKey(KeyEvent_t->code))
                {
                    PushPendingKeyEvent_t(*mapped);
                }
            }

            if (auto mouseEvent = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (auto mappedKey = MouseButtonFromSfButton(mouseEvent->button))
                {
                    auto modifier = GetModifierState();
                    PushPendingMouseEvent_t({*mappedKey, static_cast<int>(mouseEvent->position.x), static_cast<int>(mouseEvent->position.y), modifier, true});
                }
            }

            if (auto mouseEvent = event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (auto mappedKey = MouseButtonFromSfButton(mouseEvent->button))
                {
                    auto modifier = GetModifierState();
                    PushPendingMouseEvent_t({*mappedKey, static_cast<int>(mouseEvent->position.x), static_cast<int>(mouseEvent->position.y), modifier, false});
                }
            }

            if (auto mouseEvent = event->getIf<sf::Event::MouseMoved>())
            {
                PushPendingMouseEvent_t({MouseButton_t::None, static_cast<int>(mouseEvent->position.x), static_cast<int>(mouseEvent->position.y), {}, false});
            }
        }
    }

    sf::RenderWindow m_window;
    sf::Font m_font;
    std::unordered_map<std::string, sf::Texture> m_textures;
};


std::unique_ptr<Graphics> CreateGraphics()
{
    return std::make_unique<SFMLGraphics>();
}

} // namespace ac

#endif // USE_SFML
