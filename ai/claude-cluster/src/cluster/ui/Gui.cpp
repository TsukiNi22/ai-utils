/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Gui.cpp

File Description:
##  Window front-end (Qt6 Widgets): same layouts, panels, bars and
##  palette as the terminal one
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Settings.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Session.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Worker.hpp"
#include "cluster/Editor.hpp"
#include "cluster/Reveal.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Core.hpp"
#include "cluster/Git.hpp"
#include <QSyntaxHighlighter>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QFontDatabase>
#include <QTextDocument>
#include <QApplication>
#include <QTextBrowser>
#include <QKeySequence>
#include <QInputDialog>
#include <QProgressBar>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QFormLayout>
#include <QTextCursor>
#include <QScrollBar>
#include <functional>
#include <QTabWidget>
#include <QClipboard>
#include <QLineEdit>
#include <QSplitter>
#include <QShortcut>
#include <QCheckBox>
#include <QComboBox>
#include <QKeyEvent>
#include <QMimeData>
#include <QPalette>
#include <QToolTip>
#include <QSpinBox>
#include <QBuffer>
#include <QCursor>
#include <fstream>
#include <QWidget>
#include <QTabBar>
#include <QDialog>
#include <QImage>
#include <chrono>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <regex>
#include <cmath>
#include <QUrl>
#include <map>

namespace {
//----------------------------------------------------------------//
/* TOOLS */

// xstyle: ignore-next CPP-SINGLE-STATEMENT (helper of this file only)
_cold QString q_(const std::string& text)
{
    return QString::fromStdString(text);
}

// xstyle: ignore-next CPP-SINGLE-STATEMENT (helper of this file only)
_cold QString esc_(const std::string& text)
{
    return QString::fromStdString(text).toHtmlEscaped().replace("\n", "<br>");
}

_cold QString engine_html_(const std::string& help)
{
    // the help of an engine: its name in bold, its links clickable
    std::string html;
    for (const std::string& line: cluster::split(help, '\n')) {
        const std::string text = std::regex_replace(esc_(line).toStdString(), std::regex(R"((https?://[^\s<]+))"), "<a href=\"$1\">$1</a>");
        html += html.empty() ? "<b>" + text + "</b>" : "<br>" + text;
    }
    return QString::fromStdString("<small>" + html + "</small>");
}

// xstyle: ignore-next CPP-SINGLE-STATEMENT (helper of this file only)
_cold QString span_(const std::string& color, const QString& text, const bool bold = false)
{
    return "<span style=\"color:" + q_(color) + (bold ? ";font-weight:600" : "") + "\">" + text + "</span>";
}

//----------------------------------------------------------------//
/* CLASS */

class Window;

// Prompt of the window: QLineEdit (undo / redo / selection / clipboard of Qt) + the completion list and the images
class PromptEdit: public QPlainTextEdit {
    protected:
        void keyPressEvent(QKeyEvent* event) override;
        void insertFromMimeData(const QMimeData* source) override;  // a long text pasted: one [Pasted text #N]
        bool focusNextPrevChild(bool) override {return false;}; // Tab completes, never leaves the prompt

    public:
        QString text(void) const {return this->toPlainText();};
        void setText(const QString& text);  // put here (history, stash...): no completion opened by it
        int cursorPosition(void) const {return this->textCursor().position();};
        bool hasSelectedText(void) const {return this->textCursor().hasSelection();};
        void insert(const QString& text) {this->textCursor().insertText(text);};
        void setSelection(const int start, const int length)
        {
            QTextCursor cursor = this->textCursor();
            cursor.setPosition(start);
            cursor.setPosition(start + length, QTextCursor::KeepAnchor);
            this->setTextCursor(cursor);
        };
        void fit(void);                     // 1 to 8 lines high
        void vimSync(void);                 // [ui] vim / vim, the cursor and the label of the mode
        bool vimKey(QKeyEvent* event);      // a key of vim (Normal / Visual, Esc of Insert); true: used
        QLabel* vimLabel = nullptr;
        Window* window = nullptr;
        cluster::Editor completion;         // completion, vim, and the atoms ([Image #N], [Pasted text #N]: one block)
        QListWidget* popup = nullptr;
        QWidget* chipsBar = nullptr;
        void refreshChips(void);            // one chip per atom of the prompt (images, pasted texts)

        PromptEdit(Window* owner, QWidget* parent);
        void refreshCompletion(void);
        void apply(const bool final);
        bool addImage(const std::string& path, const bool insert = true); // false: this image was already in the prompt
        void reset(void);
        _nodiscard std::string raw(void) const {return this->completion.atomize(this->text().toStdString());}; // labels -> atoms
        _nodiscard std::string plain(void) const {return this->completion.expand(this->raw());};               // to send / stash
        _nodiscard cluster::Draft draft(void) const {return this->completion.draftOf(this->raw());};          // + its images and pastes
        void loadDraft(const cluster::Draft& draft);                                                         // a prompt back, blocks again
        _nodiscard std::optional<std::pair<int, int>> atomNear(const int pos, const int where) const;           // -1 before, 1 after, 0 inside
};

// The labels of the atoms marked like chips
class AtomHighlighter: public QSyntaxHighlighter {
    private:
        PromptEdit* _edit = nullptr;

    protected:
        void highlightBlock(const QString& text) override
        {
            QTextCharFormat format;
            format.setFontWeight(QFont::Bold);
            format.setForeground(this->_edit->palette().color(QPalette::Highlight));
            QColor back = this->_edit->palette().color(QPalette::Highlight);
            back.setAlpha(45);
            format.setBackground(back);
            for (std::size_t i = 0; i < this->_edit->completion.atomCount(); ++i) {
                const QString label = QString::fromStdString(this->_edit->completion.atom(i).label);
                for (int at = text.indexOf(label); at >= 0; at = text.indexOf(label, at + 1))
                    this->setFormat(at, static_cast<int>(label.size()), format);
            }
        }

    public:
        AtomHighlighter(PromptEdit* owner)
            : QSyntaxHighlighter(owner->document()), _edit(owner)
        {
        }
};

class SessionView: public QFrame {
    public:
        std::string id;
        QLabel* header = nullptr;
        QTextBrowser* log = nullptr;
        QLabel* panels = nullptr;
        QWidget* permBar = nullptr;
        QLabel* permLabel = nullptr;
        std::string lastKey;

        SessionView(Window* window, const std::string& sessionId, const bool compact);
        void update(const cluster::Snapshot& s, const cluster::Style& style, cluster::Manager& manager, const bool compact, cluster::Reveal& reveal, const bool expanded);
};

class Window: public QMainWindow {
    public:
        cluster::App& app;
        cluster::Manager& manager;
        std::string active = GLOBAL_ID;
        std::string signature;                  // layout + session ids: the central widget is rebuilt when it changes
        std::map<std::string, SessionView*> views;
        QListWidget* list = nullptr;
        QTabBar* tabs = nullptr;
        PromptEdit* input = nullptr;
        QWidget* findBar = nullptr;
        QLineEdit* findInput = nullptr;
        std::int64_t lastEscape = 0;
        QLabel* status = nullptr;
        QLabel* voiceLabel = nullptr;
        QLabel* target = nullptr;
        std::uint64_t lastVersion = 0;
        std::int64_t statusTime = 0;
        std::vector<QShortcut*> shortcuts;
        cluster::Reveal reveal;                 // the answers typed as they come
        bool animating = false;                 // typing / a turn running: drawn at every tick
        bool expanded = false;                  // Ctrl+O: thinking and tool results in full
        int historyAt = -1;                     // Up / Down: -1 the prompt being written, else an index of the history
        std::vector<cluster::Draft> historyList;
        std::string historySession;             // the session of that list
        std::map<int, QString> historyEdits;    // the texts changed while browsing (forgotten when a prompt is sent)

        bool historyStep(const int delta)
        {
            // +1 older, -1 newer, never looped; the text left (changed or not) is kept for when it comes back; the prompts
            // of the session shown only (Ctrl+R: every session), from the newest again when it changed
            if (this->historySession != this->active) {
                this->historyAt = -1;
                this->historyEdits.clear();
            }
            if (this->historyAt == -1 && delta > 0) {
                this->historyList = this->manager.drafts(500, this->active);
                this->historySession = this->active;
            }
            const int to = this->historyAt + delta;
            if (to < -1 || to >= static_cast<int>(this->historyList.size())) return false;
            this->historyEdits[this->historyAt] = this->input->text();
            this->historyAt = to;
            const auto edited = this->historyEdits.find(to);
            if (edited != this->historyEdits.end()) this->input->setText(edited->second);
            else this->input->loadDraft(this->historyList[static_cast<std::size_t>(to)]);
            this->message(to == -1 ? "back to the prompt being written" : "history " + std::to_string(to + 1) + "/" + std::to_string(this->historyList.size()) + " (↓ newer)", false);
            return true;
        }

        void historyReset(void)
        {
            this->historyAt = -1;
            this->historyEdits.clear();
            this->historyList.clear();
        }

        Window(cluster::App& application)
            : app(application), manager(*application.manager)
        {
            this->setWindowTitle("claude-cluster");
            this->resize(1400, 860);
            this->bindKeys();
            this->rebuild();
            QTimer* timer = new QTimer(this);
            QObject::connect(timer, &QTimer::timeout, [this]() {this->tick();});
            timer->start(50);
        }

        _nodiscard const cluster::Style& theme(void) const
        {
            const cluster::Config& config = this->manager.config();
            auto it = config.styles.find(this->manager.ui("style", config.style));
            return it != config.styles.end() ? it->second : config.currentStyle();
        }

        void applyStyle(void)
        {
            const cluster::Style& s = this->theme();
            QPalette palette;
            palette.setColor(QPalette::Window, QColor(q_(s.bg)));
            palette.setColor(QPalette::Base, QColor(q_(s.surface)));
            palette.setColor(QPalette::AlternateBase, QColor(q_(s.bg)));
            palette.setColor(QPalette::Text, QColor(q_(s.text)));
            palette.setColor(QPalette::WindowText, QColor(q_(s.text)));
            palette.setColor(QPalette::Button, QColor(q_(s.surface)));
            palette.setColor(QPalette::ButtonText, QColor(q_(s.text)));
            palette.setColor(QPalette::Highlight, QColor(q_(s.accent)));
            palette.setColor(QPalette::HighlightedText, QColor(q_(s.bg)));
            palette.setColor(QPalette::PlaceholderText, QColor(q_(s.muted)));
            QApplication::setPalette(palette);
            this->setStyleSheet(q_(
                "QFrame#session{border:1px solid " + s.border + ";border-radius:8px;}"
                "QFrame#session[active=\"true\"]{border:2px solid " + s.accent + ";}"
                "QTextBrowser{border:none;background:" + s.bg + ";}"
                "QLineEdit{border:1px solid " + s.border + ";border-radius:6px;padding:6px;background:" + s.surface + ";}"
                "QPushButton{border:1px solid " + s.border + ";border-radius:6px;padding:4px 10px;}"
                "QPushButton:hover{border-color:" + s.accent + ";}"
                "QListWidget{border:none;background:" + s.bg + ";}"
                "QListWidget::item:selected{background:" + s.surface + ";color:" + s.text + ";}"
                "QLabel#panels{background:" + s.surface + ";border-radius:8px;padding:8px;}"
                "QWidget#perm{background:" + s.surface + ";border:1px solid " + s.warn + ";border-radius:6px;}"));
        }

