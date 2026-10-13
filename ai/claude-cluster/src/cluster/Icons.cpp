/**************************************************************\
Edition:
##  @date 14/10/2026 by @author Tsukini

File Name:
##  @file Icons.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Icons.hpp"
#include "cluster/Tools.hpp"
#include <functional>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <mutex>

/* faces */
const char* cluster::face_name(const cluster::Face face)
{
    switch (face) {
        case cluster::Face::Happy: return "happy";
        case cluster::Face::Grin: return "grin";
        case cluster::Face::Attentive: return "attentive";
        case cluster::Face::Waiting: return "waiting";
        case cluster::Face::Idle: return "idle";
        case cluster::Face::Squint: return "working";
    }
    return "happy";
}

const char* cluster::face_text(const cluster::Face face)
{
    switch (face) {
        case cluster::Face::Happy: return "^_^";
        case cluster::Face::Grin: return ">‿<";
        case cluster::Face::Attentive: return "•_•";
        case cluster::Face::Waiting: return "o_o";
        case cluster::Face::Idle: return "-_-";
        case cluster::Face::Squint: return ">_<";
    }
    return "^_^";
}

_cold cluster::Face cluster::face_of(const std::string& name)
{
    for (const cluster::Face face: {cluster::Face::Happy, cluster::Face::Grin, cluster::Face::Attentive, cluster::Face::Waiting, cluster::Face::Idle, cluster::Face::Squint})
        if (name == cluster::face_name(face)) return face;
    return cluster::Face::Happy;
}

_cold std::string cluster::face_svg(const cluster::Face face, const int badge, const bool urgent)
{
    // three terminals stacked, the front one with the face (its eyes and mouth in strokes: sharp at 16 px)
    const std::string ink = "#15171b";
    const std::string st = "fill=\"none\" stroke=\"" + ink + "\" stroke-width=\"3\" stroke-linecap=\"round\" stroke-linejoin=\"round\"";
    const double x = 26.0;
    const double y = 38.0;
    const std::function<std::string(double)> n = [](const double v) {
        std::ostringstream out;
        out << v;
        return out.str();
    };
    const std::string mouth = "<line x1=\"" + n(x - 3.5) + "\" y1=\"" + n(y + 6) + "\" x2=\"" + n(x + 3.5) + "\" y2=\"" + n(y + 6) + "\" " + st + "/>";
    const std::string squint = "<path d=\"M" + n(x - 11) + " " + n(y - 5) + " l5 3.5 -5 3.5\" " + st + "/><path d=\"M" + n(x + 11) + " " + n(y - 5) + " l-5 3.5 5 3.5\" " + st + "/>";
    std::string eyes;
    switch (face) {
        case cluster::Face::Happy:
            eyes = "<path d=\"M" + n(x - 12) + " " + n(y - 0.5) + " l3.5 -4.5 3.5 4.5\" " + st + "/><path d=\"M" + n(x + 5) + " " + n(y - 0.5) + " l3.5 -4.5 3.5 4.5\" " + st + "/>" + mouth;
            break;
        case cluster::Face::Grin:
            eyes = squint + "<path d=\"M" + n(x - 4.5) + " " + n(y + 4) + " q4.5 5 9 0\" " + st + "/>";
            break;
        case cluster::Face::Attentive:
            eyes = "<circle cx=\"" + n(x - 8) + "\" cy=\"" + n(y - 1.5) + "\" r=\"2.7\" fill=\"" + ink + "\"/><circle cx=\"" + n(x + 8) + "\" cy=\"" + n(y - 1.5)
                + "\" r=\"2.7\" fill=\"" + ink + "\"/>" + mouth;
            break;
        case cluster::Face::Waiting:
        {
            const std::string ring = "fill=\"none\" stroke=\"" + ink + "\" stroke-width=\"2.6\"";
            eyes = "<circle cx=\"" + n(x - 8) + "\" cy=\"" + n(y - 1.5) + "\" r=\"3.4\" " + ring + "/><circle cx=\"" + n(x + 8) + "\" cy=\"" + n(y - 1.5)
                + "\" r=\"3.4\" " + ring + "/>" + mouth;
        }
            break;
        case cluster::Face::Idle:
            eyes = "<line x1=\"" + n(x - 11) + "\" y1=\"" + n(y - 1.5) + "\" x2=\"" + n(x - 5) + "\" y2=\"" + n(y - 1.5) + "\" " + st + "/><line x1=\"" + n(x + 5) + "\" y1=\""
                + n(y - 1.5) + "\" x2=\"" + n(x + 11) + "\" y2=\"" + n(y - 1.5) + "\" " + st + "/>" + mouth;
            break;
        case cluster::Face::Squint:
            eyes = squint + mouth;
            break;
    }
    std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\" width=\"256\" height=\"256\">"
        "<rect x=\"18\" y=\"6\" width=\"40\" height=\"30\" rx=\"5\" fill=\"none\" stroke=\"#8a93a6\" stroke-width=\"2.5\" opacity=\".55\"/>"
        "<rect x=\"12\" y=\"14\" width=\"40\" height=\"30\" rx=\"5\" fill=\"none\" stroke=\"#8a93a6\" stroke-width=\"2.5\" opacity=\".9\"/>"
        "<rect x=\"6\" y=\"22\" width=\"40\" height=\"32\" rx=\"5\" fill=\"#6d8ef0\"/>" + eyes;
    if (badge > 0) // its number, top right
        svg += "<circle cx=\"50\" cy=\"14\" r=\"12\" fill=\"" + std::string(urgent ? "#f0a868" : "#6fcf97") + "\" stroke=\"#15171b\" stroke-width=\"2\"/>"
            "<text x=\"50\" y=\"19\" text-anchor=\"middle\" font-family=\"sans-serif\" font-weight=\"700\" font-size=\"" + std::string(badge > 9 ? "11" : "14")
            + "\" fill=\"#15171b\">" + (badge > 99 ? std::string("99") : std::to_string(badge)) + "</text>";
    return svg + "</svg>";
}

