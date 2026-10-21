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
##  @file Manager-Policy.cpp

File Description:
##  A question of the core to the user (Allow / Deny, answered in the popup of every interface) and the change of model /
##  thinking of a session under the policy of the global session
\**************************************************************/

#include "cluster/Manager.hpp"
#include "cluster/Policy.hpp"
#include "cluster/Models.hpp"
#include "cluster/Usage.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <cctype>
#include <thread>

//----------------------------------------------------------------//
/* FUNCTION */

_cold void cluster::Manager::askUser(const std::string& session, const std::string& header, const std::string& question,
    const std::vector<std::pair<std::string, std::string>>& options, const std::function<void(const std::string&)>& onAnswer)
{
    // the question of an agent (AskUserQuestion) posed by the core on a session: every interface draws its popup, the answer
    // goes to the callback, never to an agent
    cluster::Permission permission;
    permission.requestId = "core-q" + std::to_string(++this->_coreAskN);
    permission.tool = "AskUserQuestion";
    permission.description = question;
    permission.time = cluster::now();
    cluster::Json choices = cluster::Json::array();
    for (const auto &[label, description]: options)
        choices.push_back({{"label", label}, {"description", description}});
    permission.input = {{"questions", cluster::Json::array({{{"question", question}, {"header", header}, {"multiSelect", false}, {"options", choices}}})}};
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_coreAsks[permission.requestId] = onAnswer;
    }
    this->get_(session)->showcasePermission(permission);
    ++this->_version;
    this->notice_(session, question);
    std::thread([this, question]() {
        (void)this->notify({"core-ask", "claude-cluster · a question", question, cluster::Face::Waiting, true, "dialog-question"}, "permission");
    }).detach();
}

_cold std::function<void(const std::string&)> cluster::Manager::takeCoreAsk_(const std::string& requestId)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    const auto it = this->_coreAsks.find(requestId);
    if (it == this->_coreAsks.end()) return {};
    std::function<void(const std::string&)> callback = std::move(it->second);
    this->_coreAsks.erase(it);
    return callback;
}

_cold std::string cluster::Manager::modelChange(const std::string& id, const std::string& model, const std::string& effort, const bool gated)
{
    // the model and / or the effort of a session; gated (the global session asks): the policy judges it - refused (an error that
    // tells why), allowed, or the user asked first (the text of the question returned, the change applied on Allow)
    const std::optional<cluster::Snapshot> snap = this->snapshot(id);
    if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session '" + id + "'");
    const std::function<void(void)> apply = [this, id, model, effort]() {
        if (!model.empty()) this->setModel(id, model);
        if (!effort.empty()) this->setEffort(id, effort);
    };
    if (!gated) {
        apply();
        return "";
    }
    const cluster::PolicyVerdict verdict = cluster::policy_check(this->_config, model, effort, snap->spec.model, snap->spec.effort);
    if (verdict.kind == "refuse") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "refused by the policy: " + verdict.reason);
    if (verdict.kind == "allow") {
        apply();
        return "";
    }
    const std::string name = snap->spec.name;
    const std::string question = "The global session wants to run " + name + " on " + verdict.what + " (it costs more per token). Allow it?";
    this->askUser(id, "Costly", question, {{"Allow", "run " + name + " on " + verdict.what}, {"Deny", "keep " + name + " as it is"}},
        [this, id, name, verdict, apply](const std::string& answer) {
            std::string said;
            if (answer == "Allow") {
                try {
                    apply();
                    said = "[reply] the user allowed " + verdict.what + " for " + name + ": applied";
                } catch (const utils::exception::IException& e) {
                    said = "[reply] the user allowed " + verdict.what + " for " + name + " but it failed: " + e.info();
                }
            } else {
                said = "[reply] the user refused " + verdict.what + " for " + name + ": it stays as it was (choose a cheaper model / effort, or explain why it is needed)";
            }
            if (this->snapshot(GLOBAL_ID)) {
                try {
                    this->send(GLOBAL_ID, said, true, {}, "mcp");
                } catch (const utils::exception::IException&) {} // the global session gone: nobody to tell
            }
        });
    return question;
}

