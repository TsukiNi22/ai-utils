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
#include "cluster/Voice.hpp"
#include "cluster/Tools.hpp"
#include <sys/epoll.h>
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

/* constructor */
_cold cluster::Voice::Voice(cluster::Manager& manager)
    : _manager(manager)
{
    this->_manager.onGlobalAnswer = [this](const std::string& text) {this->speak(text);};
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    if (conf.enabled && (conf.mode == "auto" || conf.mode == "wake")) this->start(); // always listening
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
    if (this->_listening) this->stop();
    else this->start();
}

_cold void cluster::Voice::listen_(void)
{
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    std::vector<std::string> args = {"--json", "--partial"};
    if (conf.onlyMe) args.push_back("--only-me");

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
                // partial hypothesis: "\r\033[K<text>" without new line
                errBuffer.append(chunk.data(), static_cast<std::size_t>(size));
                const std::size_t cr = errBuffer.rfind('\r');
                std::string partial = cr == std::string::npos ? errBuffer : errBuffer.substr(cr + 1);
                partial = std::regex_replace(partial, std::regex("\x1b\\[[0-9;]*[A-Za-z]"), "");
                {
                    std::lock_guard<std::mutex> lock(this->_mutex);
                    this->_partial = cluster::one_line(partial, 200);
                }
                if (errBuffer.size() > 8192) errBuffer.erase(0, errBuffer.size() - 1024);
                ++this->_version;
            }
        }
    }
    (void)process.wait();
    this->_listenPid = -1;
    this->_listening = false;
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
    if (text.empty()) return;

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

    // "session <name>: ..." / "<name>: ..." goes straight to that session, the rest to the global one
    cluster::Pending pending{text, GLOBAL_ID, now_ms_() + conf.confirmSeconds * 1000};
    // (speech recognition gives no punctuation: "session <name> ..." works without the colon)
    static const std::regex keyword(R"(^session\s+([\w.-]+)[\s:,]+(.+)$)", std::regex::icase);
    static const std::regex target(R"(^([\w.-]+)\s*[:,]\s*(.+)$)", std::regex::icase);
    std::smatch match;
    if (std::regex_match(text, match, keyword) || std::regex_match(text, match, target)) {
        try {
            pending.target = this->_manager.resolve(match[1].str());
            pending.text = match[2].str();
        } catch (const utils::exception::IException&) {} // not a session name: the whole sentence to the global session
    }
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_pending.push_back(pending);
        this->_partial.clear();
    }
    ++this->_version;
    if (conf.confirmSeconds <= 0) this->confirm();
}

/* pending transcriptions */
_cold void cluster::Voice::tick(void)
{
    std::vector<cluster::Pending> ready;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::int64_t now = now_ms_();
        for (auto it = this->_pending.begin(); it != this->_pending.end();) {
            if (it->deadline <= now) {
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
    } catch (const utils::exception::IException& e) {
        if (this->_manager.onNotice) this->_manager.onNotice({cluster::now(), pending.target, std::string("voice: ") + e.info(), true});
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
    const std::string voice = cluster::language_of(text) == "en" ? conf.voiceEn : conf.voiceFr;

    this->mute(); // a new answer replaces the one being read
    this->say_(summary, voice);
}

_cold void cluster::Voice::say_(const std::string& text, const std::string& voiceName)
{
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    const std::string command = cluster::expand_home(conf.sayCommand).string();
    const std::string voice = !voiceName.empty() ? voiceName : cluster::language_of(text) == "en" ? conf.voiceEn : conf.voiceFr;
    std::thread([this, command, voice, text]() {
        utils::encapsulation::Process process;
        try {
            const int null = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
            if (null != -1) {
                process.dup(null, STDOUT_FILENO);
                process.dup(null, STDERR_FILENO);
            }
            this->_sayPid = process.spawn(command, {"-v", voice, text});
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
    const pid_t pid = this->_sayPid;
    if (pid > 0) ::kill(pid, SIGTERM);
}

_cold std::vector<std::string> cluster::Voice::setup(void) const
{
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    const std::filesystem::path home = cluster::expand_home("~/.local/share/voice");
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
    bool english = false;
    std::error_code error;
    for (const std::filesystem::directory_entry& model: std::filesystem::directory_iterator(home / "models", error))
        english |= model.path().filename().string().find("streaming-zipformer-en") != std::string::npos;
    if (!english) missing.push_back("no English speech model: English is only spoken back (French is recognized)");
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
