#include <cstdlib>
#include <iostream>

#include "TickTock.hpp"
#include "GameState.hpp"

//TODO: If we implement a 4 planet variant then TickTock should only start with 8 tokens
TickTock::TickTock() : num_tokens(10)
{
	set_name("Tick-Tock");
	set_power("Patience");
	set_role( PlayerRole::AnyPlayer);
	set_mandatory(true);
	valid_phases_insert(TurnPhase::Resolution);
	set_description("You start the game with ten tokens. Each time any player wins an encounter as the defense or a successful deal is made between any two players, *use* this power to discard one token. If you have no more tokens, you immediately win the game. You may still win the game via the normal method.");
}

void TickTock::discard_token()
{
	num_tokens--;
}

unsigned TickTock::get_tokens() const
{
	return num_tokens;
}

bool TickTock::can_respond(EncounterRole e, TurnPhase t, GameEvent g, PlayerColors mycolor) const
{
	if(!check_role_and_phase(e,t))
	{
		return false;
	}

	if(g.event_type == GameEventType::SuccessfulDeal || g.event_type == GameEventType::DefensiveEncounterWin)
	{
		return true;
	}
	else
	{
		return false;
	}
}

std::function<void()> TickTock::get_resolution_callback(GameState *g, const PlayerColors player, GameEvent &this_event, const GameEvent responding_to)
{
	std::function<void()> ret = [this,g] () { this->discard_token(); g->update_tick_tock_tokens(num_tokens); };
	return ret;
}
