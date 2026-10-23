/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 22/10/2026 by @author Tsukini

File Name:
##  @file Demo.hpp

File Description:
##  The demo mode (claude-cluster --demo): an instance of its own (its config, data and socket in a cache folder, wiped at each start)
##  filled with fictional projects, sessions, categories, tags, tasks, schedules and usage; the agents are a fake one that only
##  pretends (thinking, tools, todo lists, a permission) - nothing real is touched, nothing is spent
\**************************************************************/

#ifndef DEMO_H
    #define DEMO_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* type */
    #include <string>                       // std::string

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* DEFINE */

class Manager;

//----------------------------------------------------------------//
/* FUNCTION */

// CLAUDE_CLUSTER_DEMO is set: this process (and the core, the agents it starts) is the demo
_cold _nodiscard bool demo_enabled(void);
// Before anything is read: the folder of the demo made again (config, data, projects, the fake agent), the environment pointing at it; web: its web access allowed (read only)
_cold void demo_prepare(const bool web);
// Its core stopped: the folder of the demo removed (nothing kept)
_cold void demo_cleanup(const bool core);
// In the core: the fictional sessions, categories, tags, tasks and schedules, put in a thread of its own (the sessions start one after the other)
_cold void demo_seed(cluster::Manager& manager);
// claude-cluster demo-agent: the fake agent (stream-json on stdin / stdout, like claude) the demo sessions run; its exit code
_cold _nodiscard int demo_agent(void);
// the commands of the web access that stay in the demo (it only reads)
_cold _nodiscard bool demo_web_allows(const std::string& cmd);

} // namespace end
#endif /* DEMO_H */
