#pragma once

#include "game_history.hpp"
#include "search_types.hpp"

class AlphaBetaBase {
public:
    static int eval_ctx(
        State* state,
        int depth,
        GameHistory& history,
        int ply,
        SearchContext& ctx,
        int alpha,
        int beta
    );

    static SearchResult search(
        State* state,
        int depth,
        GameHistory& history,
        SearchContext& ctx
    );

    static ParamMap default_params();
    static std::vector<ParamDef> param_defs();
};
