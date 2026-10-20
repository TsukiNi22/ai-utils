/**************************************************************\
Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Cron.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef CRON_H
    #define CRON_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    #define _Attribute
    #include <utils/utils.hpp> // _cold, _nodiscard
    #include "Types.hpp"       // cluster::Task, cluster::Schedule
    #include <cstdint>         // std::int64_t
    #include <string>          // std::string
    #include <vector>          // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* FUNCTION */

// The time rules of the schedules (the zone of each: "" = this computer's, else an IANA name like Europe/Paris)
// cron: 5 fields "min hour day month weekday" (lists, ranges, steps, names: mon-fri, jan); the day of the month and the
// weekday both restricted: either one matches (like cron)
_cold _nodiscard std::string cron_check(const std::string& expr);                                           // "" valid, else why not
_cold _nodiscard std::int64_t cron_next(const std::string& expr, const std::string& zone, const std::int64_t after);  // the first time after, 0 none
_cold _nodiscard std::string zone_check(const std::string& zone);                                           // "" valid, else why not
// weekdays: "mon-fri", "mon,wed", "weekdays", "weekend", "daily" (empty: every day); windows: "09:00-18:00,20:00-22:00"
_cold _nodiscard std::string days_check(const std::string& days);
_cold _nodiscard std::string window_check(const std::string& window);
_cold _nodiscard bool time_allowed(const std::string& days, const std::string& window, const std::string& quiet, const std::string& zone, const std::int64_t time);
_cold _nodiscard std::int64_t time_next_allowed(const std::string& days, const std::string& window, const std::string& quiet, const std::string& zone, const std::int64_t time); // the first allowed minute from time (0: none within 8 days)
_cold _nodiscard std::string zone_clock(const std::int64_t time, const std::string& zone);                  // "YYYY-MM-DD HH:MM" in the zone
_cold _nodiscard std::int64_t zone_parse(const std::string& when, const std::string& zone, const std::int64_t now);   // "HH:MM" / "YYYY-MM-DD HH:MM" in the zone (-1: not this format)

// What a schedule / a task holds besides its text, in a few words for the interfaces: "cron */15 9-17 * * mon-fri · Europe/Paris · until 2026-11-01 11:00",
// "priority 2 · after t1 · event git_push · own session · retry 1/3 · timeout 30m · budget $1.50"
_cold _nodiscard std::string schedule_rule(const cluster::Schedule& w);
_cold _nodiscard std::string task_traits(const cluster::Task& t);

} // namespace end
#endif /* CRON_H */
