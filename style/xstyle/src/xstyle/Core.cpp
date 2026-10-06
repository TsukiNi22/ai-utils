/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Core.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Arguments
#define _IOManip
#include <utils/utils.hpp>
#include "xstyle/SourceFile.hpp"
#include "xstyle/Reporter.hpp"
#include "xstyle/Checker.hpp"
#include "xstyle/Project.hpp"
#include "xstyle/Header.hpp"
#include "xstyle/Rules.hpp"
#include "xstyle/Tools.hpp"
#include "xstyle/Core.hpp"
#include "xstyle/Git.hpp"
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <regex>
#include <map>

/* tools */
_cold static std::optional<std::string> codes_hook_(const std::string& value)
{
    static const std::regex code(R"(^(\*|[A-Z][A-Z0-9]*(-[A-Z0-9]+)*-?\*?)$)");
    for (const std::string& part: xstyle::split(value, ','))
        if (!std::regex_match(part, code)) return "'" + part + "' is not a rule code (CPP-NULL), a prefix (CPP, CPP-NAMESPACE) or a pattern (LU-*)";
    if (xstyle::split(value, ',').empty()) return "no code given";
    return std::nullopt;
}

_cold static std::optional<std::string> choice_hook_(const std::string& value, const std::vector<std::string>& choices)
{
    if (std::find(choices.begin(), choices.end(), xstyle::lower(value)) != choices.end()) return std::nullopt;
    std::string list;
    for (const std::string& choice: choices)
        list += (list.empty() ? "" : " | ") + choice;
    return "'" + value + "' is not one of: " + list;
}

_cold static bool binary_file_(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    char buffer[8192];
    file.read(buffer, sizeof(buffer));
    return std::find(buffer, buffer + file.gcount(), '\0') != buffer + file.gcount();
}

