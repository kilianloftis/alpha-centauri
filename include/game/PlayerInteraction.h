#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/PauseOnEventsConfig.h"
#include "lib/GameEvent.h"

#include <optional>
#include <string>
#include <variant>

namespace ac
{

// Informational notice (OK to dismiss). Optional camera focus on a tile.
// `event` selects the PauseOnEventsConfig_t flag that can skip presentation.
struct NoticeInteraction_t
{
    PauseOnEventId_t event = PauseOnEventId_t::NewFacilityBuilt;
    std::string title;
    std::string body;
    std::optional<TileCoord_t> cameraTile;
};

// Open a full overlay; completing the interaction waits until that view closes.
struct OpenViewInteraction_t
{
    enum class View_t
    {
        Base,
        Research,
        UnitDesigner,
        SocialEngineering,
    };
    View_t view = View_t::Base;
    std::optional<BaseId_t> baseId;
    std::string techId;
};

// Production would empty the base; player Complete (allow emptying) or Disable production.
struct ProductionWouldEmptyInteraction_t
{
    FactionId_t factionId = 0;
    BaseId_t baseId = 0;
};

// Production finished with an empty queue; offer Continue or Zoom to base control.
struct ProductionIdleInteraction_t
{
    FactionId_t factionId = 0;
    BaseId_t baseId = 0;
    bool afterCompletion = false;
    std::string completedItemId;
    std::string completedItemName;
    std::optional<PauseOnEventId_t> completedEvent;
};

using PlayerInteraction_t = std::variant<
    NoticeInteraction_t,
    OpenViewInteraction_t,
    ProductionWouldEmptyInteraction_t,
    ProductionIdleInteraction_t
>;

// Every queued item pauses turn processing for its audience until CompleteFront.
struct QueuedInteraction_t
{
    PlayerInteraction_t payload;
    // Faction that should see this (normally the player). AI news still uses the player id.
    FactionId_t audience = 0;
};

} // namespace ac
