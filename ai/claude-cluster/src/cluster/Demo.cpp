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
##  @file Demo.cpp

File Description:
##  The demo mode: its folder, its fictional sessions, its fake agent
\**************************************************************/

#include "cluster/Manager.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Demo.hpp"
#include <filesystem>
#include <functional>
#include <sys/stat.h>
#include <algorithm>
#include <iostream>
#include <optional>
#include <unistd.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <poll.h>
#include <deque>
#include <map>
#include <set>

//----------------------------------------------------------------//
/* TOOLS */

namespace {
    enum class Mode {Idle, Busy, Ask, Failing};

    struct Script {
        const char* project;    // its folder in the demo
        const char* name;       // the session
        const char* category;
        std::vector<std::string> tags;
        const char* prompt;     // the first prompt (the fake agent knows it: what it does with it); empty: history only
        Mode mode;
        const char* mood;       // what the agent seems to work on (busy: its thoughts)
    };

    // the fictional company: five projects, nine sessions, what each one is doing
    const std::vector<Script>& scripts_(void)
    {
        static const std::vector<Script> list = {
            {"acme-shop", "checkout-redesign", "frontend", {"checkout", "release"},
                "Redesign the checkout flow: one page, saved cards, Apple Pay, and keep the cart summary visible on mobile.", Mode::Busy, "the checkout page"},
            {"acme-api", "payments-bug-4211", "backend", {"payments", "bug", "urgent"},
                "Payments are charged twice when the webhook is retried: find the cause in the payment service and fix it with a test.", Mode::Busy, "the payment webhook"},
            {"orbit-mobile", "ios-release-3.2", "mobile", {"release"},
                "Prepare the iOS release 3.2: bump the version, update the changelog, build and send it to TestFlight.", Mode::Ask, "the release"},
            {"acme-api", "api-rate-limits", "backend", {"perf"}, "", Mode::Idle, "rate limiting"},
            {"nova-docs", "docs-quickstart", "docs", {}, "", Mode::Idle, "the quickstart"},
            {"lumen-ml", "embedding-eval", "ml", {"experiment", "perf"},
                "Compare the three embedding models on the support-ticket set: recall at 10, latency, memory. Run the evaluation and summarise.", Mode::Busy, "the evaluation"},
            {"infra-ops", "k8s-upgrade", "ops", {"urgent"},
                "Upgrade the staging cluster to Kubernetes 1.31 and check every deployment is healthy afterwards.", Mode::Failing, "the cluster upgrade"},
            {"acme-api", "auth-refactor", "backend", {"refactor"},
                "Refactor the auth module: split the session store from the token service, no behaviour change, keep the tests green.", Mode::Busy, "the auth module"},
        };
        return list;
    }

    const char* kGlobalPrompt = "Give me the state of every session and split the checkout redesign into sub-sessions: design tokens, cart summary, payment step.";

    std::vector<std::string> files_(const std::string& project)
    {
        static const std::vector<std::pair<std::string, std::vector<std::string>>> known = {
            {"acme-shop", {"src/checkout/CheckoutPage.tsx", "src/checkout/CartSummary.tsx", "src/payment/ApplePay.ts", "src/styles/tokens.css", "tests/checkout.spec.ts"}},
            {"acme-api", {"src/payments/webhook.ts", "src/payments/service.ts", "src/auth/tokens.ts", "src/auth/store.ts", "src/middleware/rateLimit.ts"}},
            {"orbit-mobile", {"ios/Orbit/Info.plist", "ios/fastlane/Fastfile", "CHANGELOG.md", "src/App.tsx", "package.json"}},
            {"nova-docs", {"docs/quickstart.md", "docs/install.md", "docs/api/index.md", "mkdocs.yml", "docs/faq.md"}},
            {"lumen-ml", {"eval/run.py", "eval/metrics.py", "models/embed_small.py", "models/embed_large.py", "data/tickets.jsonl"}},
            {"infra-ops", {"clusters/staging/values.yaml", "clusters/staging/upgrade.sh", "charts/gateway/Chart.yaml", "runbooks/upgrade.md", "terraform/main.tf"}},
        };
        for (const auto& [name, files]: known)
            if (name == project) return files;
        return {"README.md"};
    }

    std::filesystem::path base_(void)
    {
        const char* cache = std::getenv("XDG_CACHE_HOME");
        const std::filesystem::path root = cache != nullptr && *cache != '\0' ? std::filesystem::path(cache) : cluster::expand_home("~/.cache");
        return root / "claude-cluster-demo";
    }

