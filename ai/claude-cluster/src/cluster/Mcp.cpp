/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Mcp.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Orchestrator.hpp"
#include "cluster/Display.hpp"
#include "cluster/Control.hpp"
#include "cluster/Mcp.hpp"
#include <iostream>
#include <cstdlib>
#include <string>

/* tools */
_cold static cluster::Json tool_(const std::string& name, const std::string& description, const cluster::Json& properties, const std::vector<std::string>& required)
{
    return {{"name", name}, {"description", description},
        {"inputSchema", {{"type", "object"}, {"properties", properties.is_null() ? cluster::Json::object() : properties}, {"required", required}}}};
}

_cold static cluster::Json str_(const std::string& description)
{
    return {{"type", "string"}, {"description", description}};
}

_cold static cluster::Json int_(const std::string& description)
{
    return {{"type", "integer"}, {"description", description}};
}

_cold static cluster::Json bool_(const std::string& description)
{
    return {{"type", "boolean"}, {"description", description}};
}

_cold static cluster::Json obj_(const std::string& description)
{
    return {{"type", "object"}, {"description", description}};
}

_cold static cluster::Json strs_(const std::string& description)
{
    return {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", description}};
}

/* mcp */
_cold cluster::Json cluster::mcp_tools(void)
{
    const cluster::Json session = str_("id (s1, g) or name of the session");
    cluster::Json tools = cluster::Json::array({
        tool_("session_list", "Every session of the cluster: id, name, project folder, backend, state, category, tags, permission mode, tokens, context, cost, pending permissions. "
            "Filters: category, tag (one or several, all required), under (a node of the taxonomy: every session below it), state (open: the live ones, default | "
            "archived: the trash | all | working, idle...). A category / tag also matches the sessions under the labels below it in the taxonomy (descendants: false: itself only).",
            {{"category", str_("only this category (and the ones below it in the taxonomy)")}, {"tag", str_("only the sessions with this tag (several: separated by commas; the tags below each included)")},
             {"under", str_("every session under this category / tag node of the taxonomy")}, {"descendants", bool_("false: category / tag match themselves only, not the labels below them (default true)")},
             {"state", str_("open (default) | archived (the trash) | all | a state: working, idle, waiting...")}}, {}),
        tool_("session_status", "Details of one session and its last 20 messages (skills loaded, files modified).", {{"session", session}}, {"session"}),
        tool_("session_read", "Same as session_status: the last messages of a session.", {{"session", session}}, {"session"}),
        tool_("session_spawn", "Start a new session in a project folder. backend: <provider>[/<model>] (claude, ollama/<model>, openai/<model>, opencode/<provider>/<model>, codex...). "
            "For a one-off task set one_shot: true, the session is then archived by itself when it is done.",
            {{"cwd", str_("project folder (~ allowed), or the name of a project of machine.md (its git repositories: the closest by its name)")}, {"name", str_("name of the session (default: the folder name)")}, {"backend", str_("provider[/model], default from the config")},
             {"mode", str_("auto | manual | acceptEdits | plan | dontAsk | bypassPermissions")}, {"profile", str_("profile of the config")}, {"prompt", str_("first prompt")}, {"reply", {{"type", "boolean"}, {"description", "the answer to the first prompt (and later wake-up turns) sent back to you as \"[reply from ...]\" (default true)"}}},
             {"one_shot", {{"type", "boolean"}, {"description", "this session is for ONE precise task, never to be reused: closed to the trash (restorable) once its answer is relayed to you (needs reply, and the setting Archive one-shot sessions on; kept when the user typed in it, looks at it or it waits for them). Never for the session of a project you will talk to again (default false)"}}},
             {"effort", str_("thinking effort: default | low | medium | high | xhigh | max")}}, {"cwd"}),
        tool_("session_send", "Send a prompt to a session (queued when it is working). By default (reply: true) its answer comes back to you as a message "
            "\"[reply from <name> (<id>)] ...\" when it is done: go on with something else or end your turn, never poll it with session_read / session_status. "
            "A local command of claude-cluster (/clear, /model <m>, /mode <m>, /rename, /cd, /close, /cost, /context, /export...) runs at once, as typed "
            "in its prompt (no reply): /clear starts a new conversation for the session.",
            {{"session", session}, {"text", str_("the prompt")}, {"reply", {{"type", "boolean"}, {"description", "its answer sent back to you when done (default true)"}}},
             {"attachments", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "paths of local files to attach to the prompt (images as pictures, the others as @path): "
                 "inside the project folder of the session or the home of the user only, no hidden folder; the number is limited by a setting"}}}},
            {"session", "text"}),
        tool_("session_wait", "Wait until a session is done (up to timeout seconds, 110 max) and get only its last answer (not its context); its outcome tells how its "
            "last turn ended - finished, cancelled (interrupted before it finished), error, closed: only finished is done, never take the others as finished: when you need "
            "the answer now to go on. A permission waiting is told.", {{"session", session}, {"timeout", {{"type", "integer"}, {"description", "seconds (default 100)"}}}},
            {"session"}),
        tool_("session_allow", "Answer a pending permission of a session; a question of its agent (AskUserQuestion, its options in "
            "session_status): answers {question text: the label chosen, several joined by \", \"} (behavior deny: declined).",
            {{"session", session}, {"request_id", str_("request id (empty: the oldest one)")}, {"behavior", str_("allow | always | deny")},
             {"answers", {{"type", "object"}, {"description", "AskUserQuestion: {question text: answer}"}}}}, {"session"}),
        tool_("session_set_mode", "Permission mode of a session.", {{"session", session}, {"mode", str_("auto | manual | acceptEdits | plan | dontAsk | bypassPermissions")}}, {"session", "mode"}),
        tool_("models_policy", "The model / thinking policy of the user: the default model and effort, whether they are advised or forced, the allowed ones, "
            "the costly ones (the user is asked first), and the model of the global session. Read it before choosing the model of a sub-session.", {}, {}),
        tool_("session_set_model", "Model and / or thinking effort of a session (the same conversation goes on); none given: its current ones and the "
            "choices of its backend. Under the policy of the user (models_policy): a forbidden one is refused with the reason; a costly one (opus, xhigh...) "
            "returns {asked}: the user answers Allow / Deny in their interface and the result comes back to you as a message.", {{"session", session}, {"model", str_("a model of its provider (opus, sonnet, claude-opus-5-5, an Ollama model...)")},
            {"effort", str_("default | low | medium | high | xhigh | max (claude); minimal... (codex, opencode)")}}, {"session"}),
        tool_("work_method", "The work method of the user (how they like the work organised: a task list then a review repeated until good, one task at a time, "
            "plan first, free): read it (default) to split the requests accordingly. Only the user changes the setting. Detection (when the user turned it on; a spawn "
            "result tells work_method_due): action sample = counts and a few of their prompts read from the transcripts (no model; cheap, once), then action propose "
            "(preset, text, reason in one sentence) ONLY when their habits clearly differ from the current method - the user answers Allow / Deny in a popup, the "
            "answer comes back to you as a message; never apply or repeat a refused one.",
            {{"action", {{"type", "string"}, {"enum", {"read", "sample", "propose"}}, {"description", "read (default) | sample | propose"}}},
             {"preset", str_("propose: free | list-review | one-task | plan-first | custom")}, {"text", str_("propose: the user's words added to the preset (short; alone with custom)")},
             {"reason", str_("propose: why, one sentence shown to the user")}, {"max", {{"type", "integer"}, {"description", "sample: prompts shown (10, 20 at most)"}}}}, {}),
        tool_("usage", "Tokens and cost used per vendor (anthropic, ollama, openai...) over every session: today, the last 7 days, lifetime; "
            "the 5 h / 7 d limits left when the vendor tells them.", {{"vendor", str_("one vendor (empty: all)")}}, {}),
        tool_("cluster_info", "Everything about this claude-cluster in one call, instead of searching its files: version, binary, the paths "
            "of its files (config, logs, history, usage, prompts, vocabulary, voice words, map of this computer), the global session, the "
            "agents and their commands, the voice (speech model, models installed, what is missing), the service, the interface, counts.", {}, {}),
        tool_("config_files", "The editable files of claude-cluster: your own system prompt, the one of the sub-sessions, the vocabulary and the "
            "words of the voice, config.toml, the map of this computer (read only); their path, what they hold, when a change is taken.", {}, {}),
        tool_("config_read", "The content of one of them (config_files).", {{"name", str_("global_prompt | sessions_prompt | vocabulary | voice_words | config | machine_map")}}, {"name"}),
        tool_("config_write", "Replace one of them with this content: read it first (config_read) and keep what the user did not ask to change. "
            "config is checked (TOML) before it is written, its protected settings stay the user's; the system prompts only when the user allowed it "
            "(settings: Global session can rewrite its own system prompt / the system prompt of the sub-sessions, one each), else give them the text; your own system prompt: you are started again on it after this turn.",
            {{"name", str_("global_prompt | sessions_prompt | vocabulary | voice_words | config")}, {"content", str_("the whole new content")}}, {"name", "content"}),
        tool_("machine_sync", "Ask the user to map this computer again (when asked to, or the map is clearly outdated): its question is shown "
            "in the interface (yes / no), the scan then runs as started by hand.", {}, {}),
        tool_("session_interrupt", "Stop the running turn of a session.", {{"session", session}}, {"session"}),
        tool_("session_close", "Close a session: it goes to the trash, restorable with session_restore.", {{"session", session}}, {"session"}),
        tool_("session_restore", "Restore a closed session from the trash (with its conversation).", {{"session", str_("id or name in the trash")}}, {"session"}),
        tool_("session_trash", "Closed sessions that can still be restored.", nullptr, {}),
        tool_("session_purge", "Delete for good a session of the trash (its conversation and history), \"*\" for the whole trash. Irreversible: only when the user asked it.",
            {{"session", str_("id or name in the trash, or *")}}, {"session"}),
        tool_("session_rename", "Rename a session.", {{"session", session}, {"name", str_("new name")}}, {"session", "name"}),
        tool_("session_continue", "A session goes on where it stopped (interrupted, an error of the API or of the connection): \"continue\" sent in "
            "the same conversation; a turn waiting for a retry of the API: stopped and asked again at once.", {{"session", session}}, {"session"}),
        tool_("taxonomy", "The tree of the categories and tags of the sessions: each node (a category or a tag) has an optional parent, none: it hangs under the root \"global\"; "
            "a session hangs under its category, else its first tag. The graph and the grouped list show it nested, session_list filters work on the descendants. "
            "action list (default): {parents: {node: parent}, labels: {label: sessions}}; set_parent (node, parent; none or global: the root; a cycle is refused); "
            "rename (node, to: also on every session); merge (node, to: the node it joins); delete (node: its children go to its parent; strip: the label also removed from "
            "the sessions). Build it as the sessions come: group the tags you reuse (api, ui, db) under one broader node (backend, frontend), not one node per session. "
            "Only when the user allowed it (settings: Global session can edit the taxonomy); they edit it by hand too.",
            {{"action", {{"type", "string"}, {"enum", {"list", "set_parent", "rename", "merge", "delete"}}, {"description", "default list"}}},
             {"node", str_("the category / tag to change")}, {"parent", str_("set_parent: its parent node (none or global: under the root)")},
             {"to", str_("rename: the new name; merge: the node it joins")}, {"strip", bool_("delete: also remove the label from the sessions (default false)")}}, {}),
        tool_("session_describe", "Give a session its short description (a few words: what it works on, holds; shown under its name, the trash too, to find it "
            "again), its category (ONE word: bugfix, feature, docs, refactor, research, ops...) and its tags (several short words: the topic, the project, the "
            "kind of work), the way to find and group sessions (session_list filters, search \"#tag\"). Set them when you spawn or route a session and keep them "
            "true as its topic moves; reuse the ones already in session_list. A text given: kept; empty: claude-cluster writes it again; no text: untouched. "
            "category \"\" removes it; tags replaces all of them, add_tags / remove_tags change some. The user can edit them too; no model is called.",
            {{"session", session}, {"text", str_("a few words, empty: written again; leave out to keep it")}, {"category", str_("one word, \"\" removes it")},
             {"tags", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "all its tags (replaces them)"}}},
             {"add_tags", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "tags to add"}}},
             {"remove_tags", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "tags to remove"}}}}, {"session"}),
        tool_("session_cd", "Move a session to another project folder (a new conversation there, same name).", {{"session", session}, {"cwd", str_("folder")}}, {"session", "cwd"}),
        tool_("task_add", "Queue a task for a free session (target = session id / name, a project folder, or empty: any idle one). Runs when its triggers are met "
            "(after: task ids; after_sessions: sessions idle; on_event) and a place is free (max_parallel). The result (its answer, capped) stays in task_list; "
            "\"{{t3.result}}\" in a prompt is replaced by the result of t3 (a chain: t3 runs first). A task is cheaper than a session: no new context.",
            {{"prompt", str_("the task")}, {"target", str_("session or folder (optional)")}, {"after", strs_("task ids to finish first")},
             {"after_sessions", strs_("sessions (id / name) that must be idle first")},
             {"on_event", obj_("wait for {type: session_idle (session) | git_push (repo, branch: a commit pushed) | command (command; met when it exits 0, or code: N; and its output: stdout exactly / contains / regex; every: seconds or 'constantly' (default 30); fail_after: N failures in a row = the task fails and you are told, 1 = the first, 0 = never) | file (path, changed: true = modified, not only present)}")},
             {"spawn", obj_("its own session instead of looking for one: {cwd, name, backend, mode, effort, profile, one_shot (closed to the trash after it)}")},
             {"priority", int_("higher first (default 0)")}, {"timeout_minutes", int_("stopped when running longer")},
             {"retry", int_("tried again this many times after an error")}, {"retry_delay_minutes", int_("wait before a new try")},
             {"max_concurrency", int_("at most this many tasks of its lane running")}, {"lane", str_("the group max_concurrency counts in")},
             {"max_cost", {{"type", "number"}, {"description", "stopped when its session costs more (USD)"}}}, {"max_tokens", int_("stopped past this many tokens")},
             {"expect", str_("output asked: compact, or a file path it writes (the result is then the file)")}, {"notify", bool_("a desktop notification at its end")},
             {"reply", bool_("its end told to you as a message (default true)")}}, {"prompt"}),
        tool_("task_list", "Tasks of the queue and their status, result, retries (a failed one: its reason in \"error\"; the finished ones are archived after 30 minutes: archived: true lists them too).",
            {{"archived", bool_("also the archived tasks (default false)")}, {"status", str_("only this status: queued | running | done | failed | cancelled")}, {"id", str_("one task")}}, {}),
        tool_("session_fanout", "A request on several sessions / projects at once (a group): a task each, sent as many at once as the parallel sessions "
            "allow (the others wait in the task queue); you are woken ONCE with their answers as [group fN ...] to merge them - end your turn after it, "
            "never poll. tasks: [{session or cwd, prompt (else the common prompt), after: [indexes of tasks of this group it builds on: their answers "
            "given to it]}]; match: {under: folder, has: a file marking a project (default: a git repository), name: part of its name, max} for every "
            "project found (its session reused, else created: shown to the user first, then confirmed: true); wait: {mode: all | first | count, count, "
            "timeout_minutes (then woken with what came), stop_others (the wait met: the others stopped)}; retry (a failed task tried once more, "
            "default true); merge: here (you) | session (a big merge by a session of its own, its answer to you); table: a comparison table asked too.",
            {{"label", str_("what the group does, a few words")}, {"prompt", str_("the common prompt")},
                {"tasks", {{"type", "array"}, {"items", {{"type", "object"}}}, {"description", "[{session | cwd, prompt, after: [index]}]"}}},
                {"match", {{"type", "object"}, {"description", "{under, has, name, max}"}}},
                {"wait", {{"type", "object"}, {"description", "{mode: all | first | count, count, timeout_minutes, stop_others}"}}},
                {"retry", {{"type", "boolean"}}}, {"merge", str_("here | session")}, {"table", {{"type", "boolean"}}},
                {"confirmed", {{"type", "boolean"}, {"description", "the user agreed to the projects found (match)"}}}}, {"label"}),
        tool_("group_status", "The progress of a group (its tasks, their state and answers so far); none: every group of this run.", {{"group", str_("fN")}}, {}),
        tool_("group_cancel", "Stop a group: its tasks not started are cancelled, the running ones interrupted when interrupt is true.",
            {{"group", str_("fN")}, {"interrupt", {{"type", "boolean"}}}}, {"group"}),
        tool_("task_act", "Act on a task: cancel | delete | archive | unarchive | force (started now, past the limit of parallel sessions) | retry (a failed one again) | "
            "set (change priority, timeout_minutes, retry, max_cost, max_tokens, notify, prompt of one not ended).",
            {{"id", str_("task id (task_list)")}, {"action", str_("cancel | delete | archive | unarchive | force | retry | set")}, {"reason", str_("why (optional)")},
             {"priority", int_("set")}, {"timeout_minutes", int_("set")}, {"retry", int_("set")}, {"max_cost", {{"type", "number"}}}, {"max_tokens", int_("set")}, {"notify", bool_("set")}}, {"id", "action"}),
        tool_("schedule_add", "Do something at a time or on a rule, while claude-cluster runs. action: prompt (default: to session, you by default - a wake up / reminder "
            "\"[scheduled wN] text\") | notify (desktop) | spawn (a session, options.cwd, text = its prompt) | task (queue a task with options) | command "
            "(options.command run locally, NO model: only its output, when not empty, is sent to session / notified - 0 token). When: when (+30m, HH:MM, YYYY-MM-DD HH:MM) "
            "once, every (minutes), cron (\"min hour day month weekday\"), reset (5h | 7d: at each reset of that limit window); limited by days (mon-fri), window "
            "(\"09:00-18:00\"), quiet (\"22:00-07:00\"), tz (Europe/Paris), until, max_runs. Condition if: {session_idle, command (exits 0), file_changed, usage: \"5h>80\" | \"7d<30\"}; "
            "not met: that run is skipped. ack: asked again every nag minutes until schedule_act ack. dry_run: only shows when it would fire.",
            {{"when", str_("+30m, +2h, +1d, HH:MM (today, else tomorrow), YYYY-MM-DD HH:MM (in tz)")}, {"text", str_("the prompt / the notification / the task")},
             {"action", str_("prompt (default) | notify | spawn | task | command")}, {"session", str_("the session of a prompt / of the output (default: g, you)")},
             {"every", int_("minutes between two (0 / missing: once)")}, {"cron", str_("min hour day-of-month month weekday: */15 9-17 * * mon-fri")},
             {"days", str_("only these days: mon-fri, mon,wed, weekdays, weekend")}, {"window", str_("only inside: HH:MM-HH:MM[,HH:MM-HH:MM]")}, {"quiet", str_("never inside: HH:MM-HH:MM")},
             {"tz", str_("timezone of the above (default: this computer's)")}, {"until", str_("ended after this time (when's formats)")}, {"max_runs", int_("ended after this many runs")},
             {"reset", str_("5h | 7d")}, {"ack", bool_("asked again until acknowledged")}, {"nag", int_("minutes between asks (default 10)")},
             {"if", obj_("{session_idle, command (+ code, stdout / contains / regex: what it must return; fail_after: failures in a row before the schedule is cancelled), file_changed, usage}")},
             {"options", obj_("spawn: {cwd, name, backend, mode, effort, profile, one_shot}; task: the options of task_add (target, priority, retry...); command: {command, output: prompt | notify, cwd, timeout_seconds}")},
             {"dry_run", bool_("nothing made: the next times it would fire")}}, {"text"}),
        tool_("schedule_list", "The schedules: id, status, next time and the next ones, rules, conditions, runs, skipped, last runs (history), note.",
            {{"archived", bool_("also the archived")}, {"id", str_("one schedule")}}, {}),
        tool_("schedule_edit", "Change a schedule not ended: any field of schedule_add (when, text, every, cron, days, window, quiet, tz, until, max_runs, if, options...), pause: true / false.",
            {{"id", str_("schedule id (w1...)")}, {"when", str_("a new time")}, {"text", str_("")}, {"every", int_("minutes")}, {"cron", str_("")}, {"days", str_("")}, {"window", str_("")}, {"quiet", str_("")},
             {"tz", str_("")}, {"until", str_("")}, {"max_runs", int_("")}, {"pause", bool_("paused / resumed")}, {"if", obj_("")}, {"options", obj_("")}}, {"id"}),
        tool_("schedule_act", "Act on a schedule: pause | resume | snooze (minutes: pushed back, default 30) | ack (stops the asks of an ack reminder) | trigger (run now, its conditions skipped) | cancel | delete | archive | unarchive.",
            {{"id", str_("schedule id")}, {"action", str_("pause | resume | snooze | ack | trigger | cancel | delete | archive | unarchive")}, {"minutes", int_("snooze")}, {"reason", str_("why (optional)")}}, {"id", "action"}),
        tool_("notify", "A desktop notification to the user now (something done, something needing them); off when the user turned them off.",
            {{"text", str_("the message")}, {"title", str_("its title (default: claude-cluster)")}, {"urgent", {{"type", "boolean"}, {"description", "stays on screen"}}}}, {"text"}),
        tool_("schedule_cancel", "Cancel a schedule.", {{"id", str_("schedule id (w1...)")}}, {"id"}),
        tool_("task_cancel", "Cancel a task.", {{"id", str_("task id (t1...)")}}, {"id"}),
        tool_("search", "Full-text search in the history of every session, and in their categories and tags (\"#bugfix\": the sessions with that category or tag only).", {{"text", str_("text to find")}}, {"text"}),
        tool_("export", "Export the conversation of a session as Markdown (to a file when path is given: a .md / .txt out of the hidden folders).",
            {{"session", session}, {"path", str_("file (optional)")}}, {"session"}),
        tool_("providers", "Providers / backends and whether they are ready (logged in, key stored, server up).", nullptr, {}),
        tool_("web_access", "The web access of claude-cluster on this LAN (a browser of the same network drives it after a 6-digit code, one at a "
            "time): its address, code and the browser connected; action on / off (this run), kick (the browser out, a new code), pin (a new code). "
            "Give the user the address and the code when they ask to open it.",
            {{"action", {{"type", "string"}, {"enum", {"", "on", "off", "kick", "pin"}}, {"description", "empty: the state only"}}}}, {}),
        tool_("external_conversations", "The conversations of the agents of this computer opened outside claude-cluster (Claude Code, Codex, opencode, "
            "Qwen Code): agent, id, title, folder, first prompt, age; the newest first. To open one here: session_import.",
            {{"agent", str_("claude | codex | opencode | qwen (empty: all)")}, {"limit", {{"type", "integer"}, {"description", "30 by default"}}}}, {}),
        tool_("session_import", "Open a conversation of another agent (external_conversations) as a sub-session resuming it: its transcript shown, the "
            "same conversation goes on here. fork (Claude Code): a copy, the original untouched - use it when it may still be open elsewhere.",
            {{"agent", str_("claude | codex | opencode | qwen")}, {"id", str_("its id (a unique prefix is enough)")}, {"name", str_("name of the session")},
             {"fork", {{"type", "boolean"}, {"description", "a copy (Claude Code)"}}}}, {"agent", "id"}),
        tool_("session_fork", "A copy of a sub-session of Claude Code: a new sub-session with its conversation so far (its context), each one goes on "
            "apart. Only when the user allowed it (settings: Global session can fork).", {{"session", session}, {"name", str_("name of the copy")}}, {"session"}),
        tool_("session_transcript", "The transcript of a session, whole entries (not cut like session_read): index, kind (user, assistant, tool, "
            "tool_result, system, display), text. from: the first index (negative: from the end, -50 by default), count (50, 500 max).",
            {{"session", session}, {"from", {{"type", "integer"}}}, {"count", {{"type", "integer"}}}}, {"session"}),
        tool_("session_context", "Give a context to a session as a prompt it keeps for its next requests (it answers \"ok\"): source = session:<id> "
            "(the conversation of another session), <agent>:<id> (a conversation of external_conversations), or a file path.",
            {{"session", session}, {"source", str_("session:<id> | claude:<id> | codex:<id> | qwen:<id> | a file")}}, {"session", "source"}),
        tool_("session_messages", "The real conversation of a sub-session of Claude Code (what its model sees): its messages with index, role, text, "
            "tools. To rewrite it: session_edit.", {{"session", session}}, {"session"}),
        tool_("session_edit", "Rewrite the conversation of a sub-session of Claude Code (indexes of session_messages): replace texts, delete messages "
            "(a tool call and its result go together), keep only the first N; written as a new conversation (the old one kept), the session "
            "started again on it, its transcript shown again. Only when it is idle and the user allowed it (settings: Global session can edit).",
            {{"session", session}, {"replace", {{"type", "array"}, {"description", "[{index, text}]"}}},
             {"delete", {{"type", "array"}, {"description", "[index, ...]"}}}, {"keep", {{"type", "integer"}, {"description", "keep the first N messages"}}}},
            {"session"}),
        tool_("session_display", "Show something in the transcript of a session (any sub-session, or g: yours) without a prompt: its agent "
            "never sees it, the user does (terminal, window, web page). display = {type, title?, ...}: " + cluster::display_help() + ". Or only "
            "text (markdown). For the user: a summary table, a chart of numbers, a diagram of an architecture, an image file, a drawing (SVG), "
            "a callout, a progress bar.", {{"session", session}, {"display", {{"type", "object"}, {"description", "{type, ...}"}}},
            {"text", str_("markdown (when no display)")}, {"title", str_("its title")}}, {"session"}),
        tool_("session_display_update", "Replace a display you put (its id, given back by session_display): the steps of a long work, a "
            "progress, a checklist, a table that changes. The user sees it change in place.",
            {{"session", session}, {"id", str_("its id")}, {"display", {{"type", "object"}, {"description", "{type, ...}"}}}}, {"session", "id", "display"}),
        tool_("session_display_get", "A display of a session as it is now: the items the user ticked (checklist), the buttons clicked "
            "(actions), the values sent (form, they also come to you as a message).", {{"session", session}, {"id", str_("its id")}}, {"session", "id"}),
        tool_("session_pin", "Pin something at the bottom of the view of a session (above its prompt, always visible whatever the scroll; one pin "
            "per session, a new one replaces it): a task list / checklist / steps / form the user needs to keep seeing, a table, a note. A live "
            "display stays live while pinned (session_display_update). Give display (the id of a display, or a block {type, ...} put in the "
            "transcript and pinned at once) or index (an entry of session_transcript: a display or a message). Only when the user allowed it "
            "(settings: Global session can pin); they can unpin by hand.",
            {{"session", session}, {"display", {{"description", "id of a display (session_display), or a block {type, ...}"}}},
             {"index", {{"type", "integer"}, {"description", "an entry of the transcript (negative: from the end)"}}}, {"title", str_("its title")}}, {"session"}),
        tool_("session_unpin", "Remove the pin of a session (its item stays in the transcript).", {{"session", session}}, {"session"}),
        tool_("session_pin_get", "What is pinned on a session: {kind, id, title, by}, or nothing.", {{"session", session}}, {"session"}),
        tool_("docs", "The documentation of claude-cluster itself (its sessions, tools, settings, keys, interfaces, security), embedded in the program: action search (default) "
            "finds the pages, settings, tools and keys that answer a question (typos are tolerated); page gives the page of a screen of the app or a page name; open shows it to the user "
            "in their browser. Use it to answer a question about how claude-cluster works instead of guessing.",
            {{"action", str_("search (default) | page | open")}, {"query", str_("what to look for (search)")}, {"page", str_("a page (settings, orchestrator, taxonomy...) or a screen of the app (settings, question, graph...) (page, open)")},
             {"lang", str_("en (default) | fr")}}, {}),
        tool_("voice_engines", "The engines of the voice: speech to text ([voice] listen_model) and text to speech ([voice] say_engine), "
            "local or free-tier APIs: what each one handles (languages), its strengths and limits against the others, its free use, its "
            "voices, its links, ready (installed / API key stored), used. To change one: settings_set; a key: the user runs "
            "claude-cluster auth login <provider>.", nullptr, {}),
        tool_("remote_control", "Remote Control of the global session (you): on, the user can go on with you from claude.ai / the Claude app (live both ways); off.",
            {{"on", {{"type", "boolean"}, {"description", "true: on, false: off"}}}}, {"on"}),
        tool_("ui_state", "What the interface of claude-cluster shows: the session shown, the layout, the style, Remote Control, vim; the choices.", nullptr, {}),
        tool_("ui_focus", "Show a session in the interface (a sub-session, or g: you again), while you go on talking.", {{"session", session}}, {"session"}),
        tool_("ui_layout", "Layout of the interface: list (the list + one session), grid (every session at once), tabs (window / web page only: the terminal shows the list).",
            {{"layout", str_("list | grid | tabs")}}, {"layout"}),
        tool_("ui_style", "Colors of the interface (ui_state: the styles).", {{"style", str_("a style name")}}, {"style"}),
        tool_("ui_panel", "Show / hide a panel of a session: skills, tokens, context, cost, git, diff, tools, files.",
            {{"session", session}, {"panel", str_("skills | tokens | context | cost | git | diff | tools | files")}}, {"session", "panel"}),
        tool_("settings_get", "The settings of claude-cluster (the settings page): section, key, value, choices, help. section: one section only.",
            {{"section", str_("ui | agent | commands | global | sessions | voice | service (optional)")}}, {}),
        tool_("settings_set", "Change one setting of claude-cluster (written in its config.toml, applied live except the ones marked at_next_start).",
            {{"section", str_("section of the option")}, {"key", str_("its key (settings_get)")}, {"value", str_("the new value (true / false, a number, a choice, a text)")},
                {"confirmed", str_("\"true\" only when the user explicitly asked for this value: a dangerous one is refused without it - ask the user first. "
                    "The protected ones (a shell or the web open wider, the permissions of the agents, the programs run, the MCP servers and tools) are "
                    "refused anyway: the user changes them in the settings page")}},
            {"section", "key", "value"}),
    });
    for (const cluster::Json& tool: cluster::orchestrator_tools()) // the heavy jobs (Orchestrator-Control.cpp)
        tools.push_back(tool);
    return tools;
}

