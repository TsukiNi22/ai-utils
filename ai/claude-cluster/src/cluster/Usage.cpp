/**************************************************************\
Edition:
##  @date 17/10/2026 by @author Tsukini

File Name:
##  @file Usage.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Tools.hpp"
#include "cluster/Demo.hpp"
#include "cluster/Usage.hpp"
#include <functional>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <atomic>
#include <cstdio>
#include <ctime>

#define PRICES_FILE "prices.json"
#define ACCOUNT_URL "https://api.anthropic.com/api/oauth/usage"

/* tools */
_cold static std::filesystem::path prices_file_(void)
{
    return cluster::data_dir() / PRICES_FILE;
}

_cold static std::mutex& prices_mutex_(void)
{
    static std::mutex mutex;
    return mutex;
}

_cold static cluster::Price priced_(const double input, const double output, const std::string& source)
{
    // Anthropic: the cache written 1.25x (5 min) / 2x (1 h), read 0.1x
    return {input, output, input * 1.25, input * 2.0, input * 0.1, source};
}

_cold static std::int64_t iso_time_(const std::string& text)
{
    // "2026-10-11T21:02:04.502Z" -> unix seconds (0: not a date)
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(text.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6) return 0;
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    return static_cast<std::int64_t>(::timegm(&tm));
}

_cold static cluster::Usage usage_of_(const cluster::Json& usage)
{
    cluster::Usage result;
    if (!usage.is_object()) return result;
    result.input = usage.value("input_tokens", std::int64_t{0});
    result.output = usage.value("output_tokens", std::int64_t{0});
    result.cacheRead = usage.value("cache_read_input_tokens", std::int64_t{0});
    result.cacheCreation = usage.value("cache_creation_input_tokens", std::int64_t{0});
    if (usage.contains("cache_creation") && usage["cache_creation"].is_object())
        result.cacheCreation1h = usage["cache_creation"].value("ephemeral_1h_input_tokens", std::int64_t{0});
    if (usage.contains("output_tokens_details") && usage["output_tokens_details"].is_object())
        result.thinking = usage["output_tokens_details"].value("thinking_tokens", std::int64_t{0});
    return result;
}

_cold static void sum_json_(cluster::Json& json, const cluster::UsageSum& sum)
{
    json = {{"input", sum.input}, {"cache_write", sum.cacheWrite}, {"cache_write_1h", sum.cacheWrite1h}, {"cache_read", sum.cacheRead}, {"output", sum.output},
        {"thinking", sum.thinking}, {"turns", sum.turns}};
}

_cold static cluster::UsageSum sum_of_(const cluster::Json& json)
{
    cluster::UsageSum sum;
    if (!json.is_object()) return sum;
    sum.input = json.value("input", std::int64_t{0});
    sum.cacheWrite = json.value("cache_write", std::int64_t{0});
    sum.cacheWrite1h = json.value("cache_write_1h", std::int64_t{0});
    sum.cacheRead = json.value("cache_read", std::int64_t{0});
    sum.output = json.value("output", std::int64_t{0});
    sum.thinking = json.value("thinking", std::int64_t{0});
    sum.turns = json.value("turns", 0);
    sum.tokens = sum.input + sum.cacheWrite + sum.output;
    sum.cache = sum.cacheRead;
    return sum;
}

/* prices */
_cold std::optional<cluster::Price> cluster::price_of(const cluster::Config& config, const std::string& model)
{
    // the config first ([usage.prices] model = input, or [input, output])
    for (const auto &[name, price]: config.usage.prices)
        if (model == name || model.starts_with(name)) return priced_(price.first, price.second > 0.0 ? price.second : price.first * 5.0, "config");
    // then the one learned from the cost Claude Code told
    {
        std::lock_guard<std::mutex> lock(prices_mutex_());
        if (const std::optional<cluster::Json> learned = cluster::read_json(prices_file_()); learned && learned->is_object() && learned->contains(model)
            && (*learned)[model].is_object() && (*learned)[model].value("input", 0.0) > 0.0)
            return priced_((*learned)[model].value("input", 0.0), (*learned)[model].value("input", 0.0) * 5.0, "learned");
    }
    // the public table (the model names of the API, their families)
    static const std::vector<std::pair<std::string, std::pair<double, double>>> table = {
        {"claude-opus-4", {15.0, 75.0}}, {"claude-sonnet-4", {3.0, 15.0}}, {"claude-haiku-4", {1.0, 5.0}}, {"claude-3-7-sonnet", {3.0, 15.0}},
        {"claude-3-5-sonnet", {3.0, 15.0}}, {"claude-3-5-haiku", {0.8, 4.0}}, {"claude-3-opus", {15.0, 75.0}}, {"claude-3-haiku", {0.25, 1.25}}};
    for (const auto &[prefix, price]: table)
        if (model.starts_with(prefix)) return priced_(price.first, price.second, "table");
    return std::nullopt;
}

