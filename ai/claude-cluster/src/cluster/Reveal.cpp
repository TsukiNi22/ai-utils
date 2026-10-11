/**************************************************************\
Edition:
##  @date 10/10/2026 by @author Tsukini

File Name:
##  @file Reveal.cpp

File Description:
##  Typewriter display of the answers (a speed in characters per
##  second, faster when a long text is behind: never late)
\**************************************************************/

/* include */
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Reveal.hpp"
#include <algorithm>
#include <chrono>

/* step */
_cold std::size_t cluster::Reveal::advance_(State& state, const std::string& text, const int speed)
{
    // the same text going on (stream -> final answer): kept; another one: from the start
    const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const std::size_t shown = std::min(static_cast<std::size_t>(state.shown), text.size());
    if (state.text.size() < shown || text.compare(0, shown, state.text, 0, shown) != 0) state.shown = 0.0;
    state.text = text;
    const double seconds = state.last == 0 ? 0.0 : std::min(0.25, static_cast<double>(now - state.last) / 1000.0);
    state.last = now;
    const double remaining = static_cast<double>(text.size()) - state.shown;
    const double rate = std::max(static_cast<double>(std::max(speed, 10)), remaining / 1.5); // a long text: done in ~1.5 s
    state.shown = std::min(static_cast<double>(text.size()), state.shown + rate * seconds);
    std::size_t bytes = static_cast<std::size_t>(state.shown);
    while (bytes < text.size() && (static_cast<unsigned char>(text[bytes]) & 0xC0) == 0x80)
        ++bytes;
    if (bytes < text.size()) this->_active = true;
    return bytes;
}

_cold cluster::Frame cluster::Reveal::step(const cluster::Snapshot& s, const bool enabled, const int speed)
{
    const std::string& id = s.spec.id;
    cluster::Frame frame{s.entries.size(), static_cast<std::size_t>(-1), 0, s.partial.size()};
    const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const std::int64_t drawn = this->_drawn.contains(id) ? this->_drawn[id] : 0;
    this->_drawn[id] = now;
    auto seen = this->_seen.find(id);
    if (!enabled || seen == this->_seen.end() || seen->second > s.entries.size() || now - drawn > 1000) {
        // off, first drawing, entries dropped, or a session shown again (switched to): what it has, at once; the text
        // being streamed goes on from here
        this->_seen[id] = s.entries.size();
        State& state = this->_states[id];
        state.text = s.partial;
        state.shown = static_cast<double>(s.partial.size());
        state.last = now;
        return frame;
    }
    State& state = this->_states[id];
    // the first answer not typed yet; the entries after it wait for its end
    for (std::size_t i = seen->second; i < s.entries.size(); ++i) {
        if (s.entries[i].kind != cluster::EntryKind::Assistant) continue;
        frame.visible = i + 1;
        frame.typed = i;
        frame.typedBytes = this->advance_(state, s.entries[i].text, speed);
        frame.partialBytes = 0;
        if (frame.typedBytes >= s.entries[i].text.size()) seen->second = i + 1; // done: the next drawing goes on
        return frame;
    }
    seen->second = s.entries.size();
    frame.partialBytes = s.partial.empty() ? 0 : this->advance_(state, s.partial, speed);
    return frame;
}
