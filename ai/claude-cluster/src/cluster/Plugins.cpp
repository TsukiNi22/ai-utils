/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 17/10/2026 by @author Tsukini

File Name:
##  @file Plugins.cpp

File Description:
##  Plugins and skills from sources (a repository link, a folder of this computer, a .zip): a repository cloned and a
##  .zip unpacked in <data>/plugins, a folder used in place; listed (a Claude Code marketplace: its plugins, else its
##  SKILL.md folders); the ones ticked installed (claude plugin install / a link in ~/.claude/skills), turned off /
##  on, the ones unticked that claude-cluster installed removed
\**************************************************************/

#include "cluster/Plugins.hpp"
#include "cluster/Tools.hpp"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <regex>
#include <set>

//----------------------------------------------------------------//
/* TOOLS */

_cold static std::filesystem::path root_(void)
{
    return cluster::data_dir() / "plugins";
}

_cold static std::filesystem::path skills_dir_(void)
{
    return cluster::expand_home("~/.claude/skills");
}

_cold static std::filesystem::path ours_(void)
{
    return root_() / "installed.json";   // what claude-cluster installed (only these are removed when unticked)
}

_cold static std::pair<std::string, std::string> front_matter_(const std::filesystem::path& file)
{
    // name / description of a SKILL.md (its --- block), else none
    std::ifstream in(file);
    std::string line;
    std::string name;
    std::string description;
    int marks = 0;
    while (std::getline(in, line) && marks < 2) {
        if (cluster::trim(line) == "---") {
            ++marks;
            continue;
        }
        if (marks != 1) continue;
        if (line.starts_with("name:")) name = cluster::trim(line.substr(5));
        else if (line.starts_with("description:")) description = cluster::trim(line.substr(12));
    }
    return {name, description};
}

_cold static bool on_(const std::map<std::string, bool>& table, const std::string& key)
{
    const auto it = table.find(key);
    return it != table.end() && it->second;
}

_cold static bool wanted_(const cluster::PluginSetup& setup, const cluster::PluginRepo& repo, const cluster::PluginItem& item)
{
    return on_(setup.select, repo.slug + ":*") || on_(setup.select, item.id); // "<slug>:*": all of a source
}

_cold static std::map<std::string, bool> plugins_installed_(const std::string& claude)
{
    // the plugins Claude Code has ("name@marketplace" = enabled)
    std::map<std::string, bool> ids;
    const cluster::Json list = cluster::Json::parse(cluster::capture({claude, "plugin", "list", "--json"}, "", 60).out, nullptr, false);
    if (list.is_array())
        for (const cluster::Json& p: list)
            if (p.is_object()) ids[p.value("id", std::string())] = p.value("enabled", true);
    return ids;
}

_cold static std::set<std::string> ours_read_(void)
{
    std::set<std::string> ours;
    if (const std::optional<cluster::Json> kept = cluster::read_json(ours_()); kept && kept->is_array())
        for (const cluster::Json& id: *kept)
            if (id.is_string()) ours.insert(id.get<std::string>());
    return ours;
}

