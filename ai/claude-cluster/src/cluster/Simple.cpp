/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 23/10/2026 by @author Tsukini

File Name:
##  @file Simple.cpp

File Description:
##  The simple mode: its words (English, French), a session as a card, what waits for you, the data of the page
\**************************************************************/

#include "cluster/Manager.hpp"
#include "cluster/Simple.hpp"
#include "cluster/Config.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <ctime>
#include <map>

/* words */
// <key, <English, French>>; {name} {n} {tool}... filled by simple_fill
static const std::map<std::string, std::pair<std::string, std::string>>& words_(void)
{
    static const std::map<std::string, std::pair<std::string, std::string>> words = {
        // the home
        {"greet_morning", {"Good morning", "Bonjour"}}, {"greet_afternoon", {"Good afternoon", "Bon après-midi"}}, {"greet_evening", {"Good evening", "Bonsoir"}},
        {"sub_busy", {"Your assistant and {n} helpers are on it.", "Ton assistant et {n} aides s'en occupent."}},
        {"sub_quiet", {"Nothing is running. Ask anything below.", "Rien ne tourne. Demande ce que tu veux en bas."}},
        {"chip_working", {"{n} working", "{n} travaillent"}}, {"chip_needs", {"{n} need you", "{n} t'attendent"}}, {"chip_done", {"{n} done", "{n} finies"}},
        {"chip_working_one", {"1 working", "1 travaille"}}, {"chip_needs_one", {"1 needs you", "1 t'attend"}}, {"chip_done_one", {"1 done", "1 finie"}},
        {"chip_all", {"all", "tout"}}, {"needs_you", {"Needs you", "À toi de voir"}}, {"your_work", {"Your work", "Ton travail"}},
        {"nothing_waits", {"Nothing waits for you.", "Rien ne t'attend."}}, {"no_work", {"No helper yet: ask your assistant below.", "Aucune aide pour l'instant : demande à ton assistant en bas."}},
        {"new_task", {"+ New task", "+ Nouvelle tâche"}}, {"agents_run", {"{n} agents · {t}", "{n} agents · {t}"}},
        {"agent_one", {"1 agent · {t}", "1 agent · {t}"}}, {"send", {"Send", "Envoyer"}}, {"prompt_hint", {"What do you want to get done?", "Que veux-tu faire ?"}},
        {"prompt_modes", {"/ commands · @ files · ! shell · : edit", "/ commandes · @ fichiers · ! shell · : édition"}},
        {"to_assistant", {"to your assistant", "à ton assistant"}}, {"to_session", {"to {name}", "à {name}"}}, {"assistant", {"Your assistant", "Ton assistant"}},
        // states and what a session does
        {"st_working", {"Working", "Travaille"}}, {"st_needs", {"Needs you", "T'attend"}}, {"st_stuck", {"Stuck", "Bloquée"}}, {"st_done", {"Done", "Finie"}},
        {"st_ready", {"Ready", "Prête"}}, {"st_starting", {"Starting", "Démarre"}},
        {"do_think", {"Thinking", "Réfléchit"}}, {"do_write", {"Writing the answer", "Écrit la réponse"}}, {"do_tool", {"Using {tool}", "Utilise {tool}"}},
        {"do_wait", {"Waits for your OK", "Attend ton accord"}}, {"do_question", {"Asks you a question", "Te pose une question"}}, {"do_start", {"Starting", "Démarre"}}, {"do_ready", {"Ready for a task", "Prête pour une tâche"}},
        {"do_stuck", {"Something went wrong", "Quelque chose a échoué"}}, {"do_queued", {"{n} prompts waiting", "{n} messages en attente"}},
        // what waits for you
        {"ask_run", {"{name} wants to run a command", "{name} veut lancer une commande"}}, {"ask_edit", {"{name} wants to change a file", "{name} veut modifier un fichier"}},
        {"ask_tool", {"{name} wants to use {tool}", "{name} veut utiliser {tool}"}}, {"ask_question", {"{name} asks you a question", "{name} te pose une question"}},
        {"allow", {"Allow", "Autoriser"}}, {"always", {"Always", "Toujours"}}, {"not_now", {"Not now", "Pas maintenant"}}, {"answer", {"Answer", "Répondre"}},
        // the cards
        {"steps", {"{a} of {b} steps", "étape {a} sur {b}"}}, {"retry", {"Try again", "Réessayer"}}, {"ago_now", {"just now", "à l'instant"}},
        {"ago_min", {"{n} min ago", "il y a {n} min"}}, {"ago_h", {"{n} h ago", "il y a {n} h"}}, {"ago_d", {"{n} d ago", "il y a {n} j"}},
        // the toasts
        {"allowed", {"Allowed: {name} goes on", "Autorisé : {name} continue"}}, {"refused", {"{name} will not do it", "{name} ne le fera pas"}},
        {"sent", {"Sent to your assistant", "Envoyé à ton assistant"}}, {"sent_to", {"Sent to {name}", "Envoyé à {name}"}}, {"retried", {"{name} tries again", "{name} réessaie"}},
        {"stopped", {"{name} stopped", "{name} arrêtée"}}, {"saved", {"Settings saved", "Réglages enregistrés"}}, {"created", {"New helper: {name}", "Nouvelle aide : {name}"}},
        // a session opened
        {"close", {"Close", "Fermer"}}, {"stop", {"Stop", "Arrêter"}}, {"stop_ask", {"Stop this helper?", "Arrêter cette aide ?"}},
        {"stop_why", {"It ends its current step. Its work so far is kept.", "Elle termine son étape en cours. Son travail est gardé."}}, {"yes_stop", {"Stop it", "Arrêter"}},
        {"tell_it", {"Tell it something…", "Dis-lui quelque chose…"}}, {"steps_folded", {"{n} steps", "{n} étapes"}}, {"more", {"More", "Plus"}},
        {"open_terminal", {"Open in the terminal", "Ouvrir dans le terminal"}}, {"fork", {"Fork: a copy that goes its own way", "Fork : une copie qui part de son côté"}},
        {"archive", {"Archive", "Archiver"}}, {"delete", {"Delete for good…", "Supprimer définitivement…"}}, {"back", {"Back", "Retour"}},
        {"files_touched", {"Files changed", "Fichiers modifiés"}}, {"todo", {"To do", "À faire"}},
        // a new task
        {"nt_title", {"New task", "Nouvelle tâche"}}, {"nt_what", {"What should it do?", "Que doit-elle faire ?"}}, {"nt_where", {"In which project?", "Dans quel projet ?"}},
        {"nt_create", {"Create", "Créer"}}, {"nt_hint", {"e.g. Add a dark mode to the settings page", "ex. Ajouter un mode sombre à la page des réglages"}},
        {"nt_folder", {"Another folder…", "Un autre dossier…"}}, {"nt_skills", {"@ files and / skills work here too", "@ fichiers et / skills marchent ici aussi"}},
        // the settings
        {"settings", {"Settings", "Réglages"}}, {"set_general", {"General", "Général"}}, {"set_account", {"Account & provider", "Compte & fournisseur"}},
        {"set_assistant", {"Assistant", "Assistant"}}, {"set_phone", {"Phone", "Téléphone"}}, {"set_keys", {"Shortcuts", "Raccourcis"}}, {"set_about", {"About", "À propos"}},
        {"set_language", {"Language", "Langue"}}, {"set_language_d", {"The words of the simple mode", "Les mots du mode simple"}},
        {"set_look", {"Look", "Apparence"}}, {"set_look_d", {"The colours", "Les couleurs"}},
        {"set_notify", {"Notifications", "Notifications"}}, {"set_notify_d", {"When a helper needs you or is done", "Quand une aide t'attend ou a fini"}},
        {"set_interface", {"Interface", "Interface"}}, {"set_interface_d", {"Back to the classic one: every view, every setting", "Revenir au classique : toutes les vues, tous les réglages"}},
        {"simple", {"Simple", "Simple"}}, {"classic", {"Classic", "Classique"}},
        {"set_provider", {"Provider", "Fournisseur"}}, {"set_provider_d", {"Who runs the helpers (every session)", "Qui fait tourner les aides (toutes les sessions)"}},
        {"set_status", {"Account", "Compte"}},
        {"set_orch", {"How it splits the work", "Comment il répartit le travail"}}, {"set_orch_d", {"The orchestration mode of your assistant", "Le mode d'orchestration de ton assistant"}},
        {"orch_compact", {"Compact: one assistant does almost everything itself. Fewest tokens, slower on big jobs.",
            "Compact : un assistant fait presque tout lui-même. Le moins de tokens, plus lent sur les gros travaux."}},
        {"orch_hybrid", {"Hybrid: a helper per task, a sub-assistant per big category. Good balance of speed and tokens.",
            "Hybride : une aide par tâche, un sous-assistant par grosse catégorie. Bon équilibre vitesse / tokens."}},
        {"orch_ultra", {"Ultra: many helpers in parallel, sub-assistants everywhere. Fastest, uses the limit quickly.",
            "Ultra : beaucoup d'aides en parallèle, des sous-assistants partout. Le plus rapide, consomme vite la limite."}},
        {"set_smart", {"How smart", "Quelle intelligence"}}, {"set_smart_d", {"Faster and cheaper, or slower and stronger", "Plus rapide et moins cher, ou plus lent et plus fort"}},
        {"smart_fast", {"Fast", "Rapide"}}, {"smart_balanced", {"Balanced", "Équilibré"}}, {"smart_best", {"Best", "Le meilleur"}},
        {"set_ask", {"Ask me before", "Me demander avant"}}, {"set_ask_d", {"What helpers may do without asking", "Ce que les aides peuvent faire sans demander"}},
        {"ask_every", {"Everything", "Tout"}}, {"ask_edits", {"Risky things", "Les choses risquées"}}, {"ask_never", {"Never", "Jamais"}},
        {"set_parallel", {"Helpers at once", "Aides en même temps"}}, {"set_parallel_d", {"More is faster, but uses the limit quicker", "Plus = plus rapide, mais consomme la limite plus vite"}},
        {"set_web", {"Use it from my phone", "L'utiliser depuis mon téléphone"}},
        {"set_web_d", {"Open the web access, then scan or type the address on the phone (same Wi-Fi)", "Ouvre l'accès web, puis scanne ou tape l'adresse sur le téléphone (même Wi-Fi)"}},
        {"set_web_start", {"Open at start", "Ouvrir au démarrage"}}, {"set_web_start_d", {"The phone can connect as soon as claude-cluster runs", "Le téléphone peut se connecter dès que claude-cluster tourne"}},
        {"web_on", {"Open now", "Ouvrir maintenant"}}, {"web_off", {"Close it", "Le fermer"}},
        {"set_keys_d", {"Click a shortcut to change it; one used twice is shown with a !", "Clique un raccourci pour le changer ; un raccourci en double est marqué d'un !"}},
        {"keys_twice", {"{key} is also {other}: only one works", "{key} est aussi {other} : un seul marche"}},
        {"about_version", {"Version", "Version"}}, {"about_docs", {"Documentation", "Documentation"}}, {"about_docs_d", {"Guide, FAQ, security, in your browser", "Guide, FAQ, sécurité, dans ton navigateur"}},
        {"full_settings", {"Open the full settings", "Ouvrir tous les réglages"}}, {"save", {"Save", "Enregistrer"}}, {"cancel", {"Cancel", "Annuler"}},
        {"reopens", {"Reopens on the page you left", "Se rouvre sur la page laissée"}}, {"open", {"Open", "Ouvrir"}}, {"change", {"Change", "Changer"}},
        // the stats
        {"stats", {"Stats", "Stats"}}, {"st_week", {"This week left", "Reste cette semaine"}}, {"st_5h", {"Next 5 hours left", "Reste sur 5 heures"}},
        {"st_today", {"Today", "Aujourd'hui"}}, {"st_tokens", {"{n} tokens", "{n} tokens"}}, {"st_finished", {"Finished today", "Finies aujourd'hui"}},
        {"st_7days", {"The last 7 days", "Les 7 derniers jours"}}, {"st_top", {"Who used the most", "Qui a le plus consommé"}},
        {"st_full", {"Open the full stats", "Ouvrir les stats complètes"}}, {"st_resets", {"resets {when}", "repart {when}"}}, {"st_unknown", {"not known", "inconnu"}},
        // the keys of the terminal
        {"k_choose", {"choose", "choisir"}}, {"k_open", {"open", "ouvrir"}}, {"k_quit", {"quit", "quitter"}},
        {"keys_line", {"arrows choose · Enter open · a allow · n not now · t new task · s settings · F10 stats · ? help · Ctrl+Q quit",
            "flèches choisir · Entrée ouvrir · a autoriser · n pas maintenant · t nouvelle tâche · s réglages · F10 stats · ? aide · Ctrl+Q quitter"}},
        {"keys_settings", {"Tab page · ↑↓ row · ←→ / Space change · Enter save · Esc close (reopens here) · f all the settings",
            "Tab page · ↑↓ ligne · ←→ / Espace changer · Entrée enregistrer · Échap fermer (se rouvre ici) · f tous les réglages"}},
    };
    return words;
}

