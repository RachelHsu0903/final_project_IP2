// #include "submission.hpp"
// #include "pvs.hpp"

// static constexpr int MAX_EFFECTIVE_DEPTH = 9;

// int Submission::eval_ctx(
//     State* state,
//     int depth,
//     GameHistory& history,
//     int ply,
//     SearchContext& ctx,
//     const SubmissionParams& p,
//     int alpha,
//     int beta
// ){
//     PVSParams pp;
//     pp.use_kp_eval = p.use_kp_eval;
//     pp.use_eval_mobility = p.use_eval_mobility;
//     pp.report_partial = p.report_partial;
//     pp.use_quiescence = p.use_quiescence;

//     if(depth > MAX_EFFECTIVE_DEPTH){
//         depth = MAX_EFFECTIVE_DEPTH;
//     }
//     return PVS::eval_ctx(state, depth, history, ply, ctx, pp, alpha, beta);
// }

// SearchResult Submission::search(
//     State* state,
//     int depth,
//     GameHistory& history,
//     SearchContext& ctx
// ){
//     if(depth > MAX_EFFECTIVE_DEPTH){
//         depth = MAX_EFFECTIVE_DEPTH;
//     }
//     return PVS::search(state, depth, history, ctx);
// }

// ParamMap Submission::default_params(){
//     return {
//         {"UseKPEval", "true"},
//         {"UseEvalMobility", "true"},
//         {"UseQuiescence", "true"},
//         {"ReportPartial", "true"},
//     };
// }

// std::vector<ParamDef> Submission::param_defs(){
//     return {
//         {"UseKPEval", ParamDef::CHECK, "true"},
//         {"UseEvalMobility", ParamDef::CHECK, "true"},
//         {"UseQuiescence", ParamDef::CHECK, "true"},
//         {"ReportPartial", ParamDef::CHECK, "true"},
//     };
// }
// #include <utility>
// #include <iostream>
// #include <algorithm>
// #include <unordered_map>
// #include <cstdlib>
// #include "state.hpp"
// #include "submission.hpp"

// enum TTFlag {
//     TT_EXACT,
//     TT_LOWER,
//     TT_UPPER
// };

// struct TTEntry {
//     int depth = -1;
//     int score = 0;
//     Move best_move = Move();
//     TTFlag flag = TT_EXACT;
// };

// static std::unordered_map<uint64_t, TTEntry> tt;
// static constexpr size_t TT_MAX_SIZE = 1u << 20;
// static constexpr int REPETITION_SCORE = 80;
// static constexpr int MAX_EFFECTIVE_DEPTH = 9;
// static const int material_table[7] = {0, 2, 6, 7, 8, 20, 100};

// static int king_threat_score(State* state, int ply);

// static int material_score(State* state){
//     int self_score = 0;
//     int oppn_score = 0;
//     int self = state->player;
//     int oppn = 1 - self;

//     for(int r = 0; r < BOARD_H; r++){
//         for(int c = 0; c < BOARD_W; c++){
//             self_score += material_table[state->piece_at(self, r, c)];
//             oppn_score += material_table[state->piece_at(oppn, r, c)];
//         }
//     }

//     return self_score - oppn_score;
// }

// static int eval_leaf(
//     State* state,
//     const SubmissionParams& p,
//     const GameHistory* history,
//     int ply
// ){
//     int king_score = king_threat_score(state, ply);
//     if(king_score){
//         return king_score;
//     }

//     int score = state->evaluate(
//         p.use_kp_eval,
//         p.use_eval_mobility,
//         history
//     );

//     int material = material_score(state);
//     score += material * 25;

//     if(state->step + ply >= MAX_STEP - 8){
//         score += material * 80;
//     }

//     return score;
// }

// static bool valid_move(const Move& move){
//     return move.first != move.second || move.second.first < BOARD_H;
// }

// static void tt_store(
//     uint64_t key,
//     int depth,
//     int score,
//     const Move& best_move,
//     TTFlag flag
// ){
//     if(tt.size() >= TT_MAX_SIZE){
//         tt.clear();
//     }

//     auto it = tt.find(key);
//     if(it == tt.end() || depth >= it->second.depth){
//         tt[key] = {depth, score, best_move, flag};
//     }
// }

// static bool tt_best_move(uint64_t key, Move& best_move_out){
//     auto it = tt.find(key);
//     if(it == tt.end()){
//         return false;
//     }
//     best_move_out = it->second.best_move;
//     return true;
// }

