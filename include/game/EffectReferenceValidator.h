#pragma once

namespace ac
{

struct GameDataContext;

// Every effects list LoadGameData loaded, continuous and triggered, so a typo'd id fails at
// startup. Throws on any null registry.
void ValidateEffectReferences(const GameDataContext& rData);

} // namespace ac
