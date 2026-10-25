/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 25/10/2026 by @author Tsukini

File Name:
##  @file Profiles.cpp

File Description:
##  Settings profiles: named copies of config.toml in <config>/profiles (index.json, one .toml each, .previous.toml for undo)
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Profiles.hpp"
#include "cluster/Config.hpp"
#include "cluster/Tools.hpp"
#include <sys/stat.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <mutex>

//----------------------------------------------------------------//
/* TOOLS */

namespace {
    std::mutex mutex_; // the index read, changed and written by one action at a time

    std::filesystem::path dir_(void)
    {
        return cluster::config_dir() / "profiles";
    }

    std::filesystem::path config_(void)
    {
        return cluster::config_dir() / "config.toml";
    }

    std::string read_(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return text.str();
    }

    // written whole (tmp + rename: a reader never sees half of it), private like config.toml (keys of the accounts may be in it)
    void write_(const std::filesystem::path& path, const std::string& text)
    {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        const std::filesystem::path tmp = path.string() + ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out << text;
            if (!out) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, "cannot write " + tmp.string());
        }
        ::chmod(tmp.c_str(), 0600);
        std::filesystem::rename(tmp, path, error);
        if (error) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, "cannot write " + path.string() + ": " + error.message());
    }

    std::size_t settings_in_(const std::string& text)
    {
        std::size_t count = 0;
        for (const std::string& line: cluster::split(text, '\n')) {
            const std::string bare = cluster::trim(line);
            count += !bare.empty() && bare[0] != '#' && bare[0] != '[' && bare.find('=') != std::string::npos ? 1 : 0;
        }
        return count;
    }

    // its file: the name made safe (letters, digits, - _), unique
    std::string file_for_(const std::string& name, const std::vector<cluster::SettingsProfile>& all)
    {
        std::string base;
        for (const char c: name)
            base += std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : '-';
        base = base.substr(0, 40);
        if (base.empty() || base.find_first_not_of('-') == std::string::npos) base = "profile";
        std::string file = base;
        for (int n = 2; std::any_of(all.begin(), all.end(), [&file](const cluster::SettingsProfile& p) {return p.file == file;}); ++n)
            file = base + "-" + std::to_string(n);
        return file;
    }

    std::vector<cluster::SettingsProfile> load_(void)
    {
        std::vector<cluster::SettingsProfile> all;
        const std::optional<cluster::Json> index = cluster::read_json(dir_() / "index.json");
        if (!index || !index->is_object()) return all;
        for (const cluster::Json& p: index->value("profiles", cluster::Json::array())) {
            cluster::SettingsProfile it;
            it.name = p.value("name", std::string());
            it.note = p.value("note", std::string());
            it.file = p.value("file", std::string());
            it.created = p.value("created", static_cast<std::int64_t>(0));
            it.updated = p.value("updated", static_cast<std::int64_t>(0));
            it.archived = p.value("archived", static_cast<std::int64_t>(0));
            it.deleted = p.value("deleted", static_cast<std::int64_t>(0));
            if (it.name.empty() || it.file.empty() || it.file.find('/') != std::string::npos || it.file.starts_with(".")) continue;
            it.lines = settings_in_(read_(dir_() / (it.file + ".toml")));
            all.push_back(it);
        }
        return all;
    }

    void store_(const std::vector<cluster::SettingsProfile>& all)
    {
        cluster::Json list = cluster::Json::array();
        for (const cluster::SettingsProfile& p: all)
            list.push_back({{"name", p.name}, {"note", p.note}, {"file", p.file}, {"created", p.created}, {"updated", p.updated}, {"archived", p.archived},
                {"deleted", p.deleted}});
        std::error_code error;
        std::filesystem::create_directories(dir_(), error);
        cluster::write_json(dir_() / "index.json", {{"profiles", list}});
    }

    std::string lower_(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {return static_cast<char>(std::tolower(c));});
        return text;
    }

    std::vector<cluster::SettingsProfile>::iterator find_(std::vector<cluster::SettingsProfile>& all, const std::string& name)
    {
        return std::find_if(all.begin(), all.end(), [&name](const cluster::SettingsProfile& p) {return lower_(p.name) == lower_(cluster::trim(name));});
    }

    // the settings as they are now kept before a profile replaces them (Undo puts them back)
    void keep_previous_(const std::string& reason)
    {
        write_(dir_() / ".previous.toml", read_(config_()));
        cluster::write_json(dir_() / ".previous.json", {{"reason", reason}, {"at", cluster::now()}});
    }

    void apply_text_(const std::string& text, const std::string& what)
    {
        const std::string error = cluster::Config::check(text);
        if (!error.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, what + " is not valid: " + error);
        write_(config_(), text); // every claude-cluster of this computer reads config.toml again by itself (its date changed)
    }
}

//----------------------------------------------------------------//
/* FUNCTION */

_cold void cluster::profiles_purge(void)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<cluster::SettingsProfile> all = load_();
    const std::int64_t limit = cluster::now() - static_cast<std::int64_t>(PROFILES_TRASH_DAYS) * 86400;
    const std::size_t before = all.size();
    std::error_code error;
    for (const cluster::SettingsProfile& p: all)
        if (p.deleted > 0 && p.deleted < limit) std::filesystem::remove(dir_() / (p.file + ".toml"), error);
    std::erase_if(all, [limit](const cluster::SettingsProfile& p) {return p.deleted > 0 && p.deleted < limit;});
    if (all.size() != before) store_(all);
}

