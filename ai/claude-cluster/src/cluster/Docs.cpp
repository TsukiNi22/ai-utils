/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Docs.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Docs.hpp"
#include "cluster/Match.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>

#ifndef CLUSTER_COMMIT
    #define CLUSTER_COMMIT "unknown"
#endif

/* tools */
_cold static const cluster::DocEntry* find_(const std::string& path)
{
    for (std::size_t i = 0; i < cluster::docs_count; ++i)
        if (path == cluster::docs_table[i].path) return &cluster::docs_table[i];
    return nullptr;
}

_cold static std::string text_(const cluster::DocEntry& entry)
{
    return std::string(reinterpret_cast<const char*>(entry.data), entry.size);
}

_cold static std::string fill_(std::string text)
{
    // the version and the commit of THIS binary, in the pages that show them (the generator leaves tokens)
    const std::function<void(const std::string&, const std::string&)> replace = [&text](const std::string& token, const std::string& value) {
        for (std::size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size()))
            text.replace(at, token.size(), value);
    };
    replace("@@CC_VERSION@@", CLUSTER_VERSION);
    replace("@@CC_COMMIT@@", CLUSTER_COMMIT);
    return text;
}

namespace {

struct Hit {
    std::string kind;
    std::string lang;
    std::string title;
    std::string detail;
    std::string url;
    std::string body;
};

_cold const std::vector<Hit>& index_(void)
{
    // search-index.js is "window.DOCS_INDEX={...};": its JSON read once
    static std::vector<Hit> hits;
    static std::once_flag once;
    std::call_once(once, []() {
        const cluster::DocEntry* entry = find_("search-index.js");
        if (entry == nullptr) return;
        const std::string js = text_(*entry);
        const std::size_t start = js.find('{');
        const std::size_t end = js.rfind('}');
        if (start == std::string::npos || end == std::string::npos || end < start) return;
        const cluster::Json data = cluster::Json::parse(js.substr(start, end - start + 1), nullptr, false);
        if (!data.is_object() || !data.contains("entries") || !data["entries"].is_array()) return;
        for (const cluster::Json& e: data["entries"])
            hits.push_back({e.value("k", std::string()), e.value("l", std::string()), e.value("t", std::string()), e.value("d", std::string()), e.value("u", std::string()),
                e.value("x", std::string())});
    });
    return hits;
}

} // namespace

/* functions */
_cold bool cluster::docs_built(void)
{
    return cluster::docs_count > 0 && find_("index.html") != nullptr;
}

_cold std::optional<cluster::DocAsset> cluster::docs_asset(const std::string& path)
{
    std::string name = path;
    while (name.starts_with("/")) name.erase(name.begin());
    if (name.empty()) name = "index.html";
    if (name.find("..") != std::string::npos) return std::nullopt;
    const cluster::DocEntry* entry = find_(name);
    if (entry == nullptr) return std::nullopt;
    const std::string mime = entry->mime;
    const std::string etag = std::string("\"") + CLUSTER_VERSION + "-" + CLUSTER_COMMIT + "\"";
    return cluster::DocAsset{mime.starts_with("text/html") ? fill_(text_(*entry)) : text_(*entry), mime, etag};
}

_cold cluster::Json cluster::docs_search(const std::string& query, const std::string& lang, const std::size_t limit)
{
    // c2dmp-hsm of libutils (cluster::match_words): a typo, a missing / doubled / swapped letter found; the title first, then the description, then the text
    std::vector<std::pair<float, const Hit*>> found;
    for (const Hit& hit: index_()) {
        if (!hit.lang.empty() && hit.lang != lang) continue;
        std::optional<float> best = cluster::match_words(query, hit.title);
        if (const std::optional<float> s = cluster::match_words(query, hit.detail); s && (!best || *s + 3.0F < *best)) best = *s + 3.0F;
        if (!hit.body.empty())
            if (const std::optional<float> s = cluster::match_words(query, hit.body); s && (!best || *s + 6.0F < *best)) best = *s + 6.0F;
        if (best) found.push_back({*best, &hit});
    }
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) {return a.first < b.first;});
    cluster::Json results = cluster::Json::array();
    for (std::size_t i = 0; i < found.size() && i < limit; ++i)
        results.push_back({{"k", found[i].second->kind}, {"t", found[i].second->title}, {"d", found[i].second->detail}, {"u", found[i].second->url}});
    return {{"results", results}};
}

