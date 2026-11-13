/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Method.cpp

File Description:
##  The work method of the user (see Method.hpp)
\**************************************************************/

#include "cluster/Method.hpp"
#include "cluster/Tools.hpp"

//----------------------------------------------------------------//
/* FUNCTION */

_cold const std::vector<cluster::MethodPreset>& cluster::method_presets(void)
{
    static const std::vector<cluster::MethodPreset> list = {
        {"free", "Free (no method)", "", ""},
        {"list-review", "Task list, then review, repeat until good",
            "Work through the tasks you are given one after the other, then end with a short self-review of the result (what is wrong or missing, the tests run) in your answer. "
            "A review request means: look for defects, not for praise; answer \"nothing found\" when it is clean.",
            "Task lists, then a review, repeated until good: split a request into a short list of concrete tasks and send them (one numbered `session_send`, or a "
            "`session_fanout` when they are independent); when the answers are back, ask a session (the same one, or another as a fresh pair of eyes) to review the result; "
            "send the defects found back as a new task list; repeat until a review finds nothing, then report to the user once."},
        {"one-task", "One task at a time",
            "One task at a time: finish and verify the task you were given before anything else. If a prompt holds several tasks, do the first one, report, and say what is "
            "left instead of starting the next.",
            "One task at a time: one task per prompt to a session, its answer awaited before the next one (never a list in one prompt); separate tasks may run in parallel "
            "in separate sessions."},
        {"plan-first", "Plan first, then do",
            "Plan first: before editing, answer with a short plan (steps, files, risks) and wait for the go-ahead in a following prompt, unless the task is trivial; then do it "
            "step by step.",
            "Plan first: for a request that is not trivial, ask the session for a plan (no edits) before anything; show the plan to the user in a few lines (or accept it "
            "yourself when they said so), then send the go-ahead."},
        {"custom", "Custom (the free text only)", "", ""},
    };
    return list;
}

_cold const cluster::MethodPreset* cluster::method_preset(const std::string& id)
{
    for (const cluster::MethodPreset& preset: cluster::method_presets())
        if (preset.id == id) return &preset;
    return nullptr;
}

_cold std::string cluster::method_text(const cluster::Config& config, const bool global)
{
    // short: the preset, then what the user wrote (400 characters at most, on one paragraph)
    const cluster::MethodPreset* preset = cluster::method_preset(config.workMethod);
    std::string text = preset == nullptr ? std::string() : global ? preset->global : preset->sub;
    const std::string own = cluster::one_line(cluster::trim(config.workMethodText), 400);
    if (!own.empty()) text += (text.empty() ? "" : " ") + own;
    if (text.empty()) return "";
    return std::string("## Work method of the user\n") + (global ? "Split the requests you dispatch accordingly. " : "") + text + "\n";
}
