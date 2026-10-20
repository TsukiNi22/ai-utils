/**************************************************************\
Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Cron.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Tools.hpp"
#include "cluster/Cron.hpp"
#include <functional>
#include <algorithm>
#include <sstream>
#include <bitset>
#include <chrono>
#include <cstdio>
#include <regex>
#include <map>

/* parts of a time */
namespace {

struct Parts {
    int year = 0;
    int month = 0;      // 1..12
    int day = 0;        // 1..31
    int hour = 0;
    int minute = 0;
    int weekday = 0;    // 0 = Sunday
};

namespace chr = std::chrono;

_cold const chr::time_zone* zone_(const std::string& zone)
{
    if (zone.empty()) return chr::current_zone();
    return chr::locate_zone(zone);
}

_cold Parts parts_(const chr::time_zone* zone, const std::int64_t time)
{
    const chr::zoned_time<chr::seconds> zoned(zone, chr::sys_seconds(chr::seconds(time)));
    const chr::local_seconds local = zoned.get_local_time();
    const chr::local_days days = chr::floor<chr::days>(local);
    const chr::year_month_day ymd{days};
    const chr::hh_mm_ss<chr::seconds> hms{local - days};
    return {static_cast<int>(ymd.year()), static_cast<int>(static_cast<unsigned>(ymd.month())), static_cast<int>(static_cast<unsigned>(ymd.day())),
        static_cast<int>(hms.hours().count()), static_cast<int>(hms.minutes().count()), static_cast<int>(chr::weekday{days}.c_encoding())};
}

// a local date and time -> unix seconds (0: that time does not exist, a change of hour)
_cold std::int64_t unix_(const chr::time_zone* zone, const int year, const int month, const int day, const int hour, const int minute)
{
    const chr::year_month_day ymd{chr::year(year), chr::month(static_cast<unsigned>(month)), chr::day(static_cast<unsigned>(day))};
    if (!ymd.ok()) return 0;
    try {
        const chr::local_seconds local = chr::local_days{ymd} + chr::hours(hour) + chr::minutes(minute);
        return zone->to_sys(local, chr::choose::earliest).time_since_epoch().count();
    } catch (const std::exception&) {
        return 0;
    }
}

_cold std::int64_t add_days_(const chr::time_zone* zone, const Parts& from, const int offset, const int hour, const int minute)
{
    const chr::local_days day = chr::local_days{chr::year_month_day{chr::year(from.year), chr::month(static_cast<unsigned>(from.month)), chr::day(static_cast<unsigned>(from.day))}}
        + chr::days(offset);
    const chr::year_month_day ymd{day};
    return unix_(zone, static_cast<int>(ymd.year()), static_cast<int>(static_cast<unsigned>(ymd.month())), static_cast<int>(static_cast<unsigned>(ymd.day())), hour, minute);
}

} // namespace

