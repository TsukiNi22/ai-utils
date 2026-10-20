/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Config.hpp

File Description:
##  config.toml of claude-cluster (defaults, styles, profiles,
##  providers, keys), reloaded live
\**************************************************************/

#ifndef CLUSTER_CONFIG_H
    #define CLUSTER_CONFIG_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include <filesystem>           // std::filesystem::path, std::filesystem::file_time_type
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <map>                  // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Style {
    std::string name;
    bool dark = true;
    std::string bg;
    std::string surface;
    std::string text;
    std::string muted;
    std::string accent;
    std::string border;
    std::string ok;
    std::string warn;
    std::string error;
};

struct Profile {
    std::string name;
    std::string cwd;
    std::string backend;
    std::string mode;
    std::string prompt;
    std::vector<std::string> extraArgs;
};

struct ProviderConf {
    std::string name;
    std::string driver;         // claude | qwen | opencode | codex
    std::string baseUrl;
    std::string model;          // default model
    bool key = false;           // an API key is needed (stored by `auth login`)
};

struct VoiceConf {
    bool enabled = false;
    std::string mode = "push";  // push | auto | wake
    std::string wakeWord = "ok claude"; // wake mode: phrase(s) that trigger the listening, comma separated variants
    int wakeSeconds = 8;        // wake mode: the phrase alone arms the listening this long
    std::string wakeReply;      // wake mode: spoken when armed (ex: "Oui ?"), empty: nothing
    bool onlyMe = true;
    int confirmSeconds = 3;
    bool speak = true;
    std::size_t summaryChars = 220;
    std::string listenCommand = "voice-listen";
    std::string sayCommand = "voice-say";
    std::string sayEngine = "piper";    // voice-say -e: piper | kokoro | elevenlabs | cartesia (failing: Piper)
    std::string sayVoices;              // the voice of that engine per language: "fr=ff_siwis, en=bf_emma, es=kokoro:ef_dora"
    std::string voiceFr = "tom-medium"; // Piper voices: the engine piper, the fallback of the others
    std::string voiceEn = "en_GB-northern_english_male-medium";
    // said alone while a transcription waits: dropped / sent now (cancel: also stops the answer being read); the
    // words of voice-words.txt (edited in the editor), these ones of config.toml added
    std::vector<std::string> cancelWords;
    std::vector<std::string> sendWords;
    std::string words;              // the file of these words (empty: voice-words.txt of the config folder)
    std::string listenModel;        // voice-listen --model: parakeet | whisper | kroko-fr | groq | deepgram (empty: voice-models)
    std::string listenLanguage;     // voice-listen --language (whisper, the APIs): fr, en... (empty: found by the model)
    double endSilence = 0.0;        // voice-listen --end-silence: seconds of silence ending a sentence (0: its own, 1.0)
    std::string vocabulary;         // proper names written back in what is heard (empty: vocabulary.txt of the config folder)
};

// What the user tells about themselves (asked at the first map of this computer, the settings page: User)
struct UserConf {
    std::string nickname;
    std::string email;
    std::string editor;             // the main one (Ctrl+E too when [ui] editor is empty)
    std::string language;           // of the answers: fr | en | empty (the one of the message)
    std::string notes;              // anything the global session should know
};

// The web access: a browser of the same LAN drives claude-cluster after a 6-digit code (one at a time)
struct WebConf {
    bool enabled = false;           // opened at the start (else: web on, the palette, --web)
    int port = 8787;
    bool https = true;              // TLS, self-signed certificate (the microphone of the page needs it; the browser asks once)
    bool approve = true;            // a new device: the code, then the host approves it (the trusted ones: neither)
    std::string remote = "off";     // beyond the LAN: off | tailscale (its network, nothing open) | router (a port of the router, UPnP)
    std::string remoteHost;         // the name the devices use from outside (router: a dynamic DNS name; empty: its public address)
    int remotePort = 0;             // router: the port opened outside (0: the same)
    bool shell = false;             // !command / :r !command allowed from the page (a shell of this computer: off by default)
    std::vector<std::string> settings;  // the sections of the settings a device of the page may change ("all"; empty: none, read only there)
    int ttyPort = 2222;             // the terminal page (the terminal interface in a browser: its own port; 0: off)
    int ttyRemotePort = 0;          // router: the port opened outside for it (0: the same)
};

