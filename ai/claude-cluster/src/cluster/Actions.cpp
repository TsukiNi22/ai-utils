/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Actions.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/External.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Display.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Models.hpp"
#include "cluster/Stats.hpp"
#include "cluster/Match.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Web.hpp"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <regex>

_cold static std::string session_info_(cluster::Manager& manager, const std::string& id); // everything about a session (a popup)

/* actions */
_cold std::vector<cluster::Action> cluster::actions(cluster::Manager& manager, const std::string& session)
{
    if (manager.replica()) { // a replica: the palette of its core
        std::vector<cluster::Action> list;
        try {
            for (const cluster::Json& a: manager.call({{"cmd", "actions"}, {"session", session}}))
                list.push_back({a.value("id", std::string()), a.value("title", std::string()), a.value("input", std::string()), a.value("fill", std::string()),
                    a.value("ui", false)});
        } catch (const utils::exception::IException&) {}
        return list;
    }
    const cluster::Config& config = manager.config();
    const std::optional<cluster::Snapshot> snap = session.empty() ? std::nullopt : manager.snapshot(session);
    std::vector<cluster::Action> list = {
        {"settings", "Settings: every option (setup page, like a BIOS)", "", "", true},
        {"plugins", "Plugins: sources (a repository, a folder, a .zip), install / remove, on / off, their files", "", "", true},
        {"upgrade", "Update claude-cluster to the newer version installed (the sessions kept, nothing to restart)", "", "", false},
        {"info", "Session info: model, thinking, api / login, cost, limits, everything about the one in focus", "", "", false},
        {"stats", "Statistics: cost and tokens per day / model / provider, sessions ranking, cache, uptime, heatmap, tools, limits (F10)", "", "", false},
        {"shells", "Background shells: the !commands running (a popup), stop one with /kill", "", "", false},
        {"shells_kill", "Background shells: stop them all", "", "", false},
        {"settings_search", "Settings: search one (its name, its description) and open it", "", "", true},
        {"machine_sync", "Map of this computer: scan again (asks first)", "", "", true},
        {"edit_global_prompt", "System prompt of the global session: edit it", "", "", true},
        {"edit_sessions_prompt", "System prompt of the sub-sessions: edit it (empty: none)", "", "", true},
        {"edit_vocabulary", "Vocabulary of the voice: edit it (proper names heard)", "", "", true},
        {"edit_words", "Voice words: edit them (cancel / send said alone)", "", "", true},
        {"restore_menu", "Reopen a closed session (the trash)", "", "", true},
        {"start_over", "Start over: every sub-session to the trash (restorable), the queued tasks cancelled, a new conversation for the global session",
            "type yes to confirm", "", false},
        {"vim", std::string("Vim mode of the prompt: ") + (manager.ui("vim", config.vim ? "true" : "false") == "true" ? "turn off" : "turn on"), "", "", true},
        {"new_session", "New session: folder [backend]", "folder [backend]", ".", false},
        {"new_project", "New session on a project of this computer (search it by its name)", "", "", true},
        {"tasks_view", "Tasks: the todo lists of the sessions, the task queue, the schedules", "", "", false},
        {"send", "Send a prompt to the session", "prompt", "", false},
    };

    if (snap) {
        if (!snap->permissions.empty()) {
            list.push_back({"allow", "Allow: " + cluster::one_line(snap->permissions.front().tool + " " + snap->permissions.front().description, 70), "", "", false});
            list.push_back({"always", "Always allow (this rule)", "", "", false});
            list.push_back({"deny", "Deny the permission", "", "", false});
        }
        list.push_back({"interrupt", "Interrupt the running turn", "", "", false});
        list.push_back({"rename", "Rename the session", "name", snap->spec.name, false});
        list.push_back({"describe", "Describe the session (its short description: " + (snap->spec.summary.empty() ? std::string("none yet") : cluster::one_line(snap->spec.summary, 50)) + ")",
            "a few words (empty: written again)", snap->spec.summary, false});
        if (snap->spec.backend.starts_with("anthropic") || snap->spec.backend.starts_with("claude")) { // api | auth: how it is billed
            const bool api = manager.authKind(snap->spec.backend) == "api";
            list.push_back({"backend:anthropic", std::string("Billing: auth - the Claude Code login, a subscription") + (api ? "" : " (current)"), "", "", false});
            list.push_back({"backend:anthropic-api", std::string("Billing: api - an Anthropic API key, per token (the key: F9 > Accounts)") + (api ? " (current)" : ""), "", "", false});
        }
        for (const auto &[mode, description]: cluster::permission_modes())
            list.push_back({"mode:" + mode, "Permission mode: " + mode + (snap->spec.mode == mode ? " (current)" : "") + " - " + description, "", "", false});
        try {
            const cluster::ModelInfo info = cluster::model_info(manager, session);
            for (const auto &[model, description]: info.models)
                list.push_back({"model:" + model, "Model: " + model + (info.model == model ? " (current)" : "") + " - " + description, "", "", false});
            list.push_back({"model", "Model: another one (any id of the provider)", "model", info.model, false});
            if (!info.efforts.empty()) list.push_back({"effort:default", std::string("Effort (thinking): default of the agent") + (info.effort.empty() || info.effort == "default" ? " (current)" : ""), "", "", false});
            for (const std::string& level: info.efforts)
                list.push_back({"effort:" + level, "Effort (thinking): " + level + (info.effort == level ? " (current)" : ""), "", "", false});
        } catch (const utils::exception::IException&) {} // an unknown provider: no choice
        for (const char* panel: {"skills", "tokens", "context", "cost", "usage", "git", "diff", "tools", "files", "tasks"})
            list.push_back({std::string("panel:") + panel, std::string("Panel ") + panel + (snap->spec.panels.contains(panel) ? ": hide" : ": show"), "", "", false});
        // a !command waiting: given to the agent or not
        if (manager.ui("share", "") == session) {
            list.push_back({"share:yes", "Give the !command and its output to the agent: this time", "", "", false});
            list.push_back({"share:always", "Give it to the agent: always (not asked again)", "", "", false});
            list.push_back({"share:no", "Not given to the agent: this time", "", "", false});
            list.push_back({"share:never", "Never given to the agent (not asked again)", "", "", false});
        }
        // a model / mode / effort picked: where it applies (this session first: Enter at once)
        if (const std::string pending = manager.ui("apply", ""); !pending.empty()) {
            const std::string what = pending.substr(0, pending.find(':')) + " " + pending.substr(pending.find(':') + 1);
            const std::vector<cluster::Snapshot> subs = manager.list(false);
            list.push_back({"apply:here", "Apply " + what + " to this session (" + snap->spec.name + ")", "", "", false});
            if (!snap->spec.global) list.push_back({"apply:global", "Apply " + what + " to the global session", "", "", false});
            list.push_back({"apply:subs", "Apply " + what + " to every sub-session (" + std::to_string(subs.size()) + ")", "", "", false});
            list.push_back({"apply:all", "Apply " + what + " to every session (" + std::to_string(subs.size() + 1) + ")", "", "", false});
            list.push_back({"apply:new", "Apply " + what + " to the next new sub-sessions (their default)", "", "", false});
            for (const cluster::Snapshot& other: subs)
                if (other.spec.id != session)
                    list.push_back({"apply:" + other.spec.id, "Apply " + what + " to " + other.spec.name + " only", "", "", false});
        }
        if (!snap->spec.global) {
            list.push_back({"cd", "Move the session to another folder", "folder", snap->spec.cwd, false});
            list.push_back({"close_session", "Close the session (to the trash)", "", "", false});
            if (snap->queued > 0)
                list.push_back({"send_next", "Send the next queued prompt now (the running turn interrupted): " + cluster::one_line(snap->queue.empty() ? std::string() : snap->queue.front(), 50),
                    "", "", false});
            const std::string can = cluster::resumable(*snap);
            list.push_back({"resume", can == "retry" ? "Retry now (the API error waits for its retry)" : can == "continue" ? "Continue the session (it stopped: interrupted, an error)"
                : "Continue the session (\"continue\" sent)", "", "", false});
        } else {
            list.push_back({manager.remote() ? "remote_off" : "remote_on", manager.remote() ? "Remote Control: off (back here)" : "Remote Control: on (claude.ai / app)", "", "", false});
        }
        list.push_back({"export", "Export the conversation to Markdown", "file", "~/" + snap->spec.name + ".md", false});
        list.push_back({"where_folder", "Open its folder (" + cluster::short_path(snap->spec.cwd) + ")", "", "", false});
        list.push_back({"where_terminal", "Open a terminal in its folder (" + cluster::short_path(snap->spec.cwd) + ")", "", "", false});
        if (!snap->spec.global && !snap->spec.claudeId.empty() && snap->spec.backend.substr(0, snap->spec.backend.find('/')) != "codex")
            list.push_back({"fork", "Fork the session (a copy of its conversation, each one goes on apart)", "", "", false});
        // its displays: tick, click, fill, play (the last 30)
        std::size_t seen = 0;
        for (auto it = snap->entries.rbegin(); it != snap->entries.rend() && seen < 30; ++it) {
            if (it->kind != cluster::EntryKind::Display) continue;
            ++seen;
            const cluster::Json d = cluster::Json::parse(it->text, nullptr, false);
            if (!d.is_object()) continue;
            const std::string id = d.value("id", std::string());
            const std::string type = d.value("type", std::string());
            const std::string title = d.value("title", type);
            if (type == "checklist")
                for (std::size_t i = 0; i < d["items"].size(); ++i)
                    list.push_back({"display_tick:" + id + ":" + std::to_string(i), std::string(d["items"][i].value("done", false) ? "Untick: " : "Tick: ")
                        + cluster::one_line(d["items"][i].value("text", std::string()), 60) + " (" + title + ")", "", "", false});
            if (type == "actions")
                for (std::size_t i = 0; i < d["buttons"].size(); ++i)
                    list.push_back({"display_click:" + id + ":" + std::to_string(i), "Action: " + d["buttons"][i].value("label", std::string())
                        + (d["buttons"][i].contains("clicked") ? " (again)" : ""), "", "", false});
            if (type == "form" && !d.value("submitted", false)) {
                std::string fill;
                for (const cluster::Json& field: d["fields"])
                    fill += (fill.empty() ? "" : "; ") + field.value("name", std::string()) + "=" + (field.contains("value") ? (field["value"].is_string()
                        ? field["value"].get<std::string>() : field["value"].dump()) : "");
                list.push_back({"display_form:" + id, "Fill the form: " + title, "name=value; ...", fill, false});
            }
            if (type == "audio") list.push_back({"display_play:" + id, "Play: " + title, "", "", false});
        }
    }
    for (const cluster::SessionSpec& spec: manager.trash()) {
        list.push_back({"restore:" + spec.id, "Restore " + spec.name + (spec.summary.empty() ? std::string() : " - " + cluster::one_line(spec.summary, 70)) + " ("
            + cluster::short_path(spec.cwd) + ", closed " + cluster::human_age(spec.deleted) + " ago)", "", "", false});
        list.push_back({"purge:" + spec.id, "Delete for good " + spec.name + " (trash: conversation and history)", "", "", false});
    }
    if (!manager.trash().empty())
        list.push_back({"purge_all", "Empty the whole trash (" + std::to_string(manager.trash().size()) + " session(s), for good)", "type yes to confirm", "", false});
    for (const auto &[name, profile]: config.profiles)
        list.push_back({"profile:" + name, "New session from the profile " + name, "", "", false});
    list.push_back({"task_add", "Queue a task: [target |] prompt", "task", "", false});
    list.push_back({"search", "Search in the history", "text", "", false});
    list.push_back({"transcript_top", "Go to the start of the transcript", "", "", false});
    list.push_back({"transcript_bottom", "Go to the end of the transcript", "", "", false});
    list.push_back({"providers", "Providers / backends: state", "", "", false});
    for (const cluster::Provider& p: manager.auth().providers()) {
        if (p.auth == cluster::AuthKind::Key) {
            list.push_back({"auth_key:" + p.name, "Provider " + p.name + ": store the API key", "API key", "", false});
            list.push_back({"auth_logout:" + p.name, "Provider " + p.name + ": remove the API key", "", "", false});
        } else if (p.auth == cluster::AuthKind::Login) {
            list.push_back({"auth_login:" + p.name, "Provider " + p.name + ": log in (" + cluster::one_line(manager.auth().loginCommand(p).empty() ? "" : manager.auth().loginCommand(p)[0], 20) + ")", "", "", true});
            list.push_back({"auth_logout:" + p.name, "Provider " + p.name + ": log out", "", "", true});
        }
    }
    if (const cluster::Web* web = manager.web()) {
        const cluster::WebStatus status = web->status();
        list.push_back({status.on ? "web_off" : "web_on", status.on ? "Web access: close (" + status.url + ")" : "Web access: open (a browser of this network, a code)", "", "", false});
        // its devices: connected, trusted, banned (the trusted / banned ones edited while it is closed too)
        list.push_back({"web_devices", "Web devices: connected, trusted, banned (kick, trust no more, ban / unban)", "", "", true});
        if (status.on) {
            list.push_back({"web_show", "Web access: address and code (" + status.url + ", " + status.pin + ")", "", "", false});
            list.push_back({"web_pin", "Web access: a new code", "", "", false});
            if (status.https) list.push_back({"web_trust_ca", "Web access: trust its certificate on this computer (no browser warning; asks the password)", "", "", false});
            for (const cluster::WebDevice& d: status.devices) {
                if (d.state == "waiting") {
                    list.push_back({"web_approve:" + d.id + ":allow", "Web: allow " + d.name + " (" + d.address + ")", "", "", false});
                    list.push_back({"web_approve:" + d.id + ":trust", "Web: allow and trust " + d.name + " (" + d.address + ")", "", "", false});
                    list.push_back({"web_approve:" + d.id + ":deny", "Web: deny " + d.name + " (" + d.address + ")", "", "", false});
                } else if (d.state == "connected") {
                    list.push_back({"web_kick:" + d.id, "Web: disconnect " + d.name + " (" + d.address + ")", "", "", false});
                }
            }
            if (!status.client.empty()) list.push_back({"web_kick", "Web access: disconnect every device (a new code)", "", "", false});
            for (const cluster::WebDevice& d: status.trusted)
                list.push_back({"web_untrust:" + d.id, "Web: trust " + d.name + " no more (the code asked again)", "", "", false});
        }
    }
    // the conversations of the agents opened outside (Claude Code, Codex, Qwen Code: their files; opencode: claude-cluster external)
    for (const cluster::Conversation& c: cluster::external_conversations("claude", 12, manager.conversationIds()))
        list.push_back({"import:" + c.agent + ":" + c.id, "Open a conversation of Claude Code: " + cluster::one_line(c.title, 50) + " · "
            + cluster::short_path(c.cwd) + " · " + cluster::human_age(c.updated) + " ago", "", "", false});
    for (const std::string agent: {"codex", "qwen"})
        for (const cluster::Conversation& c: cluster::external_conversations(agent, 6, manager.conversationIds()))
            list.push_back({"import:" + c.agent + ":" + c.id, "Open a conversation of " + std::string(agent == "codex" ? "Codex" : "Qwen Code") + ": "
                + cluster::one_line(c.title, 50) + " · " + cluster::short_path(c.cwd) + " · " + cluster::human_age(c.updated) + " ago", "", "", false});
    list.push_back({"voice_toggle", "Voice: listen / stop (push-to-talk)", "", "", false});
    list.push_back({"voice_confirm", "Voice: send the transcription now", "", "", false});
    list.push_back({"voice_cancel", "Voice: drop the transcription", "", "", false});
    list.push_back({"voice_edit", "Voice: correct the transcription", "text", "", false});
    list.push_back({"mute", "Voice: stop speaking", "", "", false});
    list.push_back({"voice_setup", "Voice: what is missing to set it up", "", "", false});
    for (const char* layout: {"list", "grid", "tabs", "hub"})
        list.push_back({std::string("layout:") + layout, std::string("Layout: ") + layout, "", "", true});
    for (const auto &[name, style]: config.styles)
        list.push_back({"style:" + name, "Style: " + name + (style.dark ? " (dark)" : " (light)"), "", "", true});
    list.push_back({"purge", "Empty the expired sessions of the trash", "", "", false});
    list.push_back({"config", "Open the config file (path)", "", "", false});
    list.push_back({"quit", "Quit claude-cluster (sessions kept for the next run)", "", "", true});
    return list;
}

