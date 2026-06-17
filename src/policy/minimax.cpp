#include <utility>
#include "state.hpp"
#include "minimax.hpp"


/*============================================================
 * MiniMax — eval_ctx
 *
 * Negamax without pruning. Caller manages memory.
 *============================================================*/
int MiniMax::eval_ctx(
    //遞迴模擬未來棋局
    //回傳目前state的最佳分數
    State *state,
    int depth,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* === Terminal / leaf checks === */

    // [ Hackathon TODO 3-1 ]
    // return the score for a winning terminal state
    // Hint: prefer faster wins by using ply.
    //如果在某一步可以吃掉國王:必勝
    if(state->game_state == WIN){
        return P_MAX - ply;
        //ply 離根節點的距離，可以走比較少步
        //that is 能提早贏就提早贏
        }

    if(state->game_state == DRAW){
        return 0;
    }

    /* === Repetition check (game-specific) === */
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

    /* === Negamax loop === */
    //把所有合法的走法都走一次 看哪一步最好
    int best_score = M_MAX;

    for(auto& action : state->legal_actions){
        // [ Hackathon TODO 3-2 ] 判斷下一個玩家是不是同一個玩家
        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        // [Hackathon TODO 3-3] 從新的棋盤繼續往下搜尋
        // depth 還能搜幾層 ply 目前離根節點多遠 
        int raw = eval_ctx(next, depth - 1, history, ply + 1, ctx, p);

        // [Hackathon TODO 3-4]
        int score = same ? raw : -raw;

        delete next;

        // [ Hackathon TODO 3-5 ]
        if(score > best_score){
            best_score = score;
        }
    }

    history.pop(state->hash());
    return best_score;
}


/*============================================================
 * MiniMax — search
 *
 * Iterate legal moves, call eval_ctx, return SearchResult.
 *============================================================*/
SearchResult MiniMax::search(
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


    int best_score = M_MAX - 10;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    for(auto& action : state->legal_actions){
        /* [ Hackathon TODO 4-1 ] */
        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        int raw = eval_ctx(next, depth - 1, history, 1, ctx, p);

        int score = same ? raw : -raw;

        delete next;

        if(score > best_score){
            // [ Hackathon TODO 4-2 ]
            best_score = score;
            result.best_move = action;

            if(p.report_partial && ctx.on_root_update){
            ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
            }
        }  

        move_index++;
    }

    // [ Hackathon TODO 4-3 ]
    result.score = best_score;
    return result;
} 


/*============================================================
 * MiniMax — default_params / param_defs
 *============================================================*/
ParamMap MiniMax::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> MiniMax::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