/* setup */
_cold void xstyle::Core::setup_(void)
{
    const std::function<std::optional<std::string>(const std::string&)> codes = codes_hook_;
    const std::function<std::optional<std::string>(const std::string&)> severity = [](const std::string& v) -> std::optional<std::string> {
        if (xstyle::parse_severity(v)) return std::nullopt;
        return "'" + v + "' is not a severity: unforgivable | major | minor | negligible";
    };
    const std::function<std::optional<std::string>(const std::string&)> languages = [](const std::string& v) -> std::optional<std::string> {
        for (const std::string& part: xstyle::split(v, ','))
            if (!xstyle::parse_language(part)) return "'" + part + "' is not a language: cpp, c, py, sh, rs, lua, js, cmake, make, yaml, json, md";
        return std::nullopt;
    };

    /* selection */
    this->_parser.setFlag("recursive", {"r", "", "recursive", ""}, {}, "Scan the directories recursively (always done when no path is given)");
    this->_parser.setFlag("code", {"c", "", "code", "XSTYLE_CODES"}, {{"codes", true, codes}}, "Only these rules: codes, prefixes or patterns, comma separated (CPP-NULL,LU,G-*)");
    this->_parser.setFlag("ignore", {"i", "", "ignore", "XSTYLE_IGNORE"}, {{"codes", true, codes}}, "Never these rules (same format as --code)");
    this->_parser.setFlag("severity", {"s", "", "severity", "XSTYLE_SEVERITY"}, {{"level", true, severity}}, "Minimal severity reported: unforgivable | major | minor | negligible (default)");
    this->_parser.setFlag("fail", {"F", "", "fail-on", ""}, {{"level", true, severity}}, "Exit 1 only if an issue of this severity or above is left (default: negligible)");
    this->_parser.setFlag("exclude", {"e", "", "exclude", "XSTYLE_EXCLUDE"}, {{"patterns", true, utils::arguments::defaultTrueParsingHook}},
        "Skip the paths matching these patterns, comma separated (folder name, path prefix or glob: build,third_party,*.gen.hpp)");
    this->_parser.setFlag("lang", {"l", "", "lang", ""}, {{"languages", true, languages}}, "Only these languages, comma separated (cpp,c,py,sh,rs,lua,js,cmake,make,yaml,json,md)");
    this->_parser.setFlag("top", {"t", "", "top", ""}, {}, "Only the most used language of the files found");
    this->_parser.setFlag("mode", {"m", "", "mode", "XSTYLE_MODES"}, {{"levels", true, [](const std::string& v) -> std::optional<std::string> {
        for (const std::string& part: xstyle::split(v, ','))
            if (!xstyle::parse_fix_mode(part)) return "'" + part + "' is not a fix level: auto | force | dangerous | ask | manual";
        return std::nullopt;
    }}}, "Only the issues of these fix levels, comma separated: auto, force, dangerous, ask, manual (--fix included)");
    this->_parser.setFlag("libutils", {"u", "", "libutils", "XSTYLE_LIBUTILS"}, {{"mode", true, [](const std::string& v) {return choice_hook_(v, {"auto", "on", "off"});}}},
        "libutils rules: auto (installed and used by the project, default) | on | off");

    /* fix */
    this->_parser.setFlag("fix", {"f", "", "fix", ""}, {{"codes", false, codes}}, "Fix what can be fixed automatically (only the given codes / prefixes when given)");
    this->_parser.setFlag("diff", {"d", "", "diff", ""}, {}, "Only the lines changed since HEAD (staged + not staged changes, new files included)");
    this->_parser.setFlag("diffref", {"", "", "diff-ref", ""}, {{"ref", true, utils::arguments::defaultTrueParsingHook}},
        "Only the lines changed since this ref (branch, tag, commit: --diff-ref main)");
    this->_parser.setFlag("staged", {"", "", "staged", ""}, {}, "Only the staged lines (git diff --cached): the pre-commit hook mode");
    this->_parser.setFlag("commit", {"", "", "commit", ""}, {}, "With --fix: commit the fixed files (a file that already had changes is left out)");
    this->_parser.setFlag("commitall", {"", "", "commit-all", ""}, {}, "With --fix: commit every tracked change with the fixes");
    this->_parser.setFlag("message", {"", "", "message", ""}, {{"text", true, utils::arguments::defaultTrueParsingHook}},
        "Message of --commit / --commit-all (default: chore(style): apply the xstyle fixes (...))");
    this->_parser.setFlag("dry", {"n", "", "dry-run", ""}, {}, "With --fix: show the changes without writing the files");
    this->_parser.setFlag("force", {"", "", "force", ""}, {}, "With --fix: also the [force-fix] fixes, that can change the behavior (set -euo pipefail, encoding...)");
    this->_parser.setFlag("dangerous", {"", "", "dangerous-force", ""}, {},
        "With --fix: also the [danger-fix] fixes, guessed and possibly wrong (types guessed from the usage, pointer casts...), implies --force");
    this->_parser.setFlag("header", {"", "", "header", "XSTYLE_HEADER"}, {{"banner", true, utils::arguments::defaultTrueParsingHook}},
        "With --fix: banner of the missing file headers: none | default (XARTANIA) | <text> (asked per file when not given)");
    this->_parser.setFlag("headerdesc", {"", "", "header-desc", ""}, {{"text", true, utils::arguments::defaultTrueParsingHook}},
        "With --header: description of the new file headers (default: the default description)");

    /* output */
    this->_parser.setFlag("report", {"o", "", "report", "XSTYLE_REPORT"}, {{"file", true, utils::arguments::defaultTrueParsingHook}}, "Write the report in this file instead of the terminal (.txt, .md or .json)");
    this->_parser.setFlag("format", {"", "", "format", ""}, {{"format", true, [](const std::string& v) {return choice_hook_(v, {"text", "md", "markdown", "json"});}}},
        "Format of the report file: text | md | json (default: from the extension)");
    this->_parser.setFlag("summary", {"S", "", "summary", ""}, {}, "Only the summary (counters by severity and code) in the terminal");
    this->_parser.setFlag("color", {"", "", "no-color", ""}, {}, "No colors nor hyperlinks (also with NO_COLOR or when not in a terminal)");
    this->_parser.setFlag("link", {"", "", "link", "XSTYLE_LINK"}, {{"mode", true, [](const std::string& v) {return choice_hook_(v, {"file", "vscode", "none"});}}},
        "Hyperlinks of the locations: file (file://, default) | vscode (vscode://file/...:line:column) | none");

    /* information */
    this->_parser.setFlag("list", {"L", "", "list-rules", ""}, {}, "List every rule (code, severity, fixable, languages)");
    this->_parser.setFlag("explain", {"x", "", "explain", ""}, {{"code", true, codes}}, "Explain the given rule(s)");
    this->_parser.setFlag("version", {"v", "", "version", ""}, {}, "Version of xstyle");
    this->_parser.setFlag("libcheck", {"", "", "libutils-check", ""}, {}, "Is the libutils skill reference up to date (installed version, libutils repository)? exit 0 yes, 1 no");
    this->_parser.setFlag("completion", {"", "", "completion", ""}, {{"shell", true, [](const std::string& v) {return choice_hook_(v, {"bash", "zsh", "fish"});}}},
        "Print the completion script of the shell (bash | zsh | fish), installed by setup.sh");
    this->_parser.setDefaultUsage();

    // Help in the order of the sections (the default one follows the hash order)
    this->_sections = {
        {"SELECTION", {"recursive", "diff", "diffref", "staged", "code", "ignore", "severity", "mode", "fail", "exclude", "lang", "top", "libutils"}},
        {"FIX", {"fix", "dry", "force", "dangerous", "header", "headerdesc", "commit", "commitall", "message"}},
        {"OUTPUT", {"report", "format", "summary", "color", "link"}},
        {"INFORMATION", {"list", "explain", "version", "libcheck", "completion"}},
    };
    this->_parser.setHelpHook([this](const utils::arguments::ArgParser& parser) {this->help_(parser);});
}

