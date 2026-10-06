/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Project.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/Project.hpp"
#include "xstyle/Tools.hpp"
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <regex>
#include <set>

/* tools */
_cold static std::string read_file_(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

_cold static void parse_migrations_(xstyle::ProjectInfo& project)
{
    // using old _migration(...) = new; | _migration(...) inline T old(...) {return new(...);} | _migration(...) inline constexpr T OLD = NEW;
    static const std::regex namespaceOpen(R"(^\s*namespace\s+([\w:]+)\s*\{)");
    static const std::regex alias(R"(using\s+(\w+)\s+_migration\([^)]*\)\s*=\s*([\w:]+))");
    static const std::regex variable(R"(_migration\([^)]*\)\s+inline\s+constexpr\s+[\w:<>]+\s+(\w+)\s*=\s*([\w:]+)\s*;)");
    static const std::regex function(R"(_migration\([^)]*\)[^{=]*?\b(\w+)\s*\([^{]*\{\s*(?:return\s+)?(this->)?([\w:]+))");
    std::set<std::pair<std::string, std::string>> seen;
    const std::filesystem::path root = project.libutilsInclude / "utils";
    std::error_code error;

    for (std::filesystem::recursive_directory_iterator it(root, error), end; it != end && !error; it.increment(error)) {
        if (!it->is_regular_file() || it->path().extension() != ".hpp") continue;
        std::ifstream file(it->path());
        std::string line;
        std::string space;
        while (std::getline(file, line)) {
            std::smatch match;
            if (line.find("namespace") != std::string::npos && std::regex_search(line, match, namespaceOpen)) space = match[1].str();
            if (line.find("_migration(") == std::string::npos || line.find("#define") != std::string::npos) continue;
            xstyle::Migration migration;
            if (std::regex_search(line, match, alias) || std::regex_search(line, match, variable)) {
                migration = {space + "::" + match[1].str(), match[2].str(), false};
            } else if (std::regex_search(line, match, function)) {
                const bool member = match[2].matched;
                const std::string target = match[3].str();
                migration = {member ? match[1].str() : space + "::" + match[1].str(), member ? target.substr(target.rfind(':') == std::string::npos ? 0 : target.rfind(':') + 1) : target, member};
            } else {
                continue;
            }
            if (migration.from == migration.to || !seen.insert({migration.from, migration.to}).second) continue;
            project.migrations.push_back(migration);
        }
    }
}

_cold static void parse_attributes_(xstyle::ProjectInfo& project)
{
    static const std::regex define(R"(^\s*#\s*define\s+(_[a-z]\w*))");
    static const std::vector<std::string> fallback = {"_hot", "_cold", "_nodiscard", "_unused", "_likely", "_unlikely", "_fallthrough", "_noinline", "_deprecated",
        "_noaddress", "_packed", "_assume", "_alignas", "_hidden", "_ctor", "_dtor", "_likely_c", "_unlikely_c", "_expect", "_alloc_size", "_alloc_size_mul",
        "_read_only", "_write_only", "_nonnull", "_legacy", "_migration"};
    std::error_code error;

    if (project.libutilsInstalled) {
        for (std::filesystem::directory_iterator it(project.libutilsInclude / "utils" / "attribute", error), end; it != end && !error; it.increment(error)) {
            std::ifstream file(it->path());
            std::string line;
            while (std::getline(file, line)) {
                std::smatch match;
                if (std::regex_search(line, match, define)) project.attributeMacros.insert(match[1].str());
            }
        }
    }
    if (project.attributeMacros.empty()) project.attributeMacros.insert(fallback.begin(), fallback.end());
}

/* project */
_cold xstyle::ProjectInfo xstyle::detect_project(const std::filesystem::path& start)
{
    static const std::regex version(R"(__LIBUTILS_VERSION__\s+"([^"]+)\")");
    static const std::regex usesUtils(R"(find_package\s*\(\s*utils\b|utils::utils\b)");
    static const std::regex isUtils(R"(set\s*\(\s*TARGET\s+utils\s*\)|project\s*\(\s*utils\b)");
    xstyle::ProjectInfo project;
    std::error_code error;

    // Root: the closest git repository, else the starting directory
    std::filesystem::path dir = std::filesystem::absolute(start, error);
    if (!std::filesystem::is_directory(dir, error)) dir = dir.parent_path();
    project.root = dir;
    for (std::filesystem::path p = dir; !p.empty(); p = p.parent_path()) {
        if (std::filesystem::exists(p / ".git", error)) {
            project.root = p;
            break;
        }
        if (p == p.parent_path()) break;
    }

    // libutils installed (system or CPATH)
    std::vector<std::filesystem::path> candidates = {"/usr/include", "/usr/local/include"};
    if (const char* home = std::getenv("HOME")) candidates.push_back(std::filesystem::path(home) / ".local" / "include");
    if (const char* cpath = std::getenv("CPATH"))
        for (const std::string& p: xstyle::split(cpath, ':'))
            candidates.insert(candidates.begin(), p);
    for (const std::filesystem::path& candidate: candidates) {
        if (!std::filesystem::exists(candidate / "utils" / "utils.hpp", error)) continue;
        project.libutilsInstalled = true;
        project.libutilsInclude = candidate;
        std::smatch match;
        const std::string content = read_file_(candidate / "utils" / "version.hpp");
        if (std::regex_search(content, match, version)) project.libutilsVersion = match[1].str();
        break;
    }

    // libutils used by the project (CMake), or the project is libutils itself
    const std::string cmake = read_file_(project.root / "CMakeLists.txt");
    project.libutilsUsed = std::regex_search(cmake, usesUtils);
    project.isLibutils = std::regex_search(cmake, isUtils);

    parse_attributes_(project);
    if (project.libutilsInstalled) parse_migrations_(project);
    return project;
}

_cold xstyle::Language xstyle::language_of(const std::filesystem::path& path)
{
    static const std::unordered_map<std::string, xstyle::Language> extensions = {
        {".cpp", xstyle::Language::Cpp}, {".cc", xstyle::Language::Cpp}, {".cxx", xstyle::Language::Cpp}, {".hpp", xstyle::Language::Cpp},
        {".hh", xstyle::Language::Cpp}, {".hxx", xstyle::Language::Cpp}, {".ipp", xstyle::Language::Cpp}, {".tpp", xstyle::Language::Cpp},
        {".inl", xstyle::Language::Cpp}, {".h", xstyle::Language::Cpp}, {".c", xstyle::Language::C}, {".py", xstyle::Language::Python},
        {".sh", xstyle::Language::Shell}, {".bash", xstyle::Language::Shell}, {".lua", xstyle::Language::Lua}, {".js", xstyle::Language::JavaScript},
        {".rs", xstyle::Language::Rust}, {".mjs", xstyle::Language::JavaScript}, {".ts", xstyle::Language::JavaScript}, {".tsx", xstyle::Language::JavaScript}, {".jsx", xstyle::Language::JavaScript},
        {".cmake", xstyle::Language::CMake}, {".mk", xstyle::Language::Makefile}, {".yml", xstyle::Language::Yaml}, {".yaml", xstyle::Language::Yaml},
        {".json", xstyle::Language::Json}, {".md", xstyle::Language::Markdown},
    };
    const std::string name = path.filename().string();

    if (name == "CMakeLists.txt") return xstyle::Language::CMake;
    if (name == "Makefile" || name == "makefile" || name == "GNUmakefile") return xstyle::Language::Makefile;
    auto it = extensions.find(xstyle::lower(path.extension().string()));
    return it == extensions.end() ? xstyle::Language::Other : it->second;
}