_cold cluster::Json cluster::mcp_sub_tools(const std::string& session)
{
    // the tools of a sub-session: the ones the settings open to it, lighter (no local command, nothing it cannot use)
    cluster::Json allowed = cluster::Json::object();
    try {
        allowed = cluster::Control::request({{"cmd", "sub_tools"}, {"origin", "sub"}, {"owner", session}}).value("data", cluster::Json::object());
    } catch (const utils::exception::IException&) {
        return cluster::Json::array(); // claude-cluster not reached: nothing
    }
    const cluster::Json names = allowed.value("tools", cluster::Json::array());
    const bool spawn = allowed.value("spawn", false);
    cluster::Json out = cluster::Json::array();
    for (cluster::Json tool: mcp_tools()) {
        const std::string name = tool.value("name", std::string());
        if (std::find(names.begin(), names.end(), name) == names.end()) continue;
        cluster::Json& properties = tool["inputSchema"]["properties"];
        if (name == "task_add") {
            tool["description"] = "Queue a task for yourself: it runs after your turn (or when its triggers are met: after task ids, after_sessions idle, on_event session_idle / git_push / file) and its "
                "result comes back to you as a message. \"{{t3.result}}\" in a prompt is replaced by the result of t3. Cheaper than a new session: use it for a follow-up, a retry, "
                "something to do later; keep prompts short.";
            for (const char* key: {"target", "lane", "max_concurrency", "notify"})
                properties.erase(key);
            if (!spawn) properties.erase("spawn");
        } else if (name == "session_pin" || name == "session_unpin" || name == "session_pin_get") {
            properties.erase("session");   // its own view only
            tool["inputSchema"]["required"] = cluster::Json::array();
        } else if (name == "schedule_add" || name == "schedule_edit") {
            for (const char* key: {"session", "dry_run"})
                properties.erase(key);
            if (!spawn) properties.erase("options");
            if (name == "schedule_add") tool["description"] = "Wake yourself up (a prompt \"[scheduled wN] text\") or notify the user at a time or on a rule: when (+30m, HH:MM), every (minutes), cron, "
                "days / window / quiet / tz, until, max_runs, if {session_idle, file_changed, usage: \"5h>80\"}; ack: asked again until schedule_act ack. action: prompt (default) | notify"
                + std::string(spawn ? " | spawn | task (options)" : " | task") + ". Every wake-up costs tokens: few, spaced.";
        }
        out.push_back(tool);
    }
    return out;
}

