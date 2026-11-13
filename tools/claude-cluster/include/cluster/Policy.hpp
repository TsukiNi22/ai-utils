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
##  @file Policy.hpp

File Description:
##  The model / thinking policy: what the global session may choose for the sub-sessions (a default, advised or forced, the
##  allowed ones) and what it must ask the user first (the costly models and efforts)
\**************************************************************/

#ifndef POLICY_H
    #define POLICY_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* project */
    #include "Config.hpp"                   // cluster::Config
    #include "Types.hpp"                    // cluster::Json

    /* type */
    #include <string>                       // std::string

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* DEFINE */

struct PolicyVerdict {                  // what the policy says of a change of model / thinking asked by the global session
    std::string kind = "allow";         // allow | refuse (the reason tells why) | ask (the user decides: `what` is the costly part)
    std::string reason;
    std::string what;                   // ask: "opus, thinking xhigh"
};

//----------------------------------------------------------------//
/* FUNCTION */

// A model matches an entry of a list by its name or a part of it (opus: claude-opus-5-5, opus[1m]), case ignored
_cold _nodiscard bool policy_matches(const std::string& model, const std::string& entry);
_cold _nodiscard bool policy_costly_model(const cluster::Config& config, const std::string& model);
_cold _nodiscard bool policy_costly_effort(const cluster::Config& config, const std::string& effort);

// model / effort: the ones asked (empty: unchanged); current: the ones the session has now (a change only is judged)
_cold _nodiscard cluster::PolicyVerdict policy_check(const cluster::Config& config, const std::string& model, const std::string& effort, const std::string& currentModel,
    const std::string& currentEffort);

// The policy as data (the tool models_policy of the global session) and as text (its prompt)
_cold _nodiscard cluster::Json policy_json(const cluster::Config& config);

} // namespace end
#endif /* POLICY_H */
