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
    int depth;//這個盤面當時搜到多深
    int score;
    TTFlag flag; //這個分數是精準值、下界、還是上界
    Move best_move;//這個盤面當時找到的最好步
    bool has_best_move;//有沒有 best_move
};

static std::unordered_map<uint64_t, TTEntry> tt;
static constexpr size_t TT_MAX_ENTRIES = 200000;

//判斷兩步棋是不是一樣
static bool same_move(const Move& a, const Move& b){
    return a.first == b.first && a.second == b.second;
}
//從 (ar, ac) 到 (tr, tc) 中間有沒有棋子擋住
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

//attacker 這一方，有沒有任何棋子可以攻擊 (tr, tc) 這格
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

//如果我這一步是移動 King，走完後 King 會不會走進敵人的攻擊範圍
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
//快速模擬「這步走完後的棋盤長怎樣」
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

//找某一方的 King 在哪裡
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
//目前這個棋盤上，我自己的 King 有沒有被對手攻擊
static bool own_king_attacked_on_board(const Board& board, int self){
    int opp = 1 - self;
    int kr = -1;
    int kc = -1;
    if(!find_king(board, self, kr, kc)){
        return false;
    }
    return attacks_square(board, opp, kr, kc);
}
//我有沒有威脅到對方的王
static bool threatens_enemy_king_on_board(const Board& board, int self){
    int opp = 1 - self;
    int kr = -1;
    int kc = -1;
    if(!find_king(board, opp, kr, kc)){
        return true;
    }
    return attacks_square(board, self, kr, kc);
}

//Move Ordering 的評分函式
//哪些 move 應該先被 Alpha-Beta / PVS 搜尋
static int move_score(State* state, const Move& action){
    Point from = action.first;
    Point to = action.second;

    int self = state->player;
    int opp = 1 - self;
    //取得這步資訊
    int my_piece = state->piece_at(self, from.first, from.second);
    int captured = state->piece_at(opp, to.first, to.second);

    int score = 0;
    //吃子加分
    if(captured > 0){
        score += 1000 + PIECE_VALUES[captured] * 10 - PIECE_VALUES[my_piece];
    }
    //吃 King 超大加分
    if(captured == 6){
        score += 100000;
    }
    //Pawn 升變加分
    if(my_piece == 1){
        if((self == 0 && to.first == 0) || (self == 1 && to.first == BOARD_H - 1)){
            score += 800;
        }
    }
    //King 走進攻擊範圍扣超多分
    if(my_piece == 6 && king_move_into_attack(state, action)){
        score -= 200000;
    }
    Board next = board_after_move(state, action);
    //自己 King 被攻擊，扣分
    if(own_king_attacked_on_board(next, self)){
        score -= 150000;
    }
    //威脅對方 King，加分
    if(threatens_enemy_king_on_board(next, self)){
        score += 6000;
    }

    return score;
}

//把合法步 legal_actions 重新排序，讓搜尋先看最有可能好的步
//提升alphabeta的剪枝效能
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
            //TT最佳步永遠第一
            //TT 的 best move 是之前在相同盤面搜尋得到的最佳步
            if(has_tt_move){
                bool a_tt = same_move(a, tt_move);
                bool b_tt = same_move(b, tt_move);
                if(a_tt != b_tt){
                    return a_tt;
                }
            }
            //其他用move score排序
            return move_score(state, a) > move_score(state, b);
        }
    );
}

//Quiescence Search 的入口篩選器
//是不是「會讓局面劇烈變化的步」
static bool is_noisy_move(State* state, const Move& action){
    Point from = action.first;
    Point to = action.second;

    int self = state->player;
    int opp = 1 - self;

    int my_piece = state->piece_at(self, from.first, from.second);
    int captured = state->piece_at(opp, to.first, to.second);

    if(captured > 0){//吃子
        return true;
    }
    if(my_piece == 1){//升變
        if((self == 0 && to.first == 0) || (self == 1 && to.first == BOARD_H - 1)){
            return true;
        }
    }
    return false;
}

//Quiescence Search
// 到 depth == 0 之後，不要馬上評分
// 而是繼續看「吃子、升變」這種劇烈變化，等局面安靜再回傳分數

/*============================================================
    quiescence()
    │
    ├─ 1. 停止條件
    │   ├─ 時間到
    │   └─ ply >= 8
    │
    ├─ 2. 終局判斷
    │   ├─ WIN
    │   ├─ DRAW
    │   └─ repetition
    │
    ├─ 3. stand_pat
    │   ├─ 直接 evaluate
    │   ├─ beta cutoff
    │   └─ update alpha
    │
    ├─ 4. noisy move loop
    │   ├─ sort_moves
    │   ├─ 只保留吃子/升變
    │   └─ 普通步跳過
    │
    └─ 5. recursive search
        ├─ next_state
        ├─ quiescence(next)
        ├─ score 轉換
        ├─ beta cutoff
        └─ update alpha
 *============================================================*/

