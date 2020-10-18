#include <iostream>
#include <map>
#include <vector>
#include <cassert>
#include <algorithm>
#include <sstream>

#include "GameState.hpp"

GameState::GameState(unsigned nplayers) : num_players(nplayers), players(nplayers), destiny_deck(DestinyDeck(nplayers)), invalidate_next_callback(false)
{
	assert(nplayers > 1 && nplayers < 6 && "Invalid number of players!");
	std::cout << "Starting Game with " << num_players << " players\n";

	//For now assign colors in a specific order...TODO: let the user choose colors
	players[0].make_default_player(PlayerColors::Red);
	players[1].make_default_player(PlayerColors::Blue);
	if(num_players > 2)
	{
		players[2].make_default_player(PlayerColors::Purple);
	}
	if(num_players > 3)
	{
		players[3].make_default_player(PlayerColors::Yellow);
	}
	if(num_players > 4)
	{
		players[4].make_default_player(PlayerColors::Green);
	}

	for(unsigned i=0; i<players.size(); i++)
	{
		players[i].set_game_state(this);
	}
}

void GameState::dump() const
{
	std::cout << "Current scores:\n";
	for(auto i=players.begin(),e=players.end();i!=e;++i)
	{
		std::cout << to_string(i->color) << " Player score: " << i->score << "\n";
		std::cout << "Planets: {";
		for(auto ii=i->planets.begin(),ee=i->planets.end();ii!=ee;++ii)
		{
			if(ii != i->planets.begin())
				std::cout << ",";
			std::cout << "{";
			for(auto iii=ii->begin(),eee=ii->end();iii!=eee;++iii)
			{
				if(iii != ii->begin())
					std::cout << ",";
				std::cout << to_string(*iii);
			}
			std::cout << "}";
		}
		std::cout << "}\n";
	}
	std::cout << "\n";
}

void GameState::dump_destiny_deck() const
{
	destiny_deck.dump();
}

void GameState::shuffle_destiny_deck()
{
	destiny_deck.shuffle();
}

void GameState::assign_alien(const PlayerColors color, std::unique_ptr<AlienBase> &alien)
{
	for(auto& player : players)
	{
		if(player.color == color)
		{
			player.alien = std::move(alien);
			break;
		}
	}
}

void GameState::dump_cosmic_deck() const
{
	cosmic_deck.dump();
}

void GameState::shuffle_cosmic_deck()
{
	cosmic_deck.shuffle();
}

void GameState::deal_starting_hands()
{
	const unsigned starting_hand_size = 8;
	unsigned total_cards_dealt = players.size()*starting_hand_size;
	unsigned current_cards_dealt = 0;

	auto iter = cosmic_deck.begin();
	while(total_cards_dealt > current_cards_dealt)
	{
		unsigned player_to_be_dealt_this_card = current_cards_dealt % players.size();
		players[player_to_be_dealt_this_card].hand.push_back(*iter); //Place the card in the player's hand
		iter = cosmic_deck.erase(iter); //Remove the card from the deck

		current_cards_dealt++;
	}

}

void GameState::shuffle_discard_into_cosmic_deck()
{
	assert(cosmic_deck.empty() && "Expected empty CosmicDeck when shuffling the discard pile back into the CosmicDeck");
	for(auto i=cosmic_discard.begin(),e=cosmic_discard.end();i!=e;++i)
	{
		cosmic_deck.push_back(*i);
	}
	cosmic_discard.clear();
	cosmic_deck.shuffle();
}

void GameState::draw_cosmic_card(PlayerInfo &player)
{
	//If the deck is empty, shuffle the discard deck into the deck
	if(cosmic_deck.empty())
	{
		shuffle_discard_into_cosmic_deck();
	}

	auto iter = cosmic_deck.begin();
	player.hand.push_back(*iter);
	cosmic_deck.erase(iter);
}

void GameState::dump_player_hands() const
{
	for(auto i=players.begin(),e=players.end();i!=e;++i)
	{
		i->dump_hand();
	}
}

void GameState::dump_player_hand(const PlayerInfo &p) const
{
	p.dump_hand();
}

PlayerColors GameState::choose_first_player()
{
	return destiny_deck.draw_for_first_player_and_shuffle();
}

PlayerInfo& GameState::get_player(const PlayerColors &c)
{
	for(auto i=players.begin(),e=players.end();i!=e;++i)
	{
		if(i->color == c)
		{
			return *i;
		}
	}

	assert(0 && "Unable to find player!");
}

