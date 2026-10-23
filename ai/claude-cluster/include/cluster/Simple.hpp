/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 23/10/2026 by @author Tsukini

File Name:
##  @file Simple.hpp

File Description:
##  The simple mode of the interfaces (claude-cluster --simple / --ipad-kid, [ui] mode = simple): what needs you first, then a card
##  per session in plain words (its state in one word, what it does now, its todo as a progress), the same prompt, the useful
##  settings only; its words in English and French ([ui] language), shared by the terminal, the window and the page / app
\**************************************************************/

#ifndef SIMPLE_H
    #define SIMPLE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* cluster */
    #include "Types.hpp"                    // cluster::Snapshot, cluster::Json

    /* type */
    #include <cstdint>                      // std::int64_t
    #include <string>                       // std::string
    #include <vector>                       // std::vector
    #include <map>                          // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* DEFINE */

class Manager;
class Config;

//----------------------------------------------------------------//
/* STRUCT */

// A session as a card: its state in one word (working | needs | stuck | done | ready | starting), what it does now, its todo
struct SimpleCard {
    std::string id;
    std::string name;
    std::string state;
    std::string doing;              // in the words of the language
    int done = 0;                   // its todo: done / total (0 / 0: none)
    int total = 0;
    std::string tag;                // its category (else its first tag)
    std::int64_t since = 0;         // its last change (unix seconds): "12 min ago" of a finished one
    bool global = false;
};

// What waits for you: a permission of a session (a question has its own popup)
struct SimpleAsk {
    std::string session;
    std::string name;
    std::string requestId;
    std::string what;               // "checkout wants to run a command"
    std::string detail;             // the command / the file / why it asks
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard bool simple_on(cluster::Manager& manager);                                       // the simple mode shown
_cold _nodiscard std::string simple_lang(cluster::Manager& manager);                              // en | fr
_cold _nodiscard std::string simple_word(const std::string& key, const std::string& lang);              // a word of the simple mode (its key when unknown)
_cold _nodiscard std::string simple_fill(std::string text, const std::map<std::string, std::string>& values);   // {name} -> its value
_cold _nodiscard cluster::Json simple_words(const std::string& lang);                                   // every word, for the page
_cold _nodiscard cluster::SimpleCard simple_card(const cluster::Snapshot& snap, const std::string& lang);
_cold _nodiscard std::vector<cluster::SimpleAsk> simple_asks(const std::vector<cluster::Snapshot>& sessions, const std::string& lang);
_cold _nodiscard std::string simple_greeting(const cluster::Config& config, const std::string& lang);   // "Good evening, Tsukini"
_cold _nodiscard std::string simple_ago(const std::int64_t since, const std::string& lang);             // "12 min ago"
_cold _nodiscard cluster::Json simple_json(cluster::Manager& manager);                            // cards, asks, greeting, counts: the page

} // namespace end

#endif /* SIMPLE_H */
