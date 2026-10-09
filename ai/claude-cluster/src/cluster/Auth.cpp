/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Auth.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Encapsulation
#include <utils/utils.hpp>
#include "cluster/Tools.hpp"
#include "cluster/Auth.hpp"
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <fstream>
#include <regex>

/* tools */
_cold static std::vector<cluster::Provider> builtin_providers_(void)
{
    using cluster::AuthKind;
    return {
        {"anthropic", "claude", AuthKind::Login, "", "", "Claude Code with its own login (claude auth login), default", false},
        {"anthropic-api", "claude", AuthKind::Key, "", "", "Claude Code with an Anthropic API key", false},
        {"ollama", "claude", AuthKind::None, "http://localhost:11434", "", "Claude Code on a local Ollama model (Anthropic-compatible API)", true},
        {"openrouter", "claude", AuthKind::Key, "https://openrouter.ai/api", "", "Claude Code through OpenRouter (any model)", false},
        {"deepseek", "claude", AuthKind::Key, "https://api.deepseek.com/anthropic", "deepseek-chat", "Claude Code on DeepSeek", false},
        {"kimi", "claude", AuthKind::Key, "https://api.moonshot.ai/anthropic", "kimi-k2-turbo-preview", "Claude Code on Moonshot Kimi", false},
        {"zai", "claude", AuthKind::Key, "https://api.z.ai/api/anthropic", "glm-4.6", "Claude Code on Z.ai GLM", false},
        {"minimax", "claude", AuthKind::Key, "https://api.minimax.io/anthropic", "MiniMax-M2", "Claude Code on MiniMax", false},
        {"openai", "qwen", AuthKind::Key, "https://api.openai.com/v1", "gpt-5", "Qwen Code on the OpenAI API", false},
        {"ollama-openai", "qwen", AuthKind::None, "http://localhost:11434/v1", "", "Qwen Code on a local Ollama model (OpenAI API)", true},
        {"opencode", "opencode", AuthKind::Login, "", "opencode/big-pickle", "opencode with its own providers (opencode auth login)", false},
        {"codex", "codex", AuthKind::Login, "", "", "OpenAI Codex CLI (codex login)", false},
    };
}

_cold static std::filesystem::path credentials_file_(void)
{
    return cluster::config_dir() / "credentials.json";
}

_cold static bool keyring_(void)
{
    return cluster::has_command("secret-tool") && std::getenv("DBUS_SESSION_BUS_ADDRESS");
}

_cold static std::string read_hidden_(const std::string& prompt)
{
    // API key typed in the terminal without echo
    termios old{};
    const bool tty = ::isatty(STDIN_FILENO) && ::tcgetattr(STDIN_FILENO, &old) == 0;
    std::string key;

    std::cerr << prompt << std::flush;
    if (tty) {
        termios hidden = old;
        hidden.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        ::tcsetattr(STDIN_FILENO, TCSANOW, &hidden);
    }
    std::getline(std::cin, key);
    if (tty) ::tcsetattr(STDIN_FILENO, TCSANOW, &old);
    std::cerr << std::endl;
    return cluster::trim(key);
}

/* constructor */
_cold cluster::Auth::Auth(const cluster::Config& config)
    : _config(config)
{
}

/* providers */
_cold std::vector<cluster::Provider> cluster::Auth::providers(void) const
{
    std::vector<cluster::Provider> list = builtin_providers_();

    for (const auto &[name, conf]: this->_config.providers) {
        cluster::Provider provider{name, conf.driver, conf.key ? cluster::AuthKind::Key : cluster::AuthKind::None, conf.baseUrl, conf.model,
            "own provider (config.toml)", conf.baseUrl.find("localhost") != std::string::npos || conf.baseUrl.find("127.0.0.1") != std::string::npos};
        std::erase_if(list, [&](const cluster::Provider& p) {return p.name == name;});
        list.push_back(provider);
    }
    return list;
}

_cold std::optional<cluster::Provider> cluster::Auth::find(const std::string& name) const
{
    const std::string wanted = name == "claude" ? "anthropic" : name;
    for (const cluster::Provider& provider: this->providers())
        if (provider.name == wanted) return provider;
    return std::nullopt;
}

