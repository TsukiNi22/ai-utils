/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file RunProfile.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#include "cluster/RunProfile.hpp"
#include <sys/resource.h>
#include <sys/syscall.h>
#include <algorithm>
#include <unistd.h>
#include <cstdlib>
#include <set>

/* tools */
namespace {

bool digits_(const std::string& text)
{
    return !text.empty() && std::all_of(text.begin(), text.end(), [](const char c) {return c >= '0' && c <= '9';});
}

std::string base_(const std::string& path)
{
    const std::size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// "8" -> the number lowered to the limit; "" when the text is not a number (auto, none...)
std::string lowered_(const std::string& value, const int threads)
{
    if (!digits_(value)) return "";
    const long number = std::strtol(value.c_str(), nullptr, 10);
    return std::to_string(std::min<long>(number, threads));
}

} // namespace

/* parallelism */
std::vector<std::pair<std::string, std::string>> cluster::grant_env(const int threads, const std::vector<std::string>& current)
{
    const std::string n = std::to_string(std::max(1, threads));
    std::vector<std::pair<std::string, std::string>> env = {
        {"CMAKE_BUILD_PARALLEL_LEVEL", n}, {"CARGO_BUILD_JOBS", n}, {"CTEST_PARALLEL_LEVEL", n}, {"OMP_NUM_THREADS", n},
        {"OMP_THREAD_LIMIT", n}, {"OPENBLAS_NUM_THREADS", n}, {"MKL_NUM_THREADS", n}, {"RAYON_NUM_THREADS", n}, {"GOMAXPROCS", n},
        {"PYTEST_XDIST_AUTO_NUM_WORKERS", n}, {"CLUSTER_RUN_THREADS", n}};
    // MAKEFLAGS: a jobserver of a parent make is kept (its -j belongs to it), else the flags of the caller with our -j
    std::string flags;
    for (const std::string& line: current)
        if (line.starts_with("MAKEFLAGS=")) flags = line.substr(10);
    if (flags.find("jobserver") == std::string::npos) {
        std::string kept;
        std::size_t pos = 0;
        while (pos < flags.size()) { // the words of the flags without any -j
            const std::size_t end = flags.find(' ', pos);
            const std::string word = flags.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            pos = end == std::string::npos ? flags.size() : end + 1;
            if (word.empty() || word.starts_with("-j") || word.starts_with("--jobs")) continue;
            kept += (kept.empty() ? "" : " ") + word;
        }
        env.push_back({"MAKEFLAGS", (kept.empty() ? "" : kept + " ") + "-j" + n});
    }
    return env;
}

std::vector<std::string> cluster::grant_argv(const std::vector<std::string>& argv, const int threads)
{
    if (argv.empty() || threads < 1) return argv;
    const std::string tool = base_(argv[0]);
    static const std::set<std::string> jobs = {"make", "gmake", "ninja", "samu", "cmake", "ctest", "cargo", "gradle", "gradlew", "mvn", "scons", "bazel"};
    const bool pytest = tool == "pytest" || tool == "py.test" || (tool.starts_with("python") && argv.size() > 2 && argv[1] == "-m" && argv[2] == "pytest"); // xstyle: ignore LU-ARGS (the words of a command, not our arguments)
    if (!jobs.contains(tool) && !pytest) return argv;
    std::vector<std::string> out = argv;
    bool seen = false;
    bool info = false;      // ninja -t / --version: a tool mode, not a build
    for (std::size_t i = 1; i < out.size(); ++i) {
        std::string& arg = out[i];
        const std::string next = i + 1 < out.size() ? out[i + 1] : "";
        if (tool == "ninja" && (arg == "-t" || arg == "--version" || arg == "-h" || arg == "--help")) info = true;
        if (pytest && arg == "-n") {
            seen = true;
            const std::string value = lowered_(next, threads);
            if (!value.empty() && i + 1 < out.size()) out[++i] = value;
            else if (next == "auto" && i + 1 < out.size()) out[++i] = std::to_string(threads);
            continue;
        }
        if (pytest && arg.starts_with("-n") && digits_(arg.substr(2))) {
            seen = true;
            arg = "-n" + lowered_(arg.substr(2), threads);
            continue;
        }
        if (pytest) continue;
        if (arg.starts_with("-j") && arg.size() > 2 && digits_(arg.substr(2))) {
            seen = true;
            arg = "-j" + lowered_(arg.substr(2), threads);
        } else if (arg == "-j" || arg == "--parallel" || arg == "--jobs") {
            seen = true;
            const std::string value = lowered_(next, threads);
            if (!value.empty()) out[++i] = value;
            else if (arg == "-j" && (next.empty() || next.starts_with("-"))) arg = "-j" + std::to_string(threads); // -j alone: unlimited
        } else if (arg.starts_with("--parallel=") || arg.starts_with("--jobs=") || arg.starts_with("-j=")) {
            seen = true;
            const std::string value = lowered_(arg.substr(arg.find('=') + 1), threads);
            if (!value.empty()) arg = arg.substr(0, arg.find('=') + 1) + value;
        } else if (arg.starts_with("--max-workers=")) {
            seen = true;
            const std::string value = lowered_(arg.substr(14), threads);
            if (!value.empty()) arg = "--max-workers=" + value;
        }
    }
    if (tool == "ninja" && !seen && !info) out.insert(out.begin() + 1, "-j" + std::to_string(threads));
    return out;
}

/* priority */
void cluster::grant_apply(const cluster::Grant& grant)
{
    (void)::setpriority(PRIO_PROCESS, 0, grant.nice);
    (void)::syscall(SYS_ioprio_set, 1 /* IOPRIO_WHO_PROCESS */, 0, (grant.ioClass << 13) | grant.ioLevel);
}

bool cluster::grant_group(const int pgid, const int nice) // xstyle: ignore CPP-SINGLE-STATEMENT (shared with cluster-run)
{
    return pgid > 1 && ::setpriority(PRIO_PGRP, static_cast<id_t>(pgid), nice) == 0;
}

std::string cluster::tail_of(const std::string& text, const std::size_t max)
{
    if (text.size() <= max) return text;
    std::string tail = text.substr(text.size() - max);
    const std::size_t nl = tail.find('\n');
    if (nl != std::string::npos && nl + 1 < tail.size()) tail.erase(0, nl + 1);
    return tail;
}
