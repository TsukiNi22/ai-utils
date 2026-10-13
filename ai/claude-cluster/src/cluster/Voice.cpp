/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Voice.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Encapsulation
#include <utils/utils.hpp>
#include "cluster/VoiceEngines.hpp"
#include "cluster/Voice.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Auth.hpp"
#include <sys/epoll.h>
#include <functional>
#include <algorithm>
#include <unistd.h>
#include <signal.h>
#include <optional>
#include <fcntl.h>
#include <chrono>
#include <cctype>
#include <array>
#include <regex>

/* tools */
_cold static std::int64_t now_ms_(void)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

_cold static std::string engine_voice_(const std::string& voices, const std::string& language)
{
    // [voice] say_voices "fr=ff_siwis, en=bf_emma, es=kokoro:ef_dora": the one of the language, empty: none
    for (const std::string& entry: cluster::split(voices, ',')) {
        const std::size_t equal = entry.find('=');
        if (equal != std::string::npos && cluster::lower(cluster::trim(entry.substr(0, equal))) == language) return cluster::trim(entry.substr(equal + 1));
    }
    return "";
}

_cold static std::string normalize_(const std::string& text)
{
    // lower case, punctuation as spaces, single spaces: "Ok, Claude !" -> "ok claude"
    std::string out;
    for (const char c: cluster::lower(text)) {
        const bool keep = std::isalnum(static_cast<unsigned char>(c)) || (static_cast<unsigned char>(c) & 0x80);
        if (keep) out += c;
        else if (!out.empty() && out.back() != ' ') out += ' ';
    }
    return cluster::trim(out);
}

_cold static std::optional<std::string> wake_rest_(const std::string& text, const std::string& phrases)
{
    // The words after the first wake phrase found (any variant of the comma list), std::nullopt when absent
    const std::string heard = " " + normalize_(text) + " ";
    for (const std::string& variant: cluster::split(phrases, ',')) {
        const std::string phrase = " " + normalize_(variant) + " ";
        if (phrase.size() <= 2) continue;
        const std::size_t at = heard.find(phrase);
        if (at == std::string::npos) continue;
        // keep the original words after the phrase (same number of words skipped)
        const std::size_t skip = cluster::split(heard.substr(0, at + phrase.size()), ' ').size();
        const std::vector<std::string> words = cluster::split(text, ' ');
        std::string rest;
        for (std::size_t i = skip; i < words.size(); ++i)
            rest += (rest.empty() ? "" : " ") + words[i];
        while (!rest.empty() && (rest.front() == ',' || rest.front() == ':' || rest.front() == '!' || rest.front() == '.')) rest = cluster::trim(rest.substr(1));
        return rest;
    }
    return std::nullopt;
}

_cold static bool only_punctuation_(const std::string& text)
{
    // ".", "…", "?!" heard alone (a breath, a noise): no letter nor digit, nothing to send
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            if (std::isalnum(c)) return false;
        } else if (c == 0xE2 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x80) {
            i += 2; // general punctuation: … – — ’ “ ”
        } else if (c == 0xC2 && i + 1 < text.size()) {
            i += 1; // « » ¡ ¿, the no-break space
        } else {
            return false; // a letter (é, ç...)
        }
    }
    return true;
}

/* constructor */
_cold cluster::Voice::Voice(cluster::Manager& manager)
    : _manager(manager)
{
    this->_manager.onGlobalAnswer = [this](const std::string& text) {this->speak(text);};
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    if (conf.enabled) this->start(); // loaded now: its model takes ~10 s
}

_cold cluster::Voice::~Voice()
{
    this->_manager.onGlobalAnswer = nullptr;
    this->stop();
    this->mute();
}

/* listening */
_cold void cluster::Voice::start(void)
{
    if (this->_listening || !this->_manager.config().voice.enabled) return;
    if (this->_listener.joinable()) this->_listener.join();
    this->_listening = true;
    this->_ready = false;
    this->_lastStart = now_ms_();
    this->_listener = std::thread(&cluster::Voice::listen_, this);
    ++this->_version;
}

