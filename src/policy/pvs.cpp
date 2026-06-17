#include <utility>
#include <iostream>
#include <algorithm>
#include "state.hpp"
#include "pvs.hpp"

/*============================================================
 * Move Ordering
 * 先搜尋比較可能好的 move，讓 Alpha-Beta / PVS 更容易剪枝
 *============================================================*/
static int move_score(State* state, const Move& action){
    Point from = action.first;
    Point to = action.second;

    int self = state->player;
    int opp = 1 - self;

    int my_piece = state->piece_at(self, from.first, from.second);
    int captured = state->piece_at(opp, to.first, to.second);

    int score = 0;

    // Move Ordering：優先吃子
    if(captured > 0){
        score += 1000 + PIECE_VALUES[captured] * 10 - PIECE_VALUES[my_piece];
    }

    // Move Ordering：優先吃王
    if(captured == 6){
        score += 100000;
    }

    // Move Ordering：兵升變優先
    if(my_piece == 1){
        if((self == 0 && to.first == 0) || (self == 1 && to.first == BOARD_H - 1)){
            score += 800;
        }
    }

    return score;
}
static bool is_noisy_move(State* state, const Move& action){
    Point from = action.first;
    Point to = action.second;

    int self = state->player;
    int opp = 1 - self;

    int my_piece = state->piece_at(self, from.first, from.second);
    int captured = state->piece_at(opp, to.first, to.second);

    // 吃子算 noisy
    if(captured > 0){
        return true;
    }

    // 兵升變算 noisy
    if(my_piece == 1){
        if((self == 0 && to.first == 0) || (self == 1 && to.first == BOARD_H - 1)){
            return true;
        }
    }

    return false;
}
/*============================================================
 * QUIESCENCE SEARCH
 *============================================================*/

static int quiescence(
    State* state,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const PVSParams& p,
    int alpha,
    int beta
){
    ctx.nodes++;

    if(ctx.stop){
        return 0;
    }

    /*============================================================
     * QUIESCENCE DEPTH LIMIT
     *
     * 避免吃子鏈太長導致搜尋爆炸
     *============================================================*/
    if(ply >= 8){
        return state->evaluate(
            p.use_kp_eval,
            p.use_eval_mobility,
            &history
        );
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

    /*============================================================
     * REPETITION PENALTY
     *
     * 避免 AI 一直選擇重複局面，導致來回走或卡住
     *
     * 注意：這裡檢查的是當前 state 是否在 history 裡出現過。
     * quiescence 的呼叫者（eval_ctx depth<=0）在呼叫前
     * 已經 pop 了自己，所以這裡的 history 不含當前 state，
     * 不會造成誤判。
     *============================================================*/
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }

    int stand_pat = state->evaluate(
        p.use_kp_eval,
        p.use_eval_mobility,
        &history
    );

    if(stand_pat >= beta){
        return beta;
    }

    if(stand_pat > alpha){
        alpha = stand_pat;
    }

    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [state](const Move& a, const Move& b){
            return move_score(state, a) > move_score(state, b);
        }
    );

    for(auto& action : state->legal_actions){
        if(!is_noisy_move(state, action)){
            continue;
        }

        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        history.push(next->hash());

        int raw = quiescence(
            next,
            history,
            ply + 1,
            ctx,
            p,
            -beta,
            -alpha
        );

        history.pop(next->hash());

        int score = same ? raw : -raw;

        delete next;

        if(score >= beta){
            return beta;
        }

        if(score > alpha){
            alpha = score;
        }
    }

    return alpha;
}
/*============================================================
 * PVS — eval_ctx
 *
 * PVS = Principal Variation Search
 * 可以理解成 Alpha-Beta 的進化版
 *============================================================*/