// static bool clear_line(const Board& board, int ar, int ac, int tr, int tc){
//     int dr = (tr > ar) - (tr < ar);
//     int dc = (tc > ac) - (tc < ac);
//     int r = ar + dr;
//     int c = ac + dc;
//     while(r != tr || c != tc){
//         if(board.board[0][r][c] || board.board[1][r][c]){
//             return false;
//         }
//         r += dr;
//         c += dc;
//     }
//     return true;
// }

// static bool attacks_square(const Board& board, int attacker, int tr, int tc){
//     if(tr < 0 || tc < 0){
//         return false;
//     }

//     for(int r = 0; r < BOARD_H; r++){
//         for(int c = 0; c < BOARD_W; c++){
//             int piece = board.board[attacker][r][c];
//             if(!piece){
//                 continue;
//             }

//             int dr = tr - r;
//             int dc = tc - c;
//             int adr = std::abs(dr);
//             int adc = std::abs(dc);

//             switch(piece){
//                 case 1: {
//                     int forward = attacker == 0 ? -1 : 1;
//                     if(dr == forward && adc == 1){
//                         return true;
//                     }
//                     break;
//                 }
//                 case 2:
//                     if((dr == 0 || dc == 0) &&
//                        clear_line(board, r, c, tr, tc)){
//                         return true;
//                     }
//                     break;
//                 case 3:
//                     if((adr == 2 && adc == 1) || (adr == 1 && adc == 2)){
//                         return true;
//                     }
//                     break;
//                 case 4:
//                     if(adr == adc && clear_line(board, r, c, tr, tc)){
//                         return true;
//                     }
//                     break;
//                 case 5:
//                     if((dr == 0 || dc == 0 || adr == adc) &&
//                        clear_line(board, r, c, tr, tc)){
//                         return true;
//                     }
//                     break;
//                 case 6:
//                     if(std::max(adr, adc) == 1){
//                         return true;
//                     }
//                     break;
//             }
//         }
//     }

//     return false;
// }

// static void find_king(const Board& board, int player, int& kr, int& kc){
//     kr = -1;
//     kc = -1;
//     for(int r = 0; r < BOARD_H; r++){
//         for(int c = 0; c < BOARD_W; c++){
//             if(board.board[player][r][c] == 6){
//                 kr = r;
//                 kc = c;
//                 return;
//             }
//         }
//     }
// }

// static int king_threat_score(State* state, int ply){
//     int self = state->player;
//     int opp = 1 - self;
//     int self_kr, self_kc, opp_kr, opp_kc;
//     find_king(state->board, self, self_kr, self_kc);
//     find_king(state->board, opp, opp_kr, opp_kc);

//     if(attacks_square(state->board, opp, self_kr, self_kc)){
//         return M_MAX + ply;
//     }
//     if(attacks_square(state->board, self, opp_kr, opp_kc)){
//         return P_MAX - ply;
//     }
//     return 0;
// }

// static bool king_move_into_attack(State* state, const Move& action){
//     int self = state->player;
//     Point from = action.first;
//     Point to = action.second;
//     if(state->piece_at(self, from.first, from.second) != 6){
//         return false;
//     }

//     Board next = state->board;
//     int opp = 1 - self;
//     next.board[self][from.first][from.second] = 0;
//     next.board[opp][to.first][to.second] = 0;
//     next.board[self][to.first][to.second] = 6;
//     return attacks_square(next, opp, to.first, to.second);
// }

// /*============================================================
//  * Move Ordering
//  *============================================================*/
// static int move_score(State* state, const Move& action){
//     Point from = action.first;
//     Point to = action.second;

//     int self = state->player;
//     int opp = 1 - self;

//     int my_piece = state->piece_at(self, from.first, from.second);
//     int captured = state->piece_at(opp, to.first, to.second);

//     int score = 0;

//     // Move Ordering：
//     if(captured > 0){
//         score += 1000 + PIECE_VALUES[captured] * 10 - PIECE_VALUES[my_piece];
//     }

//     // Move Ordering
//     if(captured == 6){
//         score += 100000;
//     }

//     // Move Ordering
//     if(my_piece == 1){
//         if((self == 0 && to.first == 0) || (self == 1 && to.first == BOARD_H - 1)){
//             score += 800;
//         }
//     }

//     if(my_piece == 6 && king_move_into_attack(state, action)){
//         score -= 200000;
//     }

