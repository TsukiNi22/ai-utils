/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Popup.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Popup.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <ctime>
#include <array>

_cold std::string cluster::popup_clock(const std::int64_t time)
{
    if (time <= 0) return "--:--";
    const std::time_t at = static_cast<std::time_t>(time);
    const std::time_t now = static_cast<std::time_t>(cluster::now());
    std::tm tm{};
    std::tm today{};
    localtime_r(&at, &tm);
    localtime_r(&now, &today);
    std::array<char, 32> buffer{};
    const bool same = tm.tm_yday == today.tm_yday && tm.tm_year == today.tm_year;
    const std::size_t size = std::strftime(buffer.data(), buffer.size(), same ? "%H:%M" : "%d/%m %H:%M", &tm);
    return std::string(buffer.data(), size);
}

_cold std::string cluster::popup_size(const std::size_t chars)
{
    if (chars < 1000) return std::to_string(chars) + " chars";
    const std::size_t tenths = chars / 100;
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "k chars";
}

_cold ftxui::Element cluster::popup_row(const std::string& text, const bool on, const cluster::PopupColors& colors)
{
    const ftxui::Element item = ftxui::text(" " + text + " ");
    return on ? item | ftxui::bold | ftxui::color(colors.bg) | ftxui::bgcolor(colors.accent) : item | ftxui::color(colors.text);
}