int PVS::eval_ctx(
    State *state,
    int depth,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const PVSParams& p,
    int alpha,   // Alpha-Beta：目前已知的最好下界
    int beta     // Alpha-Beta：目前允許的上界
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

    // Terminal：可以吃王，越早贏越好
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

    /*============================================================
     * QUIESCENCE SEARCH
     *
     * depth 到底後，
     * 不直接 evaluate，
     * 而是繼續搜尋吃子/升變局面。
     *
     * 修正：在進入 quiescence 之前先 pop 當前 state，
     * 避免 quiescence 裡的 check_repetition 誤判。
     * quiescence 內部只管理 child state 的 push/pop，
     * 不管理當前 state。
     *============================================================*/
    if(depth <= 0){

        int score;

        if(p.use_quiescence){

            score = quiescence(
                state,
                history,    // history 此時不含當前 state，不會誤判重複
                ply,
                ctx,
                p,
                alpha,
                beta
            );

        }else{

            score = state->evaluate(
                p.use_kp_eval,
                p.use_eval_mobility,
                &history
            );
        }

        return score;
    }

    // depth > 0：正常搜尋，push 當前 state 到 history
    history.push(state->hash());

    // Move Ordering：先搜吃子、吃王、升變
    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [state](const Move& a, const Move& b){
            return move_score(state, a) > move_score(state, b);
        }
    );

    int best_score = -P_MAX;

    // PVS：第一個 child 用完整 Alpha-Beta 搜
    // 後面的 child 先用 null-window search
    bool first_child = true;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        int raw;

        if(first_child){
            // ================================
            // PVS Part 1：
            // 第一個分支用完整 Alpha-Beta Search
            // ================================
            raw = eval_ctx(
                next,
                depth - 1,
                history,
                ply + 1,
                ctx,
                p,
                -beta,     // Alpha-Beta：Negamax window 轉號
                -alpha     // Alpha-Beta：Negamax window 轉號
            );

            first_child = false;
        }else{
            // ================================
            // PVS Part 2：
            // 後面的分支先用 Null Window Search
            //
            // 重點是這兩個參數：
            //     -alpha - 1, -alpha
            //
            // 這代表只快速檢查：
            // 這個 move 有沒有可能比目前 alpha 更好
            // ================================
            raw = eval_ctx(
                next,
                depth - 1,
                history,
                ply + 1,
                ctx,
                p,
                -alpha - 1,   // PVS：Null Window Search
                -alpha        // PVS：Null Window Search
            );

            int temp_score = same ? raw : -raw;

            // ================================
            // PVS Part 3：
            // 如果 null-window 發現這步可能比 alpha 好，
            // 才重新用完整 Alpha-Beta window 搜一次
            // ================================
            if(temp_score > alpha && temp_score < beta){
                raw = eval_ctx(
                    next,
                    depth - 1,
                    history,
                    ply + 1,
                    ctx,
                    p,
                    -beta,     // PVS Re-search：完整 Alpha-Beta window
                    -alpha     // PVS Re-search：完整 Alpha-Beta window
                );
            }
        }

        int score = same ? raw : -raw;

        delete next;

        if(score > best_score){
            best_score = score;
        }

        // ================================
        // Alpha-Beta Part 1：
        // 更新 alpha
        // ================================
        if(score > alpha){
            alpha = score;
        }

        // ================================
        // Alpha-Beta Part 2：
        // alpha >= beta 時剪枝
        // 這行就是 Alpha-Beta Pruning 的核心
        // ================================
        if(alpha >= beta){
            break;
        }
    }

    history.pop(state->hash());

    return best_score;
}

/*============================================================
 * PVS — search
 * Root 層搜尋
 *============================================================*/
SearchResult PVS::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();

    PVSParams p = PVSParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

    // Root Move Ordering
    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [state](const Move& a, const Move& b){
            return move_score(state, a) > move_score(state, b);
        }
    );

    int best_score = -P_MAX;

    // Alpha-Beta root window
    int alpha = -P_MAX;
    int beta = P_MAX;

    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    // PVS：root 第一個 child 完整搜，後面 null-window
    bool first_child = true;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        int raw;

        if(first_child){
            // PVS：第一個 root move 完整 Alpha-Beta 搜
            raw = eval_ctx(
                next,
                depth - 1,
                history,
                1,
                ctx,
                p,
                -beta,
                -alpha
            );

            first_child = false;
        }else{
            // PVS：其他 root move 先用 Null Window Search
            raw = eval_ctx(
                next,
                depth - 1,
                history,
                1,
                ctx,
                p,
                -alpha - 1,   // PVS Null Window
                -alpha        // PVS Null Window
            );

            int temp_score = same ? raw : -raw;

            // PVS：如果可能更好，重新完整搜尋
            if(temp_score > alpha && temp_score < beta){
                raw = eval_ctx(
                    next,
                    depth - 1,
                    history,
                    1,
                    ctx,
                    p,
                    -beta,
                    -alpha
                );
            }
        }

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

        // Alpha-Beta：root 更新 alpha
        if(score > alpha){
            alpha = score;
        }

        move_index++;
    }

    result.score = best_score;
    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;

    return result;
}

/*============================================================
 * PVS — default_params / param_defs
 *============================================================*/
ParamMap PVS::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},

        // Quiescence Search
        {"UseQuiescence", "true"},

        {"ReportPartial", "true"},
    };
}
std::vector<ParamDef> PVS::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},

        // Quiescence Search
        {"UseQuiescence", ParamDef::CHECK, "true"},

        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
