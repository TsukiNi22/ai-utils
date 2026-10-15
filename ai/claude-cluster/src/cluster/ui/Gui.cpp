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
#include "cluster/Display.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Session.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Worker.hpp"
#include "cluster/Editor.hpp"
#include "cluster/Reveal.hpp"
#include "cluster/Icons.hpp"
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
#include <QPainterPath>
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
#include <QScrollArea>
#include <QFileDialog>
#include <QScrollBar>
#include <functional>
#include <QTabWidget>
#include <QClipboard>
#include <QFileInfo>
#include <QLineEdit>
#include <QSplitter>
#include <QShortcut>
#include <QCheckBox>
#include <QComboBox>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPalette>
#include <QToolTip>
#include <QSpinBox>
#include <QBuffer>
#include <QCursor>
#include <fstream>
#include <QWidget>
#include <QTabBar>
#include <QDialog>
#include <cstdlib>
#include <QImage>
#include <chrono>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QFile>
#include <regex>
#include <cmath>
#include <QDir>
#include <QUrl>
#include <set>
#include <map>

_cold static std::string mix_(const std::string& a, const std::string& b, const double weight)
{
    // a + (b - a) * weight, as #rrggbb (the tints of the style sheet)
    const QColor x(QString::fromStdString(a));
    const QColor y(QString::fromStdString(b));
    const std::function<int(int, int)> at = [weight](const int p, const int q) {return static_cast<int>(p + (q - p) * weight + 0.5);};
    return QColor(at(x.red(), y.red()), at(x.green(), y.green()), at(x.blue(), y.blue())).name().toStdString();
}

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

// The outline of a card, painted over its content (the clicks go through it)
class Outline: public QWidget {
    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(this->color, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(this->rect()).adjusted(0.75, 0.75, -0.75, -0.75), 12, 12);
        }

    public:
        QColor color;
        explicit Outline(QWidget* parent) : QWidget(parent)
        {
            this->setAttribute(Qt::WA_TransparentForMouseEvents);
            this->setAttribute(Qt::WA_NoSystemBackground);
        }
};

class SessionView: public QFrame {
    protected:
        void resizeEvent(QResizeEvent* event) override
        {
            QFrame::resizeEvent(event);
            if (this->outline) {
                this->outline->setGeometry(this->rect());
                this->outline->raise();
            }
        }

    public:
        Outline* outline = nullptr;
        std::string id;
        std::string branch;                     // where it runs: its git branch (read every 5 s)
        std::int64_t branchAt = 0;
        QLabel* header = nullptr;
        QTextBrowser* log = nullptr;
        QLabel* panels = nullptr;
        QWidget* permBar = nullptr;
        QLabel* permLabel = nullptr;
        std::string lastKey;
        std::map<QString, QImage> pictures;     // the pictures of the displays (display:<entry>), given to the transcript