_cold static std::filesystem::path place_(cluster::PluginRepo& repo)
{
    // where a source is read: a folder in place, a .zip unpacked (again when it changed), a repository cloned / pulled
    std::error_code error;
    if (repo.kind == "folder") {
        const std::filesystem::path dir = cluster::expand_home(repo.url);
        if (!std::filesystem::is_directory(dir)) repo.error = "no such folder";
        return dir;
    }
    const std::filesystem::path dir = root_() / repo.slug;
    if (repo.kind == "zip") {
        const std::filesystem::path zip = cluster::expand_home(repo.url);
        if (!std::filesystem::is_regular_file(zip)) {
            repo.error = "no such .zip";
            return dir;
        }
        const std::filesystem::path mark = dir / ".claude-cluster-unpacked";
        if (!std::filesystem::exists(mark) || std::filesystem::last_write_time(mark, error) < std::filesystem::last_write_time(zip, error)) {
            std::filesystem::remove_all(dir, error);
            std::filesystem::create_directories(dir, error);
            const cluster::Captured got = cluster::capture({"unzip", "-o", "-q", zip.string(), "-d", dir.string()}, "", 120);
            if (got.code != 0) {
                repo.error = "not unpacked (unzip): " + cluster::one_line(got.out, 160);
                return dir;
            }
            std::ofstream(mark) << zip.string() << "\n";
        }
        // a .zip of one folder: that folder
        std::vector<std::filesystem::path> inside;
        for (const std::filesystem::directory_entry& e: std::filesystem::directory_iterator(dir, error))
            if (e.path().filename() != ".claude-cluster-unpacked" && e.path().filename() != "__MACOSX") inside.push_back(e.path());
        return inside.size() == 1 && std::filesystem::is_directory(inside[0]) ? inside[0] : dir;
    }
    const cluster::Captured got = std::filesystem::exists(dir / ".git")
        ? cluster::capture({"git", "-C", dir.string(), "pull", "--ff-only", "--quiet"}, "", 120)
        : cluster::capture({"git", "clone", "--depth", "1", "--quiet", "--", cluster::plugin_source(repo.url), dir.string()}, "", 300);
    if (got.code != 0) repo.error = std::filesystem::exists(dir / ".git") ? "not updated (the last copy listed): " + cluster::one_line(got.out, 160)
        : "not cloned: " + cluster::one_line(got.out, 200);
    return dir;
}

//----------------------------------------------------------------//
/* FUNCTION */

_cold std::string cluster::plugin_kind(const std::string& source)
{
    const std::string s = cluster::trim(source);
    if (s.ends_with(".zip")) return "zip";
    if (s.starts_with("/") || s.starts_with("~") || s.starts_with("./") || s.starts_with("../")) return "folder";
    return "git";
}

_cold std::string cluster::plugin_slug(const std::string& url)
{
    std::string s = cluster::trim(url);
    while (s.ends_with("/")) s.pop_back();
    if (s.ends_with(".git") || s.ends_with(".zip")) s.resize(s.size() - 4);
    if (cluster::plugin_kind(url) != "git") s = std::filesystem::path(s).filename().string() + "-local";
    s = std::regex_replace(s, std::regex(R"(^(https?://|git@|ssh://)([^/:]+)[/:])"), "");
    std::string out;
    for (const char c: s)
        out += std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' ? c : '-';
    while (out.starts_with("-") || out.starts_with(".")) out.erase(0, 1);
    return out.empty() ? std::string("source") : out;
}

_cold std::string cluster::plugin_source(const std::string& url)
{
    const std::string s = cluster::trim(url);
    if (std::regex_match(s, std::regex(R"([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)"))) return "https://github.com/" + s;
    if (cluster::plugin_kind(s) != "git") return cluster::expand_home(s).string();
    return s;
}

_cold std::vector<cluster::PluginRepo> cluster::plugins_index(void)
{
    std::vector<cluster::PluginRepo> repos;
    const std::optional<cluster::Json> index = cluster::read_json(root_() / "index.json");
    if (!index || !index->is_array()) return repos;
    for (const cluster::Json& r: *index) {
        cluster::PluginRepo repo{r.value("url", std::string()), r.value("slug", std::string()), r.value("kind", std::string("git")), r.value("dir", std::string()),
            r.value("marketplace", std::string()), r.value("error", std::string()), {}};
        for (const cluster::Json& i: r.value("items", cluster::Json::array()))
            repo.items.push_back({i.value("id", std::string()), i.value("name", std::string()), i.value("kind", std::string()), i.value("description", std::string()),
                i.value("path", std::string()), i.value("installed", false), i.value("enabled", true)});
        repos.push_back(repo);
    }
    return repos;
}