        void bindKeys(void)
        {
            for (QShortcut* shortcut: this->shortcuts)
                delete shortcut;
            this->shortcuts.clear();
            for (const auto &[action, key]: this->manager.config().keys) {
                if (action == "prefix") continue; // only the chords below
                if (action == "undo" || action == "redo" || action == "select_all" || action == "cut" || action == "paste") continue; // the prompt (Qt)
                if (action == "mode_cycle") continue; // Shift+Tab: in the prompt (else it moves the focus)
                for (const std::string& alternative: cluster::split(key, '|')) {
                    QShortcut* shortcut = new QShortcut(QKeySequence(q_(alternative)), this);
                    const std::string name = action;
                    QObject::connect(shortcut, &QShortcut::activated, [this, name]() {this->shortcut(name);});
                    this->shortcuts.push_back(shortcut);
                }
            }
            // Alt+1..9: session N
            for (int n = 1; n <= 9; ++n) {
                QShortcut* shortcut = new QShortcut(QKeySequence(q_("Alt+" + std::to_string(n))), this);
                QObject::connect(shortcut, &QShortcut::activated, [this, n]() {
                    const std::vector<cluster::Snapshot> all = this->manager.list(true);
                    if (static_cast<std::size_t>(n) <= all.size()) this->active = all[static_cast<std::size_t>(n - 1)].spec.id;
                    this->refresh(true);
                });
                this->shortcuts.push_back(shortcut);
            }
            // prefix then an arrow (Ctrl+B, Left...): session on the left / right / above / below
            const std::string prefix = this->manager.config().keys.contains("prefix") ? this->manager.config().keys.at("prefix") : "Ctrl+B";
            for (const auto &[arrow, direction]: std::vector<std::pair<std::string, std::pair<int, int>>>{
                {"Left", {-1, 0}}, {"Right", {1, 0}}, {"Up", {0, -1}}, {"Down", {0, 1}}}) {
                QShortcut* shortcut = new QShortcut(QKeySequence(q_(prefix + ", " + arrow)), this);
                const std::pair<int, int> move = direction;
                QObject::connect(shortcut, &QShortcut::activated, [this, move]() {this->moveDir(move.first, move.second);});
                this->shortcuts.push_back(shortcut);
            }
        }

        void message(const std::string& text, const bool error)
        {
            if (text.find('\n') != std::string::npos) {
                QMessageBox box(error ? QMessageBox::Warning : QMessageBox::Information, "claude-cluster", q_(text), QMessageBox::Ok, this);
                box.exec();
                return;
            }
            this->status->setText(span_(error ? this->theme().error : this->theme().muted, esc_(text)));
            this->statusTime = cluster::now();
        }

        void run(const cluster::Action& action, const std::string& value)
        {
            const std::string base = action.id.substr(0, action.id.find(':'));
            const std::string arg = action.id.find(':') == std::string::npos ? "" : action.id.substr(action.id.find(':') + 1);
            if (base == "layout") {
                this->manager.setUi("layout", arg);
            } else if (base == "style") {
                this->manager.setUi("style", arg);
                this->applyStyle();
                this->signature.clear();
            } else if (base == "settings") {
                this->settings();
            } else if (base == "machine_sync" || base == "edit_global_prompt" || base == "edit_sessions_prompt" || base == "edit_vocabulary" || base == "edit_words") {
                this->settingAction(base == "edit_global_prompt" ? "edit_prompt" : base);
            } else if (base == "restore_menu") {
                this->openRestore();
            } else if (base == "quit") {
                this->close();
            } else if (base == "auth_login" || base == "auth_logout") {
                this->message("Interactive login: run in a terminal\n\n    claude-cluster auth " + std::string(base == "auth_login" ? "login " : "logout ") + arg, false);
            } else {
                const cluster::ActionResult result = cluster::run_action(this->manager, *this->app.voice, action.id, this->active, value);
                if (!result.focus.empty()) this->active = result.focus;
                if (!result.message.empty()) this->message(result.message, result.error);
            }
            this->refresh(true);
        }

        void choose(const cluster::Action& action)
        {
            if (action.input.empty()) {
                this->run(action, "");
                return;
            }
            bool ok = false;
            const QString value = QInputDialog::getText(this, q_(action.title), q_(action.input),
                action.id.starts_with("auth_key:") ? QLineEdit::Password : QLineEdit::Normal, q_(action.fill), &ok);
            if (ok) this->run(action, value.toStdString());
        }

        void openRestore(void)
        {
            if (this->manager.trash().empty()) this->message("the trash is empty: no closed session to reopen", false);
            else this->palette("restore:");
        }