_cold cluster::Backend cluster::Auth::resolve(const std::string& backend) const
{
    const std::size_t slash = backend.find('/');
    const std::string name = backend.substr(0, slash);
    const std::optional<cluster::Provider> provider = this->find(name.empty() ? "anthropic" : name);

    if (!provider) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
        "unknown provider '" + name + "' (claude-cluster auth list)");
    return {*provider, slash == std::string::npos ? provider->model : backend.substr(slash + 1)};
}

/* status */
_cold std::pair<bool, std::string> cluster::Auth::status(const cluster::Provider& provider) const
{
    const std::string command = this->_config.command(provider.driver);

    if (!cluster::has_command(command)) return {false, "`" + command + "` is not installed"};
    if (provider.auth == cluster::AuthKind::Key) {
        if (this->secret_(provider.name)) return {true, std::string("API key stored (") + (keyring_() ? "keyring" : "credentials.json") + ")"};
        return {false, "no API key: claude-cluster auth login " + provider.name};
    }
    if (provider.auth == cluster::AuthKind::None) {
        if (provider.name.starts_with("ollama")) {
            const cluster::Captured list = cluster::capture({"ollama", "list"}, "", 5);
            if (list.code != 0) return {false, "Ollama server not reachable (ollama serve)"};
            return {true, std::to_string(std::max<long>(0, std::count(list.out.begin(), list.out.end(), '\n') - 1)) + " local model(s)"};
        }
        return {true, provider.baseUrl};
    }
    if (provider.driver == "claude") {
        const cluster::Captured out = cluster::capture({command, "auth", "status"}, "", 15);
        try {
            const cluster::Json json = cluster::Json::parse(out.out);
            if (json.value("loggedIn", false))
                return {true, json.value("authMethod", std::string("logged in")) + " " + json.value("subscriptionType", std::string())};
        } catch (const cluster::Json::exception&) {}
        return {false, "not logged in: claude-cluster auth login anthropic"};
    }
    if (provider.driver == "codex") {
        const cluster::Captured out = cluster::capture({command, "login", "status"}, "", 15);
        if (out.code == 0) return {true, cluster::one_line(out.out, 60)};
        return {false, "not logged in: claude-cluster auth login codex"};
    }
    if (provider.driver == "opencode") {
        // "N credentials" in a decorated output: the number only
        const cluster::Captured out = cluster::capture({command, "auth", "list"}, "", 15);
        std::smatch match;
        const std::string count = std::regex_search(out.out, match, std::regex(R"((\d+) credentials?)")) ? match[1].str() : "?";
        return {true, count + " credential(s) + its free models (opencode auth login)"};
    }
    return {true, ""};
}

_cold cluster::Env cluster::Auth::env(const cluster::Backend& backend) const
{
    const cluster::Provider& provider = backend.provider;
    const std::string key = provider.auth == cluster::AuthKind::Key ? this->secret_(provider.name).value_or("") : "";
    cluster::Env env;

    if (provider.driver == "claude") {
        if (provider.name == "anthropic") return env;
        if (provider.name == "anthropic-api") return {{"ANTHROPIC_API_KEY", key}};
        env.emplace_back("ANTHROPIC_BASE_URL", provider.baseUrl);
        env.emplace_back("ANTHROPIC_AUTH_TOKEN", key.empty() ? "local" : key);
        env.emplace_back("ANTHROPIC_API_KEY", "");
        if (!backend.model.empty()) {
            // The small / background model of claude must exist on the provider too
            env.emplace_back("ANTHROPIC_DEFAULT_HAIKU_MODEL", backend.model);
            env.emplace_back("ANTHROPIC_SMALL_FAST_MODEL", backend.model);
        }
    } else if (provider.driver == "qwen") {
        env.emplace_back("OPENAI_BASE_URL", provider.baseUrl);
        env.emplace_back("OPENAI_API_KEY", key.empty() ? "local" : key);
        if (!backend.model.empty()) env.emplace_back("OPENAI_MODEL", backend.model);
    }
    return env;
}

/* login */
_cold std::vector<std::string> cluster::Auth::loginCommand(const cluster::Provider& provider) const
{
    const std::string command = this->_config.command(provider.driver);
    if (provider.driver == "claude") return {command, "auth", "login"};
    if (provider.driver == "codex") return {command, "login"};
    if (provider.driver == "opencode") return {command, "auth", "login"};
    return {};
}