static int quiescence(
    State* state,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const SubmissionParams& p,
    int alpha,
    int beta
){
    ctx.nodes++;//計算節點

    ////安全停止區

    if(ctx.stop){//如果時間到，停止
        return 0;
    }

    if(ply >= 8){//Quiescence 最多只延伸 8 層
        return state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    }

    ///局面初始化與終局判斷
    //產生合法步
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }
    //終局直接回傳
    if(state->game_state == WIN){
        return P_MAX - ply;
    }
    if(state->game_state == DRAW){
        return 0;
    }
    //重複局面判斷
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }

    ///目前局面直接評分
    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    //alphabeta
    if(stand_pat >= beta){
        return beta;
    }
    if(stand_pat > alpha){
        alpha = stand_pat;
    }

    //搜吃子 升變
    sort_moves(state);

    for(auto& action : state->legal_actions){
        if(!is_noisy_move(state, action)){
            continue;
        }
        //遞迴搜尋 noisy move，更新 alpha/beta
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        history.push(next->hash());
        int raw = quiescence(next, history, ply + 1, ctx, p, -beta, -alpha);//對手回合
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
// eval_ctx()
// │
// ├─ 1. 搜尋狀態更新
// │   ├─ nodes++
// │   ├─ seldepth
// │   └─ stop 檢查
// │
// ├─ 2. 產生合法步、終局判斷
// │   ├─ get_legal_actions()
// │   ├─ WIN
// │   ├─ DRAW
// │   └─ repetition
// │
// ├─ 3. 查 TT
// │   ├─ 如果以前搜過
// │   ├─ 而且深度夠
// │   └─ 直接用 / 縮小 alpha beta
// │
// ├─ 4. depth 到 0
// │   ├─ 用 quiescence
// │   └─ 或直接 evaluate
// │
// ├─ 5. 搜尋子節點
// │   ├─ history.push
// │   ├─ sort_moves
// │   ├─ 第一個 child：full window
// │   ├─ 後面 child：null window
// │   ├─ 必要時 re-search
// │   └─ alpha-beta cutoff
// │
// └─ 6. 存 TT
//     ├─ score
//     ├─ depth
//     ├─ flag
//     └─ best_move
 *============================================================*/

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
    ///搜尋狀態管理
    ctx.nodes++;//目前總共搜尋了多少節點

    if(ply > ctx.seldepth){//實際搜尋到最深幾層
        ctx.seldepth = ply;
    }
    if(ctx.stop){//如果時間快到了
        return 0;
    }

    ///終局與特殊局面判斷
    //產生合法步
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }
    //能吃王
    if(state->game_state == WIN){
        return P_MAX - ply;//讓 AI 在同樣能贏的情況下，選擇 更快贏
    }
    if(state->game_state == DRAW){
        return 0;
    }
    //重複太多次當和局
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }

    ///transition table 減少重複搜尋
    uint64_t key = state->hash();
    int alpha_orig = alpha;
    auto tt_it = tt.find(key);
    if(tt_it != tt.end() && tt_it->second.depth >= depth){
        const TTEntry& entry = tt_it->second;
        //真正分數->直接回傳
        if(entry.flag == TT_EXACT){
            return entry.score;
        }
        //下界->更新alpha(不知道真正分數 但知道下界)
        if(entry.flag == TT_LOWER && entry.score > alpha){
            alpha = entry.score;
        //上屆->更新beta    
        }else if(entry.flag == TT_UPPER && entry.score < beta){
            beta = entry.score;
        }
        if(alpha >= beta){
            return entry.score;//不用再展開子節點
        }
    }

    //depth=0 開Quiescence
    if(depth <= 0){
        if(p.use_quiescence){
            return quiescence(state, history, ply, ctx, p, alpha, beta);
        }
        return state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    }

    ///真正開始搜尋每個 move
    ///先用 Move Ordering 排好合法步，然後用 PVS 搜尋，找出分數最高的 move
    history.push(state->hash());

    sort_moves(state);//排序合法步

    int best_score = -P_MAX;//初始化最佳分數
    Move best_move;
    bool has_best_move = false;
    bool first_child = true;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int raw;

        if(first_child){//第一個 child 用完整搜尋(排序後第一步最可能是好步，所以認真搜)
            raw = eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
            first_child = false;
        }else{//後面的 child 用 PVS 快速測試
            raw = eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -alpha - 1, -alpha);
            int temp_score = same ? raw : -raw;
            if(temp_score > alpha && temp_score < beta){
                raw = eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
            }
        }

        int score = same ? raw : -raw;
        delete next;

        //更新最佳分數
        if(score > best_score){
            best_score = score;
            best_move = action;
            has_best_move = true;
        }

        //alphabeta剪枝
        if(score > alpha){
            alpha = score;
        }
        if(alpha >= beta){
            break;
        }
    }
    
    ///存TT
    history.pop(state->hash());

    TTFlag flag = TT_EXACT;
    if(best_score <= alpha_orig){//小於下限 更新下限 
        flag = TT_UPPER;
    }else if(best_score >= beta){//大於上限 更新上限
        flag = TT_LOWER;
    }
    if(tt.size() > TT_MAX_ENTRIES){
        tt.clear();//避免記憶體一直長大。
    }
    tt[key] = {depth, best_score, flag, best_move, has_best_move};

    return best_score;
}

//真正決定「這一回合我要下哪一步」的函式
SearchResult Submission::search(
    State* state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){

    //初始化
    ctx.reset();

    SubmissionParams p = SubmissionParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    //產生合法步並排序
    if(state->legal_actions.empty()){
        state->get_legal_actions();
    }

    sort_moves(state);

    //初始化 Alpha-Beta
    int best_score = -P_MAX;
    int alpha = -P_MAX;
    int beta = P_MAX;
    int move_index = 0;
    int total_moves = static_cast<int>(state->legal_actions.size());
    bool first_child = true;

    //逐一搜尋 root move
    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int raw;
        //PVS:第一步完整搜尋
        if(first_child){
            raw = eval_ctx(next, depth - 1, history, 1, ctx, p, -beta, -alpha);
            first_child = false;
        }else{//後面用null window快速測試
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

    //存TT
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
