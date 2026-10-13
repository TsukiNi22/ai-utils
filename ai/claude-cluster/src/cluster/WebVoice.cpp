/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file WebVoice.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Encapsulation
#include <utils/utils.hpp>
#include "cluster/WebVoice.hpp"
#include "cluster/Tools.hpp"
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iterator>
#include <fcntl.h>
#include <fstream>
#include <chrono>
#include <array>

/* tools */
_cold static std::int64_t now_ms_(void)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* constructor */
_cold cluster::WebVoice::WebVoice(cluster::Manager& manager, cluster::Voice& voice)
    : _manager(manager), _voice(voice)
{
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    this->_dir = (runtime != nullptr && *runtime ? std::filesystem::path(runtime) : cluster::data_dir()) / ("claude-cluster-web-" + std::to_string(::getpid()));
}

_cold cluster::WebVoice::~WebVoice()
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->stop_();
    std::error_code error;
    std::filesystem::remove_all(this->_dir, error);
}

/* recognition */
_cold void cluster::WebVoice::start_(void)
{
    // voice-listen --files on the speech model of the settings: one recording path in, its text out (JSON)
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    std::vector<std::string> args = {"--files", "--json"};
    if (!conf.listenModel.empty()) args.insert(args.end(), {"--model", conf.listenModel});
    if (!cluster::trim(conf.listenLanguage).empty()) args.insert(args.end(), {"--language", cluster::trim(conf.listenLanguage)});
    this->_in = std::make_unique<utils::encapsulation::Pipe>();
    this->_out = std::make_unique<utils::encapsulation::Pipe>();
    this->_in->trigger();
    this->_out->trigger();
    for (const int fd: {this->_in->getRead(), this->_in->getWrite(), this->_out->getRead(), this->_out->getWrite()})
        ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
    this->_process = std::make_unique<utils::encapsulation::Process>();
    this->_process->dup(this->_in->getRead(), STDIN_FILENO);
    this->_process->dup(this->_out->getWrite(), STDOUT_FILENO);
    const int null = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (null != -1) this->_process->dup(null, STDERR_FILENO);
    try {
        (void)this->_process->spawn(cluster::expand_home(conf.listenCommand).string(), args);
    } catch (const utils::exception::IException&) {
        if (null != -1) ::close(null);
        this->_process.reset();
        throw;
    }
    if (null != -1) ::close(null);
    this->_in->closeRead();
    this->_out->closeWrite();
    this->_buffer.clear();
    this->_model = conf.listenModel;
    this->_language = conf.listenLanguage;
}

_cold void cluster::WebVoice::stop_(void)
{
    if (!this->_process) return;
    this->_in.reset();             // its stdin closed: it ends by itself
    this->_process->kill();
    (void)this->_process->wait();
    this->_process.reset();
    this->_out.reset();
}

_cold std::filesystem::path cluster::WebVoice::file_(const std::string& kind)
{
    std::error_code error;
    std::filesystem::create_directories(this->_dir, error);
    ::chmod(this->_dir.c_str(), S_IRWXU);
    return this->_dir / (kind + "-" + std::to_string(++this->_count) + ".wav");
}

_cold std::string cluster::WebVoice::transcribe(const std::string& wav)
{
    // the recording written, its path given to voice-listen, its text read (the model loads once: ~10 s the first time)
    std::lock_guard<std::mutex> lock(this->_mutex);
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    if (this->_process && (conf.listenModel != this->_model || conf.listenLanguage != this->_language)) this->stop_(); // the settings changed
    if (!this->_process) this->start_();
    this->_used = now_ms_();
    const std::filesystem::path path = this->file_("rec");
    std::ofstream(path, std::ios::binary) << wav;
    const std::string line = path.string() + "\n";
    std::error_code error;
    if (::write(this->_in->getWrite(), line.data(), line.size()) != static_cast<ssize_t>(line.size())) {
        this->stop_();
        std::filesystem::remove(path, error);
        throw utils::exception::ErrorException(utils::exception::InternalCode::Communication, "voice-listen ended");
    }
    utils::encapsulation::Poll poll;
    poll.link(this->_out->getRead(), EPOLLIN);
    std::array<char, 4096> chunk{};
    const std::int64_t end = now_ms_() + 120000;
    while (this->_buffer.find('\n') == std::string::npos) {
        const std::int64_t left = end - now_ms_();
        const ssize_t size = left > 0 && !poll.wait(static_cast<int>(left)).empty() ? ::read(this->_out->getRead(), chunk.data(), chunk.size()) : 0;
        if (size <= 0) {
            this->stop_();
            std::filesystem::remove(path, error);
            throw utils::exception::ErrorException(utils::exception::InternalCode::Communication, "no text from voice-listen (see claude-cluster engines)");
        }
        this->_buffer.append(chunk.data(), static_cast<std::size_t>(size));
    }
    std::filesystem::remove(path, error);
    const std::size_t nl = this->_buffer.find('\n');
    const std::string answer = this->_buffer.substr(0, nl);
    this->_buffer.erase(0, nl + 1);
    this->_used = now_ms_();
    try {
        const cluster::Json json = cluster::Json::parse(answer);
        if (json.contains("error")) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, json.value("error", std::string()));
        return json.value("text", std::string());
    } catch (const cluster::Json::exception&) {
        return "";
    }
}

_cold void cluster::WebVoice::tick(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_process && now_ms_() - this->_used > 600000) this->stop_();
}

/* speech */
_cold std::string cluster::WebVoice::speech(const std::string& text)
{
    // voice-say -o: the engine and the voice of the settings for the language of the text, Piper when it fails
    std::vector<std::string> args = this->_voice.sayArgs(text);
    const std::filesystem::path path = this->file_("say");
    args.insert(args.end() - 1, {"-o", path.string()});
    utils::encapsulation::Process process;
    const int null = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (null != -1) {
        process.dup(null, STDOUT_FILENO);
        process.dup(null, STDERR_FILENO);
    }
    (void)process.spawn(args[0], std::vector<std::string>(args.begin() + 1, args.end()));
    if (null != -1) ::close(null);
    const utils::encapsulation::Status status = process.wait();
    std::ifstream in(path, std::ios::binary);
    const std::string wav((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::error_code error;
    std::filesystem::remove(path, error);
    if (!status.exited || status.code != 0 || wav.size() <= 44)
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no speech made (voice-say, claude-cluster engines)");
    return wav;
}