        void moveDir(const int dx, const int dy)
        {
            // grid: rows and columns; list, tabs: previous / next
            const std::vector<cluster::Snapshot> sessions = this->manager.list(true);
            const int count = static_cast<int>(sessions.size());
            if (count == 0) return;
            auto it = std::find_if(sessions.begin(), sessions.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->active;});
            const int index = it == sessions.end() ? 0 : static_cast<int>(it - sessions.begin());
            const std::string layout = this->manager.ui("layout", this->manager.config().layout);
            const int columns = layout == "grid" && count > 1 ? static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))) : 1;
            int next = index;
            if (dx != 0 || columns == 1) next = (index + (dx != 0 ? dx : dy) + count) % count;
            else if (index + dy * columns >= 0 && index + dy * columns < count) next = index + dy * columns;
            this->active = sessions[static_cast<std::size_t>(next)].spec.id;
            this->refresh(true);
        }

        void palette(const QString& initial = QString())
        {
            QDialog dialog(this);
            dialog.setWindowTitle("Command palette");
            dialog.resize(720, 520);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            QLineEdit* filter = new QLineEdit(&dialog);
            QListWidget* items = new QListWidget(&dialog);
            filter->setPlaceholderText("type to filter, Enter to run");
            layout->addWidget(filter);
            layout->addWidget(items);
            const std::vector<cluster::Action> all = cluster::actions(this->manager, this->active);
            std::vector<cluster::Action> shown;
            const std::function<void()> fill = [&]() {
                const std::string wanted = cluster::lower(filter->text().toStdString());
                items->clear();
                shown.clear();
                for (const cluster::Action& a: all)
                    if (wanted.empty() || cluster::lower(a.title).find(wanted) != std::string::npos || cluster::lower(a.id).find(wanted) != std::string::npos) {
                        shown.push_back(a);
                        items->addItem(q_(a.title));
                    }
                items->setCurrentRow(0);
            };
            filter->setText(initial);
            fill();
            QObject::connect(filter, &QLineEdit::textChanged, [&]() {fill();});
            QObject::connect(filter, &QLineEdit::returnPressed, [&]() {dialog.accept();});
            QObject::connect(items, &QListWidget::itemActivated, [&]() {dialog.accept();});
            QShortcut* down = new QShortcut(QKeySequence(Qt::Key_Down), filter);
            QShortcut* up = new QShortcut(QKeySequence(Qt::Key_Up), filter);
            QObject::connect(down, &QShortcut::activated, [&]() {items->setCurrentRow(std::min(items->currentRow() + 1, items->count() - 1));});
            QObject::connect(up, &QShortcut::activated, [&]() {items->setCurrentRow(std::max(items->currentRow() - 1, 0));});
            if (dialog.exec() != QDialog::Accepted || items->currentRow() < 0 || static_cast<std::size_t>(items->currentRow()) >= shown.size()) return;
            this->choose(shown[static_cast<std::size_t>(items->currentRow())]);
        }

        void settings(void)
        {
            // Every option of config.toml, one tab per section; saved in the file (comments kept), reloaded live
            QDialog dialog(this);
            dialog.setWindowTitle("claude-cluster setup");
            dialog.resize(860, 620);
            dialog.setPalette(QApplication::palette());
            dialog.setStyleSheet(this->styleSheet());
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            QTabWidget* sections = new QTabWidget(&dialog);
            const cluster::Config& config = this->manager.config();
            std::vector<std::pair<cluster::Setting, std::function<std::string()>>> fields; // <setting, value of its widget>

            for (const cluster::SettingsPage& page: cluster::settings_pages(config)) {
                QWidget* tab = new QWidget(sections);
                QFormLayout* form = new QFormLayout(tab);
                for (const cluster::Setting& setting: page.settings) {
                    const std::string current = setting.value(config);
                    const QString tip = q_(setting.description + (setting.restart ? "\n(applied at the next start)" : ""));
                    QWidget* widget = nullptr;
                    QLabel* info = nullptr;     // a choice of engine: what it handles, its limits, its links
                    std::function<std::string()> read;
                    if (setting.kind == cluster::SettingKind::Bool) {
                        QCheckBox* box = new QCheckBox(tab);
                        box->setChecked(current == "true");
                        read = [box]() {return box->isChecked() ? std::string("true") : std::string("false");};
                        widget = box;
                    } else if (setting.kind == cluster::SettingKind::Choice) {
                        QComboBox* box = new QComboBox(tab);
                        for (const std::string& choice: setting.choices)
                            box->addItem(choice.empty() ? QString("(agent default)") : q_(choice), q_(choice));
                        box->setCurrentIndex(std::max(0, box->findData(q_(current))));
                        read = [box]() {return box->currentData().toString().toStdString();};
                        widget = box;
                        if (setting.choiceHelp) {
                            for (int i = 0; i < box->count(); ++i)
                                box->setItemData(i, q_(setting.choiceHelp(box->itemData(i).toString().toStdString())), Qt::ToolTipRole);
                            info = new QLabel(tab);
                            info->setWordWrap(true);
                            info->setOpenExternalLinks(true);
                            info->setTextInteractionFlags(Qt::TextBrowserInteraction);
                            const std::function<std::string(const std::string&)> help = setting.choiceHelp;
                            const std::function<void()> show = [info, box, help]() {info->setText(engine_html_(help(box->currentData().toString().toStdString())));};
                            show();
                            QObject::connect(box, &QComboBox::currentIndexChanged, info, show);
                        }
                    } else if (setting.kind == cluster::SettingKind::Int) {
                        QSpinBox* box = new QSpinBox(tab);
                        box->setRange(0, 1000000);
                        box->setValue(QString::fromStdString(current).toInt());
                        read = [box]() {return std::to_string(box->value());};
                        widget = box;
                    } else if (setting.kind == cluster::SettingKind::Float) {
                        QDoubleSpinBox* box = new QDoubleSpinBox(tab);
                        box->setRange(0, 1000000);
                        box->setDecimals(2);
                        box->setValue(QString::fromStdString(current).toDouble());
                        read = [box]() {return QString::number(box->value()).toStdString();};
                        widget = box;
                    } else if (setting.kind == cluster::SettingKind::Action) {
                        QPushButton* button = new QPushButton(q_("▶ " + current), tab);
                        const std::string key = setting.key;
                        QObject::connect(button, &QPushButton::clicked, [this, key]() {this->settingAction(key);});
                        read = [current]() {return current;}; // not an option: never a change
                        widget = button;
                    } else {
                        QLineEdit* line = new QLineEdit(q_(current), tab);
                        read = [line]() {return cluster::trim(line->text().toStdString());};
                        widget = line;
                    }
                    widget->setToolTip(tip);
                    QLabel* label = new QLabel(q_(setting.label + (setting.restart ? " *" : "")), tab);
                    label->setToolTip(tip);
                    form->addRow(label, widget);
                    if (info != nullptr) form->addRow(info);
                    fields.emplace_back(setting, read);
                }
                sections->addTab(tab, q_(page.title));
            }
            QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            // Ctrl+S: saved, the page stays (the values written become the current ones); F10: save and close
            QShortcut* saveKey = new QShortcut(QKeySequence("Ctrl+S"), &dialog);
            QObject::connect(saveKey, &QShortcut::activated, [this, &fields, &dialog]() {
                std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> now;
                for (const auto &[setting, read]: fields) {
                    const std::string value = read();
                    if (value != setting.value(this->manager.config())) now[{setting.section, setting.key}] = {setting, value};
                }
                if (now.empty()) return;
                try {
                    cluster::settings_write(this->manager.config(), now);
                } catch (const utils::exception::IException& e) {
                    this->message(std::string("not saved: ") + e.info(), true);
                    return;
                }
                for (const char* key: {"layout", "style", "vim"})
                    if (now.contains({"ui", key})) this->manager.setUi(key, now.at({"ui", key}).second);
                (void)this->manager.config().reload();
                dialog.setWindowTitle(q_("claude-cluster setup - " + std::to_string(now.size()) + " setting(s) saved"));
            });
            QShortcut* closeKey = new QShortcut(QKeySequence(Qt::Key_F10), &dialog);
            QObject::connect(closeKey, &QShortcut::activated, &dialog, &QDialog::accept);
            layout->addWidget(sections, 1);
            layout->addWidget(new QLabel("* applied at the next start · hover an option for its help · Ctrl+S save · F10 save & close · Esc close (asks to save the changes)", &dialog));
            layout->addWidget(buttons);
            // Debug: CLAUDE_CLUSTER_SCREENSHOT also saves this page (<file>-settings.png), then closes it
            const char* screenshot = std::getenv("CLAUDE_CLUSTER_SCREENSHOT");
            if (screenshot && *screenshot) {
                const QString path = QString(screenshot).replace(".png", "-settings.png");
                QTimer::singleShot(1200, &dialog, [&dialog, path]() {
                    dialog.grab().save(path);
                    dialog.reject();
                });
            }
            const bool accepted = dialog.exec() == QDialog::Accepted;

            std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> changes;
            for (const auto &[setting, read]: fields) {
                const std::string value = read();
                if (value != setting.value(config)) changes[{setting.section, setting.key}] = {setting, value};
            }
            if (changes.empty()) return;
            // closed (Esc / Cancel) with changes: asked, no = dropped
            if (!accepted && ((screenshot && *screenshot)
                || QMessageBox::question(this, "claude-cluster setup", q_("Save the " + std::to_string(changes.size()) + " change(s)?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes))
                return;
            try {
                cluster::settings_write(config, changes);
            } catch (const utils::exception::IException& e) {
                this->message(std::string("not saved: ") + e.info(), true);
                return;
            }
            for (const char* key: {"layout", "style", "vim"})
                if (changes.contains({"ui", key})) this->manager.setUi(key, changes.at({"ui", key}).second);
            (void)this->manager.config().reload();
            this->bindKeys();
            this->signature.clear();
            this->message(std::to_string(changes.size()) + " setting(s) saved", false);
        }

        void move(const int delta)
        {
            const std::vector<cluster::Snapshot> sessions = this->manager.list(true);
            if (sessions.empty()) return;
            auto it = std::find_if(sessions.begin(), sessions.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->active;});
            int index = it == sessions.end() ? 0 : static_cast<int>(it - sessions.begin());
            index = (index + delta + static_cast<int>(sessions.size())) % static_cast<int>(sessions.size());
            this->active = sessions[static_cast<std::size_t>(index)].spec.id;
            this->refresh(true);
        }

        void shortcut(const std::string& action)
        {
            if (action == "palette") this->palette();
            else if (action == "next_session") this->move(1);
            else if (action == "prev_session") this->move(-1);
            else if (action == "global") {
                this->active = GLOBAL_ID;
                this->refresh(true);
            } else if (action == "new_session") this->choose({"new_session", "New session", "folder [backend]", ".", false});
            else if (action == "close_session") this->run({"close_session", "", "", "", false}, "");
            else if (action == "allow" || action == "deny" || action == "mute") this->run({action, "", "", "", false}, "");
            else if (action == "push_to_talk") this->run({"voice_toggle", "", "", "", false}, "");
            else if (action == "layout") {
                const std::string layout = this->manager.ui("layout", this->manager.config().layout);
                this->manager.setUi("layout", layout == "list" ? "grid" : layout == "grid" ? "tabs" : "list");
            } else if (action == "settings") this->settings();
            else if (action == "restore") this->openRestore();
            else if (action == "interrupt") {
                // voice prompts waiting: dropped; else a turn runs: stop it; else clear the prompt
                const std::optional<cluster::Snapshot> snap = this->manager.snapshot(this->active);
                const bool working = snap && (snap->state == cluster::State::Working || snap->state == cluster::State::Waiting);
                if (!this->app.voice->pending().empty()) this->app.voice->cancel();
                else if (this->app.voice->speaking() && !working) {
                    this->app.voice->mute(); // an answer read after its turn: stopped like a running one
                    this->message("reading stopped", false);
                } else if (working) this->run({"interrupt", "", "", "", false}, "");
                else if (!this->input->text().isEmpty()) {
                    this->input->reset();
                    this->historyReset();
                } else {
                    // nothing to stop or clear: twice within 2 s quits (like Claude Code)
                    const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    if (now - this->lastInterrupt < 2000) this->close();
                    else this->message(this->manager.config().keys.at("interrupt") + " again to quit", false);
                    this->lastInterrupt = now;
                }
            } else if (action == "mode_cycle") {
                const std::optional<cluster::Snapshot> snap = this->manager.snapshot(this->active);
                if (snap) this->run({"mode:" + cluster::next_mode(snap->spec.mode), "", "", "", false}, "");
            } else if (action == "help") {
                QMessageBox box(QMessageBox::Information, "claude-cluster keys", q_(cluster::help_text(this->manager.config())), QMessageBox::Ok, this);
                box.setStyleSheet("QLabel{font-family:monospace;}");
                box.exec();
            } else if (action == "expand") {
                this->expanded = !this->expanded;
                for (const auto &[id, view]: this->views)
                    view->lastKey.clear();
                this->message(this->expanded ? "thinking and tool results in full" : "thinking and tool results folded", false);
                this->refresh(true);
            } else if (action == "search") {
                this->findBar->setVisible(true);
                this->findInput->setFocus();
                this->findInput->selectAll();
            } else if (action == "edit") {
                bool ok = false;
                const QString text = QInputDialog::getMultiLineText(this, "Edit the prompt", "Prompt (Ok: back in the prompt)", q_(this->input->plain()), &ok);
                if (ok) this->input->setText(q_(this->input->completion.shown(this->input->completion.collapse(text.toStdString())))); // unchanged pastes: blocks again
            } else if (action == "stash") {
                if (cluster::trim(this->input->text().toStdString()).empty()) {
                    this->message("nothing to stash", false);
                } else {
                    this->manager.stashPush(this->input->draft());
                    this->input->reset();
                    this->historyReset();
                    this->message("prompt stashed (" + std::to_string(this->manager.stashSize()) + ")", false);
                }
            } else if (action == "unstash") {
                const std::optional<cluster::Draft> text = this->manager.stashPop();
                if (!text) {
                    this->message("the stash is empty", false);
                } else {
                    if (!cluster::trim(this->input->text().toStdString()).empty()) this->manager.stashPush(this->input->draft());
                    this->input->loadDraft(*text);
                }
            } else if (action == "history") {
                // the prompts of the session shown; its last entry: every session
                static const QString every = "[every session…]";
                QStringList items;
                for (const std::string& text: this->manager.history(500, this->active))
                    items << q_(text);
                items << every;
                bool ok = false;
                QString item = QInputDialog::getItem(this, "History", q_("Prompt of " + this->active + " (type to filter)"), items, 0, true, &ok);
                if (ok && item == every) {
                    items.clear();
                    for (const std::string& text: this->manager.history())
                        items << q_(text);
                    item = items.isEmpty() ? QString() : QInputDialog::getItem(this, "History", "Prompt of every session (type to filter)", items, 0, true, &ok);
                }
                if (items.isEmpty()) {
                    this->message("no prompt yet", false);
                } else {
                    if (ok && !item.isEmpty()) {
                        const std::vector<cluster::Draft> drafts = this->manager.drafts();
                        auto it = std::find_if(drafts.begin(), drafts.end(), [&](const cluster::Draft& d) {return d.text == item.toStdString();});
                        this->input->loadDraft(it != drafts.end() ? *it : cluster::Draft{item.toStdString(), {}, {}});
                        this->historyReset();
                    }
                }
            } else if (action == "always") this->run({"always", "", "", "", false}, "");
            else if (action == "quit") this->close();
        }

        void escape(void)
        {
            if (this->app.voice->speaking()) {
                this->app.voice->mute();
                this->message("reading stopped", false);
                return;
            }
            if (!this->app.voice->pending().empty()) {
                this->app.voice->cancelLast(); // the last voice prompt waiting (Ctrl+C: all of them)
                return;
            }
            // a queued prompt of the session: back in the prompt to edit it (removed from the queue); else Esc Esc clears
            const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const std::optional<cluster::Queued> queued = this->manager.snapshot(this->active) ? this->manager.unqueue(this->active) : std::nullopt;
            if (queued) {
                if (!cluster::trim(this->input->text().toStdString()).empty()) this->manager.stashPush(this->input->draft());
                this->input->reset();
                this->historyReset();
                for (const std::string& image: queued->images) // its [Image #N] are in the text: atoms again, no new label
                    this->input->addImage(image, false);
                this->input->setText(q_(queued->text));
                this->message("queued prompt back to edit (Enter: queue it again)", false);
                this->lastEscape = 0;
            } else if (now - this->lastEscape < 600 && cluster::trim(this->input->text().toStdString()).empty()) {
                // Esc Esc on an empty prompt (like Claude Code): the last turn leaves the conversation, its prompt comes back
                this->lastEscape = 0;
                this->rewind();
            } else if (now - this->lastEscape < 600) {
                this->input->reset();
                this->historyReset();
                this->lastEscape = 0;
            } else {
                this->lastEscape = now;
            }
        }

        void find(const bool older)
        {
            // Ctrl+F: the text in the transcript of the active session (Enter: older, Shift+Enter: newer)
            SessionView* view = this->views.contains("") ? this->views[""] : this->views.contains(this->active) ? this->views[this->active] : nullptr;
            if (!view || this->findInput->text().isEmpty()) return;
            const QTextDocument::FindFlags flags = older ? QTextDocument::FindBackward : QTextDocument::FindFlags();
            if (!view->log->find(this->findInput->text(), flags)) {
                view->log->moveCursor(older ? QTextCursor::End : QTextCursor::Start);
                view->log->find(this->findInput->text(), flags);
            }
        }

        void rewind(void)
        {
            // the last turn leaves the conversation (claude started again: in the worker), its prompt comes back
            const std::string session = this->active;
            this->message("taking the last prompt back…", false);
            this->worker.push([this, session]() {
                std::optional<cluster::Queued> prompt;
                std::string error;
                try {
                    prompt = this->manager.rewind(session);
                } catch (const utils::exception::IException& e) {
                    error = e.info();
                }
                QMetaObject::invokeMethod(this, [this, prompt, error]() {
                    if (!prompt) {
                        this->message(error, true);
                        return;
                    }
                    if (!cluster::trim(this->input->text().toStdString()).empty()) this->manager.stashPush(this->input->draft());
                    this->input->loadDraft({prompt->text, prompt->images, {}});
                    this->historyReset();
                    this->message("the last prompt left the conversation: back here to edit (Esc Esc on an empty prompt: the one before)", false);
                    this->refresh(true);
                }, Qt::QueuedConnection);
            });
        }

        void send(void)
        {
            // the prompt leaves at once (feedback), the send / the local command runs in the worker, its result comes back
            // here; an error: the prompt back, nothing lost
            const std::string text = cluster::trim(this->input->plain());
            if (text.empty()) return;
            const cluster::Draft draft = this->input->draft();
            std::vector<std::string> images;
            for (const auto &[label, path]: this->input->completion.imagesIn(this->input->raw()))
                images.push_back(path);
            const std::string session = this->active;
            this->input->reset();
            this->historyReset();
            this->message(text.starts_with("/") ? cluster::split(text, ' ')[0] + "…" : "sent", false);
            this->worker.push([this, text, draft, images, session]() {
                std::optional<cluster::ActionResult> local;
                std::string error;
                try {
                    local = cluster::run_slash(this->manager, session, text);
                    if (!local) this->manager.send(session, text, false, images);
                    this->manager.addHistory(draft, session); // typed here: in the history (not the ones of the global session / MCP)
                } catch (const utils::exception::IException& e) {
                    error = std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info());
                }
                QMetaObject::invokeMethod(this, [this, local, error, draft]() {
                    if (!error.empty()) {
                        this->message(error, true);
                        if (cluster::trim(this->input->text().toStdString()).empty()) this->input->loadDraft(draft);
                        return;
                    }
                    if (local) {
                        if (!local->focus.empty()) this->active = local->focus;
                        if (local->ui == "settings") this->settings();
                        else if (local->ui == "restore_menu") this->openRestore();
                        else if (local->ui == "rewind") this->rewind();
                        if (!local->message.empty()) this->message(local->message, local->error);
                    }
                    this->refresh(true);
                }, Qt::QueuedConnection);
            });
        }

        void rebuild(void)
        {
            // Central widget for the current layout and set of sessions
            const std::vector<cluster::Snapshot> sessions = this->manager.list(true);
            const std::string layout = this->manager.ui("layout", this->manager.config().layout);
            std::string sig = layout;
            for (const cluster::Snapshot& s: sessions)
                sig += "|" + s.spec.id;
            if (sig == this->signature) return;
            this->signature = sig;
            this->applyStyle();
            this->views.clear();
            this->list = nullptr;
            this->tabs = nullptr;
            if (std::none_of(sessions.begin(), sessions.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->active;}) && !sessions.empty())
                this->active = sessions.front().spec.id;

            QWidget* central = new QWidget(this);
            QVBoxLayout* root = new QVBoxLayout(central);
            root->setContentsMargins(8, 8, 8, 8);
            if (layout == "grid" && sessions.size() > 1) {
                QGridLayout* grid = new QGridLayout();
                const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(sessions.size()))));
                for (std::size_t i = 0; i < sessions.size(); ++i) {
                    SessionView* view = new SessionView(this, sessions[i].spec.id, true);
                    this->views[sessions[i].spec.id] = view;
                    grid->addWidget(view, static_cast<int>(i) / columns, static_cast<int>(i) % columns);
                }
                root->addLayout(grid, 1);
            } else if (layout == "tabs") {
                this->tabs = new QTabBar(central);
                for (const cluster::Snapshot& s: sessions)
                    this->tabs->addTab(q_(s.spec.name));
                QObject::connect(this->tabs, &QTabBar::currentChanged, [this](int index) {
                    const std::vector<cluster::Snapshot> all = this->manager.list(true);
                    if (index >= 0 && static_cast<std::size_t>(index) < all.size()) this->active = all[static_cast<std::size_t>(index)].spec.id;
                    this->refresh(true);
                });
                root->addWidget(this->tabs);
                SessionView* view = new SessionView(this, "", false);
                this->views[""] = view;
                root->addWidget(view, 1);
            } else {
                QSplitter* splitter = new QSplitter(central);
                this->list = new QListWidget(splitter);
                this->list->setMinimumWidth(220);
                this->list->setWordWrap(true);
                this->list->setTextElideMode(Qt::ElideMiddle);
                this->list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                QObject::connect(this->list, &QListWidget::currentRowChanged, [this](int row) {
                    const std::vector<cluster::Snapshot> all = this->manager.list(true);
                    if (row >= 0 && static_cast<std::size_t>(row) < all.size() && all[static_cast<std::size_t>(row)].spec.id != this->active) {
                        this->active = all[static_cast<std::size_t>(row)].spec.id;
                        this->refresh(true);
                    }
                });
                SessionView* view = new SessionView(this, "", false);
                this->views[""] = view;
                splitter->addWidget(this->list);
                splitter->addWidget(view);
                splitter->setStretchFactor(1, 1);
                root->addWidget(splitter, 1);
            }
            this->voiceLabel = new QLabel(central);
            root->addWidget(this->voiceLabel);
            QHBoxLayout* bar = new QHBoxLayout();
            this->input = new PromptEdit(this, central);
            this->input->setPlaceholderText("Prompt (Enter to send, Shift / Alt+Enter new line) · @ file · / command · Ctrl+K palette · F1 keys");
            this->input->chipsBar = new QWidget(central);
            new QHBoxLayout(this->input->chipsBar);
            this->input->chipsBar->layout()->setContentsMargins(0, 0, 0, 0);
            this->input->chipsBar->setVisible(false);
            root->addWidget(this->input->chipsBar);
            this->findBar = new QWidget(central);
            QHBoxLayout* findLayout = new QHBoxLayout(this->findBar);
            findLayout->setContentsMargins(0, 0, 0, 0);
            findLayout->addWidget(new QLabel("Find:", this->findBar));
            this->findInput = new QLineEdit(this->findBar);
            this->findInput->setPlaceholderText("text of the transcript · Enter older · Shift+Enter newer · Esc close");
            findLayout->addWidget(this->findInput, 1);
            QObject::connect(this->findInput, &QLineEdit::textChanged, [this]() {this->find(true);});
            QObject::connect(this->findInput, &QLineEdit::returnPressed, [this]() {
                this->find(!(QApplication::keyboardModifiers() & Qt::ShiftModifier));
            });
            QShortcut* closeFind = new QShortcut(QKeySequence(Qt::Key_Escape), this->findInput, nullptr, nullptr, Qt::WidgetShortcut);
            QObject::connect(closeFind, &QShortcut::activated, [this]() {
                this->findBar->setVisible(false);
                this->input->setFocus();
            });
            this->findBar->setVisible(false);
            root->addWidget(this->findBar);
            this->input->popup = new QListWidget(central);
            this->input->popup->setFocusPolicy(Qt::NoFocus);
            this->input->popup->setVisible(false);
            this->target = new QLabel(central);
            QPushButton* sendButton = new QPushButton("Send", central);
            QObject::connect(sendButton, &QPushButton::clicked, [this]() {this->send();});
            this->input->vimLabel = new QLabel(central);
            this->input->vimLabel->setStyleSheet("font-weight:bold;");
            bar->addWidget(this->input, 1);
            bar->addWidget(this->input->vimLabel);
            bar->addWidget(this->target);
            bar->addWidget(sendButton);
            root->addLayout(bar);
            this->status = new QLabel(central);
            root->addWidget(this->status);
            this->setCentralWidget(central);
            this->input->setFocus();
            this->lastVersion = 0;
        }

        void refresh(const bool force)
        {
            this->rebuild();
            const std::uint64_t version = this->manager.version() + this->app.voice->version();
            if (!force && version == this->lastVersion && !this->animating) return;
            this->lastVersion = version;
            const std::vector<cluster::Snapshot> sessions = this->manager.list(true);
            const cluster::Style& st = this->theme();

            if (this->list) {
                const QSignalBlocker block(this->list);
                this->list->clear();
                for (const cluster::Snapshot& s: sessions) {
                    std::string line = std::string(s.state == cluster::State::Working ? "● " : s.state == cluster::State::Waiting ? "! " : "○ ") + s.spec.name
                        + "   " + cluster::human_tokens(s.metrics.total.total()) + (s.permissions.empty() ? "" : "  [" + std::to_string(s.permissions.size()) + " perm]")
                        + "\n   " + s.spec.backend.substr(0, s.spec.backend.find('/')) + " · " + cluster::short_path(s.spec.cwd);
                    this->list->addItem(q_(line));
                    if (s.spec.id == this->active) this->list->setCurrentRow(this->list->count() - 1);
                }
            }
            if (this->tabs) {
                const QSignalBlocker block(this->tabs);
                for (std::size_t i = 0; i < sessions.size() && static_cast<int>(i) < this->tabs->count(); ++i) {
                    this->tabs->setTabText(static_cast<int>(i), q_(sessions[i].spec.name + (sessions[i].permissions.empty() ? "" : " !")));
                    if (sessions[i].spec.id == this->active) this->tabs->setCurrentIndex(static_cast<int>(i));
                }
            }
            this->reveal.frameStart();
            bool working = false;
            for (const cluster::Snapshot& s: sessions) {
                working |= s.state == cluster::State::Working;
                auto it = this->views.find(s.spec.id);
                if (it != this->views.end()) {
                    it->second->setProperty("active", s.spec.id == this->active);
                    it->second->style()->unpolish(it->second);
                    it->second->style()->polish(it->second);
                    it->second->update(s, st, this->manager, true, this->reveal, this->expanded);
                } else if (s.spec.id == this->active && this->views.contains("")) {
                    this->views[""]->id = s.spec.id;
                    this->views[""]->update(s, st, this->manager, false, this->reveal, this->expanded);
                }
            }
            this->animating = working || this->reveal.active();
            this->target->setText(q_(this->active == GLOBAL_ID ? "→ global" : "→ " + this->active));

            // voice bar, last notice
            const cluster::Voice& voice = *this->app.voice;
            const cluster::VoiceConf& conf = this->manager.config().voice;
            QString voiceText;
            if (voice.listening()) voiceText = span_(st.ok, conf.mode == "wake" ? (voice.armed() ? "◉ armed: say the command" : q_("○ waiting for \"" + conf.wakeWord + "\"")) : "◉ listening", true);
            if (voice.listening() && !voice.ready()) voiceText = span_(st.warn, "… loading the speech model (~10 s)", true);
            const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            for (const cluster::Pending& p: voice.pending()) {
                const std::string when = p.paused ? " (paused while you speak)" : p.deadline > 0 ? " (" + std::to_string(std::max<std::int64_t>(0, (p.deadline - now + 999) / 1000)) + "s)" : "";
                const std::string said = p.target.empty() ? cluster::tail_utf8(p.text, 160) : p.text; // live: its end
                voiceText += "  " + esc_((p.target.empty() ? "" : "→ " + (p.target == GLOBAL_ID ? std::string("global") : p.target) + ": ") + said + when);
            }
            if (voice.speaking()) voiceText += "  " + span_(st.muted, "speaking (" + q_(conf.wakeWord.empty() ? "" : "") + q_(this->manager.config().keys.at("mute")) + " mute)");
            // the web access: its address and code shown while it is open
            const std::string web = this->manager.web() ? cluster::web_line(this->manager.web()->status()) : "";
            if (!web.empty()) voiceText = span_(st.accent, esc_(web), true) + (voiceText.isEmpty() ? QString() : "    " + voiceText);
            this->voiceLabel->setText(voiceText);
            this->voiceLabel->setVisible(!voiceText.isEmpty());
            const std::vector<cluster::Notice> notices = this->manager.notices(1);
            if (!notices.empty() && notices.back().time > this->statusTime)
                this->status->setText(span_(notices.back().error ? st.error : st.muted, esc_(cluster::one_line(notices.back().text, 200))));
        }

        std::uint64_t focusSeen = 0;            // last ui_focus applied
        cluster::Worker worker;                 // the sends / commands, in their order, out of the window thread
        QDialog* scanDialog = nullptr;          // the progress of the map of this computer
        QProgressBar* scanBar = nullptr;
        QLabel* scanText = nullptr;

        void scanProgress(void)
        {
            // the scan running (then 4 s): its bar, its steps checked, the last lines, the one running, the next one
            const std::optional<std::pair<cluster::MachineProgress, std::int64_t>> scan = this->manager.machineProgress();
            const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            const bool shown = scan && (scan->second == 0 || now - scan->second < 4000);
            if (!shown) {
                if (this->scanDialog) this->scanDialog->hide();
                return;
            }
            if (!this->scanDialog) {
                this->scanDialog = new QDialog(this);
                this->scanDialog->setWindowTitle("Map of this computer");
                this->scanDialog->resize(560, 220);
                QVBoxLayout* layout = new QVBoxLayout(this->scanDialog);
                this->scanBar = new QProgressBar(this->scanDialog);
                this->scanText = new QLabel(this->scanDialog);
                layout->addWidget(this->scanBar);
                layout->addWidget(this->scanText);
            }
            const cluster::MachineProgress& p = scan->first;
            this->scanBar->setValue(static_cast<int>(p.fraction * 100));
            QString text;
            for (const auto &[name, state]: p.steps)
                text += q_((state == 2 ? "✓ " : state == 1 ? "◌ " : "· ") + name + "    ");
            text += "<br><br>";
            for (const std::string& line: p.log)
                text += "✓ " + esc_(line) + "<br>";
            if (!p.now.empty()) text += "<b>◌ " + esc_(p.now) + "</b><br>";
            if (!p.next.empty()) text += "· next: " + esc_(p.next);
            this->scanText->setText(text);
            if (!this->scanDialog->isVisible()) this->scanDialog->show();
        }

        void askMachine(const int startup)
        {
            // the map of this computer: what is read and stored, yes / no (at start: 1 no map: no is never asked again,
            // 2 an old one: proposed again machine_days later); yes: the questions about the user still empty, the scan
            const std::string title = startup == 2 ? "The map of this computer is " + std::to_string(cluster::machine_days_old()) + " days old: sync it?\n\n"
                : startup == 3 && cluster::machine_days_old() >= 0 ? "Sync the map of this computer? (last " + cluster::machine_age() + ")\n\n" : "";
            QMessageBox box(QMessageBox::Question, "claude-cluster", q_(title + cluster::machine_scope()), QMessageBox::Yes | QMessageBox::No, this);
            box.setDefaultButton(QMessageBox::Yes);
            if (box.exec() != QMessageBox::Yes) {
                if (startup == 2) this->manager.machineLater();
                else if (startup == 1) this->manager.machineDecline();
                return;
            }
            for (const cluster::ProfileQuestion& q: cluster::machine_questions(this->manager.config())) {
                bool ok = false;
                QString answer;
                if (q.choices.empty()) {
                    answer = QInputDialog::getText(this, "About you (for the global session)", q_(q.question), QLineEdit::Normal, q_(q.value), &ok);
                } else {
                    QStringList items;
                    for (const std::string& choice: q.choices)
                        items << q_(choice);
                    answer = QInputDialog::getItem(this, "About you (for the global session)", q_(q.question), items,
                        std::max<qsizetype>(0, items.indexOf(q_(q.value))), true, &ok);
                }
                if (ok) (void)cluster::settings_set(this->manager.config(), "user", q.key, answer.trimmed().toStdString());
            }
            (void)this->manager.config().reload();
            this->manager.scanMachine();
            this->message("mapping this computer…", false);
        }

        void settingAction(const std::string& key)
        {
            // an action of the settings: the map of this computer, the system prompt of the global session or of the
            // sub-sessions (edited here)
            if (key == "machine_sync") {
                this->askMachine(0);
                return;
            }
            if (key != "edit_prompt" && key != "edit_sessions_prompt" && key != "edit_vocabulary" && key != "edit_words") return;
            const bool global = key == "edit_prompt";
            const bool vocabulary = key == "edit_vocabulary" || key == "edit_words"; // a list of the voice: no restart
            const cluster::Config& config = this->manager.config();
            const std::filesystem::path path = cluster::expand_home(global ? config.globalPrompt : key == "edit_words" ? cluster::voice_words_path(config.voice.words).string() : vocabulary ? cluster::vocabulary_path(config.voice.vocabulary).string() : config.sessionsPrompt);
            std::ifstream in(path);
            const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            bool ok = false;
            const QString edited = QInputDialog::getMultiLineText(this, global ? "System prompt of the global session" : vocabulary ? "Vocabulary of the voice"
                : "System prompt of the sub-sessions", q_(cluster::short_path(path.string()) + (vocabulary ? " (name = how it is heard, ...)" : " (empty: none)")), q_(text), &ok);
            if (!ok || edited.toStdString() == text) return;
            std::ofstream(path) << edited.toStdString();
            if (vocabulary) {
                this->message("vocabulary saved: used from the next sentence heard", false);
                return;
            }
            if (!global) {
                this->message("system prompt of the sub-sessions saved: taken by each one at its next start (/clear, /model...)", false);
                return;
            }
            try {
                this->manager.restartGlobal();
                this->message("system prompt saved: the global session started again (same conversation)", false);
            } catch (const utils::exception::IException& e) {
                this->message(std::string("system prompt saved, the global session not restarted: ") + e.info(), true);
            }
        }
        std::string styleSeen;
        std::set<std::string> asked;        // the questions of the agents opened once (declined: not again)
        bool asking = false;                // a question dialog is open
        std::uint64_t machineSeen = 0;      // the map of this computer asked by the global session (its question shown)

        void askCheck(void)
        {
            // a question of the agent of the session shown (AskUserQuestion): its dialog, like Claude Code / Claude Desktop
            if (this->asking) return;
            if (this->manager.machineRequest() != this->machineSeen) { // the global session asked to map this computer again
                this->machineSeen = this->manager.machineRequest();
                this->asking = true;
                QTimer::singleShot(0, this, [this]() {
                    this->askMachine(0);
                    this->asking = false;
                });
                return;
            }
            const std::optional<cluster::Snapshot> snap = this->manager.snapshot(this->active);
            if (!snap) return;
            for (const cluster::Permission& p: snap->permissions) {
                if (p.tool != "AskUserQuestion" || this->asked.contains(p.requestId) || !p.input.contains("questions") || !p.input["questions"].is_array()) continue;
                this->asked.insert(p.requestId);
                this->asking = true;
                QTimer::singleShot(0, this, [this, id = snap->spec.id, name = snap->spec.name, request = p.requestId, questions = p.input["questions"]]() {
                    this->askQuestions(id, name, request, questions);
                    this->asking = false;
                });
                return;
            }
        }

        void askQuestions(const std::string& id, const std::string& name, const std::string& request, const cluster::Json& questions)
        {
            // each question: its options (radio, or check boxes for a multi choice) and Other (typed); Decline: denied
            struct Row {
                cluster::Json question;
                std::vector<QAbstractButton*> options;
                QAbstractButton* other = nullptr;
                QLineEdit* text = nullptr;
            };
            QDialog dialog(this);
            dialog.setWindowTitle(q_(name + " asks"));
            dialog.setMinimumWidth(560);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            std::vector<Row> rows;
            for (const cluster::Json& q: questions) {
                const bool multi = q.value("multiSelect", false);
                QLabel* title = new QLabel(q_((q.value("header", std::string()).empty() ? "" : q.value("header", std::string()) + "  ·  ") + q.value("question", std::string())), &dialog);
                title->setWordWrap(true);
                title->setStyleSheet("font-weight: bold; margin-top: 8px;");
                layout->addWidget(title);
                QButtonGroup* group = new QButtonGroup(&dialog);
                group->setExclusive(!multi);
                Row row{q, {}, nullptr, nullptr};
                for (const cluster::Json& option: q.value("options", cluster::Json::array())) {
                    const std::string description = option.value("description", std::string());
                    const QString label = q_(option.value("label", std::string()) + (description.empty() ? "" : "   -  " + description));
                    QAbstractButton* button = multi ? static_cast<QAbstractButton*>(new QCheckBox(label, &dialog)) : new QRadioButton(label, &dialog);
                    group->addButton(button);
                    layout->addWidget(button);
                    row.options.push_back(button);
                }
                QHBoxLayout* other = new QHBoxLayout();
                row.other = multi ? static_cast<QAbstractButton*>(new QCheckBox("Other:", &dialog)) : new QRadioButton("Other:", &dialog);
                row.text = new QLineEdit(&dialog);
                group->addButton(row.other);
                QObject::connect(row.text, &QLineEdit::textEdited, row.other, [button = row.other]() {button->setChecked(true);});
                other->addWidget(row.other);
                other->addWidget(row.text, 1);
                layout->addLayout(other);
                if (!multi && !row.options.empty()) row.options.front()->setChecked(true);
                rows.push_back(row);
            }
            QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            buttons->button(QDialogButtonBox::Ok)->setText("Answer");
            buttons->button(QDialogButtonBox::Cancel)->setText("Decline");
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);
            if (dialog.exec() != QDialog::Accepted) {
                try {
                    this->manager.answer(id, request, "deny");
                    this->message("question declined", false);
                } catch (const utils::exception::IException& e) {
                    this->message(e.info(), true);
                }
                return;
            }
            cluster::Json answers = cluster::Json::object();
            for (const Row& row: rows) {
                std::string value;
                const cluster::Json options = row.question.value("options", cluster::Json::array());
                for (std::size_t i = 0; i < row.options.size(); ++i)
                    if (row.options[i]->isChecked()) value += (value.empty() ? "" : ", ") + options[i].value("label", std::string());
                const std::string typed = cluster::trim(row.text->text().toStdString());
                if (row.other->isChecked() && !typed.empty()) value += (value.empty() ? "" : ", ") + typed;
                answers[row.question.value("question", std::string())] = value;
            }
            try {
                this->manager.answerQuestions(id, request, answers);
                this->message("answered", false);
            } catch (const utils::exception::IException& e) {
                this->message(e.info(), true); // answered elsewhere meanwhile (MCP...)
            }
        }
        std::int64_t lastInterrupt = 0;     // Ctrl+C Ctrl+C on an empty prompt, nothing running: quit

        void tick(void)
        {
            if (cluster::quit_requested()) { // Ctrl+C in the terminal, kill, the terminal closed: the normal quit
                this->close();
                return;
            }
            this->scanProgress();
            // the global session asked to show a session (ui_focus) or another style (ui_style)
            const std::pair<std::uint64_t, std::string> focus = this->manager.focusRequest();
            if (focus.first != this->focusSeen) {
                this->focusSeen = focus.first;
                if (this->manager.snapshot(focus.second)) {
                    this->active = focus.second;
                    this->signature.clear();
                }
            }
            this->manager.noteActive(this->active);
            this->askCheck();
            const std::string style = this->manager.ui("style", this->manager.config().style);
            if (style != this->styleSeen) {
                this->styleSeen = style;
                this->applyStyle();
                this->signature.clear();
            }
            if (this->manager.config().reload()) {
                this->bindKeys();
                this->signature.clear();
            }
            if (this->input) this->input->vimSync();
            this->refresh(false);
        }
};

