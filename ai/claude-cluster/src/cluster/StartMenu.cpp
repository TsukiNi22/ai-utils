/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 22/10/2026 by @author Tsukini

File Name:
##  @file StartMenu.cpp

File Description:
##  The menu of the start of a terminal
\**************************************************************/

#include "cluster/StartMenu.hpp"
#include "cluster/Control.hpp"
#include "cluster/Tools.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <vector>

//----------------------------------------------------------------//
/* TOOLS */

namespace {
    struct MenuEntry {
        cluster::StartChoice choice;
        std::string key;            // the letter that picks it
        std::string title;
        std::string detail;
    };

    struct Seen {
        bool alive = false;
        std::size_t sessions = 0;
        std::size_t working = 0;
    };

    // a core on its socket: its sessions (asked on that socket, this process back on its own afterwards)
    Seen look_(const std::filesystem::path& socket)
    {
        Seen seen;
        const char* before = std::getenv("CLAUDE_CLUSTER_SOCKET");
        const std::string saved = before != nullptr ? before : "";
        ::setenv("CLAUDE_CLUSTER_SOCKET", socket.c_str(), 1);
        try {
            seen.alive = cluster::Control::alive();
            if (seen.alive) {
                const cluster::Json answer = cluster::Control::request({{"cmd", "list"}});
                if (answer.value("ok", false) && answer.contains("data") && answer["data"].is_array())
                    for (const cluster::Json& s: answer["data"]) {
                        ++seen.sessions;
                        seen.working += s.value("state", std::string()) == "working" ? 1 : 0;
                    }
            }
        } catch (const utils::exception::IException&) {}
        if (before != nullptr) ::setenv("CLAUDE_CLUSTER_SOCKET", saved.c_str(), 1);
        else ::unsetenv("CLAUDE_CLUSTER_SOCKET");
        return seen;
    }

    std::string count_(const Seen& seen)
    {
        return std::to_string(seen.sessions) + " session" + (seen.sessions == 1 ? "" : "s") + (seen.working > 0 ? " · " + std::to_string(seen.working) + " working" : "");
    }

    // the sessions of the previous run (the state kept by the last one): the ones that come back
    std::size_t previous_(const std::filesystem::path& data)
    {
        const std::optional<cluster::Json> state = cluster::read_json(data / "state.json");
        std::size_t count = 0;
        if (state && state->is_object())
            for (const cluster::Json& spec: state->value("sessions", cluster::Json::array()))
                count += !spec.value("global", false) || !spec.value("claude_id", std::string()).empty() ? 1 : 0;
        return count;
    }

    std::filesystem::path instances_(void)
    {
        return cluster::data_dir() / "instances";
    }
}

//----------------------------------------------------------------//
/* FUNCTION */

_cold void cluster::start_instance(const std::string& name)
{
    // its own data folder and socket (the config stays the one of this install); no web access, no port taken from the main one
    const std::filesystem::path dir = instances_() / name;
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    ::setenv("CLAUDE_CLUSTER_DATA_DIR", dir.c_str(), 1);
    ::setenv("CLAUDE_CLUSTER_SOCKET", (dir / "sock").c_str(), 1);
    ::setenv("CLAUDE_CLUSTER_INSTANCE", name.c_str(), 1);
}

_cold cluster::StartChoice cluster::start_menu(const std::string& version)
{
    const std::filesystem::path main = cluster::data_dir();
    const Seen running = look_(cluster::socket_path());
    const std::size_t previous = running.alive ? 0 : previous_(main);
    std::vector<MenuEntry> entries;
    if (running.alive)
        entries.push_back({{cluster::StartChoice::Kind::Open, ""}, "o", "Open the running claude-cluster", count_(running)});
    if (!running.alive && previous > 0)
        entries.push_back({{cluster::StartChoice::Kind::Restore, ""}, "r", "Restore the previous run", std::to_string(previous) + " session" + (previous == 1 ? "" : "s") + " come back, the global conversation goes on"});
    if (!running.alive)
        entries.push_back({{cluster::StartChoice::Kind::New, ""}, "n", "Start new", previous > 0 ? "nothing restored: the previous sessions go to the trash" : "nothing to restore: an empty start"});
    // the independent instances: the ones that exist (running or kept), then a new one
    std::vector<std::string> names;
    std::error_code error;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(instances_(), error))
        if (entry.is_directory(error)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    int letter = 0;
    for (const std::string& name: names) {
        const Seen seen = look_(instances_() / name / "sock");
        const std::size_t kept = seen.alive ? 0 : previous_(instances_() / name);
        entries.push_back({{cluster::StartChoice::Kind::Independent, name}, std::to_string(++letter), "Open the independent instance \"" + name + "\"",
            seen.alive ? count_(seen) : "stopped · " + std::to_string(kept) + " session" + (kept == 1 ? "" : "s") + " kept"});
    }
    std::string fresh = "indep-1";
    for (int i = 1; std::find(names.begin(), names.end(), fresh) != names.end(); fresh = "indep-" + std::to_string(++i)) {}
    entries.push_back({{cluster::StartChoice::Kind::Independent, fresh}, "i", "Start an independent instance", "its own sessions, global session and data; nothing shared (no web access)"});
    entries.push_back({{cluster::StartChoice::Kind::Quit, ""}, "q", "Quit", "nothing started"});

    int focus = 0; // the first: what is most likely wanted (open the running one, else restore, else new)
    cluster::StartChoice picked{cluster::StartChoice::Kind::Quit, ""};
    ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
    const std::function<void(int)> pick = [&](const int index) {
        picked = entries[static_cast<std::size_t>(index)].choice;
        screen.Exit();
    };
    ftxui::Component view = ftxui::Renderer([&]() {
        ftxui::Elements rows = {ftxui::text(" claude-cluster " + version) | ftxui::bold, ftxui::text("")};
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const bool on = static_cast<int>(i) == focus;
            const MenuEntry& entry = entries[i];
            const bool quit = entry.choice.kind == cluster::StartChoice::Kind::Quit;
            ftxui::Element title = ftxui::text((on ? " ❯ [" : "   [") + entry.key + "] " + entry.title + (on ? " ❮ " : "   "));
            title = on ? title | ftxui::bold | ftxui::color(ftxui::Color::Black) | ftxui::bgcolor(quit ? ftxui::Color::Red : ftxui::Color::Cyan)
                : title | ftxui::color(quit ? ftxui::Color::Red : ftxui::Color::White);
            rows.push_back(title);
            rows.push_back(ftxui::text("       " + entry.detail) | ftxui::dim);
        }
        rows.push_back(ftxui::text(""));
        rows.push_back(ftxui::text(" ↑ ↓ choose · Enter · the letter · Esc quit") | ftxui::dim);
        return ftxui::vbox(std::move(rows)) | ftxui::borderRounded | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 70) | ftxui::center;
    });
    view = ftxui::CatchEvent(view, [&](const ftxui::Event& event) {
        const int count = static_cast<int>(entries.size());
        if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Tab) focus = (focus + 1) % count;
        else if (event == ftxui::Event::ArrowUp || event == ftxui::Event::TabReverse) focus = (focus + count - 1) % count;
        else if (event == ftxui::Event::Return) pick(focus);
        else if (event == ftxui::Event::Escape || event == ftxui::Event::CtrlC) screen.Exit();
        else if (event.is_character())
            for (std::size_t i = 0; i < entries.size(); ++i)
                if (entries[i].key == event.character()) pick(static_cast<int>(i));
        return true;
    });
    screen.Loop(view);
    return picked;
}