_cold void cluster::Voice::stop(void)
{
    this->_listening = false;
    const pid_t pid = this->_listenPid;
    if (pid > 0) ::kill(pid, SIGTERM);
    if (this->_listener.joinable()) this->_listener.join();
    ++this->_version;
}

_cold void cluster::Voice::toggle(void)
{
    // push mode: voice-listen stays loaded, the key opens / closes what is taken; other modes: pause / resume
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    if (!conf.enabled) return;
    if (conf.mode == "push") {
        if (!this->_listening) this->start();
        this->_open = !this->_open;
        if (!this->_open) {
            // closed: what was said is sent after the end of the last sentence (grace) and the confirmation
            this->_graceUntil = now_ms_() + 2500;
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Pending& pending: this->_pending)
                if (pending.deadline == 0) pending.deadline = this->_graceUntil + conf.confirmSeconds * 1000;
        }
        ++this->_version;
        return;
    }
    this->_paused = this->_listening.load();
    if (this->_listening) this->stop();
    else this->start();
}

_cold void cluster::Voice::listen_(void)
{
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    std::vector<std::string> args = {"--json", "--partial"};
    if (conf.onlyMe) args.push_back("--only-me");
    if (!conf.listenModel.empty()) args.insert(args.end(), {"--model", conf.listenModel}); // else its own choice (voice-models)
    if (conf.endSilence > 0.0) args.insert(args.end(), {"--end-silence", std::to_string(conf.endSilence)});
    if (!cluster::trim(conf.listenLanguage).empty()) args.insert(args.end(), {"--language", cluster::trim(conf.listenLanguage)});
    this->_model = conf.listenModel;
    this->_endSilence = conf.endSilence;
    this->_language = conf.listenLanguage;

    // voice-listen: one JSON object per sentence on stdout, the partial hypothesis on stderr
    utils::encapsulation::Pipe out;
    utils::encapsulation::Pipe err;
    out.trigger();
    err.trigger();
    for (const int fd: {out.getRead(), out.getWrite(), err.getRead(), err.getWrite()})
        ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
    utils::encapsulation::Process process;
    process.dup(out.getWrite(), STDOUT_FILENO);
    process.dup(err.getWrite(), STDERR_FILENO);
    try {
        this->_listenPid = process.spawn(cluster::expand_home(conf.listenCommand).string(), args);
    } catch (const utils::exception::IException& e) {
        this->_listening = false;
        return;
    }
    out.closeWrite();
    err.closeWrite();

    utils::encapsulation::Poll poll;
    poll.link(out.getRead(), EPOLLIN);
    poll.link(err.getRead(), EPOLLIN);
    std::string outBuffer;
    std::string errBuffer;
    std::array<char, 4096> chunk{};
    int open = 2;
    while (open > 0) {
        for (const epoll_event& event: poll.wait(300)) {
            const int fd = event.data.fd;
            const ssize_t size = ::read(fd, chunk.data(), chunk.size());
            if (size <= 0) {
                poll.unlink(fd);
                --open;
                continue;
            }
            if (fd == out.getRead()) {
                outBuffer.append(chunk.data(), static_cast<std::size_t>(size));
                for (std::size_t nl = outBuffer.find('\n'); nl != std::string::npos; nl = outBuffer.find('\n')) {
                    const std::string line = outBuffer.substr(0, nl);
                    outBuffer.erase(0, nl + 1);
                    try {
                        this->heard_(cluster::Json::parse(line).value("text", std::string()));
                    } catch (const cluster::Json::exception&) {}
                }
            } else {
                // partial hypothesis: "\r\033[K<text>" without new line ("voice-listen: ready" once the model is loaded)
                errBuffer.append(chunk.data(), static_cast<std::size_t>(size));
                if (errBuffer.find("voice-listen:") != std::string::npos) { // its own messages ("ready"), not a partial text
                    if (errBuffer.find("ready") != std::string::npos) this->_ready = true;
                    errBuffer.clear();
                    ++this->_version;
                    continue;
                }
                if (conf.mode == "push" && !this->_open) {
                    errBuffer.clear();
                    continue;
                }
                const std::size_t cr = errBuffer.rfind('\r');
                std::string partial = cr == std::string::npos ? errBuffer : errBuffer.substr(cr + 1);
                partial = std::regex_replace(partial, std::regex("\x1b\\[[0-9;]*[A-Za-z]"), "");
                {
                    std::lock_guard<std::mutex> lock(this->_mutex);
                    this->_partial = cluster::tail_utf8(cluster::one_line(partial, 100000), 300); // its end: what is being said
                }
                if (!cluster::trim(partial).empty()) this->_lastSound = now_ms_(); // the user speaks: what waits is held
                if (errBuffer.size() > 8192) errBuffer.erase(0, errBuffer.size() - 1024);
                ++this->_version;
            }
        }
    }
    (void)process.wait();
    this->_listenPid = -1;
    this->_listening = false;
    this->_ready = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_partial.clear();
    }
    ++this->_version;
}

