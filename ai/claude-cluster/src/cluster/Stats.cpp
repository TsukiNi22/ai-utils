/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Stats.cpp

File Description:
##  The Stats session (see Stats.hpp)
\**************************************************************/

#include "cluster/Manager.hpp"
#include "cluster/Stats.hpp"
#include "cluster/Tools.hpp"
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <map>

//----------------------------------------------------------------//
/* TOOLS */

_cold static std::string day_of_(const std::int64_t time)
{
    std::time_t stamp = static_cast<std::time_t>(time);
    std::tm local{};
    ::localtime_r(&stamp, &local);
    std::ostringstream out;
    out << std::put_time(&local, "%m-%d");
    return out.str();
}

_cold static double round_(const double value, const int digits)
{
    double scale = 1.0;
    for (int i = 0; i < digits; ++i)
        scale *= 10.0;
    return std::round(value * scale) / scale;
}

_cold static std::string duration_(const std::int64_t seconds)
{
    if (seconds < 90) return std::to_string(seconds) + " s";
    if (seconds < 5400) return std::to_string(seconds / 60) + " min";
    if (seconds < 172800) return std::to_string(seconds / 3600) + " h " + std::to_string(seconds % 3600 / 60) + " min";
    return std::to_string(seconds / 86400) + " d " + std::to_string(seconds % 86400 / 3600) + " h";
}

_cold static std::string tokens_(const std::int64_t n)
{
    return cluster::human_tokens(n);
}

//----------------------------------------------------------------//
/* FUNCTION */

