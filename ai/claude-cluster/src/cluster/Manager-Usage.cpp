/**************************************************************\
Edition:
##  @date 11/10/2026 by @author Tsukini

File Name:
##  @file Manager-Usage.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Manager.hpp"
#include "cluster/Usage.hpp"
#include "cluster/Tools.hpp"
#define _Exception
#define _Attribute
#include "cluster/Auth.hpp"
#include <functional>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <ctime>
#include <set>
#include <map>

/* tools */
_cold static std::filesystem::path usage_file_(void)
{
    return cluster::data_dir() / "usage.jsonl";
}

_cold static std::filesystem::path limits_file_(void)
{
    return cluster::data_dir() / "limits.json";
}

_cold static std::int64_t midnight_(const std::int64_t now)
{
    // the start of the local day
    const std::time_t t = static_cast<std::time_t>(now);
    std::tm tm{};
    ::localtime_r(&t, &tm);
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    return static_cast<std::int64_t>(std::mktime(&tm));
}

_cold static std::string date_(const std::int64_t time, const char* format)
{
    const std::time_t t = static_cast<std::time_t>(time);
    std::tm tm{};
    ::localtime_r(&t, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, format);
    return out.str();
}

_cold static void add_sum_(cluster::UsageSum& to, const cluster::UsageSum& from, const std::optional<double> cost)
{
    to.tokens += from.tokens;
    to.cache += from.cache;
    to.turns += from.turns;
    to.input += from.input;
    to.cacheWrite += from.cacheWrite;
    to.cacheWrite1h += from.cacheWrite1h;
    to.cacheRead += from.cacheRead;
    to.output += from.output;
    to.thinking += from.thinking;
    if (cost) to.cost += *cost;
    else to.costKnown = false;
}

_cold static void add_(cluster::UsageSum& sum, const cluster::UsageRecord& record)
{
    sum.tokens += record.tokens;
    sum.cache += record.cache;
    sum.cost += record.cost;
    if (!record.costKnown) sum.costKnown = false;
    ++sum.turns;
    sum.input += record.input;
    sum.cacheWrite += record.cacheWrite;
    sum.cacheWrite1h += record.cacheWrite1h;
    sum.cacheRead += record.cacheRead;
    sum.output += record.output;
    sum.thinking += record.thinking;
}

_cold static std::string sum_text_(const cluster::UsageSum& sum)
{
    // "1.2M tok · $3.21"; a cost partly unknown (a model without a price): "+?"; none known: "cost ?"
    if (sum.tokens + sum.cache == 0) return "-";
    const std::string tokens = cluster::human_tokens(sum.tokens) + " tok";
    if (sum.costKnown) return tokens + " · " + cluster::human_cost(sum.cost);
    return tokens + (sum.cost > 0.0 ? " · " + cluster::human_cost(sum.cost) + " +?" : " · cost ?");
}

_cold static cluster::Json sum_json_(const cluster::UsageSum& sum)
{
    if (sum.input + sum.cacheWrite + sum.cacheRead + sum.output > 0)
        return {{"tokens", sum.tokens}, {"cache_read", sum.cache}, {"cost_usd", sum.costKnown || sum.cost > 0.0 ? cluster::Json(sum.cost) : cluster::Json("unknown")},
            {"cost_partial", !sum.costKnown}, {"turns", sum.turns}, {"input", sum.input}, {"cache_write", sum.cacheWrite}, {"cache_write_1h", sum.cacheWrite1h},
            {"output", sum.output}, {"thinking", sum.thinking}};
    return {{"tokens", sum.tokens}, {"cache_read", sum.cache}, {"cost_usd", sum.cost}, {"cost_complete", sum.costKnown}, {"turns", sum.turns}};
}

_cold static std::string left_text_(const double used, const std::int64_t reset, const char* format)
{
    // what is left of a window and when it is reset
    const int left = std::clamp(static_cast<int>(100.0 - used * 100.0 + 0.5), 0, 100);
    return std::to_string(left) + "%" + (reset > 0 ? " · " + date_(reset, format) : std::string());
}