_cold std::vector<cluster::SettingsProfile> cluster::profiles_list(void)
{
    cluster::profiles_purge();
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<cluster::SettingsProfile> all = load_();
    // the list first (the last changed first), then the archived ones, then the trash
    std::stable_sort(all.begin(), all.end(), [](const cluster::SettingsProfile& a, const cluster::SettingsProfile& b) {
        const int ra = a.deleted > 0 ? 2 : a.archived > 0 ? 1 : 0;
        const int rb = b.deleted > 0 ? 2 : b.archived > 0 ? 1 : 0;
        return ra != rb ? ra < rb : a.updated > b.updated;
    });
    return all;
}

_cold cluster::Json cluster::profiles_json(void)
{
    cluster::Json list = cluster::Json::array();
    for (const cluster::SettingsProfile& p: cluster::profiles_list())
        list.push_back({{"name", p.name}, {"note", p.note}, {"created", p.created}, {"updated", p.updated}, {"archived", p.archived}, {"deleted", p.deleted},
            {"settings", p.lines}, {"purge_at", p.deleted > 0 ? p.deleted + static_cast<std::int64_t>(PROFILES_TRASH_DAYS) * 86400 : 0}});
    const std::optional<cluster::Json> previous = cluster::read_json(dir_() / ".previous.json");
    cluster::Json undo = cluster::Json::object();
    if (previous && previous->is_object() && std::filesystem::exists(dir_() / ".previous.toml"))
        undo = {{"reason", previous->value("reason", std::string())}, {"at", previous->value("at", static_cast<std::int64_t>(0))}};
    return {{"profiles", list}, {"previous", undo}, {"trash_days", PROFILES_TRASH_DAYS}};
}

_cold std::string cluster::profile_act(const std::string& action, const std::string& name, const std::string& arg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<cluster::SettingsProfile> all = load_();
    const std::string clean = cluster::trim(name);
    if (action == "undo") {
        if (!std::filesystem::exists(dir_() / ".previous.toml"))
            throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "nothing to undo: no profile applied yet");
        const std::string back = read_(dir_() / ".previous.toml");
        keep_previous_("undo"); // undo again: back where it was
        apply_text_(back, "the settings before");
        return "the settings before the last apply are back";
    }
    if (clean.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "a profile needs a name");
    if (clean.size() > 80) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "a name of 80 characters at most");
    const std::vector<cluster::SettingsProfile>::iterator it = find_(all, clean);
    if (action == "save") { // the settings as they are now, in a new profile or over it
        const std::int64_t now = cluster::now();
        if (it == all.end()) {
            cluster::SettingsProfile p;
            p.name = clean;
            p.note = cluster::trim(arg);
            p.file = file_for_(clean, all);
            p.created = now;
            p.updated = now;
            write_(dir_() / (p.file + ".toml"), read_(config_()));
            all.push_back(p);
            store_(all);
            return "profile " + clean + " saved (" + std::to_string(settings_in_(read_(config_()))) + " settings)";
        }
        write_(dir_() / (it->file + ".toml"), read_(config_()));
        it->updated = now;
        it->deleted = 0;
        if (!cluster::trim(arg).empty()) it->note = cluster::trim(arg);
        store_(all);
        return "profile " + it->name + " saved again with the settings of now";
    }
    if (it == all.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no profile named " + clean);
    if (action == "apply") {
        if (it->deleted > 0) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, it->name + " is in the trash: restore it first");
        const std::string text = read_(dir_() / (it->file + ".toml"));
        keep_previous_("before " + it->name);
        apply_text_(text, "profile " + it->name);
        return "profile " + it->name + " applied (every claude-cluster of this computer follows it; Undo puts the settings before back)";
    }
    if (action == "rename") {
        const std::string to = cluster::trim(arg);
        if (to.empty() || to.size() > 80) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "a new name of 1 to 80 characters");
        if (lower_(to) != lower_(it->name) && find_(all, to) != all.end())
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "a profile is already named " + to);
        const std::string old = it->name;
        it->name = to;
        it->updated = cluster::now();
        store_(all);
        return "profile " + old + " renamed " + to;
    }
    if (action == "note") {
        it->note = cluster::trim(arg);
        store_(all);
        return it->note.empty() ? "the note of " + it->name + " removed" : "the note of " + it->name + " written";
    }
    if (action == "archive" || action == "unarchive") {
        it->archived = action == "archive" ? cluster::now() : 0;
        store_(all);
        return it->name + (action == "archive" ? " archived (kept apart, Unarchive brings it back)" : " back in the list");
    }
    if (action == "delete") {
        it->deleted = cluster::now();
        store_(all);
        return it->name + " in the trash (restorable for " + std::to_string(PROFILES_TRASH_DAYS) + " days, then deleted for good)";
    }
    if (action == "restore") {
        it->deleted = 0;
        store_(all);
        return it->name + " restored";
    }
    if (action == "purge") {
        std::error_code error;
        std::filesystem::remove(dir_() / (it->file + ".toml"), error);
        const std::string gone = it->name;
        all.erase(it);
        store_(all);
        return gone + " deleted for good";
    }
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
        "profile: save | apply | undo | rename | note | archive | unarchive | delete | restore | purge, not " + action);
}
