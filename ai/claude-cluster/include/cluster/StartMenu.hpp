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
##  @file StartMenu.hpp

File Description:
##  The menu of the start of a terminal (always shown): open the running claude-cluster, an independent instance (its own sessions,
##  its own core), restore the previous run, start new, quit
\**************************************************************/

#ifndef STARTMENU_H
    #define STARTMENU_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* type */
    #include <string>                       // std::string

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct StartChoice {
    enum class Kind {Open, Restore, New, Independent, Quit};
    Kind kind = Kind::Open;
    std::string instance;       // Independent: its name (a new one when empty, else the one opened)
};

//----------------------------------------------------------------//
/* FUNCTION */

// The menu in the terminal, over what is there (the core running, its sessions, the independent instances, the previous run to restore); the choice
_cold _nodiscard cluster::StartChoice start_menu(const std::string& version);
// The environment of an independent instance: its data folder under the instances of this install, its socket, no web access
_cold void start_instance(const std::string& name);

} // namespace end
#endif /* STARTMENU_H */