_cold cluster::Json cluster::mcp_call(const std::string& name, const cluster::Json& arguments)
{
    static const std::map<std::string, std::string> commands = {
        {"session_list", "list"}, {"session_status", "status"}, {"session_read", "read"}, {"session_spawn", "spawn"}, {"session_send", "send"}, {"session_wait", "wait"},
        {"session_allow", "allow"}, {"session_set_mode", "mode"}, {"session_set_model", "model"}, {"models_policy", "models_policy"}, {"work_method", "work_method"}, {"usage", "usage"}, {"cluster_info", "cluster_info"}, {"config_files", "config_files"}, {"config_read", "config_read"}, {"config_write", "config_write"}, {"machine_sync", "machine_sync"}, {"session_interrupt", "interrupt"}, {"session_close", "close"},
        {"session_restore", "restore"}, {"session_trash", "trash"}, {"session_purge", "purge"}, {"session_rename", "rename"}, {"session_describe", "describe"}, {"taxonomy", "taxonomy"}, {"session_continue", "resume"}, {"session_fanout", "fanout"}, {"group_status", "group_status"}, {"group_cancel", "group_cancel"}, {"session_cd", "cd"},
        {"task_add", "task_add"}, {"task_list", "task_list"}, {"schedule_add", "schedule_add"}, {"schedule_list", "schedule_list"}, {"schedule_edit", "schedule_edit"}, {"schedule_act", "schedule_act"}, {"schedule_cancel", "schedule_cancel"}, {"notify", "notify"}, {"task_cancel", "task_cancel"}, {"task_act", "task_act"}, {"search", "search"}, {"export", "export"},
        {"providers", "providers"}, {"voice_engines", "voice_engines"}, {"docs", "docs"}, {"web_access", "web"},
        {"session_display", "display"}, {"session_display_update", "display_update"}, {"session_display_get", "display_get"}, {"session_pin", "pin"}, {"session_unpin", "unpin"}, {"session_pin_get", "pin_get"}, {"external_conversations", "external"}, {"session_import", "import"}, {"session_fork", "fork"}, {"session_transcript", "transcript"},
        {"session_context", "context"}, {"session_messages", "messages"}, {"session_edit", "edit"}, {"remote_control", "remote"}, {"ui_state", "ui_state"}, {"ui_focus", "ui_focus"}, {"ui_layout", "ui_layout"},
        {"ui_style", "ui_style"}, {"ui_panel", "ui_panel"}, {"settings_get", "settings_get"}, {"settings_set", "settings_set"},
    };
    auto it = commands.find(name);
    const std::string orchestrator = cluster::orchestrator_command(name); // heavy_run, jobs_*, job_*, hardware_scan, orchestrator_config
    if (it == commands.end() && orchestrator.empty()) return {{"content", {{{"type", "text"}, {"text", "unknown tool " + name}}}}, {"isError", true}};

    cluster::Json request = arguments.is_object() ? arguments : cluster::Json::object();
    request["cmd"] = orchestrator.empty() ? it->second : orchestrator;
    request["origin"] = "mcp"; // the global session: the limits the user set for it
    request["tool"] = name;    // off in the settings (MCP page): refused
    if (const char* sub = std::getenv("CLAUDE_CLUSTER_SESSION"); sub != nullptr && *sub != '\0') {
        request["origin"] = "sub"; // the MCP of a sub-session: its own tasks / schedules only (the core checks it)
        request["owner"] = sub;
    }
    if (name == "session_list") request["global"] = false; // the global session lists the others
    if (name == "session_send" && !request.contains("text")) { // the model names it like the prompt of session_spawn: never sent empty
        for (const char* alias: {"prompt", "message", "content"})
            if (request.contains(alias)) {
                request["text"] = request[alias];
                break;
            }
    }
    if (name == "session_send" && !request.contains("reply")) request["reply"] = true; // its answer comes back (no polling)
    if (name == "session_send") request["from"] = "global"; // marked for the sub-session: a compact answer, for a model
    cluster::Json response;
    try {
        response = cluster::Control::request(request);
    } catch (const utils::exception::IException& e) {
        return {{"content", {{{"type", "text"}, {"text", e.info()}}}}, {"isError", true}};
    }
    const bool ok = response.value("ok", false);
    // read by a model, not a human: compact JSON, its empty fields left out (like rtk: every token says something)
    const std::function<void(cluster::Json&)> prune = [&prune](cluster::Json& value) {
        if (value.is_object()) {
            for (auto it = value.begin(); it != value.end();) {
                prune(*it);
                const bool empty = it->is_null() || (it->is_string() && it->get_ref<const std::string&>().empty()) || ((it->is_array() || it->is_object()) && it->empty());
                it = empty ? value.erase(it) : std::next(it);
            }
        } else if (value.is_array()) {
            for (cluster::Json& item: value)
                prune(item);
        }
    };
    cluster::Json data = response.value("data", cluster::Json());
    prune(data);
    const std::string text = ok ? (data.is_null() || (data.is_object() && data.empty()) ? "done" : data.dump(-1, ' ', false, cluster::JSON_LENIENT)) : response.value("error", std::string("error"));
    return {{"content", {{{"type", "text"}, {"text", text}}}}, {"isError", !ok}};
}