_cold std::string cluster::docs_page(const std::string& screen)
{
    // docs-map.json: the screens of the app and their page ("settings": "settings", "question": "callbacks-mcp#questions")
    const cluster::DocEntry* entry = find_("docs-map.json");
    if (screen.empty() || entry == nullptr) return "index.html";
    const cluster::Json map = cluster::Json::parse(text_(*entry), nullptr, false);
    if (map.is_object() && map.contains(screen) && map[screen].is_string()) {
        const std::string target = map[screen].get<std::string>();
        const std::size_t hash = target.find('#');
        return target.substr(0, hash) + ".html" + (hash == std::string::npos ? "" : target.substr(hash));
    }
    if (screen.find(".html") != std::string::npos && find_(screen.substr(0, screen.find('#'))) != nullptr) return screen; // a page given as it is
    return "index.html";
}

_cold std::vector<std::string> cluster::docs_screens(void)
{
    std::vector<std::string> screens;
    const cluster::DocEntry* entry = find_("docs-map.json");
    if (entry == nullptr) return screens;
    const cluster::Json map = cluster::Json::parse(text_(*entry), nullptr, false);
    if (map.is_object())
        for (const auto &[key, value]: map.items())
            screens.push_back(key);
    return screens;
}

_cold std::filesystem::path cluster::docs_extract(void)
{
    // the pages written once per build: a browser opens them as files (nothing to serve, no certificate, no web access to open)
    const std::filesystem::path base = cluster::data_dir() / "docs";
    const std::filesystem::path dir = base / (std::string(CLUSTER_VERSION) + "-" + CLUSTER_COMMIT);
    std::error_code error;
    if (std::filesystem::exists(dir / ".done", error)) return dir;
    std::filesystem::remove_all(dir, error);
    for (const std::filesystem::directory_entry& old: std::filesystem::directory_iterator(base, error))
        if (old.path() != dir) std::filesystem::remove_all(old.path(), error); // the copies of the other builds
    for (std::size_t i = 0; i < cluster::docs_count; ++i) {
        const cluster::DocEntry& entry = cluster::docs_table[i];
        const std::filesystem::path target = dir / entry.path;
        std::filesystem::create_directories(target.parent_path(), error);
        std::ofstream out(target, std::ios::binary);
        const std::string mime = entry.mime;
        out << (mime.starts_with("text/html") ? fill_(text_(entry)) : text_(entry));
    }
    std::ofstream(dir / ".done") << CLUSTER_VERSION << "\n";
    return dir;
}

_cold cluster::DocOpened cluster::docs_open(const std::string& target, const std::string& web)
{
    // the browser of the desktop on the extracted copy (file://, always works); no display: the address only, told
    cluster::DocOpened out;
    if (!cluster::docs_built()) return out;
    const std::string page = cluster::docs_page(target);
    const std::filesystem::path dir = cluster::docs_extract();
    std::string url = "file://" + (dir / page.substr(0, page.find('#'))).string() + (page.find('#') == std::string::npos ? "" : page.substr(page.find('#')));
    if (!web.empty() && std::getenv("DISPLAY") == nullptr && std::getenv("WAYLAND_DISPLAY") == nullptr) url = web + "/docs/" + page; // asked over a network: its address
    out.url = url;
    if ((std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr) && cluster::has_command("xdg-open")) {
        cluster::detach({"xdg-open", url});
        out.opened = true;
    }
    return out;
}