_cold void cluster::Voice::heard_(const std::string& heard)
{
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    std::string text = cluster::trim(heard);
    if (text.empty() || only_punctuation_(text)) return; // "." alone: ignored
    if (conf.mode == "push" && !this->_open && now_ms_() > this->_graceUntil) return; // push-to-talk closed
    if (this->command_(text)) return; // "annule" / "envoie" said alone: on what waits (no wake phrase needed)
    text = cluster::vocabulary_correct(text, this->terms_()); // the proper names the recognition bent
    const bool late = conf.mode == "push" && !this->_open; // the end of a sentence after F5: joins what is waiting

    // Wake mode: nothing is taken before the wake phrase; the phrase with a command sends it, the phrase alone
    // arms the listening for wake_seconds (the next sentence is the command)
    if (conf.mode == "wake") {
        const std::int64_t now = now_ms_();
        const std::optional<std::string> rest = wake_rest_(text, conf.wakeWord);
        if (rest) {
            if (rest->empty()) {
                this->_armedUntil = now + conf.wakeSeconds * 1000;
                ++this->_version;
                if (!conf.wakeReply.empty()) this->say_(conf.wakeReply);
                return;
            }
            text = *rest;
        } else if (now <= this->_armedUntil) {
            // the sentence following the phrase alone
        } else {
            return;
        }
        this->_armedUntil = 0;
    }

    // "session <name> ..." / "<name>: ..." goes straight to that session, the rest to the global one; push mode: sent
    // when F5 closes the listening (deadline 0 until then)
    const bool holding = conf.mode == "push" && this->_open;
    cluster::Pending pending{text, GLOBAL_ID, holding ? 0 : now_ms_() + conf.confirmSeconds * 1000};
    static const std::regex keyword(R"(^session\s+([\w.-]+)[\s:,]+(.+)$)", std::regex::icase);
    static const std::regex target(R"(^([\w.-]+)\s*:\s*(.+)$)", std::regex::icase);
    std::smatch match;
    if (std::regex_match(text, match, keyword)) {
        try {
            pending.target = this->_manager.resolve(match[1].str());
            pending.text = match[2].str();
        } catch (const utils::exception::IException&) {} // not a session name: the whole sentence to the global session
    } else if (std::regex_match(text, match, target)) {
        // "<name>: ..." only for the exact id / name of a session (a first word of the sentence is not one)
        const std::string word = cluster::lower(match[1].str());
        try {
            const std::string id = this->_manager.resolve(word);
            const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(id);
            if (snap && (id == word || cluster::lower(snap->spec.name) == word)) {
                pending.target = id;
                pending.text = match[2].str();
            }
        } catch (const utils::exception::IException&) {}
    }
    {
        // the next sentences of the same utterance (still waiting, same target) go in the same prompt
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (!this->_pending.empty() && this->_pending.back().target == pending.target) {
            this->_pending.back().text += " " + pending.text;
            if (!late) this->_pending.back().deadline = pending.deadline; // its countdown full again
            this->_pending.back().paused = false;
        } else {
            this->_pending.push_back(pending);
        }
        this->_partial.clear();
    }
    ++this->_version;
    if (conf.confirmSeconds <= 0 && !holding) this->confirm();
}