_cold static cluster::ActionResult run_action_(cluster::Manager& manager, cluster::Voice& voice, const std::string& id,
    const std::string& session, const std::string& input)
{
    if (manager.replica()) { // a replica: run by its core (its voice, its sessions)
        cluster::ActionResult result;
        try {
            const cluster::Json r = manager.call({{"cmd", "action"}, {"id", id}, {"session", session}, {"input", input}, {"ui", true}});
            result = {r.value("message", std::string()), r.value("error", false), r.value("focus", std::string()), r.value("ui", std::string())};
        } catch (const utils::exception::IException& e) {
            result = {e.info(), true, "", ""};
        }
        return result;
    }
    const std::string arg = id.find(':') == std::string::npos ? "" : id.substr(id.find(':') + 1);
    const std::string base = id.substr(0, id.find(':'));
    cluster::ActionResult result;

    try {
        if (base == "new_session") {
            const std::vector<std::string> parts = cluster::split(input, ' ');
            result.focus = manager.spawn({parts.empty() ? "." : parts[0], "", parts.size() > 1 ? parts[1] : "", "", "", "", ""});
            result.message = "session " + result.focus + " started";
        } else if (base == "send") {
            // a local /command runs like typed in the prompt, the others go to the agent
            const std::optional<cluster::ActionResult> local = cluster::run_slash(manager, session, cluster::trim(input));
            if (local) result = *local;
            else manager.send(session, input);
            manager.addHistory({input, {}, {}}, session);
        } else if (base == "vim") {
            const bool on = manager.ui("vim", manager.config().vim ? "true" : "false") != "true";
            manager.setUi("vim", on ? "true" : "false");
            result.message = on ? "vim mode on (Esc: Normal, i: Insert)" : "vim mode off";
        } else if (base == "allow" || base == "always" || base == "deny") {
            manager.answer(session, "", base);
            result.message = base == "deny" ? "denied" : "allowed";
        } else if (base == "interrupt") {
            manager.interrupt(session);
            voice.mute(); // the answer stopped: its reading too
        } else if (base == "send_next") {
            manager.sendNext(session);
            result.message = "the next queued prompt sent now";
        } else if (base == "resume") {
            result.message = manager.resume(session);
        } else if (base == "rename") {
            manager.rename(session, input);
        } else if (base == "describe") {
            manager.describe(session, input);
            result.message = cluster::trim(input).empty() ? "its description written again" : "described";
        } else if (base == "backend") {
            manager.setBackend(session, arg);
            result.message = "billing: " + manager.authKind(arg);
        } else if (base == "mode" || base == "model" || base == "effort") {
            // the value picked: where it applies is asked next (Enter at once: this session)
            const std::string value = arg.empty() ? cluster::trim(input) : arg;
            if (value.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no " + base);
            manager.setUi("apply", base + ":" + value);
            result.ui = "pick:apply:";
        } else if (base == "share") {
            result.message = manager.shareShell(session, arg);
            manager.setUi("share", "");
        } else if (base == "apply") {
            const std::string pending = manager.ui("apply", "");
            if (pending.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "nothing to apply");
            result.message = manager.applyTo(pending.substr(0, pending.find(':')), pending.substr(pending.find(':') + 1), arg, session);
            manager.setUi("apply", "");
        } else if (base == "panel") {
            manager.togglePanel(session, arg);
        } else if (base == "cd") {
            manager.cd(session, input);
        } else if (base == "new" && !arg.empty()) {
            result.focus = manager.spawn({arg, "", "", "", "", "", ""});
            result.message = "session " + result.focus + " started in " + cluster::short_path(arg);
        } else if (base == "focus") {
            result.focus = arg;
        } else if (base == "tasks_view") {
            result.focus = TASKS_ID;
        } else if (base == "task_act" || base == "sched_act") {
            const std::size_t colon = arg.rfind(':');
            const std::string target = arg.substr(0, colon);
            const std::string what = colon == std::string::npos ? "" : arg.substr(colon + 1);
            result.message = base == "task_act" ? manager.taskAct(target, what, cluster::trim(input)) : manager.scheduleAct(target, what, cluster::trim(input));
        } else if (base == "task_menu") {
            result.ui = "pick:task:" + arg;
        } else if (base == "sched_menu") {
            result.ui = "pick:sched:" + arg;
        } else if (base == "cancel") {
            result.message = "nothing changed";
        } else if (base == "delete_ask") {
            result.ui = "pick:delete:" + arg; // its confirmation (yes / no)
        } else if (base == "delete_session") {
            // closed, then out of the trash: its conversation gone
            const std::optional<cluster::Snapshot> gone = manager.snapshot(arg);
            const std::string target = gone ? gone->spec.id : arg;
            manager.close(target);
            manager.purge(target);
            result.message = (gone ? gone->spec.name : arg) + " deleted for good";
            result.focus = GLOBAL_ID;
        } else if (base == "close_session") {
            manager.close(session);
            result.message = "closed (restorable from the palette)";
            result.focus = GLOBAL_ID;
        } else if (base == "where_folder" || base == "where_terminal") {
            const std::optional<cluster::Snapshot> where = manager.snapshot(session);
            if (!where) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session " + session);
            const std::filesystem::path folder = cluster::expand_home(where->spec.cwd);
            if (base == "where_folder") {
                cluster::open_folder(folder);
                result.message = "opened " + folder.string();
            } else {
                const std::string terminal = cluster::open_terminal(folder);
                result.message = terminal.empty() ? "no terminal found (ptyxis, gnome-terminal, kitty, konsole, alacritty, xterm...)" : terminal + " opened in " + folder.string();
                result.error = terminal.empty();
            }
        } else if (base == "web_trust_ca") {
            // claude-cluster web trust-ca in the background (the window of the password), its end told
            std::thread([&manager]() {
                const cluster::Captured done = cluster::capture({cluster::self_exe().string(), "web", "trust-ca"}, "", 300);
                manager.notice(GLOBAL_ID, done.code == 0 ? "web: its certificate trusted by this computer (restart the browser: no warning)"
                    : "web: certificate not trusted (" + cluster::one_line(done.out, 160) + ")", done.code != 0);
            }).detach();
            result.message = "web: the password asked by its window...";
        } else if (base == "web_approve" || base == "web_untrust" || (base == "web_kick" && !arg.empty())) {
            cluster::Web* web = manager.web();
            if (web == nullptr) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no web access here");
            if (base == "web_approve") {
                const std::size_t colon = arg.rfind(':');
                web->approve(arg.substr(0, colon), arg.substr(colon + 1));
                result.message = "web: " + arg.substr(colon + 1) + (arg.substr(colon + 1) == "deny" ? "" : "ed");
            } else if (base == "web_untrust") {
                web->untrust({arg});
                result.message = "web: trusted no more";
            } else {
                web->kick({arg});
                result.message = "web: disconnected";
            }
        } else if (base == "web_on" || base == "web_off" || base == "web_pin" || base == "web_kick" || base == "web_show") {
            cluster::Web* web = manager.web();
            if (web == nullptr) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no web access here");
            if (base == "web_on" || base == "web_off") web->setOn(base == "web_on");
            if (base == "web_on" || base == "web_off") web->tick();
            if (base == "web_pin") web->renewPin();
            if (base == "web_kick") web->kick();
            const cluster::WebStatus status = web->status();
            result.message = status.on ? "open " + status.url + " on a device of this network, code " + status.pin
                + (status.client.empty() ? "" : " (" + status.client + " connected)") + " (click the code: copied)"
                + (status.https ? "; certificate SHA-256 " + status.fingerprint.substr(0, 23) + "... (the browser shows it)" : "")
                : status.error.empty() ? "web access closed" : "web access: " + status.error;
        } else if (base == "import") {
            // used less than 2 min ago: maybe still open elsewhere, a copy (Claude Code)
            const std::size_t colon = arg.find(':');
            const std::optional<cluster::Conversation> found = cluster::external_conversation(arg.substr(0, colon), arg.substr(colon + 1));
            const bool recent = found && cluster::now() - found->updated < 120;
            result.focus = manager.importConversation(arg.substr(0, colon), arg.substr(colon + 1), "", recent);
            result.message = std::string("opened") + (recent && found->agent == "claude" ? " as a copy (used less than 2 min ago: maybe still open elsewhere)" : "");
        } else if (base == "display_tick" || base == "display_click" || base == "display_play") {
            const std::size_t colon = arg.rfind(':');
            const std::string display = base == "display_play" ? arg : arg.substr(0, colon);
            const std::size_t item = base == "display_play" ? 0 : static_cast<std::size_t>(std::stoul(arg.substr(colon + 1)));
            result.message = manager.displayAct(session, display, {{"item", item}});
        } else if (base == "display_form") {
            // "name=value; name2=value2"
            cluster::Json values = cluster::Json::object();
            for (const std::string& pair: cluster::split(input, ';')) {
                const std::size_t equal = pair.find('=');
                if (equal != std::string::npos) values[cluster::trim(pair.substr(0, equal))] = cluster::trim(pair.substr(equal + 1));
            }
            result.message = manager.displayAct(session, arg, {{"values", values}});
        } else if (base == "stats") {
            result.message = "statistics: session " + cluster::stats_open(manager, true);
        } else if (base == "shells") {
            const std::optional<cluster::ActionResult> shown = cluster::run_slash(manager, session, "/shells");
            if (shown) result = *shown;
        } else if (base == "shells_kill") {
            result.message = manager.shellKill("all");
        } else if (base == "info") {
            result.message = session_info_(manager, session);
            result.ui = "message"; // a popup in every interface
        } else if (base == "upgrade") {
            result.message = manager.upgradeAct("update"); // the newer claude-cluster installed: switched to it
        } else if (base == "fork") {
            result.focus = manager.forkSession(session, "");
            result.message = "forked";
        } else if (base == "remote_on" || base == "remote_off") {
            manager.setRemote(base == "remote_on");
        } else if (base == "export") {
            std::ofstream(cluster::expand_home(input)) << manager.exportMarkdown(session);
            result.message = "exported to " + cluster::expand_home(input).string();
        } else if (base == "restore") {
            result.focus = manager.restore(arg);
            result.message = "restored";
        } else if (base == "profile") {
            result.focus = manager.spawn({"", "", "", "", arg, "", ""});
        } else if (base == "task_add") {
            // "target | prompt" or just the prompt
            const std::size_t bar = input.find('|');
            const std::string target = bar == std::string::npos ? "" : cluster::trim(input.substr(0, bar));
            result.message = "task " + manager.addTask(cluster::trim(bar == std::string::npos ? input : input.substr(bar + 1)), target, {}) + " queued";
        } else if (base == "search") {
            const std::vector<cluster::Hit> hits = manager.search(input, 8);
            result.message = hits.empty() ? "nothing found" : std::to_string(hits.size()) + " hit(s):";
            for (const cluster::Hit& hit: hits)
                result.message += "\n" + hit.name + " " + cluster::human_age(hit.time) + ": " + cluster::one_line(hit.text, 100);
        } else if (base == "providers") {
            for (const cluster::Provider& p: manager.auth().providers()) {
                const auto [ready, detail] = manager.auth().status(p);
                result.message += (result.message.empty() ? "" : "\n") + std::string(ready ? "[ok] " : "[--] ") + p.name + " (" + p.driver + "): " + detail;
            }
        } else if (base == "auth_key") {
            manager.auth().storeKey(arg, cluster::trim(input));
            result.message = arg + ": key stored";
        } else if (base == "auth_logout") {
            const std::optional<cluster::Provider> provider = manager.auth().find(arg);
            if (provider && provider->auth == cluster::AuthKind::Key) manager.auth().clearKey(arg);
            result.message = arg + ": logged out";
        } else if (base == "voice_toggle") {
            if (!manager.config().voice.enabled) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "voice disabled: [voice] enabled = true in the config");
            voice.toggle();
            result.message = voice.listening() ? "listening..." : "voice stopped";
        } else if (base == "voice_confirm") {
            voice.confirm();
        } else if (base == "voice_cancel") {
            voice.cancel();
        } else if (base == "voice_edit") {
            voice.edit(input);
        } else if (base == "mute") {
            voice.mute();
        } else if (base == "voice_setup") {
            const std::vector<std::string> missing = voice.setup();
            result.message = missing.empty() ? "voice: everything is set up" : "voice setup:";
            for (const std::string& line: missing)
                result.message += "\n- " + line;
        } else if (base == "purge") {
            manager.purge(arg);
            result.message = arg.empty() ? "trash purged (expired sessions)" : "deleted for good";
        } else if (base == "start_over") {
            // everything from zero: the sub-sessions closed (the trash keeps them), the tasks and groups stopped, the global session cleared
            if (cluster::lower(cluster::trim(input)) != "yes") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not confirmed: nothing changed");
            for (const cluster::Json& group: manager.groupStatus("")) {
                if (group.value("finished", false)) continue;
                try {
                    manager.groupCancel(group.value("group", std::string()), true);
                } catch (const utils::exception::IException&) {}
            }
            for (const cluster::Task& task: manager.tasks())
                if (task.status == "queued") manager.cancelTask(task.id);
            std::size_t closed = 0;
            for (const cluster::Snapshot& snap: manager.list(false)) {
                manager.close(snap.spec.id);
                ++closed;
            }
            manager.clear(GLOBAL_ID);
            result.focus = GLOBAL_ID;
            result.message = "started over: " + std::to_string(closed) + " session(s) to the trash, a new conversation for the global session";
        } else if (base == "purge_all") {
            if (cluster::lower(cluster::trim(input)) != "yes") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not confirmed: the trash is kept");
            manager.purge("*");
            result.message = "trash emptied";
        } else if (base == "config") {
            result.message = manager.config().path().string() + " (reloaded when saved), system prompts: " + manager.config().globalPrompt
                + " (global session), " + manager.config().sessionsPrompt + " (sub-sessions)";
        } else {
            result.message = "unknown action " + id;
            result.error = true;
        }
    } catch (const utils::exception::IException& e) {
        result.message = std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info());
        result.error = true;
    }
    return result;
}

