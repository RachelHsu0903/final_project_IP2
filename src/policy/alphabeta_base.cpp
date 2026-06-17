#include <algorithm>

#include "alphabeta_base.hpp"
#include "state.hpp"

static const int BASE_VALUES[7] = {0, 2, 6, 7, 8, 20, 100};

static int base_eval(State* state){
    int self = state->player;
    int opp = 1 - self;
    int self_score = 0;
    int opp_score = 0;

    for(int r = 0; r < BOARD_H; r++){
        for(int c = 0; c < BOARD_W; c++){
            int sp = state->piece_at(self, r, c);
            int op = state->piece_at(opp, r, c);
            if(sp){
                self_score += BASE_VALUES[sp];
            }
            if(op){
                opp_score += BASE_VALUES[op];
            }
        }
    }

    return self_score - opp_score;
}

static int move_score(State* state, const Move& action){
    Point from = action.first;
    Point to = action.second;
    int self = state->player;
    int opp = 1 - self;

    int my_piece = state->piece_at(self, from.first, from.second);
    int captured = state->piece_at(opp, to.first, to.second);

    int score = 0;
    if(captured > 0){
        score += 1000 + PIECE_VALUES[captured] * 10 - PIECE_VALUES[my_piece];
    }
    if(captured == 6){
        score += 100000;
    }
    if(my_piece == 1){
        if((self == 0 && to.first == 0) || (self == 1 && to.first == BOARD_H - 1)){
            score += 800;
        }
    }
    return score;
}

int AlphaBetaBase::eval_ctx(
    State* state,
    int depth,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    int alpha,
    int beta
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    if(state->game_state == WIN){
        return P_MAX - ply;
    }
    if(state->game_state == DRAW){
        return 0;
    }

    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }

    if(depth <= 0){
        return base_eval(state);
    }

    history.push(state->hash());

    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [state](const Move& a, const Move& b){
            return move_score(state, a) > move_score(state, b);
        }
    );

    int best_score = -P_MAX;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        int raw = eval_ctx(next, depth - 1, history, ply + 1, ctx, -beta, -alpha);
        int score = same ? raw : -raw;

        delete next;

        if(score > best_score){
            best_score = score;
        }
        if(score > alpha){
            alpha = score;
        }
        if(alpha >= beta){
            break;
        }
    }

    history.pop(state->hash());
    return best_score;
}

SearchResult AlphaBetaBase::search(
    State* state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    SearchResult result;
    result.depth = depth;

    if(state->legal_actions.empty()){
        state->get_legal_actions();
    }

    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [state](const Move& a, const Move& b){
            return move_score(state, a) > move_score(state, b);
        }
    );

    int best_score = -P_MAX;
    int move_index = 0;
    int total_moves = static_cast<int>(state->legal_actions.size());

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        int raw = eval_ctx(next, depth - 1, history, 1, ctx, -P_MAX, P_MAX);
        int score = same ? raw : -raw;

        delete next;

        if(score > best_score){
            best_score = score;
            result.best_move = action;
            if(ctx.on_root_update){
                ctx.on_root_update({
                    result.best_move,
                    best_score,
                    depth,
                    move_index + 1,
                    total_moves
                });
            }
        }

        move_index++;
    }

    result.score = best_score;
    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    result.pv = {result.best_move};
    return result;
}

ParamMap AlphaBetaBase::default_params(){
    return {};
}

std::vector<ParamDef> AlphaBetaBase::param_defs(){
    return {};
}
