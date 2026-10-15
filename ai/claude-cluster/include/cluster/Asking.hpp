/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file Asking.hpp

File Description:
##  The question of an agent being answered (AskUserQuestion): one
##  for the whole cluster, shown by the interfaces (the one of a
##  sub-session on the global session), driven by the keys, the
##  mouse and the voice alike
\**************************************************************/

#ifndef ASKING_H
    #define ASKING_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Json
    #include <optional>             // std::optional
    #include <cstdint>              // std::uint64_t
    #include <utility>              // std::pair
    #include <string>               // std::string
    #include <atomic>               // std::atomic
    #include <vector>               // std::vector
    #include <mutex>                // std::mutex
    #include <set>                  // std::set

namespace cluster { // namespace start
class Manager;
//----------------------------------------------------------------//
/* STRUCT */

struct Ask {
    std::string session;            // the session of the agent asking
    std::string name;               // its name
    std::string requestId;
    cluster::Json questions;        // [{question, header, options: [{label, description}], multiSelect}]
    std::size_t index = 0;          // the question shown
    int cursor = 0;                 // its option (the last one: other, typed)
    std::set<int> picked;           // multi choice: the options ticked
    std::string other;              // the answer typed
    cluster::Json answers = cluster::Json::object();
    std::vector<std::pair<std::string, std::string>> chat;   // "Chat about this": <user | assistant, text>, the session never touched
    bool chatting = false;          // an answer of the chat awaited
};

//----------------------------------------------------------------//
/* CLASS */

class Asking {
    private:
        cluster::Manager& _manager;
        mutable std::mutex _mutex;
        std::optional<cluster::Ask> _ask;
        std::set<std::string> _offered;         // the requests opened once (declined or answered: not again)
        std::atomic<std::uint64_t> _version{1};

        // ---------- Pre-Function -------- //
        _cold _nodiscard std::string next_(void);    // under the lock

    public:
        // ---------- Pre-Function -------- //
        _cold bool check(void);                 // answered elsewhere: closed; none open: the next one of any session opened (true: a new one)
        _cold _nodiscard std::optional<cluster::Ask> current(void) const;
        _cold void move(const int delta);       // the option above / below
        _cold void choose(const int option);    // an option (multi choice: ticked / unticked); the one after the last: Other
        _cold void toggle(void);                // multi choice: the option of the cursor ticked / unticked
        _cold void type(const std::string& text);       // Other: text added
        _cold void erase(void);                 // Other: its last character
        _cold void setOther(const std::string& text);   // Other: this answer
        _cold _nodiscard std::string next(void);        // the answer recorded: the next question, or all sent ("" or why not)
        _cold void back(void);                  // the previous question
        _cold void decline(void);
        _cold _nodiscard std::string toGlobal(void);    // the whole set answered by the global session (what it did, or why not)
        _cold _nodiscard std::string chat(const std::string& text);    // "Chat about this": a message (answered in the background; "" sent)
        _cold void chatAnswer(const std::string& requestId, const std::string& text); // its answer (the question still shown)
        _cold _nodiscard std::string spoken(const std::string& sentence); // a sentence of the voice on it: what it did, "" not for it
        _cold _nodiscard cluster::Json json(void) const;  // the question shown (null: none): the web page, the commands
        _nodiscard std::uint64_t version(void) const;      // a replica: the version of its core

        // ---------- Constructor -------- //
        _cold explicit Asking(cluster::Manager& manager);
};

} // namespace end

#endif /* ASKING_H */