PromptEdit::PromptEdit(Window* owner, QWidget* parent)
    : QPlainTextEdit(parent), window(owner)
{
    this->completion.setProvider(cluster::completion_provider(owner->manager, [owner]() {return owner->active;}));
    this->setTabChangesFocus(false);
    this->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    new AtomHighlighter(this);
    QObject::connect(this, &QPlainTextEdit::textChanged, [this]() {
        this->refreshCompletion();
        this->fit();
        this->refreshChips();
    });
    QObject::connect(this, &QPlainTextEdit::cursorPositionChanged, [this]() {
        // never inside a label (click, vim): to its end
        QTextCursor cursor = this->textCursor();
        const std::optional<std::pair<int, int>> inside = this->atomNear(cursor.position(), 0);
        if (inside) {
            cursor.setPosition(inside->second, cursor.hasSelection() ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
            this->setTextCursor(cursor);
            return;
        }
        this->refreshCompletion();
    });
    this->fit();
}

void PromptEdit::fit(void)
{
    const int lines = std::clamp(static_cast<int>(this->document()->size().height()), 1, 8);
    this->setFixedHeight(lines * this->fontMetrics().lineSpacing() + 2 * this->frameWidth() + static_cast<int>(2 * this->document()->documentMargin()) + 4);
}

void PromptEdit::refreshCompletion(void)
{
    // the completion of the shared editor on the text of Qt (UTF-8 bytes <-> UTF-16 positions)
    const std::string text = this->text().toStdString();
    const std::size_t cursor = this->text().left(this->cursorPosition()).toStdString().size();
    this->completion.sync(text, cursor);
    if (!this->popup) return;
    if (!this->completion.completing()) {
        this->popup->setVisible(false);
        return;
    }
    const std::string prefix = this->completion.kind() == cluster::CompletionKind::File ? "@" : this->completion.kind() == cluster::CompletionKind::Command ? "/" : "";
    this->popup->clear();
    for (const cluster::Candidate& c: this->completion.items())
        this->popup->addItem(q_(prefix + c.label + "    " + cluster::one_line(c.description, 80)));
    this->popup->setCurrentRow(this->completion.selectedItem());
    const int rows = std::min(8, this->popup->count());
    const int height = rows * (this->popup->sizeHintForRow(0) + 2) + 6;
    const QPoint at = this->mapTo(this->popup->parentWidget(), QPoint(0, 0));
    this->popup->setGeometry(at.x(), at.y() - height - 2, this->width(), height);
    this->popup->raise();
    this->popup->setVisible(true);
}

void PromptEdit::apply(const bool final)
{
    // the word replaced through a selection + insert: the undo of Qt keeps working
    if (!this->completion.completing()) return;
    const cluster::Candidate candidate = this->completion.items()[static_cast<std::size_t>(this->completion.selectedItem())];
    const std::string text = this->text().toStdString();
    const int start = static_cast<int>(QString::fromStdString(text.substr(0, this->completion.completionStart())).size());
    this->setSelection(start, this->cursorPosition() - start);
    this->insert(q_(candidate.value + (final && !candidate.directory ? " " : "")));
    if (final) {
        this->completion.dismiss();
        this->popup->setVisible(false);
    }
}

std::optional<std::pair<int, int>> PromptEdit::atomNear(const int pos, const int where) const
{
    // a label of an atom: ending at pos (-1), starting at pos (1), or around pos (0); <start, end> in UTF-16
    const QString text = this->text();
    for (std::size_t i = 0; i < this->completion.atomCount(); ++i) {
        const QString label = QString::fromStdString(this->completion.atom(i).label);
        const int size = static_cast<int>(label.size());
        for (int at = text.indexOf(label); at >= 0; at = text.indexOf(label, at + 1)) {
            if ((where < 0 && at + size == pos) || (where > 0 && at == pos) || (where == 0 && at < pos && pos < at + size))
                return std::make_pair(at, at + size);
        }
    }
    return std::nullopt;
}

void PromptEdit::insertFromMimeData(const QMimeData* source)
{
    if (source->hasText() && !source->hasImage()) {
        this->insertPlainText(QString::fromStdString(this->completion.shown(this->completion.newPaste(source->text().toStdString()))));
        return;
    }
    QPlainTextEdit::insertFromMimeData(source);
}

bool PromptEdit::addImage(const std::string& path, const bool insert)
{
    // the same image (file named by its content) again: one more reference to it, one image sent
    const std::string label = this->completion.shown(this->completion.newImage(path));
    const bool known = this->text().contains(q_(label));
    if (insert) this->insert(q_(label + " "));
    this->refreshChips();
    return !known;
}

void PromptEdit::refreshChips(void)
{
    // one button per atom of the prompt, dim when out of the text: click puts it back, right click opens the image,
    // hover shows the image / the pasted text
    if (!this->chipsBar) return;
    QLayoutItem* item = nullptr;
    while ((item = this->chipsBar->layout()->takeAt(0))) {
        if (item->widget()) item->widget()->deleteLater(); // a chip may be the one being clicked
        delete item;
    }
    const QString text = this->text();
    const std::vector<std::size_t> atoms = this->completion.promptAtoms();
    for (const std::size_t index: atoms) {
        const cluster::Atom& atom = this->completion.atom(index);
        const QString label = q_(atom.label);
        const bool in = text.contains(label);
        QPushButton* chip = new QPushButton(label + (atom.image.empty() ? QString() : " " + q_(std::filesystem::path(atom.image).filename().string())), this->chipsBar);
        chip->setFlat(!in);
        chip->setStyleSheet(in ? "padding:2px 6px;" : "padding:2px 6px;color:palette(mid);");
        if (atom.image.empty()) {
            QStringList lines = q_(atom.content).split('\n');
            const qsizetype total = lines.size();
            if (total > 40) lines = lines.mid(0, 40);
            chip->setToolTip("<pre>" + lines.join('\n').toHtmlEscaped() + (total > 40 ? "\n… " + QString::number(total - 40) + " more line(s)" : QString()) + "</pre>");
        } else {
            chip->setToolTip("<img src=\"" + q_(atom.image) + "\" width=\"520\"><br>click: in the prompt · right click: open");
            chip->setContextMenuPolicy(Qt::CustomContextMenu);
            const std::string image = atom.image;
            QObject::connect(chip, &QPushButton::customContextMenuRequested, [this, image](const QPoint&) {
                const std::string viewer = cluster::trim(this->window->manager.config().imageViewer);
                if (viewer.empty()) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(q_(image)));
                } else {
                    std::vector<std::string> command = cluster::split(viewer, ' ');
                    command.push_back(image);
                    cluster::detach(command);
                }
            });
        }
        QObject::connect(chip, &QPushButton::clicked, [this, index, label]() {
            this->insert(label + (this->completion.atom(index).image.empty() ? QString() : QString(" ")));
            this->setFocus();
        });
        this->chipsBar->layout()->addWidget(chip);
    }
    static_cast<QHBoxLayout*>(this->chipsBar->layout())->addStretch(1);
    this->chipsBar->setVisible(!atoms.empty());
}