//     return score;
// }
// static bool is_noisy_move(State* state, const Move& action){
//     Point from = action.first;
//     Point to = action.second;

//     int self = state->player;
//     int opp = 1 - self;

//     int my_piece = state->piece_at(self, from.first, from.second);
//     int captured = state->piece_at(opp, to.first, to.second);

//     // ?��?�?noisy
//     if(captured > 0){
//         return true;
//     }

//     // ?��?變�? noisy
//     if(my_piece == 1){
//         if((self == 0 && to.first == 0) || (self == 1 && to.first == BOARD_H - 1)){
//             return true;
//         }
//     }

//     return false;
// }
// /*============================================================
//  * QUIESCENCE SEARCH
//  *============================================================*/

// static int quiescence(
//     State* state,
//     GameHistory& history,
//     int ply,
//     SearchContext& ctx,
//     const SubmissionParams& p,
//     int alpha,
//     int beta
// ){
//     ctx.nodes++;

//     if(ctx.stop){
//         return 0;
//     }

//     /*============================================================
//      * QUIESCENCE DEPTH LIMIT
//      *
//      * ?��??��??�太?��??��?尋�???
//      *============================================================*/
//     if(ply >= 8){
//         return eval_leaf(state, p, &history, ply);
//     }

//     if(state->legal_actions.empty() && state->game_state == UNKNOWN){
//         state->get_legal_actions();
//     }

//     if(state->game_state == WIN){
//         return P_MAX - ply;
//     }

//     if(state->game_state == DRAW){
//         return 0;
//     }

//     /*============================================================
//      * REPETITION PENALTY
//      *
//      * ?��? AI 一?�選?��?複�??��?導致來�?走�??��?
//      *
//      * 注�?：這裡檢查?�是?��? state ?�否??history 裡出?��???
//      * quiescence ?�呼?�者�?eval_ctx depth<=0）在?�叫??
//      * 已�? pop 了自己�??�以這裡??history 不含?��? state�?
//      * 不�??��?誤判??
//      *============================================================*/
//     int rep_score;
//     if(state->check_repetition(history, rep_score)){
//         return material_score(state) * 50;
//     }

//     int stand_pat = eval_leaf(state, p, &history, ply);

//     if(stand_pat >= beta){
//         return beta;
//     }

//     if(stand_pat > alpha){
//         alpha = stand_pat;
//     }

//     std::sort(state->legal_actions.begin(), state->legal_actions.end(),
//         [state](const Move& a, const Move& b){
//             return move_score(state, a) > move_score(state, b);
//         }
//     );

//     for(auto& action : state->legal_actions){
//         if(!is_noisy_move(state, action)){
//             continue;
//         }

//         State* next = state->next_state(action);
//         bool same = next->same_player_as_parent();

//         history.push(next->hash());

//         int raw = quiescence(
//             next,
//             history,
//             ply + 1,
//             ctx,
//             p,
//             -beta,
//             -alpha
//         );

//         history.pop(next->hash());

//         int score = same ? raw : -raw;

//         delete next;

//         if(score >= beta){
//             return beta;
//         }

//         if(score > alpha){
//             alpha = score;
//         }
//     }

//     return alpha;
// }
// /*============================================================
//  * PVS ??eval_ctx
//  *
//  * PVS = Principal Variation Search
//  * ?�以?�解??Alpha-Beta ?�進�???
//  *============================================================*/
// int Submission::eval_ctx(
//     State *state,
//     int depth,
//     GameHistory& history,
//     int ply,
//     SearchContext& ctx,
//     const SubmissionParams& p,
//     int alpha,   // Alpha-Beta：目?�已?��??�好�???
//     int beta     // Alpha-Beta：目?��?許�?上�?
// ){
//     ctx.nodes++;

//     if(ply > ctx.seldepth){
//         ctx.seldepth = ply;
//     }

//     if(ctx.stop){
//         return 0;
//     }

//     if(state->legal_actions.empty() && state->game_state == UNKNOWN){
//         state->get_legal_actions();
//     }

//     // Terminal：可以�??��?越早贏�?�?
//     if(state->game_state == WIN){
//         return P_MAX - ply;
//     }

//     if(state->game_state == DRAW){
//         return 0;
//     }

//     int rep_score;
//     if(state->check_repetition(history, rep_score)){
//         return material_score(state) * 50;
//     }