void GameState::discard_and_draw_new_hand(PlayerInfo &player)
{
	//Copy cards from the player's hand to discard
	for(auto i=player.hand.begin(),e=player.hand.end();i!=e;++i)
	{
		cosmic_discard.push_back(*i);
	}
	player.hand.clear();

	if(cosmic_deck.size() < 8)
	{
		//Move remaining cards in the deck to the player, then move discard to the deck, then shuffle and finish dealing the player's new hand
		for(auto i=cosmic_deck.begin(),e=cosmic_deck.end();i!=e;++i)
		{
			player.hand.push_back(*i);
		}
		cosmic_deck.clear();
		shuffle_discard_into_cosmic_deck();
	}

	while(player.hand.size() < 8)
	{
		player.hand.push_back(*cosmic_deck.begin());
		cosmic_deck.erase(cosmic_deck.begin());
	}

	//If the new hand isn't valid, try again
	if(!player.has_encounter_cards_in_hand())
		discard_and_draw_new_hand(player);
}

void GameState::free_all_ships_from_warp()
{
	for(auto i=warp.begin(),e=warp.end();i!=e;++i)
	{
		move_ship_from_warp_to_colony(get_player(*i));
	}
}

//TODO: Some events occur before the 'standard' turn phase completes whereas others occur after...do we want to call this function twice for each turn phase with a before and after flag that gets passed around?
//Artifact cards all seem to happen after the standard turn phase, so we'll follow that policy for now. Aliens can be handled separately for now but that's probably not a great long term plan
void GameState::check_for_game_events(PlayerInfo &offense)
{
	std::vector<CosmicCardType> valid_plays;

	unsigned player_index = 6; //Sentinel value meant to be invalid
	for(unsigned i=0; i<players.size(); i++)
	{
		if(players[i].color == offense.color)
		{
			player_index = i;
			break;
		}
	}
	const unsigned int initial_player_index = player_index;

	do
	{
		PlayerInfo &current_player = players[player_index];
		//Starting with the offense, check for valid plays (artifact cards or alien powers) based on the current TurnPhase
		valid_plays.clear();
		for(auto i=current_player.hand.begin(),e=current_player.hand.end(); i!=e; ++i)
		{
			if(can_play_card_with_empty_stack(state,*i,current_player.current_role))
			{
				valid_plays.push_back(*i);
			}
		}
		//TODO: For now we'll support Aliens outside of this context, but I'm honestly not sure if that's the right decision

		//List the valid plays and ask the player if they would like to do any. Note that if they choose one they can still play another
		while(valid_plays.size())
		{
			std::stringstream prompt;
			prompt << "The " << to_string(current_player.color) << " player has the following valid plays to choose from:\n";
			for(unsigned i=0; i<valid_plays.size(); i++)
			{
				prompt << "Option " << i << ": " << to_string(valid_plays[i]) << "\n";
			}
			prompt << "Option " << valid_plays.size() << ": None\n";
			unsigned chosen_option;
			do
			{
				std::cout << prompt.str();
				std::cout << "Please choose one of the option numbers above.\n";
				std::cout << to_string(current_player.color) << ">>";
				std::cin >> chosen_option;
			} while(chosen_option > valid_plays.size()); //TODO: What's the proper way to protect bad input here? We don't want to crash, we just want to retry the prompt. Take in a string instead and filter it

			if(chosen_option != valid_plays.size()) //An action was taken
			{
				CosmicCardType play = valid_plays[chosen_option];

				//Remove this card from the player's hand and add it to discard
				add_to_discard_pile(play);
				unsigned old_hand_size = current_player.hand.size(); //Sanity checking
				for(auto i=current_player.hand.begin(),e=current_player.hand.end(); i!=e; ++i)
				{
					if(*i == play)
					{
						current_player.hand.erase(i);
						break;
					}
				}
				assert(current_player.hand.size()+1 == old_hand_size && "Error removing played card from the player's hand!");

				GameEvent g(current_player.color,to_game_event_type(play));
				get_callbacks_for_cosmic_card(play,g);
				resolve_game_event(g);

				//Remove this option from valid_plays and if there are still plays that could be made, prompt them again
				for(auto i=valid_plays.begin(),e=valid_plays.end();i!=e;++i)
				{
					if(*i == play)
					{
						valid_plays.erase(i);
					}
				}
			}
			else
			{
				break;
			}
		}

		player_index = (player_index+1) % players.size();
	} while(player_index != initial_player_index);
}