        SessionView(Window* window, const std::string& sessionId, const bool compact);
        QString displayHtml(const cluster::Json& display, const std::string& key, const cluster::Style& style, const bool expanded);
        void displayForm(Window* window, const std::string& displayId);    // a form of a display filled, its values sent
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
            QTimer* face = new QTimer(this); // the icon follows the state (>_< running, •_• waiting on you, o_o an error...)
            QObject::connect(face, &QTimer::timeout, [this]() {this->showFace();});
            face->start(500);
            this->showFace();
        }

        void webAsk(void)
        {
            // a device of the web access asks to connect: who it is, allow / allow and trust / deny / later
            cluster::Web* web = this->manager.web();
            if (web == nullptr || this->webAsking) return;
            for (const cluster::WebDevice& d: web->status().devices) {
                if (d.state != "waiting" || this->webAsked.contains(d.id)) continue;
                this->webAsked.insert(d.id);
                this->webAsking = true;
                QMessageBox box(this);
                box.setWindowTitle("Web access");
                box.setIcon(QMessageBox::Question);
                box.setText("<b>" + esc_(d.name) + "</b> asks to connect to the web access");
                box.setInformativeText("address " + q_(d.address) + "<br>browser " + esc_(cluster::one_line(d.agent, 160)) + "<br>its code was right; trusted: no code, "
                    "no approval next time (the web devices, to remove it)");
                QPushButton* allow = box.addButton("Allow", QMessageBox::AcceptRole);
                QPushButton* trust = box.addButton("Allow and trust", QMessageBox::AcceptRole);
                QPushButton* deny = box.addButton("Deny", QMessageBox::RejectRole);
                box.addButton("Later", QMessageBox::DestructiveRole);
                box.setDefaultButton(allow);
                box.exec();
                const QAbstractButton* clicked = box.clickedButton();
                const std::string answer = clicked == allow ? "allow" : clicked == trust ? "trust" : clicked == deny ? "deny" : "";
                try {
                    if (!answer.empty()) web->approve(d.id, answer);
                } catch (const utils::exception::IException& e) {
                    this->message(e.info(), true);
                }
                this->webAsking = false;
                return;
            }
        }

        void webDevices(void)
        {
            // the devices waiting, connected and trusted: ticked, then kicked or their trust removed
            cluster::Web* web = this->manager.web();
            if (web == nullptr) return;
            QDialog dialog(this);
            dialog.setWindowTitle("Web devices");
            dialog.resize(640, 420);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            QListWidget* devices = new QListWidget(&dialog);
            layout->addWidget(new QLabel("Tick the devices, then: kick them, or trust them no more (the code asked again).", &dialog));
            layout->addWidget(devices);
            const std::function<void(void)> fill = [web, devices]() {
                devices->clear();
                const cluster::WebStatus status = web->status();
                for (const cluster::WebDevice& d: status.devices) {
                    QListWidgetItem* item = new QListWidgetItem(q_(d.name + "   " + d.address + "   " + d.state + (d.trusted ? " · trusted" : "")), devices);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(Qt::Unchecked);
                    item->setData(Qt::UserRole, q_("d:" + d.id));
                    item->setToolTip(q_(d.agent));
                }
                for (const cluster::WebDevice& d: status.trusted) {
                    QListWidgetItem* item = new QListWidgetItem(q_("trusted   " + d.name + "   last " + (d.seen > 0 ? cluster::human_age(d.seen / 1000) + " ago" : std::string("never"))), devices);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(Qt::Unchecked);
                    item->setData(Qt::UserRole, q_("t:" + d.id));
                    item->setToolTip(q_(d.agent));
                }
                if (devices->count() == 0) new QListWidgetItem("nobody connected, no trusted device", devices);
            };
            fill();
            const std::function<std::pair<std::vector<std::string>, std::vector<std::string>>(void)> ticked = [devices]() {
                std::pair<std::vector<std::string>, std::vector<std::string>> out;
                for (int i = 0; i < devices->count(); ++i) {
                    const QString key = devices->item(i)->data(Qt::UserRole).toString();
                    if (key.isEmpty() || devices->item(i)->checkState() != Qt::Checked) continue;
                    (key.startsWith("t:") ? out.second : out.first).push_back(key.mid(2).toStdString());
                }
                return out;
            };
            QHBoxLayout* buttons = new QHBoxLayout();
            QPushButton* kick = new QPushButton("Kick the ticked", &dialog);
            QPushButton* kickAll = new QPushButton("Kick all (a new code)", &dialog);
            QPushButton* untrust = new QPushButton("Trust no more", &dialog);
            QPushButton* close = new QPushButton("Close", &dialog);
            for (QPushButton* b: {kick, kickAll, untrust})
                buttons->addWidget(b);
            buttons->addStretch(1);
            buttons->addWidget(close);
            layout->addLayout(buttons);
            QObject::connect(kick, &QPushButton::clicked, [web, ticked, fill]() {
                const std::vector<std::string> ids = ticked().first;
                if (!ids.empty()) web->kick(ids);
                fill();
            });
            QObject::connect(kickAll, &QPushButton::clicked, [web, fill]() {
                web->kick();
                fill();
            });
            QObject::connect(untrust, &QPushButton::clicked, [web, ticked, fill]() {
                const auto [connected, trusted] = ticked();
                std::vector<std::string> ids = trusted;
                ids.insert(ids.end(), connected.begin(), connected.end());
                if (!ids.empty()) web->untrust(ids);
                fill();
            });
            QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
            dialog.exec();
        }

        QLabel* brand = nullptr;                // the top bar: its face and the name
        bool webAsking = false;
        std::set<std::string> webAsked;

        void showFace(void)
        {
            QTimer::singleShot(0, this, [this]() {this->webAsk();}); // a device asking to connect: its dialog
            const cluster::AppFace face = this->manager.appFace(this->app.voice->listening());
            const std::string key = std::string(cluster::face_name(face.face)) + std::to_string(face.badge) + (face.urgent ? "!" : "");
            cluster::dock_badge(face.badge, face.urgent);
            if (key == this->faceShown) return;
            this->faceShown = key;
            QPixmap pixmap;
            pixmap.loadFromData(QByteArray::fromStdString(cluster::face_svg(face.face, face.badge, face.urgent)), "SVG");
            this->setWindowIcon(QIcon(pixmap));
            this->setWindowTitle(QString("(") + QString::fromUtf8(cluster::face_text(face.face)) + ") claude-cluster");
            if (this->brand) {
                const cluster::Style& st = this->theme();
                this->brand->setText("<span style=\"background:" + q_(mix_(st.surface, st.accent, 0.18)) + ";color:" + q_(st.accent) + ";font-family:monospace\">&nbsp;"
                    + QString::fromUtf8(cluster::face_text(face.face)).toHtmlEscaped() + "&nbsp;</span>&nbsp; claude-cluster");
            }
        }

        std::string faceShown;

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
            // one look with the web page: surfaces, 1 px lines, rounded panels, the accent for what is active, thin scroll bars
            const std::string tint = mix_(s.surface, s.accent, 0.14);
            const std::string hover = mix_(s.surface, s.accent, 0.07);
            const std::string sunk = mix_(s.bg, s.surface, 0.5);
            this->setStyleSheet(q_(
                "QMainWindow,QDialog{background:" + s.bg + ";}"
                "QWidget#topbar{background:" + s.surface + ";border-bottom:1px solid " + s.border + ";}"
                "QLabel#brand{font-weight:600;font-size:11pt;padding:0 6px;}"
                "QPushButton#seg{border:1px solid " + s.border + ";border-radius:0;padding:5px 14px;background:transparent;margin:0;}"
                "QPushButton#seg:first{border-top-left-radius:8px;border-bottom-left-radius:8px;}"
                "QPushButton#seg:last{border-top-right-radius:8px;border-bottom-right-radius:8px;}"
                "QPushButton#seg:checked{background:" + tint + ";color:" + s.accent + ";font-weight:600;border-color:" + s.accent + ";}"
                "QFrame#session{background:transparent;border:none;}"
                "QLabel#head{border:none;border-bottom:1px solid " + s.border + ";border-top-left-radius:12px;border-top-right-radius:12px;}"
                "QWidget#cardBody{background:" + s.bg + ";border:none;border-bottom-left-radius:12px;border-bottom-right-radius:12px;}"
                "QLabel#head{padding:10px 12px;background:" + s.surface + ";}"
                "QLineEdit,QTextEdit,QPlainTextEdit,QSpinBox,QDoubleSpinBox{border:1px solid " + s.border + ";border-radius:10px;padding:7px 10px;background:" + s.surface + ";}"
                "QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus{border-color:" + s.accent + ";}"
                "QFrame#composer{background:" + s.surface + ";border:1px solid " + s.border + ";border-radius:14px;}"
                "QFrame#composer QLineEdit,QFrame#composer QTextEdit,QFrame#composer QPlainTextEdit{border:none;background:transparent;padding:2px 4px;}"
                "QTextBrowser{border:none;border-radius:0;background:" + s.bg + ";selection-background-color:" + s.accent + ";selection-color:" + s.bg + ";padding:6px 10px;}"
                "QPushButton{border:1px solid " + s.border + ";border-radius:8px;padding:6px 12px;background:" + s.surface + ";}"
                "QPushButton:hover{border-color:" + s.accent + ";background:" + hover + ";}"
                "QPushButton:pressed{background:" + tint + ";}"
                "QPushButton#primary{background:" + s.accent + ";color:" + s.bg + ";border:none;font-weight:600;padding:7px 16px;}"
                "QPushButton#primary:hover{background:" + mix_(s.accent, s.text, 0.15) + ";}"
                "QListWidget{border:none;background:" + s.bg + ";outline:0;padding:4px;}"
                "QListWidget::item{border-radius:10px;margin:2px 0;}"
                "QListWidget::item:hover{background:" + hover + ";}"
                "QListWidget::item:selected{background:" + tint + ";color:" + s.text + ";}"
                "QLabel#panels{background:" + s.surface + ";border:none;border-radius:10px;padding:10px;}"
                "QWidget#perm{background:" + mix_(s.surface, s.warn, 0.10) + ";border:1px solid " + s.warn + ";border-radius:10px;}"
                "QLabel#status{color:" + s.muted + ";font-size:9pt;padding:2px 4px;}"
                "QSplitter::handle{background:transparent;width:8px;}"
                "QScrollBar:vertical{background:transparent;width:10px;margin:2px;}"
                "QScrollBar::handle:vertical{background:" + s.border + ";border-radius:4px;min-height:32px;}"
                "QScrollBar::handle:vertical:hover{background:" + s.muted + ";}"
                "QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}"
                "QScrollBar::handle:horizontal{background:" + s.border + ";border-radius:4px;min-width:32px;}"
                "QScrollBar::add-line,QScrollBar::sub-line,QScrollBar::add-page,QScrollBar::sub-page{background:none;border:none;width:0;height:0;}"
                "QTabWidget::pane{border:1px solid " + s.border + ";border-radius:10px;top:-1px;background:" + s.bg + ";}"
                "QTabBar::tab{background:transparent;border:1px solid transparent;border-radius:8px;padding:6px 12px;margin:2px;color:" + s.muted + ";}"
                "QTabBar::tab:selected{background:" + tint + ";color:" + s.accent + ";border-color:" + s.accent + ";}"
                "QTabBar::tab:hover{color:" + s.text + ";}"
                "QComboBox{border:1px solid " + s.border + ";border-radius:8px;padding:5px 10px;background:" + s.surface + ";}"
                "QComboBox QAbstractItemView{background:" + s.surface + ";border:1px solid " + s.border + ";selection-background-color:" + tint + ";}"
                "QCheckBox::indicator{width:16px;height:16px;border:1px solid " + s.border + ";border-radius:4px;background:" + sunk + ";}"
                "QCheckBox::indicator:checked{background:" + s.accent + ";border-color:" + s.accent + ";}"
                "QToolTip{background:" + s.surface + ";color:" + s.text + ";border:1px solid " + s.border + ";border-radius:6px;padding:6px;}"
                "QMenu{background:" + s.surface + ";border:1px solid " + s.border + ";border-radius:8px;padding:4px;}"
                "QMenu::item{padding:6px 14px;border-radius:6px;} QMenu::item:selected{background:" + tint + ";}"
                "QProgressBar{border:1px solid " + s.border + ";border-radius:6px;background:" + sunk + ";text-align:center;}"
                "QProgressBar::chunk{background:" + s.accent + ";border-radius:5px;}"));
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
            } else if (base == "web_devices") {
                this->webDevices();
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
                QScrollArea* scroll = new QScrollArea(sections);
                scroll->setWidget(tab);
                scroll->setWidgetResizable(true);
                scroll->setFrameShape(QFrame::NoFrame);
                sections->addTab(scroll, q_(page.title));
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
            root->setContentsMargins(0, 0, 0, 8);
            root->setSpacing(8);
            // the top bar: its face, the layouts, the palette / the web devices / the settings (like the web page)
            QWidget* topbar = new QWidget(central);
            topbar->setObjectName("topbar");
            topbar->setAttribute(Qt::WA_StyledBackground, true);
            QHBoxLayout* top = new QHBoxLayout(topbar);
            top->setContentsMargins(12, 8, 12, 8);
            top->setSpacing(0);
            this->brand = new QLabel(topbar);
            this->brand->setObjectName("brand");
            this->brand->setTextFormat(Qt::RichText);
            top->addWidget(this->brand);
            top->addSpacing(14);
            for (const auto &[name, label]: std::vector<std::pair<std::string, std::string>>{{"list", "List"}, {"grid", "Grid"}, {"tabs", "Tabs"}}) {
                QPushButton* seg = new QPushButton(q_(label), topbar);
                seg->setObjectName("seg");
                seg->setCheckable(true);
                seg->setChecked(layout == name);
                seg->setCursor(Qt::PointingHandCursor);
                const std::string chosen = name;
                QObject::connect(seg, &QPushButton::clicked, [this, chosen]() {
                    this->manager.setUi("layout", chosen);
                    this->refresh(true);
                });
                top->addWidget(seg);
            }
            top->addStretch(1);
            for (const auto &[label, action]: std::vector<std::pair<std::string, std::string>>{{"Palette", "palette"}, {"Devices", "devices"}, {"Settings", "settings"}}) {
                if (action == "devices" && (this->manager.web() == nullptr || !this->manager.web()->status().on)) continue;
                QPushButton* button = new QPushButton(q_(label), topbar);
                button->setCursor(Qt::PointingHandCursor);
                const std::string name = action;
                QObject::connect(button, &QPushButton::clicked, [this, name]() {
                    if (name == "devices") this->webDevices();
                    else if (name == "settings") this->settings();
                    else this->shortcut(name);
                });
                top->addSpacing(6);
                top->addWidget(button);
            }
            root->addWidget(topbar);
            QWidget* body = new QWidget(central);
            QVBoxLayout* inner = new QVBoxLayout(body);
            inner->setContentsMargins(10, 0, 10, 0);
            inner->setSpacing(8);
            root->addWidget(body, 1);
            root = inner;
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
            this->voiceLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
            QObject::connect(this->voiceLabel, &QLabel::linkActivated, [this](const QString& link) {
                // the web line: its code copied (ready to paste in the page), its devices
                cluster::Web* web = this->manager.web();
                if (web == nullptr) return;
                if (link == "web-devices") {
                    this->webDevices();
                    return;
                }
                QString digits;
                for (const QChar c: q_(web->status().pin))
                    if (c.isDigit()) digits += c;
                QGuiApplication::clipboard()->setText(digits);
                this->message("web: code " + web->status().pin + " copied", false);
            });
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
            sendButton->setObjectName("primary");
            sendButton->setCursor(Qt::PointingHandCursor);
            QObject::connect(sendButton, &QPushButton::clicked, [this]() {this->send();});
            this->input->vimLabel = new QLabel(central);
            this->input->vimLabel->setStyleSheet("font-weight:bold;");
            // the composer: the prompt, its target, Send, in one rounded field
            QFrame* composer = new QFrame(central);
            composer->setObjectName("composer");
            composer->setLayout(bar);
            bar->setContentsMargins(8, 6, 6, 6);
            bar->addWidget(this->input, 1);
            bar->addWidget(this->input->vimLabel);
            bar->addWidget(this->target);
            bar->addSpacing(6);
            bar->addWidget(sendButton);
            root->addWidget(composer);
            this->status = new QLabel(central);
            this->status->setObjectName("status");
            root->addWidget(this->status);
            this->setCentralWidget(central);
            this->input->setFocus();
            this->lastVersion = 0;
            this->faceShown.clear(); // the new top bar gets its face
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
                    const std::string dot = s.state == cluster::State::Working ? st.ok : s.state == cluster::State::Waiting || !s.permissions.empty() ? st.warn
                        : s.state == cluster::State::Error ? st.error : st.muted;
                    const char* face = !s.permissions.empty() ? "•_•" : s.state == cluster::State::Working ? ">_<" : s.state == cluster::State::Error ? "o_o" : "-_-";
                    QLabel* row = new QLabel(this->list);
                    row->setTextFormat(Qt::RichText);
                    row->setAttribute(Qt::WA_TransparentForMouseEvents);
                    row->setContentsMargins(10, 7, 10, 7);
                    row->setText("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td>" + span_(dot, "●") + "&nbsp; <b>" + esc_(s.spec.name) + "</b>&nbsp; "
                        + "<span style=\"background:" + q_(mix_(st.surface, dot, 0.22)) + ";color:" + q_(dot) + ";font-family:monospace;font-size:9pt\">&nbsp;" + face + "&nbsp;</span>"
                        + (s.permissions.empty() ? QString() : "&nbsp;" + span_(st.warn, "<b>" + QString::number(s.permissions.size()) + "</b>"))
                        + "</td><td align=\"right\">" + span_(st.muted, "<span style=\"font-family:monospace;font-size:9pt\">" + esc_(cluster::human_tokens(s.metrics.total.total()))
                        + "</span>") + "</td></tr></table>" + span_(st.muted, "<span style=\"font-family:monospace;font-size:8.5pt\">"
                        + esc_(s.spec.summary.empty() ? s.spec.backend.substr(0, s.spec.backend.find('/')) + " · " + cluster::short_path(s.spec.cwd) : cluster::one_line(s.spec.summary, 44))
                        + "</span>"));
                    QListWidgetItem* item = new QListWidgetItem(this->list);
                    item->setToolTip(q_((s.spec.summary.empty() ? std::string() : s.spec.summary + "\n") + cluster::short_path(s.spec.cwd)));
                    item->setSizeHint(QSize(0, row->sizeHint().height() + 4));
                    this->list->setItemWidget(item, row);
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
            if (!web.empty())
                voiceText = "<a href=\"web-copy\" style=\"text-decoration:none\">" + span_(st.accent, esc_(web), true) + "</a> <a href=\"web-devices\">"
                    + span_(st.muted, "devices") + "</a>" + (voiceText.isEmpty() ? QString() : "    " + voiceText);
            this->voiceLabel->setText(voiceText);
            this->voiceLabel->setVisible(!voiceText.isEmpty());
            const std::vector<cluster::Notice> notices = this->manager.notices(1);
            if (!notices.empty() && notices.back().time > this->statusTime)
                this->status->setText(span_(notices.back().error ? st.error : st.muted, esc_(cluster::one_line(notices.back().text, 200))));
        }

        std::uint64_t focusSeen = 0;            // last ui_focus applied
        std::uint64_t raiseSeen = 0;            // last notification clicked
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
            // the question waiting (any session): on the global session ([sessions] questions_to_global), else on its own
            cluster::Asking& questions = this->manager.asking();
            (void)questions.check();
            const std::optional<cluster::Ask> ask = questions.current();
            if (!ask || this->asked.contains(ask->requestId)) return;
            const bool show = this->manager.config().questionsToGlobal ? this->active == GLOBAL_ID || ask->session == GLOBAL_ID : this->active == ask->session;
            if (!show) return;
            this->asked.insert(ask->requestId);
            this->asking = true;
            QTimer::singleShot(0, this, [this, question = *ask]() {
                this->askQuestions(question);
                this->asking = false;
            });
        }

        void askQuestions(const cluster::Ask& ask)
        {
            // each question: its options (radio, or check boxes for a multi choice) and Other (typed); Decline: denied; the
            // one being answered follows the shared model both ways (the voice: a number, valide, retour...); answered
            // elsewhere (voice, web, MCP): closed
            struct Row {
                cluster::Json question;
                std::vector<QAbstractButton*> options;
                QAbstractButton* other = nullptr;
                QLineEdit* text = nullptr;
            };
            QDialog dialog(this);
            dialog.setWindowTitle(q_(ask.name + " asks"));
            dialog.setMinimumWidth(560);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            std::vector<Row> rows;
            bool syncing = false;
            for (const cluster::Json& q: ask.questions) {
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
            // a click on the question being answered: the shared model too (the voice goes on from it)
            for (std::size_t k = 0; k < rows.size(); ++k) {
                for (std::size_t i = 0; i < rows[k].options.size(); ++i)
                    QObject::connect(rows[k].options[i], &QAbstractButton::toggled, &dialog, [this, k, i, &syncing](const bool on) {
                        const std::optional<cluster::Ask> now = this->manager.asking().current();
                        if (syncing || !now || now->index != k) return;
                        const bool multi = now->questions[k].value("multiSelect", false);
                        if (multi ? now->picked.contains(static_cast<int>(i)) != on : on && now->cursor != static_cast<int>(i)) this->manager.asking().choose(static_cast<int>(i));
                    });
                QObject::connect(rows[k].text, &QLineEdit::textEdited, &dialog, [this, k, &syncing](const QString& text) {
                    const std::optional<cluster::Ask> now = this->manager.asking().current();
                    if (!syncing && now && now->index == k) this->manager.asking().setOther(text.toStdString());
                });
            }
            QLabel* voice = new QLabel(this->manager.config().voice.enabled ? "voice: a number or an option · valide / suivant · retour · autre <answer> · annule" : "", &dialog);
            voice->setStyleSheet("color: gray;");
            layout->addWidget(voice);
            QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            buttons->button(QDialogButtonBox::Ok)->setText("Answer");
            buttons->button(QDialogButtonBox::Cancel)->setText("Decline");
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);
            // the shared model -> the boxes (the voice); answered or declined elsewhere: closed
            std::uint64_t seen = 0;
            QTimer timer(&dialog);
            QObject::connect(&timer, &QTimer::timeout, &dialog, [&]() {
                cluster::Asking& model = this->manager.asking();
                (void)model.check();
                const std::optional<cluster::Ask> now = model.current();
                if (!now || now->requestId != ask.requestId) {
                    dialog.done(2);
                    return;
                }
                if (model.version() == seen) return;
                seen = model.version();
                syncing = true;
                for (std::size_t k = 0; k < rows.size() && k <= now->index; ++k) {
                    const std::string question = rows[k].question.value("question", std::string());
                    const std::string recorded = now->answers.value(question, std::string());
                    const int count = static_cast<int>(rows[k].options.size());
                    for (int i = 0; i < count; ++i) {
                        const std::string label = rows[k].question["options"][static_cast<std::size_t>(i)].value("label", std::string());
                        const bool on = k < now->index ? recorded == label || recorded.find(label + ", ") != std::string::npos || recorded.ends_with(", " + label)
                            : rows[k].question.value("multiSelect", false) ? now->picked.contains(i) : now->cursor == i;
                        rows[k].options[static_cast<std::size_t>(i)]->setChecked(on);
                    }
                    if (k == now->index) {
                        if (now->cursor == count) rows[k].other->setChecked(true);
                        if (rows[k].text->text().toStdString() != now->other) rows[k].text->setText(q_(now->other));
                    }
                }
                syncing = false;
            });
            timer.start(200);
            const int result = dialog.exec();
            timer.stop();
            if (result == 2) {
                this->message("question answered", false);
                return;
            }
            if (result != QDialog::Accepted) {
                this->manager.asking().decline();
                this->message("question declined", false);
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
                this->manager.answerQuestions(ask.session, ask.requestId, answers);
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
            if (this->manager.raiseRequest() != this->raiseSeen) { // a notification clicked: this window to the front
                this->raiseSeen = this->manager.raiseRequest();
                if (this->isMinimized()) this->showNormal();
                this->show();
                this->raise();
                this->activateWindow();
                QApplication::alert(this);
            }
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
    const std::string prefix = this->completion.kind() == cluster::CompletionKind::File ? "@" : this->completion.kind() == cluster::CompletionKind::Command || this->completion.kind() == cluster::CompletionKind::Skill ? "/" : "";
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

_cold static QImage chart_(const cluster::Json& d, const cluster::Style& st)
{
    // bars (grouped by label), lines, or a pie (the first series), with their legend
    QImage image(680, 280, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(q_(st.surface)));
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    const std::vector<QColor> colors = {QColor(q_(st.accent)), QColor(q_(st.ok)), QColor(q_(st.warn)), QColor(q_(st.error)), QColor(q_(st.text))};
    const std::string kind = d.value("kind", std::string("bar"));
    const cluster::Json labels = d.value("labels", cluster::Json::array());
    const cluster::Json& series = d["series"];
    const std::function<double(const cluster::Json&)> number = [](const cluster::Json& v) {return v.is_number() ? v.get<double>() : 0.0;};
    const std::function<QString(const cluster::Json&)> label = [](const cluster::Json& v) {return v.is_string() ? q_(v.get<std::string>()) : q_(v.dump());};
    p.setPen(QColor(q_(st.text)));
    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);
    const std::size_t count = series[0]["values"].size();
    if (kind == "pie") {
        double total = 0.0;
        for (const cluster::Json& v: series[0]["values"])
            total += std::max(0.0, number(v));
        double angle = 90.0 * 16;
        const QRectF circle(20, 20, 240, 240);
        for (std::size_t i = 0; i < count; ++i) {
            const double share = total > 0 ? std::max(0.0, number(series[0]["values"][i])) / total : 0.0;
            p.setBrush(colors[i % colors.size()]);
            p.setPen(QColor(q_(st.surface)));
            p.drawPie(circle, static_cast<int>(angle), static_cast<int>(-share * 360.0 * 16));
            angle -= share * 360.0 * 16;
            p.setPen(QColor(q_(st.text)));
            p.fillRect(QRectF(290, 30 + static_cast<double>(i) * 22, 12, 12), colors[i % colors.size()]);
            p.drawText(QPointF(310, 41 + static_cast<double>(i) * 22), (i < labels.size() ? label(labels[i]) : QString::number(i + 1)) + "  " + QString::number(share * 100, 'f', 1) + "%");
        }
        return image;
    }
    double low = 0.0;
    double high = 0.0;
    for (const cluster::Json& s: series)
        for (const cluster::Json& v: s["values"]) {
            low = std::min(low, number(v));
            high = std::max(high, number(v));
        }
    if (high <= low) high = low + 1.0;
    const QRectF area(50, series.size() > 1 ? 30 : 16, 600, series.size() > 1 ? 206 : 220);
    p.setPen(QColor(q_(st.muted)));
    p.drawLine(area.bottomLeft(), area.bottomRight());
    p.drawLine(area.bottomLeft(), area.topLeft());
    p.drawText(QPointF(4, area.top() + 8), QString::number(high, 'g', 4));
    p.drawText(QPointF(4, area.bottom()), QString::number(low, 'g', 4));
    const std::function<double(double)> y = [&](const double v) {return area.bottom() - (v - low) / (high - low) * area.height();};
    const double slot = area.width() / static_cast<double>(std::max<std::size_t>(1, count));
    for (std::size_t k = 0; k < series.size(); ++k) {
        const cluster::Json& values = series[k]["values"];
        p.setPen(QPen(colors[k % colors.size()], 2));
        p.setBrush(colors[k % colors.size()]);
        if (kind == "line") {
            QPainterPath path;
            for (std::size_t i = 0; i < values.size(); ++i) {
                const QPointF at(area.left() + slot * (static_cast<double>(i) + 0.5), y(number(values[i])));
                if (i == 0) path.moveTo(at);
                else path.lineTo(at);
            }
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        } else {
            const double width = slot * 0.8 / static_cast<double>(series.size());
            for (std::size_t i = 0; i < values.size(); ++i) {
                const double x = area.left() + slot * static_cast<double>(i) + slot * 0.1 + width * static_cast<double>(k);
                p.drawRect(QRectF(x, y(std::max(0.0, number(values[i]))), width - 2, y(std::min(0.0, number(values[i]))) - y(std::max(0.0, number(values[i])))));
            }
        }
        if (series.size() > 1) {
            p.fillRect(QRectF(area.left() + static_cast<double>(k) * 110, 6, 10, 10), colors[k % colors.size()]);
            p.setPen(QColor(q_(st.text)));
            p.drawText(QPointF(area.left() + 15 + static_cast<double>(k) * 110, 15), q_(series[k].value("name", std::string())));
        }
    }
    p.setPen(QColor(q_(st.text)));
    for (std::size_t i = 0; i < count && i < labels.size(); ++i)
        p.drawText(QRectF(area.left() + slot * static_cast<double>(i), area.bottom() + 4, slot, 18), Qt::AlignHCenter, label(labels[i]));
    return image;
}

_cold static QImage timeline_(const cluster::Json& d, const cluster::Style& st)
{
    // a Gantt: a bar per task on the span of all of them, its name on its left, its dates on its right
    const cluster::Json& tasks = d["tasks"];
    QImage image(680, 30 + static_cast<int>(tasks.size()) * 26, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(q_(st.surface)));
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    const std::vector<QColor> colors = {QColor(q_(st.accent)), QColor(q_(st.ok)), QColor(q_(st.warn)), QColor(q_(st.error))};
    double low = 1e18;
    double high = -1e18;
    for (const cluster::Json& task: tasks) {
        low = std::min(low, task.value("from", 0.0));
        high = std::max(high, task.value("to", 0.0));
    }
    const double span = std::max(1e-9, high - low);
    const QRectF area(150, 10, 420, static_cast<double>(tasks.size()) * 26);
    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);
    for (std::size_t k = 0; k < tasks.size(); ++k) {
        const double y = area.top() + static_cast<double>(k) * 26;
        const double a = area.left() + (tasks[k].value("from", 0.0) - low) / span * area.width();
        const double b = area.left() + (tasks[k].value("to", 0.0) - low) / span * area.width();
        p.setPen(QColor(q_(st.text)));
        p.drawText(QRectF(4, y, 140, 20), Qt::AlignVCenter | Qt::AlignRight, q_(tasks[k].value("name", std::string())));
        p.setPen(Qt::NoPen);
        p.setBrush(colors[k % colors.size()]);
        p.drawRoundedRect(QRectF(a, y + 3, std::max(4.0, b - a), 14), 3, 3);
        p.setPen(QColor(q_(st.muted)));
        const std::function<QString(const cluster::Json&)> date = [](const cluster::Json& v) {return v.is_string() ? q_(v.get<std::string>()) : q_(v.dump());};
        p.drawText(QRectF(area.right() + 6, y, 110, 20), Qt::AlignVCenter, date(tasks[k]["start"]) + " → " + date(tasks[k]["end"]));
    }
    return image;
}