void PromptEdit::setText(const QString& text)
{
    this->setPlainText(text);
    this->moveCursor(QTextCursor::End);
    this->completion.dismiss(); // the arrows go on browsing; it opens again when the word is changed
    if (this->popup) this->popup->setVisible(false);
}

void PromptEdit::loadDraft(const cluster::Draft& draft)
{
    // its images (labels in the order of the text) and its pasted texts as blocks
    this->reset();
    for (const std::string& image: draft.images)
        this->addImage(image, false);
    std::string text = draft.text;
    for (const std::string& paste: draft.pastes) {
        const std::size_t at = text.find(paste);
        if (at != std::string::npos) text.replace(at, paste.size(), this->completion.shown(this->completion.pasteAtom(paste)));
    }
    this->setText(q_(text));
}

void PromptEdit::reset(void)
{
    this->clear();
    this->completion.clear();
    this->refreshChips();
    if (this->popup) this->popup->setVisible(false);
}

void PromptEdit::vimSync(void)
{
    const bool on = this->window->manager.ui("vim", this->window->manager.config().vim ? "true" : "false") == "true";
    this->completion.setVim(on);
    this->completion.setPasteLimits(this->window->manager.config().pasteLines, this->window->manager.config().pasteChars);
    const cluster::VimMode vim = this->completion.vim();
    const bool block = vim != cluster::VimMode::Off && vim != cluster::VimMode::Insert;
    this->setCursorWidth(block ? std::max(2, this->fontMetrics().horizontalAdvance('M')) : 1);
    if (!this->vimLabel) return;
    static const std::map<cluster::VimMode, QString> names = {{cluster::VimMode::Insert, "-- INSERT --"}, {cluster::VimMode::Normal, "-- NORMAL --"},
        {cluster::VimMode::Visual, "-- VISUAL --"}, {cluster::VimMode::VisualLine, "-- V-LINE --"}};
    this->vimLabel->setVisible(vim != cluster::VimMode::Off);
    if (vim != cluster::VimMode::Off) this->vimLabel->setText(names.at(vim) + " " + q_(this->completion.vimPending()));
}

