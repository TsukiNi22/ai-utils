/**************************************************************\
Edition:
##  @date 10/10/2026 by @author Tsukini

File Name:
##  @file Reveal.hpp

File Description:
##  Typewriter display of the answers: the text of an answer is
##  shown a few characters at a time, as if the tokens arrived one
##  after the other (the agents often send them in bursts)
\**************************************************************/

#ifndef CLUSTER_REVEAL_H
    #define CLUSTER_REVEAL_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Snapshot
    #include <cstdint>              // std::int64_t
    #include <cstddef>              // std::size_t
    #include <string>               // std::string
    #include <map>                  // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// What to draw of a session: its first entries, the one being typed (cut), the streamed text (cut)
struct Frame {
    std::size_t visible = 0;                    // entries shown (the ones after the typed one wait)
    std::size_t typed = static_cast<std::size_t>(-1); // index of the entry being typed (-1: none)
    std::size_t typedBytes = 0;                 // its bytes shown
    std::size_t partialBytes = 0;               // bytes shown of the streamed text
};

//----------------------------------------------------------------//
/* CLASS */

class Reveal {
    private:
        struct State {
            std::string text;                   // the text being typed
            double shown = 0.0;                 // its bytes shown
            std::int64_t last = 0;              // ms of the last step
        };
        std::map<std::string, State> _states;   // per session
        std::map<std::string, std::size_t> _seen; // entries already shown (the history is never typed)
        std::map<std::string, std::int64_t> _drawn; // ms of the last drawing of each session (not drawn: nothing typed)
        bool _active = false;

        // ---------- Pre-Function -------- //
        _cold _nodiscard std::size_t advance_(State& state, const std::string& text, const int speed);

    public:
        // ---------- Pre-Function -------- //
        _cold _nodiscard cluster::Frame step(const cluster::Snapshot& s, const bool enabled, const int speed); // once per drawing

        // ---------- Function -------- //
        _nodiscard inline bool active(void) const {return this->_active;};    // a text is being typed: draw often
        inline void frameStart(void) {this->_active = false;};               // before the sessions of a drawing

        // ---------- Constructor -------- //
        Reveal(void) = default;
        ~Reveal() = default;
};

} // namespace end

#endif /* CLUSTER_REVEAL_H */
