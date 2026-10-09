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
#include "cluster/Actions.hpp"
#include "cluster/Tools.hpp"
#include <fstream>

/* actions */
_cold std::vector<cluster::Action> cluster::actions(cluster::Manager& manager, const std::string& session)
{
    const cluster::Config& config = manager.config();
    const std::optional<cluster::Snapshot> snap = session.empty() ? std::nullopt : manager.snapshot(session);
    std::vector<cluster::Action> list = {
        {"new_session", "New session: folder [backend]", "folder [backend]", ".", false},
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
        for (const auto &[mode, description]: cluster::permission_modes())
            list.push_back({"mode:" + mode, "Permission mode: " + mode + " - " + description + (snap->spec.mode == mode ? " (current)" : ""), "", "", false});
        for (const char* panel: {"skills", "tokens", "context", "cost", "git", "diff", "tools", "files"})
            list.push_back({std::string("panel:") + panel, std::string("Panel ") + panel + (snap->spec.panels.contains(panel) ? ": hide" : ": show"), "", "", false});
        if (!snap->spec.global) {
            list.push_back({"cd", "Move the session to another folder", "folder", snap->spec.cwd, false});
            list.push_back({"close_session", "Close the session (to the trash)", "", "", false});
        } else {
            list.push_back({manager.remote() ? "remote_off" : "remote_on", manager.remote() ? "Remote Control: off (back here)" : "Remote Control: on (claude.ai / app)", "", "", false});
        }
        list.push_back({"export", "Export the conversation to Markdown", "file", "~/" + snap->spec.name + ".md", false});
    }
    for (const cluster::SessionSpec& spec: manager.trash()) {
        list.push_back({"restore:" + spec.id, "Restore " + spec.name + " (" + cluster::short_path(spec.cwd) + ", closed " + cluster::human_age(spec.deleted) + " ago)", "", "", false});
        list.push_back({"purge:" + spec.id, "Delete for good " + spec.name + " (trash: conversation and history)", "", "", false});
    }
    if (!manager.trash().empty())
        list.push_back({"purge_all", "Empty the whole trash (" + std::to_string(manager.trash().size()) + " session(s), for good)", "type yes to confirm", "", false});
    for (const auto &[name, profile]: config.profiles)
        list.push_back({"profile:" + name, "New session from the profile " + name, "", "", false});
    list.push_back({"task_add", "Queue a task: [target |] prompt", "task", "", false});
    list.push_back({"search", "Search in the history", "text", "", false});
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
    list.push_back({"voice_toggle", "Voice: listen / stop (push-to-talk)", "", "", false});
    list.push_back({"voice_confirm", "Voice: send the transcription now", "", "", false});
    list.push_back({"voice_cancel", "Voice: drop the transcription", "", "", false});
    list.push_back({"voice_edit", "Voice: correct the transcription", "text", "", false});
    list.push_back({"mute", "Voice: stop speaking", "", "", false});
    list.push_back({"voice_setup", "Voice: what is missing to set it up", "", "", false});
    for (const char* layout: {"list", "grid", "tabs"})
        list.push_back({std::string("layout:") + layout, std::string("Layout: ") + layout, "", "", true});
    for (const auto &[name, style]: config.styles)
        list.push_back({"style:" + name, "Style: " + name + (style.dark ? " (dark)" : " (light)"), "", "", true});
    list.push_back({"purge", "Empty the expired sessions of the trash", "", "", false});
    list.push_back({"config", "Open the config file (path)", "", "", false});
    list.push_back({"quit", "Quit claude-cluster (sessions kept for the next run)", "", "", true});
    return list;
}

_cold cluster::ActionResult cluster::run_action(cluster::Manager& manager, cluster::Voice& voice, const std::string& id,
    const std::string& session, const std::string& input)
{
    const std::string arg = id.find(':') == std::string::npos ? "" : id.substr(id.find(':') + 1);
    const std::string base = id.substr(0, id.find(':'));
    cluster::ActionResult result;

    try {
        if (base == "new_session") {
            const std::vector<std::string> parts = cluster::split(input, ' ');
            result.focus = manager.spawn({parts.empty() ? "." : parts[0], "", parts.size() > 1 ? parts[1] : "", "", "", ""});
            result.message = "session " + result.focus + " started";
        } else if (base == "send") {
            manager.send(session, input);
        } else if (base == "allow" || base == "always" || base == "deny") {
            manager.answer(session, "", base);
            result.message = base == "deny" ? "denied" : "allowed";
        } else if (base == "interrupt") {
            manager.interrupt(session);
        } else if (base == "rename") {
            manager.rename(session, input);
        } else if (base == "mode") {
            manager.setMode(session, arg);
            result.message = "mode " + arg;
        } else if (base == "panel") {
            manager.togglePanel(session, arg);
        } else if (base == "cd") {
            manager.cd(session, input);
        } else if (base == "close_session") {
            manager.close(session);
            result.message = "closed (restorable from the palette)";
            result.focus = GLOBAL_ID;
        } else if (base == "remote_on" || base == "remote_off") {
            manager.setRemote(base == "remote_on");
        } else if (base == "export") {
            std::ofstream(cluster::expand_home(input)) << manager.exportMarkdown(session);
            result.message = "exported to " + cluster::expand_home(input).string();
        } else if (base == "restore") {
            result.focus = manager.restore(arg);
            result.message = "restored";
        } else if (base == "profile") {
            result.focus = manager.spawn({"", "", "", "", arg, ""});
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
        } else if (base == "purge_all") {
            if (cluster::lower(cluster::trim(input)) != "yes") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not confirmed: the trash is kept");
            manager.purge("*");
            result.message = "trash emptied";
        } else if (base == "config") {
            result.message = manager.config().path().string() + " (reloaded when saved), system prompt: " + manager.config().globalPrompt;
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

_cold std::string cluster::key_action(const cluster::Config& config, const std::string& key)
{
    for (const auto &[action, bound]: config.keys)
        if (cluster::lower(bound) == cluster::lower(key)) return action;
    return "";
}