_cold static std::string session_name_(cluster::Manager& manager, const std::string& session)
{
    if (session.empty()) return "claude-cluster";
    const std::optional<cluster::Snapshot> snap = manager.snapshot(session);
    return snap ? snap->spec.name + " (" + session + ")" : session;
}

_cold cluster::ActionResult cluster::run_action(cluster::Manager& manager, cluster::Voice& voice, const std::string& id,
    const std::string& session, const std::string& input)
{
    // run here: its popup while it takes time, its failure (OK / Retry); an interface of the core: run by the core (so)
    if (manager.replica()) return run_action_(manager, voice, id, session, input);
    std::string title = id.substr(0, id.find(':'));
    std::replace(title.begin(), title.end(), '_', ' ');
    if (!title.empty()) title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
    if (id.find(':') != std::string::npos) title += " " + id.substr(id.find(':') + 1);
    if (!cluster::trim(input).empty() && input.size() < 60 && !id.starts_with("auth_")) title += " " + cluster::trim(input); // never a key
    cluster::Busy busy(manager, title, session);
    const cluster::ActionResult result = run_action_(manager, voice, id, session, input);
    busy.end(!result.error, result.message);
    if (result.error && cluster::failure_popups()) (void)manager.fail(title, session_name_(manager, session), result.message, [&manager, &voice, id, session, input]() {
        (void)cluster::run_action(manager, voice, id, session, input);
    });
    return result;
}