_cold std::filesystem::path cluster::face_file(const cluster::Face face)
{
    // written when missing or changed (a new version of the drawing)
    const std::filesystem::path path = cluster::data_dir() / "icons" / (std::string("claude-cluster-") + cluster::face_name(face) + ".svg");
    const std::string svg = cluster::face_svg(face);
    std::ifstream in(path);
    const std::string old((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (old != svg) {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        std::ofstream(path) << svg;
    }
    return path;
}

_cold static void write_if_changed_(const std::filesystem::path& path, const std::string& text)
{
    std::ifstream in(path);
    const std::string old((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (old == text) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream(path) << text;
}

_cold void cluster::install_icon(void)
{
    // the binary installed (~/.local/bin, /usr...): its icon in the icon theme and its .desktop (the menu, the dock, the
    // notifications grouped under it); a build being tried leaves the desktop alone
    const std::filesystem::path exe = cluster::self_exe();
    const std::filesystem::path home = cluster::expand_home("~");
    if (exe.parent_path() != home / ".local" / "bin" && !exe.string().starts_with("/usr/")) return;
    const std::filesystem::path share = std::getenv("XDG_DATA_HOME") && *std::getenv("XDG_DATA_HOME") ? std::filesystem::path(std::getenv("XDG_DATA_HOME")) : home / ".local" / "share";
    write_if_changed_(share / "icons" / "hicolor" / "scalable" / "apps" / "claude-cluster.svg", cluster::face_svg(cluster::Face::Happy));
    write_if_changed_(share / "applications" / "claude-cluster.desktop", "[Desktop Entry]\nType=Application\nName=claude-cluster\n"
        "Comment=Several Claude Code sessions driven by a global session\nExec=" + exe.string() + " --gui\nIcon=claude-cluster\nTerminal=false\n"
        "Categories=Development;\nStartupWMClass=claude-cluster\nX-GNOME-UsesNotifications=true\n");
}

_cold void cluster::dock_badge(const int count, const bool urgent)
{
    // the Unity launcher API (Ubuntu dock, Dash to Dock, Plasma...): sent only when it changes
    static std::mutex mutex;
    static std::pair<int, bool> last{-1, false};
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (last == std::make_pair(count, urgent)) return;
        last = {count, urgent};
    }
    if (!cluster::has_command("gdbus")) return;
    cluster::detach({"gdbus", "emit", "--session", "--object-path", "/com/claude_cluster/launcher", "--signal", "com.canonical.Unity.LauncherEntry.Update",
        "application://claude-cluster.desktop", "{'count': <int64 " + std::to_string(count) + ">, 'count-visible': <" + (count > 0 ? "true" : "false")
        + ">, 'urgent': <" + (urgent ? "true" : "false") + ">}"});
}