_cold std::string cluster::simple_word(const std::string& key, const std::string& lang)
{
    const auto found = words_().find(key);
    if (found == words_().end()) return key;
    return lang == "fr" ? found->second.second : found->second.first;
}

_cold std::string cluster::simple_fill(std::string text, const std::map<std::string, std::string>& values)
{
    for (const auto &[name, value]: values)
        for (std::size_t at = text.find("{" + name + "}"); at != std::string::npos; at = text.find("{" + name + "}", at + value.size()))
            text.replace(at, name.size() + 2, value);
    return text;
}

_cold cluster::Json cluster::simple_words(const std::string& lang)
{
    cluster::Json out = cluster::Json::object();
    for (const auto &[key, pair]: words_())
        out[key] = lang == "fr" ? pair.second : pair.first;
    return out;
}

/* the mode */
_cold bool cluster::simple_on(cluster::Manager& manager)
{
    return manager.ui("mode", manager.config().uiMode) == "simple";
}

_cold std::string cluster::simple_lang(cluster::Manager& manager)
{
    const std::string lang = manager.ui("language", manager.config().uiLanguage);
    return lang == "fr" ? "fr" : "en";
}

/* a session as a card */
_cold cluster::SimpleCard cluster::simple_card(const cluster::Snapshot& snap, const std::string& lang)
{
    cluster::SimpleCard card;
    card.id = snap.spec.id;
    card.global = snap.spec.global;
    card.name = snap.spec.global ? simple_word("assistant", lang) : snap.spec.name;
    card.tag = !snap.spec.category.empty() ? snap.spec.category : snap.spec.tags.empty() ? std::string() : snap.spec.tags.front();
    for (const cluster::SubAgent& a: snap.agents)
        if (a.status == "running") {
            ++card.agents;
            card.agentsSince = card.agentsSince == 0 ? a.started : std::min(card.agentsSince, a.started);
        }
    card.total = static_cast<int>(snap.todos.size());
    card.done = static_cast<int>(std::count_if(snap.todos.begin(), snap.todos.end(), [](const cluster::Todo& t) {return t.status == "completed";}));
    card.since = snap.entries.empty() ? 0 : snap.entries.back().time;
    // its state in one word: a permission waits > an error > a turn runs > starting > answered (done) / never asked (ready)
    const bool answered = std::any_of(snap.entries.begin(), snap.entries.end(), [](const cluster::Entry& e) {return e.kind == cluster::EntryKind::Assistant;});
    if (!snap.permissions.empty() || snap.state == cluster::State::Waiting) card.state = "needs";
    else if (snap.state == cluster::State::Error || (!snap.lastError.empty() && snap.state != cluster::State::Working)) card.state = "stuck";
    else if (snap.state == cluster::State::Working || snap.state == cluster::State::Remote) card.state = "working";
    else if (snap.state == cluster::State::Starting) card.state = "starting";
    else card.state = answered ? "done" : "ready";
    // what it does now, in words: its todo in progress, else its step; done: the start of its last answer
    if (card.state == "needs") {
        // a question (AskUserQuestion: its own popup) said as one, a tool by its name
        const bool question = !snap.permissions.empty() && snap.permissions.front().tool == "AskUserQuestion";
        card.doing = question ? simple_word("do_question", lang)
            : simple_word("do_wait", lang) + (snap.permissions.empty() ? std::string() : " · " + snap.permissions.front().tool);
    } else if (card.state == "stuck") {
        card.doing = snap.lastError.empty() ? simple_word("do_stuck", lang) : cluster::one_line(snap.lastError, 90);
    } else if (card.state == "working") {
        const auto now = std::find_if(snap.todos.begin(), snap.todos.end(), [](const cluster::Todo& t) {return t.status == "in_progress";});
        if (now != snap.todos.end()) card.doing = now->activeForm.empty() ? now->content : now->activeForm;
        else if (!snap.tools.empty() && !snap.tools.back().done) card.doing = simple_fill(simple_word("do_tool", lang), {{"tool", snap.tools.back().name}});
        else if (snap.step == "writing") card.doing = simple_word("do_write", lang);
        else card.doing = simple_word("do_think", lang);
        if (snap.queued > 0) card.doing += " · " + simple_fill(simple_word("do_queued", lang), {{"n", std::to_string(snap.queued)}});
    } else if (card.state == "starting") {
        card.doing = simple_word("do_start", lang);
    } else if (card.state == "done") {
        for (auto it = snap.entries.rbegin(); it != snap.entries.rend(); ++it)
            if (it->kind == cluster::EntryKind::Assistant) {
                card.doing = cluster::one_line(it->text, 110);
                break;
            }
    } else {
        card.doing = snap.spec.summary.empty() ? simple_word("do_ready", lang) : cluster::one_line(snap.spec.summary, 110);
    }
    return card;
}

