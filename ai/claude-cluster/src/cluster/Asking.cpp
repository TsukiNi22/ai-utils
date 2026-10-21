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
#include <chrono>
#include <thread>
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

_cold static void restore_(cluster::Ask& a)
{
    // the question shown, as it was answered (the option marked, the ticks, the Other text), else the first option; the cursor
    // moved while browsing is never kept
    a.touched = false;
    a.cursor = 0;
    a.picked.clear();
    a.other.clear();
    const auto kept = a.given.find(a.index);
    if (kept != a.given.end()) {
        a.cursor = kept->second.cursor;
        a.picked = kept->second.picked;
        a.other = kept->second.other;
        return;
    }
    const cluster::Json& q = a.questions[a.index];
    const std::string previous = a.answers.value(q.value("question", std::string()), std::string());
    if (previous.empty()) return;
    // answered by another way (the voice, the page): from its text
    const cluster::Json options = q.value("options", cluster::Json::array());
    const bool multi = q.value("multiSelect", false);
    bool found = false;
    for (std::size_t i = 0; i < options.size(); ++i) {
        const std::string label = options[i].value("label", std::string());
        if (multi ? previous == label || previous.find(label + ", ") != std::string::npos || previous.ends_with(", " + label) : previous == label) {
            if (!found) a.cursor = static_cast<int>(i);
            if (multi) a.picked.insert(static_cast<int>(i));
            found = true;
        }
    }
    if (!found) { // a typed answer: on Other
        a.cursor = static_cast<int>(options.size());
        a.other = previous;
    }
}

_cold static void record_(cluster::Ask& a)
{
    // a multi choice: the answer is the ticked options (and the Other text), recorded at each tick; none ticked: no answer
    const cluster::Json& q = a.questions[a.index];
    const cluster::Json options = q.value("options", cluster::Json::array());
    const int count = static_cast<int>(options.size());
    std::string value;
    for (const int i: a.picked) {
        const std::string part = cluster::trim(i < count ? options[static_cast<std::size_t>(i)].value("label", std::string()) : i == count ? a.other : std::string());
        if (!part.empty()) value += (value.empty() ? "" : ", ") + part;
    }
    const std::string question = q.value("question", std::string());
    if (value.empty()) {
        a.answers.erase(question);
        a.given.erase(a.index);
        return;
    }
    a.answers[question] = value;
    a.given[a.index] = {a.cursor, a.picked, a.other};
}

/* constructor */
_cold cluster::Asking::Asking(cluster::Manager& manager)
    : _manager(manager)
{
}

_cold cluster::Asking::~Asking()
{
    this->flush_(); // the worker is done with this
}