_cold std::optional<double> cluster::cost_of(const cluster::Config& config, const std::string& model, const cluster::Usage& usage)
{
    const std::optional<cluster::Price> price = cluster::price_of(config, model);
    if (!price) return std::nullopt;
    const double write5m = static_cast<double>(std::max<std::int64_t>(0, usage.cacheCreation - usage.cacheCreation1h));
    return (static_cast<double>(usage.input) * price->input + static_cast<double>(usage.output) * price->output + write5m * price->cacheWrite5m
        + static_cast<double>(usage.cacheCreation1h) * price->cacheWrite1h + static_cast<double>(usage.cacheRead) * price->cacheRead) / 1000000.0;
}

_cold void cluster::price_learn(const std::string& model, const cluster::Usage& usage, const double cost)
{
    // the input price implied by the cost (output 5x, the cache 1.25x / 2x / 0.1x): kept as a weighted mean over the turns
    if (model.empty() || cost <= 0.0) return;
    const double write5m = static_cast<double>(std::max<std::int64_t>(0, usage.cacheCreation - usage.cacheCreation1h));
    const double units = static_cast<double>(usage.input) + write5m * 1.25 + static_cast<double>(usage.cacheCreation1h) * 2.0 + static_cast<double>(usage.cacheRead) * 0.1
        + static_cast<double>(usage.output) * 5.0;
    if (units <= 0.0) return;
    const double input = cost / units * 1000000.0;
    if (input < 0.01 || input > 500.0) return; // not a price (a cost of several turns at once, a rounding)
    std::lock_guard<std::mutex> lock(prices_mutex_());
    cluster::Json learned = cluster::read_json(prices_file_()).value_or(cluster::Json::object());
    if (!learned.is_object()) learned = cluster::Json::object();
    const double weight = learned[model].is_object() ? learned[model].value("weight", 0.0) : 0.0;
    const double known = learned[model].is_object() ? learned[model].value("input", 0.0) : 0.0;
    const double total = weight + units;
    learned[model] = {{"input", (known * weight + input * units) / total}, {"weight", std::min(total, 1e9)}, {"at", cluster::now()}};
    cluster::write_json(prices_file_(), learned);
}

_cold std::string cluster::vendor_of_model(const std::string& model)
{
    if (model.empty() || model == "<synthetic>") return "";
    return model.starts_with("claude") ? "anthropic" : "other";
}

_cold std::string cluster::usage_detail(const cluster::Usage& usage)
{
    std::string text = "in " + cluster::human_tokens(usage.input) + " · cache w " + cluster::human_tokens(usage.cacheCreation)
        + (usage.cacheCreation1h > 0 && usage.cacheCreation1h == usage.cacheCreation ? " (1h)" : usage.cacheCreation1h > 0 ? " (" + cluster::human_tokens(usage.cacheCreation1h) + " 1h)" : "")
        + " · cache r " + cluster::human_tokens(usage.cacheRead) + " · out " + cluster::human_tokens(usage.output);
    if (usage.thinking > 0) text += " · think " + cluster::human_tokens(usage.thinking);
    return text;
}

_cold std::pair<std::string, std::string> cluster::usage_detail2(const cluster::Usage& usage)
{
    const std::string first = "in " + cluster::human_tokens(usage.input) + " · cache w " + cluster::human_tokens(usage.cacheCreation)
        + (usage.cacheCreation1h > 0 && usage.cacheCreation1h == usage.cacheCreation ? " (1h)" : usage.cacheCreation1h > 0 ? " (" + cluster::human_tokens(usage.cacheCreation1h) + " 1h)" : "");
    std::string second = "cache r " + cluster::human_tokens(usage.cacheRead) + " · out " + cluster::human_tokens(usage.output);
    if (usage.thinking > 0) second += " · think " + cluster::human_tokens(usage.thinking);
    return {first, second};
}