/* what waits for you */
_cold std::vector<cluster::SimpleAsk> cluster::simple_asks(const std::vector<cluster::Snapshot>& sessions, const std::string& lang)
{
    std::vector<cluster::SimpleAsk> asks;
    for (const cluster::Snapshot& snap: sessions)
        for (const cluster::Permission& p: snap.permissions) {
            if (p.tool == "AskUserQuestion") continue; // a question: its own card (Answer), never an Allow of a tool
            cluster::SimpleAsk ask;
            ask.session = snap.spec.id;
            ask.name = snap.spec.global ? simple_word("assistant", lang) : snap.spec.name;
            ask.requestId = p.requestId;
            const bool run = p.tool == "Bash" || p.tool == "BashOutput";
            const bool edit = p.tool == "Edit" || p.tool == "Write" || p.tool == "MultiEdit" || p.tool == "NotebookEdit";
            ask.what = simple_fill(simple_word(run ? "ask_run" : edit ? "ask_edit" : "ask_tool", lang), {{"name", ask.name}, {"tool", p.tool}});
            if (p.input.is_object() && p.input.contains("command") && p.input["command"].is_string()) ask.detail = cluster::one_line(p.input["command"].get<std::string>(), 160);
            else if (p.input.is_object() && p.input.contains("file_path") && p.input["file_path"].is_string()) ask.detail = cluster::short_path(p.input["file_path"].get<std::string>());
            else ask.detail = cluster::one_line(p.description.empty() ? p.reason : p.description, 160);
            asks.push_back(ask);
        }
    return asks;
}

