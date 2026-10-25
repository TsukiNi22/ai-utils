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
#include <ftxui/screen/terminal.hpp>
#include <ftxui/dom/elements.hpp>
#include <filesystem>
#include <functional>
#include <sys/stat.h>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <set>

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

    struct Instance {
        std::string name;
        Seen seen;
        std::size_t kept = 0;       // stopped: its sessions kept
        std::int64_t last = 0;      // its last use: the conversation of its global session written, else its state saved
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

    // when a data folder was last used: the transcript of its global session, else its saved state (unix seconds, 0: unknown)
    std::int64_t last_used_(const std::filesystem::path& data)
    {
        for (const char* name: {"logs/g.jsonl", "state.json"}) {
            struct stat info{};
            if (::stat((data / name).c_str(), &info) == 0) return static_cast<std::int64_t>(info.st_mtim.tv_sec);
        }
        return 0;
    }

    std::string ago_(const std::int64_t time)
    {
        return time > 0 ? "last used " + cluster::human_age(time) + " ago" : "never used";
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
    const Seen running = look_(cluster::socket_path());
    const std::size_t previous = running.alive ? 0 : previous_(cluster::data_dir());
    const std::int64_t lastMain = last_used_(cluster::data_dir());
    // the independent instances worth listing: running, or stopped with sessions kept; the empty ones (started and left at once) are removed
    std::vector<Instance> instances;
    std::set<std::string> taken;
    std::error_code error;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(instances_(), error)) {
        if (!entry.is_directory(error) || entry.path().filename().string().starts_with(".")) continue; // .supervisor: its web access, not an instance
        Instance instance{entry.path().filename().string(), look_(entry.path() / "sock"), 0, last_used_(entry.path())};
        taken.insert(instance.name);
        if (!instance.seen.alive) instance.kept = previous_(entry.path());
        if (!instance.seen.alive && instance.kept == 0) {
            std::filesystem::remove_all(entry.path(), error);
            continue;
        }
        instances.push_back(instance);
    }
    std::sort(instances.begin(), instances.end(), [](const Instance& a, const Instance& b) {return a.name < b.name;});
    std::string suggestion = "indep-1";
    for (int i = 1; taken.contains(suggestion); suggestion = "indep-" + std::to_string(++i)) {}

    enum class View {Main, List, Name, Delete};
    View view = View::Main;
    int focus = 0;                  // the line under the cursor of the view
    int first = 0;                  // the first line shown of a long list
    std::string typed;              // the name being typed
    std::string problem;            // what is wrong with it
    std::string target;             // the independent instance asked to delete
    cluster::StartChoice picked{cluster::StartChoice::Kind::Quit, ""};
    ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

    // the lines of the view in use: the main menu, or the independent instances then the new one
    const std::function<std::vector<MenuEntry>(void)> lines = [&]() {
        std::vector<MenuEntry> list;
        if (view == View::List) {
            for (const Instance& instance: instances)
                list.push_back({{cluster::StartChoice::Kind::Independent, instance.name}, "", instance.name,
                    instance.seen.alive ? "running · " + count_(instance.seen) + " · " + ago_(instance.last)
                        : "stopped · " + std::to_string(instance.kept) + " session" + (instance.kept == 1 ? "" : "s") + " kept · " + ago_(instance.last)});
            list.push_back({{cluster::StartChoice::Kind::Independent, ""}, "n", "+ New independent instance…", "you name it; its own sessions and global session, nothing shared (no web access)"});
            return list;
        }
        if (running.alive) list.push_back({{cluster::StartChoice::Kind::Open, ""}, "o", "Connect to the running claude-cluster", count_(running)});
        if (!running.alive && previous > 0)
            list.push_back({{cluster::StartChoice::Kind::Restore, ""}, "r", "Restore the previous run",
                std::to_string(previous) + " session" + (previous == 1 ? "" : "s") + " come back, the global conversation goes on · " + ago_(lastMain)});
        if (!running.alive)
            list.push_back({{cluster::StartChoice::Kind::New, ""}, "n", "Start new", previous > 0 ? "nothing restored: the previous sessions go to the trash" : "nothing to restore: an empty start"});
        list.push_back({{cluster::StartChoice::Kind::Open, "list"}, "i", std::string("Independent instances") + (instances.empty() ? "" : " (" + std::to_string(instances.size()) + ")"),
            instances.empty() ? "start one of its own: its own sessions and global session, nothing shared (no web access)" : "open one of yours, or start a new one"});
        list.push_back({{cluster::StartChoice::Kind::Supervisor, ""}, "s", "Supervisor", "every claude-cluster of this computer at a glance: sessions, version, update"});
        list.push_back({{cluster::StartChoice::Kind::Quit, ""}, "q", "Quit", "nothing started"});
        return list;
    };
    const std::function<bool(const std::string&)> valid = [](const std::string& name) {
        return !name.empty() && name.size() <= 32 && std::all_of(name.begin(), name.end(), [](const unsigned char c) {return std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.';})
            && name != "." && name != "..";
    };
    const std::function<std::string(const std::string&)> why = [&valid](const std::string& name) {
        // what is wrong with a name being typed (empty: nothing), checked again at every key (a deleted character included)
        if (name.empty() || valid(name)) return std::string();
        std::string bad;
        for (const unsigned char c: name)
            if (std::isalnum(c) == 0 && c != '-' && c != '_' && c != '.' && bad.find(static_cast<char>(c)) == std::string::npos) bad += static_cast<char>(c);
        std::string shown;
        for (const char c: bad)
            shown += (shown.empty() ? "" : ", ") + (c == ' ' ? std::string("space") : "'" + std::string(1, c) + "'");
        return (shown.empty() ? std::string() : shown + " not allowed: ") + "letters, digits, - _ . only";
    };
    const std::function<int(void)> room = []() {return std::max(2, (ftxui::Terminal::Size().dimy - 10) / 2);};   // the entries (two lines each) that fit

    ftxui::Component component = ftxui::Renderer([&]() {
        ftxui::Elements rows = {ftxui::text(" claude-cluster " + version + (view == View::Main ? "" : view == View::List ? " · independent instances" : view == View::Delete ? " · delete an independent instance" : " · new independent instance")) | ftxui::bold,
            ftxui::text("")};
        if (view == View::Delete) {
            const auto found = std::find_if(instances.begin(), instances.end(), [&](const Instance& i) {return i.name == target;});
            rows.push_back(ftxui::text("   Delete the independent instance \"" + target + "\"?") | ftxui::bold);
            rows.push_back(ftxui::text(found != instances.end() ? "   its " + std::to_string(found->seen.alive ? found->seen.sessions : found->kept) + " sessions, its global conversation and its data go for good ("
                "the Claude Code conversations stay)" : "   it is gone already") | ftxui::dim);
            rows.push_back(ftxui::text(problem.empty() ? "" : "   " + problem) | ftxui::color(ftxui::Color::Red));
            rows.push_back(ftxui::text(""));
            rows.push_back(ftxui::text(" y / Enter delete · Esc / n keep it") | ftxui::dim);
            return ftxui::vbox(std::move(rows)) | ftxui::borderRounded | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 70) | ftxui::center;
        }
        if (view == View::Name) {
            rows.push_back(ftxui::hbox({ftxui::text("   Name: "), ftxui::text(typed) | ftxui::bold, ftxui::text(typed.empty() ? suggestion : "") | ftxui::dim,
                ftxui::text("▏") | ftxui::blink}));
            rows.push_back(ftxui::text(problem.empty() ? "   letters, digits, - _ . (32 at most); an existing name opens that instance" : "   " + problem)
                | (problem.empty() ? ftxui::dim : ftxui::color(ftxui::Color::Red)));
            rows.push_back(ftxui::text(""));
            rows.push_back(ftxui::text(" Enter create (empty: " + suggestion + ") · Esc back") | ftxui::dim);
            return ftxui::vbox(std::move(rows)) | ftxui::borderRounded | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 70) | ftxui::center;
        }
        const std::vector<MenuEntry> entries = lines();
        const int count = static_cast<int>(entries.size());
        const int visible = std::min(count, room());
        focus = std::clamp(focus, 0, count - 1);
        if (focus < first) first = focus;
        if (focus >= first + visible) first = focus - visible + 1;
        first = std::clamp(first, 0, std::max(0, count - visible));
        if (first > 0) rows.push_back(ftxui::text("   ↑ " + std::to_string(first) + " more") | ftxui::dim);
        for (int i = first; i < first + visible; ++i) {
            const bool on = i == focus;
            const MenuEntry& entry = entries[static_cast<std::size_t>(i)];
            const bool quit = entry.choice.kind == cluster::StartChoice::Kind::Quit;
            const std::string key = entry.key.empty() ? "" : "[" + entry.key + "] ";
            ftxui::Element title = ftxui::text((on ? " ❯ " : "   ") + key + entry.title + (on ? " ❮ " : "   "));
            title = on ? title | ftxui::bold | ftxui::color(ftxui::Color::Black) | ftxui::bgcolor(quit ? ftxui::Color::Red : ftxui::Color::Cyan)
                : title | ftxui::color(quit ? ftxui::Color::Red : ftxui::Color::White);
            rows.push_back(title);
            // its description wrapped under it (the menu keeps its width: 2-3 lines at most for a long one)
            rows.push_back(ftxui::hbox({ftxui::text("       "), ftxui::paragraph(entry.detail) | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 68)}) | ftxui::dim);
        }
        if (first + visible < count) rows.push_back(ftxui::text("   ↓ " + std::to_string(count - first - visible) + " more") | ftxui::dim);
        rows.push_back(ftxui::text(""));
        rows.push_back(ftxui::text(view == View::Main ? " ↑ ↓ choose · Enter · the letter · Esc quit" : " ↑ ↓ PgUp PgDn choose · Enter open · d delete · n new · Esc back") | ftxui::dim);
        return ftxui::vbox(std::move(rows)) | ftxui::borderRounded | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 70) | ftxui::center;
    });
    component = ftxui::CatchEvent(component, [&](const ftxui::Event& event) {
        if (event == ftxui::Event::CtrlC) {
            screen.Exit();
            return true;
        }
        if (view == View::Delete) {
            if (event == ftxui::Event::Character('y') || event == ftxui::Event::Return) {
                const auto found = std::find_if(instances.begin(), instances.end(), [&](const Instance& i) {return i.name == target;});
                if (found != instances.end() && found->seen.alive) {
                    problem = "it runs: close its terminals first";
                    return true;
                }
                std::error_code removed;
                std::filesystem::remove_all(instances_() / target, removed);
                instances.erase(std::remove_if(instances.begin(), instances.end(), [&](const Instance& i) {return i.name == target;}), instances.end());
                taken.erase(target);
                suggestion = "indep-1";
                for (int i = 1; taken.contains(suggestion); suggestion = "indep-" + std::to_string(++i)) {}
                problem.clear();
                view = View::List;
            } else if (event == ftxui::Event::Escape || event == ftxui::Event::Character('n')) {
                problem.clear();
                view = View::List;
            }
            return true;
        }
        if (view == View::Name) {
            if (event == ftxui::Event::Escape) {
                view = View::List;
                problem.clear();
            } else if (event == ftxui::Event::Backspace) {
                if (!typed.empty()) typed.pop_back();
                problem = why(typed);
            } else if (event == ftxui::Event::Return) {
                const std::string name = typed.empty() ? suggestion : typed;
                if (!valid(name)) {
                    problem = why(name);
                } else {
                    picked = {cluster::StartChoice::Kind::Independent, name};
                    screen.Exit();
                }
            } else if (event.is_character() && typed.size() < 32) {
                typed += event.character();
                problem = why(typed);
            }
            return true;
        }
        const std::vector<MenuEntry> entries = lines();
        const int count = static_cast<int>(entries.size());
        const std::function<void(int)> pick = [&](const int index) {
            const MenuEntry& entry = entries[static_cast<std::size_t>(index)];
            if (view == View::Main && entry.choice.instance == "list") { // the list of the independent instances
                view = View::List;
                focus = 0;
                first = 0;
            } else if (view == View::List && entry.choice.instance.empty()) { // the new one: its name
                view = View::Name;
                typed.clear();
                problem.clear();
            } else {
                picked = entry.choice;
                screen.Exit();
            }
        };
        if (event == ftxui::Event::ArrowDown || (view == View::Main && event == ftxui::Event::Tab)) focus = (focus + 1) % count;
        else if (event == ftxui::Event::ArrowUp || (view == View::Main && event == ftxui::Event::TabReverse)) focus = (focus + count - 1) % count;
        else if (event == ftxui::Event::PageDown) focus = std::min(count - 1, focus + room());
        else if (event == ftxui::Event::PageUp) focus = std::max(0, focus - room());
        else if (event == ftxui::Event::Home) focus = 0;
        else if (event == ftxui::Event::End) focus = count - 1;
        else if (event == ftxui::Event::Return) pick(focus);
        else if (view == View::List && (event == ftxui::Event::Character('d') || event == ftxui::Event::Delete) && !entries[static_cast<std::size_t>(focus)].choice.instance.empty()) {
            target = entries[static_cast<std::size_t>(focus)].choice.instance; // an instance (not the new one): asked before it goes
            problem.clear();
            view = View::Delete;
        } else if (event == ftxui::Event::Escape) {
            if (view == View::List) {
                view = View::Main;
                focus = 0;
                first = 0;
            } else {
                screen.Exit();
            }
        } else if (event.is_character())
            for (int i = 0; i < count; ++i)
                if (entries[static_cast<std::size_t>(i)].key == event.character()) pick(i);
        return true;
    });
    screen.Loop(component);
    return picked;
}

