/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Policy.cpp

File Description:
##  The model / thinking policy of the global session (see Policy.hpp)
\**************************************************************/

#include "cluster/Policy.hpp"
#include <algorithm>
#include <cctype>

//----------------------------------------------------------------//
/* TOOLS */

_cold static std::string lower_(const std::string& text)
{
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});
    return out;
}

_cold static bool in_list_(const std::vector<std::string>& list, const std::string& value)
{
    for (const std::string& entry: list)
        if (cluster::policy_matches(value, entry)) return true;
    return false;
}

_cold static bool same_in_(const std::vector<std::string>& list, const std::string& value)
{
    const std::string wanted = lower_(value);
    for (const std::string& entry: list)
        if (lower_(entry) == wanted) return true;
    return false;
}

_cold static std::string joined_(const std::vector<std::string>& list)
{
    std::string out;
    for (const std::string& item: list)
        out += (out.empty() ? "" : ", ") + item;
    return out;
}

//----------------------------------------------------------------//
/* FUNCTION */

_cold bool cluster::policy_matches(const std::string& model, const std::string& entry)
{
    const std::string a = lower_(model);
    const std::string b = lower_(entry);
    return !a.empty() && !b.empty() && (a == b || a.find(b) != std::string::npos || b.find(a) != std::string::npos);
}

// xstyle: ignore-next CPP-SINGLE-STATEMENT (it uses a helper private to Policy.cpp)
_cold bool cluster::policy_costly_model(const cluster::Config& config, const std::string& model)
{
    return in_list_(config.policy.costlyModels, model);
}

// xstyle: ignore-next CPP-SINGLE-STATEMENT (it uses a helper private to Policy.cpp)
_cold bool cluster::policy_costly_effort(const cluster::Config& config, const std::string& effort)
{
    return same_in_(config.policy.costlyEfforts, effort);
}

_cold cluster::PolicyVerdict cluster::policy_check(const cluster::Config& config, const std::string& model, const std::string& effort, const std::string& currentModel,
    const std::string& currentEffort)
{
    const cluster::PolicyConf& policy = config.policy;
    const bool modelChange = !model.empty() && lower_(model) != lower_(currentModel);
    const bool effortChange = !effort.empty() && lower_(effort) != lower_(currentEffort) && lower_(effort) != "default";
    // the default of the user: forced (nothing else), or the one that needs no question
    if (modelChange && policy.modelMode == "force" && !policy.defaultModel.empty() && !cluster::policy_matches(model, policy.defaultModel))
        return {"refuse", "the model is forced to '" + policy.defaultModel + "' by the user (Settings > Model policy): sessions run on it, '" + model + "' is not allowed", ""};
    if (effortChange && policy.effortMode == "force" && !policy.defaultEffort.empty() && lower_(effort) != lower_(policy.defaultEffort))
        return {"refuse", "the thinking effort is forced to '" + policy.defaultEffort + "' by the user (Settings > Model policy): '" + effort + "' is not allowed", ""};
    if (modelChange && !policy.allowedModels.empty() && !in_list_(policy.allowedModels, model))
        return {"refuse", "the model '" + model + "' is not allowed by the user (Settings > Model policy); allowed: " + joined_(policy.allowedModels), ""};
    if (effortChange && !policy.allowedEfforts.empty() && !same_in_(policy.allowedEfforts, effort))
        return {"refuse", "the thinking effort '" + effort + "' is not allowed by the user (Settings > Model policy); allowed: " + joined_(policy.allowedEfforts), ""};
    // the costly ones: the user decides (unless it is their own default)
    std::string what;
    if (policy.askCostly) {
        if (modelChange && cluster::policy_costly_model(config, model) && !cluster::policy_matches(model, policy.defaultModel)) what = model;
        if (effortChange && cluster::policy_costly_effort(config, effort) && lower_(effort) != lower_(policy.defaultEffort))
            what += (what.empty() ? "" : ", ") + std::string("thinking ") + effort;
    }
    if (!what.empty()) return {"ask", "", what};
    return {"allow", "", ""};
}

_cold cluster::Json cluster::policy_json(const cluster::Config& config)
{
    const cluster::PolicyConf& p = config.policy;
    return {{"default_model", p.defaultModel}, {"default_effort", p.defaultEffort}, {"model_mode", p.modelMode}, {"effort_mode", p.effortMode},
        {"allowed_models", p.allowedModels}, {"allowed_efforts", p.allowedEfforts}, {"ask_costly", p.askCostly}, {"costly_models", p.costlyModels},
        {"costly_efforts", p.costlyEfforts}, {"global_model", config.globalModel}, {"global_effort", config.globalEffort},
        {"modes", {{"free", "no default: choose by the complexity of each task"}, {"advise", "the default is the starting point: deviate for a reason (cheaper for easy tasks, "
            "stronger for hard ones)"}, {"force", "the default is the only one allowed"}}}};
}