_cold static QImage heatmap_(const cluster::Json& d, const cluster::Style& st)
{
    // a cell per value, from the surface to the accent; its labels
    const cluster::Json& rows = d["rows"];
    std::size_t columns = 0;
    double high = 0.0;
    for (const cluster::Json& row: rows) {
        columns = std::max(columns, row.size());
        for (const cluster::Json& v: row)
            if (v.is_number()) high = std::max(high, v.get<double>());
    }
    const int size = std::clamp(600 / static_cast<int>(std::max<std::size_t>(1, columns)), 8, 28);
    QImage image(50 + static_cast<int>(columns) * size + 10, 24 + static_cast<int>(rows.size()) * size + 10, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(q_(st.surface)));
    QPainter p(&image);
    QFont font = p.font();
    font.setPointSize(7);
    p.setFont(font);
    const QColor low(q_(st.border));
    const QColor top(q_(st.accent));
    const cluster::Json ylabels = d.value("ylabels", cluster::Json::array());
    const cluster::Json xlabels = d.value("xlabels", cluster::Json::array());
    for (std::size_t r = 0; r < rows.size(); ++r) {
        p.setPen(QColor(q_(st.muted)));
        if (r < ylabels.size() && ylabels[r].is_string()) p.drawText(QRectF(2, 20 + static_cast<double>(r) * size, 44, size), Qt::AlignVCenter | Qt::AlignRight, q_(ylabels[r].get<std::string>()));
        for (std::size_t c = 0; c < rows[r].size(); ++c) {
            const cluster::Json& v = rows[r][c];
            const double ratio = v.is_number() && high > 0 ? v.get<double>() / high : 0.0;
            const QColor color = !v.is_number() ? QColor(q_(st.surface)).darker(110) : QColor::fromRgbF(low.redF() + (top.redF() - low.redF()) * static_cast<float>(ratio),
                low.greenF() + (top.greenF() - low.greenF()) * static_cast<float>(ratio), low.blueF() + (top.blueF() - low.blueF()) * static_cast<float>(ratio));
            p.fillRect(QRectF(50 + static_cast<double>(c) * size, 20 + static_cast<double>(r) * size, size - 2, size - 2), color);
        }
    }
    p.setPen(QColor(q_(st.muted)));
    for (std::size_t c = 0; c < xlabels.size() && c < columns; ++c)
        if (xlabels[c].is_string()) p.drawText(QPointF(50 + static_cast<double>(c) * size, 14), q_(xlabels[c].get<std::string>()));
    return image;
}

