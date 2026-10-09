/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Auth.hpp

File Description:
##  Providers of the agents (Anthropic, Ollama, OpenAI-compatible,
##  opencode, codex...), their credentials and environment
\**************************************************************/

#ifndef CLUSTER_AUTH_H
    #define CLUSTER_AUTH_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Config.hpp"           // cluster::Config
    #include <optional>             // std::optional
    #include <utility>              // std::pair
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class AuthKind {
    Login,      // the agent CLI has its own login (claude auth, codex login, opencode auth)
    Key,        // API key stored by claude-cluster (keyring or credentials.json)
    None,       // nothing to set up (local server)
};

//----------------------------------------------------------------//
/* STRUCT */

struct Provider {
    std::string name;
    std::string driver;         // claude | qwen | opencode | codex
    cluster::AuthKind auth = cluster::AuthKind::None;
    std::string baseUrl;
    std::string model;          // default model (empty: the default of the agent)
    std::string description;
    bool local = false;         // no cost
};

struct Backend {
    cluster::Provider provider;
    std::string model;
};

using Env = std::vector<std::pair<std::string, std::string>>;

//----------------------------------------------------------------//
/* CLASS */

class Auth {
    private:
        const cluster::Config& _config;

        // ---------- Pre-Function -------- //
        _cold _nodiscard std::optional<std::string> secret_(const std::string& name) const;

    public:
        // ---------- Pre-Function -------- //
        _cold _nodiscard std::vector<cluster::Provider> providers(void) const;
        _cold _nodiscard std::optional<cluster::Provider> find(const std::string& name) const;
        _cold _nodiscard cluster::Backend resolve(const std::string& backend) const;   // "<provider>[/<model>]", throws
        _cold _nodiscard std::pair<bool, std::string> status(const cluster::Provider& provider) const; // <ready, detail>
        _cold _nodiscard cluster::Env env(const cluster::Backend& backend) const;
        _cold _nodiscard std::vector<std::string> loginCommand(const cluster::Provider& provider) const;  // Login kind
        _cold _nodiscard std::vector<std::string> logoutCommand(const cluster::Provider& provider) const;
        _cold void storeKey(const std::string& provider, const std::string& key) const;
        _cold void clearKey(const std::string& provider) const;
        _cold void login(const std::string& provider) const;      // interactive, in the current terminal
        _cold void logout(const std::string& provider) const;

        // ---------- Constructor -------- //
        _cold Auth(const cluster::Config& config);
        _cold ~Auth() = default;
};

} // namespace end

#endif /* CLUSTER_AUTH_H */