// The background service (claude-cluster service ...): the wake phrase opens claude-cluster with its voice on
struct ServiceConf {
    std::string frontend = "auto";  // auto: a window, else a terminal | gui: a window (a terminal on its failure) | tty: a terminal
    std::string terminal;           // tty: the terminal and its option to run a command (empty: the first installed)
    std::string voiceMode = "auto"; // the voice of the claude-cluster opened: auto (every sentence) | wake | push
    std::string wakeWord;           // empty: [voice] wake_word
    std::string python = "~/.local/share/voice/venv/bin/python"; // the python of voice-listen (sherpa-onnx)
    std::string source;             // microphone (PulseAudio / PipeWire source), empty: the default one
};

struct UsageConf {
    bool account = true;            // the windows of the Claude account read from its usage endpoint (the token of Claude Code), every 5 min
    std::vector<std::string> transcripts = {"~/.claude/projects"};  // the transcripts of Claude Code summed (every session of this computer); empty: the ledger of claude-cluster only
    std::map<std::string, std::pair<double, double>> prices;        // [usage.prices] model = input $/M tokens (output 5x), or [input, output]
};

struct LimitsConf {                 // [limits]: what the sessions may spend together
    double dailyUsd = 0.0;          // since the local midnight (0: no limit)
    double weeklyUsd = 0.0;         // the last 7 days
    double monthlyUsd = 0.0;        // since the 1st of the local month
    std::string scope = "keys";     // keys: only the turns billed through an API key | all: every turn (the notional cost of a subscription too)
    bool block = true;              // a limit reached: the prompts are refused (false: only warned)
    int warnPercent = 80;           // warned at this share of a limit (0: never before it is reached)
};

struct PolicyConf {                 // [policy]: what the global session may choose as model / thinking for the sub-sessions
    std::string defaultModel;       // the model sessions start from (empty: none)
    std::string defaultEffort;      // the thinking effort they start from (empty: the default of the agent)
    std::string modelMode = "advise";   // free (no default) | advise (the default is the start, deviate for a reason) | force (nothing else)
    std::string effortMode = "advise";
    std::vector<std::string> allowedModels;     // empty: every one
    std::vector<std::string> allowedEfforts;
    bool askCostly = true;          // the costly models / efforts: the user decides each time (Allow / Deny)
    std::vector<std::string> costlyModels = {"opus", "fable"};
    std::vector<std::string> costlyEfforts = {"xhigh", "max"};
};

struct AttachConf {                 // [attachments]: files, images and photos attached to a prompt
    bool enabled = true;
    int maxMb = 20;                 // per file
    int maxFiles = 10;              // per prompt
    int keep = 200;                 // kept per session (the oldest go)
    int maxPixels = 2048;           // an image larger is downscaled to this side (0: as it is)
    bool stripExif = true;          // images re-encoded without EXIF (location, camera); needs magick
    std::string dir;                // stored where (empty: <data>/attachments)
    bool link = true;               // a file of this computer linked by its path, nothing copied (window, terminal, a page on this computer)
    bool globalAttach = true;       // the global session may attach a file to a prompt (MCP session_send attachments): project folder / home only
    int globalMax = 5;              // files per prompt of the global session
};

