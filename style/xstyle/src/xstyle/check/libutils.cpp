/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file libutils.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/check/Checks.hpp"
#include "xstyle/Tools.hpp"
#include <unordered_map>
#include <algorithm>
#include <regex>
#include <set>

/* tools */
_hot static std::string section_of_header_(const std::string& target) // utils/network/Server.hpp -> _Network
{
    static const std::vector<std::pair<std::string, std::string>> sections = {
        {"utils/network/socket/", "_Socket"}, {"utils/manip/iomanip/", "_IOManip"}, {"utils/manip/smanip/", "_SManip"},
        {"utils/security/encryption/", "_Encryption"}, {"utils/algorithms/c2dmp-hsm/", "_C2DMP"}, {"utils/algorithms/sos/", "_SOS"},
        {"utils/attribute/", "_Attribute"}, {"utils/exception/", "_Exception"}, {"utils/verbose/", "_Verbose"}, {"utils/arguments/", "_Arguments"},
        {"utils/cli/", "_Cli"}, {"utils/network/", "_Network"}, {"utils/system/", "_System"}, {"utils/math/", "_Math"}, {"utils/type/", "_CustomType"},
        {"utils/encapsulation/", "_Encapsulation"}, {"utils/pool/", "_Pool"}, {"utils/concepts/", "_Concepts"},
    };
    for (const auto &[prefix, section]: sections)
        if (target.starts_with(prefix)) return section;
    return "";
}

_hot static bool covered_(const std::string& section, const std::set<std::string>& defined)
{
    // Groups of utils.hpp: a group defines its sub-sections
    static const std::unordered_map<std::string, std::vector<std::string>> groups = {
        {"_Handling", {"_Exception", "_Verbose", "_Pool", "_Cli", "_Arguments", "_Network", "_Socket"}},
        {"_Tools", {"_Math", "_Concepts", "_Encapsulation", "_System", "_CustomType", "_Manip", "_IOManip", "_SManip", "_Algorithms", "_C2DMP", "_SOS", "_Security", "_Encryption"}},
        {"_Network", {"_Socket"}}, {"_Manip", {"_IOManip", "_SManip"}}, {"_Algorithms", {"_C2DMP", "_SOS"}}, {"_Security", {"_Encryption"}},
    };
    if (defined.contains("_Utils") || defined.contains(section)) return true;
    for (const auto &[group, members]: groups)
        if (defined.contains(group) && std::find(members.begin(), members.end(), section) != members.end()) return true;
    return false;
}