_cold std::vector<std::string> xstyle::Core::extractPaths_(const int argc, char* argv[])
{
    // ArgParser options are positional (one each): the paths are taken out before the parsing
    static const std::unordered_map<std::string, int> values = { // 1 = mandatory value, 2 = optional codes
        {"c", 1}, {"code", 1}, {"i", 1}, {"ignore", 1}, {"s", 1}, {"severity", 1}, {"F", 1}, {"fail-on", 1}, {"e", 1}, {"exclude", 1},
        {"l", 1}, {"lang", 1}, {"m", 1}, {"mode", 1}, {"u", 1}, {"libutils", 1}, {"o", 1}, {"report", 1}, {"format", 1}, {"link", 1}, {"x", 1}, {"explain", 1},
        {"completion", 1},
        {"f", 2}, {"fix", 2}, {"header", 1}, {"header-desc", 1}, {"message", 1}, {"diff-ref", 1},
    };
    std::vector<std::string> arguments = {argc > 0 ? argv[0] : "xstyle"};

    for (int i = 1; i < argc; ++i) {
        const std::string token = argv[i];
        if (token.size() < 2 || token[0] != '-') {
            this->_options.paths.push_back(token);
            continue;
        }
        arguments.push_back(token);
        if (token.find('=') != std::string::npos) continue;

        // --name / -name / -abc (the last short flag takes the value)
        std::string name = token.substr(token.starts_with("--") ? 2 : 1);
        if (!token.starts_with("--") && !values.contains(name)) name = name.substr(name.size() - 1);
        auto it = values.find(name);
        if (it == values.end() || i + 1 >= argc) continue;
        const std::string next = argv[i + 1];
        const bool optionalValue = it->second == 2 && !codes_hook_(next) && !std::filesystem::exists(next);
        if (it->second == 1 || optionalValue) arguments.push_back(argv[++i]);
    }
    return arguments;
}