void SessionView::displayForm(Window* window, const std::string& displayId)
{
    // its fields (text, number, a choice, yes / no, a date), its values sent to the global session
    const std::optional<cluster::Json> found = window->manager.displayOf(this->id, displayId);
    if (!found) return;
    const cluster::Json& d = *found;
    QDialog dialog(this);
    dialog.setWindowTitle(q_(d.value("title", std::string("Form"))));
    QFormLayout* form = new QFormLayout(&dialog);
    std::vector<std::pair<std::string, std::function<cluster::Json()>>> reads;
    for (const cluster::Json& field: d["fields"]) {
        const std::string type = field.value("type", std::string("text"));
        const QString value = field.contains("value") ? q_(field["value"].is_string() ? field["value"].get<std::string>() : field["value"].dump()) : QString();
        QWidget* widget = nullptr;
        std::function<cluster::Json()> read;
        if (type == "choice") {
            QComboBox* box = new QComboBox(&dialog);
            for (const cluster::Json& option: field.value("options", cluster::Json::array()))
                box->addItem(q_(option.is_string() ? option.get<std::string>() : option.dump()));
            box->setCurrentText(value);
            read = [box]() {return cluster::Json(box->currentText().toStdString());};
            widget = box;
        } else if (type == "bool") {
            QCheckBox* box = new QCheckBox(&dialog);
            box->setChecked(value == "true");
            read = [box]() {return cluster::Json(box->isChecked());};
            widget = box;
        } else {
            QLineEdit* line = new QLineEdit(value, &dialog);
            if (type == "date") line->setPlaceholderText("YYYY-MM-DD");
            read = [line]() {return cluster::Json(line->text().toStdString());};
            widget = line;
        }
        form->addRow(q_(field.value("label", field.value("name", std::string()))), widget);
        reads.emplace_back(field.value("name", std::string()), read);
    }
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(q_(d.value("submit", std::string("Send"))));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    cluster::Json values = cluster::Json::object();
    for (const auto &[name, read]: reads)
        values[name] = read();
    try {
        window->message(window->manager.displayAct(this->id, displayId, {{"values", values}}), false);
    } catch (const std::exception& e) {
        window->message(e.what(), true);
    }
}