/* checks: includes & sections */
_hot static void includes_(const xstyle::SourceFile& file, const xstyle::ProjectInfo& project, xstyle::check::Issues& issues)
{
    static const std::regex include(R"(^(\s*)#\s*include\s*[<"]([^>"]+)[>"](.*)$)");
    static const std::regex define(R"(^\s*#\s*define\s+(_[A-Z]\w*)\s*$)");
    static const std::vector<std::pair<std::regex, std::string>> usage = {
        {std::regex(R"(\butils::exception::|\bOK\b|\bKO\b)"), "_Exception"}, {std::regex(R"(\butils::verbose::|\bset_verbose\b|\bon(Basic|Advanced|Debug)?Verbose[CF]?n?\b)"), "_Verbose"},
        {std::regex(R"(\butils::arguments::)"), "_Arguments"}, {std::regex(R"(\butils::cli::)"), "_Cli"},
        {std::regex(R"(\butils::network::(Server|Client|Address|Payload)\b)"), "_Network"}, {std::regex(R"(\butils::network::)"), "_Socket"},
        {std::regex(R"(\butils::system::)"), "_System"}, {std::regex(R"(\butils::math::)"), "_Math"}, {std::regex(R"(\butils::iomanip::)"), "_IOManip"},
        {std::regex(R"(\butils::smanip::)"), "_SManip"}, {std::regex(R"(\butils::type::)"), "_CustomType"}, {std::regex(R"(\butils::encapsulation::)"), "_Encapsulation"},
        {std::regex(R"(\butils::pool::)"), "_Pool"}, {std::regex(R"(\butils::security::encryption::)"), "_Encryption"}, {std::regex(R"(\butils::concepts::)"), "_Concepts"},
        {std::regex(R"(\butils::algorithms::c2dmp::)"), "_C2DMP"}, {std::regex(R"(\butils::algorithms::sos::)"), "_SOS"},
    };
    static const std::regex word(R"(\b_[a-z]\w*\b)");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();

    // Root include, the sections defined before it and the sections used by the file
    std::size_t root = NO_INDEX;
    std::set<std::string> defined;
    std::set<std::string> used;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::smatch match;
        if (std::regex_search(lines[i], match, define) && root == NO_INDEX) defined.insert(match[1].str());
        if (std::regex_search(lines[i], match, include) && (match[2].str() == "utils/utils.hpp" || match[2].str() == "utils.hpp") && root == NO_INDEX) root = i;
        if (file.getInfo()[i].preprocessor) continue;
        const bool utilsWord = code[i].find("utils::") != std::string::npos || code[i].find("OK") != std::string::npos || code[i].find("KO") != std::string::npos
            || code[i].find("erbose") != std::string::npos;
        for (const auto &[pattern, section]: usage)
            if (utilsWord && std::regex_search(code[i], pattern)) used.insert(section);
        if (code[i].find('_') == std::string::npos) continue;
        for (std::sregex_iterator it(code[i].begin(), code[i].end(), word); it != std::sregex_iterator(); ++it)
            if (project.attributeMacros.contains(it->str())) used.insert("_Attribute");
    }
    if (used.contains("_Network")) used.erase("_Socket");

    // Sub-headers included one by one
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::smatch match;
        if (!file.getInfo()[i].preprocessor || !std::regex_search(lines[i], match, include)) continue;
        const std::string target = match[2].str();
        if (!target.starts_with("utils/") || target == "utils/utils.hpp" || target.starts_with("utils/security/observer/")) continue;
        const std::string section = section_of_header_(target);
        const std::string indent = match[1].str();
        const std::string message = "libutils header <" + target + "> included directly";
        const std::string suggestion = "remove it" + (section.empty() || covered_(section, defined) ? std::string("") : ", #define " + section) + " before <utils/utils.hpp>";
        if (root != NO_INDEX) {
            // Remove the line and define the section before the root include
            const std::size_t first = std::min(i, root);
            const std::size_t last = std::max(i, root);
            std::vector<std::string> fixed;
            for (std::size_t k = first; k <= last; ++k) {
                if (k == root && !section.empty() && !covered_(section, defined)) fixed.push_back(xstyle::indentation(lines[root]) + "#define " + section);
                if (k != i) fixed.push_back(lines[k]);
            }
            issues.push_back(xstyle::check::make_issue(file, i, 0, "LU-INCLUDE", message, suggestion, xstyle::check::replace_lines(first, last - first + 1, fixed)));
        } else {
            std::vector<std::string> fixed;
            if (!section.empty() && !covered_(section, defined)) fixed.push_back(indent + "#define " + section);
            fixed.push_back(indent + "#include <utils/utils.hpp>" + match[3].str());
            issues.push_back(xstyle::check::make_issue(file, i, 0, "LU-INCLUDE", message, "", xstyle::check::replace_lines(i, 1, fixed)));
        }
        return; // one at a time: the root include moves
    }
    if (root == NO_INDEX) return;

    // Sections: none (bare include) or missing ones
    std::vector<std::string> missing;
    for (const std::string& section: used)
        if (!covered_(section, defined)) missing.push_back(section);
    std::stable_sort(missing.begin(), missing.end(), [](const std::string& a, const std::string& b) {return (a == "_Exception") > (b == "_Exception") || ((a == "_Exception") == (b == "_Exception") && (a == "_Attribute") > (b == "_Attribute"));});
    std::vector<std::string> fixed;
    for (const std::string& section: missing)
        fixed.push_back(xstyle::indentation(lines[root]) + "#define " + section);
    fixed.push_back(lines[root]);
    std::string list;
    for (const std::string& section: missing)
        list += (list.empty() ? "" : ", ") + section;

    if (defined.empty() && missing.empty())
        issues.push_back(xstyle::check::make_issue(file, root, 0, "LU-BARE-INCLUDE", "<utils/utils.hpp> without #define _Section (every section included)",
            "#define the sections used (_Exception, _Attribute...) right before it"));
    else if (defined.empty())
        issues.push_back(xstyle::check::make_issue(file, root, 0, "LU-BARE-INCLUDE", "<utils/utils.hpp> without #define _Section (uses " + list + ")", "", xstyle::check::replace_lines(root, 1, fixed)));
    else if (!missing.empty())
        issues.push_back(xstyle::check::make_issue(file, root, 0, "LU-SECTION", "Section(s) used but not defined: " + list, "", xstyle::check::replace_lines(root, 1, fixed)));
}

