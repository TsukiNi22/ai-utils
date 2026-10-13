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
#include "cluster/Tools.hpp"
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
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

_cold static void add_(cluster::UsageSum& sum, const cluster::UsageRecord& record)
{
    sum.tokens += record.tokens;
    sum.cache += record.cache;
    sum.cost += record.cost;
    sum.costKnown = sum.costKnown && record.costKnown;
    ++sum.turns;
}

_cold static std::string sum_text_(const cluster::UsageSum& sum)
{
    // 1.2M tok · $3.40 (local: the tokens only)
    if (sum.turns == 0) return "-";
    return cluster::human_tokens(sum.tokens) + " tok" + (sum.cost > 0.0 ? " · " + cluster::human_cost(sum.cost) + (sum.costKnown ? "" : "+") : std::string());
}

_cold static cluster::Json sum_json_(const cluster::UsageSum& sum)
{
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
            json.value("tokens", std::int64_t{0}), json.value("cache", std::int64_t{0}), json.value("cost", 0.0), json.value("cost_known", true)});
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
        m.last.input + m.last.cacheCreation + m.last.output, m.last.cacheRead, m.costKnown ? m.lastCost : 0.0, m.costKnown};

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
        {"session", record.session}, {"tokens", record.tokens}, {"cache", record.cache}, {"cost", record.cost}, {"cost_known", record.costKnown}}.dump() << "\n";
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

_cold cluster::UsageReport cluster::Manager::usage(const std::string& vendor) const
{
    const std::int64_t now = cluster::now();
    const std::int64_t today = midnight_(now);
    const std::int64_t week = now - 7 * 24 * 3600;
    cluster::UsageReport report;

    std::lock_guard<std::mutex> lock(this->_mutex);
    auto limits = this->_limits.find(vendor);
    if (limits != this->_limits.end()) report = limits->second;
    report.vendor = vendor;
    for (const cluster::UsageRecord& record: this->_usage) {
        if (record.vendor != vendor) continue;
        if (report.since == 0) report.since = record.time;
        add_(report.total, record);
        if (record.time >= week) add_(report.week, record);
        if (record.time >= today) add_(report.today, record);
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
    std::vector<std::pair<std::string, std::string>> lines = {{"vendor", report.vendor.empty() ? "?" : report.vendor},
        {"today", sum_text_(report.today)}, {"7 days", sum_text_(report.week)}, {"lifetime", sum_text_(report.total)}};
    if (report.since > 0) lines.emplace_back("since", date_(report.since, "%d/%m/%Y"));
    if (report.limit5h >= 0.0) lines.emplace_back("5h left", left_text_(report.limit5h, report.reset5h, "%H:%M"));
    if (report.limit7d >= 0.0) lines.emplace_back("7d left", left_text_(report.limit7d, report.reset7d, "%a %H:%M"));
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
            {"since", report.since > 0 ? date_(report.since, "%Y-%m-%d") : std::string()}};
        if (report.limit5h >= 0.0) item["five_hour"] = {{"used_percent", static_cast<int>(report.limit5h * 100.0 + 0.5)}, {"resets_at", report.reset5h}};
        if (report.limit7d >= 0.0) item["seven_day"] = {{"used_percent", static_cast<int>(report.limit7d * 100.0 + 0.5)}, {"resets_at", report.reset7d}};
        data.push_back(item);
    }
    return data;
}