_cold std::string cluster::day_of(const std::int64_t time)
{
    const std::time_t t = static_cast<std::time_t>(time);
    std::tm local{};
    ::localtime_r(&t, &local);
    std::array<char, 16> text{};
    std::strftime(text.data(), text.size(), "%Y-%m-%d", &local);
    return text.data();
}

/* the account */
_cold std::optional<cluster::Json> cluster::account_usage(void)
{
    if (cluster::demo_enabled()) return std::nullopt; // the demo touches no real login, no real account
    const char* test = std::getenv("CLAUDE_CLUSTER_ACCOUNT_URL");
    const std::string url = test != nullptr ? test : "";
    // the token of Claude Code (~/.claude/.credentials.json, the user's own file): given to its endpoint only, by a
    // header file (never on a command line), removed at once; expired: nothing (claude refreshes it when it runs)
    const std::optional<cluster::Json> credentials = cluster::read_json(cluster::expand_home("~/.claude/.credentials.json"));
    if (!credentials || !credentials->is_object() || !credentials->contains("claudeAiOauth") || !(*credentials)["claudeAiOauth"].is_object()) return std::nullopt;
    const cluster::Json& oauth = (*credentials)["claudeAiOauth"];
    const std::string token = oauth.value("accessToken", std::string());
    const std::int64_t expires = oauth.value("expiresAt", std::int64_t{0});
    if (token.empty() || token.size() > 4096 || (expires > 0 && expires / 1000 < cluster::now())) return std::nullopt;
    for (const char c: token)
        if (c < 0x21 || c > 0x7e) return std::nullopt; // a header: printable ASCII only
    const std::string target = url.starts_with("http://127.0.0.1:") ? url : ACCOUNT_URL; // a test endpoint on this computer only, else the real one
    static std::atomic<int> counter{0};
    const std::filesystem::path headers = cluster::data_dir() / ("account-auth-" + std::to_string(::getpid()) + "-" + std::to_string(++counter) + ".txt");
    {
        // made readable by this user only from its first byte (never the umask of the moment)
        const int fd = ::open(headers.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
        if (fd < 0) return std::nullopt;
        const std::string line = "Authorization: Bearer " + token + "\n";
        const bool written = ::write(fd, line.data(), line.size()) == static_cast<ssize_t>(line.size());
        ::close(fd);
        if (!written) {
            std::error_code gone;
            std::filesystem::remove(headers, gone);
            return std::nullopt;
        }
    }
    const cluster::Captured got = cluster::capture({"curl", "-s", "-m", "15", "-H", "@" + headers.string(), "-H", "anthropic-beta: oauth-2025-04-20",
        "-H", "User-Agent: claude-cluster/" CLUSTER_VERSION, "-H", "Accept: application/json", target}, "", 20);
    std::error_code ignored;
    std::filesystem::remove(headers, ignored);
    if (got.code != 0 || got.out.empty()) return std::nullopt;
    const cluster::Json json = cluster::Json::parse(got.out, nullptr, false);
    if (!json.is_object() || !json.contains("five_hour")) return std::nullopt;
    return json;
}

_cold static std::filesystem::path account_cache_(void)
{
    // one copy for the user (every instance, every core): the cache folder, not the data of an instance
    const char* cache = std::getenv("XDG_CACHE_HOME");
    const std::filesystem::path root = cache != nullptr && cache[0] == '/' ? std::filesystem::path(cache) : cluster::expand_home("~/.cache");
    return root / "claude-cluster" / (std::getenv("CLAUDE_CLUSTER_ACCOUNT_URL") != nullptr ? "account-usage-test.json" : "account-usage.json");
}

_cold std::optional<cluster::Json> cluster::account_usage_shared(const int maxAge, std::int64_t* at)
{
    if (cluster::demo_enabled()) return std::nullopt;
    const std::filesystem::path path = account_cache_();
    const std::function<std::optional<cluster::Json>()> read = [&path]() -> std::optional<cluster::Json> {
        const std::optional<cluster::Json> copy = cluster::read_json(path);
        return copy && copy->is_object() ? copy : std::nullopt;
    };
    const std::function<std::optional<cluster::Json>(const std::optional<cluster::Json>&)> answer = [at](const std::optional<cluster::Json>& copy) -> std::optional<cluster::Json> {
        if (!copy || !copy->contains("usage") || !(*copy)["usage"].is_object()) return std::nullopt;
        if (at != nullptr) *at = copy->value("at", std::int64_t{0});
        return (*copy)["usage"];
    };
    // a failed read counts too (tried): the account asked again only once maxAge has passed, never at every tick
    const std::function<bool(const std::optional<cluster::Json>&)> fresh = [maxAge](const std::optional<cluster::Json>& copy) {
        return copy && cluster::now() - std::max(copy->value("at", std::int64_t{0}), copy->value("tried", std::int64_t{0})) < maxAge;
    };
    std::optional<cluster::Json> copy = read();
    if (fresh(copy)) return answer(copy);
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    const std::filesystem::path lockPath = path.string() + ".lock";
    const int lock = ::open(lockPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (lock < 0) return answer(copy);
    if (::flock(lock, LOCK_EX | LOCK_NB) != 0) { // another core reads the account now: its copy at the next tick
        ::close(lock);
        return answer(copy);
    }
    copy = read(); // written meanwhile by the core that held the lock
    if (!fresh(copy)) {
        cluster::Json next = copy ? *copy : cluster::Json::object();
        next["tried"] = cluster::now();
        if (const std::optional<cluster::Json> json = cluster::account_usage()) {
            next["usage"] = *json;
            next["at"] = cluster::now();
        }
        try {
            cluster::write_json(path, next);
            ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
        } catch (const utils::exception::IException&) {} // not kept: this core asks again past maxAge
        copy = next;
    }
    ::flock(lock, LOCK_UN);
    ::close(lock);
    return answer(copy);
}

/* the transcripts of Claude Code */
_cold cluster::Transcripts::Transcripts(const std::vector<std::string>& roots, const std::filesystem::path& cache)
    : _cache(cache)
{
    std::string home = cluster::expand_home("~").lexically_normal().string();
    while (home.size() > 1 && home.back() == '/')
        home.pop_back();
    for (const std::string& root: roots) {
        if (cluster::trim(root).empty()) continue;
        const std::filesystem::path path = cluster::expand_home(cluster::trim(root));
        if (path.string() == home || path.string().starts_with(home + "/")) this->_roots.push_back(path); // the user's own files only
    }
}

_cold void cluster::Transcripts::load_(void)
{
    this->_loaded = true;
    const std::optional<cluster::Json> saved = cluster::read_json(this->_cache);
    if (!saved || !saved->is_object()) return;
    // the objects iterated held by name (items() on a temporary: a reference to nothing)
    const cluster::Json files = saved->value("files", cluster::Json::object());
    for (const auto &[path, item]: files.items()) {
        if (!item.is_object()) continue;
        File& file = this->_files[path];
        file.size = item.value("size", std::uintmax_t{0});
        file.offset = item.value("offset", std::uintmax_t{0});
        const cluster::Json days = item.value("days", cluster::Json::object());
        for (const auto &[day, models]: days.items())
            for (const auto &[model, sums]: models.items())
                file.days[day][model] = sum_of_(sums);
    }
    const cluster::Json recent = saved->value("recent", cluster::Json::object());
    for (const auto &[key, day]: recent.items())
        if (day.is_string()) this->_recent[key] = day.get<std::string>();
}

_cold void cluster::Transcripts::save_(void) const
{
    cluster::Json files = cluster::Json::object();
    for (const auto &[path, file]: this->_files) {
        cluster::Json days = cluster::Json::object();
        for (const auto &[day, models]: file.days)
            for (const auto &[model, sum]: models)
                sum_json_(days[day][model], sum);
        files[path] = {{"size", file.size}, {"offset", file.offset}, {"days", days}};
    }
    cluster::write_json(this->_cache, {{"files", files}, {"recent", this->_recent}, {"scanned", this->_scanned}});
}

_cold void cluster::Transcripts::read_(const std::filesystem::path& path, File& file, std::map<std::string, std::string>& recent)
{
    // the lines after the offset: the answers of the model with their usage, once each (a message streamed is written
    // once per block of its content, with the same usage); a last line without its end: read next time
    std::ifstream in(path, std::ios::binary);
    if (!in) return;
    in.seekg(static_cast<std::streamoff>(file.offset));
    std::string line;
    std::uintmax_t at = file.offset;
    while (std::getline(in, line)) {
        if (in.eof() && !line.empty() && in.peek() == std::char_traits<char>::eof()) { // the last line, maybe cut: next time
            std::ifstream check(path, std::ios::binary | std::ios::ate);
            const std::uintmax_t end = static_cast<std::uintmax_t>(check.tellg());
            if (end > 0 && at + line.size() >= end) break;
        }
        at += line.size() + 1;
        if (line.find("\"usage\"") == std::string::npos || line.find("\"assistant\"") == std::string::npos) continue;
        const cluster::Json json = cluster::Json::parse(line, nullptr, false);
        if (!json.is_object() || json.value("type", std::string()) != "assistant" || !json.contains("message") || !json["message"].is_object()) continue;
        const cluster::Json& message = json["message"];
        if (!message.contains("usage") || !message["usage"].is_object()) continue;
        const std::string model = message.value("model", std::string());
        if (cluster::vendor_of_model(model).empty()) continue;
        const std::string key = message.value("id", std::string()) + "|" + json.value("requestId", std::string());
        const std::int64_t time = iso_time_(json.value("timestamp", std::string()));
        const std::string day = cluster::day_of(time > 0 ? time : cluster::now());
        if (key != "|" && recent.contains(key)) continue;
        recent[key] = day;
        const cluster::Usage usage = usage_of_(message["usage"]);
        cluster::UsageSum& sum = file.days[day][model];
        sum.input += usage.input;
        sum.cacheWrite += usage.cacheCreation;
        sum.cacheWrite1h += usage.cacheCreation1h;
        sum.cacheRead += usage.cacheRead;
        sum.output += usage.output;
        sum.thinking += usage.thinking;
        sum.tokens += usage.input + usage.cacheCreation + usage.output;
        sum.cache += usage.cacheRead;
        ++sum.turns;
    }
    file.offset = at;
}

_cold void cluster::Transcripts::scan(const std::atomic<bool>& stop)
{
    // a snapshot of what is known, the files read without the lock (seconds the first time: the panels never wait),
    // merged under it at the end
    std::map<std::string, File> files;
    std::map<std::string, std::string> recent;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (!this->_loaded) this->load_();
        files = this->_files;
        recent = this->_recent;
    }
    bool changed = false;
    std::set<std::string> seen;
    for (const std::filesystem::path& root: this->_roots) {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) continue;
        for (const std::filesystem::directory_entry& entry: std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, error)) {
            if (error || stop) break;
            if (!entry.is_regular_file(error) || entry.path().extension() != ".jsonl") continue;
            const std::string path = entry.path().string();
            seen.insert(path);
            const std::uintmax_t size = entry.file_size(error);
            File& file = files[path];
            if (size < file.offset) file = File(); // written again from its start: read again
            if (size == file.size && size == file.offset) continue;
            file.size = size;
            cluster::Transcripts::read_(entry.path(), file, recent);
            changed = true;
        }
    }
    if (stop) return;
    for (auto it = files.begin(); it != files.end();) { // a transcript gone: its numbers too
        if (seen.contains(it->first)) ++it;
        else {
            it = files.erase(it);
            changed = true;
        }
    }
    const std::string old = cluster::day_of(cluster::now() - 2 * 24 * 3600); // the keys of the last 2 days are enough (a message is written within seconds)
    for (auto it = recent.begin(); it != recent.end();) {
        if (it->second < old) {
            it = recent.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_files = std::move(files);
    this->_recent = std::move(recent);
    this->_scanned = cluster::now();
    if (changed) this->save_();
}

_cold std::map<std::string, std::map<std::string, cluster::UsageSum>> cluster::Transcripts::days(void) const
{
    std::map<std::string, std::map<std::string, cluster::UsageSum>> all;
    std::lock_guard<std::mutex> lock(this->_mutex);
    for (const auto &[path, file]: this->_files)
        for (const auto &[day, models]: file.days)
            for (const auto &[model, sum]: models) {
                cluster::UsageSum& to = all[day][model];
                to.input += sum.input;
                to.cacheWrite += sum.cacheWrite;
                to.cacheWrite1h += sum.cacheWrite1h;
                to.cacheRead += sum.cacheRead;
                to.output += sum.output;
                to.thinking += sum.thinking;
                to.tokens += sum.tokens;
                to.cache += sum.cache;
                to.turns += sum.turns;
            }
    return all;
}

_cold bool cluster::Transcripts::any(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return std::any_of(this->_files.begin(), this->_files.end(), [](const std::pair<const std::string, File>& item) {return !item.second.days.empty();});
}
