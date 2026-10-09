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
//----------------------------------------------------------------//
/* ENUM */

enum class SettingKind {
    Bool,
    Int,
    Float,
    Text,
    Choice,
    List,       // comma separated in the page, a TOML array in the file
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
};

struct SettingsPage {
    std::string section;
    std::string title;
    std::vector<cluster::Setting> settings;
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard std::vector<cluster::SettingsPage> settings_pages(const cluster::Config& config);
_cold _nodiscard std::string setting_next(const cluster::Setting& setting, const std::string& value, const int step); // Bool / Choice / Int / Float
_cold _nodiscard std::string setting_check(const cluster::Setting& setting, const std::string& value); // error message, empty: valid
_cold void settings_write(const cluster::Config& config, const std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>>& changes);

} // namespace end

#endif /* CLUSTER_SETTINGS_H */
