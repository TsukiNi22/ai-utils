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
#include "cluster/Editor.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Core.hpp"
#include "cluster/Git.hpp"
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
        bool focusNextPrevChild(bool) override {return false;}; // Tab completes, never leaves the prompt

    public:
        QString text(void) const {return this->toPlainText();};
        void setText(const QString& text) {this->setPlainText(text); this->moveCursor(QTextCursor::End);};
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
        Window* window = nullptr;
        cluster::Editor completion;
        QListWidget* popup = nullptr;
        QWidget* chipsBar = nullptr;
        std::vector<std::string> images;

        PromptEdit(Window* owner, QWidget* parent);
        void refreshCompletion(void);
        void apply(const bool final);
        void addImage(const std::string& path);
        void reset(void);
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
        void update(const cluster::Snapshot& s, const cluster::Style& style, cluster::Manager& manager, const bool compact);
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

        Window(cluster::App& application)
            : app(application), manager(*application.manager)
        {
            this->setWindowTitle("claude-cluster");
            this->resize(1400, 860);
            this->bindKeys();
            this->rebuild();
            QTimer* timer = new QTimer(this);
            QObject::connect(timer, &QTimer::timeout, [this]() {this->tick();});
            timer->start(150);
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
                    } else {
                        QLineEdit* line = new QLineEdit(q_(current), tab);
                        read = [line]() {return cluster::trim(line->text().toStdString());};
                        widget = line;
                    }
                    widget->setToolTip(tip);
                    QLabel* label = new QLabel(q_(setting.label + (setting.restart ? " *" : "")), tab);
                    label->setToolTip(tip);
                    form->addRow(label, widget);
                    fields.emplace_back(setting, read);
                }
                sections->addTab(tab, q_(page.title));
            }
            QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            for (const QKeySequence& key: {QKeySequence("Ctrl+S"), QKeySequence(Qt::Key_F10)}) {
                QShortcut* save = new QShortcut(key, &dialog);
                QObject::connect(save, &QShortcut::activated, &dialog, &QDialog::accept);
            }
            layout->addWidget(sections, 1);
            layout->addWidget(new QLabel("* applied at the next start · hover an option for its help · Ctrl+S save · Esc close (asks to save the changes)", &dialog));
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
            for (const char* key: {"layout", "style"})
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
                // a turn runs: stop it; else clear the prompt
                const std::optional<cluster::Snapshot> snap = this->manager.snapshot(this->active);
                if (snap && (snap->state == cluster::State::Working || snap->state == cluster::State::Waiting)) this->run({"interrupt", "", "", "", false}, "");
                else this->input->reset();
            } else if (action == "mode_cycle") {
                const std::optional<cluster::Snapshot> snap = this->manager.snapshot(this->active);
                if (snap) this->run({"mode:" + cluster::next_mode(snap->spec.mode), "", "", "", false}, "");
            } else if (action == "help") {
                QMessageBox box(QMessageBox::Information, "claude-cluster keys", q_(cluster::help_text(this->manager.config())), QMessageBox::Ok, this);
                box.setStyleSheet("QLabel{font-family:monospace;}");
                box.exec();
            } else if (action == "search") {
                this->findBar->setVisible(true);
                this->findInput->setFocus();
                this->findInput->selectAll();
            } else if (action == "edit") {
                bool ok = false;
                const QString text = QInputDialog::getMultiLineText(this, "Edit the prompt", "Prompt (Ok: back in the prompt)", this->input->text(), &ok);
                if (ok) this->input->setText(text);
            } else if (action == "stash") {
                if (cluster::trim(this->input->text().toStdString()).empty()) {
                    this->message("nothing to stash", false);
                } else {
                    this->manager.stashPush(this->input->text().toStdString());
                    this->input->reset();
                    this->message("prompt stashed (" + std::to_string(this->manager.stashSize()) + ")", false);
                }
            } else if (action == "unstash") {
                const std::optional<std::string> text = this->manager.stashPop();
                if (!text) {
                    this->message("the stash is empty", false);
                } else {
                    if (!cluster::trim(this->input->text().toStdString()).empty()) this->manager.stashPush(this->input->text().toStdString());
                    this->input->setText(q_(*text));
                }
            } else if (action == "history") {
                QStringList items;
                for (const std::string& text: this->manager.history())
                    items << q_(text);
                if (items.isEmpty()) {
                    this->message("no prompt yet", false);
                } else {
                    bool ok = false;
                    const QString item = QInputDialog::getItem(this, "History", "Prompt (type to filter)", items, 0, true, &ok);
                    if (ok) this->input->setText(item);
                }
            } else if (action == "always") this->run({"always", "", "", "", false}, "");
            else if (action == "quit") this->close();
        }

        void escape(void)
        {
            // a queued prompt of the session: back in the prompt to edit it (removed from the queue); else Esc Esc clears
            const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const std::optional<cluster::Queued> queued = this->manager.unqueue(this->active);
            if (queued) {
                if (!cluster::trim(this->input->text().toStdString()).empty()) this->manager.stashPush(this->input->text().toStdString());
                this->input->reset();
                this->input->setText(q_(queued->text));
                for (const std::string& image: queued->images)
                    this->input->addImage(image);
                this->message("queued prompt back to edit (Enter: queue it again)", false);
                this->lastEscape = 0;
            } else if (now - this->lastEscape < 600) {
                this->input->reset();
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

        void send(void)
        {
            const std::string text = cluster::trim(this->input->text().toStdString());
            if (text.empty()) return;
            const std::optional<cluster::ActionResult> local = cluster::run_slash(this->manager, this->active, text);
            if (local) {
                this->manager.addHistory(text, this->active);
                this->input->reset();
                if (!local->focus.empty()) this->active = local->focus;
                if (local->ui == "settings") this->settings();
                else if (local->ui == "restore_menu") this->openRestore();
                if (!local->message.empty()) this->message(local->message, local->error);
                this->refresh(true);
                return;
            }
            try {
                this->manager.send(this->active, text, false, this->input->images);
                this->input->reset();
            } catch (const utils::exception::IException& e) {
                this->message(e.info(), true);
            }
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
            bar->addWidget(this->input, 1);
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
            if (!force && version == this->lastVersion) return;
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
            for (const cluster::Snapshot& s: sessions) {
                auto it = this->views.find(s.spec.id);
                if (it != this->views.end()) {
                    it->second->setProperty("active", s.spec.id == this->active);
                    it->second->style()->unpolish(it->second);
                    it->second->style()->polish(it->second);
                    it->second->update(s, st, this->manager, true);
                } else if (s.spec.id == this->active && this->views.contains("")) {
                    this->views[""]->id = s.spec.id;
                    this->views[""]->update(s, st, this->manager, false);
                }
            }
            this->target->setText(q_(this->active == GLOBAL_ID ? "→ global" : "→ " + this->active));

            // voice bar, last notice
            const cluster::Voice& voice = *this->app.voice;
            const cluster::VoiceConf& conf = this->manager.config().voice;
            QString voiceText;
            if (voice.listening()) voiceText = span_(st.ok, conf.mode == "wake" ? (voice.armed() ? "◉ armed: say the command" : q_("○ waiting for \"" + conf.wakeWord + "\"")) : "◉ listening", true);
            for (const cluster::Pending& p: voice.pending())
                voiceText += "  " + esc_((p.target.empty() ? "" : "→ " + (p.target == GLOBAL_ID ? std::string("global") : p.target) + ": ") + p.text);
            if (voice.speaking()) voiceText += "  " + span_(st.muted, "speaking (" + q_(conf.wakeWord.empty() ? "" : "") + q_(this->manager.config().keys.at("mute")) + " mute)");
            this->voiceLabel->setText(voiceText);
            this->voiceLabel->setVisible(!voiceText.isEmpty());
            const std::vector<cluster::Notice> notices = this->manager.notices(1);
            if (!notices.empty() && notices.back().time > this->statusTime)
                this->status->setText(span_(notices.back().error ? st.error : st.muted, esc_(cluster::one_line(notices.back().text, 200))));
        }

        void tick(void)
        {
            if (this->manager.config().reload()) {
                this->bindKeys();
                this->signature.clear();
            }
            this->refresh(false);
        }
};

PromptEdit::PromptEdit(Window* owner, QWidget* parent)
    : QPlainTextEdit(parent), window(owner)
{
    this->completion.setProvider(cluster::completion_provider(owner->manager, [owner]() {return owner->active;}));
    this->setTabChangesFocus(false);
    this->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    QObject::connect(this, &QPlainTextEdit::textChanged, [this]() {this->refreshCompletion(); this->fit();});
    QObject::connect(this, &QPlainTextEdit::cursorPositionChanged, [this]() {this->refreshCompletion();});
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

void PromptEdit::addImage(const std::string& path)
{
    this->images.push_back(path);
    this->insert(q_("[Image #" + std::to_string(this->images.size()) + "] "));
    QLabel* chip = new QLabel(q_("[Image #" + std::to_string(this->images.size()) + "] " + std::filesystem::path(path).filename().string()), this->chipsBar);
    chip->setToolTip("<img src=\"" + q_(path) + "\" width=\"420\">");
    chip->setStyleSheet("padding:2px 6px;border-radius:4px;border:1px solid palette(highlight);");
    this->chipsBar->layout()->addWidget(chip);
    this->chipsBar->setVisible(true);
}

void PromptEdit::reset(void)
{
    this->clear();
    this->images.clear();
    QLayoutItem* item = nullptr;
    while ((item = this->chipsBar->layout()->takeAt(0))) {
        delete item->widget();
        delete item;
    }
    this->chipsBar->setVisible(false);
    if (this->popup) this->popup->setVisible(false);
}

void PromptEdit::keyPressEvent(QKeyEvent* event)
{
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
            const std::filesystem::path path = cluster::data_dir() / "images" / ("paste-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()) + ".png");
            if (!image.isNull() && image.save(q_(path.string()), "PNG")) {
                this->addImage(path.string());
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

void SessionView::update(const cluster::Snapshot& s, const cluster::Style& st, cluster::Manager& manager, const bool compact)
{
    static cluster::Git git; // lives as long as the program (its refresh threads)
    const cluster::Metrics& m = s.metrics;
    this->id = s.spec.id;

    // header
    const std::string model = s.modelUsed.empty() ? s.spec.backend : s.spec.backend.substr(0, s.spec.backend.find('/')) + "/" + s.modelUsed;
    QString head = span_(st.accent, esc_(s.spec.name), true) + "  " + span_(s.state == cluster::State::Error ? st.error : st.text, cluster::state_name(s.state))
        + "  " + span_(st.muted, esc_(model + "  ·  mode " + s.spec.mode + (compact ? "" : "  ·  " + cluster::short_path(s.spec.cwd))));
    if (!s.lastError.empty()) head += "<br>" + span_(st.error, esc_(cluster::one_line(s.lastError, 200)));
    if (s.state == cluster::State::Remote) head += "<br>" + span_(st.accent, "Remote Control: continue on claude.ai / the Claude app");
    this->header->setText(head);

    // transcript (rebuilt only when it changed)
    const std::string key = std::to_string(s.entries.size()) + "|" + std::to_string(s.partial.size()) + "|" + cluster::state_name(s.state) + "|" + st.name;
    if (key != this->lastKey) {
        this->lastKey = key;
        QString html;
        const std::size_t start = s.entries.size() > 400 ? s.entries.size() - 400 : 0;
        for (std::size_t i = start; i < s.entries.size(); ++i) {
            const cluster::Entry& e = s.entries[i];
            switch (e.kind) {
                case cluster::EntryKind::User: {
                    // [Image: path] -> link, previewed on hover
                    QString body;
                    static const std::regex image(R"(\[Image: ([^\]]+)\])");
                    std::string rest = e.text;
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
                case cluster::EntryKind::Assistant: html += "<p>" + span_(st.text, esc_(e.text)) + "</p>"; break;
                case cluster::EntryKind::Thinking: if (!compact) html += "<p>" + span_(st.muted, "∴ " + esc_(cluster::one_line(e.text, 300))) + "</p>"; break;
                case cluster::EntryKind::Tool: html += "<div>" + span_(st.muted, "⏺ <b>" + esc_(e.tool) + "</b> " + esc_(e.text)) + "</div>"; break;
                case cluster::EntryKind::ToolResult: if (!compact) html += "<div>" + span_(st.muted, "&nbsp;&nbsp;⎿ " + esc_(cluster::one_line(e.text, 300))) + "</div>"; break;
                case cluster::EntryKind::System: html += "<div>" + span_(st.muted, "· " + esc_(e.text)) + "</div>"; break;
                case cluster::EntryKind::Error: html += "<div>" + span_(st.error, "✗ " + esc_(e.text)) + "</div>"; break;
            }
        }
        if (!s.partial.empty()) html += "<p>" + span_(st.muted, esc_(s.partial)) + "</p>";
        if (s.state == cluster::State::Working) html += "<p>" + span_(st.muted, "… working") + "</p>";
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
    if (s.spec.panels.contains("tokens"))
        p += title("Tokens") + "<table width=\"100%\">" + line("last prompt", cluster::human_tokens(m.last.total())) + line("current", cluster::human_tokens(m.current.total()))
            + line("total", cluster::human_tokens(m.total.total())) + line("turns", std::to_string(m.turns)) + "</table>";
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
    if (app.askRestore) {
        const std::vector<cluster::SessionSpec> previous = app.manager->previous();
        std::string list;
        for (const cluster::SessionSpec& spec: previous)
            list += "\n  " + spec.name + "  (" + spec.backend + ", " + cluster::short_path(spec.cwd) + ")";
        const QMessageBox::StandardButton answer = QMessageBox::question(&window, "claude-cluster", q_("Restore the previous sessions?" + list + "\n\nNo: start clean (they stay in the trash "
            + std::to_string(app.manager->config().trashDays) + " days)."), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        app.manager->restorePrevious(answer == QMessageBox::Yes);
        window.refresh(true);
    }
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