void GameState::get_callbacks_for_cosmic_card(const CosmicCardType play, GameEvent &g)
{
	if(play != CosmicCardType::MobiusTubes)
	{
		assert(0 && "CosmicCardType callbacks not yet implemenmted\n");
	}

	g.callback_if_resolved = [this] () { this->free_all_ships_from_warp(); };
}

void GameState::execute_turn(PlayerColors off)
{
	//Start Turn Phase
	state = TurnPhase::StartTurn;
	for(unsigned i=0; i<players.size(); i++)
	{
		players[i].current_role = EncounterRole::None;
	}

	//Ensure the offense has a valid hand
	PlayerInfo &offense = get_player(off);
	offense.current_role = EncounterRole::Offense;
	bool offense_needs_discard = !offense.has_encounter_cards_in_hand();

	std::cout << "The " << to_string(off) << " Player is now the offense.\n";
	if(offense_needs_discard)
	{
		std::cout << "The offense has no encounter cards in hand. They now must discard their hand and draw eight cards\n";
		discard_and_draw_new_hand(offense);

		//This implementations treats the dump and discard operation as one draw action, even if the player is forced to draw and discard multiple times
		//Hence Remora will only draw once for this action, which seems appropriate
		dump_player_hand(offense);

		GameEvent g(offense.color,GameEventType::DrawCard);
		resolve_game_event(g);
	}

	check_for_game_events(offense);

	//Regroup Phase
	state = TurnPhase::Regroup;

	//If the offense has any ships in the warp, they retrieve one of them
	for(auto i=warp.begin(),e=warp.end();i!=e;++i)
	{
		if(*i == offense.color)
		{
			std::cout << "The " << to_string(off) << " player will now regroup\n";
			move_ship_from_warp_to_colony(offense);
			break;
		}
	}

	//TODO: Potentially add events if people want to play Mobius Tubes or Plague
	check_for_game_events(offense);
}

//TODO: Test
//NOTE: This function assumes that at least one of p.color ships is in the warp!
void GameState::move_ship_from_warp_to_colony(PlayerInfo &p)
{
	//Check that at least one ship of the specified color resides in the warp; if not, return
	bool ship_exists_in_warp = false;
	for(auto i=warp.begin(),e=warp.end();i!=e;++i)
	{
		if(*i == p.color)
		{
			ship_exists_in_warp = true;
		}
	}

	if(!ship_exists_in_warp)
		return;

	//Gather the valid options and present them to the player
	std::vector< std::pair<PlayerColors,unsigned> > valid_colonies; //A list of planet colors and indices

	for(auto player_begin=players.begin(),player_end=players.end();player_begin!=player_end;++player_begin)
	{
		for(unsigned planet=0; planet<player_begin->planets.size(); planet++)
		{
			for(unsigned ships=0; ships<player_begin->planets[planet].size(); ships++)
			{
				if(player_begin->planets[planet][ships] == p.color) //If this ship matches our color we have a colony there
				{
					valid_colonies.push_back(std::make_pair(player_begin->color,planet));
					break;
				}
			}
		}
	}

	//If the player has no colonies the ship goes directly onto the hyperspace gate
	//This assumes the player is the offense, which will be true 99% of the time, but technically Remora could have no colonies and retrieve a ship in response to the offense retrieving a ship.
	//It's not even clear from the rulebook what should happen in that case. Perhap's Remora's ability shouldn't resolve
	if(!valid_colonies.size())
	{
		std::cout << "The  " << to_string(p.color) << " player has no valid colonies! Placing the ship directly on the hyperspace gate.\n";
		hyperspace_gate.push_back(p.color);
	}
	else
	{
		std::stringstream prompt;
		prompt << "The " << to_string(p.color) << " player has the following valid colonies to choose from:\n";
		for(unsigned i=0; i<valid_colonies.size(); i++)
		{
			prompt << "Option " << i << ": " << to_string(valid_colonies[i].first) << " Planet " << valid_colonies[i].second << "\n";
		}
		unsigned chosen_option;
		do
		{
			std::cout << prompt.str();
			std::cout << "Please choose one of the option numbers above.\n";
			std::cout << to_string(p.color) << ">>";
			std::cin >> chosen_option;
		} while(chosen_option >= valid_colonies.size()); //TODO: What's the proper way to protect bad input here? We don't want to crash, we just want to retry the prompt

		const std::pair<PlayerColors,unsigned> chosen_colony = valid_colonies[chosen_option];

		//Now actually add the colony
		bool colony_found = false; //paranoia
		for(auto player_begin=players.begin(),player_end=players.end();player_begin!=player_end;++player_begin)
		{
			if(player_begin->color != chosen_colony.first)
				continue;
			for(unsigned planet=0; planet<player_begin->planets.size(); planet++)
			{
				for(unsigned col=0; col<player_begin->planets[planet].size(); col++)
				{
					player_begin->planets[planet].push_back(p.color);
					colony_found = true;
					break;
				}
				if(colony_found)
					break;
			}
			if(colony_found)
				break;
		}

		assert(colony_found && "Failed to find colony to place ship!");
	}

	//Remove the ship from the warp
	for(auto i=warp.begin(),e=warp.end();i!=e;++i)
	{
		if(*i == p.color)
		{
			warp.erase(i);
			break;
		}
	}

	GameEvent g(p.color,GameEventType::RetrieveWarpShip);
	resolve_game_event(g);
}