/* voice commands and vocabulary */
_cold std::string cluster::Voice::voiceWord(const std::string& text) const
{
    // a sentence that is only a cancel / send word (no case, accent nor punctuation): "cancel" / "send", else empty
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    const std::string said = cluster::vocabulary_key(text);
    cluster::VoiceWords words = cluster::voice_words(cluster::voice_words_path(conf.words)); // edited: taken at once
    words.cancel.insert(words.cancel.end(), conf.cancelWords.begin(), conf.cancelWords.end());
    words.send.insert(words.send.end(), conf.sendWords.begin(), conf.sendWords.end());
    const std::function<bool(const std::vector<std::string>&)> is = [&](const std::vector<std::string>& list) {
        return std::any_of(list.begin(), list.end(), [&](const std::string& word) {return cluster::vocabulary_key(word) == said;});
    };
    if (said.empty()) return "";
    return is(words.cancel) ? "cancel" : is(words.send) ? "send" : "";
}

_cold bool cluster::Voice::command_(const std::string& text)
{
    // a sentence that is only a cancel / send word: on the transcriptions waiting; cancel with nothing waiting stops the
    // answer being read and the turn of the global session
    const std::string word = this->voiceWord(text);
    if (word.empty()) return false;
    bool waiting = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        waiting = !this->_pending.empty();
    }
    const std::function<void(const std::string&)> notice = [&](const std::string& what) {
        this->_manager.notice(GLOBAL_ID, "voice: \"" + text + "\": " + what);
    };
    if (word == "cancel") {
        if (waiting) {
            this->cancel();
            notice("the transcription waiting dropped");
            return true;
        }
        const std::optional<cluster::Snapshot> global = this->_manager.snapshot(GLOBAL_ID);
        const bool working = global && (global->state == cluster::State::Working || global->state == cluster::State::Waiting);
        if (!this->speaking() && !working) return false; // nothing to stop: a prompt like the others
        this->mute();
        if (working) {
            try {
                this->_manager.interrupt(GLOBAL_ID);
            } catch (const utils::exception::IException&) {}
        }
        notice(working ? "the answer stopped" : "the reading stopped");
        return true;
    }
    if (waiting && word == "send") {
        this->confirm();
        notice("sent now");
        return true;
    }
    return false;
}

_cold std::vector<cluster::Term> cluster::Voice::terms_(void)
{
    // the file and the map of this computer read again when they changed; the sessions and the user every time
    const cluster::Config& config = this->_manager.config();
    const std::filesystem::path file = cluster::vocabulary_path(config.voice.vocabulary);
    const std::filesystem::path machine = cluster::config_dir() / "machine.md";
    std::error_code error;
    const std::filesystem::file_time_type fileTime = std::filesystem::last_write_time(file, error);
    const std::filesystem::file_time_type machineTime = std::filesystem::last_write_time(machine, error);
    if (fileTime != this->_fileTime || machineTime != this->_machineTime || this->_fileTerms.empty()) {
        this->_fileTerms = cluster::vocabulary_file(file);
        for (const cluster::Term& term: cluster::vocabulary_projects(machine))
            if (std::none_of(this->_fileTerms.begin(), this->_fileTerms.end(), [&](const cluster::Term& t) {return t.keys.front() == term.keys.front();}))
                this->_fileTerms.push_back(term);
        this->_fileTime = fileTime;
        this->_machineTime = machineTime;
    }
    std::vector<cluster::Term> terms = this->_fileTerms;
    for (const cluster::Snapshot& snap: this->_manager.list(false))
        cluster::vocabulary_add(terms, snap.spec.name);
    cluster::vocabulary_add(terms, config.user.nickname);
    if (const char* login = std::getenv("USER")) cluster::vocabulary_add(terms, login);
    return terms;
}