_cold void xstyle::Core::apply_(const std::string& id, const std::vector<std::string>& values)
{
    const std::string value = values.empty() ? "" : values[0];

    if (id == "recursive") this->_options.recursive = true;
    else if (id == "code") this->_options.codes = xstyle::split(value, ',');
    else if (id == "ignore") this->_options.ignored = xstyle::split(value, ',');
    else if (id == "severity") this->_options.minSeverity = *xstyle::parse_severity(value);
    else if (id == "fail") this->_options.failOn = *xstyle::parse_severity(value);
    else if (id == "exclude") this->_options.excludes = xstyle::split(value, ',');
    else if (id == "top") this->_options.topOnly = true;
    else if (id == "mode")
        for (const std::string& part: xstyle::split(value, ','))
            this->_options.modes.push_back(*xstyle::parse_fix_mode(part));
    else if (id == "libutils") this->_options.libutils = xstyle::lower(value);
    else if (id == "fix") this->_options.fix = true;
    else if (id == "dry") this->_options.dryRun = true;
    else if (id == "force") this->_options.force = true;
    else if (id == "diff") this->_options.changedOnly = true;
    else if (id == "staged") this->_options.changedOnly = this->_options.staged = true;
    else if (id == "commit") this->_options.commit = true;
    else if (id == "commitall") this->_options.commitAll = true;
    else if (id == "message") this->_options.commitMessage = value;
    else if (id == "libcheck") this->_libutilsCheck = true;
    else if (id == "dangerous") this->_options.dangerous = this->_options.force = true;
    else if (id == "header") this->_options.header = value;
    else if (id == "headerdesc") this->_options.headerDescription = value;
    else if (id == "report") this->_options.report = std::filesystem::path(value);
    else if (id == "summary") this->_options.summaryOnly = true;
    else if (id == "color") this->_options.color = false;
    else if (id == "list") this->_listRules = true;
    else if (id == "explain") this->_explain = value;
    else if (id == "version") this->_version = true;
    else if (id == "completion") this->_completion = xstyle::lower(value);

    if (id == "fix" && !value.empty()) this->_options.fixCodes = xstyle::split(value, ',');
    if (id == "diffref") {
        this->_options.changedOnly = true;
        this->_options.diffRef = value;
    }
    if (id == "lang")
        for (const std::string& part: xstyle::split(value, ','))
            this->_options.languages.push_back(*xstyle::parse_language(part));
    if (id == "format") this->_options.format = xstyle::lower(value) == "json" ? xstyle::Format::Json : xstyle::lower(value) == "text" ? xstyle::Format::Text : xstyle::Format::Markdown;
    if (id == "link") this->_options.link = xstyle::lower(value) == "vscode" ? xstyle::LinkMode::Vscode : xstyle::lower(value) == "none" ? xstyle::LinkMode::None : xstyle::LinkMode::File;
}

/* files */
_cold xstyle::Files xstyle::Core::collect_(void) const
{
    static const std::unordered_set<std::string> skipped = {"build", "builds", "node_modules", "__pycache__", "third_party", "thirdparty", "3rdparty", "extern",
        "external", "vendor", "_deps", "dist", "out", "venv", "CMakeFiles"};
    xstyle::Files files;
    std::error_code error;
    const std::filesystem::path cwd = std::filesystem::current_path();

    const std::function<bool(const std::filesystem::path&)> excluded = [&](const std::filesystem::path& path) -> bool {
        const std::string relative = std::filesystem::proximate(path, cwd, error).generic_string();
        for (const std::string& pattern: this->_options.excludes) {
            if (pattern.find_first_of("*?") != std::string::npos) {
                if (xstyle::glob_match(pattern, relative) || xstyle::glob_match(pattern, path.filename().string())) return true;
                continue;
            }
            if (relative.starts_with(pattern)) return true;
            for (const std::filesystem::path& part: std::filesystem::path(relative))
                if (part.string() == pattern) return true;
        }
        return false;
    };
    const std::function<void(const std::filesystem::path&, const bool)> add = [&](const std::filesystem::path& path, const bool explicitFile) {
        xstyle::Language language = xstyle::language_of(path);
        if ((language == xstyle::Language::Other && !explicitFile) || excluded(path)) return;
        if (!explicitFile && path.filename().string().starts_with("generated_")) return; // generated code
        if (std::filesystem::file_size(path, error) > MAX_FILE_SIZE || binary_file_(path)) return;
        if (!this->_options.languages.empty() && std::find(this->_options.languages.begin(), this->_options.languages.end(), language) == this->_options.languages.end()) return;
        files.push_back({std::filesystem::absolute(path), language});
    };

    // --diff / --staged without path: the changed files themselves
    if (this->_options.changedOnly && this->_options.paths.empty()) {
        for (const auto &[path, ranges]: this->_options.changedLines)
            if (std::filesystem::is_regular_file(path, error) && !ranges.empty()) add(path, false);
        std::sort(files.begin(), files.end());
        return files;
    }
    std::vector<std::filesystem::path> paths = this->_options.paths;
    const bool recursive = this->_options.recursive || paths.empty();
    if (paths.empty()) paths.push_back(".");
    for (const std::filesystem::path& path: paths) {
        if (!std::filesystem::exists(path, error)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "No such file or directory: " + path.string());
        if (!std::filesystem::is_directory(path, error)) {
            add(path, true);
            continue;
        }
        if (!recursive) {
            for (std::filesystem::directory_iterator it(path, error), end; it != end && !error; it.increment(error))
                if (it->is_regular_file()) add(it->path(), false);
            continue;
        }
        for (std::filesystem::recursive_directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, error), end; it != end && !error; it.increment(error)) {
            const std::string name = it->path().filename().string();
            if (it->is_directory() && (name.starts_with(".") || skipped.contains(name) || name.starts_with("build") || name.starts_with("cmake-build") || excluded(it->path()))) {
                it.disable_recursion_pending();
                continue;
            }
            if (it->is_regular_file()) add(it->path(), false);
        }
    }

    // A .h is C when the project has C sources and no C++ ones
    const bool hasC = std::any_of(files.begin(), files.end(), [](const std::pair<std::filesystem::path, xstyle::Language>& f) {return f.second == xstyle::Language::C;});
    const bool hasCpp = std::any_of(files.begin(), files.end(), [](const std::pair<std::filesystem::path, xstyle::Language>& f) {return f.second == xstyle::Language::Cpp && f.first.extension() != ".h";});
    if (hasC && !hasCpp)
        for (std::pair<std::filesystem::path, xstyle::Language>& f: files)
            if (f.first.extension() == ".h") f.second = xstyle::Language::C;
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