QString SessionView::displayHtml(const cluster::Json& d, const std::string& key, const cluster::Style& st, const bool expanded)
{
    // a rich display: markdown by Qt, a table, a chart drawn, a picture as it is (image, diagram, SVG), a callout, a progress
    const std::string type = d.value("type", std::string());
    QString html = "<div style=\"margin-top:10px; margin-left:12px\">";
    if (!d.value("title", std::string()).empty()) html += "<p>" + span_(st.accent, "▣ <b>" + esc_(d.value("title", std::string())) + "</b>") + "</p>";
    const std::function<QString(const cluster::Json&)> cell = [](const cluster::Json& v) {return esc_(v.is_string() ? v.get<std::string>() : v.dump());};
    if (type == "markdown") {
        QTextDocument doc;
        doc.setMarkdown(q_(d.value("text", std::string())));
        const QString full = doc.toHtml();
        const qsizetype from = full.indexOf("<body");
        html += from < 0 ? full : full.mid(full.indexOf('>', from) + 1).replace("</body></html>", "");
    } else if (type == "code") {
        html += "<pre style=\"color:" + q_(st.muted) + "\">" + esc_(d.value("code", std::string())) + "</pre>";
    } else if (type == "callout") {
        const std::string level = d.value("level", std::string("info"));
        const std::string color = level == "success" ? st.ok : level == "warning" ? st.warn : level == "error" ? st.error : st.accent;
        const std::string icon = level == "success" ? "✔ " : level == "warning" ? "⚠ " : level == "error" ? "✗ " : "ℹ ";
        html += "<table width=\"100%\" cellpadding=\"8\" style=\"border-left: 3px solid " + q_(color) + "\"><tr><td>" + span_(color, esc_(icon + d.value("text", std::string()))) + "</td></tr></table>";
    } else if (type == "progress") {
        const double max = std::max(1e-9, d.value("max", 100.0));
        const int percent = static_cast<int>(std::clamp(d.value("value", 0.0) / max, 0.0, 1.0) * 100 + 0.5);
        html += "<table width=\"420\" cellspacing=\"0\" cellpadding=\"0\"><tr><td width=\"" + QString::number(std::max(1, percent)) + "%\" bgcolor=\"" + q_(st.accent)
            + "\">&nbsp;</td><td bgcolor=\"" + q_(st.border) + "\">&nbsp;</td></tr></table>" + span_(st.muted, QString::number(percent) + "%  " + esc_(d.value("label", std::string())));
    } else if (type == "table") {
        html += "<table border=\"1\" cellspacing=\"0\" cellpadding=\"5\" style=\"border-color:" + q_(st.border) + "\">";
        if (d.contains("columns") && d["columns"].is_array()) {
            html += "<tr>";
            for (const cluster::Json& c: d["columns"])
                html += "<th style=\"color:" + q_(st.accent) + "\">" + cell(c) + "</th>";
            html += "</tr>";
        }
        for (const cluster::Json& r: d["rows"]) {
            html += "<tr>";
            if (r.is_array())
                for (const cluster::Json& c: r)
                    html += "<td>" + cell(c) + "</td>";
            else html += "<td>" + cell(r) + "</td>";
            html += "</tr>";
        }
        html += "</table>";
    } else if (type == "diff") {
        html += "<pre>";
        for (const QString& line: q_(d.value("diff", std::string())).split('\n')) {
            const std::string color = line.startsWith("+++") || line.startsWith("---") ? st.muted : line.startsWith('+') ? st.ok : line.startsWith('-') ? st.error
                : line.startsWith("@@") ? st.accent : st.text;
            html += "<span style=\"color:" + q_(color) + "\">" + line.toHtmlEscaped() + "</span>\n";
        }
        html += "</pre>";
    } else if (type == "json" || type == "tree") {
        const std::vector<std::pair<std::string, char>> lines = type == "json" ? cluster::display_json_lines(d["data"], expanded ? 12 : 4, expanded ? 2000 : 120)
            : cluster::display_tree_lines(d["root"]);
        html += "<pre>";
        for (const auto &[line, kind]: lines) {
            const std::string color = kind == 's' || (kind == 'n' && type == "tree") ? st.ok : kind == 'n' ? st.warn : kind == 'k' || kind == 'd' ? st.accent
                : kind == 'm' ? st.warn : st.text;
            html += "<span style=\"color:" + q_(color) + "\">" + esc_(line) + "</span>\n";
        }
        html += "</pre>";
    } else if (type == "terminal") {
        // its own colors on a dark ground
        html += "<table width=\"100%\" cellpadding=\"6\" bgcolor=\"#1e1e1e\"><tr><td><pre style=\"color:#e5e5e5\">";
        if (!d.value("command", std::string()).empty()) html += "<span style=\"color:#3b8eea\">$ " + esc_(d.value("command", std::string())) + "</span>\n";
        for (const std::vector<cluster::Cell>& line: cluster::ansi_cells(d.value("output", std::string()))) {
            for (const cluster::Cell& c: line) {
                QString style;
                if (c.hasFg) style += "color:#" + QString::number(c.fg, 16).rightJustified(6, '0') + ";";
                if (c.hasBg) style += "background:#" + QString::number(c.bg, 16).rightJustified(6, '0') + ";";
                html += style.isEmpty() ? esc_(c.text) : "<span style=\"" + style + "\">" + esc_(c.text) + "</span>";
            }
            html += "\n";
        }
        html += "</pre></td></tr></table>";
    } else if (type == "checklist") {
        const std::string display = d.value("id", std::string());
        for (std::size_t i = 0; i < d["items"].size(); ++i) {
            const bool done = d["items"][i].value("done", false);
            html += "<a href=\"cluster-act:" + q_(display) + "/" + QString::number(i) + "\" style=\"text-decoration:none\">" + span_(done ? st.ok : st.text, done ? "☑" : "☐")
                + "</a> " + span_(done ? st.muted : st.text, esc_(d["items"][i].value("text", std::string()))) + "<br>";
        }
    } else if (type == "steps") {
        for (const cluster::Json& step: d["steps"]) {
            const std::string state = step.value("state", std::string("todo"));
            const std::string color = state == "done" ? st.ok : state == "current" ? st.accent : state == "error" ? st.error : st.muted;
            const QString text = esc_((state == "done" ? "✔ " : state == "current" ? "● " : state == "error" ? "✗ " : "○ ") + step.value("text", std::string()));
            html += span_(color, state == "current" ? "<b>" + text + "</b>" : text) + "<br>";
        }
    } else if (type == "kanban") {
        html += "<table cellspacing=\"6\"><tr>";
        for (const cluster::Json& column: d["columns"]) {
            html += "<td valign=\"top\" bgcolor=\"" + q_(st.surface) + "\" width=\"160\"><b>" + span_(st.accent, esc_(column.value("name", std::string()))) + "</b> "
                + span_(st.muted, QString::number(column["cards"].size()));
            for (const cluster::Json& card: column["cards"])
                html += "<table width=\"100%\" cellpadding=\"4\" style=\"border:1px solid " + q_(st.border) + "\"><tr><td>" + cell(card) + "</td></tr></table>";
            html += "</td>";
        }
        html += "</tr></table>";
    } else if (type == "actions") {
        const std::string display = d.value("id", std::string());
        for (std::size_t i = 0; i < d["buttons"].size(); ++i)
            html += "<a href=\"cluster-act:" + q_(display) + "/" + QString::number(i) + "\" style=\"text-decoration:none\"><span style=\"background:" + q_(st.accent)
                + "; color:" + q_(st.bg) + "\">&nbsp;" + (d["buttons"][i].contains("clicked") ? QString("✓ ") : QString()) + esc_(d["buttons"][i].value("label", std::string()))
                + "&nbsp;</span></a>&nbsp;&nbsp;";
    } else if (type == "form") {
        const cluster::Json values = d.value("values", cluster::Json::object());
        html += "<table cellpadding=\"3\">";
        for (const cluster::Json& field: d["fields"]) {
            const std::string name = field.value("name", std::string());
            html += "<tr><td>" + span_(st.muted, esc_(field.value("label", name))) + "</td><td>" + (values.contains(name) ? cell(values[name]) : field.contains("value")
                ? cell(field["value"]) : QString("—")) + "</td></tr>";
        }
        html += "</table>" + (d.value("submitted", false) ? span_(st.ok, "✓ sent")
            : "<a href=\"cluster-form:" + q_(d.value("id", std::string())) + "\">" + span_(st.accent, "<b>Fill the form…</b>") + "</a>");
    } else if (type == "details") {
        html += "<b>" + esc_((expanded ? "▾ " : "▸ ") + d.value("summary", std::string())) + "</b>";
        if (expanded) html += "<pre>" + esc_(d.value("text", std::string())) + "</pre>";
        else html += " " + span_(st.muted, "(Ctrl+O)");
    } else if (type == "compare") {
        html += "<table width=\"100%\" cellpadding=\"5\" border=\"1\" cellspacing=\"0\" style=\"border-color:" + q_(st.border) + "\"><tr><th>" + esc_(d["left"].value("title", std::string("A")))
            + "</th><th>" + esc_(d["right"].value("title", std::string("B"))) + "</th></tr><tr><td valign=\"top\"><pre>" + esc_(d["left"].value("text", std::string()))
            + "</pre></td><td valign=\"top\"><pre>" + esc_(d["right"].value("text", std::string())) + "</pre></td></tr></table>";
    } else if (type == "metrics") {
        html += "<table cellspacing=\"6\"><tr>";
        for (const cluster::Json& m: d["items"]) {
            const std::string delta = m.contains("delta") ? (m["delta"].is_string() ? m["delta"].get<std::string>() : m["delta"].dump()) : "";
            html += "<td bgcolor=\"" + q_(st.surface) + "\" width=\"140\">" + span_(st.muted, esc_(m.value("label", std::string()))) + "<br><span style=\"font-size:18pt\"><b>"
                + cell(m.value("value", cluster::Json(""))) + esc_(m.value("unit", std::string())) + "</b></span><br>"
                + (delta.empty() ? QString() : span_(delta.starts_with("-") ? st.error : st.ok, esc_((delta.starts_with("-") ? "▼ " : "▲ ") + delta)))
                + (m.contains("trend") ? " " + span_(st.accent, esc_(cluster::display_spark(m["trend"], 16))) : QString()) + "</td>";
        }
        html += "</tr></table>";
    } else if (type == "audio") {
        html += "<a href=\"cluster-play:" + q_(d.value("id", std::string())) + "\">" + span_(st.accent, "▶ <b>play</b>") + "</a> " + span_(st.muted, esc_(d.value("alt", d.value("text", std::string()))));
    } else if (type == "math") {
        html += "<span style=\"font-size:13pt\"><i>" + esc_(d.value("unicode", std::string())) + "</i></span>";
    } else if (type == "file") {
        // a card per file: its preview, its name, size and type, Open / Folder / Save as
        const cluster::Json files = d.value("files", cluster::Json::array());
        const QString display = q_(d.value("id", std::string()));
        html += "<table cellspacing=\"6\">";
        for (std::size_t i = 0; i < files.size(); ++i) {
            const cluster::Json& f = files[i];
            const std::uintmax_t size = f.value("size", static_cast<std::uintmax_t>(0));
            QString picture;
            if (f.contains("preview")) {
                const QString name = q_("file:" + key + ":" + std::to_string(i));
                if (!this->pictures.contains(name)) {
                    QImage image = QImage::fromData(QByteArray::fromBase64(QByteArray::fromStdString(f.value("preview", std::string()))));
                    if (!image.isNull()) this->pictures[name] = image.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                if (this->pictures.contains(name)) picture = "<img src=\"" + name + "\">";
            }
            const QString target = display + "/" + QString::number(i);
            html += "<tr><td valign=\"top\" bgcolor=\"" + q_(st.surface) + "\">" + (picture.isEmpty() ? span_(st.accent, "<span style=\"font-size:22pt\">▤</span>") : picture)
                + "</td><td valign=\"top\" nowrap bgcolor=\"" + q_(st.surface) + "\"><b>" + esc_(f.value("name", std::string())) + "</b><br>" + span_(st.muted,
                esc_((size >= 1048576 ? std::to_string(size / 1048576) + " MB" : std::to_string(std::max<std::uintmax_t>(1, size / 1024)) + " KB") + " · "
                + f.value("mime", std::string()))) + "<br><a href=\"cluster-open:" + target + "\">Open</a> · <a href=\"cluster-reveal:" + target + "\">Folder</a> · "
                "<a href=\"cluster-save:" + target + "\">Save as…</a></td></tr>";
        }
        html += "</table>";
    } else {
        // a chart drawn here; an image / a diagram / an SVG read as they are; given to the transcript as display:<entry>
        const QString name = q_("display:" + key);
        if (!this->pictures.contains(name)) {
            QImage image = type == "chart" ? chart_(d, st) : type == "timeline" ? timeline_(d, st) : type == "heatmap" ? heatmap_(d, st)
                : QImage::fromData(QByteArray::fromStdString(cluster::display_bytes(d)));
            if (image.width() > 680) image = image.scaledToWidth(680, Qt::SmoothTransformation);
            this->pictures[name] = image;
        }
        const QImage& image = this->pictures[name];
        if (image.isNull()) html += span_(st.muted, esc_("[" + type + " not readable] " + d.value("alt", std::string())));
        else html += "<img src=\"" + name + "\" width=\"" + QString::number(image.width()) + "\" height=\"" + QString::number(image.height()) + "\">";
        const std::string caption = d.value("alt", d.value("caption", std::string()));
        if (!caption.empty()) html += "<br>" + span_(st.muted, "<i>" + esc_(caption) + "</i>");
    }
    return html + "</div>";
}

SessionView::SessionView(Window* window, const std::string& sessionId, const bool compact)
    : QFrame(window), id(sessionId)
{
    this->setObjectName("session");
    QVBoxLayout* layout = new QVBoxLayout(this);
    this->header = new QLabel(this);
    this->header->setObjectName("head");
    this->header->setTextFormat(Qt::RichText);
    this->header->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    QObject::connect(this->header, &QLabel::linkActivated, [window, this](const QString& link) {
        // where it runs: its folder opened, a terminal there
        const std::optional<cluster::Snapshot> snap = window->manager.snapshot(this->id);
        if (!snap) return;
        const std::filesystem::path folder = cluster::expand_home(snap->spec.cwd);
        if (link == "cluster-folder:") {
            cluster::open_folder(folder);
        } else {
            const std::string terminal = cluster::open_terminal(folder);
            window->message(terminal.empty() ? "no terminal found" : terminal + " opened in " + folder.string(), terminal.empty());
        }
    });
    this->log = new QTextBrowser(this);
    this->log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    this->log->setOpenLinks(false);
    QObject::connect(this->log, &QTextBrowser::anchorClicked, [window, this](const QUrl& url) {
        // a display: tick / click (cluster-act:<display>/<item>), its form (cluster-form:<display>), its audio (cluster-play:<display>)
        const std::string scheme = url.scheme().toStdString();
        const std::string path = url.path().toStdString();
        if (scheme == "cluster-open" || scheme == "cluster-reveal" || scheme == "cluster-save") {
            // a file of a file display: opened, its folder, saved elsewhere
            const std::size_t slash = path.rfind('/');
            const std::optional<cluster::Json> found = window->manager.displayOf(this->id, path.substr(0, slash));
            std::size_t which = 0;
            try {
                which = static_cast<std::size_t>(std::stoul(path.substr(slash + 1)));
            } catch (const std::exception&) {}
            if (!found || !found->contains("files") || which >= (*found)["files"].size()) return;
            const QString file = q_((*found)["files"][which].value("path", std::string()));
            if (scheme == "cluster-open") QDesktopServices::openUrl(QUrl::fromLocalFile(file));
            else if (scheme == "cluster-reveal") QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).absolutePath()));
            else {
                const QString to = QFileDialog::getSaveFileName(this, "Save as", QDir::homePath() + "/" + QFileInfo(file).fileName());
                if (!to.isEmpty()) {
                    QFile::remove(to);
                    window->message(QFile::copy(file, to) ? "saved: " + to.toStdString() : "not saved", !QFile::exists(to));
                }
            }
            return;
        }
        if (scheme != "cluster-act" && scheme != "cluster-form" && scheme != "cluster-play") {
            QDesktopServices::openUrl(url);
            return;
        }
        if (scheme == "cluster-form") {
            this->displayForm(window, path);
            return;
        }
        try {
            const std::size_t slash = path.rfind('/');
            const std::size_t item = scheme == "cluster-act" && slash != std::string::npos ? static_cast<std::size_t>(std::stoul(path.substr(slash + 1))) : 0;
            const std::string display = scheme == "cluster-act" ? path.substr(0, slash) : path;
            window->message(window->manager.displayAct(this->id, display, {{"item", item}}), false);
        } catch (const std::exception& e) {
            window->message(e.what(), true);
        }
    });
    // the pictures of the displays: display:<entry> in the transcript
    this->log->document()->setResourceProvider([this](const QUrl& url) -> QVariant {
        const std::map<QString, QImage>::const_iterator it = this->pictures.find(url.toString());
        return it == this->pictures.end() ? QVariant() : QVariant(it->second);
    });
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

    // one card drawn by its two parts: the header (its top) and the body (its sides and bottom); the frame draws nothing
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QWidget* card = new QWidget(this);
    card->setObjectName("cardBody");
    card->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* inside = new QVBoxLayout(card);
    inside->setContentsMargins(4, 6, 8, 8);
    inside->setSpacing(8);
    QHBoxLayout* body = new QHBoxLayout();
    body->setSpacing(8);
    body->addWidget(this->log, 1);
    body->addWidget(this->panels);
    inside->addLayout(body, 1);
    inside->addWidget(this->permBar);
    layout->addWidget(this->header);
    layout->addWidget(card, 1);
    this->outline = new Outline(this);
    this->outline->raise();
}

