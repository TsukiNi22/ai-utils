/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Match.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _C2DMP
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Match.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>

_cold std::optional<float> cluster::match_score(const std::string& query, const std::string& text)
{
    // c2dmp-hsm on the start of the text (the length of the query, +1: a long name not pushed back by its length), the whole
    // text to break the ties; contained: close too; raw: a prefix (0), then contained (1)
    const std::string q = cluster::lower(query);
    const std::string t = cluster::lower(text);
    if (q.empty()) return 0.0F;
    const bool prefix = t.starts_with(q);
    const bool contained = !prefix && t.find(q) != std::string::npos;
    if (cluster::matching_raw()) {
        if (prefix) return static_cast<float>(t.size()) * 0.001F;
        if (contained) return 1.0F + static_cast<float>(t.size()) * 0.001F;
        return std::nullopt;
    }
    const float whole = utils::algorithms::c2dmp::c2dmp<3, std::uint_fast8_t, true>(q, t);
    float score = utils::algorithms::c2dmp::c2dmp<3, std::uint_fast8_t, true>(q, t.substr(0, std::min(t.size(), q.size() + 1))) + whole * 0.1F;
    if (contained) score = std::min(score, whole * 0.1F);
    // close enough: a typo, a missing / doubled / swapped letter (share: shared yes, store no)
    if (prefix || contained || score <= std::max(0.75F, static_cast<float>(q.size()) * 0.25F)) return score;
    return std::nullopt;
}

_cold std::optional<float> cluster::match_words(const std::string& query, const std::string& text)
{
    // each word of the query: its best word of the text (separators: space, punctuation); all found: the sum
    const std::function<std::vector<std::string>(const std::string&)> words = [](const std::string& line) {
        std::vector<std::string> out = {""};
        for (const char c: line) {
            if (std::isalnum(static_cast<unsigned char>(c)) || static_cast<unsigned char>(c) >= 0x80) out.back() += c;
            else if (!out.back().empty()) out.emplace_back();
        }
        if (out.back().empty()) out.pop_back();
        return out;
    };
    const std::vector<std::string> wanted = words(query);
    if (wanted.empty()) return 0.0F;
    const std::vector<std::string> in = words(text);
    float total = 0.0F;
    for (const std::string& w: wanted) {
        std::optional<float> best;
        for (const std::string& word: in)
            if (const std::optional<float> s = cluster::match_score(w, word); s && (!best || *s < *best)) best = s;
        if (!best) return std::nullopt;
        total += *best;
    }
    // the whole query found as it is: closer
    if (wanted.size() > 1 && cluster::lower(text).find(cluster::lower(cluster::trim(query))) != std::string::npos) total -= 2.0F;
    return total;
}