/* fix */
_cold std::vector<xstyle::AppliedFix> xstyle::Core::header_(const xstyle::Checker& checker, xstyle::SourceFile& source)
{
    // Missing file header: --header, else asked when there is a terminal (skipped otherwise)
    if (!source.isCLike() || !checker.wants("CPP-HEADER")) return {};
    const std::vector<xstyle::Issue> issues = checker.check(source);
    if (std::none_of(issues.begin(), issues.end(), [](const xstyle::Issue& issue) {return issue.code == "CPP-HEADER";})) return {};

    std::string banner;
    std::string description = this->_options.headerDescription;
    if (this->_options.header) {
        banner = *this->_options.header;
    } else if (this->_headerAnswer) {
        banner = this->_headerAnswer->first;
        description = this->_headerAnswer->second;
    } else {
        if (!::isatty(::fileno(stdin))) return {};
        std::string answer;
        std::cerr << source.getDisplay() << ": no file header. Banner: [n]one, [d]efault (XARTANIA), [t]ext, [s]kip"
            << " (upper case: same answer for the next files) > " << std::flush;
        if (!std::getline(std::cin, answer) || answer.empty()) return {};
        const char choice = answer[0];
        const char lowerChoice = static_cast<char>(std::tolower(static_cast<unsigned char>(choice)));
        if (lowerChoice == 's') {
            if (choice == 'S') this->_headerAnswer = {"", ""};
            return {};
        }
        banner = lowerChoice == 'n' ? "none" : "default";
        if (lowerChoice == 't') {
            std::cerr << "Banner text > " << std::flush;
            if (!std::getline(std::cin, banner) || xstyle::trim(banner).empty()) banner = "default";
        }
        std::cerr << "Description (empty: the default one) > " << std::flush;
        (void)std::getline(std::cin, description);
        if (std::isupper(static_cast<unsigned char>(choice))) this->_headerAnswer = {banner, description};
    }
    if (banner.empty()) return {}; // skip for every file
    if (!xstyle::banner_available() && xstyle::lower(banner) != "none") {
        std::cerr << "xstyle: built without the banner font, header written without banner" << std::endl;
        banner = "none";
    }

    std::vector<std::string> header = xstyle::make_header(source.getPath().filename().string(), banner, description);
    header.push_back("");
    source.apply({xstyle::Fix{xstyle::FixKind::Replace, 0, 0, header, false}});
    return {xstyle::AppliedFix{"CPP-HEADER", 1, {}, header}};
}