_cold static std::string tier_(const std::string& model)
{
    // a rough class of a model by its name: what it costs against the others of its provider
    std::string name = model;
    std::transform(name.begin(), name.end(), name.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});
    for (const char* word: {"haiku", "mini", "flash", "lite", "small", "nano"})
        if (name.find(word) != std::string::npos) return "cheap";
    for (const char* word: {"opus", "fable", "large", "pro", "max", "ultra"})
        if (name.find(word) != std::string::npos) return "premium";
    for (const char* word: {"sonnet", "medium", "codex", "coder"})
        if (name.find(word) != std::string::npos) return "balanced";
    return "unknown";
}

_cold cluster::Json cluster::Manager::modelsPolicyJson(void)
{
    cluster::Json json = cluster::policy_json(this->_config);
    // the provider of the sub-sessions: the one of the global session, else the default backend
    std::string backend = this->_config.backend;
    if (const std::optional<cluster::Snapshot> global = this->snapshot(GLOBAL_ID)) backend = global->spec.backend;
    const std::string name = this->vendorOf(backend);
    json["provider"] = name;
    json["billing"] = this->authKind(backend);
    const std::optional<cluster::Provider> provider = this->_auth.find(name);
    json["models"] = cluster::Json::array();
    json["efforts"] = cluster::Json::array();
    if (provider) {
        std::vector<cluster::UsageRecord> ledger;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            ledger = this->_usage;
        }
        for (const auto &[model, description]: cluster::model_choices(this->_config, *provider)) {
            double total = 0.0;
            int turns = 0;
            for (const cluster::UsageRecord& r: ledger)
                if (r.costKnown && r.cost > 0.0 && cluster::policy_matches(r.model, model)) {
                    total += r.cost;
                    ++turns;
                }
            cluster::Json entry = {{"model", model}, {"description", description}, {"tier", tier_(model)}, {"costly", cluster::policy_costly_model(this->_config, model)},
                {"allowed", this->_config.policy.allowedModels.empty() || cluster::policy_matches(model, cluster::Json(this->_config.policy.allowedModels).dump(-1, ' ', false, cluster::JSON_LENIENT))
                    || [&]() {
                        for (const std::string& allowed: this->_config.policy.allowedModels)
                            if (cluster::policy_matches(model, allowed)) return true;
                        return false;
                    }()}};
            if (const std::optional<cluster::Price> price = cluster::price_of(this->_config, model))
                entry["price"] = {{"input_per_mtok", price->input}, {"output_per_mtok", price->output}};
            if (turns > 0) entry["avg_turn_usd"] = total / turns;
            json["models"].push_back(entry);
        }
        int rank = 0;
        for (const std::string& level: cluster::effort_levels(provider->driver)) {
            const bool allowed = this->_config.policy.allowedEfforts.empty() || std::find(this->_config.policy.allowedEfforts.begin(), this->_config.policy.allowedEfforts.end(), level)
                != this->_config.policy.allowedEfforts.end();
            json["efforts"].push_back({{"effort", level}, {"rank", ++rank}, {"costly", cluster::policy_costly_effort(this->_config, level)}, {"allowed", allowed}});
        }
    }
    const cluster::Json accounts = this->accountsJson();
    const cluster::Json spent = accounts.is_object() ? accounts.value("spent", cluster::Json::object()) : cluster::Json::object();
    const cluster::LimitsConf& limits = this->_config.limits;
    json["limits"] = {{"scope", limits.scope}, {"block", limits.block}, {"session_budget_usd", this->_config.budget},
        {"day", {{"spent", spent.value("day", 0.0)}, {"limit", limits.dailyUsd}}}, {"week", {{"spent", spent.value("week", 0.0)}, {"limit", limits.weeklyUsd}}},
        {"month", {{"spent", spent.value("month", 0.0)}, {"limit", limits.monthlyUsd}}}};
    return json;
}