/* prompt: completion and local /commands */
_cold const std::vector<std::pair<std::string, std::string>>& cluster::slash_commands(void)
{
    static const std::vector<std::pair<std::string, std::string>> list = {
        {"cd", "<folder>: move the session to another project"},
        {"pwd", "where the session runs (its folder, the computer, its pid) - shown here only, nothing sent to the agent"},
        {"new", "<folder> [backend]: new session"},
        {"mode", "[mode]: permission mode of the session; none: pick it"},
        {"rename", "<name>: rename the session"},
        {"describe", "[text]: its short description (none: written again)"},
        {"continue", "the session goes on where it stopped (interrupted, an error); a retry of the API waited: now (= F4)"},
        {"retry", "an error of the API retried now (= /continue, F4)"},
        {"close", "close the session (to the trash)"},
        {"interrupt", "stop the running turn"},
        {"export", "[file]: conversation as Markdown"},
        {"restore", "<session>: reopen a closed session"},
        {"clear", "a new conversation for the session (the context again from 0; the transcript emptied, archived for the search)"},
        {"model", "[model]: the session on this model (opus, sonnet, an id, an Ollama model...), same conversation; none: pick it"},
        {"effort", "[level]: thinking / reasoning effort (default, low, medium, high, xhigh, max: per agent), same conversation; none: pick it"},
        {"cost", "cost and tokens of the session"},
        {"context", "the context window used"},
        {"usage", "the usage of the account (5 h / 7 d windows) and of this session: a popup, nothing asked to the agent"},
        {"skills", "the skills and plugins loaded for the sessions (yours, the project's, the plugins): a popup"},
        {"info", "everything about the session in focus: agent, model, thinking, api / login, cost, limits (a popup)"},
        {"stats", "the statistics: usage, activity (the Stats session, F10)"},
        {"shells", "the !commands running in the background (and the last ended ones): a popup; /kill <id|all> stops them"},
        {"kill", "<id|all>: stop a background shell (and everything it started)"},
        {"status", "the same as /info"},
        {"mcp", "the MCP servers given to the sessions: a popup"},
        {"plugins", "the plugins browser: sources (repositories, folders, .zip), install / remove, on / off, their files"},
        {"rewind", "the last turn out of the conversation, its prompt back (= Esc Esc on an empty prompt)"},
        {"exit", "close the session (= /close)"},
        {"delete", "delete the session for good: asked first (/delete! at once); /close keeps it in the trash"},
        {"now", "the next queued prompt sent now (the running turn interrupted, = F8)"},
        {"tasks", "the Tasks session: the todo lists, the task queue, the schedules"},
        {"settings", "the settings page"},
        {"vim", "vim mode of the prompt on / off"},
        {"help", "the local commands"},
    };
    return list;
}

