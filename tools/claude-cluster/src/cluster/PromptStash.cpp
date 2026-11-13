/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file PromptStash.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/PromptStash.hpp"
#include "cluster/Tools.hpp"
#include <filesystem>
#include <algorithm>
#include <mutex>

static std::mutex stash_mutex_;

_cold static std::filesystem::path stash_path_(void)
{
    return cluster::data_dir() / "stash.json";
}

_cold static std::size_t bytes_(const cluster::Draft& draft)
{
    std::size_t total = draft.text.size();
    for (const std::string& paste: draft.pastes)
        total += paste.size();
    for (const std::string& image: draft.images)
        total += image.size();
    return total;
}

_cold static cluster::Json to_json_(const cluster::StashEntry& entry)
{
    return cluster::Json::object({{"text", entry.draft.text}, {"images", entry.draft.images}, {"pastes", entry.draft.pastes}, {"time", entry.time}, {"session", entry.session}});
}

_cold static std::vector<cluster::StashEntry> read_(void)
{
    std::vector<cluster::StashEntry> list;
    const std::optional<cluster::Json> json = cluster::read_json(stash_path_());
    if (!json || !json->is_array()) return list;
    for (const cluster::Json& item: *json) {
        cluster::StashEntry entry; // a text (the first stash) or an object
        if (item.is_string()) {
            entry.draft.text = item.get<std::string>();
        } else if (item.is_object()) {
            entry.draft = {item.value("text", std::string()), item.value("images", std::vector<std::string>()), item.value("pastes", std::vector<std::string>())};
            entry.time = item.value("time", static_cast<std::int64_t>(0));
            entry.session = item.value("session", std::string());
        }
        if (!entry.draft.text.empty()) list.push_back(entry);
    }
    return list;
}

_cold static void write_(const std::vector<cluster::StashEntry>& list)
{
    std::error_code error;
    std::filesystem::create_directories(cluster::data_dir(), error);
    cluster::Json json = cluster::Json::array();
    for (const cluster::StashEntry& entry: list)
        json.push_back(to_json_(entry));
    cluster::write_json(stash_path_(), json); // atomic: tmp + rename
}

_cold std::vector<cluster::StashEntry> cluster::stash_read(void)
{
    std::lock_guard<std::mutex> lock(stash_mutex_);
    return read_();
}

_cold cluster::StashAdd cluster::stash_add(const cluster::Draft& draft, const std::string& session, const int max, const bool dropOldest, const bool force)
{
    if (cluster::trim(draft.text).empty()) return cluster::StashAdd::Empty;
    if (bytes_(draft) > cluster::STASH_ENTRY_BYTES) return cluster::StashAdd::TooBig;
    std::lock_guard<std::mutex> lock(stash_mutex_);
    std::vector<cluster::StashEntry> list = read_();
    const std::size_t limit = static_cast<std::size_t>(std::max(1, max));
    if (!force && list.size() >= limit) {
        if (!dropOldest) return cluster::StashAdd::Full;
        list.erase(list.begin(), list.begin() + static_cast<std::ptrdiff_t>(list.size() - limit + 1));
    }
    list.push_back({draft, cluster::now(), session});
    std::size_t total = 0;
    for (const cluster::StashEntry& entry: list)
        total += bytes_(entry.draft);
    if (total > cluster::STASH_TOTAL_BYTES) return cluster::StashAdd::TooBig;
    write_(list);
    return cluster::StashAdd::Added;
}

_cold std::optional<cluster::StashEntry> cluster::stash_remove(const std::size_t index)
{
    std::lock_guard<std::mutex> lock(stash_mutex_);
    std::vector<cluster::StashEntry> list = read_();
    if (index >= list.size()) return std::nullopt;
    const cluster::StashEntry entry = list[index];
    list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
    write_(list);
    return entry;
}

_cold void cluster::stash_insert(const std::size_t index, const cluster::StashEntry& entry)
{
    std::lock_guard<std::mutex> lock(stash_mutex_);
    std::vector<cluster::StashEntry> list = read_();
    list.insert(list.begin() + static_cast<std::ptrdiff_t>(std::min(index, list.size())), entry);
    write_(list);
}

_cold void cluster::stash_clear(void)
{
    std::lock_guard<std::mutex> lock(stash_mutex_);
    write_({});
}

_cold std::string cluster::stash_label(const cluster::Draft& draft)
{
    const std::string text = cluster::trim(draft.text);
    return cluster::one_line(text.substr(0, text.find('\n')), 200);
}
