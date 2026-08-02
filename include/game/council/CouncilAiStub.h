#pragma once

namespace ac
{

class PlanetaryCouncil;

// Temporary AI: cast ballots for every non-player council member on the pending proposal.
// Standard → Yea; election → the proposer (else first governor candidate, else abstain).
// Skips members that have already voted. Replace with real AI later.
void CastStubCouncilVotes(PlanetaryCouncil& rCouncil);

} // namespace ac