/* git */
_cold void xstyle::Core::commit_(const std::filesystem::path& repository, const std::vector<std::filesystem::path>& fixed,
    const std::map<std::filesystem::path, std::map<std::string, std::size_t>>& fixedCodes, const std::set<std::filesystem::path>& dirty) const
{
    // --commit: only the files that had no change before the fixes (their own changes would be mixed in the commit)
    std::vector<std::filesystem::path> files;
    std::error_code error;
    for (const std::filesystem::path& file: fixed) {
        if (dirty.contains(std::filesystem::weakly_canonical(file, error))) {
            std::cerr << "xstyle: " << std::filesystem::proximate(file, error).generic_string() << " had changes before the fixes: not committed (--commit-all to include them)" << std::endl;
            continue;
        }
        files.push_back(file);
    }
    if (files.empty() && !this->_options.commitAll) {
        std::cerr << "xstyle: nothing to commit" << std::endl;
        return;
    }

    // chore(style): apply the xstyle fixes (N issues), one line per rule in the body (committed files only)
    std::map<std::string, std::size_t> codes;
    for (const std::filesystem::path& file: files)
        if (fixedCodes.contains(file))
            for (const auto &[code, count]: fixedCodes.at(file))
                codes[code] += count;
    std::size_t total = 0;
    std::string body;
    for (const auto &[code, count]: codes) {
        total += count;
        body += "- " + code + ": " + std::to_string(count) + "\n";
    }
    std::string message = this->_options.commitMessage;
    if (message.empty()) message = "chore(style): apply the xstyle fixes (" + std::to_string(total) + " issue" + (total > 1 ? "s" : "") + ")\n\n" + body;
    if (!xstyle::git::commit(repository, files, message, this->_options.commitAll))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "git commit failed (nothing to commit, hook or identity?)");
    const std::string hash = xstyle::trim(xstyle::git::run(repository, {"log", "-1", "--format=%h %s"}).out);
    if (!this->_options.report) std::cout << "Committed: " << hash << std::endl;
}

_cold void xstyle::Core::libutilsCheck_(void)
{
    // Reference of the libutils skill (VERSION.md) vs the installed headers and the libutils repository
    static const std::regex version(R"(Version \(`CMakeLists.txt`\) \| `([^`]+)`)");
    static const std::regex hash(R"(\| Commit \| `([0-9a-f]+)`)");
    const std::filesystem::path reference = std::filesystem::path(XSTYLE_SKILLS_DIR) / "libutils" / "libutils" / "reference" / "VERSION.md";
    std::ifstream file(reference);
    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::smatch match;
    const std::string referenceVersion = std::regex_search(content, match, version) ? match[1].str() : "";
    const std::string referenceHash = std::regex_search(content, match, hash) ? match[1].str() : "";
    if (referenceVersion.empty() || referenceHash.empty()) {
        std::cout << "status: unknown (no reference at " << reference.string() << ")" << std::endl;
        this->_exit = KO;
        return;
    }

    bool upToDate = true;
    const xstyle::ProjectInfo project = xstyle::detect_project(std::filesystem::current_path());
    std::cout << "reference: " << referenceVersion << " (" << referenceHash.substr(0, 7) << ")" << std::endl;
    if (project.libutilsInstalled) {
        std::cout << "installed: " << project.libutilsVersion << (project.libutilsVersion == referenceVersion ? "" : " (differs from the reference)") << std::endl;
        upToDate = upToDate && project.libutilsVersion == referenceVersion;
    } else {
        std::cout << "installed: none" << std::endl;
    }
    const char* env = std::getenv("LIBUTILS");
    const char* home = std::getenv("HOME");
    const std::filesystem::path repository = env ? env : std::filesystem::path(home ? home : "") / "personal_delivery" / "cpp" / "libutils";
    if (xstyle::git::root(repository)) {
        const std::string ahead = xstyle::trim(xstyle::git::run(repository, {"rev-list", "--count", referenceHash + "..HEAD", "--", "include", "src", "CHANGELOG.md"}).out);
        std::cout << "repository: " << repository.string() << ", " << (ahead.empty() ? "?" : ahead) << " commit(s) after the reference" << std::endl;
        upToDate = upToDate && ahead == "0";
    }
    std::cout << "status: " << (upToDate ? "up to date" : "outdated (bash " + (std::filesystem::path(XSTYLE_SKILLS_DIR) / "libutils" / "libutils" / "scripts" / "update.sh").string()
        + " to regenerate the reference)") << std::endl;
    this->_exit = upToDate ? OK : 1;
}