/* ledger */
_cold void cluster::Manager::loadUsage_(void)
{
    std::ifstream file(usage_file_());
    std::string line;
    std::vector<cluster::UsageRecord> records;

    while (std::getline(file, line)) {
        const cluster::Json json = cluster::Json::parse(line, nullptr, false);
        if (!json.is_object()) continue;
        records.push_back({json.value("time", std::int64_t{0}), json.value("vendor", std::string()), json.value("model", std::string()), json.value("session", std::string()),
            json.value("tokens", std::int64_t{0}), json.value("cache", std::int64_t{0}), json.value("cost", 0.0), json.value("cost_known", true),
            json.value("input", std::int64_t{0}), json.value("cache_write", std::int64_t{0}), json.value("cache_write_1h", std::int64_t{0}), json.value("cache_read", std::int64_t{0}),
            json.value("output", std::int64_t{0}), json.value("thinking", std::int64_t{0})});
    }
    std::stable_sort(records.begin(), records.end(), [](const cluster::UsageRecord& a, const cluster::UsageRecord& b) {return a.time < b.time;});
    std::map<std::string, cluster::UsageReport> limits;
    if (const std::optional<cluster::Json> saved = cluster::read_json(limits_file_()); saved && saved->is_object()) {
        for (const auto &[vendor, item]: saved->items()) {
            cluster::UsageReport& report = limits[vendor];
            report.limit5h = item.value("limit_5h", -1.0);
            report.limit7d = item.value("limit_7d", -1.0);
            report.reset5h = item.value("reset_5h", std::int64_t{0});
            report.reset7d = item.value("reset_7d", std::int64_t{0});
        }
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_usage = std::move(records);
    this->_limits = std::move(limits);
}

_cold void cluster::Manager::recordUsage_(const std::string& session)
{
    const std::optional<cluster::Snapshot> snap = this->snapshot(session);
    if (!snap) return;
    const cluster::Metrics& m = snap->metrics;
    const std::string vendor = this->vendorOf(snap->spec.backend);
    cluster::UsageRecord record{cluster::now(), vendor, snap->modelUsed.empty() ? snap->spec.model : snap->modelUsed, session,
        m.last.input + m.last.cacheCreation + m.last.output, m.last.cacheRead, m.costKnown ? m.lastCost : 0.0, m.costKnown,
        m.last.input, m.last.cacheCreation, m.last.cacheCreation1h, m.last.cacheRead, m.last.output, m.last.thinking};
    // the price of the model: learned from the cost Claude Code tells (exact for its own table)
    if (vendor == "anthropic" && m.costKnown && m.lastCost > 0.0) cluster::price_learn(record.model, m.last, m.lastCost);

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (m.limit5h >= 0.0 || m.limit7d >= 0.0) {
        cluster::UsageReport& limits = this->_limits[vendor];
        if (m.limit5h >= 0.0) limits.limit5h = m.limit5h;
        if (m.limit7d >= 0.0) limits.limit7d = m.limit7d;
        if (m.reset5h > 0) limits.reset5h = m.reset5h;
        if (m.reset7d > 0) limits.reset7d = m.reset7d;
        cluster::Json saved = cluster::Json::object(); // kept over the runs
        for (const auto &[name, report]: this->_limits)
            saved[name] = {{"limit_5h", report.limit5h}, {"limit_7d", report.limit7d}, {"reset_5h", report.reset5h}, {"reset_7d", report.reset7d}};
        cluster::write_json(limits_file_(), saved);
        for (const auto &[window, used, reset]: {std::tuple<std::string, double, std::int64_t>{"5 h", m.limit5h, m.reset5h}, {"7 d", m.limit7d, m.reset7d}}) {
            const int level = used >= 1.0 ? 100 : used >= 0.95 ? 95 : used >= 0.8 ? 80 : 0;
            int& told = this->_limitLevel[vendor + window];
            if (level <= told) {
                if (level < told) told = level; // reset: told again next time
                continue;
            }
            told = level;
            const std::string when = reset > 0 ? ", reset in " + cluster::human_age(cluster::now() + (cluster::now() - reset)) : std::string();
            std::thread([this, vendor, window, level, when]() {
                (void)this->notify({"limit-" + vendor, vendor + " · " + window + " limit " + (level >= 100 ? "reached" : "close"),
                    std::to_string(level) + " % of the " + window + " window used" + when, level >= 100 ? cluster::Face::Idle : cluster::Face::Waiting, level >= 95, "dialog-warning"}, "limit");
            }).detach();
        }
    }
    if (record.tokens + record.cache == 0) return; // nothing used (an error before the agent answered)
    this->_usage.push_back(record);
    std::ofstream(usage_file_(), std::ios::app) << cluster::Json{{"time", record.time}, {"vendor", record.vendor}, {"model", record.model},
        {"session", record.session}, {"tokens", record.tokens}, {"cache", record.cache}, {"cost", record.cost}, {"cost_known", record.costKnown},
        {"input", record.input}, {"cache_write", record.cacheWrite}, {"cache_write_1h", record.cacheWrite1h}, {"cache_read", record.cacheRead},
        {"output", record.output}, {"thinking", record.thinking}}.dump() << "\n";
}

/* reports */
_cold std::string cluster::Manager::vendorOf(const std::string& backend) const
{
    try {
        return this->_auth.resolve(backend).provider.name;
    } catch (const utils::exception::IException&) {
        return "";
    }
}

_cold static cluster::UsageSum sum_from_(const cluster::Json& json)
{
    cluster::UsageSum sum;
    if (!json.is_object()) return sum;
    sum.tokens = json.value("tokens", std::int64_t{0});
    sum.cache = json.value("cache_read", std::int64_t{0});
    sum.turns = json.value("turns", 0);
    sum.costKnown = json.contains("cost_usd") && json["cost_usd"].is_number();
    sum.cost = sum.costKnown ? json["cost_usd"].get<double>() : 0.0;
    sum.input = json.value("input", std::int64_t{0});
    sum.cacheWrite = json.value("cache_write", std::int64_t{0});
    sum.cacheWrite1h = json.value("cache_write_1h", std::int64_t{0});
    sum.cacheRead = json.value("cache_read", std::int64_t{0});
    sum.output = json.value("output", std::int64_t{0});
    sum.thinking = json.value("thinking", std::int64_t{0});
    return sum;
}

_cold cluster::UsageReport cluster::Manager::usage(const std::string& vendor) const
{
    const std::int64_t now = cluster::now();
    if (this->_replica) { // an interface of the core: its report (asked every 2 s at most: the panels draw often)
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            auto cached = this->_usageCache.find(vendor);
            if (cached != this->_usageCache.end() && now - cached->second.first < 2) return cached->second.second;
        }
        cluster::UsageReport report;
        report.vendor = vendor;
        try {
            const cluster::Json data = this->call({{"cmd", "usage"}, {"vendor", vendor}});
            if (data.is_array() && !data.empty() && data[0].is_object()) {
                const cluster::Json& item = data[0];
                report.today = sum_from_(item.value("today", cluster::Json::object()));
                report.week = sum_from_(item.value("last_7_days", cluster::Json::object()));
                report.total = sum_from_(item.value("lifetime", cluster::Json::object()));
                report.source = item.value("source", std::string());
                report.accountAt = item.value("account_read_at", std::int64_t{0});
                std::tm tm{};
                if (std::sscanf(item.value("since", std::string()).c_str(), "%4d-%2d-%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
                    tm.tm_year -= 1900;
                    --tm.tm_mon;
                    tm.tm_isdst = -1;
                    report.since = static_cast<std::int64_t>(std::mktime(&tm));
                }
                if (item.contains("five_hour")) {
                    report.limit5h = item["five_hour"].value("used_percent", 0) / 100.0;
                    report.reset5h = item["five_hour"].value("resets_at", std::int64_t{0});
                }
                if (item.contains("seven_day")) {
                    report.limit7d = item["seven_day"].value("used_percent", 0) / 100.0;
                    report.reset7d = item["seven_day"].value("resets_at", std::int64_t{0});
                }
                for (const cluster::Json& w: item.value("windows", cluster::Json::array())) {
                    cluster::UsageWindow window;
                    window.name = w.value("name", std::string());
                    window.reset = w.value("resets_at", std::int64_t{0});
                    if (w.contains("used_percent")) window.used = w.value("used_percent", 0) / 100.0;
                    if (w.contains("limit_dollars")) {
                        window.usedDollars = w.value("used_dollars", 0.0);
                        window.limitDollars = w.value("limit_dollars", 0.0);
                    }
                    window.locked = w.value("locked", std::string());
                    report.windows.push_back(window);
                }
            }
        } catch (const utils::exception::IException&) {}
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_usageCache[vendor] = {now, report};
        return report;
    }
    const std::int64_t today = midnight_(now);
    const std::int64_t week = now - 7 * 24 * 3600;
    cluster::UsageReport report;

    // the transcripts of Claude Code (every session of this computer): the API's own numbers by day and model
    const bool transcripts = (vendor == "anthropic" || vendor == "other") && this->_transcripts.any();
    if (transcripts) {
        const std::string todayDay = cluster::day_of(now);
        const std::string weekDay = cluster::day_of(now - 6 * 24 * 3600);
        for (const auto &[day, models]: this->_transcripts.days())
            for (const auto &[model, sum]: models) {
                if (cluster::vendor_of_model(model) != vendor) continue;
                cluster::Usage usage;
                usage.input = sum.input;
                usage.cacheCreation = sum.cacheWrite;
                usage.cacheCreation1h = sum.cacheWrite1h;
                usage.cacheRead = sum.cacheRead;
                usage.output = sum.output;
                const std::optional<double> cost = cluster::cost_of(this->_config, model, usage);
                add_sum_(report.total, sum, cost);
                if (day >= weekDay) add_sum_(report.week, sum, cost);
                if (day == todayDay) add_sum_(report.today, sum, cost);
                if (report.since == 0 || cluster::day_of(report.since) > day) { // the first day
                    std::tm tm{};
                    if (std::sscanf(day.c_str(), "%4d-%2d-%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
                        tm.tm_year -= 1900;
                        --tm.tm_mon;
                        tm.tm_isdst = -1;
                        report.since = static_cast<std::int64_t>(std::mktime(&tm));
                    }
                }
            }
        report.source = "the transcripts of Claude Code (every session of this computer, by the API's numbers)";
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto limits = this->_limits.find(vendor);
    if (limits != this->_limits.end()) {
        report.limit5h = limits->second.limit5h;
        report.limit7d = limits->second.limit7d;
        report.reset5h = limits->second.reset5h;
        report.reset7d = limits->second.reset7d;
        report.windows = limits->second.windows;
        report.accountAt = limits->second.accountAt;
    }
    report.vendor = vendor;
    if (!transcripts) {
        report.source = "the ledger of claude-cluster (its own sessions)";
        for (const cluster::UsageRecord& record: this->_usage) {
            if (record.vendor != vendor) continue;
            if (report.since == 0) report.since = record.time;
            add_(report.total, record);
            if (record.time >= week) add_(report.week, record);
            if (record.time >= today) add_(report.today, record);
        }
    }
    // a window over: its reset passed, nothing known of the new one yet
    if (report.reset5h > 0 && report.reset5h <= now) report.limit5h = 0.0;
    if (report.reset7d > 0 && report.reset7d <= now) report.limit7d = 0.0;
    return report;
}

_cold std::vector<std::string> cluster::Manager::usageVendors(void) const
{
    std::set<std::string> vendors;
    std::lock_guard<std::mutex> lock(this->_mutex);
    for (const cluster::UsageRecord& record: this->_usage)
        vendors.insert(record.vendor);
    for (const auto &[vendor, limits]: this->_limits)
        vendors.insert(vendor);
    return {vendors.begin(), vendors.end()};
}

_cold std::vector<std::pair<std::string, std::string>> cluster::Manager::usageLines(const cluster::Snapshot& snap) const
{
    // the vendor of the session over every session and run: today, 7 days, lifetime; its limits left; the budget left
    const cluster::UsageReport report = this->usage(this->vendorOf(snap.spec.backend));
    std::vector<std::pair<std::string, std::string>> lines = {{"vendor", report.vendor.empty() ? "?" : report.vendor}, {"today", sum_text_(report.today)}};
    const std::function<void(const cluster::UsageSum&)> detail = [&lines](const cluster::UsageSum& s) {
        if (s.input + s.cacheWrite + s.cacheRead + s.output == 0) return;
        cluster::Usage u;
        u.input = s.input;
        u.cacheCreation = s.cacheWrite;
        u.cacheCreation1h = s.cacheWrite1h;
        u.cacheRead = s.cacheRead;
        u.output = s.output;
        u.thinking = s.thinking;
        const std::pair<std::string, std::string> two = cluster::usage_detail2(u);
        lines.emplace_back(" ", two.first);
        lines.emplace_back(" ", two.second);
    };
    detail(report.today);
    lines.emplace_back("7 days", sum_text_(report.week));
    lines.emplace_back("lifetime", sum_text_(report.total));
    detail(report.total);
    if (report.since > 0) lines.emplace_back("since", date_(report.since, "%d/%m/%Y"));
    if (report.limit5h >= 0.0) lines.emplace_back("5h left", left_text_(report.limit5h, report.reset5h, "%H:%M"));
    if (report.limit7d >= 0.0) lines.emplace_back("7d left", left_text_(report.limit7d, report.reset7d, "%a %H:%M"));
    for (const cluster::UsageWindow& w: report.windows) { // the other windows of the account (a model's 7 d, the extra usage)
        if (w.name == "5h" || w.name == "7d") continue;
        if (w.limitDollars >= 0.0) lines.emplace_back(w.name, cluster::human_cost(std::max(0.0, w.usedDollars)) + " / " + cluster::human_cost(w.limitDollars));
        else if (w.used >= 0.0) lines.emplace_back(w.name + " left", left_text_(w.used, w.reset, "%a %H:%M"));
    }
    if (report.accountAt > 0) lines.emplace_back("account", "read " + cluster::human_age(report.accountAt));
    if (this->_config.budget > 0.0 && snap.metrics.costKnown)
        lines.emplace_back("budget left", cluster::human_cost(std::max(0.0, this->_config.budget - snap.metrics.cost)));
    return lines;
}

_cold cluster::Json cluster::Manager::usageJson(const std::string& vendor) const
{
    cluster::Json data = cluster::Json::array();
    for (const std::string& name: vendor.empty() ? this->usageVendors() : std::vector<std::string>{vendor}) {
        const cluster::UsageReport report = this->usage(name);
        cluster::Json item = {{"vendor", name}, {"today", sum_json_(report.today)}, {"last_7_days", sum_json_(report.week)}, {"lifetime", sum_json_(report.total)},
            {"since", report.since > 0 ? date_(report.since, "%Y-%m-%d") : std::string()}, {"source", report.source}};
        if (report.accountAt > 0) item["account_read_at"] = report.accountAt;
        cluster::Json windows = cluster::Json::array();
        for (const cluster::UsageWindow& w: report.windows) {
            cluster::Json window = {{"name", w.name}, {"resets_at", w.reset}};
            if (w.used >= 0.0) window["used_percent"] = static_cast<int>(w.used * 100.0 + 0.5);
            if (w.limitDollars >= 0.0) window.update({{"used_dollars", w.usedDollars}, {"limit_dollars", w.limitDollars}});
            if (!w.locked.empty()) window["locked"] = w.locked;
            windows.push_back(window);
        }
        if (!windows.empty()) item["windows"] = windows;
        if (report.limit5h >= 0.0) item["five_hour"] = {{"used_percent", static_cast<int>(report.limit5h * 100.0 + 0.5)}, {"resets_at", report.reset5h}};
        if (report.limit7d >= 0.0) item["seven_day"] = {{"used_percent", static_cast<int>(report.limit7d * 100.0 + 0.5)}, {"resets_at", report.reset7d}};
        data.push_back(item);
    }
    return data;
}

/* the sources read in the background */
_cold void cluster::Manager::scanTranscripts_(void)
{
    if (this->_replica || this->_usageStop || this->_usageBusy.exchange(true)) return;
    if (this->_scanThread.joinable()) this->_scanThread.join(); // the one before (done: _usageBusy was false)
    this->_scanThread = std::thread([this]() {
        try {
            const std::function<std::int64_t()> count = [this]() {
                std::int64_t total = 0;
                for (const auto &[day, models]: this->_transcripts.days())
                    for (const auto &[model, sum]: models)
                        total += sum.tokens + sum.cache + sum.turns;
                return total;
            };
            const std::int64_t before = count();
            this->_transcripts.scan(this->_usageStop);
            if (count() != before) ++this->_version;
        } catch (const std::exception&) {}
        this->_usageBusy = false;
    });
}

_cold void cluster::Manager::refreshAccount_(void)
{
    // the windows of the account: its utilization in percent, the resets in ISO dates; the extra usage; kept per vendor
    if (this->_replica || !this->_config.usage.account) return;
    {
        std::lock_guard<std::mutex> lock(this->_mutex); // read less than a minute ago: kept
        if (cluster::now() - this->_limits["anthropic"].accountAt < 60) return;
    }
    if (this->_usageStop || this->_accountBusy.exchange(true)) return;
    if (this->_accountThread.joinable()) this->_accountThread.join(); // the one before (done)
    this->_accountThread = std::thread([this]() {
        const std::optional<cluster::Json> json = cluster::account_usage();
        if (!json || this->_usageStop) {
            this->_accountBusy = false;
            return;
        }
        const std::function<std::int64_t(const cluster::Json&)> reset = [](const cluster::Json& window) -> std::int64_t {
            if (!window.contains("resets_at")) return 0;
            if (window["resets_at"].is_number()) return window["resets_at"].get<std::int64_t>();
            if (!window["resets_at"].is_string()) return 0;
            int year = 0;
            int month = 0;
            int day = 0;
            int hour = 0;
            int minute = 0;
            int second = 0;
            if (std::sscanf(window["resets_at"].get<std::string>().c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6) return 0;
            std::tm tm{};
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = second;
            return static_cast<std::int64_t>(::timegm(&tm));
        };
        std::vector<cluster::UsageWindow> windows;
        for (const auto &[key, name]: std::vector<std::pair<std::string, std::string>>{{"five_hour", "5h"}, {"seven_day", "7d"}, {"seven_day_opus", "7d opus"},
            {"seven_day_sonnet", "7d sonnet"}}) {
            if (!json->contains(key) || !(*json)[key].is_object()) continue;
            const cluster::Json& w = (*json)[key];
            cluster::UsageWindow window;
            window.name = name;
            if (w.contains("utilization") && w["utilization"].is_number()) window.used = std::clamp(w["utilization"].get<double>() / 100.0, 0.0, 1.0);
            window.reset = reset(w);
            if (w.contains("locked_reason") && w["locked_reason"].is_string()) window.locked = w["locked_reason"].get<std::string>();
            windows.push_back(window);
        }
        if (json->contains("extra_usage") && (*json)["extra_usage"].is_object() && (*json)["extra_usage"].value("is_enabled", false)) {
            const cluster::Json& e = (*json)["extra_usage"];
            cluster::UsageWindow window;
            window.name = "extra usage";
            if (e.contains("used_credits") && e["used_credits"].is_number()) window.usedDollars = e["used_credits"].get<double>();
            if (e.contains("monthly_limit") && e["monthly_limit"].is_number()) window.limitDollars = e["monthly_limit"].get<double>();
            if (e.contains("utilization") && e["utilization"].is_number()) window.used = std::clamp(e["utilization"].get<double>() / 100.0, 0.0, 1.0);
            windows.push_back(window);
        }
        std::lock_guard<std::mutex> lock(this->_mutex);
        cluster::UsageReport& limits = this->_limits["anthropic"];
        limits.windows = windows;
        limits.accountAt = cluster::now();
        for (const cluster::UsageWindow& w: windows) {
            if (w.name == "5h" && w.used >= 0.0) {
                limits.limit5h = w.used;
                limits.reset5h = w.reset;
            } else if (w.name == "7d" && w.used >= 0.0) {
                limits.limit7d = w.used;
                limits.reset7d = w.reset;
            }
        }
        ++this->_version;
        this->_accountBusy = false;
    });
}

_cold void cluster::Manager::usageRefresh(void)
{
    this->scanTranscripts_();
    this->refreshAccount_();
}

/* limits and accounts */
_cold static std::int64_t window_start_(const std::string& window)
{
    // day: the local midnight; month: the 1st of the local month, midnight; week: the last 7 days
    const std::int64_t now = cluster::now();
    if (window == "week") return now - 7 * 86400;
    std::time_t stamp = static_cast<std::time_t>(now);
    std::tm local{};
    ::localtime_r(&stamp, &local);
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    if (window == "month") local.tm_mday = 1;
    return static_cast<std::int64_t>(std::mktime(&local));
}

_cold double cluster::Manager::spent(const std::int64_t since) const
{
    // the usage ledger since then; keys: only the providers billed through a key (never a subscription or a local model)
    const bool keysOnly = this->_config.limits.scope != "all";
    std::map<std::string, bool> counted;
    std::lock_guard<std::mutex> lock(this->_mutex);
    double total = 0.0;
    for (auto it = this->_usage.rbegin(); it != this->_usage.rend() && it->time >= since; ++it) {
        if (!it->costKnown || it->cost <= 0.0) continue;
        if (keysOnly) {
            auto known = counted.find(it->vendor);
            if (known == counted.end()) {
                const std::optional<cluster::Provider> provider = this->_auth.find(it->vendor);
                known = counted.emplace(it->vendor, provider && provider->auth == cluster::AuthKind::Key && !provider->local).first;
            }
            if (!known->second) continue;
        }
        total += it->cost;
    }
    return total;
}

_cold std::string cluster::Manager::limitReached(void) const
{
    const cluster::LimitsConf& limits = this->_config.limits;
    for (const auto &[window, limit]: {std::pair<std::string, double>{"day", limits.dailyUsd}, {"week", limits.weeklyUsd}, {"month", limits.monthlyUsd}})
        if (limit > 0.0 && this->spent(window_start_(window)) >= limit)
            return "the limit per " + window + " of " + cluster::human_cost(limit) + " is reached (" + cluster::human_cost(this->spent(window_start_(window))) + " spent)";
    return "";
}

_cold void cluster::Manager::limitsWarn(void)
{
    // each limit: told at the warn share and when reached, once per window (told again after it opened again)
    const cluster::LimitsConf& limits = this->_config.limits;
    for (const auto &[window, limit]: {std::pair<std::string, double>{"day", limits.dailyUsd}, {"week", limits.weeklyUsd}, {"month", limits.monthlyUsd}}) {
        const double used = limit > 0.0 ? this->spent(window_start_(window)) / limit : 0.0;
        const int level = limit <= 0.0 ? 0 : used >= 1.0 ? 100 : limits.warnPercent > 0 && used * 100.0 >= limits.warnPercent ? limits.warnPercent : 0;
        int told = 0;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            told = this->_limitLevel["budget-" + window];
            if (level != told) this->_limitLevel["budget-" + window] = std::max(level, 0);
        }
        if (level <= told) continue;
        const std::string text = "the limit per " + window + " (" + cluster::human_cost(limit) + "): " + (level >= 100 ? "reached" : std::to_string(static_cast<int>(used * 100.0)) + " % used");
        this->notice_("", text + (level >= 100 && limits.block ? ": the prompts are refused (send --force passes)" : ""), level >= 100);
        std::thread([this, window, level, text]() {
            (void)this->notify({"limit-budget-" + window, "claude-cluster · spending limit", text, level >= 100 ? cluster::Face::Idle : cluster::Face::Waiting, level >= 100,
                "dialog-warning"}, "limit");
        }).detach();
    }
}

_cold cluster::Json cluster::Manager::accountsJson(void)
{
    if (this->_replica) return this->replicaState("accounts");
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (cluster::now_ms() - this->_accountsAt < 8000 && !this->_accounts.is_null()) return this->_accounts;
    }
    cluster::Json providers = cluster::Json::array();
    for (const cluster::Provider& p: this->_auth.providers()) {
        const auto [ready, detail] = this->_auth.status(p); // a keyring read can start a process: cached
        providers.push_back({{"name", p.name}, {"driver", p.driver}, {"auth", p.auth == cluster::AuthKind::Key ? "key" : p.auth == cluster::AuthKind::Login ? "login" : "none"},
            {"ready", ready}, {"detail", detail}, {"local", p.local}});
    }
    cluster::Json json = {{"providers", providers}, {"spent", {{"day", this->spent(window_start_("day"))}, {"week", this->spent(window_start_("week"))},
        {"month", this->spent(window_start_("month"))}}}};
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_accounts = json;
    this->_accountsAt = cluster::now_ms();
    return json;
}

_cold std::string cluster::Manager::authKind(const std::string& backend) const
{
    // how a session of this backend is billed: through an API key, through the login of its agent (a subscription), or nothing (a local model)
    const std::optional<cluster::Provider> provider = this->_auth.find(this->vendorOf(backend));
    if (!provider || provider->local || provider->auth == cluster::AuthKind::None) return "local";
    return provider->auth == cluster::AuthKind::Key ? "api" : "auth";
}
