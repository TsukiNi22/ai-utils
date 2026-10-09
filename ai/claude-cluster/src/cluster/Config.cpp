/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Config.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Templates.hpp"
#include "cluster/Config.hpp"
#include "cluster/Tools.hpp"
#include <toml++/toml.hpp>
#include <optional>
#include <fstream>

/* tools */
// Built-in styles: html-style (light / dark tokens of the docs) and a few classic palettes
_cold static std::map<std::string, cluster::Style> builtin_styles_(void)
{
    // name, dark, bg, surface, text, muted, accent, border, ok, warn, error
    const std::vector<cluster::Style> styles = {
        {"html-light", false, "#f7f7f5", "#ffffff", "#1d1f23", "#5d636e", "#2f5bd3", "#dfe1e5", "#1f7a4a", "#a8540a", "#b42318"},
        {"html-dark", true, "#15171b", "#1d2025", "#e6e7ea", "#9aa0ab", "#7d9cf0", "#30343b", "#6fcf97", "#f0a868", "#f47174"},
        {"nord", true, "#2e3440", "#3b4252", "#eceff4", "#a3abb9", "#88c0d0", "#4c566a", "#a3be8c", "#ebcb8b", "#bf616a"},
        {"gruvbox", true, "#282828", "#32302f", "#ebdbb2", "#a89984", "#fabd2f", "#504945", "#b8bb26", "#fe8019", "#fb4934"},
        {"solarized-light", false, "#fdf6e3", "#eee8d5", "#073642", "#586e75", "#268bd2", "#d3cbb7", "#859900", "#b58900", "#dc322f"},
        {"mono", true, "#000000", "#111111", "#e0e0e0", "#9e9e9e", "#ffffff", "#3a3a3a", "#e0e0e0", "#e0e0e0", "#ffffff"},
    };
    std::map<std::string, cluster::Style> map;

    for (const cluster::Style& style: styles)
        map[style.name] = style;
    return map;
}

template<typename Node>
_cold static std::vector<std::string> strings_(const toml::node_view<Node>& node, const std::vector<std::string>& fallback)
{
    const toml::array* array = node.as_array();
    if (!array) return fallback;
    std::vector<std::string> values;
    for (const toml::node& item: *array)
        if (const std::optional<std::string> value = item.value<std::string>()) values.push_back(*value);
    return values;
}

/* constructor */
_cold cluster::Config::Config(void)
    : _path(cluster::config_dir() / "config.toml")
{
    this->defaults_();
}

/* setup */
_cold void cluster::Config::defaults_(void)
{
    this->styles = builtin_styles_();
    this->keys = {
        {"palette", "Ctrl+K"}, {"next_session", "Ctrl+N"}, {"prev_session", "Ctrl+P"}, {"global", "Ctrl+G"},
        {"new_session", "Ctrl+T"}, {"close_session", "Ctrl+W"}, {"allow", "Ctrl+Y"}, {"deny", "Ctrl+D"},
        {"interrupt", "Ctrl+C"}, {"push_to_talk", "F5"}, {"mute", "F6"}, {"layout", "F2"}, {"quit", "Ctrl+Q"},
    };
}

