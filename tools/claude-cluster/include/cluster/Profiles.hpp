/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 25/10/2026 by @author Tsukini

File Name:
##  @file Profiles.hpp

File Description:
##  Settings profiles: named copies of config.toml saved, applied, renamed, archived, deleted to a trash kept 7 days
\**************************************************************/

#ifndef PROFILES_H
    #define PROFILES_H

//----------------------------------------------------------------//
/* INCLUDE */

    #define _Attribute
    #include <utils/utils.hpp> // _cold, _nodiscard
    #include "Types.hpp"       // cluster::Json
    #include <cstdint>         // std::int64_t
    #include <string>          // std::string
    #include <vector>          // std::vector

//----------------------------------------------------------------//
/* DEFINE */

    #define PROFILES_TRASH_DAYS 7 // a deleted profile stays this long in the trash (restorable), then goes for good

namespace cluster { // namespace start

//----------------------------------------------------------------//
/* STRUCT */

// A profile of settings: its name (any text), its note, its file (<config>/profiles/<file>.toml), its dates; archived: kept apart
// from the list (0: not), deleted: in the trash since then (0: not)
struct SettingsProfile {
    std::string name;
    std::string note;
    std::string file;
    std::int64_t created = 0;
    std::int64_t updated = 0;
    std::int64_t archived = 0;
    std::int64_t deleted = 0;
    std::size_t lines = 0;          // the settings it holds (key = value lines)
};

//----------------------------------------------------------------//
/* FUNCTION */

// Every profile (the trash too: deleted > 0), the oldest of the trash removed first; the settings before the last apply (undo)
_cold _nodiscard std::vector<cluster::SettingsProfile> profiles_list(void);
_cold _nodiscard cluster::Json profiles_json(void);
// save <name> (new, or over it) | apply <name> (the settings before it kept: undo) | undo | rename <name> <arg> | note <name> <arg> |
// archive / unarchive <name> | delete <name> (to the trash) | restore <name> | purge <name> (for good); its result told
_cold std::string profile_act(const std::string& action, const std::string& name, const std::string& arg);
_cold void profiles_purge(void); // the trash older than PROFILES_TRASH_DAYS removed

} // namespace end
#endif /* PROFILES_H */
