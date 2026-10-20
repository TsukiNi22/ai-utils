/**************************************************************\
Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Options.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef OPTIONS_H
    #define OPTIONS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    #define _Exception
    #define _Attribute
    #include <utils/utils.hpp>   // _cold, _nodiscard, utils::exception
    #include "Types.hpp"         // cluster::Json
    #include "Tools.hpp"         // cluster::trim, cluster::lower
    #include <filesystem>        // std::filesystem
    #include <sstream>           // std::stringstream
    #include <string>            // std::string
    #include <vector>            // std::vector

namespace cluster::opt { // namespace start
//----------------------------------------------------------------//
/* FUNCTION */

// The options of a task / a schedule given as JSON (a tool of MCP, an interface): a model may write a number or a boolean as a string
_cold _nodiscard inline std::string text(const cluster::Json& o, const char* key)
{
    const auto it = o.find(key);
    if (it == o.end() || it->is_null()) return "";
    return it->is_string() ? it->get<std::string>() : it->dump();
}

_cold _nodiscard inline std::int64_t number(const cluster::Json& o, const char* key, const std::int64_t fallback)
{
    const auto it = o.find(key);
    if (it == o.end() || it->is_null()) return fallback;
    if (it->is_number()) return it->get<std::int64_t>();
    if (it->is_boolean()) return it->get<bool>() ? 1 : 0;
    try {
        return std::stoll(it->get<std::string>());
    } catch (const std::exception&) {
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, std::string(key) + ": a number, not " + it->dump());
    }
}

_cold _nodiscard inline double decimal(const cluster::Json& o, const char* key)
{
    const auto it = o.find(key);
    if (it == o.end() || it->is_null()) return 0.0;
    if (it->is_number()) return it->get<double>();
    try {
        return std::stod(it->get<std::string>());
    } catch (const std::exception&) {
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, std::string(key) + ": a number, not " + it->dump());
    }
}

_cold _nodiscard inline bool flag(const cluster::Json& o, const char* key, const bool fallback)
{
    const auto it = o.find(key);
    if (it == o.end() || it->is_null()) return fallback;
    if (it->is_boolean()) return it->get<bool>();
    const std::string word = cluster::lower(it->is_string() ? it->get<std::string>() : it->dump());
    return word == "true" || word == "1" || word == "yes" || word == "on";
}

_cold _nodiscard inline std::vector<std::string> list(const cluster::Json& o, const char* key)
{
    std::vector<std::string> out;
    const auto it = o.find(key);
    if (it == o.end() || it->is_null()) return out;
    if (it->is_array()) {
        for (const cluster::Json& item: *it)
            if (item.is_string() && !cluster::trim(item.get<std::string>()).empty()) out.push_back(cluster::trim(item.get<std::string>()));
    } else if (it->is_string()) {
        std::stringstream stream(it->get<std::string>());
        std::string word;
        while (std::getline(stream, word, ','))
            if (!cluster::trim(word).empty()) out.push_back(cluster::trim(word));
    }
    return out;
}

_cold _nodiscard inline std::string cap(const std::string& text, const std::size_t size)
{
    return text.size() > size ? text.substr(0, size) + "\n[... " + std::to_string(text.size() - size) + " characters more]" : text;
}

_cold _nodiscard inline bool is_path(const std::string& expect) {return expect.find('/') != std::string::npos || expect.starts_with("~");}; // an output asked in a file

_cold _nodiscard inline std::int64_t mtime(const std::string& path)
{
    std::error_code error;
    const std::filesystem::file_time_type time = std::filesystem::last_write_time(cluster::expand_home(path), error);
    return error ? 0 : static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count());
}

} // namespace end
#endif /* OPTIONS_H */
