/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file Asking.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Vocabulary.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Asking.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <map>

/* tools */
_cold static int number_(const std::string& key)
{
    // "deux", "option 3", "le premier", "2": its number (1..9), 0: not one
    static const std::map<std::string, int> numbers = {{"un", 1}, {"une", 1}, {"premier", 1}, {"premiere", 1}, {"one", 1}, {"first", 1},
        {"deux", 2}, {"second", 2}, {"seconde", 2}, {"deuxieme", 2}, {"two", 2}, {"trois", 3}, {"troisieme", 3}, {"three", 3}, {"third", 3},
        {"quatre", 4}, {"quatrieme", 4}, {"four", 4}, {"fourth", 4}, {"cinq", 5}, {"cinquieme", 5}, {"five", 5}, {"fifth", 5}, {"six", 6},
        {"sixieme", 6}, {"sixth", 6}, {"sept", 7}, {"seven", 7}, {"huit", 8}, {"eight", 8}, {"neuf", 9}, {"nine", 9}};
    std::string rest = key;
    for (const std::string prefix: {"option", "numero", "choix", "reponse", "choisis", "prends", "le", "la", "number", "choice", "the"})
        if (rest.starts_with(prefix) && rest.size() > prefix.size()) rest = rest.substr(prefix.size());
    if (rest.size() == 1 && rest[0] >= '1' && rest[0] <= '9') return rest[0] - '0';
    return numbers.contains(rest) ? numbers.at(rest) : 0;
}

_cold static bool is_(const std::string& key, const std::vector<std::string>& words)
{
    return std::any_of(words.begin(), words.end(), [&key](const std::string& word) {return cluster::vocabulary_key(word) == key;});
}

/* constructor */
_cold cluster::Asking::Asking(cluster::Manager& manager)
    : _manager(manager)
{
}

/* the question */
_cold bool cluster::Asking::check(void)
{
    // the one open answered elsewhere (MCP, the web page, its agent cancelled it): closed; none: the next one waiting
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_ask) {
        const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(this->_ask->session);
        const bool pending = snap && std::any_of(snap->permissions.begin(), snap->permissions.end(),
            [this](const cluster::Permission& p) {return p.requestId == this->_ask->requestId;});
        if (!pending) {
            this->_ask.reset();
            ++this->_version;
        }
        return false;
    }
    for (const cluster::Snapshot& snap: this->_manager.list(true))
        for (const cluster::Permission& p: snap.permissions) {
            if (p.tool != "AskUserQuestion" || this->_offered.contains(p.requestId) || !p.input.contains("questions") || !p.input["questions"].is_array()
                || p.input["questions"].empty()) continue;
            this->_offered.insert(p.requestId);
            cluster::Ask ask;
            ask.session = snap.spec.id;
            ask.name = snap.spec.global ? "global" : snap.spec.name;
            ask.requestId = p.requestId;
            ask.questions = p.input["questions"];
            this->_ask = ask;
            ++this->_version;
            return true;
        }
    return false;
}

_cold std::optional<cluster::Ask> cluster::Asking::current(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_ask;
}

_cold void cluster::Asking::move(const int delta)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    const int count = static_cast<int>(this->_ask->questions[this->_ask->index].value("options", cluster::Json::array()).size());
    this->_ask->cursor = std::clamp(this->_ask->cursor + delta, 0, count);
    ++this->_version;
}

_cold void cluster::Asking::choose(const int option)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    const cluster::Json& q = this->_ask->questions[this->_ask->index];
    const int count = static_cast<int>(q.value("options", cluster::Json::array()).size());
    if (option < 0 || option > count) return;
    this->_ask->cursor = option;
    if (q.value("multiSelect", false) && option < count && !this->_ask->picked.erase(option)) this->_ask->picked.insert(option);
    ++this->_version;
}

_cold void cluster::Asking::toggle(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    if (!this->_ask->picked.erase(this->_ask->cursor)) this->_ask->picked.insert(this->_ask->cursor);
    ++this->_version;
}

_cold void cluster::Asking::type(const std::string& text)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    this->_ask->other += text;
    ++this->_version;
}