_cold std::vector<std::string> cluster::Auth::logoutCommand(const cluster::Provider& provider) const
{
    const std::string command = this->_config.command(provider.driver);
    if (provider.driver == "claude") return {command, "auth", "logout"};
    if (provider.driver == "codex") return {command, "logout"};
    if (provider.driver == "opencode") return {command, "auth", "logout"};
    return {};
}

_cold void cluster::Auth::login(const std::string& name) const
{
    const std::optional<cluster::Provider> provider = this->find(name);
    if (!provider) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown provider '" + name + "'");

    if (provider->auth == cluster::AuthKind::None) {
        std::cout << provider->name << ": nothing to log in (" << this->status(*provider).second << ")" << std::endl;
        if (provider->name.starts_with("ollama")) std::cout << "  models: ollama pull <model> (ex: ollama pull qwen3-coder)" << std::endl;
        return;
    }
    if (provider->auth == cluster::AuthKind::Key) {
        const std::string key = read_hidden_("API key for " + provider->name + " (hidden): ");
        if (key.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "empty key, nothing stored");
        this->storeKey(provider->name, key);
        std::cout << provider->name << ": key stored (" << (keyring_() ? "keyring" : credentials_file_().string()) << ")" << std::endl;
        return;
    }

    // Login of the agent itself, in this terminal
    const std::vector<std::string> args = this->loginCommand(*provider);
    utils::encapsulation::Process process;
    (void)process.spawn(args[0], std::vector<std::string>(args.begin() + 1, args.end()));
    (void)process.wait();
}

_cold void cluster::Auth::logout(const std::string& name) const
{
    const std::optional<cluster::Provider> provider = this->find(name);
    if (!provider) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown provider '" + name + "'");

    if (provider->auth == cluster::AuthKind::Key) {
        this->clearKey(provider->name);
        std::cout << provider->name << ": key removed" << std::endl;
    } else if (provider->auth == cluster::AuthKind::Login) {
        const std::vector<std::string> args = this->logoutCommand(*provider);
        utils::encapsulation::Process process;
        (void)process.spawn(args[0], std::vector<std::string>(args.begin() + 1, args.end()));
        (void)process.wait();
    } else {
        std::cout << provider->name << ": no credential to remove" << std::endl;
    }
}

/* secrets */
_cold std::optional<std::string> cluster::Auth::secret_(const std::string& name) const
{
    if (keyring_()) {
        const cluster::Captured out = cluster::capture({"secret-tool", "lookup", "application", "claude-cluster", "provider", name}, "", 10);
        if (out.code == 0 && !cluster::trim(out.out).empty()) return cluster::trim(out.out);
    }
    const std::optional<cluster::Json> file = cluster::read_json(credentials_file_());
    if (file && file->contains(name) && (*file)[name].is_string()) return (*file)[name].get<std::string>();
    return std::nullopt;
}

_cold void cluster::Auth::storeKey(const std::string& provider, const std::string& key) const
{
    if (keyring_()) {
        // secret-tool reads the secret on its stdin
        // (close-on-exec ends: only the stdin copy of the child stays open, it sees the end of the key)
        utils::encapsulation::Pipe in;
        utils::encapsulation::Process process;
        in.trigger();
        for (const int fd: {in.getRead(), in.getWrite()})
            ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
        process.dup(in.getRead(), STDIN_FILENO);
        (void)process.spawn("secret-tool", {"store", "--label=claude-cluster " + provider, "application", "claude-cluster", "provider", provider});
        in.closeRead();
        (void)::write(in.getWrite(), key.data(), key.size());
        in.closeWrite();
        const utils::encapsulation::Status status = process.wait();
        if (status.exited && status.code == 0) return;
    }
    cluster::Json file = cluster::read_json(credentials_file_()).value_or(cluster::Json::object());
    file[provider] = key;
    cluster::write_json(credentials_file_(), file);
    ::chmod(credentials_file_().c_str(), S_IRUSR | S_IWUSR);
}

_cold void cluster::Auth::clearKey(const std::string& provider) const
{
    if (keyring_()) (void)cluster::capture({"secret-tool", "clear", "application", "claude-cluster", "provider", provider}, "", 10);
    std::optional<cluster::Json> file = cluster::read_json(credentials_file_());
    if (file && file->contains(provider)) {
        file->erase(provider);
        cluster::write_json(credentials_file_(), *file);
    }
}
