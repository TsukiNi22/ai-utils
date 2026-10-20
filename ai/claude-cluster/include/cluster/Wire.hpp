/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Wire.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef CLUSTER_WIRE_H
    #define CLUSTER_WIRE_H

    #include "Machine.hpp"          // cluster::MachineProgress
    #include "Manager.hpp"          // cluster::Notice
    #include "Session.hpp"          // cluster::Queued
    #include "Icons.hpp"            // cluster::AppFace
    #include "Types.hpp"            // cluster::Snapshot, cluster::Json

// The state of claude-cluster sent to its interfaces in another process (several terminals / windows on one core): every
// field of the structures, in JSON both ways (a missing field: its default)
namespace cluster { // namespace start

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Usage, input, output, cacheRead, cacheCreation)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Metrics, last, current, total, contextUsed, contextWindow, cost, lastCost, costKnown, limit5h, limit7d,
    reset5h, reset7d, turns)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Entry, kind, text, tool, time, origin)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Permission, requestId, tool, description, input, suggestions, time, reason, safety)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ToolRun, id, name, summary, done, error, time, image)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SessionSpec, id, name, cwd, backend, claudeId, leaf, resumeAt, fork, model, mode, effort, profile, extraArgs,
    panels, global, created, deleted, cost, tokens, summary, summaryAuto, summaryTurns, turns)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Todo, id, content, status, activeForm)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Progress, id, title, session, now, log, done, ok, started, ended)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Failure, id, action, source, error, retry, at)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Schedule, id, at, every, action, session, text, by, status, reason, archived, runs, last)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Snapshot, spec, state, stopped, metrics, entries, permissions, tools, skillsLoaded, skillsAvailable,
    slashCommands, commandHelp, files, partial, thinking, step, stepSince, stepDetail, turnSince, streamed, thinkingTokens, steps, modelUsed, pid,
    lastError, remoteUrl, bridgeState, queued, queue, queueGiven, version, warning, warningAt, todos)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Task, id, prompt, target, after, status, session, created, group, label, answer, retries, started, ended,
    by, reason, archived, forced)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Notice, time, session, text, error)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MachineProgress, id, steps, log, finished, now, next, fraction, done, repos)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AppFace, face, badge, urgent)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Queued, text, images, origin)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Draft, text, images, pastes)

} // namespace end

#endif /* CLUSTER_WIRE_H */
