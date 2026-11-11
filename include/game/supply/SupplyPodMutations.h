#pragma once

#include "game/base/BaseTypes.h"

#include <optional>
#include <random>
#include <string>

namespace ac
{

class GameState;
class Unit;

struct SupplyPodOpenResult_t
{
    bool bOpened = false;
    bool bVisitorDestroyed = false;
    std::string eventId;
    std::string noticeKey;
    std::optional<std::string> imagePath;
    std::optional<TileCoord_t> cameraTile;
    std::optional<int> creditAmount;
    std::optional<std::string> techName;
    std::optional<std::string> unitName;
    std::optional<std::string> resourceBonusId;
    std::optional<std::string> baseName;
    std::optional<std::string> itemName;
    std::optional<std::string> leaderTitle;
    std::optional<std::string> leaderName;
    std::optional<std::string> factionName;
};

// Applies PickEvent, removes the pod, runs the chosen event's effects when present, and
// enqueues a player notice when the visitor's faction is player-controlled. DestroyUnit in
// the effects list frees the visitor during apply.
SupplyPodOpenResult_t OpenSupplyPod(GameState& rGameState, Unit& rVisitor, std::mt19937& rRng);

} // namespace ac
