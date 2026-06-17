#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <utility>

#include "state.hpp"
#include "submission.hpp"

enum TTFlag {
    TT_EXACT,
    TT_LOWER,
    TT_UPPER
};

struct TTEntry {
    int depth;
    int score;
    TTFlag flag;
    Move best_move;
    bool has_best_move;
};

static std::unordered_map<uint64_t, TTEntry> tt;
static constexpr size_t TT_MAX_ENTRIES = 200000;

static bool same_move(const Move& a, const Move& b){
    return a.first == b.first && a.second == b.second;
}

static bool clear_line(const Board& board, int ar, int ac, int tr, int tc){
    int dr = (tr > ar) - (tr < ar);
    int dc = (tc > ac) - (tc < ac);
    int r = ar + dr;
    int c = ac + dc;
    while(r != tr || c != tc){
        if(board.board[0][r][c] || board.board[1][r][c]){
            return false;
        }
        r += dr;
        c += dc;
    }
    return true;
}

static bool attacks_square(const Board& board, int attacker, int tr, int tc){
    if(tr < 0 || tc < 0){
        return false;
    }

    for(int r = 0; r < BOARD_H; r++){
        for(int c = 0; c < BOARD_W; c++){
            int piece = board.board[attacker][r][c];
            if(!piece){
                continue;
            }

            int dr = tr - r;
            int dc = tc - c;
            int adr = std::abs(dr);
            int adc = std::abs(dc);

            switch(piece){
                case 1: {
                    int forward = attacker == 0 ? -1 : 1;
                    if(dr == forward && adc == 1){
                        return true;
                    }
                    break;
                }
                case 2:
                    if((dr == 0 || dc == 0) && clear_line(board, r, c, tr, tc)){
                        return true;
                    }
                    break;
                case 3:
                    if((adr == 2 && adc == 1) || (adr == 1 && adc == 2)){
                        return true;
                    }
                    break;
                case 4:
                    if(adr == adc && clear_line(board, r, c, tr, tc)){
                        return true;
                    }
                    break;
                case 5:
                    if((dr == 0 || dc == 0 || adr == adc) &&
                       clear_line(board, r, c, tr, tc)){
                        return true;
                    }
                    break;
                case 6:
                    if(std::max(adr, adc) == 1){
                        return true;
                    }
                    break;
            }
        }
    }
    return false;
}

static bool king_move_into_attack(State* state, const Move& action){
    int self = state->player;
    int opp = 1 - self;
    Point from = action.first;
    Point to = action.second;

    if(state->piece_at(self, from.first, from.second) != 6){
        return false;
    }

    Board next = state->board;
    next.board[self][from.first][from.second] = 0;
    next.board[opp][to.first][to.second] = 0;
    next.board[self][to.first][to.second] = 6;

    return attacks_square(next, opp, to.first, to.second);
}

static Board board_after_move(State* state, const Move& action){
    int self = state->player;
    int opp = 1 - self;
    Point from = action.first;
    Point to = action.second;

    Board next = state->board;
    int moved = next.board[self][from.first][from.second];
    if(moved == 1 && (to.first == 0 || to.first == BOARD_H - 1)){
        moved = 5;
    }

    next.board[self][from.first][from.second] = 0;
    next.board[opp][to.first][to.second] = 0;
    next.board[self][to.first][to.second] = moved;
    return next;
}

static bool find_king(const Board& board, int player, int& kr, int& kc){
    for(int r = 0; r < BOARD_H; r++){
        for(int c = 0; c < BOARD_W; c++){
            if(board.board[player][r][c] == 6){
                kr = r;
                kc = c;
                return true;
            }
        }
    }
    return false;
}

static bool own_king_attacked_on_board(const Board& board, int self){
    int opp = 1 - self;
    int kr = -1;
    int kc = -1;
    if(!find_king(board, self, kr, kc)){
        return false;
    }
    return attacks_square(board, opp, kr, kc);
}

static bool threatens_enemy_king_on_board(const Board& board, int self){
    int opp = 1 - self;
    int kr = -1;
    int kc = -1;
    if(!find_king(board, opp, kr, kc)){
        return true;
    }
    return attacks_square(board, self, kr, kc);
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
    if(my_piece == 6 && king_move_into_attack(state, action)){
        score -= 200000;
    }
    Board next = board_after_move(state, action);

    if(own_king_attacked_on_board(next, self)){
        score -= 150000;
    }

    if(threatens_enemy_king_on_board(next, self)){
        score += 6000;
    }

    return score;
}

