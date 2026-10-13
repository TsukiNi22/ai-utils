/**************************************************************\
Edition:
##  @date 14/10/2026 by @author Tsukini

File Name:
##  @file Icons.hpp

File Description:
##  The icon of claude-cluster: a stack of terminals whose front one
##  has a face made of prompt characters (^_^), one face per state
##  (the notifications, the window, the dock badge, the terminal
##  title, the tab of the web page)
\**************************************************************/

#ifndef ICONS_H
    #define ICONS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include <filesystem>           // std::filesystem::path
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class Face {
    Happy,      // ^_^ the icon of the app; the global session answered; a browser connected
    Grin,       // >‿< a session finished; the task queue done
    Attentive,  // •_• a question, a permission, a reminder; the voice listening
    Waiting,    // o_o an error, a crash, the safety check, a limit close
    Idle,       // -_- nothing running; a limit reached
    Squint,     // >_< something running
};

//----------------------------------------------------------------//
/* STRUCT */

// A desktop notification: one per session (replaced), its face, its buttons
struct Notification {
    std::string session;            // replaced by the next one of this session (empty: its own)
    std::string title;
    std::string body;               // plain text
    cluster::Face face = cluster::Face::Happy;
    bool urgent = false;
    std::string sound;              // a sound of the desktop theme: complete, message, message-new-instant, dialog-warning... (empty: none)
    std::vector<std::pair<std::string, std::string>> actions{};  // <key, label>: open, allow, deny, always, opt0..opt2
    std::string requestId{};        // the permission / question its buttons answer
};

// The face of the app now and the number shown on it
struct AppFace {
    cluster::Face face = cluster::Face::Idle;
    int badge = 0;                  // sessions waiting on the user (urgent) or running
    bool urgent = false;
};

//----------------------------------------------------------------//
/* FUNCTION */

_nodiscard const char* face_name(const cluster::Face face);     // happy, grin, attentive, waiting, idle, working
_nodiscard const char* face_text(const cluster::Face face);     // ^_^, >‿<, •_•, o_o, -_-, >_<
_cold _nodiscard cluster::Face face_of(const std::string& name);   // its name back (happy when unknown)
_cold _nodiscard std::string face_svg(const cluster::Face face, const int badge = 0, const bool urgent = false);
_cold _nodiscard std::filesystem::path face_file(const cluster::Face face);  // its SVG written once (data folder)
_cold void install_icon(void);      // the icon in the icon theme and claude-cluster.desktop (installed binary only)
_cold void dock_badge(const int count, const bool urgent);   // the number on the icon of the dock (Unity launcher API)

} // namespace end

#endif /* ICONS_H */
