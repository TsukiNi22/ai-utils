/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Config.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Templates.hpp"
#include "cluster/Config.hpp"
#include "cluster/Match.hpp"
#include "cluster/Origin.hpp"
#include "cluster/Tools.hpp"
#include <toml++/toml.hpp>
#include <algorithm>
#include <optional>
#include <fstream>

/* tools */
// Built-in styles: html-style (light / dark tokens of the docs) and a few classic palettes
_cold static std::map<std::string, cluster::Style> builtin_styles_(void)
{
    // name, dark, bg, surface, text, muted, accent, border, ok, warn, error
    const std::vector<cluster::Style> styles = {
        {"html-light", false, "#f7f7f5", "#ffffff", "#1d1f23", "#5d636e", "#2f5bd3", "#dfe1e5", "#1f7a4a", "#a8540a", "#b42318"},
        {"html-dark", true, "#15171b", "#1d2025", "#e6e7ea", "#9aa0ab", "#7d9cf0", "#30343b", "#6fcf97", "#f0a868", "#f47174"},
        {"nord", true, "#2e3440", "#3b4252", "#eceff4", "#a3abb9", "#88c0d0", "#4c566a", "#a3be8c", "#ebcb8b", "#bf616a"},
        {"gruvbox", true, "#282828", "#32302f", "#ebdbb2", "#a89984", "#fabd2f", "#504945", "#b8bb26", "#fe8019", "#fb4934"},
        {"solarized-light", false, "#fdf6e3", "#eee8d5", "#073642", "#586e75", "#268bd2", "#d3cbb7", "#859900", "#b58900", "#dc322f"},
        {"mono", true, "#000000", "#111111", "#e0e0e0", "#9e9e9e", "#ffffff", "#3a3a3a", "#e0e0e0", "#e0e0e0", "#ffffff"},
    };
    std::map<std::string, cluster::Style> map;

    for (const cluster::Style& style: styles)
        map[style.name] = style;
    return map;
}

template<typename Node>
_cold static std::vector<std::string> strings_(const toml::node_view<Node>& node, const std::vector<std::string>& fallback)
{
    const toml::array* array = node.as_array();
    if (!array) return fallback;
    std::vector<std::string> values;
    for (const toml::node& item: *array)
        if (const std::optional<std::string> value = item.value<std::string>()) values.push_back(*value);
    return values;
}

/* constructor */
_cold cluster::Config::Config(void)
    : _path(cluster::config_dir() / "config.toml")
{
    this->defaults_();
}

/* setup */
_cold void cluster::Config::defaults_(void)
{
    this->styles = builtin_styles_();
    this->keys = {
        {"palette", "Ctrl+K"}, {"next_session", "Ctrl+N|Alt+N"}, {"prev_session", "Alt+P"}, {"global", "Ctrl+G"},
        {"new_session", "Ctrl+T"}, {"close_session", "Ctrl+W"}, {"allow", "Alt+Y"}, {"always", "Alt+A"}, {"deny", "Alt+D"},
        {"undo", "Ctrl+Z"}, {"redo", "Ctrl+Y"}, {"select_all", "Ctrl+A"}, {"stash", "Ctrl+S"}, {"unstash", "Ctrl+P"}, {"stashes", "Ctrl+L"}, {"take_back", "Escape"},
        {"history", "Ctrl+R"}, {"paste", "Ctrl+V"}, {"cut", "Ctrl+X"},
        {"interrupt", "Ctrl+C"}, {"resume", "F4"}, {"send_next", "F8"}, {"push_to_talk", "F5"}, {"mute", "F6"}, {"layout", "F2"}, {"settings", "F9"}, {"stats", "F10"}, {"graph", "Alt+G"}, {"quit", "Ctrl+Q"},
        {"graph_up", "Z|Up"}, {"graph_left", "Q|Left"}, {"graph_down", "S|Down"}, {"graph_right", "D|Right"}, {"graph_zoom_in", "E|+|="}, {"graph_zoom_out", "A|-"},
        {"graph_fit", "0|Home"}, {"graph_focus", "F"}, {"graph_next", "Tab"}, {"graph_prev", "Shift+Tab"}, {"graph_open", "Enter"}, {"graph_legend", "L"}, {"graph_map", "M"},
        {"graph_close", "Escape"},
        {"restore", "Ctrl+Shift+T|Ctrl+Shift+R|Alt+T"}, {"prefix", "Ctrl+B"},
        {"mode_cycle", "Shift+Tab"}, {"search", "Ctrl+F"}, {"edit", "Ctrl+E"}, {"expand", "Ctrl+O"}, {"help", "F1"},
        {"transcript_top", "Shift+PageUp|Ctrl+Home"}, {"transcript_bottom", "Shift+PageDown|Ctrl+End"},
        {"session_up", "Alt+Up"}, {"session_down", "Alt+Down"}, {"session_left", "Alt+Left"}, {"session_right", "Alt+Right"},
        {"grid_open", "Shift+Enter|Alt+O"}, {"session_page_up", "Alt+PageUp"}, {"session_page_down", "Alt+PageDown"}, {"session_first", "Alt+Home"}, {"session_last", "Alt+End"},
    };
}