bool PromptEdit::vimKey(QKeyEvent* event)
{
    // the shared editor runs the vim command on the text of Qt, the result goes back as one edit (undo of Qt kept)
    this->vimSync();
    const cluster::VimMode vim = this->completion.vim();
    if (vim == cluster::VimMode::Off) return false;
    const bool escape = event->key() == Qt::Key_Escape;
    if (vim == cluster::VimMode::Insert && !escape) return false;
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        if (vim != cluster::VimMode::Insert && event->key() == Qt::Key_R && event->modifiers() == Qt::ControlModifier) {
            this->redo();
            return true;
        }
        return false; // the shortcuts
    }
    if (vim != cluster::VimMode::Insert && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        (void)this->completion.vimKey("Escape");
        this->window->send();
        return true;
    }
    static const std::map<int, std::string> keys = {{Qt::Key_Left, "h"}, {Qt::Key_Right, "l"}, {Qt::Key_Up, "k"}, {Qt::Key_Down, "j"},
        {Qt::Key_Backspace, "h"}, {Qt::Key_Home, "0"}, {Qt::Key_End, "$"}, {Qt::Key_Delete, "x"}, {Qt::Key_Escape, "Escape"}};
    std::string key = keys.contains(event->key()) ? keys.at(event->key()) : event->text().toStdString();
    if (key.empty() || (key.size() == 1 && static_cast<unsigned char>(key[0]) < 0x20 && key != "Escape")) return vim != cluster::VimMode::Insert;
    if (key == "u" && vim == cluster::VimMode::Normal && this->completion.vimPending().empty()) {
        this->undo();
        return true;
    }
    const QString before = this->text();
    this->completion.sync(this->raw(), this->completion.atomize(before.left(this->cursorPosition()).toStdString()).size());
    const bool used = this->completion.vimKey(key);
    const std::string after = this->completion.text();
    const QString text = QString::fromStdString(this->completion.shown(after));
    const std::function<int(std::size_t)> utf16 = [this, &after](std::size_t at) {
        return static_cast<int>(QString::fromStdString(this->completion.shown(after.substr(0, std::min(at, after.size())))).size());
    };
    QTextCursor cursor = this->textCursor();
    if (text != before) {
        cursor.beginEditBlock();
        cursor.select(QTextCursor::Document);
        cursor.insertText(text);
        cursor.endEditBlock();
    }
    const std::pair<std::size_t, std::size_t> sel = this->completion.selection();
    const cluster::VimMode now = this->completion.vim();
    if ((now == cluster::VimMode::Visual || now == cluster::VimMode::VisualLine) && sel.first != sel.second) {
        cursor.setPosition(utf16(sel.first));
        cursor.setPosition(utf16(sel.second), QTextCursor::KeepAnchor);
    } else {
        cursor.setPosition(utf16(this->completion.cursor()));
    }
    this->setTextCursor(cursor);
    if (now != cluster::VimMode::Insert && this->popup) this->popup->setVisible(false);
    this->vimSync();
    return used;
}