    std::uint32_t next_(std::uint32_t& state)
    {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    }

    void ledger_(const std::filesystem::path& data)
    {
        // thirty days of usage (the Stats session, the limits): a few turns a day on the demo sessions, more on the weekdays
        std::ofstream file(data / "usage.jsonl");
        std::uint32_t seed = 42;
        const std::int64_t now = cluster::now();
        std::vector<std::string> sessions = {"g"};
        for (std::size_t i = 0; i < scripts_().size(); ++i)
            sessions.push_back("s" + std::to_string(i + 1));
        for (int day = 30; day >= 0; --day) {
            const int turns = 3 + static_cast<int>(next_(seed) % 9);
            for (int turn = 0; turn < turns; ++turn) {
                const std::int64_t time = now - day * 86400 + 9 * 3600 + static_cast<std::int64_t>(next_(seed) % (9 * 3600)) - (day == 0 ? 3600 : 0);
                if (time > now) continue;
                const std::int64_t input = 800 + next_(seed) % 6000;
                const std::int64_t output = 400 + next_(seed) % 5000;
                const std::int64_t write = 3000 + next_(seed) % 40000;
                const std::int64_t read = 20000 + next_(seed) % 400000;
                const std::int64_t think = next_(seed) % 3 == 0 ? static_cast<std::int64_t>(next_(seed) % 4000) : 0;
                const bool heavy = next_(seed) % 5 == 0;
                const double cost = (static_cast<double>(input) * 3 + static_cast<double>(output) * 15 + static_cast<double>(write) * 3.75 + static_cast<double>(read) * 0.3) / 1e6 * (heavy ? 5.0 : 1.0);
                cluster::Json record = {{"time", time}, {"vendor", "anthropic"}, {"model", heavy ? "claude-opus-5-5" : "claude-sonnet-5-5"}, {"session", sessions[next_(seed) % sessions.size()]},
                    {"tokens", input + write + output}, {"cache", read}, {"cost", cost}, {"cost_known", true}, {"input", input}, {"cache_write", write}, {"cache_write_1h", 0},
                    {"cache_read", read}, {"output", output}, {"thinking", think}, {"dur_ms", 8000 + next_(seed) % 90000}};
                file << record.dump() << "\n";
            }
        }
        cluster::write_json(data / "limits.json", {{"anthropic", {{"limit_5h", 0.38}, {"limit_7d", 0.27}, {"reset_5h", now + 2 * 3600 + 900}, {"reset_7d", now + 3 * 86400}}}});
    }
}

//----------------------------------------------------------------//
/* FUNCTION */

_cold bool cluster::demo_enabled(void)
{
    const char* env = std::getenv("CLAUDE_CLUSTER_DEMO");
    return env != nullptr && *env != '\0' && std::string(env) != "0";
}

_cold bool cluster::demo_web_allows(const std::string& cmd)
{
    static const std::set<std::string> reads = {"list", "read", "status", "notices", "usage", "task_list", "transcript", "display_get", "settings_get", "settings_search",
        "settings_suggest", "orch_state", "job_status", "schedule_list", "todos", "services", "projects", "ask_preview", "stats", "shells", "trash", "hello"};
    return reads.contains(cmd);
}

_cold void cluster::demo_prepare(const bool web)
{
    // the demo folder made again: nothing of a previous demo, nothing of the real data
    const std::filesystem::path dir = base_();
    std::error_code error;
    std::filesystem::remove_all(dir, error);
    for (const char* sub: {"cfg", "data", "projects"})
        std::filesystem::create_directories(dir / sub, error);
    for (const Script& script: scripts_()) {
        const std::filesystem::path folder = dir / "projects" / script.project;
        std::filesystem::create_directories(folder, error);
        for (const std::string& file: files_(script.project)) {
            std::filesystem::create_directories((folder / file).parent_path(), error);
            std::ofstream(folder / file) << "// " << file << " (demo: a fictional file)\n";
        }
    }
    const std::filesystem::path agent = dir / "agent.sh";
    std::ofstream(agent) << "#!/bin/sh\nexec \"" << cluster::self_exe().string() << "\" demo-agent\n";
    ::chmod(agent.c_str(), 0755);
    std::ofstream(dir / "cfg" / "config.toml") << "# the demo: written again at each start\n[global]\nenabled = true\n[ui]\nnotify = false\nmachine_days = 0\nrestore = \"never\"\nlayout = \"list\"\n"
        "[user]\nname = \"Demo\"\nemail = \"demo@example.com\"\n[usage]\naccount = false\n[commands]\nclaude = \"" << agent.string() << "\"\n"
        "[agent]\norchestration = \"ultra\"\n[service]\nkeep_awake = \"off\"\n[web]\nenabled = " << (web ? "true" : "false") << "\nport = 8444\ntty_port = 2223\napprove = false\n";
    ledger_(dir / "data");
    ::setenv("CLAUDE_CLUSTER_CONFIG_DIR", (dir / "cfg").c_str(), 1);
    ::setenv("CLAUDE_CLUSTER_DATA_DIR", (dir / "data").c_str(), 1);
    ::setenv("CLAUDE_CLUSTER_SOCKET", (dir / "sock").c_str(), 1);
    ::setenv("CLAUDE_CLUSTER_DEMO", "1", 1);
}

