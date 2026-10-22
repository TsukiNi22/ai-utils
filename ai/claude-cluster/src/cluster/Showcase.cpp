/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 17/10/2026 by @author Tsukini

File Name:
##  @file Showcase.cpp

File Description:
##  The Debug page: every popup, display block, notification and state, shown for real on the interface asking
\**************************************************************/

#include "cluster/Showcase.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Display.hpp"
#include "cluster/Tools.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>

/* tools */
namespace {
    std::atomic<unsigned> showcaseNext{0};

    void put32_(std::string& out, const std::uint32_t value)
    {
        for (int shift = 24; shift >= 0; shift -= 8)
            out.push_back(static_cast<char>((value >> shift) & 0xff));
    }

    std::uint32_t crc_(const std::string& bytes)
    {
        std::uint32_t crc = 0xffffffffu;
        for (const unsigned char c: bytes) {
            crc ^= c;
            for (int k = 0; k < 8; ++k)
                crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
        return crc ^ 0xffffffffu;
    }

    std::string png_(const int width, const int height)
    {
        // a gradient with a disc, written as a PNG without compression (stored deflate blocks): no library needed
        std::string raw;
        for (int y = 0; y < height; ++y) {
            raw.push_back('\0');
            for (int x = 0; x < width; ++x) {
                const double dx = x - width * 0.68;
                const double dy = y - height * 0.45;
                const bool disc = dx * dx + dy * dy < (height * 0.28) * (height * 0.28);
                raw.push_back(static_cast<char>(disc ? 246 : 30 + x * 90 / width));
                raw.push_back(static_cast<char>(disc ? 195 : 60 + y * 120 / height));
                raw.push_back(static_cast<char>(disc ? 67 : 140 + x * 100 / width));
            }
        }
        std::string zlib = {'\x78', '\x01'};
        std::uint32_t a = 1;
        std::uint32_t b = 0;
        for (const unsigned char c: raw) {
            a = (a + c) % 65521;
            b = (b + a) % 65521;
        }
        for (std::size_t at = 0; at < raw.size(); at += 65535) {
            const std::size_t size = std::min<std::size_t>(65535, raw.size() - at);
            zlib.push_back(at + size >= raw.size() ? '\x01' : '\x00');
            zlib.push_back(static_cast<char>(size & 0xff));
            zlib.push_back(static_cast<char>(size >> 8));
            zlib.push_back(static_cast<char>(~size & 0xff));
            zlib.push_back(static_cast<char>((~size >> 8) & 0xff));
            zlib += raw.substr(at, size);
        }
        put32_(zlib, (b << 16) | a);
        const std::function<std::string(const std::string&, const std::string&)> chunk = [](const std::string& type, const std::string& data) {
            std::string out;
            put32_(out, static_cast<std::uint32_t>(data.size()));
            out += type + data;
            put32_(out, crc_(type + data));
            return out;
        };
        std::string header;
        put32_(header, static_cast<std::uint32_t>(width));
        put32_(header, static_cast<std::uint32_t>(height));
        header += std::string("\x08\x02\x00\x00\x00", 5);
        return std::string("\x89PNG\r\n\x1a\n", 8) + chunk("IHDR", header) + chunk("IDAT", zlib) + chunk("IEND", "");
    }

    std::string wav_(void)
    {
        // a short chime (two notes, faded), 16-bit mono 22 050 Hz
        const int rate = 22050;
        std::string samples;
        for (int i = 0; i < rate; ++i) {
            const double t = static_cast<double>(i) / rate;
            const double note = t < 0.45 ? 660.0 : 880.0;
            const double fade = std::exp(-4.0 * (t < 0.45 ? t : t - 0.45));
            const std::int16_t value = static_cast<std::int16_t>(std::sin(2 * M_PI * note * t) * 9000 * fade);
            samples.push_back(static_cast<char>(value & 0xff));
            samples.push_back(static_cast<char>((value >> 8) & 0xff));
        }
        const std::function<std::string(std::uint32_t, int)> le = [](const std::uint32_t value, const int bytes) {
            std::string out;
            for (int k = 0; k < bytes; ++k)
                out.push_back(static_cast<char>((value >> (8 * k)) & 0xff));
            return out;
        };
        return "RIFF" + le(36 + static_cast<std::uint32_t>(samples.size()), 4) + "WAVEfmt " + le(16, 4) + le(1, 2) + le(1, 2) + le(rate, 4) + le(rate * 2, 4)
            + le(2, 2) + le(16, 2) + "data" + le(static_cast<std::uint32_t>(samples.size()), 4) + samples;
    }

    std::string session_(cluster::Manager& manager)
    {
        // the showcase session: one, reused (its folder in the data of claude-cluster); its agent never asked anything
        const std::filesystem::path folder = cluster::data_dir() / "showcase";
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        for (const cluster::Snapshot& s: manager.list(false))
            if (s.spec.name == "showcase" && std::filesystem::path(s.spec.cwd) == folder) return s.spec.id;
        cluster::SpawnRequest request;
        request.cwd = folder.string();
        request.name = "showcase";
        request.cheap = true;   // it only shows what the interface can draw
        return manager.spawn(request);
    }

    std::string platform_note_(const std::string& platform)
    {
        // how this interface shows each element (the others in the README)
        if (platform == "terminal" || platform == "web terminal")
            return "| element | in this terminal |\n|---|---|\n"
                "| popups (questions, permissions, progress, failure) | boxes over the page, keys and mouse (hover, click) |\n"
                "| images, diagrams, svg | blocks of 24-bit colours (chafa; a click opens the real image on the computer) |\n"
                "| charts, metrics, heatmap | unicode bars and sparklines |\n"
                "| audio | a line: a click plays it on the computer |\n"
                "| notifications | the desktop (a click raises this terminal: wmctrl on X11, its tab marked on Wayland) |\n"
                "| states | the dot / icon of the session, the title of the terminal, the face of the bar |\n"
                + std::string(platform == "web terminal" ? "| this page | the terminal of the computer drawn in the browser (WebSocket) |\n" : "");
        if (platform == "window")
            return "| element | in this window |\n|---|---|\n"
                "| popups | Qt dialogs and the popup of the conversation |\n"
                "| images, diagrams, svg | drawn as they are (a click: the viewer of the desktop) |\n"
                "| charts, metrics, heatmap | drawn |\n| audio | a Play link |\n"
                "| notifications | the desktop: a click brings the window to the front (its activation token on Wayland) |\n"
                "| states | the icon of the session, the title of the window, the face |\n";
        return "| element | in this " + std::string(platform == "app" ? "app" : "page") + " |\n|---|---|\n"
            "| popups | dialogs of the page (touch and keyboard) |\n"
            "| images, diagrams, svg | drawn by the browser |\n| charts, metrics, heatmap | drawn by the browser |\n"
            "| audio | the player of the browser |\n"
            "| notifications | " + std::string(platform == "app" ? "Android, by the watch service of the app while it is not in front" : "the browser (its service worker), when allowed") + " |\n"
            "| states | the dot of the session, the title of the tab |\n";
    }

    void displays_(cluster::Manager& manager, const std::string& id, const std::string& platform)
    {
        const std::filesystem::path folder = cluster::data_dir() / "showcase";
        const std::filesystem::path picture = folder / "picture.png";
        const std::filesystem::path chime = folder / "chime.wav";
        std::ofstream(picture, std::ios::binary) << png_(320, 180);
        std::ofstream(chime, std::ios::binary) << wav_();
        const std::vector<cluster::Json> blocks = {
            {{"type", "markdown"}, {"title", "Showcase on this " + platform}, {"text", "Every block claude-cluster can show in a conversation, as the agents "
                "and the global session use them (the tool `session_display`). Nothing here is sent to an agent.\n\n" + platform_note_(platform)}},
            {{"type", "callout"}, {"level", "info"}, {"text", "A callout: info."}},
            {{"type", "callout"}, {"level", "success"}, {"text", "A callout: success."}},
            {{"type", "callout"}, {"level", "warning"}, {"text", "A callout: warning."}},
            {{"type", "callout"}, {"level", "error"}, {"text", "A callout: error."}},
            {{"type", "code"}, {"title", "code"}, {"language", "cpp"}, {"code", "_cold int main(void)\n{\n    return cluster::run(); // a comment\n}"}},
            {{"type", "table"}, {"title", "table"}, {"columns", {"interface", "where", "input"}},
                {"rows", {{"terminal", "a pty", "keys + mouse"}, {"window", "Qt", "keys + mouse"}, {"page", "a browser", "touch + keys"}, {"app", "Android", "touch"}}}},
            {{"type", "chart"}, {"title", "chart (bar)"}, {"kind", "bar"}, {"labels", {"Mon", "Tue", "Wed", "Thu", "Fri"}},
                {"series", {{{"name", "turns"}, {"values", {4, 9, 6, 12, 7}}}, {{"name", "errors"}, {"values", {1, 0, 2, 1, 0}}}}}},
            {{"type", "chart"}, {"title", "chart (line)"}, {"kind", "line"}, {"labels", {"1", "2", "3", "4", "5", "6"}},
                {"series", {{{"name", "tokens (k)"}, {"values", {12, 18, 15, 24, 30, 27}}}}}},
            {{"type", "chart"}, {"title", "chart (pie)"}, {"kind", "pie"}, {"labels", {"input", "cache", "output"}},
                {"series", {{{"name", "share"}, {"values", {20, 65, 15}}}}}},
            {{"type", "progress"}, {"title", "progress"}, {"value", 64}, {"max", 100}, {"label", "64 % of the build"}},
            {{"type", "metrics"}, {"title", "metrics"}, {"items", {{{"label", "turns"}, {"value", 42}, {"delta", "+5"}, {"trend", {3, 5, 4, 7, 9}}},
                {{"label", "cost"}, {"value", 1.27}, {"unit", "$"}, {"delta", "-0.10"}, {"trend", {2, 1.8, 1.5, 1.4, 1.27}}}}}},
            {{"type", "heatmap"}, {"title", "heatmap"}, {"rows", {{1, 3, 0, 5}, {2, 4, 6, 1}, {0, 2, 3, 7}}}, {"xlabels", {"w1", "w2", "w3", "w4"}},
                {"ylabels", {"a", "b", "c"}}},
            {{"type", "image"}, {"title", "image (a PNG of this computer)"}, {"path", picture.string()}, {"alt", "a gradient and a disc"}},
            {{"type", "svg"}, {"title", "svg"}, {"svg", "<svg xmlns='http://www.w3.org/2000/svg' width='240' height='80' viewBox='0 0 240 80'>"
                "<rect x='4' y='4' width='232' height='72' rx='14' fill='#1f2a44' stroke='#7aa2f7' stroke-width='3'/>"
                "<circle cx='40' cy='40' r='18' fill='#f6c343'/><text x='70' y='47' font-size='20' fill='#e6e7ea' font-family='sans-serif'>claude-cluster</text></svg>"}},
            {{"type", "diagram"}, {"title", "diagram (Graphviz)"}, {"dot", "digraph { rankdir=LR; node [shape=box, style=rounded];"
                " interface -> core -> session -> agent; core -> web -> phone; }"}},
            {{"type", "diff"}, {"title", "diff"}, {"file", "hello.cpp"}, {"before", "int main()\n{\n    return 1;\n}\n"}, {"after", "int main(void)\n{\n    return 0;\n}\n"}},
            {{"type", "json"}, {"title", "json"}, {"data", {{"session", "showcase"}, {"state", "idle"}, {"queue", cluster::Json::array()}, {"tokens", {{"in", 120}, {"out", 48}}}}}},
            {{"type", "tree"}, {"title", "tree"}, {"root", {{"name", "project"}, {"dir", true}, {"children", {{{"name", "src"}, {"dir", true},
                {"children", {{{"name", "main.cpp"}, {"dir", false}, {"size", 812}}}}}, {{"name", "README.md"}, {"dir", false}, {"size", 2048}, {"git", "M"}}}}}}},
            {{"type", "terminal"}, {"title", "terminal"}, {"command", "make"}, {"output", "\x1b[32m✓\x1b[0m build ok\n\x1b[33m!\x1b[0m 1 warning\n\x1b[31m✗\x1b[0m 1 test failed"}}, // xstyle: ignore LU-ANSI (a terminal output shown as it is)
            {{"type", "checklist"}, {"title", "checklist (tick it)"}, {"items", {{{"text", "a done item"}, {"done", true}}, {{"text", "an item to tick"}, {"done", false}}}}},
            {{"type", "steps"}, {"title", "steps"}, {"steps", {{{"text", "read"}, {"state", "done"}}, {{"text", "write"}, {"state", "current"}},
                {{"text", "test"}, {"state", "todo"}}, {{"text", "deploy"}, {"state", "error"}}}}},
            {{"type", "timeline"}, {"title", "timeline"}, {"tasks", {{{"name", "design"}, {"start", "2026-10-01"}, {"end", "2026-10-06"}},
                {{"name", "build"}, {"start", "2026-10-05"}, {"end", "2026-10-14"}}, {{"name", "release"}, {"start", "2026-10-14"}, {"end", "2026-10-17"}}}}},
            {{"type", "kanban"}, {"title", "kanban"}, {"columns", {{{"name", "todo"}, {"cards", {"docs", "tests"}}}, {{"name", "doing"}, {"cards", {"showcase"}}},
                {{"name", "done"}, {"cards", {"background core"}}}}}},
            {{"type", "actions"}, {"title", "actions (buttons: a click sends their prompt to the session; here to the showcase, nothing runs)"},
                {"buttons", {{{"label", "A button"}, {"prompt", "(showcase) a button clicked"}}, {{"label", "Another"}, {"prompt", "(showcase) another button"}}}}},
            {{"type", "form"}, {"title", "form (its values come back to the agent; here: a notice)"}, {"fields", {{{"name", "name"}, {"label", "Name"}, {"type", "text"}},
                {{"name", "count"}, {"label", "Count"}, {"type", "number"}, {"value", 3}}, {{"name", "color"}, {"label", "Colour"}, {"type", "choice"},
                {"options", {"blue", "amber"}}}, {{"name", "ok"}, {"label", "Agreed"}, {"type", "bool"}}}}, {"submit", "Send"}},
            {{"type", "details"}, {"title", "details"}, {"summary", "Folded text (open it)"}, {"text", "Hidden until opened: logs, long explanations, raw data."}},
            {{"type", "compare"}, {"title", "compare"}, {"left", {{"title", "before"}, {"text", "one interface, one core"}}},
                {"right", {{"title", "after"}, {"text", "every interface on one core, kept in background"}}}},
            {{"type", "math"}, {"title", "math"}, {"latex", "E = mc^2 \\quad \\sum_{i=1}^{n} i = \\frac{n(n+1)}{2} \\quad \\sqrt{x^2 + y^2}"}},
            {{"type", "audio"}, {"title", "audio (a chime made here)"}, {"path", chime.string()}},
            {{"type", "file"}, {"title", "file (open / download)"}, {"paths", {picture.string(), chime.string()}}},
        };
        for (const cluster::Json& block: blocks) {
            try {
                (void)manager.display(id, block);
            } catch (const utils::exception::IException& e) { // a tool missing (dot...): said instead
                (void)manager.display(id, {{"type", "callout"}, {"level", "warning"}, {"text", block.value("title", block.value("type", std::string()))
                    + " not shown here: " + e.info()}});
            }
        }
    }

    void questions_(cluster::Manager& manager, const std::string& id)
    {
        // the popup of the questions of an agent: a single choice with descriptions, a multiple choice, a free answer (Other),
        // the chat about it (answered here), back / next; answered: a line in the showcase, nothing sent
        cluster::Permission ask;
        ask.requestId = "showcase-q" + std::to_string(++showcaseNext);
        ask.tool = "AskUserQuestion";
        ask.description = "the questions of an agent (showcase)";
        ask.time = cluster::now();
        ask.input = {{"questions", {
            {{"question", "Which interface do you use the most? (a single choice: Enter answers, ← back)"}, {"header", "Interface"}, {"multiSelect", false},
                {"options", {{{"label", "Terminal (Recommended)"}, {"description", "the fastest: keys, mouse, every popup in boxes"}},
                    {{"label", "Window"}, {"description", "Qt: real images, dialogs"}}, {{"label", "Web page / app"}, {"description", "the phone, touch"}}}}},
            {{"question", "What should the showcase show next? (several: Space ticks, Enter validates the ticked ones - none too)"}, {"header", "Next"}, {"multiSelect", true},
                {"options", {{{"label", "Charts"}, {"description", "bar, line, pie"}}, {{"label", "Popups"}, {"description", "progress, failure, permission"}},
                    {{"label", "Notifications"}, {"description", "desktop, phone"}}}}},
            {{"question", "Anything to add? (type on Other: a cursor, ←→, Home / End; Ctrl+T: the chat about these questions)"}, {"header", "Free"}, {"multiSelect", false},
                {"options", {{{"label", "Nothing"}, {"description", "close the showcase question"}}}}}}}};
        manager.showcasePermission(id, ask);
    }

    void previews_(cluster::Manager& manager, const std::string& id)
    {
        // options with a preview (Claude Desktop): markdown, a diff, a table with code, then a chart, a mock-up and a picture
        const std::filesystem::path folder = cluster::data_dir() / "showcase";
        std::filesystem::create_directories(folder);
        const std::filesystem::path picture = folder / "picture.png";
        std::ofstream(picture, std::ios::binary) << png_(320, 180);
        cluster::Permission ask;
        ask.requestId = "showcase-v" + std::to_string(++showcaseNext);
        ask.tool = "AskUserQuestion";
        ask.description = "questions with previews (showcase)";
        ask.time = cluster::now();
        const cluster::Json chart = {{"type", "chart"}, {"title", "Cost per level"}, {"labels", {"L1", "L2", "L3", "L4"}}, {"series", {{{"name", "k"}, {"values", {50, 100, 200, 400}}}}}};
        const cluster::Json image = {{"type", "image"}, {"path", picture.string()}};
        ask.input = {{"questions", {
            {{"question", "Which cost curve? (the preview follows the option: arrows, hover)"}, {"header", "Curve"}, {"multiSelect", false},
                {"options", {{{"label", "Average (Recommended)"}, {"description", "a markdown note"}, {"preview", "# Levels\nLevel 1 50 k\nLevel 2 100 k\nLevel 3 200 k\n\nTotal 350 k"}},
                    {{"label", "Soft"}, {"description", "a coloured diff"}, {"preview", "```diff\n--- a/costs.py\n+++ b/costs.py\n@@ -1,3 +1,3 @@\n COSTS = [\n-    50, 100, 200\n+    50, 80, 120\n ]\n```"}},
                    {{"label", "Hard"}, {"description", "a table and code"}, {"preview", "| Level | Cost |\n|---|---|\n| 1 | 50 k |\n| 2 | 150 k |\n\n```python\ndef cost(level):\n    return 50_000 * 3 ** (level - 1)\n```"}}}}},
            {{"question", "Which layout? (a chart, a mock-up, a picture)"}, {"header", "Layout"}, {"multiSelect", false},
                {"options", {{{"label", "Chart"}, {"description", "a display block"}, {"preview", chart.dump()}},
                    {{"label", "Mock-up"}, {"description", "fixed-width text"}, {"preview", "┌──────────────┐\n│  Menu        │\n│  [ OK ] [ X ]│\n└──────────────┘"}},
                    {{"label", "Picture"}, {"description", "an image file"}, {"preview", image.dump()}}}}}}}};
        manager.showcasePermission(id, ask);
    }

    bool notifications_(cluster::Manager& manager, const std::string& id)
    {
        // a plain one and an urgent one with its buttons (a click shows the showcase session), whatever the settings
        cluster::Notification plain{id, "claude-cluster showcase", "a notification: a click shows its session", cluster::Face::Happy, false, "message"};
        cluster::Notification urgent{"showcase-urgent", "claude-cluster showcase: urgent", "waits for you - its buttons answer it", cluster::Face::Attentive, true,
            "message-new-instant"};
        urgent.actions = {{"open", "Open"}, {"later", "Later"}};
        urgent.onAction = [&manager, id](const std::string& key) {manager.notice(id, "showcase: the notification answered: " + key);};
        const bool sent = manager.notify(plain, "showcase");
        return manager.notify(urgent, "showcase") || sent;
    }

    void permission_(cluster::Manager& manager, const std::string& id)
    {
        cluster::Permission ask;
        ask.requestId = "showcase-p" + std::to_string(++showcaseNext);
        ask.tool = "Bash";
        ask.description = "rm -rf build && cmake -S . -B build (showcase: never run)";
        ask.reason = "a permission asked by an agent: Allow, Always (its rule kept), Deny";
        ask.time = cluster::now();
        ask.input = {{"command", "rm -rf build && cmake -S . -B build"}, {"description", "rebuild from scratch (showcase: never run)"}};
        manager.showcasePermission(id, ask);
    }
}

/* functions */
_cold std::string cluster::showcase(cluster::Manager& manager, const std::string& what, const std::string& platform)
{
    if (manager.replica()) return manager.call({{"cmd", "showcase"}, {"what", what}, {"platform", platform}}).value("message", std::string());
    static const std::set<std::string> known = {"all", "displays", "questions", "previews", "upgrade", "permission", "states", "progress", "failure", "notify"};
    if (!known.contains(what)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "showcase: " + what
        + " (all, displays, questions, previews, upgrade, permission, states, progress, failure, notify)");
    const std::string id = session_(manager);
    manager.requestFocus(id);
    const std::string shown = platform.empty() ? std::string("interface") : platform;
    if (what == "displays") {
        std::thread([&manager, id, shown]() {
            try {
                displays_(manager, id, shown);
            } catch (const std::exception& e) {
                manager.notice(id, std::string("showcase: ") + e.what(), true);
            }
        }).detach();
        return "the showcase session: every display block";
    }
    if (what == "questions") {
        questions_(manager, id);
        return "the popup of the questions of an agent";
    }
    if (what == "previews") {
        previews_(manager, id);
        return "the questions with a preview beside their options";
    }
    if (what == "upgrade") {
        manager.showcaseUpgrade();
        return "the popup of a newer claude-cluster (a pretend version: nothing installed or stopped)";
    }
    if (what == "permission") {
        permission_(manager, id);
        return "the popup of a permission";
    }
    if (what == "notify") {
        const bool sent = notifications_(manager, id);
        manager.notice(id, "showcase: a notice (the line under the prompt, the toast of the page)");
        manager.notice(id, "showcase: an error notice", true);
        return sent ? "two desktop notifications (one urgent, with its buttons) and two notices (shown here even when Notifications > notify is off)"
            : "two notices; no desktop notification: no notification daemon here (gdbus / notify-send)";
    }
    // the timed ones: in the background (the interface keeps drawing them)
    std::thread([&manager, what, id, shown]() {
        const std::function<void(int)> wait = [](const int ms) {std::this_thread::sleep_for(std::chrono::milliseconds(ms));};
        try {
            if (what == "states" || what == "all") {
                if (what == "all") displays_(manager, id, shown);
                for (const cluster::State state: {cluster::State::Working, cluster::State::Waiting, cluster::State::Error, cluster::State::Idle}) {
                    manager.showcaseState(id, state);
                    wait(1600);
                }
            }
            if (what == "progress" || what == "all") {
                cluster::Busy busy(manager, "Showcase: a long action");
                for (const char* step: {"reading", "building", "testing", "packing"}) {
                    cluster::busy_step(step);
                    for (int line = 1; line <= 3; ++line) {
                        cluster::busy_log(std::string(step) + ": line " + std::to_string(line) + " of its log");
                        wait(350);
                    }
                }
                busy.end(true, "done: the popup tells it, then closes");
            }
            if (what == "failure" || what == "all") {
                wait(what == "all" ? 2500 : 0);
                (void)manager.fail("Showcase: an action that failed", "showcase", "what went wrong is told here; Retry runs it again, OK closes",
                    [&manager, id]() {manager.notice(id, "showcase: Retry pressed (the action would run again)");});
            }
            if (what == "all") {
                wait(1500);
                (void)notifications_(manager, id);
                manager.notice(id, "showcase: a notice");
                wait(1500);
                permission_(manager, id);
                questions_(manager, id);
            }
        } catch (const std::exception& e) {
            manager.notice(id, std::string("showcase: ") + e.what(), true);
        }
    }).detach();
    return what == "all" ? "the whole showcase: displays, states, progress, failure, notification, permission, questions" : "showcase: " + what;
}