struct OrchConf {                   // [orchestrator]: the heavy jobs of the sessions queued and limited (docs/orchestrator.md)
    bool enabled = true;
    std::string profile = "low";    // low (1 thread, nice 19, ionice idle) | normal | max
    int maxJobs = 0;                // heavy jobs running at once (0: from the hardware, the profile and the live load)
    int maxPerSession = 2;          // running at once for one session (0: no limit)
    int reservedCpu = 2;            // cores left to the user and the interface
    int reservedRamMb = 3072;       // memory left to them
    int threads = 0;                // parallelism granted to a job (0: from the profile)
    int classMax[5] = {0, 0, 2, 1, 0};  // per class (build, tests, container, gpu, custom): running at once (0: auto)
    int classRam[5] = {2000, 1200, 1500, 2000, 1000};   // per class: memory of a job (MB)
    std::vector<std::string> patterns;  // "text" or "text = class": a command containing it is heavy
    std::vector<std::string> sessionPrio;   // "s3=5": priority of a session
    std::vector<std::string> sessionQuota;  // "s3=1": jobs of a session running at once
    int detectCpuSeconds = 60;      // a command outside the queue burning this many CPU seconds is told (0: off)
    std::string detectAction = "notice";    // off | notice (the interfaces) | tell (its session too)
    double holdLoad = 1.5;          // load per core: nothing new starts
    double reduceLoad = 3.0;        // load per core: the running jobs are slowed (reniced, the lowest stopped)
    int holdMemMb = 3000;           // MemAvailable: nothing new starts
    int reduceMemMb = 1200;
    int oomMemMb = 500;             // MemAvailable: the newest job is killed (kill_on_oom)
    int holdSwapMb = 4096;          // swap used, with the memory short: nothing new starts
    int psiCpuHold = 80;            // pressure stall (avg10, %)
    int psiCpuReduce = 95;
    int psiMemHold = 20;
    int psiMemReduce = 40;
    int psiIoHold = 50;
    int psiIoReduce = 80;
    int resumeAfter = 20;           // seconds under the thresholds before a stopped job resumes
    bool killOnOom = false;
    int agingSeconds = 30;          // a waiting job gains one point of priority every so many seconds
    int focusBoost = 15;            // points of the session the user looks at
    bool subAdd = true;             // a sub-session may queue heavy jobs (MCP)
    bool subKillOwn = true;         // ... kill its own
    bool subSeeOthers = false;      // ... see the jobs of the others
    bool notifyKilled = true;       // a notification when a job is killed
    bool notifyDone = false;        // ... when one finishes
    int history = 200;              // finished jobs kept
    std::string procDir = "/proc";  // a fake tree for the tests
    std::string sysDir = "/sys";
};

struct ServicesConf {
    std::string refresh = "live";   // the Services tab: live (what changes, at once) | interval (every delay s) | manual (R)
    int delay = 2;                  // seconds: interval; the slow parts (systemd, its journal, tailscale) kept so long
};

//----------------------------------------------------------------//
/* CLASS */

class Config {
    private:
        std::filesystem::path _path;
        std::filesystem::file_time_type _mtime{};
        std::string _error;

        // ---------- Pre-Function -------- //
        _cold void defaults_(void);