_cold std::vector<cluster::PluginRepo> cluster::plugins_scan(const cluster::PluginSetup& setup)
{
    // each source placed (cloned, unpacked, in place), then listed; the index written for the settings and the browser
    std::error_code error;
    std::filesystem::create_directories(root_(), error);
    const std::map<std::string, bool> installed = plugins_installed_(setup.claude);
    const std::set<std::string> ours = ours_read_();
    std::vector<cluster::PluginRepo> repos;
    cluster::Json index = cluster::Json::array();
    for (const std::string& url: setup.repos) {
        if (cluster::trim(url).empty()) continue;
        cluster::PluginRepo repo{cluster::trim(url), cluster::plugin_slug(url), cluster::plugin_kind(url), "", "", "", {}};
        const std::filesystem::path dir = place_(repo);
        repo.dir = dir.string();
        if (std::filesystem::is_directory(dir)) {
            const std::optional<cluster::Json> market = cluster::read_json(dir / ".claude-plugin" / "marketplace.json");
            if (market && market->is_object() && market->contains("plugins")) {
                // a Claude Code marketplace: its plugins
                repo.marketplace = market->value("name", repo.slug);
                for (const cluster::Json& p: market->value("plugins", cluster::Json::array())) {
                    const std::string name = p.value("name", std::string());
                    if (name.empty()) continue;
                    const std::string source = p.contains("source") && p["source"].is_string() ? p["source"].get<std::string>() : std::string("./");
                    const auto it = installed.find(name + "@" + repo.marketplace);
                    repo.items.push_back({repo.slug + ":" + name, name, "plugin", p.value("description", std::string()),
                        (dir / source).lexically_normal().string(), it != installed.end(), it == installed.end() || it->second});
                }
            } else {
                // skills: the folders with a SKILL.md (5 levels, .git skipped)
                for (auto it = std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, error);
                    it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
                    if (error) break;
                    if (it->is_directory() && (it->path().filename() == ".git" || it.depth() > 4)) {
                        it.disable_recursion_pending();
                        continue;
                    }
                    if (it->path().filename() != "SKILL.md") continue;
                    const std::filesystem::path folder = it->path().parent_path();
                    const auto [found, description] = front_matter_(it->path());
                    const std::string name = found.empty() ? folder.filename().string() : found;
                    const std::filesystem::path link = skills_dir_() / name;
                    const bool linked = std::filesystem::is_symlink(link) && std::filesystem::read_symlink(link, error) == folder;
                    const std::string id = repo.slug + ":" + name;
                    const bool off = !linked && ours.contains(id) && on_(setup.off, id); // turned off: unlinked, still ours
                    repo.items.push_back({id, name, "skill", description, folder.string(), linked || off, !off});
                }
                std::sort(repo.items.begin(), repo.items.end(), [](const cluster::PluginItem& a, const cluster::PluginItem& b) {return a.name < b.name;});
                if (repo.items.empty() && repo.error.empty()) repo.error = "no plugin (.claude-plugin/marketplace.json) nor skill (SKILL.md) found";
            }
        }
        cluster::Json items = cluster::Json::array();
        for (const cluster::PluginItem& i: repo.items)
            items.push_back({{"id", i.id}, {"name", i.name}, {"kind", i.kind}, {"description", i.description}, {"path", i.path}, {"installed", i.installed},
                {"enabled", i.enabled}});
        index.push_back({{"url", repo.url}, {"slug", repo.slug}, {"kind", repo.kind}, {"dir", repo.dir}, {"marketplace", repo.marketplace}, {"error", repo.error},
            {"items", items}});
        repos.push_back(repo);
    }
    cluster::write_json(root_() / "index.json", index);
    return repos;
}

