#pragma once

namespace ac
{

class CouncilVoteExecutor;
class PlanetaryCouncil;

// Temporary AI: every non-player member votes Yea, or for the proposer in an election.
// TODO: replace with real AI.
void CastStubCouncilVotes(CouncilVoteExecutor& rVotes, const PlanetaryCouncil& rCouncil);

} // namespace ac
