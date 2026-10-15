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

// The model and the effort of a session, and the choices of its backend
struct ModelInfo {
    std::string driver;
    std::string model;      // the one asked (empty: the default of the provider), else the one answering
    std::string effort;     // empty / default: the one of the agent
    std::vector<std::pair<std::string, std::string>> models;    // <model, description>
    std::vector<std::string> efforts;                           // empty: the driver has none
};

struct ActionResult {
    std::string message;    // shown to the user
    bool error = false;
    std::string focus;      // session to show after the action
    std::string ui;         // front-end action to run after (settings, restore_menu, fill: fill in the prompt, quit, search:<text>)
    std::string fill{};     // ui fill: the text put in the prompt (a :s of the last prompt, a :r)
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard std::vector<cluster::Action> actions(cluster::Manager& manager, const std::string& session);
_cold _nodiscard std::optional<float> action_score(const cluster::Action& action, const std::string& filter); // lower: closer ([ui] matching)
_cold _nodiscard std::vector<cluster::Action> filter_actions(const std::vector<cluster::Action>& actions, const std::string& filter,
    cluster::Manager* manager = nullptr); // ranked; a manager: new: / menu:<id> / delete:<id> made, the settings found too
_cold _nodiscard bool action_matches(const cluster::Action& action, const std::string& filter); // the palette filter ("apply:every": its prefix, then words)
_cold _nodiscard cluster::ActionResult run_action(cluster::Manager& manager, cluster::Voice& voice, const std::string& id,
    const std::string& session, const std::string& input);
_cold _nodiscard std::string key_action(const cluster::Config& config, const std::string& key);   // "Ctrl+K" -> "palette"
_cold _nodiscard cluster::CompletionProvider completion_provider(cluster::Manager& manager, std::function<std::string(void)> session);
_cold _nodiscard std::optional<cluster::ActionResult> run_slash(cluster::Manager& manager, const std::string& session, const std::string& line);
_cold _nodiscard const std::vector<std::pair<std::string, std::string>>& slash_commands(void);  // local /commands <usage, description>
// the modes of the prompt (Prompt.cpp): !<command> in the shell of the session, :<command> like vim
_cold _nodiscard std::optional<cluster::ActionResult> run_prompt_mode(cluster::Manager& manager, const std::string& session, const std::string& line);
_cold _nodiscard cluster::ActionResult run_shell(cluster::Manager& manager, const std::string& session, const std::string& command);
_cold _nodiscard const std::vector<std::pair<std::string, std::string>>& ex_commands(void);   // <:command, description>
_cold _nodiscard std::vector<cluster::Candidate> shell_commands(const std::string& prefix);    // the commands of the user's shell
_cold _nodiscard std::string next_mode(const std::string& mode);    // Shift+Tab: auto, acceptEdits, plan, bypassPermissions, manual
_cold _nodiscard std::string help_text(const cluster::Config& config);   // F1: every key
_cold _nodiscard cluster::ModelInfo model_info(cluster::Manager& manager, const std::string& session); // throws: no session

} // namespace end

#endif /* CLUSTER_ACTIONS_H */
