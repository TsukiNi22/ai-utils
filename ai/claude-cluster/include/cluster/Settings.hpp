/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Settings.hpp

File Description:
##  Settings page (BIOS-like) of claude-cluster: every option of
##  config.toml with its kind, and the writing of the changes in
##  the file (its comments kept)
\**************************************************************/

#ifndef CLUSTER_SETTINGS_H
    #define CLUSTER_SETTINGS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Config.hpp"           // cluster::Config
    #include <functional>           // std::function
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <map>                  // std::map

namespace cluster { // namespace start
class Manager;
//----------------------------------------------------------------//
/* ENUM */

enum class SettingKind {
    Bool,
    Int,
    Float,
    Text,
    Choice,
    List,       // comma separated in the page, a TOML array in the file
    Action,     // not an option: Enter runs it (its key names it), its value is a status
};

//----------------------------------------------------------------//
/* STRUCT */

struct Setting {
    std::string section;                // page / TOML table ("ui", "agent"...)
    std::string key;
    std::string label;
    cluster::SettingKind kind = cluster::SettingKind::Text;
    std::vector<std::string> choices;   // Choice
    std::string description;
    bool restart = false;               // applied at the next start only
    std::function<std::string(const cluster::Config&)> value;   // current value, as shown
    std::function<std::string(const std::string&)> choiceHelp{}; // Choice: the help of a value (shown with the description)
    std::function<std::string(const std::string&)> danger{};     // a value to confirm first: its warning (empty: safe)
};

struct SettingsPage {
    std::string section;
    std::string title;
    std::vector<cluster::Setting> settings;
};

struct Requirement {
    std::string problem;            // what this value needs and is missing, empty: fine
    std::string package;            // the package installing it, empty: done by hand (told in problem)
};

struct SettingHit {
    std::size_t page = 0;       // in the pages searched
    std::size_t item = 0;       // in its page
    float score = 0.0F;         // lower: closer
    bool name = true;           // matched by its name (its label, its key); false: by its description only
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard std::vector<cluster::SettingsPage> settings_pages(const cluster::Config& config);
_cold _nodiscard std::string setting_next(const cluster::Setting& setting, const std::string& value, const int step); // Bool / Choice / Int / Float
_cold _nodiscard std::string setting_check(const cluster::Setting& setting, const std::string& value); // error message, empty: valid
_cold bool settings_set(const cluster::Config& config, const std::string& section, const std::string& key, const std::string& value);
// the settings matching a text: their name (c2dmp-hsm, [ui] matching) far before their description, the closest first
_cold _nodiscard std::vector<cluster::SettingHit> settings_search(const std::vector<cluster::SettingsPage>& pages, const std::string& query);
_cold _nodiscard inline std::string setting_danger(const cluster::Setting& setting, const std::string& value) {return setting.danger ? setting.danger(value) : std::string();}; // its warning, empty: safe
// Requirements.cpp: what a setting needs with this value, its package; installed by the password window of the desktop
_cold _nodiscard cluster::Requirement setting_requirement(cluster::Manager& manager, const cluster::Setting& setting, const std::string& value);
_cold _nodiscard std::vector<std::string> install_command(const std::string& package);   // dnf / apt-get / pacman / zypper, empty: none
_cold _nodiscard std::string install_package(const std::string& package, cluster::Manager* manager = nullptr); // pkexec (its popup): its error, empty: ok
// the settings only the user changes (a shell, the web open to the Internet, a program run as the user...): never by the
// global session (MCP: settings_set, config_write) - it asks the user to change them in the settings page
_cold _nodiscard bool setting_protected(const std::string& section, const std::string& key);
_cold _nodiscard std::vector<std::string> protected_changes(const std::string& oldToml, const std::string& newToml); // "section.key" changed
_cold void settings_write(const cluster::Config& config, const std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>>& changes);

} // namespace end

#endif /* CLUSTER_SETTINGS_H */