_cold std::string cluster::plugins_apply(const cluster::PluginSetup& setup)
{
    // the index against the ticks: installed what is ticked, turned off / on, removed what claude-cluster installed and is unticked
    const std::vector<cluster::PluginRepo> repos = cluster::plugins_index();
    const std::string& claude = setup.claude;
    std::set<std::string> ours = ours_read_();
    std::vector<std::string> done;
    std::vector<std::string> failed;
    std::set<std::string> markets;
    const cluster::Json listed = cluster::Json::parse(cluster::capture({claude, "plugin", "marketplace", "list", "--json"}, "", 60).out, nullptr, false);
    if (listed.is_array())
        for (const cluster::Json& m: listed)
            if (m.is_object()) markets.insert(m.value("name", std::string()));
    std::error_code error;
    for (const cluster::PluginRepo& repo: repos) {
        for (const cluster::PluginItem& item: repo.items) {
            const bool want = wanted_(setup, repo, item);
            const bool off = on_(setup.off, item.id);
            if (!want && (!item.installed || !ours.contains(item.id))) continue; // not wanted, or installed by hand: left as it is
            if (item.kind == "plugin") {
                const std::string id = item.name + "@" + repo.marketplace;
                std::vector<std::string> args;
                if (want && !item.installed) args = {claude, "plugin", "install", id, "--scope", "user"};
                else if (!want) args = {claude, "plugin", "uninstall", id, "--scope", "user"};
                else if (off == item.enabled) args = {claude, "plugin", off ? "disable" : "enable", id};
                if (args.empty()) continue;
                if (want && !markets.contains(repo.marketplace)) {
                    const cluster::Captured added = cluster::capture({claude, "plugin", "marketplace", "add", cluster::plugin_source(repo.url)}, "", 300);
                    if (added.code != 0) {
                        failed.push_back(repo.marketplace + ": " + cluster::one_line(added.out, 160));
                        continue;
                    }
                    markets.insert(repo.marketplace);
                }
                const cluster::Captured r = cluster::capture(args, "", 300);
                if (r.code != 0) {
                    failed.push_back(id + ": " + cluster::one_line(r.out, 160));
                    continue;
                }
                if (want && !item.installed && off) (void)cluster::capture({claude, "plugin", "disable", id}, "", 60);
                done.push_back(std::string(!want ? "-" : !item.installed ? "+" : off ? "off " : "on ") + item.name);
            } else {
                // a skill: linked (installed, on), unlinked (off, or removed)
                const std::filesystem::path link = skills_dir_() / item.name;
                const bool linked = std::filesystem::is_symlink(link) && std::filesystem::read_symlink(link, error).string() == item.path;
                const bool link_it = want && !off;
                if (link_it == linked) {
                    if (!want) { // removed while off: nothing to unlink, no longer installed
                        ours.erase(item.id);
                        done.push_back("-" + item.name);
                    }
                    continue;
                }
                if (link_it) {
                    if (std::filesystem::exists(link) || std::filesystem::is_symlink(link)) {
                        failed.push_back(item.name + ": ~/.claude/skills/" + item.name + " already exists");
                        continue;
                    }
                    std::filesystem::create_directories(skills_dir_(), error);
                    std::filesystem::create_directory_symlink(item.path, link, error);
                } else {
                    std::filesystem::remove(link, error);
                }
                if (error) {
                    failed.push_back(item.name + ": " + error.message());
                    error.clear();
                    continue;
                }
                done.push_back(std::string(!want ? "-" : !item.installed ? "+" : off ? "off " : "on ") + item.name);
            }
            if (want) ours.insert(item.id);
            else ours.erase(item.id);
        }
    }
    cluster::write_json(ours_(), cluster::Json(std::vector<std::string>(ours.begin(), ours.end())));
    if (!done.empty() || !failed.empty()) (void)cluster::plugins_scan(setup); // the marks again
    std::string text;
    for (const std::string& d: done)
        text += (text.empty() ? "" : ", ") + d;
    if (!failed.empty()) {
        text += std::string(text.empty() ? "" : "; ") + "failed:";
        for (const std::string& f: failed)
            text += " " + f + ";";
    }
    return text;
}