_cold std::vector<cluster::Json> cluster::stats_blocks(cluster::Manager& manager, const int days)
{
    const std::int64_t now = cluster::now();
    const std::vector<cluster::UsageRecord> ledger = manager.usageLedger();
    const std::vector<cluster::Json> events = manager.statsEvents(now - 90 * 86400);
    std::map<std::string, std::string> names;     // a session id: its name (open or closed)
    for (const cluster::Snapshot& s: manager.list(true))
        names[s.spec.id] = s.spec.global ? "global" : s.spec.name;
    for (const cluster::SessionSpec& spec: manager.trash())
        names.emplace(spec.id, spec.name + " (closed)");

    // ---- the days of the window: oldest first
    std::vector<std::string> labels;
    std::map<std::string, std::size_t> slot;
    for (int d = days - 1; d >= 0; --d) {
        const std::string day = day_of_(now - static_cast<std::int64_t>(d) * 86400);
        slot[day] = labels.size();
        labels.push_back(day);
    }
    std::vector<double> cost(labels.size(), 0.0);
    std::vector<double> tokens(labels.size(), 0.0);
    std::vector<double> turns(labels.size(), 0.0);
    std::map<std::string, std::pair<double, std::int64_t>> byModel;
    std::map<std::string, std::pair<double, std::int64_t>> byProvider;
    struct Row {
        std::int64_t turns = 0;
        std::int64_t tokens = 0;
        double cost = 0.0;
        std::int64_t workMs = 0;
        std::int64_t last = 0;
    };
    std::map<std::string, Row> bySession;
    std::int64_t input = 0;
    std::int64_t cacheRead = 0;
    std::int64_t cacheWrite = 0;
    std::int64_t output = 0;
    std::int64_t thinking = 0;
    std::int64_t workMs = 0;
    std::int64_t longestMs = 0;
    std::int64_t timedTurns = 0;
    double today = 0.0;
    double week = 0.0;
    double month = 0.0;
    std::int64_t todayTokens = 0;
    std::int64_t todayTurns = 0;
    std::int64_t workToday = 0;
    std::time_t stamp = static_cast<std::time_t>(now);
    std::tm local{};
    ::localtime_r(&stamp, &local);
    local.tm_hour = local.tm_min = local.tm_sec = 0;
    const std::int64_t midnight = static_cast<std::int64_t>(std::mktime(&local));
    local.tm_mday = 1;
    const std::int64_t monthStart = static_cast<std::int64_t>(std::mktime(&local));
    std::vector<std::vector<double>> heat(7, std::vector<double>(24, 0.0));
    for (const cluster::UsageRecord& r: ledger) {
        const std::int64_t age = now - r.time;
        std::time_t at = static_cast<std::time_t>(r.time);
        std::tm when{};
        ::localtime_r(&at, &when);
        if (age <= 90 * 86400) heat[static_cast<std::size_t>((when.tm_wday + 6) % 7)][static_cast<std::size_t>(when.tm_hour)] += 1.0; // Monday first
        if (age > static_cast<std::int64_t>(days) * 86400) continue;
        const auto it = slot.find(day_of_(r.time));
        if (it != slot.end()) {
            cost[it->second] += r.costKnown ? r.cost : 0.0;
            tokens[it->second] += static_cast<double>(r.tokens + r.cache);
            turns[it->second] += 1.0;
        }
        const double c = r.costKnown ? r.cost : 0.0;
        byModel[r.model.empty() ? "?" : r.model].first += c;
        byModel[r.model.empty() ? "?" : r.model].second += r.tokens;
        const std::string billing = manager.authKind(r.vendor);
        byProvider[r.vendor + " (" + billing + ")"].first += c;
        byProvider[r.vendor + " (" + billing + ")"].second += r.tokens;
        Row& row = bySession[r.session];
        ++row.turns;
        row.tokens += r.tokens;
        row.cost += c;
        row.workMs += r.durMs;
        row.last = std::max(row.last, r.time);
        input += r.input;
        cacheRead += r.cacheRead;
        cacheWrite += r.cacheWrite;
        output += r.output;
        thinking += r.thinking;
        if (r.durMs > 0) {
            workMs += r.durMs;
            ++timedTurns;
            longestMs = std::max(longestMs, r.durMs);
        }
        if (r.time >= midnight) {
            today += c;
            todayTokens += r.tokens + r.cache;
            ++todayTurns;
            workToday += r.durMs;
        }
        if (age <= 7 * 86400) week += c;
        if (r.time >= monthStart) month += c;
    }
    const std::int64_t sumCost = static_cast<std::int64_t>(0);
    (void)sumCost;
    double total30 = 0.0;
    double turns30 = 0.0;
    for (std::size_t i = 0; i < cost.size(); ++i) {
        total30 += cost[i];
        turns30 += turns[i];
    }
    std::vector<cluster::Json> blocks;
    const std::function<std::vector<double>(const std::vector<double>&, std::size_t)> lastN = [](const std::vector<double>& v, const std::size_t n) {
        return std::vector<double>(v.end() - static_cast<long>(std::min(n, v.size())), v.end());
    };
    std::vector<double> rounded(cost.size());
    for (std::size_t i = 0; i < cost.size(); ++i)
        rounded[i] = round_(cost[i], 3);

    // ---- usage: the numbers
    const double denominator = static_cast<double>(input + cacheRead + cacheWrite);
    blocks.push_back({{"id", "stats-head"}, {"type", "callout"}, {"level", "info"}, {"text", "Statistics · the last " + std::to_string(days) + " days · updated " + cluster::clock_time(now)
        + " (refreshed every minute while this session is open; F10 / the palette: Stats)"}});
    blocks.push_back({{"id", "stats-money"}, {"type", "metrics"}, {"title", "Usage"}, {"items", {
        {{"label", "today"}, {"value", round_(today, 3)}, {"unit", " $"}, {"trend", lastN(rounded, 14)}},
        {{"label", "7 days"}, {"value", round_(week, 2)}, {"unit", " $"}},
        {{"label", "this month"}, {"value", round_(month, 2)}, {"unit", " $"}},
        {{"label", "tokens today"}, {"value", tokens_(todayTokens)}},
        {{"label", "turns today"}, {"value", todayTurns}},
        {{"label", "cost per turn"}, {"value", turns30 > 0 ? round_(total30 / turns30, 4) : 0.0}, {"unit", " $"}}}}});
    blocks.push_back({{"id", "stats-cost-day"}, {"type", "chart"}, {"title", "Cost per day (USD)"}, {"kind", "line"}, {"labels", labels}, {"series", {{{"name", "cost"}, {"values", rounded}}}}});
    std::vector<double> tokensK(tokens.size());
    for (std::size_t i = 0; i < tokens.size(); ++i)
        tokensK[i] = std::round(tokens[i] / 1000.0);
    blocks.push_back({{"id", "stats-tokens-day"}, {"type", "chart"}, {"title", "Tokens per day (thousands, the cache read included)"}, {"kind", "bar"}, {"labels", labels},
        {"series", {{{"name", "tokens (k)"}, {"values", tokensK}}}}});

    // ---- by model and by provider (the cost; the tokens when nothing costs)
    const std::function<cluster::Json(const std::string&, const std::string&, const std::map<std::string, std::pair<double, std::int64_t>>&, const std::string&)> breakdown =
        [](const std::string& id, const std::string& title, const std::map<std::string, std::pair<double, std::int64_t>>& by, const std::string& kind) {
            double sum = 0.0;
            for (const auto &[name, v]: by)
                sum += v.first;
            const bool money = sum > 0.0;
            std::vector<std::pair<std::string, double>> rows;
            for (const auto &[name, v]: by)
                rows.emplace_back(name, money ? v.first : static_cast<double>(v.second));
            std::sort(rows.begin(), rows.end(), [](const std::pair<std::string, double>& a, const std::pair<std::string, double>& b) {return a.second > b.second;});
            if (rows.size() > 8) rows.resize(8);
            cluster::Json labelsJson = cluster::Json::array();
            cluster::Json values = cluster::Json::array();
            for (const auto &[name, v]: rows) {
                labelsJson.push_back(name);
                values.push_back(money ? round_(v, 3) : v);
            }
            return cluster::Json{{"id", id}, {"type", "chart"}, {"title", title + (money ? " (USD)" : " (tokens: no cost known)")}, {"kind", kind}, {"labels", labelsJson},
                {"series", cluster::Json::array({{{"name", money ? "cost" : "tokens"}, {"values", values}}})}};
        };
    blocks.push_back(breakdown("stats-models", "By model", byModel, "pie"));
    blocks.push_back(breakdown("stats-providers", "By provider (api: an API key · auth: a login · local)", byProvider, "bar"));

    // ---- the sessions: the ranking
    std::vector<std::pair<std::string, Row>> ranking(bySession.begin(), bySession.end());
    std::sort(ranking.begin(), ranking.end(), [](const std::pair<std::string, Row>& a, const std::pair<std::string, Row>& b) {return a.second.cost != b.second.cost ? a.second.cost > b.second.cost : a.second.tokens > b.second.tokens;});
    cluster::Json rows = cluster::Json::array();
    for (std::size_t i = 0; i < ranking.size() && i < 10; ++i) {
        const auto &[id, r] = ranking[i];
        rows.push_back({names.contains(id) ? names.at(id) : id, r.turns, tokens_(r.tokens), "$" + std::to_string(round_(r.cost, 3)).substr(0, 6), r.turns > 0 ? "$" + std::to_string(round_(r.cost / static_cast<double>(r.turns), 4)).substr(0, 7) : "-",
            r.workMs > 0 ? duration_(r.workMs / 1000) : "-", cluster::human_age(r.last) + " ago"});
    }
    blocks.push_back({{"id", "stats-sessions"}, {"type", "table"}, {"title", "The sessions that cost the most"}, {"columns", {"session", "turns", "tokens", "cost", "per turn", "working", "last turn"}}, {"rows", rows}});

    // ---- the cache and the thinking: where the tokens go
    blocks.push_back({{"id", "stats-cache"}, {"type", "metrics"}, {"title", "Cache and thinking"}, {"items", {
        {{"label", "cache read"}, {"value", denominator > 0 ? round_(100.0 * static_cast<double>(cacheRead) / denominator, 1) : 0.0}, {"unit", " % of the input side"}},
        {{"label", "cache written"}, {"value", denominator > 0 ? round_(100.0 * static_cast<double>(cacheWrite) / denominator, 1) : 0.0}, {"unit", " %"}},
        {{"label", "thinking"}, {"value", output > 0 ? round_(100.0 * static_cast<double>(thinking) / static_cast<double>(output), 1) : 0.0}, {"unit", " % of the output"}},
        {{"label", "output tokens"}, {"value", tokens_(output)}}}}});

    // ---- the activity: uptime and working time
    std::int64_t upTotal = 0;
    std::int64_t first = now;
    std::int64_t segment = 0;
    std::int64_t lastSeen = 0;
    for (const cluster::Json& e: events) {
        const std::int64_t t = e.value("t", std::int64_t{0});
        const std::string kind = e.value("kind", std::string());
        if (kind != "start" && kind != "up" && kind != "stop") continue;
        first = std::min(first, t);
        if (kind == "start") {
            if (segment > 0 && lastSeen > segment) upTotal += lastSeen - segment; // the one before ended without a stop (a crash): up to its last sign
            segment = t;
            lastSeen = t;
        } else if (kind == "up") {
            lastSeen = t;
        } else {
            if (segment > 0) upTotal += t - segment;
            segment = 0;
            lastSeen = 0;
        }
    }
    if (segment > 0) upTotal += now - segment; // this run
    const std::int64_t windowStart = std::max(first, now - static_cast<std::int64_t>(days) * 86400);
    const std::int64_t window = std::max<std::int64_t>(1, now - windowStart);
    blocks.push_back({{"id", "stats-uptime"}, {"type", "metrics"}, {"title", "Time"}, {"items", {
        {{"label", "core up (this run)"}, {"value", duration_(now - manager.startedAt())}},
        {{"label", "availability"}, {"value", round_(std::min(100.0, 100.0 * static_cast<double>(upTotal) / static_cast<double>(window)), 1)}, {"unit", " % since " + (first < now ? day_of_(first) : std::string("now"))}},
        {{"label", "agents working today"}, {"value", workToday > 0 ? duration_(workToday / 1000) : std::string("-")}},
        {{"label", "working (" + std::to_string(days) + " d)"}, {"value", workMs > 0 ? duration_(workMs / 1000) : std::string("-")}},
        {{"label", "average turn"}, {"value", timedTurns > 0 ? duration_(workMs / 1000 / timedTurns) : std::string("-")}},
        {{"label", "longest turn"}, {"value", longestMs > 0 ? duration_(longestMs / 1000) : std::string("-")}}}}});

    // ---- the turns by weekday and hour
    blocks.push_back({{"id", "stats-heat"}, {"type", "heatmap"}, {"title", "When the turns happen (the last 90 days: weekday × hour)"}, {"rows", heat},
        {"xlabels", {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23"}},
        {"ylabels", {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"}}});

    // ---- the tools and the permissions
    std::map<std::string, std::int64_t> used;
    struct Decision {
        std::int64_t allow = 0;
        std::int64_t always = 0;
        std::int64_t deny = 0;
    };
    std::map<std::string, Decision> decisions;
    std::map<std::string, std::int64_t> problems;
    std::vector<std::pair<std::int64_t, std::string>> recent;
    for (const cluster::Json& e: events) {
        const std::int64_t t = e.value("t", std::int64_t{0});
        if (now - t > static_cast<std::int64_t>(days) * 86400) continue;
        const std::string kind = e.value("kind", std::string());
        if (kind == "tool") {
            ++used[e.value("tool", std::string("?"))];
        } else if (kind == "perm") {
            Decision& d = decisions[e.value("tool", std::string("?"))];
            const std::string decision = e.value("decision", std::string());
            if (decision == "deny") ++d.deny;
            else if (decision == "always") ++d.always;
            else ++d.allow;
        } else if (kind == "limit" || kind == "error") {
            ++problems[kind + ": " + e.value("what", std::string())];
            recent.emplace_back(t, kind + " · " + (names.contains(e.value("session", std::string())) ? names.at(e.value("session", std::string())) + " · " : std::string())
                + cluster::one_line(e.value("what", std::string()), 90));
        }
    }
    std::vector<std::string> toolNames;
    for (const auto &[name, count]: used)
        toolNames.push_back(name);
    for (const auto &[name, d]: decisions)
        if (!used.contains(name)) toolNames.push_back(name);
    std::sort(toolNames.begin(), toolNames.end(), [&used](const std::string& a, const std::string& b) {
        const std::int64_t x = used.contains(a) ? used.at(a) : 0;
        const std::int64_t y = used.contains(b) ? used.at(b) : 0;
        return x != y ? x > y : a < b;
    });
    cluster::Json toolRows = cluster::Json::array();
    for (std::size_t i = 0; i < toolNames.size() && i < 12; ++i) {
        const std::string& name = toolNames[i];
        const Decision d = decisions.contains(name) ? decisions.at(name) : Decision{};
        toolRows.push_back({name, used.contains(name) ? used.at(name) : 0, d.allow, d.always, d.deny});
    }
    blocks.push_back({{"id", "stats-tools"}, {"type", "table"}, {"title", "Tools used and permissions asked (since this version recorded them)"},
        {"columns", {"tool", "uses", "allowed", "always", "denied"}}, {"rows", toolRows}});

    // ---- the limits and the errors
    std::sort(recent.begin(), recent.end(), [](const std::pair<std::int64_t, std::string>& a, const std::pair<std::int64_t, std::string>& b) {return a.first > b.first;});
    cluster::Json problemRows = cluster::Json::array();
    for (std::size_t i = 0; i < recent.size() && i < 8; ++i)
        problemRows.push_back({cluster::human_age(recent[i].first) + " ago", recent[i].second});
    std::int64_t limits = 0;
    std::int64_t errors = 0;
    for (const auto &[what, count]: problems)
        (what.starts_with("limit") ? limits : errors) += count;
    blocks.push_back({{"id", "stats-problems"}, {"type", "table"}, {"title", "Limits reached " + std::to_string(limits) + " · errors " + std::to_string(errors) + " (the last " + std::to_string(days) + " days)"},
        {"columns", {"when", "what"}}, {"rows", problemRows.empty() ? cluster::Json::array({{"-", "nothing"}}) : problemRows}});
    return blocks;
}

_cold std::string cluster::stats_open(cluster::Manager& manager, const bool focus)
{
    if (manager.replica()) return manager.call({{"cmd", "stats"}, {"focus", focus}}).value("id", std::string()); // the core builds it
    // the Stats session: one, reused (its folder in the data of claude-cluster); its agent never asked anything
    const std::filesystem::path folder = cluster::data_dir() / "stats";
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    std::string id;
    for (const cluster::Snapshot& s: manager.list(false))
        if (s.spec.name == "stats" && std::filesystem::path(s.spec.cwd) == folder) id = s.spec.id;
    if (id.empty()) {
        cluster::SpawnRequest request;
        request.cwd = folder.string();
        request.name = "stats";
        id = manager.spawn(request);
    }
    for (const cluster::Json& block: cluster::stats_blocks(manager, 30)) {
        const std::string displayId = block.value("id", std::string());
        if (manager.displayOf(id, displayId)) manager.displayUpdate(id, displayId, block); // in place
        else (void)manager.display(id, block);
    }
    if (focus) manager.requestFocus(id);
    return id;
}

_cold bool cluster::stats_refresh(cluster::Manager& manager)
{
    // only while the session exists (the user opened it once): its blocks updated in place
    const std::filesystem::path folder = cluster::data_dir() / "stats";
    for (const cluster::Snapshot& s: manager.list(false))
        if (s.spec.name == "stats" && std::filesystem::path(s.spec.cwd) == folder) {
            (void)cluster::stats_open(manager);
            return true;
        }
    return false;
}