void PromptEdit::keyPressEvent(QKeyEvent* event)
{
    if (!(this->popup && this->popup->isVisible()) && this->vimKey(event)) return;
    {
        // an atom ([Image #N], [Pasted text #N]) is one character: removed / crossed at once
        QTextCursor cursor = this->textCursor();
        const bool shift = event->modifiers() & Qt::ShiftModifier;
        const int key = event->key();
        std::optional<std::pair<int, int>> atom;
        if (!cursor.hasSelection() && key == Qt::Key_Backspace) atom = this->atomNear(cursor.position(), -1);
        else if (!cursor.hasSelection() && key == Qt::Key_Delete) atom = this->atomNear(cursor.position(), 1);
        if (atom) {
            cursor.setPosition(atom->first);
            cursor.setPosition(atom->second, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            this->setTextCursor(cursor);
            return;
        }
        if ((key == Qt::Key_Left || key == Qt::Key_Right) && !(event->modifiers() & Qt::ControlModifier)) {
            atom = this->atomNear(cursor.position(), key == Qt::Key_Left ? -1 : 1);
            if (atom) {
                cursor.setPosition(key == Qt::Key_Left ? atom->first : atom->second, shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
                this->setTextCursor(cursor);
                return;
            }
        }
    }
    const bool open = this->popup && this->popup->isVisible();
    const bool enter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
    if (enter && (event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier))) {
        this->insertPlainText("\n"); // Alt+Enter / Shift+Enter: new line
        return;
    }
    if (enter && !open) {
        this->window->send();
        return;
    }
    if (!open && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && !(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
        // on the first / last line of the prompt: the history (never looped)
        QTextCursor probe = this->textCursor();
        if (!probe.movePosition(event->key() == Qt::Key_Up ? QTextCursor::Up : QTextCursor::Down) && this->window->historyStep(event->key() == Qt::Key_Up ? 1 : -1)) return;
    }
    if (event->key() == Qt::Key_Backtab) {
        this->window->shortcut("mode_cycle");
        return;
    }
    if (event->key() == Qt::Key_Escape && !open) {
        this->window->escape();
        return;
    }
    if (open) {
        if (event->key() == Qt::Key_Tab) {
            this->apply(false);
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            this->apply(true);
            return;
        }
        if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
            this->completion.select(event->key() == Qt::Key_Up ? -1 : 1);
            this->popup->setCurrentRow(this->completion.selectedItem());
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            this->completion.dismiss();
            this->popup->setVisible(false);
            return;
        }
        if (event->key() == Qt::Key_Space && !(this->cursorPosition() > 0 && this->text().at(this->cursorPosition() - 1) == '\\')) {
            this->completion.dismiss();
            this->popup->setVisible(false);
        }
    }
    if (event->key() == Qt::Key_Tab) return;
    if (event->matches(QKeySequence::Copy) && !this->hasSelectedText()) {
        this->window->shortcut("interrupt"); // Ctrl+C without selection: stop the turn, else clear the prompt
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        // an image of the clipboard: saved, [Image #N] in the prompt, previewed on hover of its chip
        const QMimeData* mime = QApplication::clipboard()->mimeData();
        if (mime && mime->hasImage()) {
            const QImage image = qvariant_cast<QImage>(mime->imageData());
            std::error_code error;
            std::filesystem::create_directories(cluster::data_dir() / "images", error);
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            if (!image.isNull() && image.save(&buffer, "PNG")) {
                // named by its content: the same image pasted again is the same [Image #N]
                const std::filesystem::path path = cluster::data_dir() / "images" / ("img-" + cluster::content_hash(bytes.toStdString()) + ".png");
                if (!std::filesystem::exists(path, error)) std::ofstream(path, std::ios::binary) << bytes.toStdString();
                if (!this->addImage(path.string())) this->window->message("the same image: one more reference to it (sent once)", false);
                return;
            }
        }
    }
    QPlainTextEdit::keyPressEvent(event);
}

SessionView::SessionView(Window* window, const std::string& sessionId, const bool compact)
    : QFrame(window), id(sessionId)
{
    this->setObjectName("session");
    QVBoxLayout* layout = new QVBoxLayout(this);
    this->header = new QLabel(this);
    this->header->setTextFormat(Qt::RichText);
    this->log = new QTextBrowser(this);
    this->log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    this->log->setOpenLinks(false);
    QObject::connect(this->log, &QTextBrowser::anchorClicked, [](const QUrl& url) {QDesktopServices::openUrl(url);});
    QObject::connect(this->log, &QTextBrowser::highlighted, [](const QUrl& url) {
        if (url.isLocalFile() && !QImage(url.toLocalFile()).isNull()) QToolTip::showText(QCursor::pos(), "<img src=\"" + url.toLocalFile() + "\" width=\"420\">");
        else QToolTip::hideText();
    });
    this->panels = new QLabel(this);
    this->panels->setObjectName("panels");
    this->panels->setTextFormat(Qt::RichText);
    this->panels->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    this->panels->setFixedWidth(280);
    this->panels->setVisible(!compact);

    this->permBar = new QWidget(this);
    this->permBar->setObjectName("perm");
    QHBoxLayout* perm = new QHBoxLayout(this->permBar);
    this->permLabel = new QLabel(this->permBar);
    perm->addWidget(this->permLabel, 1);
    for (const char* behavior: {"allow", "always", "deny"}) {
        QPushButton* button = new QPushButton(QString(behavior).replace(0, 1, QString(behavior).at(0).toUpper()), this->permBar);
        const std::string name = behavior;
        QObject::connect(button, &QPushButton::clicked, [window, this, name]() {
            try {
                window->manager.answer(this->id, "", name);
            } catch (const utils::exception::IException& e) {
                window->message(e.info(), true);
            }
        });
        perm->addWidget(button);
    }
    this->permBar->setVisible(false);

    QHBoxLayout* body = new QHBoxLayout();
    body->addWidget(this->log, 1);
    body->addWidget(this->panels);
    layout->addWidget(this->header);
    layout->addLayout(body, 1);
    layout->addWidget(this->permBar);
}

void SessionView::update(const cluster::Snapshot& s, const cluster::Style& st, cluster::Manager& manager, const bool compact, cluster::Reveal& reveal, const bool expanded)
{
    static cluster::Git git; // lives as long as the program (its refresh threads)
    const cluster::Metrics& m = s.metrics;
    this->id = s.spec.id;

    // header
    const std::string model = s.modelUsed.empty() ? s.spec.backend : s.spec.backend.substr(0, s.spec.backend.find('/')) + "/" + s.modelUsed;
    QString head = span_(st.accent, esc_(s.spec.name), true) + "  " + span_(s.state == cluster::State::Error ? st.error : st.text, cluster::state_name(s.state))
        + "  " + span_(st.muted, esc_(model + "  ·  mode " + s.spec.mode
        + (s.spec.effort.empty() || s.spec.effort == "default" ? "" : "  ·  effort " + s.spec.effort) + (compact ? "" : "  ·  " + cluster::short_path(s.spec.cwd))));
    if (!s.lastError.empty()) head += "<br>" + span_(st.error, esc_(cluster::one_line(s.lastError, 200)));
    if (!s.remoteUrl.empty() || !s.bridgeState.empty())
        head += "<br>" + span_(st.accent, "Remote Control " + esc_(s.bridgeState.empty() ? std::string("on") : s.bridgeState) + (s.remoteUrl.empty() ? QString()
            : ": <a href=\"" + q_(s.remoteUrl) + "\">" + esc_(s.remoteUrl) + "</a>")) + span_(st.muted, "  (live on claude.ai and here)");
    this->header->setText(head);

    // transcript (rebuilt only when it changed)
    const cluster::Config& config = manager.config();
    const cluster::Frame frame = reveal.step(s, config.typewriter, config.typewriterSpeed);
    const std::int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const std::string key = std::to_string(frame.visible) + "|" + std::to_string(frame.typedBytes) + "|" + std::to_string(frame.partialBytes) + "|" + std::to_string(s.thinking.size())
        + "|" + cluster::state_name(s.state) + "|" + st.name + "|" + (expanded ? "e" : "") + (s.state == cluster::State::Working ? "|" + std::to_string(ms / 150) : "");
    if (key != this->lastKey) {
        this->lastKey = key;
        QString html;
        const std::size_t start = s.entries.size() > 400 ? s.entries.size() - 400 : 0;
        const QString more = " (" + q_(config.keys.contains("expand") ? config.keys.at("expand") : std::string("Ctrl+O")) + " to expand)";
        const std::function<QString(const std::string&)> pre = [](const std::string& text) {return "<pre style=\"margin:0;white-space:pre-wrap\">" + esc_(text) + "</pre>";};
        for (std::size_t i = start; i < frame.visible && i < s.entries.size(); ++i) {
            const cluster::Entry& e = s.entries[i];
            const std::string text = i == frame.typed ? e.text.substr(0, frame.typedBytes) : e.text;
            switch (e.kind) {
                case cluster::EntryKind::User: {
                    // [Image: path] -> link, previewed on hover
                    QString body;
                    static const std::regex image(R"(\[Image: ([^\]]+)\])");
                    std::string rest = text.starts_with("[voice] ") ? text.substr(8) : text; // the mark is for the agent
                    std::smatch match;
                    while (std::regex_search(rest, match, image)) {
                        body += esc_(match.prefix().str()) + "<a href=\"" + QUrl::fromLocalFile(q_(match[1].str())).toString() + "\">[Image: "
                            + esc_(std::filesystem::path(match[1].str()).filename().string()) + "]</a>";
                        rest = match.suffix().str();
                    }
                    body += esc_(rest);
                    html += "<p>" + span_(st.accent, "› " + body, true) + "</p>";
                    break;
                }
                case cluster::EntryKind::Assistant: html += "<p>" + span_(st.text, "⏺ " + esc_(text)) + "</p>"; break;
                case cluster::EntryKind::Thinking:
                    // like Claude Code: folded, Ctrl+O shows it
                    if (compact) break;
                    html += "<div style=\"margin-top:10px\">" + span_(st.muted, "<i>∴ Thinking" + (expanded ? QString("…") : more) + "</i>") + "</div>";
                    if (expanded) html += "<div style=\"margin-left:16px\">" + span_(st.muted, "<i>" + pre(text) + "</i>") + "</div>";
                    break;
                case cluster::EntryKind::Tool:
                    if (e.tool == "TodoWrite") html += "<div style=\"margin-top:10px\">" + span_(st.ok, "⏺ <b>Update Todos</b>") + "</div><div style=\"margin-left:16px\">" + span_(st.text, pre(text)) + "</div>";
                    else html += "<div style=\"margin-top:10px\">" + span_(st.ok, "⏺ <b>" + esc_(e.tool) + "</b>") + span_(st.text, "(" + esc_(text) + ")") + "</div>";
                    break;
                case cluster::EntryKind::ToolResult: {
                    // the first lines (Ctrl+O: all)
                    if (compact) break;
                    QStringList parts = q_(text).split('\n');
                    const qsizetype total = parts.size();
                    if (!expanded && total > 3) parts = parts.mid(0, 3);
                    const QString shown = parts.join('\n').trimmed().isEmpty() ? QString("(no output)") : parts.join('\n');
                    html += "<div style=\"margin-left:12px\">" + span_(st.muted, pre(("⎿  " + shown).replace("\n", "\n   ").toStdString())) + "</div>";
                    if (total > parts.size()) html += "<div style=\"margin-left:12px\">" + span_(st.muted, pre("   … +" + std::to_string(total - parts.size()) + " lines" + more.toStdString())) + "</div>";
                    break;
                }
                case cluster::EntryKind::System: html += "<div style=\"margin-top:10px\">" + span_(st.muted, "· " + esc_(text)) + "</div>"; break;
                case cluster::EntryKind::Error: html += "<div style=\"margin-top:10px\">" + span_(st.error, "✗ " + esc_(text)) + "</div>"; break;
            }
        }
        // the text streamed (typed), the live thinking, the state of the turn (like Claude Code)
        if (frame.typed == static_cast<std::size_t>(-1) && frame.partialBytes > 0) html += "<p>" + span_(st.text, "⏺ " + esc_(s.partial.substr(0, frame.partialBytes))) + "</p>";
        if (!s.thinking.empty() && !compact) html += "<div>" + span_(st.muted, "<i>∴ " + (expanded ? pre(s.thinking) : esc_(cluster::one_line(s.thinking.substr(s.thinking.size() > 400 ? s.thinking.size() - 400 : 0), 160))) + "</i>") + "</div>";
        if (s.state == cluster::State::Working) {
            static const std::vector<std::string> spinner = {"·", "✢", "✳", "✶", "✻", "✽", "✻", "✶", "✳", "✢"};
            std::int64_t since = 0;
            for (auto it = s.entries.rbegin(); it != s.entries.rend() && since == 0; ++it)
                if (it->kind == cluster::EntryKind::User) since = it->time;
            const bool tool = !s.entries.empty() && s.entries.back().kind == cluster::EntryKind::Tool;
            const std::string verb = !s.thinking.empty() ? "Thinking" : (!s.partial.empty() || frame.typed != static_cast<std::size_t>(-1)) ? "Writing"
                : tool ? "Running " + s.entries.back().tool : "Working";
            const std::int64_t output = s.metrics.current.output;
            html += "<p>" + span_(st.accent, esc_(spinner[static_cast<std::size_t>(ms / 120) % spinner.size()] + " " + verb + "… (" + (since > 0 ? std::to_string(cluster::now() - since) + "s" : std::string("…"))
                + (output > 0 ? " · ↓ " + cluster::human_tokens(output) + " tokens" : std::string()) + " · " + config.keys.at("interrupt") + " to interrupt)")) + "</p>";
        }
        QScrollBar* bar = this->log->verticalScrollBar();
        const bool bottom = bar->value() >= bar->maximum() - 4;
        const int value = bar->value();
        this->log->setHtml(html);
        bar->setValue(bottom ? bar->maximum() : value);
    }

    // permission bar
    this->permBar->setVisible(!s.permissions.empty());
    if (!s.permissions.empty())
        this->permLabel->setText(span_(st.warn, "permission", true) + "  " + esc_(s.permissions.front().tool + ": " + cluster::one_line(s.permissions.front().description, 140)));

    // panels
    if (compact) return;
    QString p;
    const std::function<QString(const std::string&, const std::string&)> line = [&](const std::string& left, const std::string& right) -> QString {
        return "<tr><td>" + span_(st.muted, esc_(left)) + "</td><td align=\"right\">" + esc_(right) + "</td></tr>";
    };
    const std::function<QString(const std::string&)> title = [&](const std::string& name) -> QString {return "<p style=\"margin-top:8px\">" + span_(st.accent, esc_(name), true) + "</p>";};
    if (s.spec.panels.contains("tokens")) {
        // the new tokens apart from the ones read again from the cache at every step
        const std::function<std::string(const cluster::Usage&)> split = [](const cluster::Usage& u) {
            return cluster::human_tokens(u.input + u.cacheCreation + u.output) + (u.cacheRead > 0 ? " +" + cluster::human_tokens(u.cacheRead) + " cache" : "");
        };
        p += title("Tokens") + "<table width=\"100%\">" + line("last turn", split(m.last)) + line("current", split(m.current))
            + line("total", split(m.total)) + line("turns", std::to_string(m.turns)) + "</table>";
    }
    if (s.spec.panels.contains("context")) {
        const int percent = m.contextWindow > 0 ? static_cast<int>(100 * m.contextUsed / m.contextWindow) : 0;
        p += title("Context") + "<table width=\"100%\">" + line(cluster::human_tokens(m.contextUsed) + " / " + cluster::human_tokens(m.contextWindow), std::to_string(percent) + "%")
            + "</table><table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr>"
            + (percent > 0 ? "<td width=\"" + QString::number(percent) + "%\" bgcolor=\"" + q_(percent > 80 ? st.warn : st.accent) + "\">&nbsp;</td>" : QString())
            + (percent < 100 ? "<td bgcolor=\"" + q_(st.border) + "\">&nbsp;</td>" : QString()) + "</tr></table>";
    }
    if (s.spec.panels.contains("cost")) {
        p += title("Cost") + "<table width=\"100%\">" + line("session", m.costKnown ? cluster::human_cost(m.cost) : "local / unknown")
            + line("last turn", m.costKnown ? cluster::human_cost(m.lastCost) : "-");
        if (manager.config().budget > 0) p += line("budget", cluster::human_cost(manager.config().budget));
        if (m.limit5h >= 0) p += line("5h limit", std::to_string(static_cast<int>(m.limit5h * 100)) + "%");
        if (m.limit7d >= 0) p += line("7d limit", std::to_string(static_cast<int>(m.limit7d * 100)) + "%");
        p += "</table>";
    }
    if (s.spec.panels.contains("usage")) {
        // the vendor of the session over every session and run: today, 7 days, lifetime, its limits left
        p += title("Usage") + "<table width=\"100%\">";
        for (const auto &[left, right]: manager.usageLines(s))
            p += line(left, right);
        p += "</table>";
    }
    if (s.spec.panels.contains("skills")) {
        p += title("Skills");
        for (const std::string& skill: s.skillsLoaded)
            p += "<div>" + span_(st.ok, "● " + esc_(skill)) + "</div>";
        p += "<div>" + span_(st.muted, esc_(s.skillsLoaded.empty() ? "none loaded" : "") + " · " + QString::number(s.skillsAvailable.size()) + " available") + "</div>";
    }
    if (s.spec.panels.contains("git") || s.spec.panels.contains("diff")) {
        const cluster::GitInfo info = git.info(s.spec.cwd, s.spec.panels.contains("diff"));
        if (s.spec.panels.contains("git")) {
            p += title("Git");
            if (!info.repository) p += span_(st.muted, info.time > 0 ? "not a repository" : "…");
            else {
                p += "<div>" + span_(st.ok, " " + esc_(info.branch), true) + "</div><pre style=\"margin:0\">";
                for (std::size_t i = 0; i < info.graph.size() && i < 16; ++i)
                    p += esc_(cluster::one_line(info.graph[i], 40)) + "\n";
                p += "</pre>";
            }
        }
        if (info.repository && s.spec.panels.contains("diff")) {
            p += title("Diff") + "<pre style=\"margin:0\">";
            for (const std::string& stat: info.diffStat)
                p += esc_(cluster::one_line(stat, 40)) + "\n";
            for (std::size_t i = 0; i < info.diff.size() && i < 60; ++i) {
                const std::string& d = info.diff[i];
                p += span_(d.starts_with("+") ? st.ok : d.starts_with("-") ? st.error : st.muted, esc_(cluster::one_line(d, 40))) + "\n";
            }
            p += "</pre>";
        }
    }
    if (s.spec.panels.contains("tools")) {
        p += title("Tools");
        const std::size_t start = s.tools.size() > 12 ? s.tools.size() - 12 : 0;
        for (std::size_t i = start; i < s.tools.size(); ++i) {
            const cluster::ToolRun& t = s.tools[i];
            p += "<div>" + span_(!t.done ? st.accent : t.error ? st.error : st.ok, t.done ? (t.error ? "✗ " : "✓ ") : "… ") + span_(st.muted, esc_(cluster::one_line(t.name + " " + t.summary, 36))) + "</div>";
        }
    }
    if (s.spec.panels.contains("files")) {
        p += title("Files");
        for (const std::string& file: s.files)
            p += "<div>" + esc_(cluster::one_line(cluster::short_path(file), 38)) + "</div>";
    }
    this->panels->setText(p);
    this->panels->setVisible(!p.isEmpty());
}

} // namespace

