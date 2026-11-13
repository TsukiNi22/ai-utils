/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Rewind.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Rewind.hpp"
#include "cluster/Tools.hpp"

_cold std::vector<cluster::RewindPoint> cluster::rewind_points(const std::vector<cluster::Entry>& entries)
{
    std::vector<cluster::RewindPoint> points;
    std::size_t tools = 0;      // of the entries after the prompt looked at
    std::size_t prompts = 0;
    std::size_t chars = 0;      // the answer after the prompt, up to the next prompt
    std::size_t answerAfter = 0;
    int back = 0;
    for (std::size_t i = entries.size(); i-- > 0;) {
        const cluster::Entry& entry = entries[i];
        if (entry.kind == cluster::EntryKind::Tool) ++tools;
        if (entry.kind == cluster::EntryKind::Assistant) chars += entry.text.size();
        if (entry.kind != cluster::EntryKind::User) continue;
        ++prompts;
        cluster::RewindPoint point;
        point.back = ++back;
        point.time = entry.time;
        const std::string text = cluster::trim(entry.text);
        point.label = cluster::one_line(text.substr(0, text.find('\n')), 200);
        point.messages = entries.size() - i;
        point.prompts = prompts;
        point.tools = tools;
        point.answerChars = chars - answerAfter;
        points.push_back(point);
        answerAfter = chars;    // the answers of the older prompts are counted from here
    }
    return points;
}