static void sort_moves(State* state){
    Move tt_move;
    bool has_tt_move = false;
    auto it = tt.find(state->hash());
    if(it != tt.end() && it->second.has_best_move){
        tt_move = it->second.best_move;
        has_tt_move = true;
    }

    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [state, has_tt_move, tt_move](const Move& a, const Move& b){
            if(has_tt_move){
                bool a_tt = same_move(a, tt_move);
                bool b_tt = same_move(b, tt_move);
                if(a_tt != b_tt){
                    return a_tt;
                }
            }
            return move_score(state, a) > move_score(state, b);
        }
    );
}

static bool is_noisy_move(State* state, const Move& action){
    Point from = action.first;
    Point to = action.second;

    int self = state->player;
    int opp = 1 - self;

    int my_piece = state->piece_at(self, from.first, from.second);
    int captured = state->piece_at(opp, to.first, to.second);

    if(captured > 0){
        return true;
    }
    if(my_piece == 1){
        if((self == 0 && to.first == 0) || (self == 1 && to.first == BOARD_H - 1)){
            return true;
        }
    }
    return false;
}

static int quiescence(
    State* state,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const SubmissionParams& p,
    int alpha,
    int beta
){
    ctx.nodes++;

    if(ctx.stop){
        return 0;
    }

    if(ply >= 8){
        return state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
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

    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    if(stand_pat >= beta){
        return beta;
    }
    if(stand_pat > alpha){
        alpha = stand_pat;
    }

    sort_moves(state);

    for(auto& action : state->legal_actions){
        if(!is_noisy_move(state, action)){
            continue;
        }

        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        history.push(next->hash());
        int raw = quiescence(next, history, ply + 1, ctx, p, -beta, -alpha);
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

int Submission::eval_ctx(
    State* state,
    int depth,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const SubmissionParams& p,
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

    uint64_t key = state->hash();
    int alpha_orig = alpha;
    auto tt_it = tt.find(key);
    if(tt_it != tt.end() && tt_it->second.depth >= depth){
        const TTEntry& entry = tt_it->second;
        if(entry.flag == TT_EXACT){
            return entry.score;
        }
        if(entry.flag == TT_LOWER && entry.score > alpha){
            alpha = entry.score;
        }else if(entry.flag == TT_UPPER && entry.score < beta){
            beta = entry.score;
        }
        if(alpha >= beta){
            return entry.score;
        }
    }

    if(depth <= 0){
        if(p.use_quiescence){
            return quiescence(state, history, ply, ctx, p, alpha, beta);
        }
        return state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    }

    history.push(state->hash());

    sort_moves(state);

    int best_score = -P_MAX;
    Move best_move;
    bool has_best_move = false;
    bool first_child = true;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int raw;

        if(first_child){
            raw = eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
            first_child = false;
        }else{
            raw = eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -alpha - 1, -alpha);
            int temp_score = same ? raw : -raw;
            if(temp_score > alpha && temp_score < beta){
                raw = eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
            }
        }

        int score = same ? raw : -raw;
        delete next;

        if(score > best_score){
            best_score = score;
            best_move = action;
            has_best_move = true;
        }
        if(score > alpha){
            alpha = score;
        }
        if(alpha >= beta){
            break;
        }
    }

    history.pop(state->hash());

    TTFlag flag = TT_EXACT;
    if(best_score <= alpha_orig){
        flag = TT_UPPER;
    }else if(best_score >= beta){
        flag = TT_LOWER;
    }
    if(tt.size() > TT_MAX_ENTRIES){
        tt.clear();
    }
    tt[key] = {depth, best_score, flag, best_move, has_best_move};

    return best_score;
}

SearchResult Submission::search(
    State* state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();

    SubmissionParams p = SubmissionParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(state->legal_actions.empty()){
        state->get_legal_actions();
    }

    sort_moves(state);

    int best_score = -P_MAX;
    int alpha = -P_MAX;
    int beta = P_MAX;
    int move_index = 0;
    int total_moves = static_cast<int>(state->legal_actions.size());
    bool first_child = true;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int raw;

        if(first_child){
            raw = eval_ctx(next, depth - 1, history, 1, ctx, p, -beta, -alpha);
            first_child = false;
        }else{
            raw = eval_ctx(next, depth - 1, history, 1, ctx, p, -alpha - 1, -alpha);
            int temp_score = same ? raw : -raw;
            if(temp_score > alpha && temp_score < beta){
                raw = eval_ctx(next, depth - 1, history, 1, ctx, p, -beta, -alpha);
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
        if(score > alpha){
            alpha = score;
        }

        move_index++;
    }

    result.score = best_score;
    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    result.pv = {result.best_move};
    if(tt.size() > TT_MAX_ENTRIES){
        tt.clear();
    }
    tt[state->hash()] = {depth, best_score, TT_EXACT, result.best_move, true};
    return result;
}

ParamMap Submission::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"UseQuiescence", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> Submission::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"UseQuiescence", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