/* the question */
_cold bool cluster::Asking::check(void)
{
    if (this->_manager.replica()) {
        // a replica: the question of its core (true: a new one)
        const cluster::Json json = this->_manager.replicaState("asking");
        const std::string id = json.is_object() ? json.value("request_id", std::string()) : "";
        std::lock_guard<std::mutex> lock(this->_mutex);
        const bool fresh = !id.empty() && !this->_offered.contains(id);
        if (!id.empty()) this->_offered.insert(id);
        return fresh;
    }

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
    if (this->_manager.replica()) { // an interface of the core: the question of its core (its popup shown here too)
        const cluster::Json j = this->_manager.replicaState("asking");
        if (!j.is_object() || j.value("request_id", std::string()).empty() || !j.contains("questions") || !j["questions"].is_array() || j["questions"].empty())
            return std::nullopt;
        cluster::Ask ask;
        ask.session = j.value("session", std::string());
        ask.name = j.value("name", std::string());
        ask.requestId = j.value("request_id", std::string());
        ask.questions = j["questions"];
        ask.index = std::min(j.value("index", std::size_t{0}), ask.questions.size() - 1);
        ask.cursor = j.value("cursor", 0);
        ask.picked = j.value("picked", std::set<int>());
        ask.other = j.value("other", std::string());
        ask.answers = j.value("answers", cluster::Json::object());
        ask.chatting = j.value("chatting", false);
        ask.review = j.value("review", false);
        for (const cluster::Json& m: j.value("chat", cluster::Json::array()))
            if (m.is_object()) ask.chat.emplace_back(m.value("role", std::string()), m.value("text", std::string()));
        std::lock_guard<std::mutex> lock(this->_localMutex);
        if (this->_local && this->_local->requestId == ask.requestId && this->_local->index == ask.index && cluster::now_ms() - this->_local->at < 2000) {
            ask.cursor = this->_local->cursor; // what was just done here, before the core says it
            ask.picked = this->_local->picked;
            ask.other = this->_local->other;
        }
        return ask;
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_ask;
}

_cold void cluster::Asking::flush_(void)
{
    // the core has what was done here (in order) before a command that depends on it; the local view no longer needed
    for (int waited = 0; waited < 3000; ++waited) {
        {
            std::lock_guard<std::mutex> lock(this->_postMutex);
            if (this->_posts.empty() && !this->_posting) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::lock_guard<std::mutex> lock(this->_localMutex);
    this->_local.reset();
}

_cold void cluster::Asking::post_(cluster::Json request)
{
    request["cmd"] = "ask";
    {
        std::lock_guard<std::mutex> lock(this->_postMutex);
        this->_posts.push_back(std::move(request));
        if (this->_posting) return;
        this->_posting = true;
    }
    std::thread([this]() {
        while (true) {
            cluster::Json next;
            {
                std::lock_guard<std::mutex> lock(this->_postMutex);
                if (this->_posts.empty()) {
                    this->_posting = false;
                    return;
                }
                next = std::move(this->_posts.front());
                this->_posts.pop_front();
            }
            try {
                (void)this->_manager.call(next);
            } catch (const utils::exception::IException&) {}
        }
    }).detach();
}

_cold void cluster::Asking::local_(const std::function<void(cluster::Ask&)>& change)
{
    std::optional<cluster::Ask> ask = this->current();
    if (!ask) return;
    change(*ask);
    std::lock_guard<std::mutex> lock(this->_localMutex);
    this->_local = Local{ask->requestId, ask->index, ask->cursor, ask->picked, ask->other, cluster::now_ms()};
}

_cold void cluster::Asking::move(const int delta)
{
    if (this->_manager.replica()) {
        this->local_([delta](cluster::Ask& a) {
            a.touched = true;
            const int count = static_cast<int>(a.questions[a.index].value("options", cluster::Json::array()).size());
            a.cursor = std::clamp(a.cursor + delta, 0, count);
        });
        this->post_({{"action", "move"}, {"delta", delta}});
        return;
    }

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    const int count = static_cast<int>(this->_ask->questions[this->_ask->index].value("options", cluster::Json::array()).size());
    this->_ask->cursor = std::clamp(this->_ask->cursor + delta, 0, count);
    this->_ask->touched = true; // only the cursor: no answer recorded
    ++this->_version;
}

_cold void cluster::Asking::choose(const int option)
{
    if (this->_manager.replica()) {
        this->local_([option](cluster::Ask& a) {
            const cluster::Json& q = a.questions[a.index];
            const int count = static_cast<int>(q.value("options", cluster::Json::array()).size());
            if (option < 0 || option > count) return;
            a.cursor = option;
            if (q.value("multiSelect", false) && option < count && !a.picked.erase(option)) a.picked.insert(option);
        });
        this->post_({{"action", "choose"}, {"option", option}});
        return;
    }

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    const cluster::Json& q = this->_ask->questions[this->_ask->index];
    const int count = static_cast<int>(q.value("options", cluster::Json::array()).size());
    if (option < 0 || option > count) return;
    this->_ask->cursor = option;
    this->_ask->touched = true;
    if (q.value("multiSelect", false)) {
        if (option < count && !this->_ask->picked.erase(option)) this->_ask->picked.insert(option);
        record_(*this->_ask); // a multi choice: the answer follows the ticks
    }
    ++this->_version;
}

_cold void cluster::Asking::toggle(void)
{
    if (this->_manager.replica()) {
        this->local_([](cluster::Ask& a) {
            if (!a.picked.erase(a.cursor)) a.picked.insert(a.cursor);
        });
        this->post_({{"action", "toggle"}});
        return;
    }

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask || !this->_ask->questions[this->_ask->index].value("multiSelect", false)) return;
    if (!this->_ask->picked.erase(this->_ask->cursor)) this->_ask->picked.insert(this->_ask->cursor);
    this->_ask->touched = true;
    record_(*this->_ask);
    ++this->_version;
}

_cold void cluster::Asking::type(const std::string& text)
{
    if (this->_manager.replica()) {
        this->flush_();
        cluster::Json request = {{"action", "type"}, {"text", text}};
        request["cmd"] = "ask";
        try {
            (void)this->_manager.call(request);
        } catch (const utils::exception::IException&) {}
        return;
    }

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    this->_ask->other += text;
    this->_ask->touched = true;
    if (this->_ask->questions[this->_ask->index].value("multiSelect", false)) {
        this->_ask->picked.insert(static_cast<int>(this->_ask->questions[this->_ask->index].value("options", cluster::Json::array()).size()));
        record_(*this->_ask);
    }
    ++this->_version;
}

_cold void cluster::Asking::erase(void)
{
    if (this->_manager.replica()) {
        this->flush_();
        cluster::Json request = {{"action", "erase"}};
        request["cmd"] = "ask";
        try {
            (void)this->_manager.call(request);
        } catch (const utils::exception::IException&) {}
        return;
    }

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    std::string& other = this->_ask->other;
    while (!other.empty() && (static_cast<unsigned char>(other.back()) & 0xC0) == 0x80) other.pop_back();
    if (!other.empty()) other.pop_back();
    if (this->_ask->questions[this->_ask->index].value("multiSelect", false)) record_(*this->_ask);
    ++this->_version;
}

_cold void cluster::Asking::setOther(const std::string& text)
{
    if (this->_manager.replica()) {
        this->local_([&text](cluster::Ask& a) {
            const cluster::Json& q = a.questions[a.index];
            const int count = static_cast<int>(q.value("options", cluster::Json::array()).size());
            a.other = text;
            a.cursor = count;
            if (q.value("multiSelect", false)) a.picked.insert(count);
        });
        this->post_({{"action", "other"}, {"text", text}});
        return;
    }

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask) return;
    const cluster::Json& q = this->_ask->questions[this->_ask->index];
    const int count = static_cast<int>(q.value("options", cluster::Json::array()).size());
    this->_ask->other = text;
    this->_ask->cursor = count;
    this->_ask->touched = true;
    if (q.value("multiSelect", false)) {
        this->_ask->picked.insert(count);
        record_(*this->_ask);
    }
    ++this->_version;
}

_cold std::string cluster::Asking::next(void)
{
    if (this->_manager.replica()) {
        this->flush_();
        try {
            const cluster::Json r = this->_manager.call({{"cmd", "ask"}, {"action", "next"}});
            return r.is_object() ? r.value("why", std::string()) : std::string();
        } catch (const utils::exception::IException& e) {
            return e.info();
        }
    }

    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->next_();
}

_cold std::string cluster::Asking::next_(void)
{
    // the answer of the question shown (labels of the options, several joined by ", "; other: the text typed)
    if (!this->_ask) return "no question";
    cluster::Ask& a = *this->_ask;
    if (a.review) {
        // Submit: like Claude Code, what is answered is sent, even with questions left (the review flagged them)
        const cluster::Ask done = a;
        this->_ask.reset();
        try {
            this->_manager.answerQuestions(done.session, done.requestId, done.answers);
        } catch (const utils::exception::IException& e) {
            return e.info();
        }
        return "";
    }
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
        if (value.empty()) value = "(none)"; // nothing ticked: a valid answer (Enter validates, Space ticks)
    } else {
        add(a.cursor < count ? options[static_cast<std::size_t>(a.cursor)].value("label", std::string()) : a.other);
    }
    if (value.empty()) return "type the answer (other), or choose an option";
    a.answers[q.value("question", std::string())] = value;
    a.given[a.index] = {a.cursor, a.picked, a.other}; // kept: back shows it, Enter goes on with it
    ++this->_version;
    if (a.fix) { // taken from the review to this one: back to the review (it flags the next one left, if any)
        a.fix = false;
        a.index = a.questions.size() - 1;
        a.review = true;
        return "";
    }
    if (++a.index < a.questions.size()) { // its kept selection (back, or an arrow, then here again): as it was
        restore_(a);
        return "";
    }
    a.index = a.questions.size() - 1;
    if (a.questions.size() > 1 && !a.review) { // like Claude Code: every answer reviewed before it is sent
        a.review = true;
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

_cold bool cluster::Asking::forward(void)
{
    // → / Tab: the next question, answered or not; no answer is recorded or changed by it (the review flags the questions not
    // answered); on the only question: sent, once it is answered (a multi choice: as ticked, none is an answer too)
    if (this->_manager.replica()) {
        this->flush_();
        try {
            return this->_manager.call({{"cmd", "ask"}, {"action", "forward"}}).value("moved", false);
        } catch (const utils::exception::IException&) {
            return false;
        }
    }

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_ask || this->_ask->review) return false;
    cluster::Ask& a = *this->_ask;
    if (a.index + 1 < a.questions.size()) {
        ++a.index;
        restore_(a);
        ++this->_version;
        return true;
    }
    if (a.questions.size() > 1) {
        a.review = true;
        ++this->_version;
        return true;
    }
    const cluster::Json& q = a.questions[a.index];
    if (q.value("multiSelect", false)) return this->next_().empty();
    if (!a.answers.contains(q.value("question", std::string()))) return false; // nothing validated: Enter on an option
    a.review = true;
    return this->next_().empty(); // sends what is recorded
}

_cold void cluster::Asking::back(void)
{
    if (this->_manager.replica()) {
        this->flush_();
        cluster::Json request = {{"action", "back"}};
        request["cmd"] = "ask";
        try {
            (void)this->_manager.call(request);
        } catch (const utils::exception::IException&) {}
        return;
    }

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_ask && this->_ask->review) { // the review left: the last question, as answered
        this->_ask->review = false;
        restore_(*this->_ask);
        ++this->_version;
        return;
    }
    if (!this->_ask || this->_ask->index == 0) return;
    --this->_ask->index; // its answer back as it was given, what was only browsed on this one dropped
    restore_(*this->_ask);
    ++this->_version;
}