_cold cluster::CompletionProvider cluster::completion_provider(cluster::Manager& manager, std::function<std::string(void)> session)
{
    return [&manager, session](const cluster::CompletionKind kind, const std::string& query) {
        std::vector<cluster::Candidate> items;
        std::vector<float> scores; // of items: the closest first ([ui] matching)
        const std::function<void(void)> ranked = [&items, &scores]() {
            std::vector<std::size_t> order(items.size());
            for (std::size_t i = 0; i < order.size(); ++i)
                order[i] = i;
            std::stable_sort(order.begin(), order.end(), [&scores](const std::size_t a, const std::size_t b) {return scores[a] < scores[b];});
            std::vector<cluster::Candidate> sorted;
            for (const std::size_t i: order)
                sorted.push_back(items[i]);
            items = std::move(sorted);
        };
        if (kind == cluster::CompletionKind::Shell) return cluster::shell_commands(query); // this computer (a replica too)
        if (kind == cluster::CompletionKind::Ex) {
            for (const auto &[ex, description]: cluster::ex_commands())
                if (const std::optional<float> s = cluster::match_score(query, cluster::trim(ex))) {
                    items.push_back({ex, ":" + cluster::trim(ex), description, ex.ends_with("/") || ex.ends_with(" ")});
                    scores.push_back(*s);
                }
            ranked();
            return items;
        }
        if (kind == cluster::CompletionKind::Setting) {
            for (const cluster::SettingsPage& page: cluster::settings_pages(manager.config()))
                for (const cluster::Setting& s: page.settings) {
                    if (s.kind == cluster::SettingKind::Action) continue;
                    const std::optional<float> a = cluster::match_score(query, s.key);
                    const std::optional<float> b = cluster::match_score(query, s.section + "." + s.key);
                    if (!a && !b) continue;
                    items.push_back({s.section + "." + s.key + "=", s.section + "." + s.key, s.label + " (now: " + (s.value ? s.value(manager.config()) : std::string()) + ")", true});
                    scores.push_back(std::min(a.value_or(1e9F), b.value_or(1e9F)));
                }
            ranked();
            return items;
        }
        if (manager.replica()) { // a replica: completed by its core
            try {
                for (const cluster::Json& c: manager.call({{"cmd", "complete"}, {"session", session()}, {"kind", static_cast<int>(kind)}, {"query", query}}))
                    items.push_back({c.value("value", std::string()), c.value("label", std::string()), c.value("description", std::string()), c.value("directory", false)});
            } catch (const utils::exception::IException&) {}
            return items;
        }
        const std::optional<cluster::Snapshot> snap = manager.snapshot(session());
        const std::string cwd = snap ? snap->spec.cwd : cluster::expand_home("~").string();
        const std::string wanted = cluster::lower(query);
        const std::function<void(const std::string&, const std::string&)> add = [&](const std::string& value, const std::string& description) {
            const std::optional<float> score = cluster::match_score(wanted, value);
            if (!score) return;
            if (std::any_of(items.begin(), items.end(), [&](const cluster::Candidate& c) {return c.value == value;})) return;
            items.push_back({value, value, description, false});
            scores.push_back(*score);
        };
        switch (kind) {
            case cluster::CompletionKind::File:
                if (snap && snap->spec.global && !query.starts_with("/") && !query.starts_with("~")) {
                    // the global session: from the folder claude-cluster was started in, as ~/... paths (valid for it too)
                    std::error_code error;
                    const std::string base = cluster::short_path(std::filesystem::current_path(error).string());
                    const std::string prefix = base == "~" ? "~/" : base + "/";
                    std::vector<cluster::Candidate> list = cluster::complete_paths(cluster::expand_home(prefix).string(), query, false);
                    for (cluster::Candidate& candidate: list)
                        candidate.value = cluster::escape_path(prefix) + candidate.value;
                    return list;
                }
                return cluster::complete_paths(cwd, query, false);
            case cluster::CompletionKind::Folder: {
                // a name (no path typed): the projects of the map first, then the folders
                std::vector<cluster::Candidate> list;
                if (!query.empty() && query.find('/') == std::string::npos && !query.starts_with("~") && !query.starts_with("."))
                    for (const cluster::Project& p: cluster::projects(query)) {
                        list.push_back({cluster::escape_path(p.path), p.name, "project · " + (p.kind.empty() ? std::string("git") : p.kind) + " · " + p.path, true});
                        if (list.size() >= 12) break;
                    }
                for (const cluster::Candidate& c: cluster::complete_paths(cwd, query, true))
                    list.push_back(c);
                return list;
            }
            case cluster::CompletionKind::Mode:
                for (const auto &[mode, description]: cluster::permission_modes())
                    add(mode, description);
                break;
            case cluster::CompletionKind::Model:
            case cluster::CompletionKind::Effort:
                try {
                    const cluster::ModelInfo info = cluster::model_info(manager, session());
                    if (kind == cluster::CompletionKind::Model) {
                        for (const auto &[model, description]: info.models)
                            add(model, description + (info.model == model ? " (current)" : ""));
                    } else if (!info.efforts.empty()) {
                        add("default", "the one of the agent");
                        for (const std::string& level: info.efforts)
                            add(level, info.effort == level ? "current" : "");
                    }
                } catch (const utils::exception::IException&) {}
                break;
            case cluster::CompletionKind::Session: {
                // the sessions: open ones first (their state, their folder), then the closed ones; a name with a space: its id
                const std::function<std::string(const cluster::SessionSpec&)> handle = [](const cluster::SessionSpec& spec) {
                    return spec.name.find_first_of(" \t") == std::string::npos && !spec.name.empty() ? spec.name : spec.id;
                };
                for (const cluster::Snapshot& s: manager.list(false))
                    if (!s.spec.global) add(handle(s.spec), std::string("open · ") + cluster::state_name(s.state) + " · " + cluster::short_path(s.spec.cwd)
                        + (s.spec.summary.empty() ? std::string() : " · " + cluster::one_line(s.spec.summary, 60)));
                for (const cluster::SessionSpec& spec: manager.trash())
                    add(handle(spec), "closed " + cluster::human_age(spec.deleted) + " ago · " + cluster::short_path(spec.cwd));
                break;
            }
            case cluster::CompletionKind::Trash:
                for (const cluster::SessionSpec& spec: manager.trash())
                    add(spec.name, "closed " + cluster::human_age(spec.deleted) + " ago, " + cluster::short_path(spec.cwd));
                break;
            case cluster::CompletionKind::Command:
            case cluster::CompletionKind::Skill:
                if (kind == cluster::CompletionKind::Command)
                    for (const auto &[name, description]: cluster::slash_commands())
                        add(name, description);
                if (snap) {
                    for (const std::string& skill: snap->skillsAvailable)
                        add(skill, snap->commandHelp.contains(skill) ? snap->commandHelp.at(skill) : "skill");
                    for (const std::string& command: snap->slashCommands)
                        add(command, snap->commandHelp.contains(command) ? snap->commandHelp.at(command) : "command of the agent");
                }
                break;
            default: break;
        }
        if (!wanted.empty()) ranked(); // the closest first
        if (items.size() > 60) items.resize(60);
        return items;
    };
}

_cold static std::string session_info_(cluster::Manager& manager, const std::string& id)
{
    // everything about a session: how it runs (agent, model, thinking, api | auth), where, its conversation, what it used and what limits it
    const std::optional<cluster::Snapshot> snap = manager.snapshot(id);
    if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session");
    const cluster::SessionSpec& spec = snap->spec;
    const cluster::Metrics& m = snap->metrics;
    const std::string provider = spec.backend.substr(0, spec.backend.find('/'));
    const std::string auth = manager.authKind(spec.backend);
    const std::string model = snap->modelUsed.empty() ? spec.model : snap->modelUsed;
    std::string text = spec.name + " (" + spec.id + ")  " + cluster::state_name(snap->state);
    text += "\n\nagent        " + provider + " · " + (auth == "api" ? "API key (billed per token)" : auth == "auth" ? "login (a subscription)" : "local (no cost)");
    text += "\nmodel        " + (model.empty() ? std::string("the default of the agent") : model) + (spec.model.empty() || spec.model == model ? "" : "  (asked: " + spec.model + ")");
    text += "\nthinking     " + (spec.effort.empty() ? std::string("default (the one of the agent)") : spec.effort);
    text += "\nmode         " + spec.mode + "\nfolder       " + cluster::short_path(spec.cwd);
    text += "\nconversation " + (spec.claudeId.empty() ? std::string("not started") : spec.claudeId) + " · " + std::to_string(m.turns) + " turn(s)";
    text += "\ntokens       " + cluster::human_tokens(m.total.total()) + " (last turn " + cluster::human_tokens(m.last.total()) + ", cache read " + cluster::human_tokens(m.total.cacheRead)
        + ", thinking " + cluster::human_tokens(m.total.thinking) + ")";
    text += "\ncontext      " + cluster::human_tokens(m.contextUsed) + " / " + cluster::human_tokens(m.contextWindow)
        + (m.contextWindow > 0 ? " (" + std::to_string(100 * m.contextUsed / m.contextWindow) + "%)" : std::string());
    text += "\ncost         " + (m.costKnown ? cluster::human_cost(m.cost) + " (last turn " + cluster::human_cost(m.lastCost) + ")" : std::string("local / unknown"));
    const cluster::Config& config = manager.config();
    if (config.budget > 0) text += "  · budget " + cluster::human_cost(config.budget);
    const cluster::Json accounts = manager.accountsJson();
    const cluster::Json spent = accounts.is_object() ? accounts.value("spent", cluster::Json::object()) : cluster::Json::object();
    std::string limits;
    for (const auto &[window, limit]: {std::pair<std::string, double>{"day", config.limits.dailyUsd}, {"week", config.limits.weeklyUsd}, {"month", config.limits.monthlyUsd}})
        if (limit > 0) limits += (limits.empty() ? "" : " · ") + window + " " + cluster::human_cost(spent.value(window, 0.0)) + " / " + cluster::human_cost(limit);
    if (!limits.empty()) text += "\nlimits       " + limits + " (" + config.limits.scope + ")";
    const cluster::UsageReport usage = manager.usage(provider == "claude" ? "anthropic" : provider);
    for (const cluster::UsageWindow& w: usage.windows)
        if (w.used >= 0) text += "\naccount      " + w.name + " window " + std::to_string(static_cast<int>(w.used * 100 + 0.5)) + "%" + (w.reset > 0 ? " (reset in " + cluster::took((w.reset - cluster::now()) * 1000) + ")" : "");
    if (snap->queued > 0) text += "\nqueued       " + std::to_string(snap->queued) + " prompt(s)";
    if (!snap->permissions.empty()) text += "\nwaiting      " + std::to_string(snap->permissions.size()) + " permission(s)";
    if (!snap->remoteUrl.empty()) text += "\nremote       " + snap->remoteUrl;
    return text;
}

