/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Origin.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Origin.hpp"
#include <cctype>

/* tools */
static std::mutex mutex_;
static cluster::OriginColors colors_;

_cold static bool hex_(const std::string& value)
{
    if (value.size() != 7 || value[0] != '#') return false;
    for (std::size_t i = 1; i < value.size(); ++i)
        if (!std::isxdigit(static_cast<unsigned char>(value[i]))) return false;
    return true;
}

/* colours */
_cold void cluster::set_origin_colors(const cluster::OriginColors& colors)
{
    std::lock_guard<std::mutex> lock(mutex_);
    colors_ = colors;
}

_cold cluster::OriginColors cluster::origin_colors(void)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return colors_;
}

_cold std::string cluster::prompt_origin(const std::string& origin, const std::string& text)
{
    if (origin == "user" || origin == "mcp" || origin == "task" || origin == "schedule" || origin == "wake") return origin;
    for (const char* mark: {"[from the global session]", "[reply from ", "[error from ", "[interrupted: ", "[form \"", "[question from ", "[questions from ",
        "[permission request from ", "[context given by the global session"})
        if (text.starts_with(mark)) return "mcp";
    if (text.starts_with("[scheduled w")) return "schedule";
    if (text.starts_with("[tasks]")) return "task";
    return "user";
}

_cold std::string cluster::origin_color(const std::string& origin, const bool dark)
{
    const cluster::OriginColors colors = cluster::origin_colors();
    const std::string group = cluster::origin_group(origin);
    const std::string& chosen = group == "mcp" ? colors.mcp : group == "task" ? colors.task : colors.user;
    if (hex_(chosen)) return chosen;
    if (group == "mcp") return dark ? "#bb8cf2" : "#8a3fb5";
    if (group == "task") return dark ? "#f08ccb" : "#b3267a";
    return "";
}
