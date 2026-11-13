/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 17/10/2026 by @author Tsukini

File Name:
##  @file Plugins.hpp

File Description:
##  Plugins and skills from repository links (like claude.ai): listed, then the ones chosen installed for the sessions
\**************************************************************/

#ifndef PLUGINS_H
    #define PLUGINS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* type */
    #include <string>                       // std::string
    #include <vector>                       // std::vector
    #include <map>                          // std::map
    #include <set>                          // std::set
    #include <cstdint>                      // std::int64_t

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* DEFINE */

struct PluginSetup {                    // what the listing and the install need of the config (copied: used by a thread)
    std::vector<std::string> repos;     // [plugins] repos
    std::map<std::string, bool> select; // [plugins.select]: installed
    std::map<std::string, bool> off;    // [plugins.off]: installed but turned off
    std::map<std::string, bool> paused; // [plugins.paused] "<slug>": every item of that source turned off (kept installed)
    std::string claude = "claude";      // the claude command ([commands] claude)
};

struct PluginItem {                     // a plugin of a marketplace, or a skill (a SKILL.md folder)
    std::string id;                     // "<repository>:<name>": its key in [plugins.select]
    std::string name;
    std::string kind;                   // plugin | skill
    std::string description;
    std::string path;                   // a skill: its folder in the clone
    bool installed = false;
    bool enabled = true;                // installed and on (off: a plugin disabled, a skill unlinked)
};

struct PluginRepo {                     // a repository given in [plugins] repos
    std::string url;                    // as given
    std::string slug;                   // its folder / key ("owner-repo", "<folder>-local")
    std::string kind;                   // git | folder | zip
    std::string dir;                    // where it is read (the clone, the folder, the .zip unpacked)
    std::string marketplace;            // its Claude Code marketplace (.claude-plugin/marketplace.json), empty: skills only
    std::string error;                  // cloned / read: what went wrong
    std::vector<cluster::PluginItem> items;
    std::int64_t synced = 0;            // its last pull / unpack that worked (unix time), 0: never
    std::string commit;                 // a repository: its last commit ("<short hash> <subject>"), a .zip / folder: empty
    std::int64_t commitTime = 0;        // the date of that commit, or of the .zip / the newest SKILL.md of a folder
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard std::string plugin_kind(const std::string& source);        // git | folder (a path) | zip (a .zip)
_cold _nodiscard std::string plugin_slug(const std::string& url);           // "owner/repo", https / git URL -> "owner-repo"
_cold _nodiscard std::string plugin_source(const std::string& url);         // "owner/repo" -> its GitHub https URL, the rest as given
_cold _nodiscard std::vector<cluster::PluginRepo> plugins_index(void);      // the last listing (<data>/plugins/index.json), installed or not
_cold _nodiscard std::vector<cluster::PluginRepo> plugins_scan(const cluster::PluginSetup& setup, const std::set<std::string>& pull = {"*"}); // listed;
                                                                            // pull ("*": all): pulled / unpacked again; a new one always cloned
_cold _nodiscard std::string plugins_apply(const cluster::PluginSetup& setup); // the ones ticked installed, the ones unticked removed; what was done
_cold _nodiscard std::map<std::string, std::string> plugin_skill_sources(void); // the imported skills linked now: <skill, its source (slug of the link)>
_cold _nodiscard std::string unprefix_skill(const std::string& text);
_cold _nodiscard bool plugin_auto(const std::map<std::string, bool>& autos, const std::string& slug); // [plugins.auto]: pulled by itself (unless false)       // "/<source>:<skill> x" of an imported skill -> "/<skill> x" (the agent knows the skill only)

} // namespace end
#endif /* PLUGINS_H */