/* information */
_cold void xstyle::Core::help_(const utils::arguments::ArgParser& parser) const
{
    const bool tty = ::isatty(::fileno(stdout)) && !std::getenv("NO_COLOR");
    const std::string bold = tty ? utils::iomanip::strong() : "";
    const std::string green = tty ? utils::iomanip::color(utils::iomanip::Color::Green) : "";
    const std::string grey = tty ? utils::iomanip::color(utils::iomanip::Color::BrightBlack) : "";
    const std::string reset = tty ? utils::iomanip::reset() : "";
    const std::unordered_map<std::string, utils::arguments::Flag>& flags = parser.getFlags();

    std::cout << bold << "xstyle " << XSTYLE_VERSION << reset << " - " << parser.getDescription() << "\n\n";
    std::cout << bold << "USAGE" << reset << "\n    xstyle [paths...] [flags]\n";
    for (const auto &[title, ids]: this->_sections) {
        std::cout << "\n" << bold << title << reset << "\n";
        for (const std::string& id: ids) {
            const utils::arguments::Flag& flag = flags.at(id);
            const auto &[shortName, flagName, longName, env] = flag.flag;
            std::string usage = shortName.empty() ? "    " : "-" + shortName + ", ";
            usage += "--" + longName;
            for (const auto &[name, mandatory, check]: flag.options)
                usage += mandatory ? " <" + name + ">" : " [" + name + "]";
            std::cout << "    " << green << usage << reset << std::string(usage.size() < 28 ? 28 - usage.size() : 1, ' ') << flag.description
                << (env.empty() ? "" : grey + " (env " + env + ")" + reset) << "\n";
        }
    }
    std::cout << "\n" << bold << "EXAMPLES" << reset << "\n"
        << "    xstyle                                 check the current directory (recursive)\n"
        << "    xstyle -r src include -S               only the counters by severity / code\n"
        << "    xstyle --fix                           fix everything that can be fixed\n"
        << "    xstyle --fix CPP-NULL,G-TRAILING src   fix only these codes, in src\n"
        << "    xstyle -f -n -c CPP -r src/core        preview the C++ fixes of one directory\n"
        << "    xstyle -s major -o report.md           major issues and above, Markdown report (terminal silent)\n"
        << "    xstyle -x CPP-THIS                     explain a rule (-L: every rule)\n";
    std::cout << "\n" << bold << "SUPPRESSION" << reset << " (in a comment)\n"
        << "    xstyle: ignore [CODE,...]              this line\n"
        << "    xstyle: ignore-next [CODE,...]         the next line\n"
        << "    xstyle: ignore-file [CODE,...]         the whole file\n";
    std::cout << "\n" << bold << "SEVERITY" << reset << "  unforgivable > major > minor > negligible\n";
    std::cout << bold << "EXIT" << reset << "      0 clean, 1 issues left (>= --fail-on), 255 error\n";
}

_cold void xstyle::Core::listRules_(void) const
{
    std::cout << std::left << std::setw(24) << "Code" << std::setw(14) << "Severity" << std::setw(7) << "Fix" << std::setw(26) << "Languages" << "Description" << "\n";
    for (const xstyle::Rule& rule: xstyle::rules())
        std::cout << std::left << std::setw(24) << rule.code << std::setw(14) << xstyle::severity_name(rule.severity) << std::setw(7) << xstyle::fix_mode_name(rule.fix)
            << std::setw(26) << rule.languages << rule.description << "\n";
}

_cold void xstyle::Core::explain_(void) const
{
    bool found = false;
    const std::vector<std::string> patterns = xstyle::split(this->_explain, ',');
    for (const xstyle::Rule& rule: xstyle::rules()) {
        if (!std::any_of(patterns.begin(), patterns.end(), [&](const std::string& p) {return xstyle::match_code(rule.code, p);})) continue;
        found = true;
        std::cout << rule.code << " (" << xstyle::severity_name(rule.severity) << (rule.fix == xstyle::FixMode::Auto ? ", fixed by --fix" : rule.fix == xstyle::FixMode::Force ? ", fixed by --fix --force (can change the behavior)"
            : rule.fix == xstyle::FixMode::Ask ? ", fixed by --fix after a question (or --header)" : ", not fixable") << ", languages: " << rule.languages << ")\n"
            << "    " << rule.description << "\n";
    }
    if (!found) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "Unknown rule: " + this->_explain + " (see --list-rules)");
}

/* main endpoint */
_cold void xstyle::Core::init(const int argc, char* argv[])
{
    this->setup_();
    const std::vector<std::string> arguments = this->extractPaths_(argc, argv);
    const utils::arguments::ParsedUsages usages = this->_parser.parse(arguments);
    if (usages.empty()) return;
    for (const auto &[id, option, values]: usages[0].arguments)
        this->apply_(id, values);
}