void SessionView::update(const cluster::Snapshot& s, const cluster::Style& st, cluster::Manager& manager, const bool compact, cluster::Reveal& reveal, const bool expanded)
{
    static cluster::Git git; // lives as long as the program (its refresh threads)
    const cluster::Metrics& m = s.metrics;
    this->id = s.spec.id;

    // header
    if (cluster::now() - this->branchAt >= 5) { // its git branch, read again every 5 s
        this->branch = cluster::git_branch(cluster::expand_home(s.spec.cwd));
        this->branchAt = cluster::now();
    }
    const std::string model = s.modelUsed.empty() ? s.spec.backend : s.spec.backend.substr(0, s.spec.backend.find('/')) + "/" + s.modelUsed;
    const std::string tone = s.state == cluster::State::Error ? st.error : s.state == cluster::State::Working ? st.ok : s.state == cluster::State::Waiting ? st.warn : st.muted;
    const std::function<QString(const std::string&, const std::string&)> chip = [&st](const std::string& color, const std::string& text) {
        return "<span style=\"background:" + q_(mix_(st.surface, color, 0.2)) + ";color:" + q_(color) + ";font-family:monospace;font-size:9pt\">&nbsp;" + esc_(text)
            + "&nbsp;</span>";
    };
    QString head = "<span style=\"font-size:11.5pt\"><b>" + esc_(s.spec.name) + "</b></span>&nbsp;&nbsp;" + chip(tone, cluster::state_name(s.state)) + "&nbsp; "
        + chip(st.accent, model) + "&nbsp; " + chip(st.muted, "mode " + s.spec.mode)
        + (s.spec.effort.empty() || s.spec.effort == "default" ? QString() : "&nbsp; " + chip(st.muted, "effort " + s.spec.effort))
        + (compact ? QString() : "&nbsp;&nbsp;<a href=\"cluster-folder:\" style=\"color:" + q_(st.muted) + ";text-decoration:none;font-family:monospace;font-size:9pt\">"
            + esc_(cluster::short_path(s.spec.cwd)) + "</a>" + span_(st.muted, "<span style=\"font-family:monospace;font-size:9pt\">" + esc_(
                (this->branch.empty() ? std::string() : "  ⎇ " + this->branch) + "  " + cluster::host_name() + (s.pid > 0 ? "  pid " + std::to_string(s.pid) : ""))
            + "</span>") + "&nbsp; <a href=\"cluster-terminal:\" style=\"color:" + q_(st.accent) + ";text-decoration:none;font-size:9pt\">terminal</a>");
    if (!s.lastError.empty()) head += "<br>" + span_(st.error, esc_(cluster::one_line(s.lastError, 200)));
    if (!s.remoteUrl.empty() || !s.bridgeState.empty())
        head += "<br>" + span_(st.accent, "Remote Control " + esc_(s.bridgeState.empty() ? std::string("on") : s.bridgeState) + (s.remoteUrl.empty() ? QString()
            : ": <a href=\"" + q_(s.remoteUrl) + "\">" + esc_(s.remoteUrl) + "</a>")) + span_(st.muted, "  (live on claude.ai and here)");
    this->header->setText(head);
    // the links of the transcript in the accent (the dark blue of Qt unreadable on a dark style)
    const QString links = "a { color: " + q_(st.accent) + "; text-decoration: none; }";
    if (this->log->document()->defaultStyleSheet() != links) this->log->document()->setDefaultStyleSheet(links);
    if (this->outline) {
        const QColor color(q_(this->property("active").toBool() ? st.accent : st.border));
        if (color != this->outline->color) {
            this->outline->color = color;
            this->outline->update();
        }
    }

    // transcript (rebuilt only when it changed)
    const cluster::Config& config = manager.config();
    const cluster::Frame frame = reveal.step(s, config.typewriter, config.typewriterSpeed);
    const std::int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    std::size_t displays = 0;   // a display ticked / clicked / updated: rebuilt too
    for (const cluster::Entry& e: s.entries)
        if (e.kind == cluster::EntryKind::Display) displays += e.text.size();
    const std::string key = std::to_string(frame.visible) + "|" + std::to_string(frame.typedBytes) + "|" + std::to_string(frame.partialBytes) + "|" + std::to_string(s.thinking.size())
        + "|" + std::to_string(displays)
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
                case cluster::EntryKind::Display: {
                    const cluster::Json d = cluster::Json::parse(e.text, nullptr, false);
                    if (d.is_object()) html += this->displayHtml(d, s.spec.id + "-" + std::to_string(e.time) + "-" + std::to_string(i), st, expanded);
                    break;
                }
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
            // its step and how long, the tools running, the tokens as they come (Ctrl+O: every step)
            const cluster::Json turn = cluster::turn_state(s);
            const std::string tokens = turn.value("tokens", std::string());
            html += "<p>" + span_(st.accent, esc_(spinner[static_cast<std::size_t>(ms / 120) % spinner.size()] + " " + turn.value("verb", std::string()) + "… "
                + cluster::took(turn.value("step_ms", 0LL)) + "  (turn " + cluster::took(turn.value("turn_ms", since > 0 ? (cluster::now() - since) * 1000 : 0LL))
                + (tokens.empty() ? "" : " · ↓ " + tokens) + " · " + config.keys.at("interrupt") + " to interrupt" + (expanded ? "" : " · Ctrl+O details") + ")"));
            if (!turn.value("detail", std::string()).empty()) html += "<br>" + span_(st.muted, esc_("  ⎿ " + turn.value("detail", std::string())));
            for (const cluster::Json& tool: turn["tools"])
                html += "<br>" + span_(st.muted, esc_("  ⎿ " + tool.value("name", std::string()) + "  " + cluster::one_line(tool.value("summary", std::string()), 110)
                    + "  " + cluster::took(tool.value("ms", 0LL))));
            if (expanded) {
                for (const cluster::Json& round: turn["rounds"]) // a line per request to the model
                    html += "<br>" + span_(round.value("current", false) ? st.accent : st.muted, esc_("  │ " + round.value("text", std::string())));
                if (turn.value("hidden_thinking", false)) html += "<br>" + span_(st.muted, "<i>the text of its thinking is not sent by the model: its tokens only</i>");
            }
            html += "</p>";
        }
        QScrollBar* bar = this->log->verticalScrollBar();
        const bool bottom = bar->value() >= bar->maximum() - 4;
        const int value = bar->value();
        this->log->setHtml(html);
        bar->setValue(bottom ? bar->maximum() : value);
    }

    // permission bar (a question of this sub-session shown on the global session: it waits there)
    this->permBar->setVisible(!s.permissions.empty());
    const std::optional<cluster::Ask> asked = manager.asking().current();
    if (asked && asked->session == s.spec.id && s.spec.id != GLOBAL_ID && manager.config().questionsToGlobal) {
        this->permLabel->setText(span_(st.muted, q_(s.spec.name + " waits for your answer: its question is on the global session ("
            + manager.config().keys.at("global") + ")"), true));
    } else if (!s.permissions.empty()) {
        const cluster::Permission& p = s.permissions.front();
        this->permLabel->setText(span_(p.safety ? st.error : st.warn, p.safety ? "safety check" : "permission", true) + "  " + esc_(p.tool + ": " + cluster::one_line(p.description, 140))
            + (p.reason.empty() ? QString() : "<br>" + span_(st.muted, esc_("why: " + cluster::one_line(p.reason, 240)))));
    }

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
    QGuiApplication::setDesktopFileName("claude-cluster"); // the dock: its .desktop and its icon
    Window window(app);
    window.show();
    // tests: a picture of the window after a moment (CLAUDE_CLUSTER_GUI_SHOT=<file.png>)
    if (const char* shot = std::getenv("CLAUDE_CLUSTER_GUI_SHOT"); shot != nullptr && *shot != '\0') {
        const std::string path = shot;
        QTimer::singleShot(4000, &window, [&window, path]() {(void)window.grab().save(q_(path));});
    }
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