_cold cluster::Json cluster::mcp_servers(const std::vector<std::string>& servers)
{
    cluster::Json out = cluster::Json::object();
    for (const std::string& entry: servers) {
        const std::size_t colon = entry.find(':');
        if (colon == std::string::npos) continue;
        const std::string name = cluster::trim(entry.substr(0, colon));
        const std::string rest = cluster::trim(entry.substr(colon + 1));
        if (name.empty() || rest.empty() || name == "claude-cluster") continue;
        if (rest.starts_with("http://") || rest.starts_with("https://")) {
            out[name] = {{"type", "http"}, {"url", rest}};
            continue;
        }
        // a command and its arguments (words; "quoted words" kept as one)
        std::vector<std::string> words;
        std::string word;
        char quote = 0;
        for (const char c: rest) {
            if (quote) {
                if (c == quote) quote = 0;
                else word += c;
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (c == ' ' || c == '\t') {
                if (!word.empty()) words.push_back(word);
                word.clear();
            } else {
                word += c;
            }
        }
        if (!word.empty()) words.push_back(word);
        cluster::Json server = {{"type", "stdio"}, {"command", cluster::expand_home(words[0]).string()}, {"args", cluster::Json::array()}};
        for (std::size_t i = 1; i < words.size(); ++i)
            server["args"].push_back(words[i]);
        out[name] = server;
    }
    return out;
}

_cold int cluster::mcp_serve(void)
{
    std::string line;
    while (std::getline(std::cin, line)) {
        cluster::Json message;
        try {
            message = cluster::Json::parse(line);
        } catch (const cluster::Json::exception&) {
            continue;
        }
        if (!message.contains("id")) continue; // notifications (initialized, cancelled...)
        const std::string method = message.value("method", std::string());
        const cluster::Json params = message.value("params", cluster::Json::object());
        cluster::Json response = {{"jsonrpc", "2.0"}, {"id", message["id"]}};

        if (method == "initialize") {
            response["result"] = {
                {"protocolVersion", params.value("protocolVersion", std::string("2025-06-18"))},
                {"capabilities", {{"tools", cluster::Json::object()}}},
                {"serverInfo", {{"name", "claude-cluster"}, {"version", CLUSTER_VERSION}}},
                {"instructions", "Tools of claude-cluster: manage the other Claude Code / agent sessions of the user (list, spawn, send, permissions, modes, tasks), its interface (session shown, layout, style, panels), its settings and the Remote Control of the global session."},
            };
        } else if (method == "tools/list" && std::getenv("CLAUDE_CLUSTER_SESSION") != nullptr && *std::getenv("CLAUDE_CLUSTER_SESSION") != '\0') {
            response["result"] = {{"tools", cluster::mcp_sub_tools(std::getenv("CLAUDE_CLUSTER_SESSION"))}};
        } else if (method == "tools/list") {
            // the ones off in the settings (MCP page) left out
            cluster::Json tools = cluster::mcp_tools();
            try {
                const cluster::Json off = cluster::Control::request({{"cmd", "mcp_off"}}).value("data", cluster::Json::array());
                cluster::Json kept = cluster::Json::array();
                for (const cluster::Json& tool: tools)
                    if (std::find(off.begin(), off.end(), tool.value("name", std::string())) == off.end()) kept.push_back(tool);
                tools = kept;
            } catch (const utils::exception::IException&) {
                // claude-cluster not reached: every tool listed (each call says it)
            }
            response["result"] = {{"tools", tools}};
        } else if (method == "tools/call") {
            response["result"] = cluster::mcp_call(params.value("name", std::string()), params.value("arguments", cluster::Json::object()));
        } else if (method == "ping") {
            response["result"] = cluster::Json::object();
        } else {
            response["error"] = {{"code", -32601}, {"message", "method not found: " + method}};
        }
        std::cout << response.dump(-1, ' ', false, cluster::JSON_LENIENT) << std::endl;
    }
    return 0;
}