_cold static std::optional<cluster::ActionResult> run_slash_(cluster::Manager& manager, const std::string& session, const std::string& line)
{
    // A local /command runs here; any other /command (skill, command of the agent) is sent as the prompt; !<command> and
    // :<command>: the modes of the prompt
    const bool mode = line.starts_with("!") || (line.starts_with(":") && !line.starts_with("::"));
    if (!line.starts_with("/") && !mode) return std::nullopt;
    if (manager.replica()) { // a replica: run by its core
        const cluster::Json r = manager.call({{"cmd", "slash"}, {"session", session}, {"text", line}, {"ui", true}});
        if (!r.value("local", false)) return std::nullopt;
        return cluster::ActionResult{r.value("message", std::string()), r.value("error", false), r.value("focus", std::string()), r.value("ui", std::string()),
            r.value("fill", std::string())};
    }
    if (mode) return cluster::run_prompt_mode(manager, session, line);
    const std::size_t space = line.find(' ');
    const std::string name = line.substr(1, space == std::string::npos ? std::string::npos : space - 1);
    const std::string arg = space == std::string::npos ? "" : cluster::unescape_path(cluster::trim(line.substr(space + 1)));
    const std::vector<std::pair<std::string, std::string>>& local = cluster::slash_commands();
    const std::string bare = name.ends_with("!") ? name.substr(0, name.size() - 1) : name; // /delete!: forced
    if (std::none_of(local.begin(), local.end(), [&](const std::pair<std::string, std::string>& c) {return c.first == bare;})) return std::nullopt;

    cluster::ActionResult result;
    try {
        if (name == "cd") {
            manager.cd(session, arg);
            result.message = "moved to " + cluster::short_path(arg);
        } else if (name == "pwd") {
            // where it runs: shown in the status line, never a prompt (the context untouched)
            const std::optional<cluster::Snapshot> snap = manager.snapshot(session);
            if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no such session");
            result.message = cluster::expand_home(snap->spec.cwd).string() + " · " + cluster::host_name() + (snap->pid > 0 ? " · pid " + std::to_string(snap->pid) : "");
        } else if (name == "now") {
            manager.sendNext(session);
            result.message = "the next queued prompt sent now";
        } else if (name == "tasks") {
            result.focus = TASKS_ID;
        } else if (name == "delete" || name == "delete!") {
            if (session == GLOBAL_ID) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the global session can't be deleted");
            if (name == "delete") {
                result.ui = "pick:delete:" + manager.resolve(session); // yes / no
            } else {
                const std::string id = manager.resolve(session);
                manager.close(id);
                manager.purge(id);
                result.message = "deleted for good";
                result.focus = GLOBAL_ID;
            }
        } else if (name == "new") {
            const std::vector<std::string> parts = cluster::split(arg, ' ');
            result.focus = manager.spawn({parts.empty() ? "." : parts[0], "", parts.size() > 1 ? parts[1] : "", "", "", "", ""});
            result.message = "session " + result.focus + " started";
        } else if (name == "mode" && cluster::trim(arg).empty()) {
            result.ui = "pick:mode:"; // its choices to pick from (like Claude Code)
        } else if (name == "mode") {
            manager.setMode(session, arg);
        } else if (name == "continue" || name == "retry") {
            result.message = manager.resume(session);
        } else if (name == "rename") {
            manager.rename(session, arg);
        } else if (name == "describe") {
            manager.describe(session, arg);
            result.message = cluster::trim(arg).empty() ? "its description written again" : "described";
        } else if (name == "clear") {
            manager.clear(session);
            result.message = "new conversation";
        } else if (name == "model") {
            if (arg.empty()) {
                // its choices to pick from (like Claude Code); none known: told
                const cluster::ModelInfo info = cluster::model_info(manager, session);
                if (!info.models.empty()) result.ui = "pick:model:";
                else result.message = "model: " + (info.model.empty() ? std::string("default of the provider") : info.model) + " (/model <name> to change)";
            } else {
                manager.setModel(session, arg);
                result.message = "model " + arg + " (same conversation)";
            }
        } else if (name == "effort") {
            if (arg.empty()) {
                const cluster::ModelInfo info = cluster::model_info(manager, session);
                std::string levels = "default";
                for (const std::string& level: info.efforts)
                    levels += ", " + level;
                if (info.efforts.empty()) result.message = "no effort level for the " + info.driver + " driver";
                else result.ui = "pick:effort:"; // its choices to pick from
            } else {
                manager.setEffort(session, arg);
                result.message = "effort " + arg + " (same conversation)";
            }
        } else if (name == "cost" || name == "context") {
            const std::optional<cluster::Snapshot> snap = manager.snapshot(session);
            if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session");
            const cluster::Metrics& m = snap->metrics;
            if (name == "cost") result.message = "cost " + (m.costKnown ? cluster::human_cost(m.cost) + " (last turn " + cluster::human_cost(m.lastCost) + ")" : std::string("local / unknown"))
                + " · tokens " + cluster::human_tokens(m.total.total()) + " in " + std::to_string(m.turns) + " turn(s)";
            else result.message = "context " + cluster::human_tokens(m.contextUsed) + " / " + cluster::human_tokens(m.contextWindow)
                + (m.contextWindow > 0 ? " (" + std::to_string(100 * m.contextUsed / m.contextWindow) + "%)" : std::string());
        } else if (name == "stats") {
            result.message = "statistics: session " + cluster::stats_open(manager, true);
        } else if (name == "shells") {
            const cluster::Json jobs = manager.shells();
            result.message = jobs.empty() ? std::string("no !command yet: the ones that last more than a few seconds run here in the background (!tail -f log, !npm run dev...)") : "shells";
            for (const cluster::Json& j: jobs)
                result.message += "\n" + j.value("id", std::string()) + "  " + (j.value("running", false) ? "running " : j.value("killed", false) ? "stopped " : "exit " + std::to_string(j.value("code", -1)) + " ")
                    + std::to_string(j.value("seconds", std::int64_t{0})) + " s  " + cluster::one_line(j.value("command", std::string()), 70) + (j.value("tail", std::string()).empty() ? "" : "\n      " + j.value("tail", std::string()));
            result.message += jobs.empty() ? "" : "\n\n/kill <id|all> stops one";
            result.ui = "message";
        } else if (name == "kill") {
            result.message = manager.shellKill(arg.empty() ? std::string("all") : arg);
        } else if (name == "info" || name == "status") {
            result.message = session_info_(manager, session);
            result.ui = "message";
        } else if (name == "usage") {
            // shown here (a popup), never asked to the agent: nothing in its conversation, nothing spent
            const std::optional<cluster::Snapshot> snap = manager.snapshot(session);
            if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session");
            const cluster::Metrics& m = snap->metrics;
            const std::string vendor = snap->spec.backend.substr(0, snap->spec.backend.find('/'));
            {
                const cluster::UsageReport u = manager.usage(vendor == "claude" ? "anthropic" : vendor);
                const std::function<std::string(const cluster::UsageSum&)> sum = [](const cluster::UsageSum& s) {
                    return cluster::human_tokens(s.tokens) + " tokens, " + std::to_string(s.turns) + " turn(s)" + (s.costKnown || s.cost > 0 ? ", " + cluster::human_cost(s.cost) : "");
                };
                result.message = "usage (" + u.vendor + ")";
                for (const cluster::UsageWindow& w: u.windows)
                    result.message += "\n" + w.name + ": " + (w.used < 0 ? std::string("?") : std::to_string(static_cast<int>(w.used * 100 + 0.5)) + "%")
                        + (w.reset > 0 ? " (reset in " + cluster::took((w.reset - cluster::now()) * 1000) + ")" : "") + (w.locked.empty() ? "" : " · " + w.locked);
                if (u.windows.empty() && u.limit5h >= 0) result.message += "\n5 h: " + std::to_string(static_cast<int>(u.limit5h * 100 + 0.5)) + "%";
                if (u.windows.empty() && u.limit7d >= 0) result.message += "\n7 d: " + std::to_string(static_cast<int>(u.limit7d * 100 + 0.5)) + "%";
                result.message += "\ntoday: " + sum(u.today) + "\n7 days: " + sum(u.week) + "\nthis session: " + cluster::human_tokens(m.total.total()) + " tokens, "
                    + std::to_string(m.turns) + " turn(s)" + (m.costKnown ? ", " + cluster::human_cost(m.cost) : "");
            }
            result.ui = "message";
        } else if (name == "skills") {
            // the skills: of the user (~/.claude/skills), of the project (.claude/skills), the plugins of Claude Code
            const std::optional<cluster::Snapshot> snap = manager.snapshot(session);
            std::vector<std::string> lines;
            std::error_code error;
            for (const std::filesystem::path& dir: {cluster::expand_home("~/.claude/skills"), std::filesystem::path(snap ? cluster::expand_home(snap->spec.cwd) : "") / ".claude" / "skills"}) {
                if (!std::filesystem::is_directory(dir, error)) continue;
                std::vector<std::string> names;
                for (const std::filesystem::directory_entry& e: std::filesystem::directory_iterator(dir, error))
                    if (std::filesystem::exists(e.path() / "SKILL.md", error)) names.push_back(e.path().filename().string());
                std::sort(names.begin(), names.end());
                std::string row;
                for (const std::string& n: names)
                    row += (row.empty() ? "" : ", ") + n;
                lines.push_back(cluster::short_path(dir.string()) + " (" + std::to_string(names.size()) + "): " + (row.empty() ? "none" : row));
            }
            const cluster::Json plugins = cluster::Json::parse(cluster::capture({manager.config().command("claude"), "plugin", "list", "--json"}, "", 30).out, nullptr, false);
            std::string row;
            std::size_t count = 0;
            if (plugins.is_array())
                for (const cluster::Json& p: plugins)
                    if (p.is_object()) {
                        row += (row.empty() ? "" : ", ") + p.value("id", std::string()) + (p.value("enabled", true) ? "" : " (off)");
                        ++count;
                    }
            lines.push_back("plugins (" + std::to_string(count) + "): " + (row.empty() ? "none" : row));
            result.message = "skills and plugins loaded for the sessions started now";
            for (const std::string& text: lines)
                result.message += "\n" + text;
            result.ui = "message";
        } else if (name == "mcp") {
            result.message = "MCP servers";
            result.message += "\nclaude-cluster (the global session: its tools, settings > MCP)";
            for (const std::string& server: manager.config().mcpServers)
                result.message += "\n" + server + " (the global session)";
            result.message += "\nthe ones of Claude Code itself (claude mcp list) for every session";
            result.ui = "message";
        } else if (name == "plugins") {
            result.ui = "plugins";
        } else if (name == "rewind") {
            result.ui = "rewind";
        } else if (name == "close" || name == "exit") {
            manager.close(session);
            result.focus = GLOBAL_ID;
            result.message = "closed (Ctrl+Shift+T / Alt+T to reopen)";
        } else if (name == "interrupt") {
            manager.interrupt(session);
        } else if (name == "export") {
            const std::optional<cluster::Snapshot> snap = manager.snapshot(session);
            const std::string path = arg.empty() ? "~/" + (snap ? snap->spec.name : session) + ".md" : arg;
            std::ofstream(cluster::expand_home(path)) << manager.exportMarkdown(session);
            result.message = "exported to " + cluster::expand_home(path).string();
        } else if (name == "restore") {
            if (arg.empty()) result.ui = "restore_menu";
            else result.focus = manager.restore(arg);
        } else if (name == "settings") {
            result.ui = "settings";
        } else if (name == "vim") {
            const bool on = manager.ui("vim", manager.config().vim ? "true" : "false") != "true";
            manager.setUi("vim", on ? "true" : "false");
            result.message = on ? "vim mode on (Esc: Normal, i: Insert)" : "vim mode off";
        } else if (name == "help") {
            result.message = "local commands (the other / commands go to the agent: skills, /compact...):";
            for (const auto &[command, description]: local)
                result.message += "\n/" + command + " " + description;
        }
    } catch (const utils::exception::IException& e) {
        result.message = std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info());
        result.error = true;
    }
    return result;
}