_cold void cluster::demo_seed(cluster::Manager& manager)
{
    // in a thread of its own: the sessions start one after the other (the interface shows them arriving), then the labels, the tasks, the schedules
    std::thread([&manager]() {
        const std::function<void(int)> wait = [](const int ms) {std::this_thread::sleep_for(std::chrono::milliseconds(ms));};
        try {
            const std::int64_t now = cluster::now();
            const std::filesystem::path projects = base_() / "projects";
            wait(2500);
            std::map<std::string, std::string> ids;
            for (const Script& script: scripts_()) {
                cluster::SpawnRequest request;
                request.cwd = (projects / script.project).string();
                request.name = script.name;
                request.mode = script.mode == Mode::Ask ? "default" : "bypassPermissions";
                if (script.mode == Mode::Idle) { // a conversation already had: its history shown
                    const std::string what = script.mood;
                    request.history = {
                        {cluster::EntryKind::User, "Look at " + what + " and tell me what you would change before we ship it.", "", now - 7200, ""},
                        {cluster::EntryKind::Thinking, "The user wants a review of " + what + ". Read the main files first, then list the risks by how likely they are to hurt.", "", now - 7190, ""},
                        {cluster::EntryKind::Tool, "Read " + files_(script.project)[0], "Read", now - 7185, ""},
                        {cluster::EntryKind::ToolResult, "(210 lines)", "Read", now - 7184, ""},
                        {cluster::EntryKind::Assistant, "## Review of " + what + "\n\nThree things I would change before shipping:\n\n| # | Change | Risk if left | Effort |\n|---|---|---|---|\n"
                            "| 1 | Validate the input at the boundary | wrong data reaches the core | small |\n| 2 | Make the retry idempotent | duplicates under load | medium |\n"
                            "| 3 | Add a test for the empty case | a crash in production | small |\n\n```ts\nexport function guard(input: unknown): Result {\n    if (!isValid(input)) return fail(\"invalid\");\n    return ok(input);\n}\n```\n\n"
                            "I can start with 1 and 3 now - they do not change any behaviour.", "", now - 7140, ""},
                        {cluster::EntryKind::User, "Do 1 and 3, leave 2 for later.", "", now - 3600, ""},
                        {cluster::EntryKind::Assistant, "Done: the guard is in and covered by two tests (empty and malformed input). 2 is noted as a task.", "", now - 3500, ""},
                    };
                }
                const std::string id = manager.spawn(request);
                ids[script.name] = id;
                manager.label(id, std::string(script.category), script.tags);
                if (script.prompt[0] != '\0') manager.send(id, script.prompt);
                wait(1200);
            }
            // the forest of the categories / tags: ops under backend, the tags under the category they belong to
            for (const auto& [node, parent]: std::vector<std::pair<std::string, std::string>>{{"ops", "backend"}, {"checkout", "frontend"}, {"payments", "backend"}, {"release", "mobile"}}) {
                try {
                    manager.taxonomyEdit("set_parent", node, parent, false);
                } catch (const std::exception&) {}
            }
            manager.send(GLOBAL_ID, kGlobalPrompt);
            // the tasks: one waits for a busy session, one for another task; the schedules: soon, and every day
            const std::string payments = ids["payments-bug-4211"];
            const std::string docs = ids["docs-quickstart"];
            const std::string first = manager.addTask("Write the regression test for the double charge once the fix is in", payments, {}, "demo");
            (void)manager.addTask("Update the payments runbook with the retry rules", docs, {first}, "demo");
            (void)manager.addTask("Run the full test suite on the checkout branch and report the failures", ids["checkout-redesign"], {}, "demo");
            (void)manager.addSchedule("+25m", "prompt", ids["ios-release-3.2"], "Check the TestFlight processing status and tell me when it is ready", 0, "demo");
            (void)manager.addSchedule("+2h", "prompt", ids["embedding-eval"], "Post the evaluation table when it is done", 0, "demo");
            (void)manager.addScheduleJson({{"cron", "0 9 * * 1-5"}, {"action", "prompt"}, {"session", GLOBAL_ID},
                {"text", "Weekday summary: what finished, what is blocked, what costs the most"}}, "demo", false);
            (void)manager.addScheduleJson({{"when", "17:30"}, {"action", "notify"}, {"text", "Review the release notes of orbit-mobile 3.2 before the store submission"}}, "demo", false);
        } catch (const std::exception& e) {
            std::cerr << "claude-cluster demo: " << e.what() << std::endl;
        }
    }).detach();
}