_cold void cluster::Config::load(void)
{
    std::error_code error;

    // First run: the default files, commented, to edit
    std::filesystem::create_directories(cluster::config_dir(), error);
    if (!std::filesystem::exists(this->_path)) std::ofstream(this->_path) << cluster::templates::CONFIG;
    const std::filesystem::path prompt = cluster::config_dir() / "global.md";
    if (!std::filesystem::exists(prompt)) std::ofstream(prompt) << cluster::templates::GLOBAL_PROMPT;
    this->_mtime = std::filesystem::last_write_time(this->_path, error);

    toml::table table;
    try {
        table = toml::parse_file(this->_path.string());
    } catch (const toml::parse_error& e) {
        this->_error = this->_path.string() + ":" + std::to_string(e.source().begin.line) + ": " + std::string(e.description());
        return;
    }
    this->_error.clear();
    this->defaults_();

    /* ui */
    this->frontend = table["ui"]["frontend"].value_or(this->frontend);
    this->layout = table["ui"]["layout"].value_or(this->layout);
    this->style = table["ui"]["style"].value_or(this->style);
    this->panels = strings_(table["ui"]["panels"], this->panels);
    this->restore = table["ui"]["restore"].value_or(this->restore);
    this->notify = table["ui"]["notify"].value_or(this->notify);

    /* agent */
    this->backend = table["agent"]["default"].value_or(this->backend);
    this->mode = table["agent"]["permission_mode"].value_or(this->mode);
    this->allowBypass = table["agent"]["allow_bypass"].value_or(this->allowBypass);
    this->extraArgs = strings_(table["agent"]["extra_args"], {});
    if (const toml::table* commandsTable = table["commands"].as_table())
        for (const auto &[name, value]: *commandsTable)
            if (const std::optional<std::string> cmd = value.value<std::string>()) this->commands[std::string(name.str())] = *cmd;

    /* global */
    this->globalEnabled = table["global"]["enabled"].value_or(this->globalEnabled);
    this->globalBackend = table["global"]["backend"].value_or(this->globalBackend);
    this->globalCwd = table["global"]["cwd"].value_or(this->globalCwd);
    this->globalModel = table["global"]["model"].value_or(this->globalModel);
    this->globalMode = table["global"]["permission_mode"].value_or(this->globalMode);
    this->globalPrompt = table["global"]["prompt"].value_or(this->globalPrompt);
    this->remoteName = table["global"]["remote_name"].value_or(this->remoteName);

    /* sessions */
    this->trashDays = table["sessions"]["trash_days"].value_or(this->trashDays);
    this->maxParallel = table["sessions"]["max_parallel"].value_or(this->maxParallel);
    this->budget = table["sessions"]["budget_usd"].value_or(this->budget);

    /* voice */
    toml::node_view<const toml::node> v = std::as_const(table)["voice"];
    this->voice.enabled = v["enabled"].value_or(this->voice.enabled);
    this->voice.mode = v["mode"].value_or(this->voice.mode);
    this->voice.wakeWord = v["wake_word"].value_or(this->voice.wakeWord);
    this->voice.wakeSeconds = v["wake_seconds"].value_or(this->voice.wakeSeconds);
    this->voice.wakeReply = v["wake_reply"].value_or(this->voice.wakeReply);
    this->voice.onlyMe = v["only_me"].value_or(this->voice.onlyMe);
    this->voice.confirmSeconds = v["confirm_seconds"].value_or(this->voice.confirmSeconds);
    this->voice.speak = v["speak"].value_or(this->voice.speak);
    this->voice.summaryChars = v["summary_chars"].value_or(this->voice.summaryChars);
    this->voice.listenCommand = v["listen_command"].value_or(this->voice.listenCommand);
    this->voice.sayCommand = v["say_command"].value_or(this->voice.sayCommand);
    this->voice.voiceFr = v["voice_fr"].value_or(this->voice.voiceFr);
    this->voice.voiceEn = v["voice_en"].value_or(this->voice.voiceEn);

    /* keys */
    if (const toml::table* keysTable = table["keys"].as_table())
        for (const auto &[action, key]: *keysTable)
            if (const std::optional<std::string> value = key.value<std::string>()) this->keys[std::string(action.str())] = *value;

    /* styles (own ones: missing keys taken from html-dark) */
    if (const toml::table* stylesTable = table["styles"].as_table()) {
        for (const auto &[name, node]: *stylesTable) {
            const toml::table* s = node.as_table();
            if (!s) continue;
            cluster::Style custom = this->styles.at("html-dark");
            custom.name = std::string(name.str());
            custom.dark = (*s)["dark"].value_or(custom.dark);
            custom.bg = (*s)["bg"].value_or(custom.bg);
            custom.surface = (*s)["surface"].value_or(custom.surface);
            custom.text = (*s)["text"].value_or(custom.text);
            custom.muted = (*s)["muted"].value_or(custom.muted);
            custom.accent = (*s)["accent"].value_or(custom.accent);
            custom.border = (*s)["border"].value_or(custom.border);
            custom.ok = (*s)["ok"].value_or(custom.ok);
            custom.warn = (*s)["warn"].value_or(custom.warn);
            custom.error = (*s)["error"].value_or(custom.error);
            this->styles[custom.name] = custom;
        }
    }

    /* profiles */
    this->profiles.clear();
    if (const toml::table* profilesTable = table["profiles"].as_table()) {
        for (const auto &[name, node]: *profilesTable) {
            const toml::table* p = node.as_table();
            if (!p) continue;
            cluster::Profile profile;
            profile.name = std::string(name.str());
            profile.cwd = (*p)["cwd"].value_or(std::string());
            profile.backend = (*p)["backend"].value_or(std::string());
            profile.mode = (*p)["permission_mode"].value_or(std::string());
            profile.prompt = (*p)["prompt"].value_or(std::string());
            profile.extraArgs = strings_(std::as_const(*p)["extra_args"], {});
            this->profiles[profile.name] = profile;
        }
    }

    /* providers */
    this->providers.clear();
    if (const toml::table* providersTable = table["providers"].as_table()) {
        for (const auto &[name, node]: *providersTable) {
            const toml::table* p = node.as_table();
            if (!p) continue;
            cluster::ProviderConf provider;
            provider.name = std::string(name.str());
            provider.driver = (*p)["driver"].value_or(std::string("claude"));
            provider.baseUrl = (*p)["base_url"].value_or(std::string());
            provider.model = (*p)["model"].value_or(std::string());
            provider.key = (*p)["key"].value_or(false);
            this->providers[provider.name] = provider;
        }
    }
}

_cold bool cluster::Config::reload(void)
{
    std::error_code error;
    const std::filesystem::file_time_type mtime = std::filesystem::last_write_time(this->_path, error);
    if (error || mtime == this->_mtime) return false;
    this->load();
    return true;
}

_cold const cluster::Style& cluster::Config::currentStyle(void) const
{
    auto it = this->styles.find(this->style);
    return it != this->styles.end() ? it->second : this->styles.at("html-dark");
}

_cold std::string cluster::Config::command(const std::string& driver) const
{
    auto it = this->commands.find(driver);
    return it != this->commands.end() ? it->second : driver;
}
