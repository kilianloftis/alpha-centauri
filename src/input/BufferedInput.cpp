#include "input/Input.h"
#include "input/PlatformEventQueue.h"

#include <memory>

namespace ac
{

namespace
{

// Reads whatever the windowing backend queued, so one implementation serves every backend.
class BufferedInput : public Input
{
public:
    explicit BufferedInput(PlatformEventQueue& rEvents)
        : m_rEvents(rEvents)
    {
    }

    std::optional<KeyEvent_t> PollKey() override
    {
        return m_rEvents.PopKey();
    }

    std::optional<MouseEvent_t> PollMouse() override
    {
        return m_rEvents.PopMouse();
    }

    std::optional<MousePosition_t> GetLastMousePosition() const override
    {
        return m_rEvents.GetLastMousePosition();
    }

private:
    PlatformEventQueue& m_rEvents;
};

} // namespace

std::unique_ptr<Input> CreateInput(PlatformEventQueue& rEvents)
{
    return std::make_unique<BufferedInput>(rEvents);
}

} // namespace ac