//----------------------------------------------------------------//
/* FAKE AGENT */

namespace {
    class Input {
        private:
            std::string _buffer;
            bool _eof = false;

        public:
            // one line of stdin: none within `ms`; closed: eof()
            std::optional<std::string> line(const int ms)
            {
                for (;;) {
                    const std::size_t end = this->_buffer.find('\n');
                    if (end != std::string::npos) {
                        std::string out = this->_buffer.substr(0, end);
                        this->_buffer.erase(0, end + 1);
                        return out;
                    }
                    pollfd waiting{0, POLLIN, 0};
                    if (::poll(&waiting, 1, ms) <= 0) return std::nullopt;
                    char chunk[4096];
                    const ssize_t got = ::read(0, chunk, sizeof(chunk));
                    if (got <= 0) {
                        this->_eof = true;
                        return std::nullopt;
                    }
                    this->_buffer.append(chunk, static_cast<std::size_t>(got));
                }
            }

            _nodiscard bool eof(void) const {return this->_eof;};
    };

    struct Agent {
        Input input;
        std::deque<std::string> prompts;    // the user messages not taken yet
        bool answered = false;              // a control response (a permission answered)
        int step = 0;
        std::string project;
        std::string session = "demo";
    };

    void out_(const cluster::Json& message)
    {
        std::cout << message.dump() << "\n" << std::flush;
    }

    void answer_control_(const cluster::Json& message)
    {
        out_({{"type", "control_response"}, {"response", {{"subtype", "success"}, {"request_id", message.value("request_id", std::string())}, {"response", cluster::Json::object()}}}});
    }