//     if(state->step + ply >= MAX_STEP){
//         return material_score(state) * 100;
//     }

//     /*============================================================
//      * QUIESCENCE SEARCH
//      *
//      * depth ?��?後�?
//      * 不直??evaluate�?
//      * ?�是繼�??��??��?/?��?局?��?
//      *
//      * 修正：在?�入 quiescence 之�???pop ?��? state�?
//      * ?��? quiescence 裡�? check_repetition 誤判??
//      * quiescence ?�部?�管??child state ??push/pop�?
//      * 不管?�當??state??
//      *============================================================*/
//     if(depth <= 0){

//         int score;

//         if(p.use_quiescence){

//             score = quiescence(
//                 state,
//                 history,    // history 此�?不含?��? state，�??�誤?��?�?
//                 ply,
//                 ctx,
//                 p,
//                 alpha,
//                 beta
//             );

//         }else{

//             score = eval_leaf(state, p, &history, ply);
//         }

//         return score;
//     }

//     uint64_t key = state->hash();
//     Move tt_move;
//     tt_best_move(key, tt_move);

//     // depth > 0：正常�?尋�?push ?��? state ??history
//     history.push(state->hash());

//     // Move Ordering：�??��?子、�??�、�?�?
//     std::sort(state->legal_actions.begin(), state->legal_actions.end(),
//         [state](const Move& a, const Move& b){
//             return move_score(state, a) > move_score(state, b);
//         }
//     );

//     if(valid_move(tt_move)){
//         auto it = std::find(
//             state->legal_actions.begin(),
//             state->legal_actions.end(),
//             tt_move
//         );
//         if(it != state->legal_actions.end()){
//             std::iter_swap(state->legal_actions.begin(), it);
//         }
//     }

//     int best_score = -P_MAX;
//     Move best_move;

//     // PVS：第一??child ?��???Alpha-Beta ??
//     // 後面??child ?�用 null-window search
//     bool first_child = true;

//     for(auto& action : state->legal_actions){
//         State* next = state->next_state(action);
//         bool same = next->same_player_as_parent();

//         int raw;

//         if(first_child){
//             // ================================
//             // PVS Part 1�?
//             // 第�??��??�用完整 Alpha-Beta Search
//             // ================================
//             raw = eval_ctx(
//                 next,
//                 depth - 1,
//                 history,
//                 ply + 1,
//                 ctx,
//                 p,
//                 -beta,     // Alpha-Beta：Negamax window 轉�?
//                 -alpha     // Alpha-Beta：Negamax window 轉�?
//             );

//             first_child = false;
//         }else{
//             // ================================
//             // PVS Part 2�?
//             // 後面?��??��???Null Window Search
//             //
//             // ?��??�這兩?��??��?
//             //     -alpha - 1, -alpha
//             //
//             // ?�代表只快速檢?��?
//             // ?��?move ?��??�可?��??��? alpha ?�好
//             // ================================
//             raw = eval_ctx(
//                 next,
//                 depth - 1,
//                 history,
//                 ply + 1,
//                 ctx,
//                 p,
//                 -alpha - 1,   // PVS：Null Window Search
//                 -alpha        // PVS：Null Window Search
//             );

//             int temp_score = same ? raw : -raw;

//             // ================================
//             // PVS Part 3�?
//             // 如�? null-window ?�現?�步?�能�?alpha 好�?
//             // ?��??�用完整 Alpha-Beta window ?��?�?
//             // ================================
//             if(temp_score > alpha && temp_score < beta){
//                 raw = eval_ctx(
//                     next,
//                     depth - 1,
//                     history,
//                     ply + 1,
//                     ctx,
//                     p,
//                     -beta,     // PVS Re-search：�???Alpha-Beta window
//                     -alpha     // PVS Re-search：�???Alpha-Beta window
//                 );
//             }
//         }

//         int score = same ? raw : -raw;

//         delete next;

//         if(score > best_score){
//             best_score = score;
//             best_move = action;
//         }

//         // ================================
//         // Alpha-Beta Part 1�?
//         // ?�新 alpha
//         // ================================
//         if(score > alpha){
//             alpha = score;
//         }

//         // ================================
//         // Alpha-Beta Part 2�?
//         // alpha >= beta ?�剪??
//         // ?��?就是 Alpha-Beta Pruning ?�核�?
//         // ================================
//         if(alpha >= beta){
//             break;
//         }
//     }

