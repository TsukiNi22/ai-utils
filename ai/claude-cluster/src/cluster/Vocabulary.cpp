/**************************************************************\
Edition:
##  @date 12/10/2026 by @author Tsukini

File Name:
##  @file Vocabulary.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Vocabulary.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <fstream>
#include <cctype>
#include <map>

/* tools */
_cold static std::size_t distance_(const std::string& a, const std::string& b)
{
    // edits from a to b (insertion, deletion, substitution)
    std::vector<std::size_t> row(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j)
        row[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        std::size_t diagonal = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t above = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[b.size()];
}

_cold static void add_term_(std::vector<cluster::Term>& terms, const std::string& text, const std::vector<std::string>& heard)
{
    // one term per key: a name met twice (a session and its project) is one term, its aliases joined
    const std::string key = cluster::vocabulary_key(text);
    if (key.size() < 3) return;
    auto it = std::find_if(terms.begin(), terms.end(), [&](const cluster::Term& t) {return !t.keys.empty() && t.keys.front() == key;});
    if (it == terms.end()) {
        terms.push_back({text, {key}});
        it = terms.end() - 1;
    }
    for (const std::string& alias: heard) {
        const std::string other = cluster::vocabulary_key(alias);
        if (other.size() >= 2 && std::find(it->keys.begin(), it->keys.end(), other) == it->keys.end()) it->keys.push_back(other);
    }
}

/* vocabulary */
_cold std::string cluster::vocabulary_key(const std::string& text)
{
    // the accented letters of French (UTF-8, 0xC3 xx) to their base letter; anything else but letters and digits dropped
    static const std::map<unsigned char, char> accents = {
        {0xA0, 'a'}, {0xA2, 'a'}, {0xA4, 'a'}, {0xA7, 'c'}, {0xA8, 'e'}, {0xA9, 'e'}, {0xAA, 'e'}, {0xAB, 'e'}, {0xAE, 'i'}, {0xAF, 'i'},
        {0xB4, 'o'}, {0xB6, 'o'}, {0xB9, 'u'}, {0xBB, 'u'}, {0xBC, 'u'}, {0x80, 'a'}, {0x82, 'a'}, {0x87, 'c'}, {0x88, 'e'}, {0x89, 'e'},
        {0x8A, 'e'}, {0x8B, 'e'}, {0x8E, 'i'}, {0x8F, 'i'}, {0x94, 'o'}, {0x99, 'u'}, {0x9B, 'u'}, {0x9C, 'u'}};
    std::string key;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == 0xC3 && i + 1 < text.size()) {
            auto it = accents.find(static_cast<unsigned char>(text[++i]));
            if (it != accents.end()) key += it->second;
        } else if (c < 0x80 && std::isalnum(c)) {
            key += static_cast<char>(std::tolower(c));
        }
    }
    return key;
}

_cold cluster::VoiceWords cluster::voice_words(const std::filesystem::path& path)
{
    cluster::VoiceWords words;
    std::ifstream in(path);
    std::string line;
    std::vector<std::string>* section = nullptr;
    while (std::getline(in, line)) {
        line = cluster::trim(line.substr(0, line.find('#')));
        if (line == "[cancel]") section = &words.cancel;
        else if (line == "[send]") section = &words.send;
        else if (line == "[next]") section = &words.next;
        else if (line == "[back]") section = &words.back;
        else if (line == "[other]") section = &words.other;
        else if (line == "[global]") section = &words.global;
        else if (!line.empty() && line.front() == '[') section = nullptr;
        else if (!line.empty() && section) section->push_back(line);
    }
    // the sections of the questions missing (a file of a previous version): their words of the template
    if (words.next.empty()) words.next = {"suivant", "suivante", "continue", "continuer", "next", "skip"};
    if (words.back.empty()) words.back = {"retour", "reviens", "precedent", "precedente", "en arriere", "back", "previous", "go back"};
    if (words.other.empty()) words.other = {"autre", "ecris", "reponds", "reponse", "other", "write", "answer"};
    if (words.global.empty())
        words.global = {"globale", "la globale", "demande a la globale", "laisse la globale", "laisse la globale repondre", "la globale repond",
            "global", "ask the global", "let the global answer", "global answers"};
    return words;
}

