#pragma once

#include "search_types.hpp"
#include "game_history.hpp"
#include <vector>
#include <string>


struct PVSParams {
    bool use_kp_eval = true;
    bool use_eval_mobility = true;
    bool report_partial = true;
    
    bool use_quiescence = true;
    


    static PVSParams from_map(const ParamMap& params){
        PVSParams p;

        auto get_bool = [&](const std::string& key, bool default_value){
            auto it = params.find(key);
            if(it == params.end()){
                return default_value;
            }
            return it->second == "true" || it->second == "1";
        };

        p.use_kp_eval = get_bool("UseKPEval", true);
        p.use_eval_mobility = get_bool("UseEvalMobility", true);
        p.report_partial = get_bool("ReportPartial", true);
        p.use_quiescence =get_bool("UseQuiescence", true);//quiescene
        

        return p;
    }
};

class PVS{
public:
    static int eval_ctx(
        State *state,
        int depth,
        GameHistory& history,
        int ply,
        SearchContext& ctx,
        const PVSParams& p,
        int alpha,
        int beta
    );

    static SearchResult search(
        State *state,
        int depth,
        GameHistory& history,
        SearchContext& ctx
    );

    static ParamMap default_params();
    static std::vector<ParamDef> param_defs();
};