/* checks: attributes & migrations */
_hot static void names_(const xstyle::SourceFile& file, const xstyle::ProjectInfo& project, xstyle::check::Issues& issues)
{
    static const std::regex simple(R"(\[\[\s*(nodiscard|maybe_unused|likely|unlikely|fallthrough|gnu::hot|gnu::cold|noinline|gnu::noinline|no_unique_address|gnu::packed|gnu::constructor|gnu::destructor)\s*\]\])");
    static const std::regex withArguments(R"(\[\[\s*(deprecated|assume|gnu::nonnull)\s*(\((?:[^()]|\([^()]*\))*\))\s*\]\])");
    static const std::regex alignas_(R"((^|[^\w])alignas\s*\()");
    static const std::unordered_map<std::string, std::string> macros = {
        {"nodiscard", "_nodiscard"}, {"maybe_unused", "_unused"}, {"likely", "_likely"}, {"unlikely", "_unlikely"}, {"fallthrough", "_fallthrough"},
        {"gnu::hot", "_hot"}, {"gnu::cold", "_cold"}, {"noinline", "_noinline"}, {"gnu::noinline", "_noinline"}, {"no_unique_address", "_noaddress"},
        {"gnu::packed", "_packed"}, {"gnu::constructor", "_ctor"}, {"gnu::destructor", "_dtor"}, {"deprecated", "_deprecated"}, {"assume", "_assume"},
        {"gnu::nonnull", "_nonnull"},
    };
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();

    for (std::size_t i = 0; i < code.size(); ++i) {
        if (file.getInfo()[i].preprocessor) continue;
        const std::string& line = code[i];

        // [[attribute]] -> libutils macro (only the macros of the installed version)
        struct Replacement {
            std::size_t pos;
            std::size_t length;
            std::string text;
        };
        std::vector<Replacement> replacements;
        for (std::sregex_iterator it(line.begin(), line.end(), simple); it != std::sregex_iterator(); ++it) {
            const std::string macro = macros.at((*it)[1].str());
            if (project.attributeMacros.contains(macro)) replacements.push_back({static_cast<std::size_t>(it->position(0)), static_cast<std::size_t>(it->length(0)), macro});
        }
        for (std::sregex_iterator it(line.begin(), line.end(), withArguments); it != std::sregex_iterator(); ++it) {
            const std::string macro = macros.at((*it)[1].str());
            const std::string arguments = lines[i].substr(static_cast<std::size_t>(it->position(2)), static_cast<std::size_t>(it->length(2)));
            if (project.attributeMacros.contains(macro)) replacements.push_back({static_cast<std::size_t>(it->position(0)), static_cast<std::size_t>(it->length(0)), macro + arguments});
        }
        for (std::sregex_iterator it(line.begin(), line.end(), alignas_); it != std::sregex_iterator(); ++it)
            if (project.attributeMacros.contains("_alignas")) replacements.push_back({static_cast<std::size_t>(it->position(0) + it->length(1)), 7, "_alignas"});
        if (!replacements.empty()) {
            std::sort(replacements.begin(), replacements.end(), [](const Replacement& a, const Replacement& b) {return a.pos > b.pos;});
            std::string fixed = lines[i];
            for (const Replacement& r: replacements)
                fixed.replace(r.pos, r.length, r.text);
            issues.push_back(xstyle::check::make_issue(file, i, replacements.back().pos, "LU-ATTRIBUTE", "Standard attribute, libutils macro available", "", xstyle::check::replace_line(i, fixed)));
        }

        // Deprecated names (migration aliases of the installed libutils)
        std::string fixed = lines[i];
        std::vector<std::string> renamed;
        std::size_t first = std::string::npos;
        for (const xstyle::Migration& migration: project.migrations) {
            std::size_t pos = line.rfind(migration.from);
            while (pos != std::string::npos) {
                const std::size_t end = pos + migration.from.size();
                const bool wordEnd = end >= line.size() || !xstyle::is_word(line[end]);
                const bool wordStart = pos == 0 || (!xstyle::is_word(line[pos - 1]) && line[pos - 1] != ':');
                std::size_t before = pos;
                while (before > 0 && xstyle::is_space(line[before - 1])) --before;
                const bool memberCall = before > 0 && (line[before - 1] == '.' || (before > 1 && line.compare(before - 2, 2, "->") == 0));
                if (wordEnd && wordStart && (!migration.member || memberCall)) {
                    fixed.replace(pos, migration.from.size(), migration.to);
                    renamed.push_back(migration.from + " -> " + migration.to);
                    first = std::min(first, pos);
                }
                pos = pos == 0 ? std::string::npos : line.rfind(migration.from, pos - 1);
            }
        }
        if (renamed.empty()) continue;
        std::string list;
        for (const std::string& r: renamed)
            list += (list.empty() ? "" : ", ") + r;
        issues.push_back(xstyle::check::make_issue(file, i, first, "LU-MIGRATION", "Deprecated libutils name: " + list, "", xstyle::check::replace_line(i, fixed)));
    }
}

