/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗ 
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 21/09/2026 by @author Tsukini

File Name:
##  @file ArgParsers.hpp

File Description:
##  Declaration of the ArgParser type for void & non void function
\**************************************************************/

#ifndef ARGPARSERSTYPE_H
    #define ARGPARSERSTYPE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include <functional>   // std::function
    #include <optional>     // std::optional
    #include <vector>       // std::vector
    #include <string>       // std::string
    #include <deque>        // std::deque
    #include <tuple>        // std::tuple

namespace utils::arguments { // namespace
//----------------------------------------------------------------//
/* STRUCT */

// Return of the parser
/* internaly used */
struct ParsedUsageFull {
    std::string id; // empty -> nothing
    bool valid = true; // keep the usage for the final return
    bool ordered = false; // is the usage ordered
    std::deque<std::pair<std::string, bool>> ids; // ids that are still not used in the arguments
    std::vector<std::tuple<std::string, bool, std::vector<std::string>>> arguments; // <id, option(true)|flag(false), {option}>
};

/* user return */
struct ParsedUsage {
    std::string id; // empty -> nothing
    std::vector<std::tuple<std::string, bool, std::vector<std::string>>> arguments; // <id, option(true)|flag(false), {option}>
};

using ParsedUsages = std::vector<utils::arguments::ParsedUsage>;

struct Usage {
    std::string name; // special name: default -> allow all flag (like no usage defined)
    bool ordered = false; // The order of the option will always matter, Raw option will be forced in mandatory
    std::vector<std::pair<std::string, bool>> ids; // List of ids allowed by this usage <id, mandatory>
    std::string description = "[None]";
};

struct Option {
    std::string name;
    bool exact = false;
    std::function<std::optional<std::string>(const std::string&)> check;
    std::string description = "[None]";
};

struct Flag {
    std::tuple<std::string, std::string, std::string, std::string> flag; // <short, flag, long, env>
    std::pair<bool, bool> unlimited = {false, false}; // <ulimited, ignore flag>
    std::vector<std::tuple<std::string, bool, std::function<std::optional<std::string>(const std::string&)>>> options; // <name, mandatory, check>, the order matter
    std::string description = "[None]";
};

} // namespace end
#endif /* ARGPARSERSTYPE_H */