/* pending transcriptions */
_cold void cluster::Voice::tick(void)
{
    // voice-listen follows the config: started when the voice is enabled (again 10 s after a crash), stopped when not
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    if (conf.enabled && !this->_listening && !this->_paused && now_ms_() - this->_lastStart > 10000) this->start();
    else if (!conf.enabled && this->_listening) this->stop();
    else if (this->_listening && (conf.listenModel != this->_model || conf.endSilence != this->_endSilence || conf.listenLanguage != this->_language)) {
        // voice-listen again on them
        this->stop();
        this->start();
    }
    std::vector<cluster::Pending> ready;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::int64_t now = now_ms_();
        // the user speaks ("…" / live text), or stopped less than 2 s ago (the end of the sentence still decoded): its
        // countdown stopped and full again (confirm_seconds) once the new text is added - or sent / dropped by a word
        const bool speaking = !cluster::trim(this->_partial).empty() || now - this->_lastSound < 2000;
        for (cluster::Pending& pending: this->_pending) {
            if (pending.deadline == 0) continue; // push-to-talk still open
            if (speaking) pending.deadline = now + conf.confirmSeconds * 1000;
            if (pending.paused != speaking) ++this->_version;
            pending.paused = speaking;
        }
        for (auto it = this->_pending.begin(); it != this->_pending.end();) {
            if (it->deadline != 0 && it->deadline <= now && !speaking) { // 0: push-to-talk still open
                ready.push_back(*it);
                it = this->_pending.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (const cluster::Pending& pending: ready)
        this->dispatch_(pending);
    if (!ready.empty()) ++this->_version;
}

_cold void cluster::Voice::dispatch_(const cluster::Pending& pending)
{
    try {
        // The global session knows a voice message can hold misheard words (global.md)
        this->_manager.send(pending.target, pending.target == GLOBAL_ID ? "[voice] " + pending.text : pending.text);
        this->_manager.addHistory({pending.text, {}, {}}, pending.target); // said by the user: in the history
    } catch (const utils::exception::IException& e) {
        this->_manager.notice(pending.target, std::string("voice: ") + e.info(), true);
    }
}

_cold void cluster::Voice::confirm(void)
{
    std::vector<cluster::Pending> ready;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        ready.swap(this->_pending);
    }
    for (const cluster::Pending& pending: ready)
        this->dispatch_(pending);
    ++this->_version;
}

_cold void cluster::Voice::cancel(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_pending.clear();
    ++this->_version;
}

_cold void cluster::Voice::cancelLast(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_pending.empty()) this->_pending.pop_back();
    ++this->_version;
}

_cold void cluster::Voice::edit(const std::string& text)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_pending.empty()) return;
    this->_pending.back().text = text;
    this->_pending.back().deadline = now_ms_() + this->_manager.config().voice.confirmSeconds * 1000;
    ++this->_version;
}

/* speech */
_cold void cluster::Voice::speak(const std::string& text)
{
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    if (!conf.enabled || !conf.speak || text.empty()) return;
    const std::string summary = cluster::summary_of(text, conf.summaryChars);
    if (summary.empty()) return;

    this->mute(); // a new answer replaces the one being read
    this->say_(summary);
}

_cold std::vector<std::string> cluster::Voice::sayArgs(const std::string& text) const
{
    // the engine and its voice for the language of the text ([voice] say_voices, else its default one); the Piper voice
    // of the language: the engine piper, the fallback of the others (no key, quota, model missing)
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    const std::string command = cluster::expand_home(conf.sayCommand).string();
    const std::string language = cluster::language_of(text);
    const std::string engine = cluster::trim(conf.sayEngine).empty() ? std::string("piper") : cluster::trim(conf.sayEngine);
    const std::string piper = language == "en" ? conf.voiceEn : language == "fr" ? conf.voiceFr : "";
    std::string voice = engine_voice_(conf.sayVoices, language);
    if (voice.empty() && engine == "piper") voice = piper;
    std::vector<std::string> args = {command, "-e", engine, "-l", language, "-f", piper};
    if (!voice.empty()) args.insert(args.end(), {"-v", voice});
    args.push_back(text);
    return args;
}

_cold void cluster::Voice::say_(const std::string& text)
{
    const std::vector<std::string> args = this->sayArgs(text);
    std::thread([this, args]() {
        utils::encapsulation::Process process;
        try {
            const int null = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
            if (null != -1) {
                process.dup(null, STDOUT_FILENO);
                process.dup(null, STDERR_FILENO);
            }
            // setsid: voice-say leads a group of its own, its piper | pacat stopped with it (mute: the whole group)
            if (cluster::has_command("setsid")) this->_sayPid = process.spawn("setsid", args);
            else this->_sayPid = process.spawn(args[0], std::vector<std::string>(args.begin() + 1, args.end()));
            if (null != -1) ::close(null);
            ++this->_version;
            (void)process.wait();
        } catch (const utils::exception::IException&) {}
        this->_sayPid = -1;
        ++this->_version;
    }).detach();
}