_cold void cluster::Asking::erase(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    std::string& other = this->_ask->other;
    while (!other.empty() && (static_cast<unsigned char>(other.back()) & 0xC0) == 0x80) other.pop_back();
    if (!other.empty()) other.pop_back();
    ++this->_version;
}

_cold void cluster::Asking::setOther(const std::string& text)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    const cluster::Json& q = this->_ask->questions[this->_ask->index];
    const int count = static_cast<int>(q.value("options", cluster::Json::array()).size());
    this->_ask->other = text;
    this->_ask->cursor = count;
    if (q.value("multiSelect", false)) this->_ask->picked.insert(count);
    ++this->_version;
}

_cold std::string cluster::Asking::next(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->next_();
}

_cold std::string cluster::Asking::next_(void)
{
    // the answer of the question shown (labels of the options, several joined by ", "; other: the text typed)
    if (!this->_ask) return "no question";
    cluster::Ask& a = *this->_ask;
    const cluster::Json& q = a.questions[a.index];
    const cluster::Json options = q.value("options", cluster::Json::array());
    const int count = static_cast<int>(options.size());
    std::string value;
    const std::function<void(const std::string&)> add = [&value](const std::string& part) {
        if (!cluster::trim(part).empty()) value += (value.empty() ? "" : ", ") + cluster::trim(part);
    };
    if (q.value("multiSelect", false)) {
        for (const int i: a.picked)
            if (i < count) add(options[static_cast<std::size_t>(i)].value("label", std::string()));
        if (a.picked.contains(count) || (a.picked.empty() && a.cursor == count)) add(a.other);
        if (value.empty() && a.cursor < count) add(options[static_cast<std::size_t>(a.cursor)].value("label", std::string()));
    } else {
        add(a.cursor < count ? options[static_cast<std::size_t>(a.cursor)].value("label", std::string()) : a.other);
    }
    if (value.empty()) return "type the answer (other), or choose an option";
    a.answers[q.value("question", std::string())] = value;
    ++this->_version;
    if (++a.index < a.questions.size()) {
        a.cursor = 0;
        a.picked.clear();
        a.other.clear();
        return "";
    }
    const cluster::Ask done = a;
    this->_ask.reset();
    try {
        this->_manager.answerQuestions(done.session, done.requestId, done.answers);
    } catch (const utils::exception::IException& e) {
        return e.info();
    }
    return "";
}

_cold void cluster::Asking::back(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask || this->_ask->index == 0) return;
    cluster::Ask& a = *this->_ask;
    --a.index;
    // its answer back in the choices
    const cluster::Json& q = a.questions[a.index];
    const std::string previous = a.answers.value(q.value("question", std::string()), std::string());
    const cluster::Json options = q.value("options", cluster::Json::array());
    a.cursor = 0;
    a.picked.clear();
    a.other.clear();
    for (std::size_t i = 0; i < options.size(); ++i)
        if (previous == options[i].value("label", std::string()) || previous.find(options[i].value("label", std::string()) + ", ") != std::string::npos) {
            a.cursor = static_cast<int>(i);
            a.picked.insert(static_cast<int>(i));
        }
    ++this->_version;
}

_cold void cluster::Asking::decline(void)
{
    std::optional<cluster::Ask> done;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        done = this->_ask;
        this->_ask.reset();
        ++this->_version;
    }
    if (!done) return;
    try {
        this->_manager.answer(done->session, done->requestId, "deny");
    } catch (const utils::exception::IException&) {}
}

_cold std::string cluster::Asking::toGlobal(void)
{
    // every question of the set given to the global session (it answers with session_allow), the popup closed
    std::optional<cluster::Ask> ask;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (!this->_ask) return "no question";
        if (this->_ask->session == GLOBAL_ID) return "it is a question of the global session itself: answer it";
        ask = this->_ask;
        this->_offered.insert(this->_ask->requestId);
        this->_ask.reset();
        ++this->_version;
    }
    try {
        this->_manager.askGlobal(ask->session, ask->requestId, ask->questions);
    } catch (const utils::exception::IException& e) {
        std::lock_guard<std::mutex> lock(this->_mutex); // not given: open again
        this->_offered.erase(ask->requestId);
        this->_ask = ask;
        ++this->_version;
        return std::string("not given to the global session: ") + e.info();
    }
    return "the global session answers it";
}