_cold cluster::StartChoice cluster::start_menu_buttons(const std::string& version,
    const std::function<int(const std::string&, const std::string&, const std::vector<std::string>&)>& choose)
{
    // the choices of the terminal menu for the window (its dialog): connect / restore / start new / an independent instance / quit
    const Seen running = look_(cluster::socket_path());
    const std::size_t previous = running.alive ? 0 : previous_(cluster::data_dir());
    std::vector<Instance> instances;
    std::set<std::string> taken;
    std::error_code error;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(instances_(), error)) {
        if (!entry.is_directory(error) || entry.path().filename().string().starts_with(".")) continue; // .supervisor: its web access, not an instance
        Instance instance{entry.path().filename().string(), look_(entry.path() / "sock"), 0, last_used_(entry.path())};
        taken.insert(instance.name);
        if (!instance.seen.alive) instance.kept = previous_(entry.path());
        if (instance.seen.alive || instance.kept > 0) instances.push_back(instance);
    }
    std::sort(instances.begin(), instances.end(), [](const Instance& a, const Instance& b) {return a.name < b.name;});
    std::vector<std::string> buttons;
    std::vector<cluster::StartChoice> picks;
    std::string text = "claude-cluster " + version + "\n\n";
    if (running.alive) {
        buttons.push_back("Connect");
        picks.push_back({cluster::StartChoice::Kind::Open, ""});
        text += "Connect: the running claude-cluster (" + count_(running) + ")\n";
    } else {
        if (previous > 0) {
            buttons.push_back("Restore");
            picks.push_back({cluster::StartChoice::Kind::Restore, ""});
            text += "Restore: " + std::to_string(previous) + " session" + (previous == 1 ? "" : "s") + " of the previous run come back\n";
        }
        buttons.push_back("Start new");
        picks.push_back({cluster::StartChoice::Kind::New, ""});
        text += std::string("Start new: ") + (previous > 0 ? "nothing restored (the previous sessions go to the trash)" : "an empty start") + "\n";
    }
    buttons.push_back("Independent…");
    picks.push_back({cluster::StartChoice::Kind::Open, "list"});
    text += "Independent: an instance of its own (its own sessions and global session, nothing shared)\n";
    buttons.push_back("Supervisor");
    picks.push_back({cluster::StartChoice::Kind::Supervisor, ""});
    text += "Supervisor: every claude-cluster of this computer at a glance (their sessions, version, update)\n";
    buttons.push_back("Quit");
    picks.push_back({cluster::StartChoice::Kind::Quit, ""});
    const int chosen = choose("claude-cluster", text, buttons);
    if (chosen < 0 || chosen >= static_cast<int>(picks.size())) return {cluster::StartChoice::Kind::Quit, ""};
    if (picks[static_cast<std::size_t>(chosen)].instance != "list") return picks[static_cast<std::size_t>(chosen)];
    // the independent instances: one of them, or a new one (its name: the next free indep-N)
    std::string suggestion = "indep-1";
    for (int i = 1; taken.contains(suggestion); suggestion = "indep-" + std::to_string(++i)) {}
    std::vector<std::string> names;
    std::string list = "Independent instances\n\n";
    for (const Instance& instance: instances) {
        names.push_back(instance.name);
        list += instance.name + ": " + (instance.seen.alive ? "running · " + count_(instance.seen) : "stopped · " + std::to_string(instance.kept) + " session(s) kept")
            + " · " + ago_(instance.last) + "\n";
    }
    names.push_back("New (" + suggestion + ")");
    names.push_back("Back");
    const int which = choose("claude-cluster: independent instances", list + (instances.empty() ? "none yet\n" : ""), names);
    if (which < 0) return {cluster::StartChoice::Kind::Quit, ""};
    if (which == static_cast<int>(names.size()) - 1) return cluster::start_menu_buttons(version, choose);
    if (which == static_cast<int>(instances.size())) return {cluster::StartChoice::Kind::Independent, suggestion};
    return {cluster::StartChoice::Kind::Independent, instances[static_cast<std::size_t>(which)].name};
}