_cold void cluster::Voice::mute(void)
{
    // the group of voice-say (its piper | pacat): the reading stops at once, claude-cluster closed too
    const pid_t pid = this->_sayPid;
    if (pid <= 0) return;
    if (::kill(-pid, SIGTERM) != 0) ::kill(pid, SIGTERM);
}

_cold std::vector<std::string> cluster::voice_missing(const cluster::Config& config)
{
    const cluster::VoiceConf& conf = config.voice;
    const std::filesystem::path home = cluster::expand_home("~/.local/share/voice");
    const cluster::Auth auth(config);
    std::vector<std::string> missing;

    if (!cluster::has_command(conf.listenCommand)) missing.push_back("`" + conf.listenCommand + "` not found (voice-text tools: ~/testing/voice-text)");
    if (!cluster::has_command(conf.sayCommand)) missing.push_back("`" + conf.sayCommand + "` not found (voice-text tools: ~/testing/voice-text)");
    if (conf.onlyMe && !std::filesystem::exists(home / "speaker.json"))
        missing.push_back("your voice is not enrolled (only_me): run `voice-enroll` (then `voice-enroll --tune`)");
    for (const std::string& voice: {conf.voiceFr, conf.voiceEn}) {
        const std::string file = voice.find('_') == std::string::npos ? "fr_FR-" + voice : voice;
        if (!std::filesystem::exists(home / "voices" / (file + ".onnx")))
            missing.push_back("voice " + voice + " not installed: `voice-voices set " + voice + "` (downloads it)");
    }
    // French, English and mixed: Parakeet (or Whisper); the French streaming model alone recognizes French only
    const bool english = std::filesystem::exists(home / "models" / "sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8")
        || std::filesystem::exists(home / "models" / "sherpa-onnx-whisper-small");
    if (!english) missing.push_back("no English / mixed speech model (only French is recognized): `voice-models install parakeet`");
    if (conf.listenModel == "kroko-fr") missing.push_back("[voice] listen_model = kroko-fr: French only (faster, live text)");
    // the engines chosen: their model, their API key (the local model / Piper meanwhile)
    const std::array<std::pair<std::string, std::string>, 2> chosen = {{{"listen", conf.listenModel}, {"say", conf.sayEngine}}};
    for (const auto &[kind, id]: chosen) {
        const cluster::VoiceEngine* engine = cluster::voice_engine(kind, id);
        if (engine == nullptr) {
            if (!id.empty()) missing.push_back("[voice] " + std::string(kind == "listen" ? "listen_model" : "say_engine") + " = " + id + ": unknown engine (claude-cluster engines)");
            continue;
        }
        const std::string instead = kind == "listen" ? "the local model meanwhile" : "Piper meanwhile";
        if (engine->local && !cluster::voice_engine_installed(*engine) && !engine->install.empty())
            missing.push_back(engine->id + " not installed: `" + engine->install + "` (" + instead + ")");
        const std::optional<cluster::Provider> provider = engine->provider.empty() ? std::nullopt : auth.find(engine->provider);
        if (provider && !auth.status(*provider).first)
            missing.push_back("no " + engine->id + " API key: `claude-cluster auth login " + engine->id + "` (made on " + engine->keyLink + "; " + instead + ")");
    }
    if (cluster::trim(conf.sayEngine) != "piper" && !cluster::trim(conf.sayEngine).empty() && !std::filesystem::exists(home / "bin" / "voice-tts"))
        missing.push_back("voice-tts not found (" + (home / "bin").string() + "): the engine " + conf.sayEngine + " needs it (Piper meanwhile)");
    return missing;
}

// xstyle: ignore-next CPP-SINGLE-STATEMENT (uses now_ms_, a helper of this file)
bool cluster::Voice::armed(void) const
{
    return now_ms_() <= this->_armedUntil;
}

std::vector<cluster::Pending> cluster::Voice::pending(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    std::vector<cluster::Pending> list = this->_pending;
    if (!this->_partial.empty()) list.push_back({this->_partial + " ...", "", 0});
    return list;
}