_cold void xstyle::Core::run(void)
{
    if (!this->_completion.empty()) return this->completion_();
    if (this->_version) {
        std::cout << "xstyle " << XSTYLE_VERSION << std::endl;
        return;
    }
    if (this->_listRules) return this->listRules_();
    if (!this->_explain.empty()) return this->explain_();

    // The report must be writable before anything is fixed
    if (this->_options.report) {
        std::ofstream report(*this->_options.report, std::ios::app);
        if (!report.is_open()) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, "Can't write the report " + this->_options.report->string());
    }

    // Project, git (changed lines, files already changed before the fixes) & files
    if (this->_libutilsCheck) return this->libutilsCheck_();
    xstyle::ProjectInfo project = xstyle::detect_project(this->_options.paths.empty() ? std::filesystem::current_path() : this->_options.paths[0]);
    const std::optional<std::filesystem::path> repository = xstyle::git::root(project.root);
    const bool commit = this->_options.fix && !this->_options.dryRun && (this->_options.commit || this->_options.commitAll);
    if ((this->_options.changedOnly || commit) && !repository)
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "--diff / --staged / --commit need a git repository");
    if (this->_options.changedOnly) {
        std::error_code error;
        for (const auto &[path, ranges]: xstyle::git::changed_lines(*repository, this->_options.staged, this->_options.staged ? "" : this->_options.diffRef))
            this->_options.changedLines[std::filesystem::weakly_canonical(path, error)] = ranges;
    }
    const std::set<std::filesystem::path> dirty = commit && !this->_options.commitAll ? xstyle::git::dirty_files(*repository) : std::set<std::filesystem::path>();
    xstyle::Files files = this->collect_();
    if (!project.libutilsUsed) {
        static const std::regex include(R"(#\s*include\s*[<"](utils/)?utils\.hpp[>"])");
        for (const auto &[path, language]: files) {
            if (language != xstyle::Language::Cpp) continue;
            std::ifstream file(path);
            for (std::string line; std::getline(file, line) && !project.libutilsUsed;)
                if (std::regex_search(line, include)) project.libutilsUsed = true;
        }
    }

    // Only the most used language
    if (this->_options.topOnly) {
        std::map<xstyle::Language, std::size_t> counts;
        for (const auto &[path, language]: files)
            if (language != xstyle::Language::Markdown && language != xstyle::Language::Json && language != xstyle::Language::Yaml) ++counts[language];
        xstyle::Language top = xstyle::Language::Other;
        std::size_t best = 0;
        for (const auto &[language, count]: counts)
            if (count > best) {top = language; best = count;}
        std::erase_if(files, [&](const std::pair<std::filesystem::path, xstyle::Language>& f) {return f.second != top;});
    }

    // Check (and fix) every file
    const xstyle::Checker checker(this->_options, project);
    xstyle::Reporter reporter(this->_options, project, checker.libutils());
    const std::filesystem::path cwd = std::filesystem::current_path();
    std::vector<std::filesystem::path> fixed; // files written by the fixes
    std::map<std::filesystem::path, std::map<std::string, std::size_t>> fixedCodes; // file -> code -> fixes applied
    for (const auto &[path, language]: files) {
        std::error_code error;
        std::string display = std::filesystem::proximate(path, cwd, error).generic_string();
        if (error || display.starts_with("../../")) display = path.generic_string();
        try {
            xstyle::SourceFile source(path, display, language);
            reporter.addFile(source);
            if (this->_options.fix) {
                std::vector<xstyle::AppliedFix> applied = this->header_(checker, source);
                const std::vector<xstyle::AppliedFix> loop = checker.fix(source);
                applied.insert(applied.end(), loop.begin(), loop.end());
                if (!applied.empty() && !this->_options.dryRun) {
                    source.save();
                    fixed.push_back(path);
                    for (const xstyle::AppliedFix& fix: applied)
                        ++fixedCodes[path][fix.code];
                }
                reporter.addFixes(display, applied);
            }
            reporter.addIssues(checker.check(source));
        } catch (const utils::exception::IException& e) {
            std::cerr << "xstyle: " << display << ": " << e.info() << std::endl;
        }
    }

    // With a report file nothing goes to the terminal (only the errors on stderr and the exit status)
    if (!this->_options.report) reporter.print(std::cout);
    reporter.write();
    this->_exit = reporter.exitCode();
    if (commit) this->commit_(*repository, fixed, fixedCodes, dirty);
}