    public:
        /* ui */
        std::string frontend = "auto";
        std::string matching = "c2dmp";   // completions, palette, settings search: c2dmp (c2dmp-hsm, typos found) | raw (a prefix, contained)
        bool promptOrigin = true;         // the prompts of a transcript coloured by their origin (you / mcp / task + scheduled); false: all as before
        std::string promptColorUser;      // "#rrggbb" of the prompts you typed (empty: the accent of the style)
        std::string promptColorMcp;       // of the prompts of the global session / the cluster (empty: violet)
        std::string promptColorTask;      // of the prompts of the tasks and the schedules (empty: magenta)
        std::string shellContext = "ask"; // !command given to the agent: ask (after it ran) | always | never
        bool sharedCore = true;         // several terminals / windows at once on one core of its own (false: the first one is the core)
        std::string quitBackground = "ask"; // quit with sessions running: ask (keep them in background / stop) | always (kept) | never (stopped)
        bool debug = false;             // [debug] enabled: <data>/debug.log written by every process (core, terminals, windows)
        std::vector<std::string> debugTopics = {"core", "interfaces", "render", "voice", "service", "web", "platform"}; // [debug] topics
        std::string layout = "list";
        std::string guiMode = "gui";            // the window: gui (its own interface) | tty (the terminal interface in it)
        std::string style = "html-dark";
        std::vector<std::string> panels = {"skills", "tokens", "context", "cost", "usage", "git"};
        std::string restore = "ask";
        bool notify = true;                     // desktop notifications at all
        bool notifyGlobal = true;               // the global session answered
        bool notifyDone = true;                 // a sub-session finished its turn
        bool notifyPermission = true;           // a permission waits
        bool notifySound = true;                // a short sound with the notifications (one per kind)
        std::string notifyWhen = "unfocused";   // unfocused: only when claude-cluster is not in front (its popups there) | always
        bool notifyQuestion = true;             // a question of an agent
        bool notifyError = true;                // an error, a crash, the safety check
        bool notifyLimits = true;               // a limit close / reached (5 h, 7 d, the budget)
        bool notifyWeb = true;                  // a device of the web access: asks to connect, connected
        bool notifyTasks = true;                // the task queue done
        bool notifyReminders = true;            // a reminder (schedule add --notify)
        bool notifyApp = true;                  // the Android app: the notifications published to it (the kinds below apply; each device opts in)
        bool notifyReady = true;                // something slow ready: claude-cluster started, the speech model loaded, the map written
        bool vim = false;                       // vim mode of the prompt (Insert / Normal / Visual)
        bool groupByCategory = false;           // the session list grouped under the category of its sessions (the palette, /group, a click on the list title)
        int pasteLines = 3;                     // a paste of more lines becomes [Pasted text #N +L lines] (0: never)
        int pasteChars = 800;                   // or of more characters
        std::string editor;                     // Ctrl+E (empty: nvim, vim, vi, $VISUAL, $EDITOR, emacs -nw, nano)
        std::string imageViewer;                // right click on an image chip (empty: xdg-open)
        std::string imagePreview = "blocks";    // terminal preview: blocks (pixels as half blocks) | chafa (symbols)
        bool typewriter = true;                 // the answers typed as they come (also when they arrive at once)
        int typewriterSpeed = 400;              // characters per second (faster when a long text is behind)

        /* agent */
        std::string provider = "claude";    // [agent] provider: the one of EVERY session (global, existing, new, imported), <provider>[/<model>]
        std::string backend = "claude";     // the backend spec of the new sessions (= provider; --backend overrides it for one run)
        std::string mode = "auto";      // permission mode of the new sessions (empty: the default of the agent)
        std::string effort;             // thinking / reasoning effort of the new sessions (empty: the default of the agent)
        std::string workMethod = "free";    // the work method: free | list-review | one-task | plan-first | custom (Method.hpp)
        std::string workMethodText;         // what the user adds to it (short: injected in the prompts of the sessions)
        bool workMethodDetect = false;      // the global session may propose a method from the habits of the user (the user answers Allow / Deny)
        int workMethodEvery = 20;           // sessions started between two checks of the detection (0: only when asked)
        std::map<std::string, std::vector<std::string>> models; // [models] <provider> = [...]: offered in the palette, /model
        bool allowBypass = true;
        std::vector<std::string> extraArgs;
        std::map<std::string, std::string> commands = {{"claude", "claude"}, {"qwen", "qwen"}, {"opencode", "opencode"}, {"codex", "codex"}};

        /* global */
        bool globalEnabled = true;
        std::string globalBackend = "claude";
        std::string globalCwd;                  // empty: a folder of its own (<data>/global, trusted for Remote Control)
        std::string globalModel;
        std::string globalMode = "bypassPermissions"; // never asked (empty: the mode of [agent])
        std::string globalEffort;               // empty: the one of [agent]
        std::string globalPrompt = "~/.config/claude-cluster/global.md";
        std::string remoteName = "claude-cluster";