_cold std::optional<cluster::ActionResult> cluster::run_slash(cluster::Manager& manager, const std::string& session, const std::string& line)
{
    // a local command run here: its popup while it takes time, its failure (OK / Retry); else sent as the prompt
    if (manager.replica() || !(line.starts_with("/") || line.starts_with("!") || (line.starts_with(":") && !line.starts_with("::")))) return run_slash_(manager, session, line);
    cluster::Busy busy(manager, cluster::one_line(line, 70), session);
    const std::optional<cluster::ActionResult> result = run_slash_(manager, session, line);
    if (!result) return result; // not a local one (its popup dropped: never shown)
    busy.end(!result->error, result->message);
    if (result->error && cluster::failure_popups()) (void)manager.fail(cluster::one_line(line, 70), session_name_(manager, session), result->message,
        [&manager, session, line]() {
        (void)cluster::run_slash(manager, session, line);
    });
    return result;
}

_cold std::string cluster::next_mode(const std::string& mode)
{
    static const std::vector<std::string> cycle = {"auto", "acceptEdits", "plan", "bypassPermissions", "manual"};
    auto it = std::find(cycle.begin(), cycle.end(), mode == "default" ? "manual" : mode);
    return it == cycle.end() || it + 1 == cycle.end() ? cycle.front() : *(it + 1);
}

_cold std::string cluster::help_text(const cluster::Config& config)
{
    static const std::vector<std::pair<std::string, std::string>> actions = {
        {"palette", "command palette"}, {"settings", "settings page"}, {"stats", "statistics (usage, activity)"}, {"help", "this help"},
        {"next_session", "next session"}, {"prev_session", "previous session"}, {"prefix", "then an arrow: session on the left / right / above / below"},
        {"global", "the global session"}, {"new_session", "new session"}, {"close_session", "close the session (trash)"},
        {"restore", "reopen a closed session"}, {"allow", "allow the permission"}, {"always", "always allow (rule)"}, {"deny", "deny the permission"},
        {"interrupt", "stop the running turn, else clear the prompt (copy with a selection); twice on an empty prompt: quit"}, {"mode_cycle", "next permission mode"},
        {"layout", "next layout"}, {"push_to_talk", "voice: listen / stop"}, {"resume", "retry now / continue the session"}, {"send_next", "the next queued prompt now (the turn interrupted)"}, {"mute", "voice: stop speaking"}, {"quit", "quit"},
        {"undo", "undo"}, {"redo", "redo"}, {"select_all", "select all"}, {"cut", "cut"}, {"paste", "paste (text or image)"},
        {"stash", "stash the prompt"}, {"unstash", "take the stashed prompt back"}, {"history", "history of the prompts"},
        {"search", "search in the transcript"}, {"edit", "edit the prompt in $EDITOR"},
        {"expand", "thinking and tool results: in full / short"},
        {"session_up", "session above (previous one in the list; the view follows)"}, {"session_down", "session below (next one)"},
        {"session_left", "session on the left (grid; previous one in the list)"}, {"session_right", "session on the right (grid; next one in the list)"},
        {"session_page_up", "a page of sessions up (list, grid rows)"}, {"session_page_down", "a page of sessions down"}, {"session_first", "the first session"}, {"session_last", "the last session"},
        {"transcript_top", "jump to the start of the transcript"}, {"transcript_bottom", "jump to the end (follow the end again)"},
    };
    std::string text = "Keys (config [keys], settings page F9):";
    for (const auto &[action, description]: actions) {
        if (!config.keys.contains(action)) continue;
        const std::string key = config.keys.at(action);
        text += "\n" + key + std::string(key.size() < 30 ? 30 - key.size() : 1, ' ') + description;
    }
    text += "\nAlt+1..9                      session N"
        "\nAlt+Enter / Shift+Enter       new line in the prompt"
        "\nEsc                           queued prompt back in the prompt (to edit it)"
        "\nEsc Esc                       clear the prompt; empty: the last turn removed, its prompt back"
        "\n↑ / ↓                         history (first / last line of the prompt)"
        "\nEnter                         send (/local commands run here)"
        "\n/vim                          vim mode: Esc Normal (hjkl w b e 0 ^ $ gg G f t x X s S D C dd cc yy d/c/y+motion"
        "\n                              p P u Ctrl+R ~ J r v V counts), i a I A o O Insert, Enter sends"
        "\nShift / Ctrl+Shift + arrows   select; Ctrl + arrows: by word; Home / End"
        "\nPageUp / PageDown, wheel      scroll the transcript (Shift+PageUp / Shift+PageDown, Ctrl+Home / Ctrl+End: start / end)"
        "\n@ / /                         completion: Tab complete, Enter accept, arrows choose, Space / Esc close";
    return text;
}

_cold std::string cluster::key_action(const cluster::Config& config, const std::string& key)
{
    // a key can have alternatives: "Ctrl+Shift+T|Alt+T"
    for (const auto &[action, bound]: config.keys)
        for (const std::string& alternative: cluster::split(bound, '|'))
            if (cluster::lower(alternative) == cluster::lower(key)) return action;
    return "";
}

/* model and effort */
_cold cluster::ModelInfo cluster::model_info(cluster::Manager& manager, const std::string& session)
{
    const std::optional<cluster::Snapshot> snap = manager.snapshot(session);
    if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session '" + session + "'");
    const cluster::Auth auth(manager.config());
    const cluster::Backend backend = auth.resolve(snap->spec.backend);
    cluster::ModelInfo info;
    info.driver = backend.provider.driver;
    info.model = !backend.model.empty() ? backend.model : snap->modelUsed;
    info.effort = snap->spec.effort;
    info.models = cluster::model_choices(manager.config(), backend.provider);
    info.efforts = cluster::effort_levels(info.driver);
    return info;
}