    // what arrives within `ms`: prompts queued, control requests answered
    void listen_(Agent& agent, const int ms)
    {
        const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        for (;;) {
            const int left = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count());
            const std::optional<std::string> line = agent.input.line(std::max(0, left));
            if (agent.input.eof()) ::_exit(0);
            if (!line) return;
            const cluster::Json message = cluster::Json::parse(*line, nullptr, false);
            if (!message.is_object()) continue;
            const std::string type = message.value("type", std::string());
            if (type == "control_request") {
                answer_control_(message);
            } else if (type == "control_response") {
                agent.answered = true;
            } else if (type == "user") {
                const cluster::Json content = message.contains("message") ? message["message"].value("content", cluster::Json()) : cluster::Json();
                std::string text = content.is_string() ? content.get<std::string>() : std::string();
                if (content.is_array())
                    for (const cluster::Json& block: content)
                        if (block.value("type", std::string()) == "text") text += block.value("text", std::string());
                if (!text.empty()) agent.prompts.push_back(text);
            }
            if (std::chrono::steady_clock::now() >= end) return;
        }
    }

    void assistant_(const std::vector<cluster::Json>& blocks)
    {
        out_({{"type", "assistant"}, {"message", {{"role", "assistant"}, {"content", blocks},
            {"usage", {{"input_tokens", 900}, {"output_tokens", 260}, {"cache_read_input_tokens", 24000}, {"cache_creation_input_tokens", 1200}}}}}});
    }

    std::string tool_id_(Agent& agent)
    {
        return "demo-t" + std::to_string(++agent.step);
    }

    const Script* script_of_(const std::string& prompt)
    {
        for (const Script& script: scripts_())
            if (prompt == script.prompt) return &script;
        return nullptr;
    }

    void todo_(Agent& agent, const std::string& mood, const int done)
    {
        const std::vector<std::string> items = {"Read the code around " + mood, "Reproduce the problem with a test", "Make the change", "Run the tests", "Write the summary"};
        cluster::Json todos = cluster::Json::array();
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
            todos.push_back({{"content", items[static_cast<std::size_t>(i)]}, {"status", i < done ? "completed" : i == done ? "in_progress" : "pending"},
                {"activeForm", i == done ? "Working on: " + items[static_cast<std::size_t>(i)] : items[static_cast<std::size_t>(i)]}});
        const std::string id = tool_id_(agent);
        assistant_({{{"type", "tool_use"}, {"id", id}, {"name", "TodoWrite"}, {"input", {{"todos", todos}}}}});
        out_({{"type", "user"}, {"message", {{"role", "user"}, {"content", {{{"type", "tool_result"}, {"tool_use_id", id}, {"content", "Todos updated"}}}}}}});
    }

    // one step of a long piece of work: a thought, a tool and its result
    void work_step_(Agent& agent, const std::string& mood, const int n)
    {
        static const std::vector<std::string> thoughts = {
            "Let me look at how %s is wired before I touch it: the entry point, who calls it, what it returns on the error path.",
            "The test fails only when the second call arrives before the first one is stored. That points at the order of the writes in %s.",
            "I should keep the change small: one function in %s, and a test that fails without it.",
            "Checking the edge cases of %s: an empty list, a timeout in the middle, the same request twice.",
            "The naming in %s is inconsistent with the rest of the code base - I will follow the existing convention, not rename things.",
            "Before I run everything, let me run only the tests that touch %s: faster to read if one still fails.",
        };
        std::string thought = thoughts[static_cast<std::size_t>(n) % thoughts.size()];
        thought.replace(thought.find("%s"), 2, mood);
        assistant_({{{"type", "thinking"}, {"thinking", thought}, {"signature", ""}}});
        listen_(agent, 1500 + (n % 3) * 700);
        const std::vector<std::string> files = files_(agent.project);
        const std::string file = files[static_cast<std::size_t>(n) % files.size()];
        const std::string id = tool_id_(agent);
        cluster::Json call;
        std::string result;
        switch (n % 4) {
            case 0: call = {{"name", "Read"}, {"input", {{"file_path", file}}}}; result = "(" + std::to_string(80 + n * 7) + " lines)"; break;
            case 1: call = {{"name", "Grep"}, {"input", {{"pattern", "retry|idempot|timeout"}, {"path", "src"}}}}; result = file + ":42:  const retries = options.retries ?? 3\n" + file + ":97:  // timeout: see the config"; break;
            case 2: call = {{"name", "Edit"}, {"input", {{"file_path", file}, {"old_string", "return value"}, {"new_string", "return normalize(value)"}}}}; result = "The file " + file + " has been updated."; break;
            default: call = {{"name", "Bash"}, {"input", {{"command", "npm test -- --run " + file}, {"description", "run the related tests"}}}};
                result = "Tests: " + std::to_string(18 + n) + " passed, " + (n % 8 == 3 ? "1 failed" : "0 failed"); break;
        }
        assistant_({{{"type", "tool_use"}, {"id", id}, {"name", call["name"]}, {"input", call["input"]}}});
        listen_(agent, 1800);
        out_({{"type", "user"}, {"message", {{"role", "user"}, {"content", {{{"type", "tool_result"}, {"tool_use_id", id}, {"content", result}}}}}}});
        listen_(agent, 900);
    }

    void finish_(Agent& agent, const std::string& text, const bool error)
    {
        out_({{"type", "result"}, {"subtype", error ? "error_during_execution" : "success"}, {"is_error", error}, {"result", text}, {"session_id", agent.session},
            {"total_cost_usd", 0.031 + 0.002 * agent.step}, {"usage", {{"input_tokens", 1500}, {"output_tokens", 600}, {"cache_read_input_tokens", 30000}, {"cache_creation_input_tokens", 2000}}}});
    }

    void turn_(Agent& agent, const std::string& prompt)
    {
        const Script* script = script_of_(prompt);
        const bool global = prompt == kGlobalPrompt;
        const std::string mood = script != nullptr ? script->mood : global ? "the checkout redesign" : "your request";
        out_({{"type", "system"}, {"subtype", "init"}, {"session_id", agent.session}, {"model", "claude-sonnet-5-5"}, {"tools", cluster::Json::array()}, {"mcp_servers", cluster::Json::array()}});
        if (global) { // the orchestrator: it lists, splits and spawns (what the global session does), without ending
            const std::vector<std::string> steps = {"mcp__claude-cluster__session_list", "mcp__claude-cluster__models_policy", "mcp__claude-cluster__session_spawn",
                "mcp__claude-cluster__task_add", "mcp__claude-cluster__session_display"};
            assistant_({{{"type", "thinking"}, {"thinking", "The request has three independent parts: design tokens, the cart summary and the payment step. They touch different files, so three sub-sessions can work "
                "in parallel; I will ask the cheapest model for the tokens, the default one for the payment step."}, {"signature", ""}}});
            listen_(agent, 2500);
            for (int i = 0;; ++i) {
                const std::string id = tool_id_(agent);
                assistant_({{{"type", "tool_use"}, {"id", id}, {"name", steps[static_cast<std::size_t>(i) % steps.size()]}, {"input", {{"note", "demo"}}}}});
                listen_(agent, 2200);
                out_({{"type", "user"}, {"message", {{"role", "user"}, {"content", {{{"type", "tool_result"}, {"tool_use_id", id}, {"content", "ok"}}}}}}});
                assistant_({{{"type", "thinking"}, {"thinking", "Waiting for the sub-sessions: checking which ones finished and what they touched, so the next one starts from their result."}, {"signature", ""}}});
                listen_(agent, 3500);
                if (!agent.prompts.empty()) break;
            }
            finish_(agent, "(demo) the orchestrator goes on", false);
            return;
        }
        if (script != nullptr && script->mode == Mode::Busy) { // thinking and working: it never ends
            todo_(agent, mood, 1);
            for (int n = 0;; ++n) {
                work_step_(agent, mood, n);
                if (n % 6 == 5) todo_(agent, mood, 1 + (n / 6) % 4);
                if (!agent.prompts.empty()) {
                    assistant_({{{"type", "text"}, {"text", "(demo) taken at the next step: " + agent.prompts.front()}}});
                    agent.prompts.pop_front();
                }
            }
        }
        if (script != nullptr && script->mode == Mode::Ask) { // a permission waiting for the answer
            todo_(agent, mood, 2);
            assistant_({{{"type", "thinking"}, {"thinking", "The version is bumped and the changelog is written. The build needs fastlane, which sends the build to TestFlight: that is outside the folder, so it asks."}, {"signature", ""}}});
            listen_(agent, 1500);
            out_({{"type", "control_request"}, {"request_id", "demo-ask-" + std::to_string(++agent.step)}, {"request", {{"subtype", "can_use_tool"}, {"tool_name", "Bash"},
                {"input", {{"command", "bundle exec fastlane beta"}, {"description", "build the release and upload it to TestFlight"}}}}}});
            while (!agent.answered)
                listen_(agent, 500);
            agent.answered = false;
            assistant_({{{"type", "text"}, {"text", "The build is uploaded: **3.2 (412)**. TestFlight is processing it; I will tell you when it is ready."}}});
            finish_(agent, "uploaded", false);
            return;
        }
        if (script != nullptr && script->mode == Mode::Failing) {
            todo_(agent, mood, 1);
            for (int n = 0; n < 9; ++n)
                work_step_(agent, mood, n);
            assistant_({{{"type", "text"}, {"text", "The control plane did not come back after the upgrade: `kubectl: connection refused` on the staging API server for the last 3 minutes."}}});
            finish_(agent, "API Error: staging cluster unreachable (demo)", true);
            return;
        }
        // any other prompt: one short turn, then it waits
        assistant_({{{"type", "thinking"}, {"thinking", "A request about " + mood + ": this is the demo agent, so I answer with a short, fixed reply."}, {"signature", ""}}});
        listen_(agent, 1500);
        const std::string text = "(demo) This is a pretend agent: nothing was read or changed.\n\nYou asked:\n\n> " + prompt + "\n\n| what | result |\n|---|---|\n| files read | 0 |\n| files changed | 0 |\n| cost | $0.00 |\n";
        assistant_({{{"type", "text"}, {"text", text}}});
        finish_(agent, text, false);
    }
}

_cold int cluster::demo_agent(void)
{
    Agent agent;
    const std::filesystem::path cwd = std::filesystem::current_path();
    agent.project = cwd.filename().string();
    agent.session = "demo-" + std::to_string(::getpid());
    for (;;) {
        while (agent.prompts.empty())
            listen_(agent, 1000);
        const std::string prompt = agent.prompts.front();
        agent.prompts.pop_front();
        turn_(agent, prompt);
    }
}