        /* sessions */
        int trashDays = 14;
        std::string describe = "model";         // the short description of a session: model (a small model) | prompt (its first one) | off
        std::string describeModel;              // the <provider>/<model> writing them (empty: the provider of the session, haiku for Anthropic)
        int maxParallel = 4;                    // the parallel sessions: sub-sessions working at the same time (the tasks wait past it)
        int taskQueue = 100;                    // the task queue: tasks waiting for a free place, at most
        int historyLimit = 1000;                // the prompt history (↑, Ctrl+R): its newest prompts kept (0: all)
        bool subPin = true;                     // a sub-session may pin an item on itself
        bool pinGlobal = true;                  // the global session may pin an item on any session (MCP session_pin)
        int transcriptLimitMb = 25;             // each session transcript (logs/<id>.jsonl): its newest entries within this size (0: no limit)
        std::string queueGiven = "step";        // a prompt queued while the turn runs: step (given to claude after 1.5 s, taken at its next step) | end (after the turn)
        double budget = 0.0;
        bool questionsToGlobal = true;          // the questions of the sub-sessions shown on the global session (their own one: greyed)
        bool globalAnswers = false;             // the global session may choose the answer of a question of a sub-session
        bool bypassToGlobal = true;             // the permissions a sub-session in bypassPermissions still asks: decided by the global session
        bool globalFork = false;                // the global session may fork a sub-session (MCP session_fork)
        bool archiveOneShot = true;             // a session the global session started for one task (session_spawn one_shot) is closed to the trash once its answer is relayed
        bool globalEdit = false;                // the global session may rewrite the conversation of a sub-session (MCP session_edit)
        bool globalPromptWrite = false;         // the global session may rewrite its own system prompt (MCP config_write global_prompt)
        bool sessionsPromptWrite = true;        // the global session may rewrite the system prompt of the sub-sessions (MCP config_write sessions_prompt)
        std::string sessionsPrompt = "~/.config/claude-cluster/sessions.md"; // system prompt of the sub-sessions (empty: none)

        /* mcp */
        std::map<std::string, bool> mcpTools;   // [mcp_tools] <tool> = false: a tool of the global session off (missing: on)
        std::vector<std::string> mcpServers;    // [mcp] servers: "name: command args" | "name: https://url", given to the global session
        std::vector<std::string> pluginRepos;   // [plugins] repos: links of repositories (plugins / skills offered)
        std::map<std::string, bool> pluginSelect; // [plugins.select] "<repository>:<name>" ("<repository>:*": all) = installed
        std::map<std::string, bool> pluginOff;  // [plugins.off] "<repository>:<name>" = installed but turned off

        cluster::VoiceConf voice;
        cluster::ServiceConf service;
        cluster::ServicesConf services;
        cluster::UsageConf usage;
        cluster::LimitsConf limits;
        cluster::PolicyConf policy;
        cluster::WebConf web;
        cluster::AttachConf attach;
        cluster::UserConf user;
        int machineDays = 30;                   // the map of this computer older: a sync proposed at start (0: never)
        std::string voiceForce;                 // --voice <mode>: the voice on in this mode for this run (over the file)
        std::map<std::string, std::string> keys;
        std::map<std::string, cluster::Style> styles;
        std::map<std::string, cluster::Profile> profiles;
        std::map<std::string, cluster::ProviderConf> providers;

        // ---------- Pre-Function -------- //
        _cold void load(void);                              // creates the default files on the first run
        _cold _nodiscard bool reload(void);                 // true when the file changed and was loaded again
        _cold _nodiscard const cluster::Style& currentStyle(void) const;
        _cold _nodiscard std::string command(const std::string& driver) const;
        _cold _nodiscard static std::string check(const std::string& text);   // a config.toml text: its parse error ("" valid)

        cluster::OrchConf orch;
        // ---------- Function -------- //
        _nodiscard inline const std::filesystem::path& path(void) const {return this->_path;};
        _nodiscard inline const std::string& error(void) const {return this->_error;};   // last parse error (the previous values are kept)
        _nodiscard inline bool mcpTool(const std::string& tool) const {return !this->mcpTools.contains(tool) || this->mcpTools.at(tool);};  // on for the global session

        // ---------- Constructor -------- //
        _cold Config(void);

        // ----------- Destructor --------- //
        _cold ~Config() = default;
};

} // namespace end

#endif /* CLUSTER_CONFIG_H */