/* the greeting, the time */
_cold std::string cluster::simple_greeting(const cluster::Config& config, const std::string& lang)
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    (void)::localtime_r(&now, &local);
    const std::string greet = simple_word(local.tm_hour < 12 ? "greet_morning" : local.tm_hour < 18 ? "greet_afternoon" : "greet_evening", lang);
    return config.user.nickname.empty() ? greet : greet + ", " + config.user.nickname;
}

_cold std::string cluster::simple_ago(const std::int64_t since, const std::string& lang)
{
    const std::int64_t gone = std::max<std::int64_t>(0, cluster::now() - since);
    if (since == 0 || gone < 60) return simple_word("ago_now", lang);
    if (gone < 3600) return simple_fill(simple_word("ago_min", lang), {{"n", std::to_string(gone / 60)}});
    if (gone < 86400) return simple_fill(simple_word("ago_h", lang), {{"n", std::to_string(gone / 3600)}});
    return simple_fill(simple_word("ago_d", lang), {{"n", std::to_string(gone / 86400)}});
}

/* the page */
_cold cluster::Json cluster::simple_json(cluster::Manager& manager)
{
    const std::string lang = simple_lang(manager);
    const std::vector<cluster::Snapshot> sessions = manager.shown();
    cluster::Json cards = cluster::Json::array();
    int working = 0;
    int needs = 0;
    int done = 0;
    for (const cluster::Snapshot& snap: sessions) {
        const cluster::SimpleCard c = simple_card(snap, lang);
        working += c.state == "working" ? 1 : 0;
        needs += c.state == "needs" ? 1 : 0;
        done += c.state == "done" ? 1 : 0;
        cards.push_back({{"id", c.id}, {"name", c.name}, {"state", c.state}, {"doing", c.doing}, {"done", c.done}, {"total", c.total}, {"tag", c.tag}, {"agents", c.agents}, {"agents_since", c.agentsSince},
            {"ago", simple_ago(c.since, lang)}, {"global", c.global}});
    }
    cluster::Json asks = cluster::Json::array();
    for (const cluster::SimpleAsk& a: simple_asks(sessions, lang))
        asks.push_back({{"session", a.session}, {"name", a.name}, {"request_id", a.requestId}, {"what", a.what}, {"detail", a.detail}});
    const int helpers = static_cast<int>(std::count_if(sessions.begin(), sessions.end(), [](const cluster::Snapshot& s) {return !s.spec.global;}));
    return {{"on", simple_on(manager)}, {"lang", lang}, {"greeting", simple_greeting(manager.config(), lang)},
        {"subtitle", working + needs > 0 ? simple_fill(simple_word("sub_busy", lang), {{"n", std::to_string(helpers)}}) : simple_word("sub_quiet", lang)},
        {"counts", {{"working", working}, {"needs", needs}, {"done", done}}}, {"cards", cards}, {"asks", asks}};
}