std::string GameState::prompt_player(PlayerInfo &p, const std::string &prompt) const
{
	std::cout << prompt;
	std::cout << to_string(p.color) << ">>";
	std::string response;
	std::cin >> response;

	return response;
}

void GameState::dump_current_stack() const
{
	std::stack<GameEvent> copy_stack = stack;

	std::cout << "Current game stack:\n";
	while(!copy_stack.empty())
	{
		GameEvent g = copy_stack.top();
		unsigned depth = copy_stack.size()-1;
		std::cout << depth << ": " << to_string(g.player) << " -> " << to_string(g.event_type) << "\n";
		copy_stack.pop();
	}

	assert(copy_stack.empty() && "Error printing stack!");
}

void GameState::resolve_game_event(const GameEvent g)
{
	stack.push(g);
	dump_current_stack();

	//Enforce player order during resolution
	unsigned player_index = 6; //Sentinel value meant to be invalid
	for(unsigned i=0; i<players.size(); i++)
	{
		if(players[i].color == g.player)
		{
			player_index = i;
			break;
		}
	}
	const unsigned int initial_player_index = player_index;

	do
	{
		GameEvent can_respond = players[player_index].can_respond(state,g); //NOTE: This will eventually be a vector when multiple responses are valid
		if(can_respond.event_type != GameEventType::None) //If there is a valid response...
		{
			bool take_action = false;
			GameEvent must_respond = players[player_index].must_respond(state,g);
			if(must_respond.event_type != GameEventType::None)
			{
				std::cout << "The " << to_string(players[player_index].color) << " player *must* respond to the " << to_string(g.event_type) << " action.\n";
				take_action = true;
			}
			else
			{
				std::cout << "The " << to_string(players[player_index].color) << " player can respond to the " << to_string(g.event_type) << " action.\n";
				std::string response;
				do
				{
					std::stringstream response_prompt;
					response_prompt << "Would you like to respond to the " << to_string(g.event_type) << " with your " << to_string(can_respond.event_type) << "? y/n\n";
					response = prompt_player(players[player_index],response_prompt.str());
				} while(response.compare("y") != 0 && response.compare("n") != 0);
				if(response.compare("y") == 0)
				{
					take_action = true;
				}
			}

			if(take_action)
			{
				//FIXME: Check if must respond is valid and if so, take that action
				if(can_respond.callback_if_action_taken)
				{
					can_respond.callback_if_action_taken();
				}
				resolve_game_event(can_respond);
			}
		}

		player_index = (player_index+1) % players.size();
	} while(player_index != initial_player_index);

	if(g.callback_if_resolved)
	{
		if(invalidate_next_callback) //Countered!
		{
			invalidate_next_callback  = false;
		}
		else
		{
			g.callback_if_resolved();
		}
	}
	stack.pop();
}


void GameState::debug_send_ship_to_warp()
{
	PlayerColors victim = PlayerColors::Yellow;
	warp.push_back(victim);

	PlayerInfo &yellow = get_player(victim);
	yellow.planets[0].erase(yellow.planets[0].begin());

	PlayerColors victim2 = PlayerColors::Red;
	warp.push_back(victim2);
	PlayerInfo &red = get_player(victim2);
	red.planets[2].erase(red.planets[2].begin());
}

