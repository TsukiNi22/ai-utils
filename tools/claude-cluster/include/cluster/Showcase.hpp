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
##  @file Showcase.hpp

File Description:
##  The Debug page: everything claude-cluster can show, shown for real on the interface asking
\**************************************************************/

#ifndef SHOWCASE_H
    #define SHOWCASE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* type */
    #include <string>                       // std::string
    #include <utility>                      // std::pair
    #include <vector>                       // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* DEFINE */

class Manager;

//----------------------------------------------------------------//
/* FUNCTION */

// The sounds of the notifications (the soundboard of the Debug page): the name of each one in the desktop theme, when it is heard
_nodiscard const std::vector<std::pair<std::string, std::string>>& notify_sounds(void);

// The showcase of the Debug page: what = all | displays | questions | permission | states | progress | failure | notify;
// platform = terminal | window | web | app | web terminal (what it says about each element); run by the core, in its
// showcase session (no agent turn: nothing spent); its message
_cold _nodiscard std::string showcase(cluster::Manager& manager, const std::string& what, const std::string& platform);
_cold _nodiscard inline bool showcase_request(const std::string& requestId) {return requestId.starts_with("showcase-") || requestId.starts_with("core-");}; // answered here, never sent to an agent

} // namespace end
#endif /* SHOWCASE_H */
