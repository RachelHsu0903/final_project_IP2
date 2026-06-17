#include <utility>
#include <iostream>
#include <algorithm>
#include "state.hpp"
#include "alphabeta.hpp"

// 優化：move ordering
static int move_score(State* state, const Move& action){
    Point from = action.first;
    Point to = action.second;

    int self = state->player;
    int opp = 1 - self;

    int my_piece = state->piece_at(self, from.first, from.second);
    int captured = state->piece_at(opp, to.first, to.second);

    int score = 0;

    // 優先吃子：吃越貴的棋越先搜
    if(captured > 0){
        score += 1000 + PIECE_VALUES[captured] * 10 - PIECE_VALUES[my_piece];
    }

    // 優先吃王
    if(captured == 6){
        score += 100000;
    }

    // 兵升變優先
    if(my_piece == 1){
        if((self == 0 && to.first == 0) || (self == 1 && to.first == BOARD_H - 1)){
            score += 800;
        }
    }

    return score;
}

/*============================================================
 * AlphaBeta — eval_ctx
 *
 * Negamax with Alpha-Beta pruning.
 *============================================================*/
int AlphaBeta::eval_ctx(
    State *state,
    int depth,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p,
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

    history.push(state->hash());

    if(depth <= 0){
        int score = state->evaluate(
            p.use_kp_eval, p.use_eval_mobility, &history
        );
        history.pop(state->hash());
        return score;
    }

    int best_score = -P_MAX;

    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [state](const Move& a, const Move& b){
            return move_score(state, a) > move_score(state, b);
        }
    );

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        int raw = eval_ctx(
            next,
            depth - 1,
            history,
            ply + 1,
            ctx,
            p,
            -beta,
            -alpha
        );

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

/*============================================================
 * AlphaBeta — search
 *============================================================*/
SearchResult AlphaBeta::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();

    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

    int best_score = -P_MAX;

    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [state](const Move& a, const Move& b){
            return move_score(state, a) > move_score(state, b);
        }
    );

    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        int raw = eval_ctx(
            next,
            depth - 1,
            history,
            1,
            ctx,
            p,
            -P_MAX,
            P_MAX
        );

        int score = same ? raw : -raw;

        delete next;

        if(score > best_score){
            best_score = score;
            result.best_move = action;

            if(p.report_partial && ctx.on_root_update){
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

    // std::cerr << "[DEBUG] AlphaBeta depth = "
    //           << depth
    //           << ", nodes = "
    //           << ctx.nodes
    //           << ", seldepth = "
    //           << ctx.seldepth
    //           << std::endl;

    return result;
}

/*============================================================
 * AlphaBeta — default_params / param_defs
 *============================================================*/
ParamMap AlphaBeta::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> AlphaBeta::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