/* cron */
namespace {

struct Cron {
    std::bitset<60> minutes;
    std::bitset<24> hours;
    std::bitset<32> days;
    std::bitset<13> months;
    std::bitset<7> weekdays;
    bool anyDay = false;        // the day of the month is "*"
    bool anyWeekday = false;
};

const std::map<std::string, int>& names_(void)
{
    static const std::map<std::string, int> names = {
        {"jan", 1}, {"feb", 2}, {"mar", 3}, {"apr", 4}, {"may", 5}, {"jun", 6}, {"jul", 7}, {"aug", 8}, {"sep", 9}, {"oct", 10}, {"nov", 11}, {"dec", 12},
        {"sun", 0}, {"mon", 1}, {"tue", 2}, {"wed", 3}, {"thu", 4}, {"fri", 5}, {"sat", 6},
    };
    return names;
}

// one field: "*", "5", "1-5", "*/15", "1-30/5", "mon,wed", the values set in mark (an error: its text)
_cold std::string field_(const std::string& text, const int low, const int high, const bool names, const std::function<void(const int)>& mark)
{
    if (text.empty()) return "empty field";
    std::stringstream stream(text);
    std::string item;
    const std::function<bool(const std::string&, int&)> number = [&](const std::string& word, int& out) -> bool {
        if (names && names_().contains(cluster::lower(word))) {
            out = names_().at(cluster::lower(word));
            return true;
        }
        if (word.empty() || !std::all_of(word.begin(), word.end(), [](const char c) {return std::isdigit(static_cast<unsigned char>(c)) != 0;}) || word.size() > 3) return false;
        out = std::stoi(word);
        return true;
    };
    while (std::getline(stream, item, ',')) {
        int step = 1;
        const std::size_t slash = item.find('/');
        if (slash != std::string::npos) {
            if (!number(item.substr(slash + 1), step) || step < 1) return "bad step in '" + item + "'";
            item = item.substr(0, slash);
        }
        int from = low;
        int to = high;
        if (item != "*") {
            const std::size_t dash = item.find('-');
            if (dash == std::string::npos) {
                if (!number(item, from)) return "bad value '" + item + "'";
                to = slash == std::string::npos ? from : high;
            } else if (!number(item.substr(0, dash), from) || !number(item.substr(dash + 1), to)) {
                return "bad range '" + item + "'";
            }
        }
        if (names && high == 6 && to == 7) to = 6; // 7 = Sunday
        if (names && high == 6 && from == 7) from = 0;
        if (from < low || to > high || from > to) return "'" + item + "' out of " + std::to_string(low) + ".." + std::to_string(high);
        for (int v = from; v <= to; v += step)
            mark(v);
    }
    return "";
}

_cold std::string parse_(const std::string& expr, Cron& cron)
{
    std::vector<std::string> words;
    std::stringstream stream(expr);
    std::string word;
    while (stream >> word)
        words.push_back(word);
    if (words.size() != 5) return "cron: 5 fields (minute hour day-of-month month weekday), not " + std::to_string(words.size());
    std::string error = field_(words[0], 0, 59, false, [&](const int v) {cron.minutes.set(static_cast<std::size_t>(v));});
    if (error.empty()) error = field_(words[1], 0, 23, false, [&](const int v) {cron.hours.set(static_cast<std::size_t>(v));});
    if (error.empty()) error = field_(words[2], 1, 31, false, [&](const int v) {cron.days.set(static_cast<std::size_t>(v));});
    if (error.empty()) error = field_(words[3], 1, 12, true, [&](const int v) {cron.months.set(static_cast<std::size_t>(v));});
    if (error.empty()) error = field_(words[4], 0, 6, true, [&](const int v) {cron.weekdays.set(static_cast<std::size_t>(v));});
    cron.anyDay = words[2].starts_with("*");
    cron.anyWeekday = words[4].starts_with("*");
    return error.empty() ? "" : "cron: " + error;
}

} // namespace

_cold std::string cluster::cron_check(const std::string& expr)
{
    Cron cron;
    return parse_(expr, cron);
}

_cold std::string cluster::zone_check(const std::string& zone)
{
    try {
        (void)zone_(zone);
        return "";
    } catch (const std::exception&) {
        return "unknown timezone '" + zone + "' (an IANA name: Europe/Paris, America/New_York, UTC)";
    }
}

_cold std::int64_t cluster::cron_next(const std::string& expr, const std::string& zone, const std::int64_t after)
{
    Cron cron;
    if (!parse_(expr, cron).empty()) return 0;
    const chr::time_zone* tz = zone_(zone);
    const Parts start = parts_(tz, after);
    for (int offset = 0; offset < 800; ++offset) { // a day at a time (a rare date: Feb 29, within two years)
        const std::int64_t noon = add_days_(tz, start, offset, 12, 0);
        if (noon == 0) continue;
        const Parts day = parts_(tz, noon);
        const bool dom = cron.days.test(static_cast<std::size_t>(day.day));
        const bool dow = cron.weekdays.test(static_cast<std::size_t>(day.weekday));
        const bool match = cron.anyDay || cron.anyWeekday ? dom && dow : dom || dow;
        if (!cron.months.test(static_cast<std::size_t>(day.month)) || !match) continue;
        for (int hour = 0; hour < 24; ++hour) {
            if (!cron.hours.test(static_cast<std::size_t>(hour))) continue;
            for (int minute = 0; minute < 60; ++minute) {
                if (!cron.minutes.test(static_cast<std::size_t>(minute))) continue;
                const std::int64_t at = unix_(tz, day.year, day.month, day.day, hour, minute);
                if (at > after) return at;
            }
        }
    }
    return 0;
}

