/**************************************************************\
Edition:
##  @date 11/10/2026 by @author Tsukini

File Name:
##  @file Models.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Models.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Demo.hpp"
#include <algorithm>
#include <chrono>
#include <mutex>
#include <map>

/* tools */
using Choices = std::vector<std::pair<std::string, std::string>>;

_cold static Choices list_models_(const cluster::Config& config, const cluster::Provider& provider)
{
    Choices out;
    const std::function<void(const std::string&, const std::string&)> add = [&](const std::string& model, const std::string& description) {
        if (model.empty() || std::any_of(out.begin(), out.end(), [&](const std::pair<std::string, std::string>& c) {return c.first == model;})) return;
        out.emplace_back(model, description);
    };

    if (provider.driver == "claude" && provider.baseUrl.empty()) {
        // Anthropic: the aliases (the latest of each family) then the ids
        add("opus", "alias: the latest Opus");
        add("sonnet", "alias: the latest Sonnet");
        add("haiku", "alias: the latest Haiku");
        add("fable", "alias: the latest Fable");
        add("claude-opus-5-5", "Opus 5.5");
        add("claude-sonnet-5-5", "Sonnet 5.5");
        add("claude-haiku-5-5", "Haiku 5.5");
        add("claude-fable-5-1", "Fable 5.1");
    } else if (provider.name.starts_with("ollama") && !cluster::demo_enabled() && cluster::has_command("ollama")) {
        const std::vector<std::string> lines = cluster::split(cluster::capture({"ollama", "list"}, "", 3).out, '\n');
        for (std::size_t i = 1; i < lines.size(); ++i) {
            const std::vector<std::string> fields = cluster::split(lines[i], ' ');
            if (!fields.empty()) add(fields[0], "local (Ollama)");
        }
    } else if (provider.driver == "opencode" && cluster::has_command(config.command("opencode"))) {
        const std::vector<std::string> lines = cluster::split(cluster::capture({config.command("opencode"), "models"}, "", 5).out, '\n');
        for (std::size_t i = 0; i < lines.size() && out.size() < 80; ++i)
            if (lines[i].find('/') != std::string::npos && lines[i].find(' ') == std::string::npos) add(lines[i], "opencode");
    }
    add(provider.model, "default of the provider");
    auto it = config.models.find(provider.name);
    if (it != config.models.end())
        for (const std::string& model: it->second)
            add(model, "config [models]");
    return out;
}

/* models */
_cold std::vector<std::pair<std::string, std::string>> cluster::model_choices(const cluster::Config& config, const cluster::Provider& provider)
{
    static std::mutex mutex;
    static std::map<std::string, std::pair<std::chrono::steady_clock::time_point, Choices>> cache;
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cache.find(provider.name);
        if (it != cache.end() && now - it->second.first < std::chrono::seconds(60)) return it->second.second;
    }
    Choices list = list_models_(config, provider);
    std::lock_guard<std::mutex> lock(mutex);
    cache[provider.name] = {now, list};
    return list;
}

/* effort */
_cold const std::vector<std::string>& cluster::effort_levels(const std::string& driver)
{
    static const std::vector<std::string> claude = {"low", "medium", "high", "xhigh", "max"};
    static const std::vector<std::string> codex = {"minimal", "low", "medium", "high", "xhigh"};
    static const std::vector<std::string> opencode = {"minimal", "low", "medium", "high", "max"};
    static const std::vector<std::string> none;
    if (driver == "claude") return claude;
    if (driver == "codex") return codex;
    if (driver == "opencode") return opencode;
    return none; // qwen: no effort
}

_cold std::vector<std::string> cluster::effort_args(const std::string& driver, const std::string& effort)
{
    // claude --effort, codex -c model_reasoning_effort=, opencode --variant (provider-specific)
    if (effort.empty() || effort == "default") return {};
    const std::vector<std::string>& levels = cluster::effort_levels(driver);
    if (std::find(levels.begin(), levels.end(), effort) == levels.end()) return {};
    if (driver == "claude") return {"--effort", effort};
    if (driver == "codex") return {"-c", "model_reasoning_effort=\"" + effort + "\""};
    if (driver == "opencode") return {"--variant", effort};
    return {};
}