/* entry */
_cold int cluster::run_gui(cluster::App& app)
{
    int argc = 1;
    char name[] = "claude-cluster";
    char* argv[] = {name, nullptr};
    QApplication application(argc, argv);
    QApplication::setApplicationName("claude-cluster");
    Window window(app);
    window.show();
    // the end of a prompt of the global session is notified only when the window is not the active one
    app.manager->setFocused(QGuiApplication::applicationState() == Qt::ApplicationActive);
    QObject::connect(&application, &QGuiApplication::applicationStateChanged, [&app](const Qt::ApplicationState state) {
        app.manager->setFocused(state == Qt::ApplicationActive);
    });
    if (app.askRestore) {
        const std::vector<cluster::SessionSpec> previous = app.manager->previous();
        std::string list;
        for (const cluster::SessionSpec& spec: previous)
            list += "\n  " + spec.name + "  (" + spec.backend + ", " + cluster::short_path(spec.cwd) + ")";
        QMessageBox box(QMessageBox::Question, "claude-cluster", q_("Restore the previous run?" + list + "\n  global session: its conversation\n\nNo: start new (the sessions to the trash "
            + std::to_string(app.manager->config().trashDays) + " days, a new global conversation).\nQuit: asked again at the next start."),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Close, &window);
        box.button(QMessageBox::Close)->setText("Quit");
        box.setDefaultButton(QMessageBox::Yes);
        box.setEscapeButton(QMessageBox::Yes); // closed: everything kept
        const int answer = box.exec();
        if (answer == QMessageBox::Close || cluster::quit_requested()) return 0; // out without answering: the previous run kept
        app.manager->answerRestore(answer == QMessageBox::Yes);
        window.refresh(true);
    }
    if (app.askMachine != 0 && !std::getenv("CLAUDE_CLUSTER_SCREENSHOT")) window.askMachine(app.askMachine);
    // Debug: CLAUDE_CLUSTER_SCREENSHOT=<file.png> saves the window after 3 s then quits (with QT_QPA_PLATFORM=offscreen)
    const char* screenshot = std::getenv("CLAUDE_CLUSTER_SCREENSHOT");
    if (screenshot && *screenshot) {
        const QString path = screenshot;
        if (std::getenv("CLAUDE_CLUSTER_SCREENSHOT_SETTINGS")) QTimer::singleShot(1000, &window, [&window]() {window.settings();});
        if (const char* typed = std::getenv("CLAUDE_CLUSTER_SCREENSHOT_TYPE")) QTimer::singleShot(1500, &window, [&window, typed]() {
            window.input->setText(typed); // debug: text typed in the prompt (completion list shown)
            window.input->refreshCompletion();
        });
        QTimer::singleShot(3000, &window, [&window, path]() {
            window.refresh(true);
            window.grab().save(path);
            QApplication::quit();
        });
    }
    return QApplication::exec();
}