_cold std::vector<cluster::Term> cluster::vocabulary_file(const std::filesystem::path& path)
{
    std::vector<cluster::Term> terms;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        line = cluster::trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;
        const std::size_t equal = line.find('=');
        const std::string text = cluster::trim(line.substr(0, equal));
        std::vector<std::string> heard;
        if (equal != std::string::npos)
            for (const std::string& alias: cluster::split(line.substr(equal + 1), ','))
                if (!cluster::trim(alias).empty()) heard.push_back(cluster::trim(alias));
        add_term_(terms, text, heard);
    }
    return terms;
}

_cold std::vector<cluster::Term> cluster::vocabulary_projects(const std::filesystem::path& machine)
{
    // "| `~/path/name` | Kind | ..." rows of the "## Projects" section
    std::vector<cluster::Term> terms;
    std::ifstream in(machine);
    std::string line;
    bool projects = false;
    while (std::getline(in, line)) {
        if (line.starts_with("## ")) projects = line.starts_with("## Projects");
        if (!projects || !line.starts_with("| `")) continue;
        const std::size_t end = line.find('`', 3);
        if (end == std::string::npos) continue;
        const std::string path = line.substr(3, end - 3);
        add_term_(terms, path.substr(path.find_last_of('/') + 1), {});
    }
    return terms;
}

_cold void cluster::vocabulary_add(std::vector<cluster::Term>& terms, const std::string& name)
{
    add_term_(terms, name, {});
}

_cold std::string cluster::vocabulary_correct(const std::string& text, const std::vector<cluster::Term>& terms)
{
    // words with their punctuation apart ("lib utils," -> "lib" + "utils" + ","), the longest match first
    const std::vector<std::string> words = cluster::split(text, ' ');
    std::string out;
    for (std::size_t i = 0; i < words.size();) {
        const cluster::Term* best = nullptr;
        std::size_t bestSize = 0;
        std::size_t bestDistance = 0;
        for (std::size_t size = std::min<std::size_t>(4, words.size() - i); size >= 1 && !best; --size) {
            std::string joined;
            for (std::size_t k = i; k < i + size; ++k)
                joined += words[k];
            const std::string key = cluster::vocabulary_key(joined);
            if (key.size() < 3) continue;
            // several words close to a term: each end word must be part of it (not a word before / after it)
            const std::size_t ends = std::min(cluster::vocabulary_key(words[i]).size(), cluster::vocabulary_key(words[i + size - 1]).size());
            for (const cluster::Term& term: terms) {
                for (std::size_t a = 0; a < term.keys.size(); ++a) {
                    const std::string& wanted = term.keys[a];
                    // close enough: a long name only (a short one would catch common words)
                    const bool fuzzy = a == 0 && wanted.size() >= (size > 1 ? 6u : 7u);
                    const std::size_t limit = wanted.size() >= 9 ? 2 : 1;
                    const std::size_t d = key == wanted ? 0 : fuzzy && key.size() + limit >= wanted.size() && wanted.size() + limit >= key.size() ? distance_(key, wanted) : limit + 1;
                    if (d > limit || (best && d >= bestDistance) || (size > 1 && d > 0 && d >= ends)) continue;
                    best = &term;
                    bestSize = size;
                    bestDistance = d;
                }
            }
        }
        if (!best) {
            out += (out.empty() ? "" : " ") + words[i];
            ++i;
            continue;
        }
        // the punctuation after the last word kept; one word already written like the term: left as said
        const std::string& last = words[i + bestSize - 1];
        std::size_t tail = last.size();
        while (tail > 0 && std::ispunct(static_cast<unsigned char>(last[tail - 1])) && last[tail - 1] != '-') --tail;
        const bool same = bestSize == 1 && cluster::lower(words[i].substr(0, tail)) == cluster::lower(best->text);
        out += (out.empty() ? "" : " ") + (same ? words[i].substr(0, tail) : best->text) + last.substr(tail);
        i += bestSize;
    }
    return out;
}
