/**************************************************************\
Edition:
##  @date 11/10/2026 by @author Tsukini

File Name:
##  @file Models.hpp

File Description:
##  Models and thinking / reasoning effort of the backends: the
##  models offered per provider (Claude, Ollama, opencode, config),
##  the effort levels per driver and their arguments
\**************************************************************/

#ifndef CLUSTER_MODELS_H
    #define CLUSTER_MODELS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Config.hpp"           // cluster::Config
    #include "Auth.hpp"             // cluster::Provider
    #include <utility>              // std::pair
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* FUNCTION */

// the models offered for a provider <model, description>: Claude aliases and ids, `ollama list`, `opencode models`, its
// default model, the [models] of the config (kept 60 s: the palette asks it at every key)
_cold _nodiscard std::vector<std::pair<std::string, std::string>> model_choices(const cluster::Config& config, const cluster::Provider& provider);

// the thinking / reasoning effort levels of a driver (empty: none); "default" or empty: the one of the agent
_cold _nodiscard const std::vector<std::string>& effort_levels(const std::string& driver);

// the arguments of an effort for a driver (empty: none / not supported)
_cold _nodiscard std::vector<std::string> effort_args(const std::string& driver, const std::string& effort);

} // namespace end

#endif /* CLUSTER_MODELS_H */
