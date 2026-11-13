/**************************************************************\
Edition:
##  @date 17/10/2026 by @author Tsukini

File Name:
##  @file Usage.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef USAGE_H
    #define USAGE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Config.hpp"           // cluster::Config
    #include "Types.hpp"            // cluster::Usage, cluster::UsageSum, cluster::Json
    #include <filesystem>           // std::filesystem::path
    #include <optional>             // std::optional
    #include <atomic>               // std::atomic
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <mutex>                // std::mutex
    #include <map>                  // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// The price of a model, $ per million tokens: the input, the output, the cache written (5 min / 1 h), the cache read
struct Price {
    double input = 0.0;
    double output = 0.0;
    double cacheWrite5m = 0.0;
    double cacheWrite1h = 0.0;
    double cacheRead = 0.0;
    std::string source;             // config | learned | table
};

//----------------------------------------------------------------//
/* FUNCTION */

// The price of a model: the config ([usage.prices]), else the one learned from the cost Claude Code tells for its turns
// (prices.json), else the public table of the known models; none: unknown (its cost is not shown)
_cold _nodiscard std::optional<cluster::Price> price_of(const cluster::Config& config, const std::string& model);
_cold _nodiscard std::optional<double> cost_of(const cluster::Config& config, const std::string& model, const cluster::Usage& usage);
// a turn with its cost told by Claude Code (total_cost_usd): the input price of its model kept (the output 5x, the cache
// 1.25x / 2x / 0.1x as Anthropic prices them)
_cold void price_learn(const std::string& model, const cluster::Usage& usage, const double cost);
_cold _nodiscard std::string vendor_of_model(const std::string& model);   // anthropic (claude-*), other (another model run by Claude Code), empty: nothing (<synthetic>)
_cold _nodiscard std::string usage_detail(const cluster::Usage& usage);    // "in 2 · cache w 15.1k (1h) · cache r 29.9k · out 529 · think 245"
_cold _nodiscard std::pair<std::string, std::string> usage_detail2(const cluster::Usage& usage);   // the same on two lines (a narrow panel)
_cold _nodiscard std::string day_of(const std::int64_t time);              // local YYYY-MM-DD
// the windows of the Claude account logged in here (the token of Claude Code, never shown): the OAuth usage endpoint
// (a test: CLAUDE_CLUSTER_ACCOUNT_URL in the environment of the core, a local http://127.0.0.1 one only - never the config)
_cold _nodiscard std::optional<cluster::Json> account_usage(void);
// the same shared by every core of this user (~/.cache/claude-cluster/account-usage.json): read from that copy while younger than
// maxAge seconds, else fetched by one core only (a lock; the others read the copy); *at: when the answer was read from the account
_cold _nodiscard std::optional<cluster::Json> account_usage_shared(const int maxAge, std::int64_t* at);

//----------------------------------------------------------------//
/* CLASS */

// The transcripts of Claude Code on this computer (~/.claude/projects/*/*.jsonl: every session, in claude-cluster or
// not): the usage of each answer (the API's own numbers), read as they grow, summed by day and model
class Transcripts {
    private:
        struct File {
            std::uintmax_t size = 0;
            std::uintmax_t offset = 0;      // read up to here
            std::map<std::string, std::map<std::string, cluster::UsageSum>> days;   // day -> model -> its sums
        };
        std::vector<std::filesystem::path> _roots;
        std::filesystem::path _cache;
        std::map<std::string, File> _files;             // by path
        std::map<std::string, std::string> _recent;     // message id | request id -> day (a message written several times: once)
        mutable std::mutex _mutex;
        bool _loaded = false;
        std::int64_t _scanned = 0;

        // ---------- Pre-Function -------- //
        _cold void load_(void);
        _cold void save_(void) const;
        _cold static void read_(const std::filesystem::path& path, File& file, std::map<std::string, std::string>& recent);

    public:
        // ---------- Constructor -------- //
        _cold Transcripts(const std::vector<std::string>& roots, const std::filesystem::path& cache);

        // ---------- Pre-Function -------- //
        _cold void scan(const std::atomic<bool>& stop); // what grew since the last time (seconds the first time, then nothing); stop: ended early
        _cold _nodiscard std::map<std::string, std::map<std::string, cluster::UsageSum>> days(void) const;  // day -> model -> sums
        _cold _nodiscard bool any(void) const;

        // ---------- Function -------- //
        _nodiscard inline std::int64_t scannedAt(void) const {return this->_scanned;};
};

} // namespace end
#endif /* USAGE_H */