/* checks: code that libutils already gives */
_hot static void reinvent_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    struct Pattern {
        std::regex regex;
        std::string code;
        std::string message;
        std::string suggestion;
        bool raw; // searched in the real line (strings included)
        bool once; // once per file
        std::vector<std::string> keys; // one of them must be in the line before the regex is tried
    };
    static const std::vector<Pattern> patterns = {
        {std::regex(R"(\bthrow\s+std::(runtime_error|logic_error|invalid_argument|out_of_range|domain_error|length_error|overflow_error|underflow_error|range_error|exception)\b)"),
            "LU-EXCEPTION", "Standard exception thrown", "throw utils::exception::ErrorException(utils::exception::InternalCode::X, \"info\") (project codes: libutils-exception skill)", false, false, {"throw"}},
        {std::regex(R"(\bargv\s*\[\s*(?!0\s*\])\w+(\s*[+-]\s*\d+)?\s*\]|\bgetopt(_long)?\s*\()"), "LU-ARGS", "Arguments parsed by hand",
            "utils::arguments::ArgParser (setFlag / setOption / setUsage, parse(argc, argv))", false, true, {"argv", "getopt"}},
        {std::regex(R"(\\033\[|\\x1[bB]\[|\\e\[|\\u001[bB]\[)"), "LU-ANSI", "Raw ANSI escape sequence",
            "utils::iomanip (color(Color::Red), set_style({Style::Bold}), reset(), file_hyperlink...)", true, false, {"\\033", "\\x1", "\\e[", "\\u001"}}, // xstyle: ignore LU-ANSI (keys of the rule)
        {std::regex(R"(\bsocket\s*\(\s*(AF_|PF_)|\bsockaddr_in6?\b)"), "LU-SOCKET", "Raw BSD socket", "utils::network::TCPSocket, utils::network::Server / Client", false, true, {"socket", "sockaddr"}},
        {std::regex(R"((?:^|[^\w.>])(?:::)?(pipe2?|dup2?|fork|epoll_create1?|epoll_wait|shm_open|dlopen|dlsym)\s*\()"), "LU-SYSCALL", "Raw system call",
            "utils::encapsulation (Pipe, Dup, Process, Poll, SharedMemory, SharedObject)", false, false, {"pipe", "dup", "fork", "epoll", "shm_open", "dlopen", "dlsym"}},
        {std::regex(R"(std::vector\s*<\s*std::j?thread\s*>)"), "LU-THREADS", "Hand made thread pool", "utils::pool::Cluster (workers) or utils::system::Scheduler (delayed tasks)", false, false, {"thread"}},
        {std::regex(R"((M_PI|std::numbers::pi(_v<\w+>)?)\s*/\s*180|180(\.0*)?f?\s*/\s*(M_PI|std::numbers::pi))"), "LU-ANGLE", "Degree / radian conversion by hand",
            "utils::math::trigo::deg_to_rad / rad_to_deg", false, false, {"180"}},
        {std::regex(R"(ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789\+/)"), "LU-BASE64", "Hand made base64 alphabet", "utils::smanip::codec::Base64Codec", true, true, {"ABCDEFGHIJ"}},
        {std::regex(R"(\bif\s*\(\s*(this->)?_?verbose\w*\s*\))"), "LU-VERBOSE", "Verbose flag checked by hand", "utils::verbose: set_verbose(level), onBasicVerbose(info), onDebugVerbose(info)", false, true, {"erbose"}},
        {std::regex(R"(\b\w*(levenshtein|Levenshtein|editDistance|edit_distance|damerau)\w*\s*\()"), "LU-DISTANCE", "Hand made string distance", "utils::algorithms::c2dmp::c2dmp", false, true, {"evenshtein", "istance", "damerau"}},
    };
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    std::set<std::string> reported;

    // A file using the ArgParser only prepares its arguments
    if (std::any_of(code.begin(), code.end(), [](const std::string& line) {return line.find("utils::arguments::") != std::string::npos;})) reported.insert("LU-ARGS");

    for (std::size_t i = 0; i < code.size(); ++i) {
        if (file.getInfo()[i].preprocessor) continue;
        for (const Pattern& pattern: patterns) {
            std::smatch match;
            if (pattern.once && reported.contains(pattern.code)) continue;
            const std::string& text = pattern.raw ? lines[i] : code[i];
            if (std::none_of(pattern.keys.begin(), pattern.keys.end(), [&](const std::string& key) {return text.find(key) != std::string::npos;})) continue;
            if (!std::regex_search(pattern.raw ? lines[i] : code[i], match, pattern.regex)) continue;
            // The raw patterns must be inside a string, not in a comment
            if (pattern.raw && !xstyle::is_blank(file.getComments()[i].substr(static_cast<std::size_t>(match.position(0)), 1))) continue;
            reported.insert(pattern.code);
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), pattern.code, pattern.message, pattern.suggestion));
        }
    }
}

/* checks */
_hot void xstyle::check::libutils(const xstyle::SourceFile& file, const xstyle::ProjectInfo& project, xstyle::check::Issues& issues)
{
    if (project.isLibutils) return;
    includes_(file, project, issues);
    names_(file, project, issues);
    reinvent_(file, issues);
}