/* days and windows */
namespace {

_cold std::bitset<7> days_(const std::string& days, std::string& error)
{
    std::bitset<7> set;
    const std::string text = cluster::lower(cluster::trim(days));
    if (text.empty() || text == "daily" || text == "everyday") {
        set.set();
        return set;
    }
    if (text == "weekdays") return std::bitset<7>("0111110");
    if (text == "weekend") return std::bitset<7>("1000001");
    error = field_(text, 0, 6, true, [&](const int v) {set.set(static_cast<std::size_t>(v));});
    if (!error.empty()) error = "days: " + error + " (mon-fri, mon,wed, weekdays, weekend)";
    return set;
}

// "HH:MM-HH:MM" -> minutes of the day (an end before the start: past midnight)
struct Range {
    int from = 0;
    int to = 0;
};

_cold std::vector<Range> ranges_(const std::string& text, std::string& error)
{
    std::vector<Range> out;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        std::smatch match;
        const std::string trimmed = cluster::trim(item);
        if (trimmed.empty()) continue;
        if (!std::regex_match(trimmed, match, std::regex(R"((\d{1,2}):(\d{2})\s*-\s*(\d{1,2}):(\d{2}))")) || std::stoi(match[1]) > 24 || std::stoi(match[3]) > 24
            || std::stoi(match[2]) > 59 || std::stoi(match[4]) > 59) {
            error = "window: HH:MM-HH:MM[,HH:MM-HH:MM] - not '" + trimmed + "'";
            return out;
        }
        out.push_back({std::stoi(match[1]) * 60 + std::stoi(match[2]), std::stoi(match[3]) * 60 + std::stoi(match[4])});
    }
    return out;
}

_cold bool in_(const std::vector<Range>& list, const int minute)
{
    return std::any_of(list.begin(), list.end(), [minute](const Range& r) {return r.from <= r.to ? minute >= r.from && minute < r.to : minute >= r.from || minute < r.to;});
}

} // namespace

_cold std::string cluster::days_check(const std::string& days)
{
    std::string error;
    (void)days_(days, error);
    return error;
}

_cold std::string cluster::window_check(const std::string& window)
{
    std::string error;
    (void)ranges_(window, error);
    return error;
}

_cold bool cluster::time_allowed(const std::string& days, const std::string& window, const std::string& quiet, const std::string& zone, const std::int64_t time)
{
    if (days.empty() && window.empty() && quiet.empty()) return true;
    std::string error;
    const std::bitset<7> set = days_(days, error);
    const std::vector<Range> allowed = ranges_(window, error);
    const std::vector<Range> blocked = ranges_(quiet, error);
    const Parts p = parts_(zone_(zone), time);
    const int minute = p.hour * 60 + p.minute;
    return set.test(static_cast<std::size_t>(p.weekday)) && (allowed.empty() || in_(allowed, minute)) && !in_(blocked, minute);
}

_cold std::int64_t cluster::time_next_allowed(const std::string& days, const std::string& window, const std::string& quiet, const std::string& zone, const std::int64_t time)
{
    std::int64_t at = time - time % 60;
    if (at < time) at += 60;
    for (int i = 0; i < 8 * 1440; ++i, at += 60)
        if (cluster::time_allowed(days, window, quiet, zone, at)) return at;
    return 0;
}

_cold std::string cluster::zone_clock(const std::int64_t time, const std::string& zone)
{
    const Parts p = parts_(zone_(zone), time);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d", p.year, p.month, p.day, p.hour, p.minute);
    return buffer;
}

