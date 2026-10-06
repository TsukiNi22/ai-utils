/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Git.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Encapsulation
#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/Tools.hpp"
#include "xstyle/Git.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <regex>

/* git */
_cold xstyle::git::Result xstyle::git::run(const std::filesystem::path& dir, const std::vector<std::string>& args)
{
    // stdout captured through a pipe, stderr silenced (the caller reports the failures)
    utils::encapsulation::Pipe pipe;
    utils::encapsulation::Process process;
    std::vector<std::string> argv = {"-C", dir.string()};
    xstyle::git::Result result;
    const int null = ::open("/dev/null", O_WRONLY);

    argv.insert(argv.end(), args.begin(), args.end());
    pipe.trigger();
    process.dup(pipe.getWrite(), STDOUT_FILENO);
    if (null != -1) process.dup(null, STDERR_FILENO);
    (void)process.spawn("git", argv);
    pipe.closeWrite();
    if (null != -1) ::close(null);
    char buffer[4096];
    for (ssize_t n = ::read(pipe.getRead(), buffer, sizeof(buffer)); n > 0; n = ::read(pipe.getRead(), buffer, sizeof(buffer)))
        result.out.append(buffer, static_cast<std::size_t>(n));
    const utils::encapsulation::Status status = process.wait();
    result.code = status.exited ? status.code : -1;
    return result;
}

_cold std::optional<std::filesystem::path> xstyle::git::root(const std::filesystem::path& dir)
{
    const xstyle::git::Result result = xstyle::git::run(dir, {"rev-parse", "--show-toplevel"});
    if (result.code != 0) return std::nullopt;
    return std::filesystem::path(xstyle::trim(result.out.substr(0, result.out.find('\n'))));
}

_cold xstyle::git::ChangedLines xstyle::git::changed_lines(const std::filesystem::path& root, const bool staged, const std::string& ref)
{
    static const std::regex hunk(R"(^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@)");
    xstyle::git::ChangedLines changed;
    std::vector<std::string> args = {"diff", "-U0", "--no-color", "--no-ext-diff", "--no-renames"};
    if (staged) args.push_back("--cached");
    if (!ref.empty()) args.push_back(ref);
    std::istringstream stream(xstyle::git::run(root, args).out);
    std::filesystem::path file;

    for (std::string line; std::getline(stream, line);) {
        std::smatch match;
        if (line.starts_with("+++ ")) {
            file = line == "+++ /dev/null" ? std::filesystem::path() : root / line.substr(6); // +++ b/<path>
            if (!file.empty()) (void)changed[file];
        } else if (!file.empty() && std::regex_search(line, match, hunk)) {
            const std::size_t first = std::stoul(match[1].str());
            const std::size_t count = match[2].matched ? std::stoul(match[2].str()) : 1;
            if (count > 0) changed[file].push_back({first, first + count - 1});
        }
    }
    // New files not added yet: every line is new (working tree only)
    if (!staged) {
        std::istringstream untracked(xstyle::git::run(root, {"ls-files", "--others", "--exclude-standard"}).out);
        for (std::string line; std::getline(untracked, line);)
            if (!line.empty()) changed[root / line] = {{1, static_cast<std::size_t>(-1)}};
    }
    return changed;
}

_cold std::set<std::filesystem::path> xstyle::git::dirty_files(const std::filesystem::path& root)
{
    std::set<std::filesystem::path> files;
    std::istringstream stream(xstyle::git::run(root, {"status", "--porcelain", "--untracked-files=all"}).out);
    for (std::string line; std::getline(stream, line);)
        if (line.size() > 3) files.insert(std::filesystem::weakly_canonical(root / line.substr(3)));
    return files;
}

_cold bool xstyle::git::commit(const std::filesystem::path& root, const std::vector<std::filesystem::path>& files, const std::string& message, const bool all)
{
    // all: every tracked change + the fixed files, else only the fixed files (--only: the rest of the index is untouched)
    std::vector<std::string> add = {"add", "--"};
    for (const std::filesystem::path& file: files)
        add.push_back(file.string());
    if (all && xstyle::git::run(root, {"add", "-u"}).code != 0) return false;
    if (!files.empty() && xstyle::git::run(root, add).code != 0) return false;
    std::vector<std::string> args = {"commit", "-q", "-m", message};
    if (!all) {
        args.insert(args.end(), {"--only", "--"});
        for (const std::filesystem::path& file: files)
            args.push_back(file.string());
    }
    return xstyle::git::run(root, args).code == 0;
}
