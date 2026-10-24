/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Origin.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef ORIGIN_H
    #define ORIGIN_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    #define _Attribute
    #include <utils/utils.hpp> // _cold, _nodiscard
    #include <string>          // std::string
    #include <mutex>           // std::mutex

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// The colours of the prompts of a transcript by their origin ([ui] prompt_origin, prompt_color_user / _mcp / _task):
// empty = the default of the theme (user: the accent, as before)
struct OriginColors {
    bool enabled = true;
    std::string user;
    std::string mcp;
    std::string task;
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold void set_origin_colors(const cluster::OriginColors& colors);
_cold _nodiscard cluster::OriginColors origin_colors(void);

// "user" | "mcp" | "task" | "schedule" | "wake" of a prompt: the origin stored when it entered the session, else (old transcripts) the
// markers of its text
_cold _nodiscard std::string prompt_origin(const std::string& origin, const std::string& text);

// the colour group of an origin ("user" | "mcp" | "task": a schedule and a wake-up are tasks) and its label in the header ("" for the user)
_cold _nodiscard inline std::string origin_group(const std::string& origin) {return origin == "schedule" || origin == "wake" ? "task" : origin;};
_cold _nodiscard inline std::string origin_label(const std::string& origin) {return origin == "mcp" ? "mcp" : origin == "task" ? "task" : origin == "schedule" ? "scheduled"
    : origin == "wake" ? "woken" : "";};

// the colour "#rrggbb" of an origin on a light / dark theme (the config, else the default; user: "" = the accent of the theme)
// defaults: mcp violet, task magenta, 4.5:1 or more on the backgrounds of the built-in themes, apart from the blue accent,
// the green / yellow / red states and the grey of the muted text
_cold _nodiscard std::string origin_color(const std::string& origin, const bool dark);

} // namespace end

#endif /* ORIGIN_H */
