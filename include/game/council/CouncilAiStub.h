#pragma once

namespace ac
{

class CouncilVoteExecutor;
class PlanetaryCouncil;

// Temporary AI: cast ballots for every non-player council member on the pending proposal.
// Standard → Yea; election → the proposer (else first eligible candidate, else abstain).
// Skips members that have already voted. Replace with real AI later.
void CastStubCouncilVotes(CouncilVoteExecutor& rVotes, const PlanetaryCouncil& rCouncil);

} // namespace ac