_cold std::int64_t cluster::zone_parse(const std::string& when, const std::string& zone, const std::int64_t now)
{
    const chr::time_zone* tz = zone_(zone);
    std::smatch match;
    if (std::regex_match(when, match, std::regex(R"((\d{1,2}):(\d{2}))"))) {
        const Parts today = parts_(tz, now);
        std::int64_t at = unix_(tz, today.year, today.month, today.day, std::stoi(match[1]), std::stoi(match[2]));
        if (at <= now) at = add_days_(tz, today, 1, std::stoi(match[1]), std::stoi(match[2]));
        return at;
    }
    if (std::regex_match(when, match, std::regex(R"((\d{4})-(\d{2})-(\d{2})[ T](\d{1,2}):(\d{2}))")))
        return unix_(tz, std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3]), std::stoi(match[4]), std::stoi(match[5]));
    return -1;
}

/* descriptions */
_cold std::string cluster::schedule_rule(const cluster::Schedule& w)
{
    std::vector<std::string> parts;
    if (w.action != "prompt") parts.push_back(w.action == "command" ? "command, no model" : w.action);
    if (!w.reset.empty()) parts.push_back("at each " + w.reset + " reset");
    if (!w.cron.empty()) parts.push_back("cron " + w.cron);
    if (!w.days.empty()) parts.push_back(w.days);
    if (!w.window.empty()) parts.push_back("only " + w.window);
    if (!w.quiet.empty()) parts.push_back("not " + w.quiet);
    if (!w.tz.empty()) parts.push_back(w.tz);
    if (w.until > 0) parts.push_back("until " + cluster::zone_clock(w.until, w.tz));
    if (w.maxRuns > 0) parts.push_back(std::to_string(w.runs) + "/" + std::to_string(w.maxRuns) + " runs");
    if (!w.cond.sessionIdle.empty()) parts.push_back("if " + w.cond.sessionIdle + " idle");
    if (!w.cond.command.empty()) parts.push_back("if `" + cluster::one_line(w.cond.command, 30) + "` ok");
    if (!w.cond.fileChanged.empty()) parts.push_back("if " + cluster::one_line(w.cond.fileChanged, 30) + " changed");
    if (!w.cond.usage.empty()) parts.push_back("if limit " + w.cond.usage);
    if (w.ack) parts.push_back(w.pending ? "asking again (" + std::to_string(w.asks) + ") until acknowledged" : "ack asked");
    if (w.skipped > 0) parts.push_back(std::to_string(w.skipped) + " skipped");
    std::string out;
    for (const std::string& part: parts)
        out += (out.empty() ? "" : " · ") + part;
    return out;
}

_cold std::string cluster::task_traits(const cluster::Task& t)
{
    std::vector<std::string> parts;
    if (t.priority != 0) parts.push_back("priority " + std::to_string(t.priority));
    if (!t.after.empty()) {
        std::string deps;
        for (const std::string& dep: t.after)
            deps += (deps.empty() ? "" : ",") + dep;
        parts.push_back("after " + deps);
    }
    if (!t.afterSessions.empty()) parts.push_back("after " + std::to_string(t.afterSessions.size()) + " session(s) idle");
    if (!t.on.type.empty()) parts.push_back("on " + t.on.type + (t.on.met ? " (met)" : ""));
    if (!t.spawn.cwd.empty() || t.spawned) parts.push_back(t.spawn.oneShot ? "own session, closed after" : "own session");
    if (t.retry > 0) parts.push_back("retry " + std::to_string(t.retries) + "/" + std::to_string(t.retry));
    if (t.timeout > 0) parts.push_back("timeout " + std::to_string(t.timeout) + "m");
    if (t.maxCost > 0.0) parts.push_back("max $" + std::to_string(t.maxCost).substr(0, std::to_string(t.maxCost).find('.') + 3));
    if (t.maxTokens > 0) parts.push_back("max " + cluster::human_tokens(t.maxTokens) + " tokens");
    if (t.maxConcurrency > 0) parts.push_back("max " + std::to_string(t.maxConcurrency) + " at once");
    if (!t.schedule.empty()) parts.push_back("from " + t.schedule);
    std::string out;
    for (const std::string& part: parts)
        out += (out.empty() ? "" : " · ") + part;
    return out;
}
