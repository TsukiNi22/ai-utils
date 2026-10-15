/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Services.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef SERVICES_H
    #define SERVICES_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    #define _Attribute
    #include <utils/utils.hpp> // _cold, _nodiscard
    #include "Types.hpp"       // cluster::Json
    #include <string>          // std::string
    #include <vector>          // std::vector

namespace cluster { // namespace start
class Manager;

//----------------------------------------------------------------//
/* STRUCT */

// A line of the card of a service: its text, its kind (text, muted, log, heard, error, ok, warn: its color)
struct ServiceLine {
    std::string text;
    std::string kind;
};

//----------------------------------------------------------------//
/* FUNCTION */

// The sub services of claude-cluster (the Services tab of the settings): service (systemd), web, core, voice, mcp,
// outside - their state (the slow parts: systemd, its journal, tailscale, kept [services] delay seconds); an action on
// one (its progress popup, its failure popup; an interface of the core: run by the core)
_cold _nodiscard cluster::Json services(cluster::Manager& manager);
_cold std::string service_act(cluster::Manager& manager, const std::string& service, const std::string& action);
_cold _nodiscard std::vector<int> interface_pids(void); // the terminals / windows on this core (Core.cpp)
// its card, from services(): its state (on the right of its name), its lines, its buttons <label, action>; the actions
// confirmed first (the core, a removal)
_cold _nodiscard std::string service_summary(const std::string& name, const cluster::Json& all);
_cold _nodiscard std::vector<cluster::ServiceLine> service_lines(const std::string& name, const cluster::Json& all);
_cold _nodiscard std::vector<std::pair<std::string, std::string>> service_buttons(const std::string& name, const cluster::Json& all);
_cold _nodiscard inline bool service_risky(const std::string& name, const std::string& action) {return name == "core" || (name == "service" && action == "remove");};

} // namespace end
#endif /* SERVICES_H */