_cold std::string cluster::Asking::chat(const std::string& text)
{
    // a side chat about the questions with the model of the session: its answer comes back here, the agent sees none of it
    const std::string message = cluster::trim(text);
    if (message.empty()) return "nothing to send";
    cluster::Ask ask;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (!this->_ask) return "no question";
        if (this->_ask->chatting) return "its answer is still coming";
        this->_ask->chat.emplace_back("user", message);
        this->_ask->chatting = true;
        ++this->_version;
        ask = *this->_ask;
    }
    try {
        this->_manager.chatAbout(ask);
    } catch (const utils::exception::IException& e) {
        this->chatAnswer(ask.requestId, std::string("(no answer: ") + e.info() + ")");
    }
    return "";
}

_cold void cluster::Asking::chatAnswer(const std::string& requestId, const std::string& text)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask || this->_ask->requestId != requestId) return; // answered meanwhile: its chat goes with it
    this->_ask->chat.emplace_back("assistant", text);
    this->_ask->chatting = false;
    ++this->_version;
}

_cold std::string cluster::Asking::spoken(const std::string& sentence)
{
    // a sentence while a question is shown: a word of [send] / [next] (validated), [back], [cancel], [other] <its text>,
    // a number or the label of an option (chosen); anything else: not for it
    const std::optional<cluster::Ask> ask = this->current();
    if (!ask) return "";
    const cluster::VoiceConf& conf = this->_manager.config().voice;
    cluster::VoiceWords words = cluster::voice_words(cluster::voice_words_path(conf.words));
    words.cancel.insert(words.cancel.end(), conf.cancelWords.begin(), conf.cancelWords.end());
    words.send.insert(words.send.end(), conf.sendWords.begin(), conf.sendWords.end());
    const std::string key = cluster::vocabulary_key(sentence);
    if (key.empty()) return "";
    if (is_(key, words.cancel)) {
        this->decline();
        return "question declined";
    }
    if (is_(key, words.send) || is_(key, words.next)) {
        const std::string why = this->next();
        return why.empty() ? (this->current() ? "next question" : "answered") : why;
    }
    if (is_(key, words.back)) {
        this->back();
        return "previous question";
    }
    if (is_(key, words.global)) return this->toGlobal();
    // "<other word> <the answer>"
    const std::string text = cluster::trim(sentence);
    const std::size_t space = text.find(' ');
    if (space != std::string::npos && is_(cluster::vocabulary_key(text.substr(0, space)), words.other)) {
        std::string answer = cluster::trim(text.substr(space + 1));
        while (!answer.empty() && (answer.back() == '.' || answer.back() == '!')) answer.pop_back();
        this->setOther(answer);
        return "other: " + answer;
    }
    const cluster::Json options = ask->questions[ask->index].value("options", cluster::Json::array());
    if (const int n = number_(key); n > 0 && n <= static_cast<int>(options.size()) + 1) {
        this->choose(n - 1);
        return "option " + std::to_string(n);
    }
    for (std::size_t i = 0; i < options.size(); ++i) {
        const std::string label = cluster::vocabulary_key(options[i].value("label", std::string()));
        if (label.size() >= 3 && (key.find(label) != std::string::npos || (key.size() >= 4 && label.find(key) != std::string::npos))) {
            this->choose(static_cast<int>(i));
            return "option " + std::to_string(i + 1) + ": " + options[i].value("label", std::string());
        }
    }
    return "";
}

_cold cluster::Json cluster::Asking::json(void) const
{
    const std::optional<cluster::Ask> ask = this->current();
    if (!ask) return nullptr;
    return {{"session", ask->session}, {"name", ask->name}, {"request_id", ask->requestId}, {"questions", ask->questions}, {"index", ask->index},
        {"cursor", ask->cursor}, {"picked", ask->picked}, {"other", ask->other}, {"answers", ask->answers}, {"chatting", ask->chatting},
        {"chat", [&ask]() {
            cluster::Json chat = cluster::Json::array();
            for (const auto &[role, text]: ask->chat)
                chat.push_back({{"role", role}, {"text", text}});
            return chat;
        }()}};
}