_cold std::string cluster::Config::check(const std::string& text)
{
    try {
        (void)toml::parse(text);
    } catch (const toml::parse_error& e) {
        return std::string(e.description()) + " (line " + std::to_string(e.source().begin.line) + ")";
    }
    return "";
}

_cold void cluster::Config::load(void)
{
    std::error_code error;

    // First run: the default files, commented, to edit
    cluster::private_dir(cluster::config_dir()); // this user only
    cluster::private_dir(cluster::data_dir());
    if (!std::filesystem::exists(this->_path)) std::ofstream(this->_path) << cluster::templates::CONFIG;
    const std::filesystem::path prompt = cluster::config_dir() / "global.md";
    if (!std::filesystem::exists(prompt)) std::ofstream(prompt) << cluster::templates::GLOBAL_PROMPT;
    const std::filesystem::path sessions = cluster::config_dir() / "sessions.md";
    if (!std::filesystem::exists(sessions)) std::ofstream(sessions) << cluster::templates::SESSIONS_PROMPT;
    const std::filesystem::path vocabulary = cluster::config_dir() / "vocabulary.txt";
    if (!std::filesystem::exists(vocabulary)) std::ofstream(vocabulary) << cluster::templates::VOCABULARY;
    const std::filesystem::path words = cluster::config_dir() / "voice-words.txt";
    if (!std::filesystem::exists(words)) std::ofstream(words) << cluster::templates::VOICE_WORDS;
    this->_mtime = std::filesystem::last_write_time(this->_path, error);

    toml::table table;
    try {
        table = toml::parse_file(this->_path.string());
    } catch (const toml::parse_error& e) {
        this->_error = this->_path.string() + ":" + std::to_string(e.source().begin.line) + ": " + std::string(e.description());
        return;
    }
    this->_error.clear();
    this->defaults_();

    /* ui */
    this->frontend = table["ui"]["frontend"].value_or(this->frontend);
    this->sharedCore = table["ui"]["shared_core"].value_or(this->sharedCore);
    this->quitBackground = table["ui"]["quit_background"].value_or(this->quitBackground);
    this->debug = table["debug"]["enabled"].value_or(this->debug);
    if (const toml::array* topics = table["debug"]["topics"].as_array()) {
        this->debugTopics.clear();
        for (const toml::node& topic: *topics)
            if (topic.is_string()) this->debugTopics.push_back(topic.as_string()->get());
    }
    cluster::debug_configure(this->debug, this->debugTopics); // every process reads it (and its reload)
    this->shellContext = table["ui"]["shell_context"].value_or(this->shellContext);
    this->promptOrigin = table["ui"]["prompt_origin"].value_or(this->promptOrigin);
    this->promptColorUser = table["ui"]["prompt_color_user"].value_or(this->promptColorUser);
    this->promptColorMcp = table["ui"]["prompt_color_mcp"].value_or(this->promptColorMcp);
    this->promptColorTask = table["ui"]["prompt_color_task"].value_or(this->promptColorTask);
    cluster::set_origin_colors({this->promptOrigin, this->promptColorUser, this->promptColorMcp, this->promptColorTask});
    this->matching = table["ui"]["matching"].value_or(this->matching);
    cluster::set_matching(this->matching);
    this->layout = table["ui"]["layout"].value_or(this->layout);
    this->guiMode = table["ui"]["gui_mode"].value_or(this->guiMode);
    this->style = table["ui"]["style"].value_or(this->style);
    this->panels = strings_(table["ui"]["panels"], this->panels);
    this->restore = table["ui"]["restore"].value_or(this->restore);
    this->notify = table["ui"]["notify"].value_or(this->notify);
    this->notifyGlobal = table["ui"]["notify_global"].value_or(this->notifyGlobal);
    this->notifyDone = table["ui"]["notify_done"].value_or(this->notifyDone);
    this->notifyPermission = table["ui"]["notify_permission"].value_or(this->notifyPermission);
    this->notifySound = table["ui"]["notify_sound"].value_or(this->notifySound);
    this->notifyWhen = table["ui"]["notify_when"].value_or(this->notifyWhen);
    this->notifyQuestion = table["ui"]["notify_question"].value_or(this->notifyQuestion);
    this->notifyError = table["ui"]["notify_error"].value_or(this->notifyError);
    this->notifyLimits = table["ui"]["notify_limits"].value_or(this->notifyLimits);
    this->notifyWeb = table["ui"]["notify_web"].value_or(this->notifyWeb);
    this->notifyTasks = table["ui"]["notify_tasks"].value_or(this->notifyTasks);
    this->notifyReminders = table["ui"]["notify_reminders"].value_or(this->notifyReminders);
    this->notifyApp = table["ui"]["notify_app"].value_or(this->notifyApp);
    this->notifyReady = table["ui"]["notify_ready"].value_or(this->notifyReady);
    this->vim = table["ui"]["vim"].value_or(this->vim);
    this->groupByCategory = table["ui"]["group_by_category"].value_or(this->groupByCategory);
    this->pasteLines = table["ui"]["paste_lines"].value_or(this->pasteLines);
    this->pasteChars = table["ui"]["paste_chars"].value_or(this->pasteChars);
    this->stashMax = std::clamp(table["ui"]["stash_max"].value_or(this->stashMax), 1, 500);
    this->stashFull = table["ui"]["stash_full"].value_or(this->stashFull) == "drop_oldest" ? "drop_oldest" : "refuse";
    this->stashRewound = table["ui"]["stash_rewound"].value_or(this->stashRewound);
    this->editor = table["ui"]["editor"].value_or(this->editor);
    this->imageViewer = table["ui"]["image_viewer"].value_or(this->imageViewer);
    this->imagePreview = table["ui"]["image_preview"].value_or(this->imagePreview);
    this->typewriter = table["ui"]["typewriter"].value_or(this->typewriter);
    this->typewriterSpeed = table["ui"]["typewriter_speed"].value_or(this->typewriterSpeed);

    /* agent */
    // one provider for everything; the old keys ([agent] default, [global] backend) are the fallback of the old configs
    const std::optional<std::string> legacyAgent = table["agent"]["default"].value<std::string>();
    const std::optional<std::string> legacyGlobal = table["global"]["backend"].value<std::string>();
    if (const std::optional<std::string> chosen = table["agent"]["provider"].value<std::string>(); chosen && !chosen->empty()) this->provider = *chosen;
    else if (legacyAgent && !legacyAgent->empty()) this->provider = *legacyAgent;
    else if (legacyGlobal && !legacyGlobal->empty()) this->provider = *legacyGlobal;
    this->backend = this->provider;
    this->mode = table["agent"]["permission_mode"].value_or(this->mode);
    this->effort = table["agent"]["effort"].value_or(this->effort);
    this->allowBypass = table["agent"]["allow_bypass"].value_or(this->allowBypass);
    this->workMethod = table["agent"]["work_method"].value_or(this->workMethod);
    this->workMethodText = table["agent"]["work_method_text"].value_or(this->workMethodText);
    this->workMethodDetect = table["agent"]["work_method_detect"].value_or(this->workMethodDetect);
    this->workMethodEvery = std::max(0, table["agent"]["work_method_every"].value_or(this->workMethodEvery));
    this->extraArgs = strings_(table["agent"]["extra_args"], {});
    if (const toml::table* commandsTable = table["commands"].as_table())
        for (const auto &[name, value]: *commandsTable)
            if (const std::optional<std::string> cmd = value.value<std::string>()) this->commands[std::string(name.str())] = *cmd;

    /* global */
    this->globalEnabled = table["global"]["enabled"].value_or(this->globalEnabled);
    this->globalBackend = this->provider.substr(0, this->provider.find('/')); // the provider of every session (its model: [global] model)
    this->globalCwd = table["global"]["cwd"].value_or(this->globalCwd);
    this->globalModel = table["global"]["model"].value_or(this->globalModel);
    this->globalMode = table["global"]["permission_mode"].value_or(this->globalMode);
    this->globalEffort = table["global"]["effort"].value_or(this->globalEffort);
    this->globalPrompt = table["global"]["prompt"].value_or((cluster::config_dir() / "global.md").string()); // the config dir of this process (CLAUDE_CLUSTER_CONFIG_DIR), not the one of HOME
    this->remoteName = table["global"]["remote_name"].value_or(this->remoteName);

    /* sessions */
    this->trashDays = table["sessions"]["trash_days"].value_or(this->trashDays);
    this->describe = table["sessions"]["describe"].value_or(this->describe);
    this->describeModel = table["sessions"]["describe_model"].value_or(this->describeModel);
    this->maxParallel = table["sessions"]["max_parallel"].value_or(this->maxParallel);
    this->taskQueue = table["sessions"]["task_queue"].value_or(this->taskQueue);
    this->taskResultChars = std::clamp(table["sessions"]["task_result_chars"].value_or(this->taskResultChars), 200, 100000);
    this->notifyTaskEach = table["ui"]["notify_task_each"].value_or(this->notifyTaskEach);
    this->subRead = table["agent"]["sub_read"].value_or(this->subRead);
    this->subTasks = table["agent"]["sub_tasks"].value_or(this->subTasks);
    this->subSchedules = table["agent"]["sub_schedules"].value_or(this->subSchedules);
    this->subSpawn = table["agent"]["sub_spawn"].value_or(this->subSpawn);
    this->subSpawnCap = std::clamp(table["agent"]["sub_spawn_cap"].value_or(this->subSpawnCap), 0, 50);
    this->historyLimit = std::max(0, table["sessions"]["history_limit"].value_or(this->historyLimit));
    this->subPin = table["agent"]["sub_pin"].value_or(this->subPin);
    this->pinGlobal = table["agent"]["global_pin"].value_or(this->pinGlobal);
    this->taxonomyEdit = table["agent"]["taxonomy_edit"].value_or(this->taxonomyEdit);
    this->transcriptLimitMb = std::max(0, table["sessions"]["transcript_limit_mb"].value_or(this->transcriptLimitMb));
    this->queueGiven = table["sessions"]["queue_given"].value_or(this->queueGiven);
    this->budget = table["sessions"]["budget_usd"].value_or(this->budget);
    this->sessionsPrompt = table["sessions"]["prompt"].value_or((cluster::config_dir() / "sessions.md").string());
    this->globalFork = table["sessions"]["global_fork"].value_or(this->globalFork);
    this->bypassToGlobal = table["sessions"]["bypass_to_global"].value_or(this->bypassToGlobal);
    this->questionsToGlobal = table["sessions"]["questions_to_global"].value_or(this->questionsToGlobal);
    this->globalAnswers = table["sessions"]["global_answers"].value_or(this->globalAnswers);
    this->archiveOneShot = table["sessions"]["archive_one_shot"].value_or(this->archiveOneShot);
    this->globalEdit = table["sessions"]["global_edit"].value_or(this->globalEdit);
    // the old key global_prompts (both prompts) stays the fallback of the global prompt only
    this->globalPromptWrite = table["sessions"]["rewrite_global_prompt"].value_or(table["sessions"]["global_prompts"].value_or(this->globalPromptWrite));
    this->sessionsPromptWrite = table["sessions"]["rewrite_sessions_prompt"].value_or(this->sessionsPromptWrite);

    /* voice */
    toml::node_view<const toml::node> v = std::as_const(table)["voice"];
    this->voice.enabled = v["enabled"].value_or(this->voice.enabled);
    this->voice.mode = v["mode"].value_or(this->voice.mode);
    this->voice.wakeWord = v["wake_word"].value_or(this->voice.wakeWord);
    this->voice.wakeSeconds = v["wake_seconds"].value_or(this->voice.wakeSeconds);
    this->voice.wakeReply = v["wake_reply"].value_or(this->voice.wakeReply);
    this->voice.onlyMe = v["only_me"].value_or(this->voice.onlyMe);
    this->voice.confirmSeconds = v["confirm_seconds"].value_or(this->voice.confirmSeconds);
    this->voice.speak = v["speak"].value_or(this->voice.speak);
    this->voice.summaryChars = v["summary_chars"].value_or(this->voice.summaryChars);
    this->voice.listenCommand = v["listen_command"].value_or(this->voice.listenCommand);
    this->voice.sayCommand = v["say_command"].value_or(this->voice.sayCommand);
    this->voice.sayEngine = v["say_engine"].value_or(this->voice.sayEngine);
    this->voice.sayVoices = v["say_voices"].value_or(this->voice.sayVoices);
    this->voice.voiceFr = v["voice_fr"].value_or(this->voice.voiceFr);
    this->voice.voiceEn = v["voice_en"].value_or(this->voice.voiceEn);
    this->voice.cancelWords = strings_(v["cancel_words"], this->voice.cancelWords);
    this->voice.sendWords = strings_(v["send_words"], this->voice.sendWords);
    this->voice.words = v["words"].value_or(this->voice.words);
    this->voice.listenModel = v["listen_model"].value_or(this->voice.listenModel);
    this->voice.listenLanguage = v["listen_language"].value_or(this->voice.listenLanguage);
    this->voice.endSilence = v["end_silence"].value_or(this->voice.endSilence);
    this->voice.vocabulary = v["vocabulary"].value_or(this->voice.vocabulary);
    if (!this->voiceForce.empty()) {
        this->voice.enabled = true;
        this->voice.mode = this->voiceForce;
    }

    /* user */
    toml::node_view<const toml::node> us = std::as_const(table)["user"];
    this->user.nickname = us["nickname"].value_or(this->user.nickname);
    this->user.email = us["email"].value_or(this->user.email);
    this->user.editor = us["editor"].value_or(this->user.editor);
    this->user.language = us["language"].value_or(this->user.language);
    this->user.notes = us["notes"].value_or(this->user.notes);
    this->machineDays = table["ui"]["machine_days"].value_or(this->machineDays);

    /* web */
    this->web.enabled = table["web"]["enabled"].value_or(this->web.enabled);
    this->web.port = table["web"]["port"].value_or(this->web.port);
    this->web.https = table["web"]["https"].value_or(this->web.https);
    this->web.approve = table["web"]["approve"].value_or(this->web.approve);
    this->web.remote = table["web"]["remote"].value_or(this->web.remote);
    this->web.remoteHost = table["web"]["remote_host"].value_or(this->web.remoteHost);
    this->web.remotePort = table["web"]["remote_port"].value_or(this->web.remotePort);
    this->web.shell = table["web"]["shell"].value_or(this->web.shell);
    this->web.settings = strings_(table["web"]["settings"], this->web.settings);
    this->web.ttyPort = table["web"]["tty_port"].value_or(this->web.ttyPort);
    this->web.ttyRemotePort = table["web"]["tty_remote_port"].value_or(this->web.ttyRemotePort);

    /* attachments */
    this->attach.enabled = table["attachments"]["enabled"].value_or(this->attach.enabled);
    this->attach.maxMb = std::clamp(static_cast<int>(table["attachments"]["max_mb"].value_or(this->attach.maxMb)), 1, 500);
    this->attach.maxFiles = std::clamp(static_cast<int>(table["attachments"]["max_files"].value_or(this->attach.maxFiles)), 1, 100);
    this->attach.keep = std::max(0, static_cast<int>(table["attachments"]["keep"].value_or(this->attach.keep)));
    this->attach.maxPixels = std::max(0, static_cast<int>(table["attachments"]["max_pixels"].value_or(this->attach.maxPixels)));
    this->attach.stripExif = table["attachments"]["strip_exif"].value_or(this->attach.stripExif);
    this->attach.dir = table["attachments"]["dir"].value_or(this->attach.dir);
    this->attach.link = table["attachments"]["link"].value_or(this->attach.link);
    this->attach.globalAttach = table["attachments"]["global"].value_or(this->attach.globalAttach);
    this->attach.globalMax = std::clamp(static_cast<int>(table["attachments"]["global_max"].value_or(this->attach.globalMax)), 0, 50);

    /* service */
    toml::node_view<const toml::node> sv = std::as_const(table)["service"];
    // merged into Interface / Voice (one setting each): the old [service] keys still read when they are in the file
    this->service.frontend = sv["frontend"].value_or(this->frontend == "tty" ? std::string("tty") : this->frontend == "gui" ? std::string("gui") : std::string("auto"));
    this->service.terminal = table["ui"]["terminal"].value_or(sv["terminal"].value_or(this->service.terminal));
    this->service.voiceMode = sv["voice_mode"].value_or(this->voice.mode == "push" ? std::string("auto") : this->voice.mode); // opened by the voice: it listens
    this->service.wakeWord = sv["wake_word"].value_or(std::string());
    this->service.python = sv["python"].value_or(this->service.python);
    this->service.source = table["voice"]["source"].value_or(sv["source"].value_or(this->service.source));
    this->policy.defaultModel = table["policy"]["default_model"].value_or(this->policy.defaultModel);
    this->policy.defaultEffort = table["policy"]["default_effort"].value_or(this->policy.defaultEffort);
    for (const auto &[key, field]: {std::pair<std::string, std::string*>{"model_mode", &this->policy.modelMode}, {"effort_mode", &this->policy.effortMode}}) {
        const std::string chosen = table["policy"][key].value_or(*field);
        *field = chosen == "free" || chosen == "force" ? chosen : "advise";
    }
    /* orchestrator */
    toml::node_view<const toml::node> oc = std::as_const(table)["orchestrator"];
    this->orch.enabled = oc["enabled"].value_or(this->orch.enabled);
    this->orch.profile = oc["profile"].value_or(this->orch.profile);
    if (this->orch.profile != "low" && this->orch.profile != "normal" && this->orch.profile != "max") this->orch.profile = "low";
    this->orch.maxJobs = std::clamp(static_cast<int>(oc["max_jobs"].value_or(this->orch.maxJobs)), 0, 256);
    this->orch.maxPerSession = std::clamp(static_cast<int>(oc["max_per_session"].value_or(this->orch.maxPerSession)), 0, 256);
    this->orch.reservedCpu = std::clamp(static_cast<int>(oc["reserved_cpu"].value_or(this->orch.reservedCpu)), 0, 256);
    this->orch.reservedRamMb = std::clamp(static_cast<int>(oc["reserved_ram_mb"].value_or(this->orch.reservedRamMb)), 0, 1 << 20);
    this->orch.threads = std::clamp(static_cast<int>(oc["threads"].value_or(this->orch.threads)), 0, 256);
    {
        static const char* classes[5] = {"build", "tests", "container", "gpu", "custom"};
        for (int i = 0; i < 5; ++i) {
            this->orch.classMax[i] = std::clamp(static_cast<int>(oc[std::string(classes[i]) + "_max"].value_or(this->orch.classMax[i])), 0, 256);
            this->orch.classRam[i] = std::clamp(static_cast<int>(oc[std::string(classes[i]) + "_ram_mb"].value_or(this->orch.classRam[i])), 0, 1 << 20);
        }
    }
    this->orch.patterns = strings_(oc["patterns"], this->orch.patterns);
    this->orch.sessionPrio = strings_(oc["session_prio"], this->orch.sessionPrio);
    this->orch.sessionQuota = strings_(oc["session_quota"], this->orch.sessionQuota);
    this->orch.detectCpuSeconds = std::max(0, static_cast<int>(oc["detect_cpu_seconds"].value_or(this->orch.detectCpuSeconds)));
    this->orch.detectAction = oc["detect_action"].value_or(this->orch.detectAction);
    if (this->orch.detectAction != "notice" && this->orch.detectAction != "tell" && this->orch.detectAction != "off") this->orch.detectAction = "notice";
    this->orch.holdLoad = oc["hold_load"].value_or(this->orch.holdLoad);
    this->orch.reduceLoad = oc["reduce_load"].value_or(this->orch.reduceLoad);
    this->orch.holdMemMb = std::max(0, static_cast<int>(oc["hold_mem_mb"].value_or(this->orch.holdMemMb)));
    this->orch.reduceMemMb = std::max(0, static_cast<int>(oc["reduce_mem_mb"].value_or(this->orch.reduceMemMb)));
    this->orch.oomMemMb = std::max(0, static_cast<int>(oc["oom_mem_mb"].value_or(this->orch.oomMemMb)));
    this->orch.holdSwapMb = std::max(0, static_cast<int>(oc["hold_swap_mb"].value_or(this->orch.holdSwapMb)));
    this->orch.psiCpuHold = std::clamp(static_cast<int>(oc["psi_cpu_hold"].value_or(this->orch.psiCpuHold)), 0, 100);
    this->orch.psiCpuReduce = std::clamp(static_cast<int>(oc["psi_cpu_reduce"].value_or(this->orch.psiCpuReduce)), 0, 100);
    this->orch.psiMemHold = std::clamp(static_cast<int>(oc["psi_mem_hold"].value_or(this->orch.psiMemHold)), 0, 100);
    this->orch.psiMemReduce = std::clamp(static_cast<int>(oc["psi_mem_reduce"].value_or(this->orch.psiMemReduce)), 0, 100);
    this->orch.psiIoHold = std::clamp(static_cast<int>(oc["psi_io_hold"].value_or(this->orch.psiIoHold)), 0, 100);
    this->orch.psiIoReduce = std::clamp(static_cast<int>(oc["psi_io_reduce"].value_or(this->orch.psiIoReduce)), 0, 100);
    this->orch.resumeAfter = std::clamp(static_cast<int>(oc["resume_after"].value_or(this->orch.resumeAfter)), 0, 3600);
    this->orch.killOnOom = oc["kill_on_oom"].value_or(this->orch.killOnOom);
    this->orch.agingSeconds = std::clamp(static_cast<int>(oc["aging_seconds"].value_or(this->orch.agingSeconds)), 1, 86400);
    this->orch.focusBoost = std::clamp(static_cast<int>(oc["focus_boost"].value_or(this->orch.focusBoost)), 0, 1000);
    this->orch.subAdd = oc["sub_add"].value_or(this->orch.subAdd);
    this->orch.subKillOwn = oc["sub_kill_own"].value_or(this->orch.subKillOwn);
    this->orch.subSeeOthers = oc["sub_see_others"].value_or(this->orch.subSeeOthers);
    this->orch.notifyKilled = oc["notify_killed"].value_or(this->orch.notifyKilled);
    this->orch.notifyDone = oc["notify_done"].value_or(this->orch.notifyDone);
    this->orch.history = std::clamp(static_cast<int>(oc["history"].value_or(this->orch.history)), 10, 5000);
    this->orch.procDir = oc["proc_dir"].value_or(this->orch.procDir);
    this->orch.sysDir = oc["sys_dir"].value_or(this->orch.sysDir);

    this->policy.allowedModels = strings_(table["policy"]["allowed_models"], {});
    this->policy.allowedEfforts = strings_(table["policy"]["allowed_efforts"], {});
    this->policy.askCostly = table["policy"]["ask_costly"].value_or(this->policy.askCostly);
    this->policy.costlyModels = strings_(table["policy"]["costly_models"], this->policy.costlyModels);
    this->policy.costlyEfforts = strings_(table["policy"]["costly_efforts"], this->policy.costlyEfforts);
    this->limits.dailyUsd = std::max(0.0, table["limits"]["daily_usd"].value_or(this->limits.dailyUsd));
    this->limits.weeklyUsd = std::max(0.0, table["limits"]["weekly_usd"].value_or(this->limits.weeklyUsd));
    this->limits.monthlyUsd = std::max(0.0, table["limits"]["monthly_usd"].value_or(this->limits.monthlyUsd));
    this->limits.scope = table["limits"]["scope"].value_or(this->limits.scope) == "all" ? "all" : "keys";
    this->limits.block = table["limits"]["block"].value_or(this->limits.block);
    this->limits.warnPercent = std::clamp(static_cast<int>(table["limits"]["warn_percent"].value_or(this->limits.warnPercent)), 0, 99);
    this->usage.account = table["usage"]["account"].value_or(this->usage.account);
    if (table["usage"]["transcripts"].as_array()) this->usage.transcripts = strings_(table["usage"]["transcripts"], this->usage.transcripts);
    else if (const char* env = std::getenv("CLAUDE_CLUSTER_TRANSCRIPTS")) // the environment of the core (tests): folders apart by ':', empty: none
        this->usage.transcripts = cluster::split(env, ':');
    if (const toml::table* prices = table["usage"]["prices"].as_table()) {
        this->usage.prices.clear();
        for (const auto &[name, node]: *prices) {
            if (const std::optional<double> input = node.value<double>()) this->usage.prices[std::string(name.str())] = {*input, 0.0};
            else if (const toml::array* pair = node.as_array(); pair && pair->size() >= 1)
                this->usage.prices[std::string(name.str())] = {(*pair)[0].value<double>().value_or(0.0), pair->size() >= 2 ? (*pair)[1].value<double>().value_or(0.0) : 0.0};
        }
    }
    this->services.refresh = table["services"]["refresh"].value_or(this->services.refresh);
    this->services.delay = std::max(1, table["services"]["delay"].value_or(this->services.delay));

    /* mcp */
    this->mcpServers = strings_(table["mcp"]["servers"], {});
    this->pluginRepos = strings_(table["plugins"]["repos"], {});
    this->pluginSelect.clear();
    if (const toml::table* select = table["plugins"]["select"].as_table())
        for (const auto &[name, value]: *select)
            if (value.is_boolean()) this->pluginSelect[std::string(name.str())] = value.as_boolean()->get();
    this->pluginOff.clear();
    if (const toml::table* off = table["plugins"]["off"].as_table())
        for (const auto &[name, value]: *off)
            if (value.is_boolean()) this->pluginOff[std::string(name.str())] = value.as_boolean()->get();
    this->mcpTools.clear();
    if (const toml::table* toolsTable = table["mcp_tools"].as_table())
        for (const auto &[tool, on]: *toolsTable)
            if (const std::optional<bool> value = on.value<bool>()) this->mcpTools[std::string(tool.str())] = *value;

    /* keys */
    if (const toml::table* keysTable = table["keys"].as_table())
        for (const auto &[action, key]: *keysTable)
            if (const std::optional<std::string> value = key.value<std::string>()) this->keys[std::string(action.str())] = *value;

    /* styles (own ones: missing keys taken from html-dark) */
    if (const toml::table* stylesTable = table["styles"].as_table()) {
        for (const auto &[name, node]: *stylesTable) {
            const toml::table* s = node.as_table();
            if (!s) continue;
            cluster::Style custom = this->styles.at("html-dark");
            custom.name = std::string(name.str());
            custom.dark = (*s)["dark"].value_or(custom.dark);
            custom.bg = (*s)["bg"].value_or(custom.bg);
            custom.surface = (*s)["surface"].value_or(custom.surface);
            custom.text = (*s)["text"].value_or(custom.text);
            custom.muted = (*s)["muted"].value_or(custom.muted);
            custom.accent = (*s)["accent"].value_or(custom.accent);
            custom.border = (*s)["border"].value_or(custom.border);
            custom.ok = (*s)["ok"].value_or(custom.ok);
            custom.warn = (*s)["warn"].value_or(custom.warn);
            custom.error = (*s)["error"].value_or(custom.error);
            this->styles[custom.name] = custom;
        }
    }

    /* models offered per provider */
    this->models.clear();
    if (const toml::table* modelsTable = table["models"].as_table())
        for (const auto &[name, node]: *modelsTable)
            this->models[std::string(name.str())] = strings_(std::as_const(*modelsTable)[name.str()], {});

    /* profiles */
    this->profiles.clear();
    if (const toml::table* profilesTable = table["profiles"].as_table()) {
        for (const auto &[name, node]: *profilesTable) {
            const toml::table* p = node.as_table();
            if (!p) continue;
            cluster::Profile profile;
            profile.name = std::string(name.str());
            profile.cwd = (*p)["cwd"].value_or(std::string());
            profile.backend = (*p)["backend"].value_or(std::string());
            profile.mode = (*p)["permission_mode"].value_or(std::string());
            profile.prompt = (*p)["prompt"].value_or(std::string());
            profile.extraArgs = strings_(std::as_const(*p)["extra_args"], {});
            this->profiles[profile.name] = profile;
        }
    }

    /* providers */
    this->providers.clear();
    if (const toml::table* providersTable = table["providers"].as_table()) {
        for (const auto &[name, node]: *providersTable) {
            const toml::table* p = node.as_table();
            if (!p) continue;
            cluster::ProviderConf entry;
            entry.name = std::string(name.str());
            entry.driver = (*p)["driver"].value_or(std::string("claude"));
            entry.baseUrl = (*p)["base_url"].value_or(std::string());
            entry.model = (*p)["model"].value_or(std::string());
            entry.key = (*p)["key"].value_or(false);
            this->providers[entry.name] = entry;
        }
    }
}

_cold bool cluster::Config::reload(void)
{
    std::error_code error;
    const std::filesystem::file_time_type mtime = std::filesystem::last_write_time(this->_path, error);
    if (error || mtime == this->_mtime) return false;
    this->load();
    return true;
}

_cold const cluster::Style& cluster::Config::currentStyle(void) const
{
    auto it = this->styles.find(this->style);
    return it != this->styles.end() ? it->second : this->styles.at("html-dark");
}

_cold std::string cluster::Config::command(const std::string& driver) const
{
    auto it = this->commands.find(driver);
    return it != this->commands.end() ? it->second : driver;
}
