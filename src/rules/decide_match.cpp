#include "melee/match.h"
#include "melee/player.h"
#include "melee/rules.h"
#include "melee/scene.h"
#include "rules/values.h"
#include <algorithm>

struct score {
	int id;
	bool violation;
	int stocks;
	int percent;
};

static bool lgl_violated(int slot)
{
	const auto lgl = get_ledge_grab_limit();
	if (lgl == 0)
		return 0;
	
	const auto *stats = PlayerBlock_GetStats(slot);
	const auto ledge_grabs = PlayerBlockStats_GetActionStat(stats, ActionStat_LedgeGrabs);
	return ledge_grabs > lgl;
}

static bool is_score_better(const score &a, const score &b)
{
	if (!a.violation && b.violation)
		return true;

	if (a.stocks > b.stocks)
		return true;
	
	if (a.percent < b.percent)
		return true;
		
	return false;
}

static bool is_score_equal(const score &a, const score &b)
{
	if (a.violation != b.violation)
		return false;

	if (a.stocks != b.stocks)
		return false;
	
	if (a.percent != b.percent)
		return false;
		
	return true;
}

static score add_score(const score &a, const score &b)
{
	return {
		.id = a.id,
		.violation = a.violation || b.violation,
		.stocks = a.stocks + b.stocks,
		.percent = a.percent + b.percent
	};
}

// Returns count
static int get_player_scores(MatchController *match, score *scores, bool *violation)
{
	auto count = 0;
	*violation = false;

	for (auto i = 0; i < 6; i++) {
		if (match->players[i].slot_type == SlotType_None)
			continue;

		scores[count] = {
			.id = i,
			.violation = lgl_violated(i),
			.stocks = match->players[i].score,
			.percent = match->players[i].percent
		};
		
		if (scores[count].violation)
			*violation = true;
		
		count++;
	}
	
	return count;
}

// Returns count
static int get_team_scores(MatchController *match, score *team_scores, bool *violation)
{
	score player_scores[6];
	const auto players = get_player_scores(match, player_scores, violation);

	auto count = 0;
	bool team_exists[5] = { false };

	for (auto i = 0; i < players; i++) {
		const auto team = match->players[player_scores[i].id].team;
		
		if (!team_exists[team]) {
			team_scores[count] = { .id = team };
			team_exists[team] = true;
			count++;
		}
			
		team_scores[team] = add_score(team_scores[team], player_scores[i]);
	}
	
	return count;
}

static void set_result(MatchController *match, bool teams, int slot, bool win)
{
	if (teams) {
		match->teams[slot].is_big_loser = !win;
		if (win) {
			match->team_winners[match->team_winner_count] = (u8)slot;
			match->team_winner_count++;
		}
	} else {
		match->players[slot].is_big_loser = !win;
		if (win) {
			match->winners[match->winner_count] = (u8)slot;
			match->winner_count++;
		}
	}
}

static bool decide_winners(MatchController *match, bool teams)
{
	// Only apply for timeouts
	if (match->result != MatchResult_Timeout)
		return false;
		
	// Don't check singles win conditions in teams and vice versa
	if(match->is_teams != teams)
		return false;

	score scores[6];
	bool violation;
	const auto count = teams ? get_team_scores(match, scores, &violation)
	                         : get_player_scores(match, scores, &violation);

	// We only care about the highest score
	std::partial_sort(scores, scores + 1, scores + count, is_score_better);
	
	// Everyone tied for first wins
	if (teams)
		match->team_winner_count = 0;
	else
		match->winner_count = 0;
	
	set_result(match, teams, scores[0].id, true);
	
	for (auto i = 1; i < count; i++) {
		const auto win = is_score_equal(scores[i], scores[0]);
		set_result(match, teams, scores[i].id, win);
	}
		
	// Custom result type
	if (violation)
		match->result = MatchResult_RuleViolation;
	
	return true;
}

extern "C" void orig_MatchController_UpdateTeamWinners(MatchController *match);
extern "C" void hook_MatchController_UpdateTeamWinners(MatchController *match)
{
	if (!decide_winners(match, true))
		orig_MatchController_UpdateTeamWinners(match);
}

extern "C" void orig_MatchController_UpdateWinners(MatchController *match);
extern "C" void hook_MatchController_UpdateWinners(MatchController *match)
{
	if (!decide_winners(match, false))
		orig_MatchController_UpdateWinners(match);
}
