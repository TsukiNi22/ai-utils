/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Actions.hpp

File Description:
##  Actions of the command palette and of the shortcuts, shared by
##  the terminal and the window front-ends
\**************************************************************/

#ifndef CLUSTER_ACTIONS_H
    #define CLUSTER_ACTIONS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Manager.hpp"          // cluster::Manager
    #include "Editor.hpp"           // cluster::CompletionProvider
    #include "Voice.hpp"            // cluster::Voice
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Action {
    std::string id;         // "new_session", "style:nord", "mode:plan"...
    std::string title;
    std::string input;      // label of the text asked before running it (empty: none)
    std::string fill;       // default value of the input
    bool ui = false;        // handled by the front-end (layout, style, focus, quit...)
};

struct ActionResult {
    std::string message;    // shown to the user
    bool error = false;
    std::string focus;      // session to show after the action
    std::string ui;         // front-end action to run after (settings, restore_menu)
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard std::vector<cluster::Action> actions(cluster::Manager& manager, const std::string& session);
_cold _nodiscard cluster::ActionResult run_action(cluster::Manager& manager, cluster::Voice& voice, const std::string& id,
    const std::string& session, const std::string& input);
_cold _nodiscard std::string key_action(const cluster::Config& config, const std::string& key);   // "Ctrl+K" -> "palette"
_cold _nodiscard cluster::CompletionProvider completion_provider(cluster::Manager& manager, std::function<std::string(void)> session);
_cold _nodiscard std::optional<cluster::ActionResult> run_slash(cluster::Manager& manager, const std::string& session, const std::string& line);
_cold _nodiscard const std::vector<std::pair<std::string, std::string>>& slash_commands(void);  // local /commands <usage, description>
_cold _nodiscard std::string next_mode(const std::string& mode);    // Shift+Tab: auto, acceptEdits, plan, bypassPermissions, manual
_cold _nodiscard std::string help_text(const cluster::Config& config);   // F1: every key

} // namespace end

#endif /* CLUSTER_ACTIONS_H */