_cold static std::vector<cluster::Action> generated_(cluster::Manager& manager, const std::string& wanted)
{
    // the lists made for a filter: new: (a project of the map, its folder), menu:<id> (what a session can do: its right
    // click), delete:<id> (its confirmation); none: not one of them
    std::vector<cluster::Action> out;
    if (cluster::lower(wanted).starts_with("new:")) {
        const std::string query = cluster::trim(wanted.substr(4));
        for (const cluster::Project& p: cluster::projects(query)) {
            out.push_back({"new:" + p.path, "New session on " + p.name + "  · " + (p.kind.empty() ? std::string("git") : p.kind) + " · " + p.path
                + (p.byName || query.empty() ? "" : "  (found by its path / kind)"), "", "", false});
            if (out.size() >= 40) break;
        }
        std::error_code error;
        if (!query.empty() && std::filesystem::is_directory(cluster::expand_home(query), error))
            out.insert(out.begin(), cluster::Action{"new:" + query, "New session in the folder " + query, "", "", false});
        if (out.empty()) out.push_back({"new_session", "New session: a folder (no project matches" + std::string(query.empty() ? ": map this computer first" : "") + ")", "folder [backend]", ".", false});
        return out;
    }
    const std::function<std::string(const std::string&)> who = [&manager](const std::string& by) {
        if (by.empty()) return std::string("you");
        if (by == GLOBAL_ID) return std::string("the global session");
        const std::optional<cluster::Snapshot> s = manager.snapshot(by);
        return s ? s->spec.name : by;
    };
    if (cluster::lower(wanted).starts_with("task:")) {
        const std::string id = cluster::trim(wanted.substr(5));
        for (const cluster::Task& t: manager.tasks()) {
            if (t.id != id) continue;
            const std::string told = "a reason (optional: told to " + who(t.by) + ")";
            if (t.status == "queued") out.push_back({"task_act:" + id + ":force", "Start " + id + " now (past the limit of parallel sessions)", "", "", false});
            if (t.status == "queued" || t.status == "running") out.push_back({"task_act:" + id + ":cancel", "Cancel " + id, told, "", false});
            if (!t.session.empty()) out.push_back({"focus:" + t.session, "Open its session (" + (manager.snapshot(t.session) ? manager.snapshot(t.session)->spec.name : t.session) + ")", "", "", false});
            out.push_back({"task_act:" + id + (t.archived ? ":unarchive" : ":archive"), t.archived ? "Back in the list" : "Archive " + id, "", "", false});
            out.push_back({"task_act:" + id + ":delete", "Delete " + id + " for good", told, "", false});
        }
        return out;
    }
    if (cluster::lower(wanted).starts_with("sched:")) {
        const std::string id = cluster::trim(wanted.substr(6));
        for (const cluster::Schedule& w: manager.schedules()) {
            if (w.id != id) continue;
            const std::string told = "a reason (optional: told to " + who(w.by) + ")";
            if (w.status == "waiting") out.push_back({"sched_act:" + id + ":trigger", "Run " + id + " now" + (w.every > 0 ? " (its next time kept)" : ""), "", "", false});
            if (w.status == "waiting") out.push_back({"sched_act:" + id + ":cancel", "Cancel " + id, told, "", false});
            if (w.action == "prompt") out.push_back({"focus:" + w.session, "Open its session", "", "", false});
            out.push_back({"sched_act:" + id + (w.archived ? ":unarchive" : ":archive"), w.archived ? "Back in the list" : "Archive " + id, "", "", false});
            out.push_back({"sched_act:" + id + ":delete", "Delete " + id + " for good", told, "", false});
        }
        return out;
    }
    if (cluster::lower(wanted).starts_with("archive:")) {
        for (const cluster::Task& t: manager.tasks())
            if (t.archived) out.push_back({"task_menu:" + t.id, "Task " + t.id + " (" + t.status + ") · " + cluster::one_line(t.prompt, 70), "", "", false});
        for (const cluster::Schedule& w: manager.schedules())
            if (w.archived) out.push_back({"sched_menu:" + w.id, "Schedule " + w.id + " (" + w.status + ") · " + cluster::one_line(w.text, 70), "", "", false});
        if (out.empty()) out.push_back({"cancel", "Nothing archived", "", "", false});
        return out;
    }
    const bool menu = cluster::lower(wanted).starts_with("menu:");
    const bool remove = cluster::lower(wanted).starts_with("delete:");
    if (!menu && !remove) return out;
    const std::string id = cluster::trim(wanted.substr(menu ? 5 : 7));
    const std::optional<cluster::Snapshot> snap = manager.snapshot(id);
    if (!snap) return out;
    const std::string name = snap->spec.name;
    if (remove) {
        out.push_back({"delete_session:" + id, "Yes: delete " + name + " for good (its conversation gone, not in the trash)", "", "", false});
        out.push_back({"cancel", "No: keep " + name, "", "", false});
        return out;
    }
    out.push_back({"focus:" + id, "Open " + name, "", "", false});
    if (snap->spec.global) return out;
    static const std::vector<std::string> kept = {"rename", "describe", "cd", "resume", "where_folder", "where_terminal", "close_session"};
    for (const cluster::Action& a: cluster::actions(manager, id))
        if (std::find(kept.begin(), kept.end(), a.id.substr(0, a.id.find(':'))) != kept.end()) {
            cluster::Action shown = a;
            if (a.id == "close_session") shown.title = "Archive " + name + ": closed, to the trash (restorable " + std::to_string(manager.config().trashDays) + " days)";
            out.push_back(shown);
        }
    out.push_back({"delete_ask:" + id, "Delete " + name + " for good… (asked first)", "", "", false});
    return out;
}

_cold std::vector<cluster::Action> cluster::filter_actions(const std::vector<cluster::Action>& actions, const std::string& filter, cluster::Manager* manager)
{
    // the palette: the actions matching, the closest first (no filter: their order); a manager: the lists made for a filter
    // (new:, menu:<id>, delete:<id>) and the settings found too (after the actions as close; setting: alone: only them),
    // Enter opens the settings page at it
    if (manager != nullptr)
        if (std::vector<cluster::Action> made = generated_(*manager, cluster::trim(filter)); !made.empty()) return made;
    const cluster::Config* config = manager != nullptr ? &manager->config() : nullptr;
    std::vector<std::pair<float, cluster::Action>> found;
    std::string wanted = cluster::trim(filter);
    const bool settingsOnly = cluster::lower(wanted).starts_with("setting:");
    if (!settingsOnly)
        for (const cluster::Action& action: actions)
            if (const std::optional<float> score = cluster::action_score(action, filter)) found.emplace_back(*score, action);
    if (settingsOnly) wanted = cluster::trim(wanted.substr(8));
    const std::size_t colon = wanted.find(':');
    if (config != nullptr && !wanted.empty() && (settingsOnly || colon == std::string::npos || wanted.find(' ') < colon)) {
        const std::vector<cluster::SettingsPage> pages = cluster::settings_pages(*config);
        for (const cluster::SettingHit& hit: cluster::settings_search(pages, wanted)) {
            const cluster::Setting& s = pages[hit.page].settings[hit.item];
            found.emplace_back(hit.score + 2.0F, cluster::Action{"setting:" + s.section + "." + s.key, "⚙ Setting: " + s.label + "  · " + pages[hit.page].title
                + (hit.name ? "" : "  (found in its description)"), "", "", false});
        }
    }
    std::stable_sort(found.begin(), found.end(), [](const std::pair<float, cluster::Action>& a, const std::pair<float, cluster::Action>& b) {return a.first < b.first;});
    std::vector<cluster::Action> out;
    for (const auto &[score, action]: found)
        out.push_back(action);
    return out;
}

_cold std::optional<float> cluster::action_score(const cluster::Action& action, const std::string& filter)
{
    // a prefix ("apply:", "mode:"): the actions of that kind, the rest matched in their title or id; else the words of the
    // title / the id ([ui] matching)
    const std::string wanted = cluster::trim(cluster::lower(filter));
    if (wanted.empty()) return 0.0F;
    const std::size_t colon = wanted.find(':');
    if (colon != std::string::npos && colon > 0 && wanted.find(' ') > colon) {
        // a kind: its actions first; another action only when it holds the text as typed
        if (!cluster::lower(action.id).starts_with(wanted.substr(0, colon + 1))) {
            if (cluster::lower(action.title).find(wanted) == std::string::npos && cluster::lower(action.id).find(wanted) == std::string::npos) return std::nullopt;
            return 0.0F;
        }
        const std::string rest = cluster::trim(wanted.substr(colon + 1));
        if (rest.empty()) return -1000.0F;
        const std::optional<float> t = cluster::match_words(rest, action.title);
        const std::optional<float> i = cluster::match_words(rest, action.id);
        if (!t && !i) return std::nullopt;
        return std::min(t.value_or(1e9F), i.value_or(1e9F)) - 1000.0F;
    }
    const std::optional<float> t = cluster::match_words(wanted, action.title);
    const std::optional<float> i = cluster::match_words(wanted, action.id);
    if (!t && !i) return std::nullopt;
    return std::min(t.value_or(1e9F), i.value_or(1e9F));
}

_cold bool cluster::action_matches(const cluster::Action& action, const std::string& filter)
{
    // a prefix ("apply:", "mode:"): the actions of that kind, the rest found in their title or id; else anywhere
    const std::string wanted = cluster::lower(filter);
    const std::string title = cluster::lower(action.title);
    const std::string id = cluster::lower(action.id);
    if (wanted.empty()) return true;
    const std::size_t colon = wanted.find(':');
    if (colon != std::string::npos && colon > 0 && wanted.find(' ') > colon) {
        const std::string prefix = wanted.substr(0, colon + 1);
        const std::string rest = cluster::trim(wanted.substr(colon + 1));
        if (id.starts_with(prefix)) return rest.empty() || title.find(rest) != std::string::npos || id.find(rest) != std::string::npos;
    }
    return title.find(wanted) != std::string::npos || id.find(wanted) != std::string::npos;
}

_cold cluster::ActionResult cluster::tasks_prompt(cluster::Manager& manager, const std::string& line)
{
    // the prompt of the Tasks session: "<when> <text>" a reminder to the global session (+30m, +2h, 18:00...), "<target> |
    // <prompt>" a task for it, else a task for any free session
    cluster::ActionResult result;
    const std::string text = cluster::trim(line);
    const std::size_t space = text.find(' ');
    const std::string first = text.substr(0, space);
    const bool when = space != std::string::npos && (std::regex_match(first, std::regex(R"(\+\d+[smhd])")) || std::regex_match(first, std::regex(R"(\d{1,2}:\d{2})")));
    if (when) {
        // a prompt to the global session; none (off): a notification
        const bool global = manager.snapshot(GLOBAL_ID).has_value();
        const std::string id = manager.addSchedule(first, global ? "prompt" : "notify", GLOBAL_ID, cluster::trim(text.substr(space + 1)), 0);
        result.message = "schedule " + id + " at " + first + (global ? ": a prompt to the global session" : ": a notification (no global session)");
        return result;
    }
    const std::size_t bar = text.find(" | ");
    const std::string id = bar == std::string::npos ? manager.addTask(text, "", {}) : manager.addTask(cluster::trim(text.substr(bar + 3)), cluster::trim(text.substr(0, bar)), {});
    result.message = "task " + id + " queued" + (bar == std::string::npos ? " (any free session)" : " for " + cluster::trim(text.substr(0, bar)));
    return result;
}
