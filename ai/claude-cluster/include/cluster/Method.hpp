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
##  @file Method.hpp

File Description:
##  The work method of the user (task list then review, one task at a time, plan first...): its presets, the short text
##  injected in the system prompts of the sub-sessions and of the global session
\**************************************************************/

#ifndef METHOD_H
    #define METHOD_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* project */
    #include "Config.hpp"                   // cluster::Config

    /* type */
    #include <string>                       // std::string
    #include <vector>                       // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* DEFINE */

struct MethodPreset {
    std::string id;                     // free | list-review | one-task | plan-first | custom
    std::string label;
    std::string sub;                    // what the sub-sessions are told
    std::string global;                 // how the global session splits the requests
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard const std::vector<cluster::MethodPreset>& method_presets(void);
_cold _nodiscard const cluster::MethodPreset* method_preset(const std::string& id);     // null: unknown

// The text injected in a system prompt (global: for the global session, else for the sub-sessions): the preset, then the free
// text of the user; "" for free without text
_cold _nodiscard std::string method_text(const cluster::Config& config, const bool global);

} // namespace end
#endif /* METHOD_H */
