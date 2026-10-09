/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Tools.hpp

File Description:
##  Small helpers: paths, time, formatting, sub-processes, json
##  files, language detection
\**************************************************************/

#ifndef CLUSTER_TOOLS_H
    #define CLUSTER_TOOLS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Json
    #include <filesystem>           // std::filesystem::path
    #include <optional>             // std::optional
    #include <cstdint>              // std::int64_t
    #include <chrono>               // std::chrono
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Captured {
    int code = -1;
    std::string out;
};

//----------------------------------------------------------------//
/* FUNCTION */

/* paths */
_cold _nodiscard std::filesystem::path expand_home(const std::string& path);
_cold _nodiscard std::filesystem::path config_dir(void);     // ~/.config/claude-cluster
_cold _nodiscard std::filesystem::path data_dir(void);       // ~/.local/share/claude-cluster
_cold _nodiscard std::filesystem::path runtime_dir(void);    // $XDG_RUNTIME_DIR/claude-cluster (or /tmp)
_cold _nodiscard std::filesystem::path socket_path(void);
_cold _nodiscard std::filesystem::path self_exe(void);
_cold _nodiscard std::string short_path(const std::string& path); // ~ for the home

/* text */
_nodiscard std::vector<std::string> split(const std::string& text, const char sep);
_nodiscard std::string trim(const std::string& text);
_nodiscard std::string lower(std::string text);
_nodiscard std::string one_line(const std::string& text, const std::size_t max);
_nodiscard std::string language_of(const std::string& text);      // "fr" | "en"
_nodiscard std::string summary_of(const std::string& text, const std::size_t max);

/* format */
_nodiscard inline std::int64_t now(void) {return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();};
_nodiscard std::string human_tokens(const std::int64_t tokens);   // 12.3k, 1.2M
_nodiscard std::string human_cost(const double usd);              // $0.0123
_nodiscard std::string human_age(const std::int64_t time);        // 3m, 2h, 5d
_nodiscard std::string clock_time(const std::int64_t time);       // 14:03

/* processes */
_cold _nodiscard bool has_command(const std::string& name);
_cold _nodiscard cluster::Captured capture(const std::vector<std::string>& args, const std::string& cwd = "", const int timeout = 10);
_cold void detach(const std::vector<std::string>& args);            // run without waiting (notify-send...)

/* json files */
_cold _nodiscard std::optional<cluster::Json> read_json(const std::filesystem::path& path);
_cold void write_json(const std::filesystem::path& path, const cluster::Json& data); // atomic (tmp + rename)

} // namespace end

#endif /* CLUSTER_TOOLS_H */