//     history.pop(state->hash());

//     tt_store(key, depth, best_score, best_move, TT_EXACT);

//     return best_score;
// }

// /*============================================================
//  * PVS ??search
//  * Root 層�?�?
//  *============================================================*/
// SearchResult Submission::search(
//     State *state,
//     int depth,
//     GameHistory& history,
//     SearchContext& ctx
// ){
//     ctx.reset();

//     SubmissionParams p = SubmissionParams::from_map(ctx.params);
//     if(depth > MAX_EFFECTIVE_DEPTH){
//         depth = MAX_EFFECTIVE_DEPTH;
//     }
//     SearchResult result;
//     result.depth = depth;
//     if(depth <= 1){
//         tt.clear();
//         tt.reserve(TT_MAX_SIZE);
//     }

//     if(!state->legal_actions.size()){
//         state->get_legal_actions();
//     }

//     // Root Move Ordering
//     std::sort(state->legal_actions.begin(), state->legal_actions.end(),
//         [state](const Move& a, const Move& b){
//             return move_score(state, a) > move_score(state, b);
//         }
//     );

//     Move tt_move;
//     auto tt_it = tt.find(state->hash());
//     if(tt_it != tt.end()){
//         tt_move = tt_it->second.best_move;
//         auto move_it = std::find(
//             state->legal_actions.begin(),
//             state->legal_actions.end(),
//             tt_move
//         );
//         if(move_it != state->legal_actions.end()){
//             std::iter_swap(state->legal_actions.begin(), move_it);
//         }
//     }

//     int best_score = -P_MAX;

//     // Alpha-Beta root window
//     int alpha = -P_MAX;
//     int beta = P_MAX;

//     int move_index = 0;
//     int total_moves = (int)state->legal_actions.size();

//     // PVS：root 第�???child 完整?��?後面 null-window
//     bool first_child = true;

//     for(auto& action : state->legal_actions){
//         State* next = state->next_state(action);
//         bool same = next->same_player_as_parent();

//         int raw;

//         if(first_child){
//             // PVS：第一??root move 完整 Alpha-Beta ??
//             raw = eval_ctx(
//                 next,
//                 depth - 1,
//                 history,
//                 1,
//                 ctx,
//                 p,
//                 -beta,
//                 -alpha
//             );

//             first_child = false;
//         }else{
//             // PVS：其�?root move ?�用 Null Window Search
//             raw = eval_ctx(
//                 next,
//                 depth - 1,
//                 history,
//                 1,
//                 ctx,
//                 p,
//                 -alpha - 1,   // PVS Null Window
//                 -alpha        // PVS Null Window
//             );

//             int temp_score = same ? raw : -raw;

//             // PVS：�??�可?�更好�??�新完整?��?
//             if(temp_score > alpha && temp_score < beta){
//                 raw = eval_ctx(
//                     next,
//                     depth - 1,
//                     history,
//                     1,
//                     ctx,
//                     p,
//                     -beta,
//                     -alpha
//                 );
//             }
//         }

//         int score = same ? raw : -raw;

//         delete next;

//         if(score > best_score){
//             best_score = score;
//             result.best_move = action;

//             if(p.report_partial && ctx.on_root_update){
//                 ctx.on_root_update({
//                     result.best_move,
//                     best_score,
//                     depth,
//                     move_index + 1,
//                     total_moves
//                 });
//             }
//         }

//         // Alpha-Beta：root ?�新 alpha
//         if(score > alpha){
//             alpha = score;
//         }

//         move_index++;
//     }

//     result.score = best_score;
//     result.nodes = ctx.nodes;
//     result.seldepth = ctx.seldepth;

//     return result;
// }

// /*============================================================
//  * PVS ??default_params / param_defs
//  *============================================================*/
// ParamMap Submission::default_params(){
//     return {
//         {"UseKPEval", "true"},
//         {"UseEvalMobility", "false"},

//         // Quiescence Search
//         {"UseQuiescence", "true"},

//         {"ReportPartial", "true"},
//     };
// }
// std::vector<ParamDef> Submission::param_defs(){
//     return {
//         {"UseKPEval", ParamDef::CHECK, "true"},
//         {"UseEvalMobility", ParamDef::CHECK, "false"},

//         // Quiescence Search
//         {"UseQuiescence", ParamDef::CHECK, "true"},

//         {"ReportPartial", ParamDef::CHECK, "true"},
//     };
// }