_cold void cluster::Asking::decline(void)
{
    if (this->_manager.replica()) {
        this->flush_();
        cluster::Json request = {{"action", "decline"}};
        request["cmd"] = "ask";
        try {
            (void)this->_manager.call(request);
        } catch (const utils::exception::IException&) {}
        return;
    }

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
    if (this->_manager.replica()) {
        this->flush_();
        try {
            const cluster::Json r = this->_manager.call({{"cmd", "ask"}, {"action", "global"}});
            return r.is_object() ? r.value("done", std::string()) : std::string();
        } catch (const utils::exception::IException& e) {
            return e.info();
        }
    }

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
    if (this->_manager.replica()) {
        this->flush_();
        try {
            const cluster::Json r = this->_manager.call({{"cmd", "ask"}, {"action", "chat"}, {"text", text}});
            return r.is_object() ? r.value("done", std::string()) : std::string();
        } catch (const utils::exception::IException& e) {
            return e.info();
        }
    }

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
    if (this->_manager.replica()) {
        this->flush_();
        try {
            const cluster::Json r = this->_manager.call({{"cmd", "ask"}, {"action", "say"}, {"text", sentence}});
            return r.is_object() ? r.value("done", std::string()) : std::string();
        } catch (const utils::exception::IException& e) {
            return e.info();
        }
    }

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
        const cluster::Json& shown = ask->questions[ask->index];
        if (!ask->review && !shown.value("multiSelect", false) && !ask->touched && !ask->answers.contains(shown.value("question", std::string())))
            return "choose an option first (a number, or its name)"; // the cursor alone is not an answer
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
        {"cursor", ask->cursor}, {"picked", ask->picked}, {"other", ask->other}, {"answers", ask->answers}, {"chatting", ask->chatting}, {"review", ask->review}, {"answered", ask->given.contains(ask->index)},
        {"chat", [&ask]() {
            cluster::Json chat = cluster::Json::array();
            for (const auto &[role, text]: ask->chat)
                chat.push_back({{"role", role}, {"text", text}});
            return chat;
        }()}};
}

std::uint64_t cluster::Asking::version(void) const
{
    if (!this->_manager.replica()) return this->_version;
    const cluster::Json j = this->_manager.replicaState("asking");
    return std::hash<std::string>{}(j.dump(-1, ' ', false, cluster::JSON_LENIENT)); // its content: changed when it changes
}
