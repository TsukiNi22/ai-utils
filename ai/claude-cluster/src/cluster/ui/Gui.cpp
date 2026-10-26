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
#include "cluster/Attachments.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Simple.hpp"
#include "cluster/RunProfile.hpp"
#include "cluster/Services.hpp"
#include "cluster/Showcase.hpp"
#include "cluster/Display.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Docs.hpp"
#include "cluster/Session.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Origin.hpp"
#include "cluster/WebTty.hpp"
#include "cluster/Worker.hpp"
#include "cluster/Editor.hpp"
#include "cluster/Reveal.hpp"
#include "cluster/Usage.hpp"
#include "cluster/Icons.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Cron.hpp"
#include "cluster/Core.hpp"
#include "cluster/Supervisor.hpp"
#include "cluster/Git.hpp"
#include "cluster/Vt.hpp"
#include <QAbstractTextDocumentLayout>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsEllipseItem>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QGraphicsLineItem>
#include <QContextMenuEvent>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QStringListModel>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QGraphicsScene>
#include <QFontDatabase>
#include <QTextDocument>
#include <QGestureEvent>
#include <QFontMetricsF>
#include <QGraphicsView>
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
#include <QCloseEvent>
#include <QFormLayout>
#include <QTextCursor>
#include <QScrollArea>
#include <QFileDialog>
#include <QWheelEvent>
#include <string_view>
#include <QToolButton>
#include <QScrollBar>
#include <QTextBlock>
#include <functional>
#include <QTabWidget>
#include <QTreeWidget>
#include <QClipboard>
#include <QCompleter>
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
#include <iterator>
#include <numeric>
#include <QBuffer>
#include <QCursor>
#include <fstream>
#include <QWidget>
#include <QTabBar>
#include <QDialog>
#include <QScreen>
#include <QHeaderView>
#include <QTableWidget>
#include <cstdlib>
#include <QImage>
#include <chrono>
#include <thread>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QMenu>
#include <QFile>
#include <regex>
#include <cmath>
#include <QDir>
#include <QUrl>
#include <set>
#include <map>

_cold static QString pill_(const std::string& text, const std::string& fg, const std::string& bg, const bool mono = true, const bool bold = false,
    const double points = 8.6, const bool outline = false)
{
    static std::map<std::string, QString> made;
    const std::string key = text + "\x1f" + fg + "\x1f" + bg + (mono ? "m" : "") + (bold ? "b" : "") + (outline ? "o" : "") + std::to_string(points);
    if (const auto it = made.find(key); it != made.end()) return it->second;
    QFont font = mono ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : QApplication::font();
    font.setPointSizeF(points * 2);
    font.setBold(bold);
    const QFontMetricsF metrics(font);
    const QString label = QString::fromStdString(text);
    const double height = std::ceil(metrics.height() + 6);
    const double width = std::ceil(metrics.horizontalAdvance(label) + 20);
    QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(outline ? QPen(QColor(QString::fromStdString(fg)), 2) : Qt::NoPen);
    painter.setBrush(outline ? QBrush(Qt::transparent) : QBrush(QColor(QString::fromStdString(bg))));
    painter.drawRoundedRect(QRectF(1, 1, width - 2, height - 2), (height - 2) / 2, (height - 2) / 2);
    painter.setPen(QColor(QString::fromStdString(fg)));
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, width, height), Qt::AlignCenter, label);
    painter.end();
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    const QString html = "<img src=\"data:image/png;base64," + QString::fromLatin1(png.toBase64()) + "\" width=\"" + QString::number(static_cast<int>(width / 2))
        + "\" height=\"" + QString::number(static_cast<int>(height / 2)) + "\" style=\"vertical-align:middle\">";
    if (made.size() > 600) made.clear();
    made[key] = html;
    return html;
}

_cold static std::string chip_color_(const std::string& label)
{
    // the color of a category / a tag: made from its text (the same label, the same color everywhere)
    std::uint32_t hash = 2166136261U;
    for (const char c: label)
        hash = (hash ^ static_cast<unsigned char>(c)) * 16777619U;
    return QColor::fromHsv(static_cast<int>(hash % 360), 150, 215).name().toStdString();
}

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

const QSize GRID_CELL(320, 190);    // a cell of the grid: its smallest size (the rows that do not fit scroll, the window never made larger)

// xstyle: ignore-next CPP-SINGLE-STATEMENT (helper of this file only)
_cold QString q_(const std::string& text)
{
    return QString::fromStdString(text);
}

_cold static QString preview_html_(const std::string& source, const cluster::Style& st)
{
    // the preview of an option of a question: markdown (headings, bullets, tables, fenced code, a diff in green / red), a picture (a path, ![](path)),
    // a display block ({"type": "image" | "table" | "chart"}), a mock-up in a fixed font
    const std::string text = cluster::trim(source);
    const std::function<QString(const std::string&)> escape = [](const std::string& s) {return QString::fromStdString(s).toHtmlEscaped();};
    const std::function<bool(const std::string&)> isPicture = [](const std::string& raw) {
        const std::filesystem::path path = cluster::expand_home(cluster::trim(raw));
        std::error_code error;
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});
        static const std::set<std::string> kinds = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".svg"};
        return kinds.contains(extension) && std::filesystem::is_regular_file(path, error);
    };
    const std::function<QString(const std::string&)> picture = [&](const std::string& raw) {
        return "<p><img src=\"" + QString::fromStdString(cluster::expand_home(cluster::trim(raw)).string()).toHtmlEscaped() + "\" width=\"420\"></p>";
    };
    const cluster::Json block = text.starts_with("{") ? cluster::Json::parse(text, nullptr, false) : cluster::Json();
    if (block.is_object() && block.contains("type")) {
        const std::string type = block.value("type", std::string());
        if (type == "image" && isPicture(block.value("path", std::string()))) return picture(block.value("path", std::string()));
        if (type == "table" && block.contains("rows") && block["rows"].is_array()) {
            QString html = "<table cellspacing=\"0\" cellpadding=\"4\">";
            if (block.contains("columns") && block["columns"].is_array()) {
                html += "<tr>";
                for (const cluster::Json& c: block["columns"]) html += "<th align=\"left\" style=\"color:" + q_(st.accent) + "\">" + escape(c.is_string() ? c.get<std::string>() : c.dump()) + "</th>";
                html += "</tr>";
            }
            for (const cluster::Json& r: block["rows"]) {
                html += "<tr>";
                if (r.is_array()) for (const cluster::Json& c: r) html += "<td>" + escape(c.is_string() ? c.get<std::string>() : c.dump()) + "</td>";
                html += "</tr>";
            }
            return html + "</table>";
        }
        if (type == "chart" && block.contains("series") && block["series"].is_array() && !block["series"].empty()) {
            const cluster::Json values = block["series"][0].value("values", cluster::Json::array());
            const cluster::Json labels = block.value("labels", cluster::Json::array());
            double biggest = 0.0;
            for (const cluster::Json& v: values) biggest = std::max(biggest, v.is_number() ? v.get<double>() : 0.0);
            QString html = block.contains("title") ? "<p><b>" + escape(block.value("title", std::string())) + "</b></p>" : QString();
            html += "<table cellspacing=\"2\">";
            for (std::size_t i = 0; i < values.size(); ++i) {
                const double v = values[i].is_number() ? values[i].get<double>() : 0.0;
                const int bar = biggest > 0 ? static_cast<int>(v / biggest * 260) : 0;
                html += "<tr><td>" + escape(i < labels.size() && labels[i].is_string() ? labels[i].get<std::string>() : std::to_string(i + 1)) + "</td><td><table cellspacing=\"0\"><tr><td width=\""
                    + QString::number(std::max(1, bar)) + "\" bgcolor=\"" + q_(st.accent) + "\">&nbsp;</td></tr></table></td><td>" + QString::number(v) + "</td></tr>";
            }
            return html + "</table>";
        }
    }
    const std::vector<std::string> lines = cluster::split(text, '\n');
    std::size_t counted = 0;
    std::size_t signs = 0;
    for (const std::string& line: lines) {
        if (cluster::trim(line).empty()) continue;
        ++counted;
        signs += (line.starts_with("+") || line.starts_with("-") || line.starts_with("@@") || line.starts_with("diff ") || line.starts_with(" ")) && !line.starts_with("- ") ? 1 : 0;
    }
    const bool wholeDiff = counted >= 2 && signs * 10 >= counted * 8 && text.find("```") == std::string::npos;
    const std::function<QString(const std::string&)> diffLine = [&](const std::string& line) {
        QString style = "color:" + q_(st.text);
        if (line.starts_with("+++") || line.starts_with("---") || line.starts_with("diff ")) style = "color:" + q_(st.muted) + ";font-weight:bold";
        else if (line.starts_with("@@")) style = "color:" + q_(st.accent);
        else if (line.starts_with("+")) style = "color:" + q_(st.ok) + ";background:rgba(60,200,100,0.14)";
        else if (line.starts_with("-")) style = "color:" + q_(st.error) + ";background:rgba(230,70,80,0.16)";
        return "<div style=\"white-space:pre;font-family:monospace;" + style + "\">" + escape(line.empty() ? " " : line) + "</div>";
    };
    QString html;
    bool fenced = false;
    std::string language;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        if (line.starts_with("```")) {
            fenced = !fenced;
            language = fenced ? cluster::trim(line.substr(3)) : std::string();
            continue;
        }
        if (wholeDiff || (fenced && (language == "diff" || language == "patch"))) {
            html += diffLine(line);
        } else if (fenced) {
            html += "<div style=\"white-space:pre;font-family:monospace;background:" + q_(st.surface) + ";\">&nbsp;" + escape(line) + "</div>";
        } else if (line.starts_with("![") && line.find("](") != std::string::npos && isPicture(line.substr(line.find("](") + 2, line.rfind(')') - line.find("](") - 2))) {
            html += picture(line.substr(line.find("](") + 2, line.rfind(')') - line.find("](") - 2));
        } else if (isPicture(line)) {
            html += picture(line);
        } else if (line.starts_with("|")) { // a table: its consecutive lines, the separator dropped
            std::vector<std::vector<std::string>> grid;
            for (; i < lines.size() && lines[i].starts_with("|"); ++i) {
                std::vector<std::string> cells = cluster::split(lines[i], '|');
                if (!cells.empty() && cluster::trim(cells.front()).empty()) cells.erase(cells.begin());
                if (!cells.empty() && cluster::trim(cells.back()).empty()) cells.pop_back();
                const bool separator = !cells.empty() && std::all_of(cells.begin(), cells.end(), [](const std::string& c) {return cluster::trim(c).find_first_not_of("-: ") == std::string::npos;});
                if (!separator) grid.push_back(cells);
            }
            --i;
            html += "<table cellspacing=\"0\" cellpadding=\"4\">";
            for (std::size_t r = 0; r < grid.size(); ++r) {
                html += "<tr>";
                for (const std::string& c: grid[r])
                    html += (r == 0 ? "<th align=\"left\" style=\"color:" + q_(st.accent) + "\">" : QString("<td>")) + escape(cluster::trim(c)) + (r == 0 ? "</th>" : "</td>");
                html += "</tr>";
            }
            html += "</table>";
        } else if (line.starts_with("#")) {
            html += "<p><b style=\"color:" + q_(st.accent) + "\">" + escape(cluster::trim(line.substr(line.find_first_not_of('#')))) + "</b></p>";
        } else if (line.starts_with("- ") || line.starts_with("* ")) {
            html += "<div>&bull; " + escape(line.substr(2)) + "</div>";
        } else if (cluster::trim(line).empty()) {
            html += "<div>&nbsp;</div>";
        } else if (line.starts_with("    ") || line.starts_with("\t") || line.find("  ") != std::string::npos) {
            html += "<div style=\"white-space:pre;font-family:monospace\">" + escape(line) + "</div>"; // a mock-up: its spaces kept
        } else {
            html += "<div>" + escape(line) + "</div>";
        }
    }
    return html;
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


// a session whose turn is over but whose shells / tasks / agents still run in the background: purple (~_~), idle only once they all ended
static std::string busy_color_(const cluster::Style& st)
{
    return st.dark ? "#bb8cf2" : "#8a3fb5";
}

class Window;

// Prompt of the window: QLineEdit (undo / redo / selection / clipboard of Qt) + the completion list and the images
class PromptEdit: public QPlainTextEdit {
    protected:
        void keyPressEvent(QKeyEvent* event) override;
        void insertFromMimeData(const QMimeData* source) override;  // a long text pasted: one [Pasted text #N]; files / images dropped or pasted: attached
        bool canInsertFromMimeData(const QMimeData* source) const override {return source->hasUrls() || source->hasImage() || QPlainTextEdit::canInsertFromMimeData(source);};
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
        _nodiscard const std::set<std::string>& knownSlash(void);   // the /words of the session shown (read again when the sessions changed)
        std::set<std::string> known;
        std::pair<std::string, std::uint64_t> knownKey;

        PromptEdit(Window* owner, QWidget* parent);
        void refreshCompletion(void);
        void apply(const bool final);
        bool addImage(const std::string& path, const bool insert = true); // false: this image was already in the prompt
        void attachFiles(const QStringList& paths, const int mode = 0);   // files of this computer: mode 0 asks link / copy, 1 copy, 2 link
        void attachBytes(const std::string& name, const std::string& bytes);   // a picture of the clipboard / a drop: copied
        void attachOne(const cluster::Attachment& item);                       // in the prompt: [Image #N] or @path
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
            // @files, ::sessions and the /commands / skills that exist like the atoms; a first /word that does not exist in red
            const std::string line = text.toStdString();
            for (const cluster::PromptMark& mark: cluster::prompt_marks(line, this->_edit->knownSlash())) {
                const bool unknown = mark.kind == cluster::MarkKind::Unknown;
                if (unknown && this->currentBlock().blockNumber() > 0) continue;
                QTextCharFormat shown = format;
                if (unknown) {
                    shown.setForeground(QColor(0xef, 0x6b, 0x5b));
                    shown.setBackground(Qt::transparent);
                    shown.setFontUnderline(true);
                }
                const int from = static_cast<int>(QString::fromStdString(line.substr(0, mark.start)).size());
                this->setFormat(from, static_cast<int>(QString::fromStdString(line.substr(mark.start, mark.end - mark.start)).size()), shown);
            }
        }

    public:
        AtomHighlighter(PromptEdit* owner): QSyntaxHighlighter(owner->document()), _edit(owner) { /* Nothing */ };
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
        QPushButton* newCue = nullptr;          // under the transcript: new output came while it is scrolled up (a click: the end)
        QFrame* pinBox = nullptr;               // the pinned item above the prompt: its title (folds it), Unpin, the item
        QPushButton* pinFold = nullptr;
        QTextBrowser* pinLog = nullptr;
        std::string pinKey;
        bool pinFolded = false;
        QLabel* panels = nullptr;
        bool plain = false;                     // the simple mode: no header line, no panels (its name above it)
        QWidget* permBar = nullptr;
        QLabel* permLabel = nullptr;
        std::string lastKey;
        std::size_t shownEntries = 400;         // the newest entries drawn (400 more each time its top is reached)
        std::size_t olderSeen = 0;              // its entries left in the log at the last drawing (the kept line follows a load / a trim)
        std::int64_t olderAsked = 0;            // when its older entries were last asked to the core (ms)
        std::map<QString, QImage> pictures;     // the pictures of the displays (display:<entry>), given to the transcript

        SessionView(Window* window, const std::string& sessionId, const bool compact);
        QString displayHtml(const cluster::Json& display, const std::string& key, const cluster::Style& style, const bool expanded);
        void displayForm(Window* window, const std::string& displayId);    // a form of a display filled, its values sent
        void entryMenu(Window* window, QTextBrowser* from, const QPoint& at);   // right click on a message / block: Pin / Unpin
        void update(const cluster::Snapshot& s, const cluster::Style& style, cluster::Manager& manager, const bool compact, cluster::Reveal& reveal, const bool expanded);
};

// The terminal interface inside the window: claude-cluster --tty in a pty, its screen drawn (the emulator), every key
// and the mouse of a terminal given to it, resized with the window
class TermView: public QWidget {
    private:
        cluster::Vt _vt{120, 36};
        cluster::Pty _pty;
        std::thread _reader;
        std::mutex _mutex;
        std::string _pending;           // what the pty wrote, not fed yet
        std::int64_t _held = 0;         // since when (ms) the end of the pending bytes is a frame not finished, 0: none
        std::atomic<bool> _alive{true};
        QFont _font;
        int _cellW = 8;
        int _cellH = 18;
        int _pressed = -1;

        void send(const std::string& bytes)
        {
            if (this->_pty.fd < 0) return;
            for (std::size_t written = 0; written < bytes.size();) {
                const ssize_t size = ::write(this->_pty.fd, bytes.data() + written, bytes.size() - written);
                if (size <= 0) return;
                written += static_cast<std::size_t>(size);
            }
        }

        void fit(void)
        {
            const int cols = std::max(20, this->width() / this->_cellW);
            const int rows = std::max(5, this->height() / this->_cellH);
            if (cols == this->_vt.cols() && rows == this->_vt.rows()) return;
            this->_vt.resize(cols, rows);
            cluster::pty_resize(this->_pty, cols, rows);
        }

        _nodiscard std::string mouseReport(const QMouseEvent* event, const int button, const bool release) const
        {
            const int x = std::max(1, event->pos().x() / this->_cellW + 1);
            const int y = std::max(1, event->pos().y() / this->_cellH + 1);
            return "\x1b[<" + std::to_string(button) + ";" + std::to_string(x) + ";" + std::to_string(y) + (release ? "m" : "M"); // xstyle: ignore LU-ANSI (a mouse report of a terminal)
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            {
                // only the whole frames (?2026h ... ?2026l) are fed: a frame cut in the middle (cursor hidden, lines
                // erased, not drawn yet) is kept for the next paint, never shown (a flicker); held 150 ms at most
                std::lock_guard<std::mutex> lock(this->_mutex);
                if (!this->_pending.empty()) {
                    static constexpr std::string_view BEGIN = "\x1b[?2026h"; // xstyle: ignore LU-ANSI (synchronized update)
                    static constexpr std::string_view END = "\x1b[?2026l";   // xstyle: ignore LU-ANSI (synchronized update)
                    std::size_t cut = this->_pending.size();
                    const std::size_t begin = this->_pending.rfind(BEGIN);
                    if (begin != std::string::npos && (this->_pending.rfind(END) == std::string::npos || this->_pending.rfind(END) < begin)) cut = begin;
                    if (cut < this->_pending.size() && this->_held == 0) this->_held = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                    if (cut < this->_pending.size() && now - this->_held >= 150) cut = this->_pending.size();
                    if (cut == this->_pending.size()) this->_held = 0;
                    else QTimer::singleShot(160, this, [this]() {this->update();});
                    this->_vt.feed(this->_pending.substr(0, cut));
                    this->_pending.erase(0, cut);
                }
            }
            QPainter painter(this);
            painter.fillRect(this->rect(), QColor(0x15, 0x17, 0x1b));
            painter.setFont(this->_font);
            const std::vector<std::vector<cluster::VtCell>>& grid = this->_vt.grid();
            for (int y = 0; y < this->_vt.rows(); ++y)
                for (int x = 0; x < this->_vt.cols(); ++x) {
                    const cluster::VtCell& c = grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                    QColor fg = c.fgSet ? QColor::fromRgb(static_cast<QRgb>(c.fg)) : QColor(0xe6, 0xe7, 0xea);
                    QColor bg = c.bgSet ? QColor::fromRgb(static_cast<QRgb>(c.bg)) : QColor(0x15, 0x17, 0x1b);
                    if (c.inverse) std::swap(fg, bg);
                    const QRect cell(x * this->_cellW, y * this->_cellH, this->_cellW * (c.ch.empty() ? 1 : cluster::Vt::width(c.ch) == 2 ? 2 : 1), this->_cellH);
                    if (c.bgSet || c.inverse) painter.fillRect(cell, bg);
                    if (this->_vt.cursorShown() && x == this->_vt.cursorX() && y == this->_vt.cursorY() && this->hasFocus()) painter.fillRect(cell, QColor(0xe0, 0x8a, 0x55, 140));
                    if (c.ch.empty() || c.ch == " ") continue;
                    QFont font = this->_font;
                    font.setBold(c.bold);
                    font.setItalic(c.italic);
                    font.setUnderline(c.underline);
                    font.setStrikeOut(c.strike);
                    painter.setFont(font);
                    if (c.dim) fg.setAlpha(140);
                    painter.setPen(fg);
                    painter.drawText(cell, Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(c.ch));
                }
            if (!this->_alive) {
                painter.setPen(QColor(0xef, 0x6b, 0x5b));
                painter.drawText(this->rect(), Qt::AlignCenter, "The terminal interface quit (Ctrl+Q): close this window, or open it again");
            }
        }

        void keyPressEvent(QKeyEvent* event) override
        {
            // what a terminal sends (arrows, function keys, Ctrl / Alt letters, the text)
            const bool ctrl = event->modifiers() & Qt::ControlModifier;
            const bool alt = event->modifiers() & Qt::AltModifier;
            const bool shift = event->modifiers() & Qt::ShiftModifier;
            const int mod = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0);
            const std::string csi = "\x1b["; // xstyle: ignore LU-ANSI (the keys of a terminal)
            const std::function<std::string(const char*)> arrow = [mod, &csi](const char* letter) {return mod == 1 ? csi + letter : csi + "1;" + std::to_string(mod) + letter;};
            const std::function<std::string(int)> tilde = [mod, &csi](const int n) {return mod == 1 ? csi + std::to_string(n) + "~" : csi + std::to_string(n) + ";" + std::to_string(mod) + "~";};
            std::string bytes;
            switch (event->key()) {
                case Qt::Key_Up: bytes = arrow("A"); break;
                case Qt::Key_Down: bytes = arrow("B"); break;
                case Qt::Key_Right: bytes = arrow("C"); break;
                case Qt::Key_Left: bytes = arrow("D"); break;
                case Qt::Key_Home: bytes = arrow("H"); break;
                case Qt::Key_End: bytes = arrow("F"); break;
                case Qt::Key_PageUp: bytes = tilde(5); break;
                case Qt::Key_PageDown: bytes = tilde(6); break;
                case Qt::Key_Delete: bytes = tilde(3); break;
                case Qt::Key_Insert: bytes = tilde(2); break;
                case Qt::Key_F1: bytes = mod == 1 ? "\x1bOP" : csi + "1;" + std::to_string(mod) + "P"; break;
                case Qt::Key_F2: bytes = mod == 1 ? "\x1bOQ" : csi + "1;" + std::to_string(mod) + "Q"; break;
                case Qt::Key_F3: bytes = mod == 1 ? "\x1bOR" : csi + "1;" + std::to_string(mod) + "R"; break;
                case Qt::Key_F4: bytes = mod == 1 ? "\x1bOS" : csi + "1;" + std::to_string(mod) + "S"; break;
                case Qt::Key_F5: bytes = tilde(15); break;
                case Qt::Key_F6: bytes = tilde(17); break;
                case Qt::Key_F7: bytes = tilde(18); break;
                case Qt::Key_F8: bytes = tilde(19); break;
                case Qt::Key_F9: bytes = tilde(20); break;
                case Qt::Key_F10: bytes = tilde(21); break;
                case Qt::Key_F11: bytes = tilde(23); break;
                case Qt::Key_F12: bytes = tilde(24); break;
                case Qt::Key_Return:
                case Qt::Key_Enter: bytes = alt || shift ? "\x1b\r" : "\r"; break;
                case Qt::Key_Backspace: bytes = alt ? "\x1b\x7f" : "\x7f"; break;
                case Qt::Key_Tab: bytes = "\t"; break;
                case Qt::Key_Backtab: bytes = csi + "Z"; break;
                case Qt::Key_Escape: bytes = "\x1b"; break;
                default: {
                    const QString text = event->text();
                    if (ctrl) {
                        const int key = event->key();
                        if (key >= Qt::Key_A && key <= Qt::Key_Z) bytes = std::string(1, static_cast<char>(key - Qt::Key_A + 1));
                        else if (key == Qt::Key_Space) bytes = std::string(1, '\0');
                        else if (key == Qt::Key_BracketLeft) bytes = "\x1b";
                        else if (key == Qt::Key_Backslash) bytes = "\x1c";
                        else if (key == Qt::Key_BracketRight) bytes = "\x1d";
                    } else if (!text.isEmpty()) {
                        bytes = (alt ? "\x1b" : "") + text.toStdString();
                    }
                }
            }
            if (bytes.empty()) {
                QWidget::keyPressEvent(event);
                return;
            }
            this->send(bytes);
            event->accept();
        }

        void mousePressEvent(QMouseEvent* event) override
        {
            this->setFocus();
            if (!this->_vt.mouse()) return;
            this->_pressed = event->button() == Qt::RightButton ? 2 : event->button() == Qt::MiddleButton ? 1 : 0;
            this->send(this->mouseReport(event, this->_pressed, false));
        }

        void mouseReleaseEvent(QMouseEvent* event) override
        {
            if (!this->_vt.mouse()) return;
            this->send(this->mouseReport(event, this->_pressed < 0 ? 0 : this->_pressed, true));
            this->_pressed = -1;
        }

        void mouseMoveEvent(QMouseEvent* event) override
        {
            if (this->_vt.mouse() && this->_pressed >= 0) this->send(this->mouseReport(event, 32 + this->_pressed, false));
        }

        void wheelEvent(QWheelEvent* event) override
        {
            if (!this->_vt.mouse()) return;
            const int x = std::max(1, static_cast<int>(event->position().x()) / this->_cellW + 1);
            const int y = std::max(1, static_cast<int>(event->position().y()) / this->_cellH + 1);
            this->send("\x1b[<" + std::string(event->angleDelta().y() > 0 ? "64" : "65") + ";" + std::to_string(x) + ";" + std::to_string(y) + "M"); // xstyle: ignore LU-ANSI (the wheel of a terminal)
            event->accept();
        }

        void resizeEvent(QResizeEvent*) override
        {
            this->fit();
        }

        bool focusNextPrevChild(bool) override
        {
            return false; // Tab / Shift+Tab: keys of the terminal, never the focus chain
        }

        void focusInEvent(QFocusEvent*) override
        {
            this->send("\x1b[I"); // focus reporting: in - xstyle: ignore LU-ANSI (a report of a terminal)
            this->update();
        }

        void focusOutEvent(QFocusEvent*) override
        {
            this->send("\x1b[O"); // xstyle: ignore LU-ANSI (focus out)
            this->update();
        }

    public:
        explicit TermView(QWidget* parent): QWidget(parent)
        {
            this->_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
            this->_font.setPointSize(11);
            const QFontMetrics metrics(this->_font);
            this->_cellW = std::max(6, metrics.horizontalAdvance("M"));
            this->_cellH = std::max(12, metrics.height() + 2);
            this->setFocusPolicy(Qt::StrongFocus);
            this->setMouseTracking(true);
            this->setCursor(Qt::IBeamCursor);
            this->setAttribute(Qt::WA_OpaquePaintEvent);
            this->_pty = cluster::pty_spawn(this->_vt.cols(), this->_vt.rows(), false);
            this->_reader = std::thread([this]() {
                std::array<char, 16384> chunk{};
                while (true) {
                    const ssize_t size = ::read(this->_pty.fd, chunk.data(), chunk.size());
                    if (size <= 0) break;
                    {
                        std::lock_guard<std::mutex> lock(this->_mutex);
                        this->_pending.append(chunk.data(), static_cast<std::size_t>(size));
                    }
                    QMetaObject::invokeMethod(this, [this]() {this->update();}, Qt::QueuedConnection);
                }
                this->_alive = false;
                QMetaObject::invokeMethod(this, [this]() {this->update();}, Qt::QueuedConnection);
            });
        }

        ~TermView() override
        {
            cluster::pty_kill(this->_pty); // the read ends (EIO), its thread joined, then only the fd closed
            if (this->_reader.joinable()) this->_reader.join();
            cluster::pty_close(this->_pty);
        }

        _nodiscard bool alive(void) const {return this->_alive;};
};

// The hub: the global session in the middle, the sessions around it, a graph moved (a drag, the arrows, a finger) and
// zoomed (the wheel, a / e, two fingers, + -; double click: whole); a click on a node opens its session
class HubView: public QGraphicsView {
    private:
        QGraphicsScene _scene;
        QPoint _pressed;
        std::string _pressedId;
        std::string _selected;                  // the node chosen with the keys (Tab / Shift+Tab): outlined, Enter opens it
        QGraphicsRectItem* _mark = nullptr;
        QColor _markColor;

        struct LabelSpot {
            double x = 0;
            double y = 0;
            double r = 0;
            QString title;
            QString sub;
            bool pinned = false;        // the one shown: its label first, always drawn
            QString id;                 // carried by its text (a click on it is a click on the node)
        };

        std::vector<std::pair<std::string, QRectF>> nodes_(void) const
        {
            // the nodes drawn (a session, a category / tag) and the box of their items; the tree: left to right then top to bottom, the ring: around
            std::map<std::string, QRectF> boxes;
            for (QGraphicsItem* item: this->_scene.items()) {
                const std::string id = item->data(0).toString().toStdString();
                if (id.empty() || id.starts_with("fold:") || item == this->_mark) continue;
                QRectF& box = boxes[id];
                box = box.isNull() ? item->sceneBoundingRect() : box.united(item->sceneBoundingRect());
            }
            std::vector<std::pair<std::string, QRectF>> all(boxes.begin(), boxes.end());
            const std::function<std::pair<double, double>(const QRectF&)> rank = [this](const QRectF& box) {
                if (!this->ring) return std::pair<double, double>(std::round(box.center().x() / 40), box.center().y());
                return std::pair<double, double>(QLineF(QPointF(0, 0), box.center()).length() < 40 ? -10 : std::atan2(box.center().y(), box.center().x()), 0);
            };
            std::sort(all.begin(), all.end(), [&rank](const std::pair<std::string, QRectF>& a, const std::pair<std::string, QRectF>& b) {return rank(a.second) < rank(b.second);});
            return all;
        }

        void markSelected_(void)
        {
            // the node chosen: a dashed box around it (drawn again with the scene)
            if (this->_mark) {
                this->_scene.removeItem(this->_mark);
                delete this->_mark;
                this->_mark = nullptr;
            }
            if (this->_selected.empty()) return;
            for (const auto &[id, box]: this->nodes_())
                if (id == this->_selected) {
                    this->_mark = this->_scene.addRect(box.adjusted(-6, -6, 6, 6), QPen(this->_markColor, 2, Qt::DashLine), Qt::NoBrush);
                    this->_mark->setZValue(10);
                    return;
                }
        }

        void step_(const int delta)
        {
            // Tab / Shift+Tab: the next / previous node chosen, brought into view
            const std::vector<std::pair<std::string, QRectF>> all = this->nodes_();
            if (all.empty()) return;
            const int count = static_cast<int>(all.size());
            const auto it = std::find_if(all.begin(), all.end(), [this](const std::pair<std::string, QRectF>& node) {return node.first == this->_selected;});
            const int index = it == all.end() ? (delta > 0 ? 0 : count - 1) : (static_cast<int>(it - all.begin()) + delta + count) % count;
            this->_selected = all[static_cast<std::size_t>(index)].first;
            this->markSelected_();
            this->ensureVisible(all[static_cast<std::size_t>(index)].second, 60, 60);
        }

        bool pressed_(const std::string& action, const QKeyEvent* event) const
        {
            // the key of an action of the graph (its alternatives separated by |, as in the terminal); Shift only counts when the key names it
            const auto found = this->keys.find(action);
            if (found == this->keys.end()) return false;
            const int key = event->key() == Qt::Key_Backtab ? Qt::Key_Tab : event->key() == Qt::Key_Enter ? Qt::Key_Return : event->key();
            const Qt::KeyboardModifiers held = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
            for (const std::string& alternative: cluster::split(found->second, '|')) {
                const QKeySequence sequence(q_(alternative));
                if (sequence.isEmpty()) continue;
                const int wanted = sequence[0].key() == Qt::Key_Enter ? Qt::Key_Return : sequence[0].key();
                const Qt::KeyboardModifiers modifiers = sequence[0].keyboardModifiers();
                const bool shiftMatters = (modifiers & Qt::ShiftModifier) || wanted == Qt::Key_Tab;
                if (wanted == key && (held & ~Qt::ShiftModifier) == (modifiers & ~Qt::ShiftModifier)
                    && (!shiftMatters || (held & Qt::ShiftModifier) == (modifiers & Qt::ShiftModifier)))
                    return true;
            }
            return false;
        }

        void zoomBy(const double factor)
        {
            const double now = this->transform().m11();
            const double wanted = std::clamp(now * factor, 0.2, 6.0);
            this->scale(wanted / now, wanted / now);
        }

    protected:
        void wheelEvent(QWheelEvent* event) override
        {
            this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            this->zoomBy(event->angleDelta().y() > 0 ? 1.15 : 1 / 1.15);
            event->accept();
        }

        void keyPressEvent(QKeyEvent* event) override
        {
            const int step = 40;
            this->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
            if (this->pressed_("graph_left", event)) this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() - step);
            else if (this->pressed_("graph_right", event)) this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() + step);
            else if (this->pressed_("graph_up", event)) this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - step);
            else if (this->pressed_("graph_down", event)) this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() + step);
            else if (this->pressed_("graph_zoom_in", event)) this->zoomBy(1.2);
            else if (this->pressed_("graph_zoom_out", event)) this->zoomBy(1 / 1.2);
            else if (this->pressed_("graph_fit", event)) this->whole();
            else if (this->pressed_("graph_prev", event)) this->step_(-1);
            else if (this->pressed_("graph_next", event)) this->step_(1);
            else if (this->pressed_("graph_open", event) && !this->_selected.empty()) {
                const std::string node = this->_selected;
                if (node.starts_with("label:")) {
                    if (this->onNode) this->onNode(node.substr(6));
                } else if (this->onOpen) {
                    this->onOpen(node);
                }
            } else if (this->pressed_("graph_focus", event) && this->_selected.starts_with("label:")) {
                // the tree rooted at the node chosen (again: the whole tree)
                const std::string node = this->_selected.substr(6);
                this->focus = this->focus == node ? std::string() : node;
                if (this->onRedraw) this->onRedraw();
            } else if (this->pressed_("graph_close", event)) {
                if (this->onClose) this->onClose();
            } else {
                QGraphicsView::keyPressEvent(event);
                return;
            }
            event->accept();
        }

        void mousePressEvent(QMouseEvent* event) override
        {
            // a node pressed: opened at the release when the mouse did not move (a drag moves the graph)
            this->_pressed = event->pos();
            this->_pressedId.clear();
            for (QGraphicsItem* item: this->items(event->pos()))
                if (!item->data(0).toString().isEmpty()) {
                    this->_pressedId = item->data(0).toString().toStdString();
                    break;
                }
            QGraphicsView::mousePressEvent(event);
        }

        void mouseReleaseEvent(QMouseEvent* event) override
        {
            QGraphicsView::mouseReleaseEvent(event);
            if (!this->_pressedId.empty() && (event->pos() - this->_pressed).manhattanLength() < 6) {
                if (this->_pressedId.starts_with("label:")) {   // a node of the taxonomy: the list filtered to it
                    if (this->onNode) this->onNode(this->_pressedId.substr(6));
                } else if (this->_pressedId.starts_with("fold:")) {
                    const std::string node = this->_pressedId.substr(5);
                    if (this->collapsed.erase(node) == 0) this->collapsed.insert(node);
                    if (this->onRedraw) this->onRedraw();
                } else if (this->onOpen) {
                    this->onOpen(this->_pressedId);
                }
            }
            this->_pressedId.clear();
        }

        void mouseDoubleClickEvent(QMouseEvent* event) override
        {
            if (this->items(event->pos()).empty()) this->whole();
            else QGraphicsView::mouseDoubleClickEvent(event);
        }

        void contextMenuEvent(QContextMenuEvent* event) override
        {
            // a right click: the menu of the node under it (a category / tag), else the one of the graph
            std::string node;
            for (QGraphicsItem* item: this->items(event->pos())) {
                const std::string id = item->data(0).toString().toStdString();
                if (id.starts_with("label:")) {
                    node = id.substr(6);
                    break;
                }
            }
            if (this->onMenu) this->onMenu(node, event->globalPos());
            event->accept();
        }

        bool event(QEvent* event) override
        {
            // Tab / Shift+Tab: the nodes of the graph (not the next widget)
            if (event->type() == QEvent::KeyPress) {
                QKeyEvent* key = static_cast<QKeyEvent*>(event);
                if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) {
                    this->keyPressEvent(key);
                    return true;
                }
            }
            // two fingers: zoomed around the middle of the view
            if (event->type() == QEvent::Gesture)
                if (QPinchGesture* pinch = static_cast<QPinchGesture*>(static_cast<QGestureEvent*>(event)->gesture(Qt::PinchGesture))) {
                    this->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
                    this->zoomBy(pinch->scaleFactor());
                    return true;
                }
            return QGraphicsView::event(event);
        }

    public:
        std::function<void(const std::string&)> onOpen;
        std::function<void(const std::string&)> onNode;                     // a node of the taxonomy clicked: the list filtered to it (again: cleared)
        std::function<void(const std::string&, const QPoint&)> onMenu;      // a right click: the node under it (empty: the graph)
        std::function<void(void)> onRedraw;                                 // a node folded / unfolded: drawn again
        std::function<void(void)> onClose;                                  // Escape: back to the list
        std::map<std::string, std::string> keys;                            // the keys of the settings (graph_*)
        bool ring = false;                      // the sessions around global, not the tree of the taxonomy
        bool archived = false;                  // the archived sessions greyed in the tree
        bool byTags = false;                    // a session under each of its labels, not under its primary one only
        std::string focus;                      // the tree rooted at this node (empty: global)
        std::string filter;                     // the node chosen (outlined): the list shows what is below it
        std::set<std::string> collapsed;        // the folded nodes

        explicit HubView(QWidget* parent): QGraphicsView(parent)
        {
            this->setScene(&this->_scene);
            this->setSceneRect(-1400, -1000, 2800, 2000);
            this->setRenderHint(QPainter::Antialiasing);
            this->setDragMode(QGraphicsView::ScrollHandDrag);
            this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            this->setFrameShape(QFrame::NoFrame);
            this->setFocusPolicy(Qt::StrongFocus);
            this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            this->grabGesture(Qt::PinchGesture);
            this->setToolTip("drag · wheel · z q s d / arrows · e / a zoom · Tab: a node, Enter: open it · two fingers · double click: whole");
        }

        void whole(void)
        {
            this->resetTransform();
            this->centerOn(0, 0);
        }

        void populateRing_(const std::vector<cluster::Snapshot>& sessions, const cluster::Style& st, const std::string& active)
        {
            // drawn again (the view kept where it is): the lines, the nodes around, the middle one
            this->_mark = nullptr; // removed with the scene
            this->_scene.clear();
            this->_scene.setBackgroundBrush(QColor(QString::fromStdString(st.bg)));
            const cluster::Snapshot* global = nullptr;
            std::vector<const cluster::Snapshot*> others;
            for (const cluster::Snapshot& s: sessions)
                (s.spec.id == GLOBAL_ID ? global : others.emplace_back()) = &s;
            const std::function<QColor(const cluster::Snapshot&)> color = [&st](const cluster::Snapshot& s) {
                return QColor(QString::fromStdString(!s.permissions.empty() ? st.warn : s.state == cluster::State::Working || s.state == cluster::State::Starting ? st.ok
                    : s.state == cluster::State::Error ? st.error : cluster::background_busy(s) ? busy_color_(st) : st.muted));
            };
            const double radius = std::max(170.0, static_cast<double>(others.size()) * 92.0 / (2.0 * M_PI));
            std::vector<LabelSpot> spots;           // the text of each node, placed once all of them are drawn (no text over a node or another text)
            std::vector<QRectF> spokes;             // the key points of the lines: obstacles of the labels
            const std::function<void(const cluster::Snapshot&, double, double, double)> node = [this, &st, &color, &active, &spots](const cluster::Snapshot& s, const double x, const double y, const double r) {
                QGraphicsEllipseItem* circle = this->_scene.addEllipse(x - r, y - r, 2 * r, 2 * r, QPen(color(s), s.spec.id == active ? 3.5 : 2), QBrush(QColor(QString::fromStdString(st.surface))));
                circle->setData(0, QString::fromStdString(s.spec.id));
                circle->setCursor(Qt::PointingHandCursor);
                circle->setToolTip(QString::fromStdString(s.spec.name + " · " + cluster::state_name(s.state) + " · " + cluster::short_path(s.spec.cwd)));
                const char* face = !s.permissions.empty() ? "•_•" : s.state == cluster::State::Working ? ">_<" : s.state == cluster::State::Error ? "o_o" : cluster::background_busy(s) ? "~_~" : "-_-";
                QGraphicsSimpleTextItem* faceItem = this->_scene.addSimpleText(face, QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), r > 40 ? 13 : 10));
                faceItem->setBrush(color(s));
                faceItem->setPos(x - faceItem->boundingRect().width() / 2, y - faceItem->boundingRect().height() / 2);
                faceItem->setData(0, QString::fromStdString(s.spec.id));
                spots.push_back({x, y, r, QString::fromStdString(cluster::one_line(s.spec.global ? std::string("global") : s.spec.name, 22)),
                    QString::fromStdString(s.state == cluster::State::Working ? cluster::turn_state(s).value("verb", std::string("working")) : cluster::background_busy(s)
                        ? std::string("~_~ background") : cluster::state_name(s.state)),
                    s.spec.id == active || s.spec.global, QString::fromStdString(s.spec.id)});
            };
            for (std::size_t i = 0; i < others.size(); ++i) {
                const double a = (static_cast<double>(i) / static_cast<double>(std::max<std::size_t>(1, others.size()))) * 2.0 * M_PI - M_PI / 2.0;
                const double x = std::cos(a) * radius;
                const double y = std::sin(a) * radius * 0.78;
                const bool solid = others[i]->state == cluster::State::Working || cluster::background_busy(*others[i]); // its background runs: solid too
                QPen pen(color(*others[i]), solid ? 2.5 : 1.5);
                if (!solid) pen.setStyle(Qt::DashLine);
                this->_scene.addLine(0, 0, x, y, pen);
                for (const double t: {0.55, 0.7, 0.85})
                    spokes.emplace_back(x * t - 3, y * t - 3, 6, 6);
            }
            for (std::size_t i = 0; i < others.size(); ++i) {
                const double a = (static_cast<double>(i) / static_cast<double>(std::max<std::size_t>(1, others.size()))) * 2.0 * M_PI - M_PI / 2.0;
                node(*others[i], std::cos(a) * radius, std::sin(a) * radius * 0.78, 30);
            }
            if (global != nullptr) {
                node(*global, 0, 0, 46);
            } else { // no global session: claude-cluster itself in the middle
                QPen pen(QColor(QString::fromStdString(st.accent)), 2);
                pen.setStyle(Qt::DotLine);
                this->_scene.addEllipse(-40, -40, 80, 80, pen, QBrush(QColor(QString::fromStdString(st.surface))));
                QGraphicsSimpleTextItem* face = this->_scene.addSimpleText("^_^", QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), 12));
                face->setBrush(QColor(QString::fromStdString(st.accent)));
                face->setPos(-face->boundingRect().width() / 2, -face->boundingRect().height() / 2);
                QGraphicsSimpleTextItem* name = this->_scene.addSimpleText("claude-cluster");
                name->setBrush(QColor(QString::fromStdString(st.muted)));
                name->setPos(-name->boundingRect().width() / 2, 46);
            }
            this->placeLabels_(spots, spokes, st);
        }

        void placeLabels_(const std::vector<LabelSpot>& spots, const std::vector<QRectF>& obstacles, const cluster::Style& st)
        {
            // the text of a node (title, status) in a box tried below, above, right, left, then the diagonals; the nodes and the key points of the edges
            // are obstacles (a grid of cells keeps it cheap: no pass over every pair); no room: the status line goes, then the end of the title, then the
            // label (its tooltip has it); the node shown keeps its label, on its plate; edges under the nodes under the labels, a plate under each text
            QFont bold;
            bold.setBold(true);
            const QFontMetricsF boldMetrics(bold);
            const QFontMetricsF plainMetrics((QFont()));
            std::map<std::pair<int, int>, std::vector<QRectF>> grid;
            const std::function<std::vector<std::pair<int, int>>(const QRectF&)> cells = [](const QRectF& box) {
                std::vector<std::pair<int, int>> out;
                for (int i = static_cast<int>(std::floor(box.left() / 80)); i <= static_cast<int>(std::floor(box.right() / 80)); ++i)
                    for (int j = static_cast<int>(std::floor(box.top() / 80)); j <= static_cast<int>(std::floor(box.bottom() / 80)); ++j)
                        out.emplace_back(i, j);
                return out;
            };
            const std::function<void(const QRectF&)> add = [&](const QRectF& box) {
                for (const std::pair<int, int>& cell: cells(box))
                    grid[cell].push_back(box);
            };
            const std::function<bool(const QRectF&)> isFree = [&](const QRectF& box) {
                for (const std::pair<int, int>& cell: cells(box)) {
                    const auto found = grid.find(cell);
                    if (found == grid.end()) continue;
                    for (const QRectF& other: found->second)
                        if (box.intersects(other)) return false;
                }
                return true;
            };
            for (const LabelSpot& n: spots)
                add(QRectF(n.x - n.r - 2, n.y - n.r - 2, 2 * n.r + 4, 2 * n.r + 4));
            for (const QRectF& o: obstacles)
                add(o);
            std::vector<std::size_t> order(spots.size());
            std::iota(order.begin(), order.end(), static_cast<std::size_t>(0));
            std::stable_sort(order.begin(), order.end(), [&spots](const std::size_t a, const std::size_t b) {
                if (spots[a].pinned != spots[b].pinned) return spots[a].pinned;     // the one shown first, then top to bottom
                return spots[a].y != spots[b].y ? spots[a].y < spots[b].y : spots[a].x < spots[b].x;
            });
            struct Wording {
                QString title;
                QString sub;
            };
            const double gap = 5;
            for (const std::size_t index: order) {
                const LabelSpot& n = spots[index];
                const QString shortTitle = n.title.size() > 13 ? n.title.left(12) + "…" : n.title;
                QRectF box;
                int side = 0;       // 0: centered, 1: starts at the box, 2: ends at it
                QString title;
                QString sub;
                bool placed = false;
                for (const Wording& wording: {Wording{n.title, n.sub}, Wording{n.title, QString()}, Wording{shortTitle, QString()}}) {
                    const double w = std::max(boldMetrics.horizontalAdvance(wording.title), plainMetrics.horizontalAdvance(wording.sub)) + 8;
                    const double h = boldMetrics.height() + (wording.sub.isEmpty() ? 0.0 : plainMetrics.height()) + 4;
                    const double d = n.r * 0.7;
                    const std::vector<std::pair<QPointF, int>> candidates = {
                        {QPointF(n.x - w / 2, n.y + n.r + gap), 0}, {QPointF(n.x - w / 2, n.y - n.r - gap - h), 0},
                        {QPointF(n.x + n.r + gap, n.y - h / 2), 1}, {QPointF(n.x - n.r - gap - w, n.y - h / 2), 2},
                        {QPointF(n.x + d, n.y + d + gap), 1}, {QPointF(n.x - d - w, n.y + d + gap), 2},
                        {QPointF(n.x + d, n.y - d - gap - h), 1}, {QPointF(n.x - d - w, n.y - d - gap - h), 2},
                    };
                    for (const std::pair<QPointF, int>& candidate: candidates) {
                        const QRectF wanted(candidate.first.x(), candidate.first.y(), w, h);
                        if (!isFree(wanted)) continue;
                        box = wanted;
                        side = candidate.second;
                        title = wording.title;
                        sub = wording.sub;
                        placed = true;
                        break;
                    }
                    if (placed) break;
                }
                if (!placed && !n.pinned) continue;     // no room: no label (the tooltip of the node has it)
                if (!placed) {                          // the node shown: below it, on its plate, over what is there
                    title = n.title;
                    sub = n.sub;
                    const double w = std::max(boldMetrics.horizontalAdvance(title), plainMetrics.horizontalAdvance(sub)) + 8;
                    box = QRectF(n.x - w / 2, n.y + n.r + gap, w, boldMetrics.height() + plainMetrics.height() + 4);
                    side = 0;
                }
                add(box);
                QColor plate(QString::fromStdString(st.bg));
                plate.setAlpha(204);
                QGraphicsRectItem* back = this->_scene.addRect(box, QPen(Qt::NoPen), QBrush(plate));
                back->setZValue(2);
                back->setData(0, n.id);
                const std::function<double(const double)> at = [&](const double width) {return side == 0 ? box.center().x() - width / 2 : side == 1 ? box.left() + 4 : box.right() - 4 - width;};
                QGraphicsSimpleTextItem* name = this->_scene.addSimpleText(title, bold);
                name->setBrush(QColor(QString::fromStdString(st.text)));
                name->setPos(at(name->boundingRect().width()), box.top() + 2);
                name->setZValue(3);
                name->setData(0, n.id);
                if (!sub.isEmpty()) {
                    QGraphicsSimpleTextItem* state = this->_scene.addSimpleText(sub);
                    state->setBrush(QColor(QString::fromStdString(st.muted)));
                    state->setPos(at(state->boundingRect().width()), box.top() + 2 + boldMetrics.height());
                    state->setZValue(3);
                    state->setData(0, n.id);
                }
            }
        }

        void populateTree_(const std::vector<cluster::Snapshot>& sessions, const cluster::Style& st, const cluster::Taxonomy& taxonomy,
            const std::vector<cluster::SessionSpec>& archivedSpecs)
        {
            // global at the root, then the categories / tags (the taxonomy, colored like their chips, with the number of sessions below), then the sessions as
            // leaves; left to right, a column per depth as wide as its widest node (no text over another); a folded node shows its count only
            this->_mark = nullptr; // removed with the scene
            this->_scene.clear();
            this->_scene.setBackgroundBrush(QColor(QString::fromStdString(st.bg)));
            struct Item {
                const cluster::Snapshot* snap = nullptr;
                const cluster::SessionSpec* spec = nullptr;
                bool archived = false;
            };
            const cluster::Snapshot* global = nullptr;
            std::vector<Item> items;
            for (const cluster::Snapshot& s: sessions) {
                if (s.spec.id == GLOBAL_ID) global = &s;
                else items.push_back({&s, &s.spec, false});
            }
            if (this->archived)
                for (const cluster::SessionSpec& spec: archivedSpecs)
                    items.push_back({nullptr, &spec, true});
            std::set<std::string> names;
            for (const std::string& node: taxonomy.nodes()) {
                names.insert(node);
                if (!taxonomy.parent(node).empty()) names.insert(taxonomy.parent(node));
            }
            for (const Item& item: items) {
                if (!item.spec->category.empty()) names.insert(item.spec->category);
                names.insert(item.spec->tags.begin(), item.spec->tags.end());
            }
            std::map<std::string, std::vector<std::string>> kids;       // a node -> the nodes under it (sorted)
            for (const std::string& node: names)
                kids[taxonomy.parent(node)].push_back(node);
            std::map<std::string, std::vector<const Item*>> own;        // a node -> the sessions hanging directly under it ("": under global)
            for (const Item& item: items) {
                std::set<std::string> labels;
                if (this->byTags) {
                    if (!item.spec->category.empty()) labels.insert(item.spec->category);
                    labels.insert(item.spec->tags.begin(), item.spec->tags.end());
                } else if (!cluster::Taxonomy::primary(*item.spec).empty()) {
                    labels.insert(cluster::Taxonomy::primary(*item.spec));
                }
                if (labels.empty()) labels.insert(std::string());
                for (const std::string& label: labels)
                    own[label].push_back(&item);
            }
            const std::function<void(const std::string&, std::set<std::string>&, const int)> collect = [&](const std::string& name, std::set<std::string>& ids, const int depth) {
                if (depth > 40) return;
                for (const Item* item: own[name])
                    ids.insert(item->spec->id);
                for (const std::string& child: kids[name])
                    collect(child, ids, depth + 1);
            };
            const std::function<std::size_t(const std::string&)> below = [&](const std::string& name) {
                std::set<std::string> ids;
                collect(name, ids, 0);
                return ids.size();
            };
            std::string focusNode = this->focus;
            if (!focusNode.empty() && !names.contains(focusNode)) focusNode.clear();
            QFont bold;
            bold.setBold(true);
            const QFontMetricsF boldMetrics(bold);
            struct Node {
                int kind = 0;               // 0: the root (global), 1: a category / tag, 2: a session
                std::string name;
                const Item* item = nullptr;
                bool top = false;           // the node in focus (the root of the graph)
                bool has = false;           // something under it
                int depth = 0;
                double y = 0;
                double w = 0;               // its whole width, the fold mark included
                double pw = 0;              // the width of its pill
                QString text;
                std::vector<std::size_t> children;
            };
            std::vector<Node> nodes;
            const std::function<std::size_t(const int, const std::string&, const Item*, const int)> grow =
                [&](const int kind, const std::string& name, const Item* item, const int depth) -> std::size_t {
                const std::size_t index = nodes.size();
                nodes.emplace_back();
                nodes[index].kind = kind;
                nodes[index].name = name;
                nodes[index].item = item;
                nodes[index].depth = depth;
                if (kind == 2) {
                    nodes[index].text = QString::fromStdString(cluster::one_line(item->spec->name, 28) + (item->archived ? " (archived)" : ""));
                    nodes[index].w = 26 + boldMetrics.horizontalAdvance(nodes[index].text) + 8;
                    return index;
                }
                nodes[index].top = kind == 1 && depth == 0;
                nodes[index].has = !kids[name].empty() || !own[name].empty();
                if (kind == 0) {
                    nodes[index].w = 44;
                } else {
                    nodes[index].text = QString::fromStdString(cluster::one_line(name, 26)) + " · " + QString::number(below(name));
                    nodes[index].pw = boldMetrics.horizontalAdvance(nodes[index].text) + 24;
                    nodes[index].w = nodes[index].pw + (nodes[index].has && !nodes[index].top ? 26 : 0);
                }
                if (depth > 40 || (kind == 1 && !nodes[index].top && this->collapsed.contains(name))) return index;
                std::vector<std::size_t> children;
                for (const std::string& child: kids[name])
                    if (below(child) > 0) children.push_back(grow(1, child, nullptr, depth + 1));
                for (const Item* leaf: own[name])
                    children.push_back(grow(2, leaf->spec->name, leaf, depth + 1));
                nodes[index].children = children;
                return index;
            };
            const std::size_t root = grow(focusNode.empty() ? 0 : 1, focusNode, nullptr, 0);
            // the rows: a leaf a row, a node in the middle of its children; the columns: as wide as their widest node
            double row = 0;
            int deepest = 0;
            const std::function<void(const std::size_t)> place = [&](const std::size_t index) {
                deepest = std::max(deepest, nodes[index].depth);
                if (nodes[index].children.empty()) {
                    nodes[index].y = row;
                    row += 46;
                    return;
                }
                for (const std::size_t child: nodes[index].children)
                    place(child);
                nodes[index].y = (nodes[nodes[index].children.front()].y + nodes[nodes[index].children.back()].y) / 2;
            };
            place(root);
            std::vector<double> width(static_cast<std::size_t>(deepest) + 1, 0.0);
            for (const Node& n: nodes)
                width[static_cast<std::size_t>(n.depth)] = std::max(width[static_cast<std::size_t>(n.depth)], n.w);
            std::vector<double> left(width.size(), 0.0);
            for (std::size_t d = 1; d < width.size(); ++d)
                left[d] = left[d - 1] + width[d - 1] + 56;
            const double ox = -(left.back() + width.back()) / 2;
            const double oy = -std::max(row - 46, 0.0) / 2;
            const std::function<double(const Node&)> X = [&](const Node& n) {return ox + left[static_cast<std::size_t>(n.depth)];};
            const std::function<double(const Node&)> Y = [&](const Node& n) {return oy + n.y;};
            const std::function<QColor(const cluster::Snapshot&)> color = [&st](const cluster::Snapshot& s) {
                return QColor(QString::fromStdString(!s.permissions.empty() ? st.warn : s.state == cluster::State::Working || s.state == cluster::State::Starting ? st.ok
                    : s.state == cluster::State::Error ? st.error : cluster::background_busy(s) ? busy_color_(st) : st.muted));
            };
            const QColor surface(QString::fromStdString(st.surface));
            QColor plate(QString::fromStdString(st.bg));
            plate.setAlpha(204);
            const std::function<void(const std::size_t)> draw = [&](const std::size_t index) {
                const Node& n = nodes[index];
                for (const std::size_t childIndex: n.children) {   // the edges first (under the nodes), then the node
                    const Node& child = nodes[childIndex];
                    const double x1 = X(n) + n.w, y1 = Y(n), x2 = X(child), y2 = Y(child), xm = (x1 + x2) / 2;
                    const bool live = child.kind == 2 && child.item->snap != nullptr;
                    QPen pen(child.kind == 1 ? QColor(QString::fromStdString(chip_color_(child.name))) : live ? color(*child.item->snap) : QColor(QString::fromStdString(st.border)), 1.6);
                    if (child.kind == 2 && (!live || (child.item->snap->state != cluster::State::Working && !cluster::background_busy(*child.item->snap)))) pen.setStyle(Qt::DashLine);
                    QPainterPath path(QPointF(x1, y1));
                    path.cubicTo(xm, y1, xm, y2, x2, y2);
                    this->_scene.addPath(path, pen)->setZValue(0);
                    draw(childIndex);
                }
                const double x = X(n), y = Y(n);
                if (n.kind == 0) {
                    const QColor c = global != nullptr ? color(*global) : QColor(QString::fromStdString(st.accent));
                    QGraphicsEllipseItem* circle = this->_scene.addEllipse(x, y - 22, 44, 44, QPen(c, 2), QBrush(surface));
                    circle->setZValue(1);
                    circle->setToolTip(global != nullptr ? "the global session (click: open it)" : "claude-cluster");
                    const char* face = global == nullptr ? "^_^" : !global->permissions.empty() ? "•_•" : global->state == cluster::State::Working ? ">_<" : global->state == cluster::State::Error ? "o_o" : cluster::background_busy(*global) ? "~_~" : "-_-";
                    QGraphicsSimpleTextItem* faceItem = this->_scene.addSimpleText(face, QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), 10));
                    faceItem->setBrush(c);
                    faceItem->setPos(x + 22 - faceItem->boundingRect().width() / 2, y - faceItem->boundingRect().height() / 2);
                    faceItem->setZValue(2);
                    QGraphicsSimpleTextItem* count = this->_scene.addSimpleText(QString("global · ") + QString::number(below("")));
                    count->setBrush(QColor(QString::fromStdString(st.muted)));
                    count->setPos(x + 22 - count->boundingRect().width() / 2, y + 26);
                    count->setZValue(2);
                    if (global != nullptr)
                        for (QGraphicsItem* item: std::vector<QGraphicsItem*>{circle, faceItem, count})
                            item->setData(0, QString::fromStdString(global->spec.id));
                } else if (n.kind == 1) {
                    const QColor c(QString::fromStdString(chip_color_(n.name)));
                    const QString id = QString::fromStdString("label:" + n.name);
                    QPainterPath pill;
                    pill.addRoundedRect(QRectF(x, y - 12, n.pw, 24), 12, 12);
                    QGraphicsPathItem* shape = this->_scene.addPath(pill, n.name == this->filter ? QPen(QColor(QString::fromStdString(st.accent)), 3) : QPen(Qt::NoPen), QBrush(c));
                    shape->setZValue(1);
                    shape->setData(0, id);
                    shape->setCursor(Qt::PointingHandCursor);
                    shape->setToolTip(QString::fromStdString(n.name + ": click filters the session list to it and what is below it; right click: set parent, rename, merge, delete; the arrow folds it"));
                    QGraphicsSimpleTextItem* text = this->_scene.addSimpleText(n.text, bold);
                    text->setBrush(QColor(QString::fromStdString(st.bg)));
                    text->setPos(x + 12, y - text->boundingRect().height() / 2);
                    text->setZValue(2);
                    text->setData(0, id);
                    if (n.has && !n.top) {
                        const bool folded = this->collapsed.contains(n.name);
                        const QString foldId = QString::fromStdString("fold:" + n.name);
                        QGraphicsEllipseItem* mark = this->_scene.addEllipse(x + n.pw + 4, y - 9, 18, 18, QPen(QColor(QString::fromStdString(st.border))), QBrush(surface));
                        mark->setZValue(1);
                        mark->setData(0, foldId);
                        mark->setCursor(Qt::PointingHandCursor);
                        mark->setToolTip(folded ? "unfold" : "fold");
                        QGraphicsSimpleTextItem* arrow = this->_scene.addSimpleText(folded ? "▸" : "▾");
                        arrow->setBrush(QColor(QString::fromStdString(st.text)));
                        arrow->setPos(x + n.pw + 13 - arrow->boundingRect().width() / 2, y - arrow->boundingRect().height() / 2);
                        arrow->setZValue(2);
                        arrow->setData(0, foldId);
                    }
                } else {
                    const Item& item = *n.item;
                    const bool live = item.snap != nullptr;
                    const QColor c = live ? color(*item.snap) : QColor(QString::fromStdString(st.muted));
                    const double opacity = item.archived ? 0.45 : !this->filter.empty() && !taxonomy.under(*item.spec, this->filter, true) ? 0.35 : 1.0;
                    const QString id = live ? QString::fromStdString(item.spec->id) : QString();
                    QPen pen(c, item.spec->id == this->activeId ? 3.5 : 2);
                    if (!live) pen.setStyle(Qt::DotLine);
                    QGraphicsEllipseItem* circle = this->_scene.addEllipse(x, y - 9, 18, 18, pen, QBrush(surface));
                    QGraphicsEllipseItem* dot = this->_scene.addEllipse(x + 5.5, y - 3.5, 7, 7, QPen(Qt::NoPen), QBrush(c));
                    QGraphicsRectItem* back = this->_scene.addRect(QRectF(x + 22, y - 10, boldMetrics.horizontalAdvance(n.text) + 8, 20), QPen(Qt::NoPen), QBrush(plate));
                    QGraphicsSimpleTextItem* text = this->_scene.addSimpleText(n.text, bold);
                    text->setBrush(QColor(QString::fromStdString(st.text)));
                    text->setPos(x + 26, y - text->boundingRect().height() / 2);
                    const std::string labels = cluster::label_text(*item.spec);
                    for (QGraphicsItem* part: std::vector<QGraphicsItem*>{circle, dot, back, text}) {
                        part->setData(0, id);
                        part->setOpacity(opacity);
                        part->setZValue(part == circle || part == dot ? 1 : 2);
                        part->setToolTip(QString::fromStdString(item.spec->name + (item.archived ? " · archived" : " · " + std::string(cluster::state_name(item.snap->state)) + " · " + cluster::short_path(item.spec->cwd))
                            + (labels.empty() ? std::string() : "\n" + labels)));
                        if (live) part->setCursor(Qt::PointingHandCursor);
                    }
                }
            };
            draw(root);
            this->setSceneRect(QRectF(-1400, -1000, 2800, 2000).united(this->_scene.itemsBoundingRect().adjusted(-100, -100, 100, 100)));
        }

        std::string activeId;       // the session shown (its ring thicker), set before populate

        void populate(const std::vector<cluster::Snapshot>& sessions, const cluster::Style& st, const std::string& active, const cluster::Taxonomy& taxonomy,
            const std::vector<cluster::SessionSpec>& archivedSpecs)
        {
            // the graph: always the tree (global, the taxonomy, the sessions - without labels the sessions right under global); the hub: the ring
            // (the two views never the same drawing)
            this->activeId = active;
            if (this->ring) this->populateRing_(sessions, st, active);
            else this->populateTree_(sessions, st, taxonomy, archivedSpecs);
            this->_markColor = QColor(QString::fromStdString(st.accent));
            this->markSelected_();
            if (this->fitPending) { // its first drawing: the whole graph in the view
                this->fitPending = false;
                QTimer::singleShot(0, this, [this]() {this->whole();});
            }
        }

        bool fitPending = true;
};

// A card of the simple mode: clicked anywhere (its buttons apart), its action
class SimpleTile: public QFrame {
    public:
        std::function<void()> onClick;

        explicit SimpleTile(QWidget* parent): QFrame(parent)
        {
            this->setCursor(Qt::PointingHandCursor);
            this->setAttribute(Qt::WA_Hover, true);
        }

    protected:
        void mouseReleaseEvent(QMouseEvent* event) override
        {
            if (event->button() == Qt::LeftButton && this->rect().contains(event->position().toPoint()) && this->onClick) this->onClick();
            QFrame::mouseReleaseEvent(event);
        }
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
        bool quitDecided = false;               // the sessions running asked about once (keep them in background / stop)
        int historyAt = -1;                     // Up / Down: -1 the prompt being written, else an index of the history
        std::vector<cluster::Draft> historyList;
        std::string historySession;             // the session of that list
        std::string labelFilter;                // a node of the taxonomy chosen in the hub: the list shows what is below it (empty: all)
        // the simple mode (--simple / --ipad-kid, [ui] mode = simple): its home, the session opened on its right
        QScrollArea* simpleHome = nullptr;
        QWidget* simpleSide = nullptr;
        QLabel* simpleSideName = nullptr;
        std::string simpleKey;                  // what the home shows: drawn again only when it changes
        std::string simpleFilter;               // a chip clicked: the cards of that state only
        std::string simplePage = "general";     // the page of its settings (reopened there)
        bool simplePanel = false;               // a session opened on the right
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

        void closeEvent(QCloseEvent* event) override
        {
            // sessions running (a turn, prompts queued): keep them in background / stop everything / back ([ui] quit_background)
            const std::string& mode = this->manager.config().quitBackground;
            if (!this->quitDecided && mode != "never" && !cluster::quit_requested() && this->manager.busy()) {
                if (mode == "ask") {
                    QMessageBox box(QMessageBox::Question, "claude-cluster", "Sessions are still running.\n\nKeep them running in background? The core goes on "
                        "without window, closes its web access and stops by itself once every session is idle (opening claude-cluster meanwhile shows them again).",
                        QMessageBox::NoButton, this);
                    QPushButton* keep = box.addButton("Keep in background", QMessageBox::AcceptRole);
                    QPushButton* stop = box.addButton("Stop everything", QMessageBox::DestructiveRole);
                    box.addButton(QMessageBox::Cancel);
                    box.setDefaultButton(keep);
                    box.exec();
                    if (box.clickedButton() == keep) this->manager.requestBackground();
                    else if (box.clickedButton() != stop) {
                        event->ignore();
                        return;
                    }
                } else {
                    this->manager.requestBackground();
                }
            }
            this->quitDecided = true;
            if (this->input) this->manager.draftSave("gui", this->input->draft()); // what is typed and not sent: kept for the next start
            QMainWindow::closeEvent(event);
        }

        Window(cluster::App& application)
            : app(application), manager(*application.manager)
        {
            this->setWindowTitle("claude-cluster");
            this->resize(1400, 860);
            this->bindKeys();
            if (this->app.options.layout.empty()) this->manager.setUi("layout", "list"); // every start on the list (as the terminal and the page)
            this->rebuild();
            QTimer* timer = new QTimer(this);
            QObject::connect(timer, &QTimer::timeout, [this]() {this->tick();});
            timer->start(50);
            QTimer* face = new QTimer(this); // the icon follows the state (>_< running, •_• waiting on you, o_o an error...)
            QObject::connect(face, &QTimer::timeout, [this]() {this->showFace();});
            face->start(500);
            this->showFace();
            // a check of the look (tests): every view in turn, a picture of the window just after the change and once it settled
            if (const char* shots = std::getenv("CLAUDE_CLUSTER_GUI_SHOT"); shots != nullptr && *shots != '\0') {
                const std::string dir = shots;
                QTimer* cycle = new QTimer(this);
                std::shared_ptr<int> step = std::make_shared<int>(0);
                QObject::connect(cycle, &QTimer::timeout, this, [this, dir, step]() {
                    if (cluster::simple_on(this->manager)) { // the simple mode: its home, a session opened, its windows (each shot, then closed)
                        static const std::array<const char*, 5> simple = {"simple-home", "simple-panel", "simple-settings", "simple-stats", "simple-newtask"};
                        if (*step >= static_cast<int>(simple.size())) return;
                        const std::string view = simple[static_cast<std::size_t>(*step)];
                        ++*step;
                        if (view == "simple-panel") {
                            const std::vector<cluster::Snapshot> all = this->manager.shown();
                            const auto other = std::find_if(all.begin(), all.end(), [](const cluster::Snapshot& s) {return !s.spec.global;});
                            if (other != all.end()) this->simpleOpen(other->spec.id);
                        } else if (view != "simple-home") {
                            this->simpleClose();
                            QTimer::singleShot(1200, this, [dir, view]() {
                                if (QWidget* modal = QApplication::activeModalWidget(); modal != nullptr) {
                                    modal->grab().save(QString::fromStdString(dir + "/" + view + ".png"));
                                    modal->close();
                                }
                            });
                            QTimer::singleShot(0, this, [this, view]() {
                                if (view == "simple-settings") this->simpleSettings();
                                else if (view == "simple-stats") this->simpleStats();
                                else this->simpleNewTask();
                            });
                            return;
                        }
                        QTimer::singleShot(1500, this, [this, dir, view]() {this->grab().save(QString::fromStdString(dir + "/" + view + ".png"));});
                        return;
                    }
                    static const std::array<const char*, 6> shots = {"list", "grid", "graph", "hub", "tree", "tasks"};
                    if (*step >= static_cast<int>(shots.size())) return;
                    const std::string view = shots[static_cast<std::size_t>(*step)];
                    ++*step;
                    if (view == "tasks") this->active = TASKS_ID;
                    this->manager.setUi("layout", view == "tasks" ? "list" : view);
                    this->refresh(true);
                    QTimer::singleShot(120, this, [this, dir, view]() {this->grab().save(QString::fromStdString(dir + "/" + view + "-early.png"));});
                    QTimer::singleShot(1500, this, [this, dir, view]() {this->grab().save(QString::fromStdString(dir + "/" + view + ".png"));});
                });
                cycle->start(2500);
            }
        }

        void webAsk(void)
        {
            // a device of the web access asks to connect: who it is, allow / allow and trust / deny / later
            const std::optional<cluster::WebStatus> web = cluster::web_status(this->manager);
            if (!web || this->webAsking) return;
            for (const cluster::WebDevice& d: web->devices) {
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
                    if (!answer.empty()) cluster::web_act(this->manager, "approve", {}, d.id, answer);
                } catch (const utils::exception::IException& e) {
                    this->message(e.info(), true);
                }
                this->webAsking = false;
                return;
            }
        }

        void webDevices(void)
        {
            // the devices connected / waiting (their ping), trusted, banned: ticked, then kicked, trusted no more, banned or
            // unbanned; kick all and ban asked first
            if (!cluster::web_status(this->manager)) return;
            QDialog dialog(this);
            dialog.setWindowTitle("Web devices");
            dialog.resize(760, 480);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            QLabel* head = new QLabel(&dialog);
            head->setTextFormat(Qt::RichText);
            QListWidget* devices = new QListWidget(&dialog);
            devices->setMouseTracking(true);
            devices->setStyleSheet("QListWidget::item{padding:6px 4px;}QListWidget::item:hover{background:" + q_(this->theme().surface) + ";}");
            layout->addWidget(head);
            layout->addWidget(new QLabel("Tick the devices, then: kick them, trust them no more, ban them (refused, their code too) or unban them.", &dialog));
            layout->addWidget(devices);
            const cluster::Style st = this->theme();
            const std::function<void(void)> fill = [this, devices, head, st]() {
                std::set<QString> ticked;
                for (int i = 0; i < devices->count(); ++i)
                    if (devices->item(i)->checkState() == Qt::Checked) ticked.insert(devices->item(i)->data(Qt::UserRole).toString());
                devices->clear();
                const std::optional<cluster::WebStatus> now = cluster::web_status(this->manager);
                if (!now) return;
                head->setText(!now->on ? span_(st.muted, "the web access is closed (the palette: Web access: open)")
                    : span_(st.accent, esc_(now->url + " · code " + now->pin), true) + span_(st.muted, esc_(now->ping >= 0 ? "  · mean ping " + std::to_string(now->ping) + " ms" : "")));
                const std::function<void(const std::string&, const std::string&, const cluster::WebDevice&)> add = [devices, &ticked](const std::string& key,
                    const std::string& line, const cluster::WebDevice& d) {
                    QListWidgetItem* item = new QListWidgetItem(q_(line), devices);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(ticked.contains(q_(key)) ? Qt::Checked : Qt::Unchecked);
                    item->setData(Qt::UserRole, q_(key));
                    item->setToolTip(q_(d.address + "\n" + d.agent));
                };
                for (const cluster::WebDevice& d: now->devices)
                    add("d:" + d.id, (d.state == "connected" ? "● " : "◌ ") + d.name + "   " + d.address + "   " + d.state + (d.trusted ? " · trusted" : "")
                        + (d.state != "connected" ? "" : d.ping < 0 ? "   ping …" : "   ping " + std::to_string(d.ping) + " ms"), d);
                for (const cluster::WebDevice& d: now->trusted)
                    add("t:" + d.id, "trusted   " + d.name + "   last " + (d.seen > 0 ? cluster::human_age(d.seen / 1000) + " ago" : std::string("never")), d);
                for (const cluster::WebDevice& d: now->banned)
                    add("b:" + d.id, "banned   " + d.name + "   " + d.address + "   since " + cluster::human_age(d.since / 1000) + " ago", d);
                if (devices->count() == 0) new QListWidgetItem("nobody connected, no trusted or banned device", devices);
            };
            fill();
            QTimer* live = new QTimer(&dialog); // their ping, the ones coming and going
            QObject::connect(live, &QTimer::timeout, &dialog, fill);
            live->start(2000);
            const std::function<std::map<char, std::vector<std::string>>(void)> ticked = [devices]() {
                std::map<char, std::vector<std::string>> out; // d / t / b: their ids
                for (int i = 0; i < devices->count(); ++i) {
                    const QString key = devices->item(i)->data(Qt::UserRole).toString();
                    if (key.size() > 2 && devices->item(i)->checkState() == Qt::Checked) out[key.at(0).toLatin1()].push_back(key.mid(2).toStdString());
                }
                return out;
            };
            QHBoxLayout* buttons = new QHBoxLayout();
            QPushButton* kick = new QPushButton("Kick the ticked", &dialog);
            QPushButton* kickAll = new QPushButton("Kick all (a new code)", &dialog);
            QPushButton* untrust = new QPushButton("Trust no more", &dialog);
            QPushButton* ban = new QPushButton("Ban", &dialog);
            QPushButton* unban = new QPushButton("Unban", &dialog);
            QPushButton* close = new QPushButton("Close", &dialog);
            for (QPushButton* b: {kick, kickAll, untrust, ban, unban}) {
                b->setCursor(Qt::PointingHandCursor);
                buttons->addWidget(b);
            }
            buttons->addStretch(1);
            buttons->addWidget(close);
            layout->addLayout(buttons);
            const std::function<void(const std::string&, const std::vector<std::string>&)> act = [this, fill](const std::string& action, const std::vector<std::string>& ids) {
                try {
                    cluster::web_act(this->manager, action, ids);
                } catch (const utils::exception::IException& e) {
                    this->message(e.info(), true);
                }
                fill();
            };
            QObject::connect(kick, &QPushButton::clicked, [act, ticked]() {
                if (const std::vector<std::string> ids = ticked()['d']; !ids.empty()) act("kick", ids);
            });
            QObject::connect(kickAll, &QPushButton::clicked, [act, &dialog]() {
                if (QMessageBox::question(&dialog, "Web devices", "Kick every device? A new code is made too.") == QMessageBox::Yes) act("kick", {});
            });
            QObject::connect(untrust, &QPushButton::clicked, [act, ticked]() {
                std::map<char, std::vector<std::string>> t = ticked();
                std::vector<std::string> ids = t['t'];
                ids.insert(ids.end(), t['d'].begin(), t['d'].end());
                if (!ids.empty()) act("untrust", ids);
            });
            QObject::connect(ban, &QPushButton::clicked, [act, ticked, &dialog]() {
                std::map<char, std::vector<std::string>> t = ticked();
                std::vector<std::string> ids = t['d'];
                ids.insert(ids.end(), t['t'].begin(), t['t'].end());
                if (ids.empty()) return;
                if (QMessageBox::warning(&dialog, "Ban", q_("Ban " + std::to_string(ids.size()) + " device(s)? Disconnected, trusted no more, refused from now - "
                    "their code too - until unbanned here."), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) act("ban", ids);
            });
            QObject::connect(unban, &QPushButton::clicked, [act, ticked]() {
                if (const std::vector<std::string> ids = ticked()['b']; !ids.empty()) act("unban", ids);
            });
            QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
            dialog.exec();
        }

        QMessageBox* failureBox = nullptr;      // what failed of what the user did: its dialog (OK / Retry)
        std::string failureShown;

        void showFailure(void)
        {
            // the oldest one, one at a time; answered elsewhere: closed here
            const std::vector<cluster::Failure> failed = this->manager.failures();
            if (this->failureBox != nullptr) {
                if (std::none_of(failed.begin(), failed.end(), [this](const cluster::Failure& f) {return f.id == this->failureShown;})) {
                    QMessageBox* box = this->failureBox;
                    this->failureBox = nullptr;
                    box->disconnect(this);
                    box->close();
                }
                return;
            }
            if (failed.empty() || (this->progressBox != nullptr && this->progressBox->isVisible())) return; // after the progress box
            const cluster::Failure f = failed.front();
            const cluster::Style& st = this->theme();
            QMessageBox* box = new QMessageBox(QMessageBox::Critical, q_(f.action + " failed"), "<b>" + esc_(f.action) + "</b> failed"
                + (failed.size() > 1 ? span_(st.muted, " (+" + QString::number(failed.size() - 1) + " more)") : QString()) + "<br><br>" + span_(st.muted, "source") + "&nbsp; "
                + esc_(f.source) + "<br>" + span_(st.muted, "error") + "&nbsp;&nbsp; " + span_(st.error, esc_(f.error)), QMessageBox::NoButton, this);
            box->setTextFormat(Qt::RichText);
            QPushButton* ok = box->addButton(QMessageBox::Ok);
            QPushButton* retry = f.retry ? box->addButton("Retry", QMessageBox::AcceptRole) : nullptr;
            QPushButton* other = f.other.empty() ? nullptr : box->addButton(q_(f.other), QMessageBox::ActionRole); // a third choice (a free port...)
            box->setDefaultButton(ok);
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->setWindowModality(Qt::NonModal);
            QObject::connect(box, &QMessageBox::finished, this, [this, box, retry, other, id = f.id]() {
                const bool again = retry != nullptr && box->clickedButton() == retry;
                this->failureBox = nullptr;
                this->manager.failureAnswer(id, other != nullptr && box->clickedButton() == other ? std::string("other") : std::string(again ? "retry" : "ok"));
                this->refresh(true);
            });
            this->failureBox = box;
            this->failureShown = f.id;
            box->open();
        }

        QFrame* progressBox = nullptr;          // a long operation: its box over the window
        QLabel* progressLabel = nullptr;

        void showProgress(void)
        {
            // its title, a spinner, what it does now, its last lines greyed; one running: redrawn (its box at 0.4 s)
            if (this->manager.progressActive()) this->animating = true;
            const std::optional<cluster::Progress> p = this->manager.progress();
            if (!p) {
                if (this->progressBox != nullptr) this->progressBox->hide();
                return;
            }
            const cluster::Style& st = this->theme();
            if (this->progressBox == nullptr) {
                this->progressBox = new QFrame(this);
                this->progressBox->setObjectName("progressBox");
                this->progressBox->setStyleSheet("QFrame#progressBox{background:" + q_(st.surface) + ";border:1px solid " + q_(st.border) + ";border-radius:12px;}");
                QVBoxLayout* layout = new QVBoxLayout(this->progressBox);
                this->progressLabel = new QLabel(this->progressBox);
                this->progressLabel->setTextFormat(Qt::RichText);
                this->progressLabel->setWordWrap(true);
                layout->addWidget(this->progressLabel);
            }
            static const char* spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
            const std::int64_t now = cluster::now_ms();
            const std::string mark = p->done ? (p->ok ? "✓" : "✗") : spinner[(now / 90) % 10];
            QString html = "<b>" + span_(p->done ? (p->ok ? st.ok : st.error) : st.accent, q_(mark)) + "&nbsp; " + esc_(p->title) + "</b> &nbsp;"
                + span_(st.muted, QString::number(((p->done ? p->ended : now) - p->started) / 1000) + "s") + "<br>";
            if (!p->now.empty()) html += span_(p->done ? (p->ok ? st.ok : st.error) : st.text, esc_(p->now)) + "<br>";
            for (const std::string& line: p->log)
                html += "<span style=\"color:" + q_(st.muted) + ";font-family:monospace;font-size:9pt\">" + esc_(cluster::one_line(line, 110)) + "</span><br>";
            this->progressLabel->setText(html);
            this->progressBox->setFixedWidth(std::min(640, this->width() - 40));
            this->progressBox->adjustSize();
            this->progressBox->move((this->width() - this->progressBox->width()) / 2, std::max(60, (this->height() - this->progressBox->height()) / 2)); // in the middle of the window
            this->progressBox->show();
            this->progressBox->raise();
            this->animating = this->animating || !p->done;
        }

        QTextBrowser* tasksView = nullptr;      // the Tasks session (list / tabs)
        HubView* hub = nullptr;                 // the graph / hub layouts
        QTreeWidget* folders = nullptr;         // the tree layout: the sessions by project folder
        QScrollArea* gridArea = nullptr;        // the grid layout: its cells (scrolled when they do not fit)
        int gridColumns = 1;
        int listWidth = 0;                      // the width the rows of the list were elided to
        int settingsTab = 0;                    // the tab the settings were left on (reopened there)
        QLabel* hubNote = nullptr;              // its notes at the bottom (no sub-session, the keys)
        std::string tasksShownHtml;

        QTextBrowser* makeTasksView(QWidget* parent)
        {
            QTextBrowser* browser = new QTextBrowser(parent);
            browser->setObjectName("tasksPanel");
            browser->setOpenLinks(false);
            browser->setFrameShape(QFrame::NoFrame);
            browser->setVisible(false);
            // without the mouse: its links by the keys (↑↓ / j k / Tab: the next one, Enter: its actions)
            browser->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard | Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
            Outline* outline = new Outline(browser);
            outline->setObjectName("tasksOutline");
            outline->color = QColor(q_(this->theme().accent));
            outline->raise();
            browser->setTabChangesFocus(false);
            browser->installEventFilter(this);
            QObject::connect(browser, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
                // a task / a schedule / the archive: its actions (the palette: its reason asked when it takes one); a session: shown
                const QString link = url.toString();
                if (link.startsWith("focus:")) {
                    this->active = link.mid(6).toStdString();
                    this->refresh(true);
                } else if (link.startsWith("job:")) { // a heavy job: its details and its output live, like an agent
                    this->jobDialog(link.mid(4).toStdString());
                } else {
                    this->palette(link);
                }
            });
            return browser;
        }

        void pluginsDialog(const std::string& open = "")
        {
            // the sources first (their counts, last sync); the one chosen: its information and settings, its offer as a checklist; the ticks
            // written at once by the core, which applies them (read again every 1.5 s)
            QDialog dialog(this);
            dialog.setWindowTitle("Plugins and skills");
            dialog.resize(1080, 680);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            QHBoxLayout* top = new QHBoxLayout();
            layout->addLayout(top);
            QLineEdit* add = new QLineEdit(&dialog);
            add->setPlaceholderText("a source: owner/repo, an https / git URL, a folder (~/...), a .zip");
            QPushButton* addButton = new QPushButton("Add", &dialog);
            QPushButton* syncAll = new QPushButton("Sync all", &dialog);
            QSpinBox* hours = new QSpinBox(&dialog);
            hours->setRange(0, 24 * 30);
            hours->setSuffix(" h");
            hours->setToolTip("the sources with auto sync pulled again every N hours (0: at each start of the core only)");
            top->addWidget(add, 1);
            top->addWidget(addButton);
            top->addSpacing(12);
            top->addWidget(new QLabel("Auto sync every", &dialog));
            top->addWidget(hours);
            top->addWidget(syncAll);
            QSplitter* split = new QSplitter(&dialog);
            layout->addWidget(split, 1);
            QListWidget* sourcesList = new QListWidget(split);
            sourcesList->setMinimumWidth(300);
            QWidget* side = new QWidget(split);
            split->setStretchFactor(1, 1);
            QVBoxLayout* sideLayout = new QVBoxLayout(side);
            QLabel* about = new QLabel(side);
            about->setTextFormat(Qt::RichText);
            about->setWordWrap(true);
            about->setTextInteractionFlags(Qt::TextSelectableByMouse);
            sideLayout->addWidget(about);
            QHBoxLayout* sourceRow = new QHBoxLayout();
            sideLayout->addLayout(sourceRow);
            QCheckBox* autoBox = new QCheckBox("Auto sync", side);
            QCheckBox* onBox = new QCheckBox("Source on", side);
            onBox->setToolTip("off: every plugin / skill of it turned off, kept installed");
            sourceRow->addWidget(autoBox);
            sourceRow->addWidget(onBox);
            sourceRow->addStretch(1);
            std::map<std::string, QPushButton*> sourceButtons;
            for (const auto &[key, label]: std::vector<std::pair<std::string, std::string>>{{"sync", "Sync now"}, {"all", "Install all"}, {"none", "Remove all"}, {"drop", "Drop source"}}) {
                QPushButton* b = new QPushButton(q_(label), side);
                sourceButtons[key] = b;
                sourceRow->addWidget(b);
            }
            QTreeWidget* itemsTree = new QTreeWidget(side);
            itemsTree->setHeaderLabels({"", "Name", "Kind", "State", "Description"});
            itemsTree->setRootIsDecorated(false);
            itemsTree->setAlternatingRowColors(true);
            itemsTree->setColumnWidth(0, 28);
            itemsTree->setColumnWidth(1, 200);
            itemsTree->setColumnWidth(2, 60);
            itemsTree->setColumnWidth(3, 110);
            sideLayout->addWidget(itemsTree, 1);
            QHBoxLayout* bulk = new QHBoxLayout();
            sideLayout->addLayout(bulk);
            QLabel* ticked = new QLabel(side);
            bulk->addWidget(ticked);
            bulk->addStretch(1);
            std::map<std::string, QPushButton*> itemButtons;
            for (const auto &[key, label]: std::vector<std::pair<std::string, std::string>>{{"every", "Tick all"}, {"clear", "Untick all"}, {"install", "Install"}, {"remove", "Remove"},
                {"on", "Turn on"}, {"off", "Turn off"}}) {
                QPushButton* b = new QPushButton(q_(label), side);
                itemButtons[key] = b;
                bulk->addWidget(b);
            }
            QLabel* said = new QLabel(&dialog);
            layout->addWidget(said);
            QDialogButtonBox* close = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
            QObject::connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(close);
            const std::shared_ptr<cluster::Json> data = std::make_shared<cluster::Json>(cluster::Json::object());
            const std::shared_ptr<std::string> shown = std::make_shared<std::string>();     // the data drawn (redrawn only when it changed)
            const std::shared_ptr<std::string> chosen = std::make_shared<std::string>(open == "+add" ? std::string() : open); // the slug of the source chosen
            const std::shared_ptr<std::set<std::string>> marked = std::make_shared<std::set<std::string>>();
            const std::shared_ptr<bool> filling = std::make_shared<bool>(false);
            const std::function<cluster::Json(void)> source = [data, chosen]() {
                for (const cluster::Json& s: data->value("sources", cluster::Json::array()))
                    if (s.value("slug", std::string()) == *chosen) return s;
                return cluster::Json(nullptr);
            };
            const std::function<std::string(const std::int64_t)> ago = [](const std::int64_t when) {
                return when <= 0 ? std::string("never") : cluster::now() - when < 60 ? std::string("just now") : cluster::human_age(when) + " ago";
            };
            const std::function<std::string(const cluster::Json&)> counts = [](const cluster::Json& s) {
                return std::to_string(s.value("offered", 0)) + " offered · " + std::to_string(s.value("installed", 0)) + " installed · " + std::to_string(s.value("enabled", 0)) + " on";
            };
            std::function<void(bool)> draw;
            const std::function<void(const cluster::Json&, const std::string&)> set = [this, said, &draw](const cluster::Json& changes, const std::string& done) {
                try {
                    this->manager.pluginsSet(changes);
                    said->setText(q_(done));
                } catch (const utils::exception::IException& e) {
                    said->setText(q_(std::string("refused: ") + e.info()));
                }
                draw(true);
            };
            const std::function<void(const std::string&)> items = [source, marked, set](const std::string& action) {
                // install / remove / on / off of the ticked ones; a source installed whole unticked first (its others kept)
                const cluster::Json s = source();
                if (s.is_null() || marked->empty()) return;
                const std::string slug = s.value("slug", std::string());
                cluster::Json changes = cluster::Json::array();
                if (action == "remove" && s.value("all", false)) {
                    changes.push_back({{"table", "select"}, {"key", slug + ":*"}, {"value", false}});
                    for (const cluster::Json& i: s.value("items", cluster::Json::array()))
                        if (!marked->contains(i.value("id", std::string())) && i.value("wanted", false)) changes.push_back({{"table", "select"}, {"key", i.value("id", std::string())}, {"value", true}});
                }
                for (const std::string& id: *marked) {
                    if (action == "install" || action == "remove") changes.push_back({{"table", "select"}, {"key", id}, {"value", action == "install"}});
                    if (action == "remove" || action == "on" || action == "off") changes.push_back({{"table", "off"}, {"key", id}, {"value", action == "off"}});
                }
                const std::size_t n = marked->size();
                marked->clear();
                set({{"items", changes}}, action + ": " + std::to_string(n) + " of " + slug + "… (applied by the core, for the sessions started after it)");
            };
            draw = [this, data, shown, chosen, marked, filling, source, ago, counts, sourcesList, about, autoBox, onBox, itemsTree, ticked, hours, sourceButtons, itemButtons,
                said](const bool force) {
                try {
                    *data = this->manager.pluginsJson();
                } catch (const utils::exception::IException& e) {
                    said->setText(q_(std::string("the core: ") + e.info()));
                }
                const std::string now = data->dump() + std::to_string(marked->size());
                if (!force && now == *shown) return;
                *shown = now;
                *filling = true;
                const cluster::Style& st = this->theme();
                const cluster::Json sources = data->value("sources", cluster::Json::array());
                if (chosen->empty() && !sources.empty()) *chosen = sources[0].value("slug", std::string());
                sourcesList->clear();
                for (const cluster::Json& s: sources) {
                    const bool broken = !s.value("error", std::string()).empty();
                    QListWidgetItem* row = new QListWidgetItem(q_(s.value("slug", std::string()) + (s.value("paused", false) ? "  (off)" : s.value("pending", false) ? "  (being listed…)" : "")
                        + "\n" + counts(s) + (s.value("kind", std::string()) == "folder" ? std::string(" · in place") : " · synced " + ago(s.value("synced", std::int64_t{0})))), sourcesList);
                    row->setData(Qt::UserRole, q_(s.value("slug", std::string())));
                    row->setForeground(QColor(q_(broken ? st.warn : s.value("paused", false) ? st.muted : st.text)));
                    if (s.value("slug", std::string()) == *chosen) sourcesList->setCurrentItem(row);
                }
                hours->setValue(data->value("sync_hours", 24));
                const cluster::Json s = source();
                const bool any = !s.is_null();
                for (auto &[key, b]: sourceButtons)
                    b->setEnabled(any);
                autoBox->setEnabled(any);
                onBox->setEnabled(any);
                itemsTree->clear();
                if (!any) {
                    about->setText(sources.empty() ? "No source yet: add a repository link, a folder or a .zip above." : "");
                } else {
                    const std::string kind = s.value("kind", std::string("git"));
                    const int every = data->value("sync_hours", 24);
                    QString html = "<b>" + esc_(s.value("url", std::string())) + "</b><br>" + span_(st.muted, esc_(kind + (s.value("marketplace", std::string()).empty() ? std::string(" · skills")
                        : " · marketplace " + s.value("marketplace", std::string())) + " · " + std::to_string(s.value("skills", 0)) + " skill(s), " + std::to_string(s.value("plugins", 0))
                        + " plugin(s)")) + "<br>" + esc_(counts(s)) + "<br>" + span_(st.muted, esc_(kind == "folder" ? std::string("a folder read in place: nothing to sync")
                        : "last sync " + ago(s.value("synced", std::int64_t{0})) + (s.value("auto", true) ? every > 0 ? " · auto every " + std::to_string(every) + " h"
                        : std::string(" · auto at each start") : std::string(" · auto sync off: by hand only"))));
                    if (!s.value("commit", std::string()).empty() || s.value("commit_time", std::int64_t{0}) > 0)
                        html += "<br>" + span_(st.muted, esc_((kind == "git" ? "last commit " + s.value("commit", std::string()) : kind == "zip" ? std::string("the .zip")
                            : std::string("its newest SKILL.md")) + (s.value("commit_time", std::int64_t{0}) > 0 ? " · " + ago(s.value("commit_time", std::int64_t{0})) : std::string())));
                    if (!s.value("error", std::string()).empty()) html += "<br>" + span_(st.warn, esc_(s.value("error", std::string())));
                    html += "<br>" + span_(st.border, esc_(cluster::short_path(s.value("dir", std::string()))));
                    about->setText(html);
                    autoBox->setChecked(s.value("auto", true));
                    onBox->setChecked(!s.value("paused", false));
                    for (const cluster::Json& i: s.value("items", cluster::Json::array())) {
                        const bool installed = i.value("installed", false);
                        const bool enabled = i.value("enabled", true);
                        QTreeWidgetItem* row = new QTreeWidgetItem(itemsTree);
                        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
                        row->setCheckState(0, marked->contains(i.value("id", std::string())) ? Qt::Checked : Qt::Unchecked);
                        row->setData(0, Qt::UserRole, q_(i.value("id", std::string())));
                        row->setText(1, q_(i.value("name", std::string())));
                        row->setText(2, q_(i.value("kind", std::string())));
                        row->setText(3, q_(!installed ? "○ not installed" : enabled ? "● on" : "◐ off"));
                        row->setForeground(3, QColor(q_(!installed ? st.muted : enabled ? st.ok : st.warn)));
                        row->setText(4, q_(cluster::one_line(i.value("description", std::string()), 200)));
                        row->setToolTip(4, q_(i.value("description", std::string())));
                    }
                }
                ticked->setText(q_(std::to_string(marked->size()) + " ticked"));
                for (const std::string& key: {"install", "remove", "on", "off"})
                    itemButtons.at(key)->setEnabled(!marked->empty());
                *filling = false;
            };
            QObject::connect(sourcesList, &QListWidget::currentItemChanged, &dialog, [chosen, marked, filling, &draw](QListWidgetItem* row, QListWidgetItem*) {
                if (*filling || row == nullptr) return;
                *chosen = row->data(Qt::UserRole).toString().toStdString();
                marked->clear();
                QTimer::singleShot(0, [&draw]() {draw(true);});
            });
            QObject::connect(itemsTree, &QTreeWidget::itemChanged, &dialog, [marked, filling, ticked, itemButtons](QTreeWidgetItem* row, const int column) {
                if (*filling || column != 0) return;
                const std::string id = row->data(0, Qt::UserRole).toString().toStdString();
                if (row->checkState(0) == Qt::Checked) marked->insert(id);
                else marked->erase(id);
                ticked->setText(q_(std::to_string(marked->size()) + " ticked"));
                for (const std::string& key: {"install", "remove", "on", "off"})
                    itemButtons.at(key)->setEnabled(!marked->empty());
            });
            QObject::connect(itemsTree, &QTreeWidget::itemDoubleClicked, &dialog, [](QTreeWidgetItem* row, int) { // a double click ticks / unticks
                row->setCheckState(0, row->checkState(0) == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            });
            QObject::connect(itemButtons.at("every"), &QPushButton::clicked, &dialog, [source, marked, &draw]() {
                for (const cluster::Json& i: source().value("items", cluster::Json::array()))
                    marked->insert(i.value("id", std::string()));
                draw(true);
            });
            QObject::connect(itemButtons.at("clear"), &QPushButton::clicked, &dialog, [marked, &draw]() {
                marked->clear();
                draw(true);
            });
            for (const std::string& action: {"install", "remove", "on", "off"})
                QObject::connect(itemButtons.at(action), &QPushButton::clicked, &dialog, [items, action, &dialog]() {
                    if (action == "remove" && QMessageBox::question(&dialog, "Remove", "Remove the ticked plugins / skills (the ones installed by claude-cluster)?") != QMessageBox::Yes) return;
                    items(action);
                });
            QObject::connect(autoBox, &QCheckBox::clicked, &dialog, [source, set](const bool on) {
                const std::string slug = source().value("slug", std::string());
                set({{"items", cluster::Json::array({{{"table", "auto"}, {"key", slug}, {"value", on}}})}}, slug + ": auto sync " + (on ? "on" : "off (Sync now by hand)"));
            });
            QObject::connect(onBox, &QCheckBox::clicked, &dialog, [source, set](const bool on) {
                const std::string slug = source().value("slug", std::string());
                set({{"items", cluster::Json::array({{{"table", "paused"}, {"key", slug}, {"value", !on}}})}}, slug + (on ? ": turned on again" : ": turned off (everything of it, kept installed)"));
            });
            QObject::connect(sourceButtons.at("sync"), &QPushButton::clicked, &dialog, [this, source, said]() {
                const std::string slug = source().value("slug", std::string());
                this->manager.pluginsRefresh(slug);
                said->setText(q_("syncing " + slug + "…"));
            });
            QObject::connect(syncAll, &QPushButton::clicked, &dialog, [this, said]() {
                this->manager.pluginsRefresh("");
                said->setText("syncing every source…");
            });
            QObject::connect(sourceButtons.at("all"), &QPushButton::clicked, &dialog, [source, set]() {
                const std::string slug = source().value("slug", std::string());
                set({{"items", cluster::Json::array({{{"table", "select"}, {"key", slug + ":*"}, {"value", true}}})}}, "installing everything of " + slug + " (the new ones too)…");
            });
            QObject::connect(sourceButtons.at("none"), &QPushButton::clicked, &dialog, [source, set, &dialog]() {
                const cluster::Json s = source();
                const std::string slug = s.value("slug", std::string());
                if (QMessageBox::question(&dialog, "Remove all", q_("Remove every plugin / skill of " + slug + " installed by claude-cluster?")) != QMessageBox::Yes) return;
                cluster::Json changes = cluster::Json::array({{{"table", "select"}, {"key", slug + ":*"}, {"value", false}}});
                for (const cluster::Json& i: s.value("items", cluster::Json::array()))
                    changes.push_back({{"table", "select"}, {"key", i.value("id", std::string())}, {"value", false}});
                set({{"items", changes}}, "removing everything of " + slug + "…");
            });
            QObject::connect(sourceButtons.at("drop"), &QPushButton::clicked, &dialog, [this, source, set, chosen, &dialog]() {
                const cluster::Json s = source();
                if (QMessageBox::question(&dialog, "Drop the source", q_("Drop " + s.value("slug", std::string()) + "? What it installed stays until removed (Remove all first).")) != QMessageBox::Yes) return;
                std::vector<std::string> repos = this->manager.config().pluginRepos;
                repos.erase(std::remove(repos.begin(), repos.end(), s.value("url", std::string())), repos.end());
                chosen->clear();
                set({{"repos", repos}}, "source dropped: " + s.value("slug", std::string()));
            });
            const std::function<void(void)> adding = [this, add, set, &dialog]() {
                const std::string url = cluster::trim(add->text().toStdString());
                if (url.empty()) return;
                std::vector<std::string> repos = this->manager.config().pluginRepos;
                if (std::find(repos.begin(), repos.end(), url) != repos.end()) return;
                if (QMessageBox::warning(&dialog, "Add a source", q_("Its plugins / skills will run as you in every session (commands, hooks, MCP servers).\nAdd " + url
                    + " (only a source you trust)?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
                repos.push_back(url);
                add->clear();
                set({{"repos", repos}}, "source added: " + url + " (listed in a moment: cloned / unpacked by the core)");
            };
            QObject::connect(addButton, &QPushButton::clicked, &dialog, adding);
            QObject::connect(add, &QLineEdit::returnPressed, &dialog, adding);
            QObject::connect(hours, &QSpinBox::editingFinished, &dialog, [hours, data, set]() {
                if (hours->value() != data->value("sync_hours", 24)) set({{"sync_hours", hours->value()}}, "auto sync every " + std::to_string(hours->value()) + " h");
            });
            draw(true);
            if (open == "+add") add->setFocus(); // Settings > Plugins > Add a source: its field first
            QTimer timer(&dialog);
            QObject::connect(&timer, &QTimer::timeout, &dialog, [&draw]() {draw(false);});
            timer.start(1500);
            dialog.exec();
        }

        void jobDialog(const std::string& id)
        {
            // a heavy job of the orchestrator: its state, command, session, folder, pid, memory, threads, priority, its time live, the end of
            // its output (read again every second); its actions as buttons (stop / resume, kill, priority, its session, every action)
            QDialog dialog(this);
            dialog.setWindowTitle(q_("Job " + id));
            dialog.resize(820, 560);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            QTextBrowser* text = new QTextBrowser(&dialog);
            layout->addWidget(text, 1);
            QHBoxLayout* row = new QHBoxLayout();
            layout->addLayout(row);
            auto data = std::make_shared<cluster::Json>(cluster::Json::object());
            const std::function<void(const std::string&)> act = [this, id](const std::string& what) {this->run({"job_act:" + id + ":" + what, "", "", "", false}, "");};
            std::map<std::string, QPushButton*> buttons;
            for (const auto &[key, label]: std::vector<std::pair<std::string, std::string>>{{"stop", "Stop"}, {"resume", "Resume"}, {"prio_up", "Priority +1"}, {"prio_down", "Priority -1"},
                {"kill", "Kill"}, {"session", "Its session"}, {"all", "Every action…"}}) {
                QPushButton* b = new QPushButton(q_(label), &dialog);
                b->setCursor(Qt::PointingHandCursor);
                const std::string what = key;
                QObject::connect(b, &QPushButton::clicked, [this, act, what, data, id, &dialog]() {
                    if (what == "session") {
                        this->active = data->value("session", std::string());
                        dialog.accept();
                        this->refresh(true);
                    } else if (what == "all") {
                        dialog.accept();
                        QTimer::singleShot(0, this, [this, id]() {this->palette(q_("job:" + id));});
                    } else {
                        act(what);
                    }
                });
                buttons[key] = b;
                row->addWidget(b);
            }
            row->addStretch(1);
            QPushButton* close = new QPushButton("Close", &dialog);
            QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
            row->addWidget(close);
            const std::function<void()> draw = [this, text, data, buttons, id]() {
                try {
                    *data = this->manager.jobsCommand("job_status", {{"id", id}, {"full", true}});
                } catch (const utils::exception::IException& e) {
                    (*data)["gone"] = std::string(e.info());
                }
                const cluster::Style& st = this->theme();
                const cluster::Json& j = *data;
                const std::string state = j.value("state", std::string());
                const std::int64_t now = cluster::now();
                const std::int64_t started = j.value("started", std::int64_t{0});
                const std::int64_t ended = j.value("ended", std::int64_t{0});
                const std::int64_t took = started > 0 ? std::max<std::int64_t>(0, (ended > 0 ? ended : now) - started) : 0;
                const std::string time = started > 0 ? (took < 60 ? std::to_string(took) + " s" : took < 3600 ? std::to_string(took / 60) + " min " + std::to_string(took % 60) + " s"
                    : std::to_string(took / 3600) + " h " + std::to_string(took % 3600 / 60) + " min") : std::string();
                const std::string tint = state == "running" ? st.ok : state == "queued" ? st.accent : state == "stopped" ? st.warn : state == "done" ? st.muted : st.error;
                QString html = "<b>" + esc_(id) + "</b> &nbsp;" + span_(st.accent, esc_(j.value("class", std::string()))) + " &nbsp;" + span_(tint, esc_(state))
                    + (time.empty() ? QString() : " &nbsp;" + span_(st.muted, q_("⏱ " + time))) + "<br><br><table>";
                const std::function<void(const std::string&, const std::string&)> line = [&html, &st](const std::string& key, const std::string& value) {
                    html += "<tr><td>" + span_(st.muted, esc_(key)) + "&nbsp;&nbsp;</td><td>" + esc_(value) + "</td></tr>";
                };
                line("command", j.value("command", std::string()));
                line("session", j.value("session", std::string()).empty() ? std::string("you") : j.value("session", std::string()));
                line("folder", cluster::short_path(j.value("cwd", std::string())));
                line("process", (j.value("pid", 0) > 0 ? "pid " + std::to_string(j.value("pid", 0)) + " · " : std::string()) + (j.value("rss_mb", 0) > 0 ? std::to_string(j.value("rss_mb", 0))
                    + " MB in use · " : std::string()) + "~" + std::to_string(j.value("ram_mb", 0)) + " MB planned" + (j.value("threads", 0) > 0 ? " · " + std::to_string(j.value("threads", 0))
                    + " thread(s)" : std::string()) + " · priority " + std::to_string(j.value("prio", 0)) + " · " + j.value("origin", std::string()));
                line("now", j.contains("gone") ? j.value("gone", std::string()) : j.value("message", std::string()));
                html += "</table><br><b>output</b>" + span_(st.muted, esc_(j.value("log", std::string()).empty() ? std::string() : " (" + cluster::short_path(j.value("log", std::string())) + ")"))
                    + "<pre>" + esc_(cluster::tail_of(j.value("tail", std::string("(nothing yet)")), 3000)) + "</pre>";
                const int scrolled = text->verticalScrollBar()->value();
                const bool bottom = scrolled >= text->verticalScrollBar()->maximum() - 4;
                text->setHtml(html);
                text->verticalScrollBar()->setValue(bottom ? text->verticalScrollBar()->maximum() : scrolled);
                const bool live = state == "queued" || state == "running" || state == "stopped";
                buttons.at("stop")->setVisible(state == "running");
                buttons.at("resume")->setVisible(state == "stopped");
                buttons.at("prio_up")->setVisible(live);
                buttons.at("prio_down")->setVisible(live);
                buttons.at("kill")->setVisible(live);
                buttons.at("session")->setVisible(!j.value("session", std::string()).empty());
            };
            draw();
            QTimer timer(&dialog);
            QObject::connect(&timer, &QTimer::timeout, &dialog, draw);
            timer.start(1000);
            dialog.exec();
        }

        bool eventFilter(QObject* watched, QEvent* event) override
        {
            // the grid: a click on a cell makes it the one of the prompt (the prompt keeps the keys; the Tasks keeps them for its links)
            if (event->type() == QEvent::MouseButtonPress && watched->property("ccCell").isValid()) {
                const std::string id = watched->property("ccCell").toString().toStdString();
                if (id != this->active)
                    QTimer::singleShot(0, this, [this, id]() {
                        this->active = id;
                        this->refresh(true);
                        if (id != TASKS_ID) this->input->setFocus();
                    });
            }
            if (this->simpleHome != nullptr && watched == this->simpleHome->viewport() && event->type() == QEvent::Resize) // its columns of cards: as many as fit
                QTimer::singleShot(0, this, [this]() {if (this->simpleHome != nullptr) this->refreshSimple();});
            if (this->list && watched == this->list->viewport() && event->type() == QEvent::Resize && this->list->viewport()->width() != this->listWidth)
                QTimer::singleShot(0, this, [this]() {this->refresh(true);});
            // a Tasks panel resized: its outline with it
            if (event->type() == QEvent::Resize && watched->objectName() == "tasksPanel")
                if (QWidget* outline = static_cast<QWidget*>(watched)->findChild<QWidget*>("tasksOutline", Qt::FindDirectChildrenOnly)) {
                    outline->setGeometry(static_cast<QWidget*>(watched)->rect());
                    outline->raise();
                }
            // the Tasks view: ↑↓ / j k move between its links like Tab / Shift+Tab
            if (watched == this->tasksView && event->type() == QEvent::KeyPress) {
                const QKeyEvent* key = static_cast<QKeyEvent*>(event);
                const bool down = key->key() == Qt::Key_Down || key->key() == Qt::Key_J;
                const bool up = key->key() == Qt::Key_Up || key->key() == Qt::Key_K;
                if ((down || up) && key->modifiers() == Qt::NoModifier) {
                    QKeyEvent tab(QEvent::KeyPress, down ? Qt::Key_Tab : Qt::Key_Backtab, down ? Qt::NoModifier : Qt::ShiftModifier);
                    QCoreApplication::sendEvent(this->tasksView, &tab);
                    return true;
                }
            }
            return QMainWindow::eventFilter(watched, event);
        }

        void showTasks(void)
        {
            // the Tasks session: shown in place of a session (list, tabs), its cell chosen (grid)
            this->active = TASKS_ID;
            this->refresh(true);
        }

        void fillTasks(QTextBrowser* browser)
        {
            // the todo lists of the sessions, the task queue, the schedules, the archive (a link each: its actions)
            const cluster::Style& st = this->theme();
            const std::int64_t now = cluster::now();
            const std::function<std::string(std::int64_t)> span = [](std::int64_t s) {
                s = std::max<std::int64_t>(0, s);
                return s < 60 ? std::to_string(s) + "s" : s < 3600 ? std::to_string(s / 60) + "m" : s < 86400 ? std::to_string(s / 3600) + "h" : std::to_string(s / 86400) + "d";
            };
            const std::function<std::string(const std::string&)> who = [this](const std::string& by) {
                if (by.empty()) return std::string("you");
                if (by == GLOBAL_ID) return std::string("global");
                const std::optional<cluster::Snapshot> s = this->manager.snapshot(by);
                return s ? s->spec.name : by;
            };
            const std::function<QString(const std::string&)> link = [&st](const std::string& href) {
                return "<a href=\"" + q_(href) + "\" style=\"color:" + q_(st.text) + ";text-decoration:none\">";
            };
            QString html = "<style>td{padding:3px 8px;} h3{color:" + q_(st.accent) + ";margin:14px 0 4px 0;} .m{color:" + q_(st.muted) + ";}</style>";
            html += "<p class=m>the todo lists of the sessions, the task queue, the schedules · a click on one: its actions · the prompt: a task "
                "(target | prompt: for it), +30m &lt;text&gt; / 18:00 &lt;text&gt;: a reminder</p>";
            // the todo lists
            QString todos;
            for (const cluster::Snapshot& s: this->manager.shown()) {
                if (s.todos.empty()) continue;
                const auto done = std::count_if(s.todos.begin(), s.todos.end(), [](const cluster::Todo& d) {return d.status == "completed";});
                todos += "<p>" + link("focus:" + s.spec.id) + "<b>" + esc_(s.spec.global ? "global" : s.spec.name) + "</b></a> " + span_(st.muted, QString::number(done) + "/"
                    + QString::number(s.todos.size()) + " done") + "<br>";
                for (const cluster::Todo& d: s.todos) {
                    const bool over = d.status == "completed";
                    const bool doing = d.status == "in_progress";
                    todos += "&nbsp;&nbsp;" + span_(over ? st.ok : doing ? st.accent : st.muted, over ? "☒" : doing ? "◼" : "☐") + " "
                        + (over ? "<s>" + span_(st.muted, esc_(d.content)) + "</s>" : doing ? "<b>" + span_(st.accent, esc_(d.activeForm.empty() ? d.content : d.activeForm)) + "</b>"
                        : esc_(d.content)) + "<br>";
                }
                todos += "</p>";
            }
            html += "<h3>Todo lists</h3>" + (todos.isEmpty() ? "<p class=m>none: a session writes its list while it works</p>" : todos);
            // the task queue
            std::vector<cluster::Task> tasks;
            std::size_t archived = 0;
            std::map<std::string, int> count;
            for (const cluster::Task& t: this->manager.tasks()) {
                if (t.archived) {
                    ++archived;
                    continue;
                }
                ++count[t.status];
                tasks.push_back(t);
            }
            const std::function<int(const cluster::Task&)> rank = [](const cluster::Task& x) {return x.status == "running" ? 0 : x.status == "queued" ? 1 : 2;};
            std::stable_sort(tasks.begin(), tasks.end(), [&rank](const cluster::Task& a, const cluster::Task& b) {
                return rank(a) != rank(b) ? rank(a) < rank(b) : rank(a) < 2 ? a.created < b.created : a.ended > b.ended;
            });
            html += "<h3>Task queue</h3><p class=m>" + QString::number(count["running"]) + " running · " + QString::number(count["queued"]) + " waiting · "
                + QString::number(count["done"]) + " done" + (count["failed"] ? " · " + QString::number(count["failed"]) + " failed" : QString())
                + (count["cancelled"] ? " · " + QString::number(count["cancelled"]) + " cancelled" : QString()) + "</p><table width=100%>";
            for (std::size_t i = 0; i < tasks.size() && i < 12; ++i) {
                const cluster::Task& t = tasks[i];
                const std::string tint = t.status == "running" ? st.ok : t.status == "queued" ? st.accent : t.status == "failed" ? st.error : t.status == "cancelled" ? st.warn : st.muted;
                const std::string mark = t.status == "running" ? "▶" : t.status == "queued" ? (t.forced ? "»" : "◌") : t.status == "done" ? "✓" : t.status == "failed" ? "✗"
                    : t.status == "stopped" ? "⏸" : "⊘";
                const std::string where = !t.session.empty() ? (this->manager.snapshot(t.session) ? this->manager.snapshot(t.session)->spec.name : t.session) : t.target.empty() ? "any" : t.target;
                const std::string age = t.status == "queued" ? "waits " + span(now - t.created) : t.status == "running" ? span(now - t.started) : span(now - t.ended) + " ago";
                html += "<tr><td width=18>" + span_(tint, q_(mark)) + "</td><td width=\"55%\">" + link("task:" + t.id) + span_(st.muted, q_(t.id)) + " " + esc_(cluster::one_line(t.prompt, 90)) + "</a>"
                    + (t.reason.empty() ? QString() : "<br>" + span_(st.warn, "↳ " + esc_(t.reason)))
                    + (cluster::task_traits(t).empty() ? QString() : "<br>" + span_(st.muted, "· " + esc_(cluster::task_traits(t))))
                    + (t.status == "done" && !t.result.empty() && i < 4 ? "<br>" + span_(st.muted, "= " + esc_(cluster::one_line(t.result, 120))) : QString()) + "</td><td class=m>→ " + esc_(where) + "</td><td class=m>by "
                    + esc_(who(t.by)) + "</td><td class=m>" + q_(age) + "</td></tr>";
            }
            html += "</table>" + (tasks.size() > 12 ? "<p class=m>… +" + QString::number(tasks.size() - 12) + " more</p>" : QString())
                + (tasks.empty() ? "<p class=m>empty: type a prompt below</p>" : QString());
            // the heavy jobs of the resource orchestrator: the gauges of the computer, what runs, waits, is stopped
            if (const cluster::Json jobs = this->manager.jobsState(); jobs.is_object() && !jobs.empty()) {
                const cluster::Json host = jobs.value("host", cluster::Json::object());
                const int level = jobs.value("level", 0);
                char gauge[200];
                std::snprintf(gauge, sizeof(gauge), "load %.1f/core · %ld MB free · swap %ld MB", host.value("load_per_core", 0.0), host.value("mem_avail_mb", 0L), host.value("swap_used_mb", 0L));
                html += "<h3>Heavy jobs</h3><p class=m>" + QString::number(jobs.value("running", 0)) + "/" + QString::number(jobs.value("slots", 0)) + " running · " + QString::number(jobs.value("waiting", 0))
                    + " waiting" + (jobs.value("stopped", 0) > 0 ? " · " + QString::number(jobs.value("stopped", 0)) + " stopped" : QString()) + " · profile " + q_(jobs.value("profile", std::string("low")))
                    + (jobs.value("enabled", true) ? QString() : QString(" · OFF")) + "<br>" + q_(gauge) + " · "
                    + span_(level == 0 ? st.ok : level == 1 ? st.warn : st.error, level == 0 ? "calm" : level == 1 ? "hold: nothing new starts" : "reduce: jobs slowed")
                    + (level > 0 && !jobs.value("pressure", std::string()).empty() ? "<br>" + span_(st.warn, "↳ " + esc_(jobs.value("pressure", std::string()))) : QString()) + "</p><table width=100%>";
                std::size_t listed = 0;
                for (const cluster::Json& job: jobs.value("jobs", cluster::Json::array())) {
                    if (++listed > 14) break;
                    const std::string state = job.value("state", std::string());
                    const std::string tint = state == "running" ? st.ok : state == "queued" ? st.accent : state == "failed" ? st.error : state == "done" ? st.muted : st.warn;
                    const std::string mark = state == "running" ? "▶" : state == "queued" ? "◌" : state == "stopped" ? "⏸" : state == "done" ? "✓" : state == "failed" ? "✗" : "⊘";
                    const std::string session = job.value("session", std::string());
                    const std::string age = state == "queued" ? "#" + std::to_string(job.value("position", 0)) + " ETA " + span(job.value("eta_s", std::int64_t{0})) : state == "running" ? span(now - job.value("started", now))
                        : state == "stopped" ? "stopped" : span(now - job.value("ended", now)) + " ago";
                    const std::string id = job.value("id", std::string());
                    html += "<tr><td width=18>" + span_(tint, q_(mark)) + "</td><td width=\"55%\">" + link("job:" + id) + span_(st.muted, q_(id)) + " " + span_(st.accent, q_(job.value("class", std::string()))) + " "
                        + esc_(cluster::one_line(job.value("name", std::string()).empty() ? job.value("command", std::string()) : job.value("name", std::string()), 90)) + "</a>"
                        + (state == "queued" && !job.value("waiting_for", std::string()).empty() ? "<br>" + span_(st.muted, "↳ waits for " + esc_(job.value("waiting_for", std::string()))) : QString())
                        + (state == "stopped" || state == "killed" || state == "failed" ? "<br>" + span_(st.warn, "↳ " + esc_(job.value("message", std::string()))) : QString())
                        + "</td><td class=m>→ " + esc_(session.empty() ? std::string("you") : who(session)) + "</td><td class=m>" + q_(age) + "</td></tr>";
                }
                html += "</table>";
                for (const cluster::Json& d: jobs.value("detected", cluster::Json::array()))
                    html += "<p>" + span_(st.warn, "! outside the queue: " + esc_(cluster::one_line(d.value("command", std::string()), 100)) + " (" + QString::number(d.value("cpu_seconds", 0)) + " s CPU, "
                        + esc_(who(d.value("session", std::string()))) + ")") + "</p>";
                if (jobs.value("jobs", cluster::Json::array()).empty()) html += "<p class=m>none: heavy commands go through cluster-run / heavy_run and wait here for a slot</p>";
            }
            // the schedules
            std::vector<cluster::Schedule> planned;
            int waiting = 0;
            for (const cluster::Schedule& w: this->manager.schedules()) {
                if (w.archived) {
                    ++archived;
                    continue;
                }
                waiting += w.status == "waiting" ? 1 : 0;
                planned.push_back(w);
            }
            std::stable_sort(planned.begin(), planned.end(), [](const cluster::Schedule& a, const cluster::Schedule& b) {
                return (a.status == "waiting") != (b.status == "waiting") ? a.status == "waiting" : a.status == "waiting" ? a.at < b.at : a.last > b.last;
            });
            html += "<h3>Scheduled</h3><p class=m>" + QString::number(waiting) + " waiting · " + QString::number(planned.size() - static_cast<std::size_t>(waiting)) + " run / cancelled</p><table width=100%>";
            for (std::size_t i = 0; i < planned.size() && i < 12; ++i) {
                const cluster::Schedule& w = planned[i];
                const bool open = w.status == "waiting";
                const std::string when = open ? "in " + span(w.at - now) + " (" + cluster::clock_time(w.at) + ")" : w.status == "paused" ? "paused" : w.status == "done" ? "run " + span(now - w.last) + " ago" : "cancelled";
                html += "<tr><td width=18>" + span_(open ? st.accent : w.status == "done" ? st.muted : st.warn, open ? "⏱" : w.status == "done" ? "✓" : "⊘") + "</td><td>" + link("sched:" + w.id)
                    + span_(st.muted, q_(w.id)) + " " + esc_(cluster::one_line(w.text, 80)) + "</a>" + (w.reason.empty() ? QString() : "<br>" + span_(st.warn, "↳ " + esc_(w.reason)))
                    + (cluster::schedule_rule(w).empty() ? QString() : "<br>" + span_(st.muted, "· " + esc_(cluster::schedule_rule(w))))
                    + (w.note.empty() || w.runs + w.skipped == 0 ? QString() : "<br>" + span_(st.muted, "↻ " + esc_(cluster::one_line(w.note, 100))))
                    + "</td><td class=m width=\"22%\">" + q_(when + (w.every > 0 ? " · every " + span(static_cast<std::int64_t>(w.every) * 60) : "")) + "</td><td class=m>"
                    + esc_(w.action == "notify" ? std::string("a notification") : w.action == "spawn" ? std::string("a new session") : w.action == "task" ? std::string("a task") : w.action == "command" ? std::string("a command") : "→ " + (w.session == GLOBAL_ID ? std::string("global") : w.session)) + "</td><td class=m>by "
                    + esc_(who(w.by)) + "</td></tr>";
            }
            html += "</table>" + (planned.empty() ? "<p class=m>none</p>" : QString());
            if (archived > 0) html += "<p>" + link("archive:") + span_(st.accent, "▸ " + QString::number(archived) + " archived: open") + "</a></p>";
            if (html.toStdString() == this->tasksShownHtml && browser == this->tasksView) return; // unchanged: its scroll kept
            if (browser == this->tasksView) this->tasksShownHtml = html.toStdString();
            const int scroll = browser->verticalScrollBar()->value();
            browser->setHtml(html);
            browser->verticalScrollBar()->setValue(scroll);
        }

        QLabel* brand = nullptr;                // the top bar: its face and the name
        QLabel* webLabel = nullptr;             // the web access under it

        void webLink(const QString& link)
        {
            // the voice bar: the sentence waiting sent now, dropped, corrected
            if (link == "voice-send") {
                this->run({"voice_confirm", "", "", "", false}, "");
                return;
            }
            if (link == "voice-drop") {
                this->app.voice->cancelLast();
                this->message("voice prompt dropped", false);
                return;
            }
            if (link == "voice-edit") {
                std::string last;
                for (const cluster::Pending& p: this->app.voice->pending())
                    if (!p.target.empty()) last = p.text;
                bool ok = false;
                const QString text = QInputDialog::getText(this, "Correct the voice prompt", "What you said (sent after its countdown):", QLineEdit::Normal, q_(last), &ok);
                if (ok && !text.trimmed().isEmpty()) this->run({"voice_edit", "", "", "", false}, text.trimmed().toStdString());
                return;
            }
            // the web line: its code copied (ready to paste in the page), its devices
            const std::optional<cluster::WebStatus> web = cluster::web_status(this->manager);
            if (!web) return;
            if (link == "web-devices") {
                this->webDevices();
                return;
            }
            QString digits;
            for (const QChar c: q_(web->pin))
                if (c.isDigit()) digits += c;
            QGuiApplication::clipboard()->setText(digits);
            this->message("web: code " + web->pin + " copied", false);
        }
        bool webAsking = false;
        std::set<std::string> webAsked;

        void showFace(void)
        {
            QTimer::singleShot(0, this, [this]() {this->webAsk();}); // a device asking to connect: its dialog
            const cluster::AppFace face = this->manager.appFace(this->app.voice->listening());
            // the update not done yet: on the top bar and the title (waiting: what for; offered: installed)
            const cluster::UpgradeOffer offer = this->manager.upgradeOffer();
            const std::string update = offer.started || offer.upgrading ? "⟳ update → " + (offer.target.empty() ? offer.version : offer.target)
                + (offer.upgrading ? ": switching" : offer.waiting.empty() ? std::string(": saving") : ": waiting for " + cluster::one_line(offer.waiting, 60))
                : offer.version.empty() ? std::string() : "⬆ " + offer.version + " installed";
            const std::string key = std::string(cluster::face_name(face.face)) + std::to_string(face.badge) + (face.urgent ? "!" : "") + update;
            cluster::dock_badge(face.badge, face.urgent);
            if (key == this->faceShown) return;
            this->faceShown = key;
            QPixmap pixmap;
            pixmap.loadFromData(QByteArray::fromStdString(cluster::face_svg(face.face, face.badge, face.urgent)), "SVG");
            this->setWindowIcon(QIcon(pixmap));
            this->setWindowTitle(QString("(") + QString::fromUtf8(cluster::face_text(face.face)) + ") " + (face.badge > 0 ? QString::number(face.badge) + " · " : QString())
                + "claude-cluster" + (update.empty() ? QString() : " · " + q_(update)));
            if (this->brand) {
                const cluster::Style& st = this->theme();
                this->brand->setText(pill_(cluster::face_text(face.face), st.accent, mix_(st.surface, st.accent, 0.18), true, true, 9.5) + "&nbsp; claude-cluster"
                    + (update.empty() ? QString() : "&nbsp; " + pill_(update, offer.started || offer.upgrading ? st.warn : st.accent,
                        mix_(st.surface, offer.started || offer.upgrading ? st.warn : st.accent, 0.18), true, true, 9.0)));
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
                "QFrame#segs{border:1px solid " + s.border + ";border-radius:10px;background:" + s.bg + ";}"
                "QPushButton#seg{border:1px solid transparent;border-radius:7px;padding:4px 14px;background:transparent;margin:0;}"
                "QPushButton#seg:hover{background:" + hover + ";}"
                "QPushButton#seg:checked{background:" + tint + ";color:" + s.accent + ";font-weight:600;border-color:" + s.accent + ";}"
                "QFrame#session{background:transparent;border:none;}"
                "QLabel#head{border:none;border-bottom:1px solid " + s.border + ";border-top-left-radius:12px;border-top-right-radius:12px;}"
                "QWidget#cardBody{background:" + s.bg + ";border:none;border-bottom-left-radius:12px;border-bottom-right-radius:12px;}"
                "QLabel#head{padding:10px 12px;background:" + s.surface + ";}"
                "QLineEdit,QTextEdit,QPlainTextEdit,QSpinBox,QDoubleSpinBox{border:1px solid " + s.border + ";border-radius:10px;padding:7px 10px;background:" + s.surface + ";}"
                "QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus{border-color:" + s.accent + ";}"
                "QFrame#composer{background:" + s.surface + ";border:1px solid " + s.border + ";border-radius:14px;}"
                "QFrame#composer QLineEdit,QFrame#composer QTextEdit,QFrame#composer QPlainTextEdit{border:none;background:transparent;padding:2px 4px;}"
                "QFrame#pinBox{border:1px solid " + s.accent + ";border-radius:8px;background:" + s.surface + ";}"
                "QFrame#pinBox QTextBrowser{background:" + s.surface + ";}"
                "QTextBrowser{border:none;border-radius:0;background:" + s.bg + ";selection-background-color:" + s.accent + ";selection-color:" + s.bg + ";padding:6px 10px;}"
                "QTextBrowser#tasksPanel{border:none;border-radius:12px;background:" + s.bg + ";padding:12px 16px;}"
                "QTreeWidget{border:1px solid " + s.border + ";border-radius:12px;background:" + s.bg + ";padding:6px;outline:0;}"
                "QTreeWidget::item{padding:4px 2px;border-radius:6px;}"
                "QTreeWidget::item:hover{background:" + hover + ";}"
                "QTreeWidget::item:selected{background:" + tint + ";color:" + s.text + ";}"
                "QHeaderView::section{background:" + s.surface + ";color:" + s.muted + ";border:none;border-bottom:1px solid " + s.border + ";padding:6px 8px;}"
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
                if (action.starts_with("graph_")) continue; // the graph / hub view reads them (shortcuts of the window: letters lost by every view)
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
                    const std::vector<cluster::Snapshot> all = this->manager.shown();
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

        void openProgram(const std::string& command)
        {
            // a program that needs a terminal (vim, less, ssh...): a terminal emulator of the computer runs it in the folder of the session
            const std::optional<cluster::Snapshot> snap = this->manager.snapshot(this->active);
            const std::string folder = snap ? cluster::expand_home(snap->spec.cwd).string() : std::filesystem::current_path().string();
            const std::string terminal = cluster::open_terminal_command(folder, command);
            this->message(terminal.empty() ? "no terminal emulator found to run " + cluster::one_line(command, 40) + " (install one, or use the terminal interface)"
                : cluster::one_line(command, 40) + " opened in " + terminal, terminal.empty());
        }

        void info(const std::string& text)
        {
            // a text of several lines in a popup (the information of a session...), in a fixed font
            QMessageBox box(QMessageBox::Information, "claude-cluster", QString(), QMessageBox::Ok, this);
            box.setTextFormat(Qt::PlainText);
            box.setText(q_(text));
            box.setStyleSheet("QLabel {font-family: monospace;}");
            box.exec();
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
            } else if (base == "new_project") {
                QTimer::singleShot(0, this, [this]() {this->palette("new:");});
            } else if (base == "setting") {
                this->settings(arg); // found in the palette: opened at it
            } else if (base == "settings_search") {
                this->settings("", true);
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
                this->runAction(action.id, value);
            }
            this->refresh(true);
        }

        void confirmDelete(const std::string& id)
        {
            // a session deleted for good: its conversation gone, not in the trash; asked first
            const std::optional<cluster::Snapshot> snap = this->manager.snapshot(id);
            if (!snap) return;
            if (QMessageBox::warning(this, "Delete for good", q_("Delete " + snap->spec.name + " for good?\n\nIts conversation is gone, it is not in the trash "
                "(Archive keeps it restorable)."), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
            this->runAction("delete_session:" + id, "");
        }

        void runAction(const std::string& id, const std::string& value)
        {
            // run in the worker: the window goes on (its progress box when it takes time), its result back here
            const std::string session = this->active;
            this->worker.push([this, id, value, session]() {
                const cluster::ActionResult result = cluster::run_action(this->manager, *this->app.voice, id, session, value);
                QMetaObject::invokeMethod(this, [this, result]() {
                    if (!result.focus.empty()) this->active = result.focus;
                    if (!result.message.empty()) this->message(result.message, result.error);
                    if (result.ui.starts_with("term:")) {
                        this->openProgram(result.ui.substr(5));
                    } else if (result.ui == "message") {
                        this->info(result.message); // a popup (/info...): never in the conversation
                    } else if (result.ui == "plugins") {
                        QTimer::singleShot(0, this, [this]() {this->pluginsDialog();});
                    } else if (result.ui.starts_with("pick:delete:")) { // a session deleted for good: asked here
                        this->confirmDelete(result.ui.substr(12));
                    } else if (result.ui.starts_with("pick:")) { // a next step to pick (where it applies), once this palette is closed
                        const QString next = q_(result.ui.substr(5));
                        QTimer::singleShot(0, this, [this, next]() {this->palette(next);});
                    }
                    this->refresh(true);
                }, Qt::QueuedConnection);
            });
        }

        void hubMenu(const std::string& node, const QPoint& at)
        {
            // the right click in the graph: a node of the taxonomy (focus, set parent, rename, merge, delete), else the graph (tree / ring, archived, tags, fold)
            const cluster::Taxonomy taxonomy = this->manager.taxonomy();
            const std::function<void(const std::string&, const std::string&, const std::string&, const bool)> edit = [this](const std::string& action, const std::string& name, const std::string& other, const bool strip) {
                try {
                    this->manager.taxonomyEdit(action, name, other, strip);
                    this->message("taxonomy: " + action + " " + name, false);
                } catch (const utils::exception::IException& error) {
                    this->message(error.info(), true);
                }
                this->refresh(true);
            };
            QMenu menu(this);
            if (!node.empty()) {
                menu.addAction(q_("Focus the graph on " + node), [this, node]() {
                    this->hub->focus = node;
                    this->refresh(true);
                });
                menu.addAction("Set parent…", [this, node, taxonomy, edit]() {
                    bool ok = false;
                    const QString parent = QInputDialog::getText(this, "Set parent", q_("Parent of " + node + " (a category / tag; empty: under global)"), QLineEdit::Normal,
                        q_(taxonomy.parent(node)), &ok);
                    if (ok) edit("set_parent", node, parent.trimmed().toStdString(), false);
                });
                menu.addAction("Rename…", [this, node, edit]() {
                    bool ok = false;
                    const QString name = QInputDialog::getText(this, "Rename", q_("New name of " + node + " (on every session that has it)"), QLineEdit::Normal, q_(node), &ok);
                    if (ok && !name.trimmed().isEmpty()) edit("rename", node, name.trimmed().toStdString(), false);
                });
                menu.addAction("Merge into…", [this, node, edit]() {
                    bool ok = false;
                    const QString into = QInputDialog::getText(this, "Merge", q_("Merge " + node + " into (another category / tag)"), QLineEdit::Normal, QString(), &ok);
                    if (ok && !into.trimmed().isEmpty()) edit("merge", node, into.trimmed().toStdString(), false);
                });
                menu.addAction("Delete", [this, node, edit]() {
                    if (QMessageBox::question(this, "Delete", q_("Delete the node " + node + "? Its children go to its parent.")) != QMessageBox::Yes) return;
                    const bool strip = QMessageBox::question(this, "Delete", q_("Also remove the label " + node + " from the sessions that have it? (No: they keep it)")) == QMessageBox::Yes;
                    edit("delete", node, "", strip);
                });
                menu.addSeparator();
            }
            menu.addAction(this->hub->ring ? "Graph: the categories and tags between" : "Hub: every session around the global session", [this]() {
                this->manager.setUi("layout", this->hub->ring ? "graph" : "hub");
                this->refresh(true);
            });
            if (!this->hub->focus.empty())
                menu.addAction("The whole tree (from global)", [this]() {
                    this->hub->focus.clear();
                    this->refresh(true);
                });
            menu.addAction(this->hub->archived ? "Archived sessions: hide" : "Archived sessions: show (greyed)", [this]() {
                this->hub->archived = !this->hub->archived;
                this->refresh(true);
            });
            menu.addAction(this->hub->byTags ? "By category, else first tag" : "By every tag (a session under each)", [this]() {
                this->hub->byTags = !this->hub->byTags;
                this->refresh(true);
            });
            menu.addAction("Unfold all", [this]() {
                this->hub->collapsed.clear();
                this->refresh(true);
            });
            menu.addAction("Fold all", [this, taxonomy]() {
                const std::vector<std::string> all = taxonomy.nodes();
                this->hub->collapsed.insert(all.begin(), all.end());
                this->refresh(true);
            });
            menu.addAction("Add node…", [this, edit]() {
                bool ok = false;
                const QString text = QInputDialog::getText(this, "Add node", "A node of the tree (a category / tag) and its parent: api backend", QLineEdit::Normal, QString(), &ok);
                const QStringList words = text.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
                if (ok && !words.isEmpty()) edit("set_parent", words[0].toStdString(), words.size() > 1 ? words[1].toStdString() : std::string(), false);
            });
            menu.exec(at);
        }

        void sessionMenu(const std::string& id, const QPoint& where)
        {
            // a session right-clicked: what it can do (open, rename, describe, move, archive, delete for good)
            QMenu menu(this);
            const std::vector<cluster::Action> items = cluster::filter_actions({}, "menu:" + id, &this->manager);
            for (const cluster::Action& action: items) {
                if (action.id.starts_with("delete_ask:")) menu.addSeparator();
                QAction* entry = menu.addAction(q_(action.title));
                QObject::connect(entry, &QAction::triggered, this, [this, action, id]() {
                    this->active = id;
                    if (action.id.starts_with("delete_ask:")) this->confirmDelete(id);
                    else this->choose(action);
                });
            }
            (void)menu.exec(where);
        }

        void choose(const cluster::Action& action)
        {
            if (action.input.empty()) {
                this->run(action, "");
                return;
            }
            if (action.input == "type yes to confirm") { // a confirmation: a yes / no popup
                if (QMessageBox::question(this, "claude-cluster", q_(action.title + "?"), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes)
                    this->run(action, "yes");
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
            // grid: rows and columns (as the terminal); list, tabs: previous / next
            const std::vector<cluster::Snapshot> sessions = this->manager.shown();
            const int count = static_cast<int>(sessions.size());
            if (count == 0) return;
            const bool grid = this->manager.ui("layout", this->manager.config().layout) == "grid";
            const int columns = grid ? this->gridColumns : 1;
            if (!grid || columns == 1) {
                this->move(dx != 0 ? dx : dy);
                return;
            }
            // the sessions, then the Tasks as the last cell; left / right turn inside the row (the last one has fewer cells), up / down inside the
            // column, from the top to the bottom and back; under a shorter last row, its last cell
            const int cellCount = count + 1;
            const auto it = std::find_if(sessions.begin(), sessions.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->active;});
            const int index = this->active == TASKS_ID ? count : it == sessions.end() ? 0 : static_cast<int>(it - sessions.begin());
            const int rows = (cellCount + columns - 1) / columns;
            int row = index / columns;
            int column = index % columns;
            if (dx != 0) {
                const int cells = std::min(columns, cellCount - row * columns);
                column = (column + dx + cells) % cells;
            } else {
                row = (row + dy + rows) % rows;
                column = std::min(column, cellCount - row * columns - 1);
            }
            const int chosen = row * columns + column;
            this->active = chosen == count ? std::string(TASKS_ID) : sessions[static_cast<std::size_t>(chosen)].spec.id;
            this->refresh(true);
            QWidget* shown = this->active == TASKS_ID ? static_cast<QWidget*>(this->tasksView) : this->views.contains(this->active) ? this->views[this->active] : nullptr;
            if (this->gridArea && shown) this->gridArea->ensureWidgetVisible(shown, 0, 0);
        }

        bool gridKey(const QKeyEvent* event)
        {
            // the grid, the prompt empty: the arrows move between the cells, Shift+Enter opens the cell in the list (a prompt being written: they
            // edit it as ever)
            if (this->manager.ui("layout", this->manager.config().layout) != "grid") return false;
            const Qt::KeyboardModifiers held = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
            if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && held == Qt::ShiftModifier) return this->gridOpen();
            if (held != Qt::NoModifier) return false;
            if (event->key() == Qt::Key_Left) this->moveDir(-1, 0);
            else if (event->key() == Qt::Key_Right) this->moveDir(1, 0);
            else if (event->key() == Qt::Key_Up) this->moveDir(0, -1);
            else if (event->key() == Qt::Key_Down) this->moveDir(0, 1);
            else return false;
            return true;
        }

        bool gridOpen(void)
        {
            // the cell chosen in the grid: opened in the list
            if (this->manager.ui("layout", this->manager.config().layout) != "grid") return false;
            this->manager.setUi("layout", "list");
            this->refresh(true);
            this->message("opened in the list", false);
            return true;
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
                shown = cluster::filter_actions(all, wanted, &this->manager); // the closest first, the settings found too
                for (const cluster::Action& a: shown)
                    items->addItem(q_(a.title));
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

        _nodiscard bool confirmDangers(const std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>>& changes, QWidget* parent)
        {
            // the save with dangerous values: one popup listing them with their warning; yes: saved, no: nothing saved
            QString listed;
            std::vector<std::string> packages;
            for (const auto &[where, change]: changes)
                if (const cluster::Requirement need = cluster::setting_requirement(this->manager, change.first, change.second); !need.problem.empty()) {
                    listed += "<p><b style=\"color:" + q_(this->theme().error) + "\">✗ " + esc_(change.first.label) + "</b> = " + esc_(change.second) + "<br><span style=\"color:"
                        + q_(this->theme().muted) + "\">" + esc_(need.problem) + "</span></p>";
                    if (!need.package.empty()) packages.push_back(need.package);
                }
            for (const auto &[where, change]: changes)
                if (const std::string warning = cluster::setting_danger(change.first, change.second); !warning.empty())
                    listed += "<p><b style=\"color:" + q_(this->theme().error) + "\">⚠ " + esc_(change.first.label) + "</b> = " + esc_(change.second.empty() ? "(empty)" : change.second)
                        + "<br><span style=\"color:" + q_(this->theme().muted) + "\">" + esc_(warning) + "</span></p>";
            if (listed.isEmpty()) return true;
            QMessageBox box(QMessageBox::Warning, "Before saving", "<b>Save these settings?</b>" + listed, QMessageBox::Yes | QMessageBox::No, parent);
            box.setTextFormat(Qt::RichText);
            box.button(QMessageBox::Yes)->setText(packages.empty() ? "Save them" : "Save anyway");
            box.button(QMessageBox::No)->setText("Back (nothing saved)");
            QPushButton* install = packages.empty() ? nullptr : box.addButton("Install, then save", QMessageBox::AcceptRole);
            box.setDefaultButton(QMessageBox::No);
            const int answer = box.exec();
            if (install != nullptr && box.clickedButton() == install) {
                for (const std::string& package: packages) {
                    const std::string error = cluster::install_package(package, &this->manager); // its password window, its popup
                    this->message(error.empty() ? package + " installed" : error, !error.empty());
                }
                return true;
            }
            return answer == QMessageBox::Yes;
        }

        void openDocs(const std::string& screen)
        {
            // the documentation embedded in this binary, in the browser of the desktop (its copy on disk: nothing to serve, no web access needed)
            const cluster::DocOpened docs = cluster::docs_open(screen);
            this->message(docs.url.empty() ? "the documentation is not in this build" : (docs.opened ? "docs: " : "docs, open it in a browser: ") + docs.url, docs.url.empty());
        }

        void settings(const std::string& focus = "", const bool search = false)
        {
            // Every option of config.toml, one tab per section; saved in the file (comments kept), reloaded live; focus: the
            // one shown (section.key), search: its field first
            QDialog dialog(this);
            dialog.setWindowTitle("claude-cluster setup");
            dialog.resize(860, 620);
            dialog.setPalette(QApplication::palette());
            dialog.setStyleSheet(this->styleSheet());
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            QTabWidget* sections = new QTabWidget(&dialog);
            const cluster::Config& config = this->manager.config();
            std::vector<std::pair<cluster::Setting, std::function<std::string()>>> fields; // <setting, value of its widget>
            std::map<std::string, std::string> opened;     // section.key -> its value as the page showed it at its opening (or at its last save)
            const std::function<bool(const cluster::Setting&, const std::string&)> changed = [&opened](const cluster::Setting& setting, const std::string& value) {
                const auto it = opened.find(setting.section + "." + setting.key);
                return it != opened.end() && it->second != value;
            };
            const std::vector<cluster::SettingsPage> pages = cluster::settings_pages(config);
            std::vector<std::vector<std::pair<QLabel*, QWidget*>>> placed(pages.size()); // per page, per setting: its label, its widget
            std::vector<QScrollArea*> scrolls;
            std::vector<std::function<void()>> marks; // the dangerous marks, followed while the values change
            // the Services tab: its cards filled from services() (read by the worker while the tab is shown)
            struct ServicesView {
                std::vector<std::function<void(const cluster::Json&)>> fills;
                std::int64_t at = 0;            // ms of its last read (0: read again)
                std::uint64_t seen = 0;         // the version of the core it was read at
                bool reading = false;
                bool alive = true;              // the dialog still open
                QWidget* page = nullptr;        // its tab
                QWidget* profiles = nullptr;    // the Profiles tab (the same cards)
            };
            const std::shared_ptr<ServicesView> svc = std::make_shared<ServicesView>();

            for (const cluster::SettingsPage& page: pages) {
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
                    } else if (setting.kind == cluster::SettingKind::Action && (page.section == "services" || page.section == "profiles")) {
                        // a sub service: its card (its state, its lines, its buttons)
                        const std::string name = setting.key.substr(4);
                        QFrame* card = new QFrame(tab);
                        card->setObjectName("svcCard");
                        card->setStyleSheet("QFrame#svcCard{border:1px solid " + q_(this->theme().border) + ";border-radius:8px;}");
                        QVBoxLayout* inside = new QVBoxLayout(card);
                        QLabel* state = new QLabel(card);
                        state->setTextFormat(Qt::RichText);
                        state->setWordWrap(true);
                        state->setTextInteractionFlags(Qt::TextSelectableByMouse);
                        inside->addWidget(state);
                        QHBoxLayout* row = new QHBoxLayout();
                        std::vector<QPushButton*> buttons;
                        for (int b = 0; b < 6; ++b) {
                            QPushButton* button = new QPushButton(card);
                            button->hide();
                            row->addWidget(button);
                            buttons.push_back(button);
                            QObject::connect(button, &QPushButton::clicked, &dialog, [this, button, name, svc, &dialog]() {
                                std::string action = button->property("action").toString().toStdString();
                                const std::string label = button->text().toStdString();
                                if (cluster::service_asks(action)) { // a text first (a profile's name)
                                    bool ok = false;
                                    const QString text = QInputDialog::getText(&dialog, q_(label), q_(cluster::service_ask_question(action)), QLineEdit::Normal, QString(), &ok);
                                    if (!ok || text.trimmed().isEmpty()) return;
                                    action = cluster::service_ask_action(action) + ":" + text.trimmed().toStdString();
                                }
                                if (cluster::service_risky(name, action)
                                    && QMessageBox::question(&dialog, q_(label), q_(label + " (" + name + ")?")) != QMessageBox::Yes) return;
                                button->setEnabled(false);
                                this->worker.push([this, name, action, button, svc]() { // its progress / failure popups
                                    std::string done;
                                    bool failed = false;
                                    try {
                                        done = cluster::service_act(this->manager, name, action);
                                    } catch (const utils::exception::IException& e) {
                                        done = std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info());
                                        failed = true;
                                    }
                                    const bool rows = !failed && (name == "profiles" || name.starts_with("profile:"))
                                        && !action.starts_with("note") && action != "apply" && action != "save_over" && action != "undo";
                                    QMetaObject::invokeMethod(this, [this, done, failed, button, svc, rows]() {
                                        this->message(done, failed);
                                        if (!svc->alive) return;
                                        if (rows) { // a profile added, renamed, gone: the settings opened again on this tab (its rows)
                                            if (QDialog* open = qobject_cast<QDialog*>(button->window())) open->reject();
                                            QTimer::singleShot(0, this, [this]() {this->settings();});
                                            return;
                                        }
                                        button->setEnabled(true);
                                        svc->at = 0; // read again
                                    }, Qt::QueuedConnection);
                                });
                            });
                        }
                        row->addStretch(1);
                        inside->addLayout(row);
                        const cluster::Style st = this->theme();
                        svc->fills.push_back([card, state, buttons, name, st](const cluster::Json& all) {
                            QString html = "<b>" + esc_(cluster::service_summary(name, all)) + "</b>";
                            for (const cluster::ServiceLine& l: cluster::service_lines(name, all)) {
                                const std::string color = l.kind == "error" ? st.error : l.kind == "ok" ? st.ok : l.kind == "warn" ? st.warn : l.kind == "heard" ? st.accent
                                    : l.kind == "text" ? st.text : st.muted;
                                html += "<br>" + (l.kind == "log" ? "<span style=\"color:" + q_(color) + ";font-family:monospace;font-size:9pt\">" + esc_(l.text) + "</span>"
                                    : span_(color, esc_(l.text)));
                            }
                            state->setText(html);
                            state->setMinimumHeight(state->heightForWidth(std::max(320, state->width()))); // its lines all shown (wrapped rich text)
                            card->setMinimumHeight(card->layout()->totalMinimumSize().height()); // the card around them (its buttons under them)
                            card->updateGeometry();
                            const std::vector<std::pair<std::string, std::string>> list = cluster::service_buttons(name, all);
                            for (std::size_t b = 0; b < buttons.size(); ++b) {
                                buttons[b]->setVisible(b < list.size());
                                if (b >= list.size()) continue;
                                buttons[b]->setText(q_(list[b].first));
                                buttons[b]->setProperty("action", q_(list[b].second));
                            }
                        });
                        svc->fills.back()(cluster::Json()); // "reading…"
                        read = [current]() {return current;}; // not an option: never a change
                        widget = card;
                    } else if (setting.kind == cluster::SettingKind::Action) {
                        QPushButton* button = new QPushButton(q_("▶ " + current), tab);
                        const std::string key = setting.key;
                        QObject::connect(button, &QPushButton::clicked, [this, key, button]() {
                            if (key.starts_with("showcase_")) button->window()->close(); // the settings closed: the showcase seen where it happens
                            this->settingAction(key);
                        });
                        read = [current]() {return current;}; // not an option: never a change
                        widget = button;
                    } else {
                        QLineEdit* line = new QLineEdit(q_(current), tab);
                        read = [line]() {return cluster::trim(line->text().toStdString());};
                        widget = line;
                        if (setting.kind == cluster::SettingKind::Text || setting.kind == cluster::SettingKind::List) { // the values it takes, suggested as typed
                            QStringListModel* model = new QStringListModel(line);
                            QCompleter* completer = new QCompleter(model, line);
                            completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion); // already ranked and complete values: shown as they are
                            line->setCompleter(completer);
                            QObject::connect(line, &QLineEdit::textEdited, line, [model, line, setting, &config]() {
                                QStringList values;
                                for (const std::string& value: cluster::setting_suggestions(config, setting, line->text().toStdString()))
                                    values << q_(value);
                                model->setStringList(values);
                            });
                        }
                    }
                    widget->setToolTip(tip);
                    QLabel* label = new QLabel(tab);
                    label->setTextFormat(Qt::RichText);
                    label->setToolTip(tip);
                    const cluster::Style st = this->theme();
                    // ⚠: it can be dangerous; red: its value is; !: one of its keys another action has too (the keys as typed in the page)
                    const std::function<void()> mark = [label, widget, setting, read, st, &fields, &config]() {
                        const bool now = !cluster::setting_danger(setting, read()).empty();
                        std::string twice;
                        if (setting.section == "keys") {
                            std::map<std::string, std::string> keys = config.keys;
                            for (const auto &[other, value]: fields)
                                if (other.section == "keys") keys[other.key] = value();
                            keys[setting.key] = read();
                            for (const auto &[key, others]: cluster::setting_key_clashes(keys, setting.key)) {
                                std::string names;
                                for (const std::string& o: others)
                                    names += (names.empty() ? "" : ", ") + o;
                                twice += (twice.empty() ? "" : " · ") + key + " is also " + names;
                            }
                            widget->setStyleSheet(twice.empty() ? QString() : "QLineEdit{color:" + q_(st.warn) + ";border:1px solid " + q_(st.warn) + ";}");
                            widget->setToolTip(twice.empty() ? q_(setting.description) : q_("! " + twice + " (only one of them works)"));
                        }
                        label->setText((now ? "<span style=\"color:" + q_(st.error) + ";font-weight:600\">" + esc_(setting.label) + "</span>" : esc_(setting.label))
                            + (setting.restart ? " *" : "") + (setting.danger ? " <span style=\"color:#e5b400\" title=\"can be dangerous\">⚠</span>" : "")
                            + (twice.empty() ? QString() : " <span style=\"color:" + q_(st.warn) + ";font-weight:800\">!</span>"));
                        label->setToolTip(twice.empty() ? q_(setting.description) : q_("! " + twice));
                    };
                    mark();
                    if (setting.danger || setting.section == "keys") marks.push_back(mark);
                    form->addRow(label, widget);
                    if (info != nullptr) form->addRow(info);
                    {
                        // what its value needs and is missing: red under it, its Install button
                        QWidget* needRow = new QWidget(tab);
                        QHBoxLayout* needLayout = new QHBoxLayout(needRow);
                        needLayout->setContentsMargins(0, 0, 0, 0);
                        QLabel* need = new QLabel(needRow);
                        need->setWordWrap(true);
                        need->setStyleSheet("color:" + q_(this->theme().error) + ";font-weight:600;");
                        QPushButton* install = new QPushButton(needRow);
                        needLayout->addWidget(need, 1);
                        needLayout->addWidget(install);
                        form->addRow(needRow);
                        const cluster::Setting copy = setting;
                        const std::function<std::string()> value = read;
                        const std::function<void()> check = [this, needRow, need, install, copy, value]() {
                            const cluster::Requirement r = cluster::setting_requirement(this->manager, copy, value());
                            needRow->setVisible(!r.problem.empty());
                            need->setText(q_("✗ " + r.problem));
                            install->setVisible(!r.package.empty());
                            install->setText(q_("Install " + r.package));
                            install->setProperty("package", q_(r.package));
                        };
                        QObject::connect(install, &QPushButton::clicked, this, [this, install, check]() {
                            const std::string package = install->property("package").toString().toStdString();
                            install->setEnabled(false);
                            install->setText("installing…");
                            const std::string error = cluster::install_package(package, &this->manager); // its password window, its popup
                            install->setEnabled(true);
                            this->message(error.empty() ? package + " installed" : error, !error.empty());
                            check();
                        });
                        check();
                        marks.push_back(check);
                    }
                    if (!setting.description.empty()) { // its help under it, line by line (not only on hover)
                        QLabel* help = new QLabel(q_(setting.description), tab);
                        help->setWordWrap(true);
                        help->setStyleSheet("color:" + q_(this->theme().muted) + ";font-size:11px;margin:0 0 6px 0;");
                        form->addRow(help);
                    }
                    fields.emplace_back(setting, read);
                    placed[scrolls.size()].emplace_back(label, widget);
                }
                if (page.section == "services") { // read again by hand (Refresh: manual)
                    QPushButton* again = new QPushButton("Read again", tab);
                    QObject::connect(again, &QPushButton::clicked, &dialog, [svc]() {svc->at = 0;});
                    form->addRow(again);
                }
                QScrollArea* scroll = new QScrollArea(sections);
                if (page.section == "services") svc->page = scroll;
                if (page.section == "profiles") svc->profiles = scroll;
                scrolls.push_back(scroll);
                scroll->setWidget(tab);
                scroll->setWidgetResizable(true);
                scroll->setFrameShape(QFrame::NoFrame);
                sections->addTab(scroll, q_(page.title));
            }
            for (std::size_t p = 0; p < pages.size(); ++p)
                if (pages[p].section == "keys") // its tab marked ! (warning colour) while a key is used twice
                    marks.push_back([this, sections, p, title = pages[p].title, &fields, &config]() {
                        std::map<std::string, std::string> keys = config.keys;
                        for (const auto &[other, value]: fields)
                            if (other.section == "keys") keys[other.key] = value();
                        const bool twice = std::any_of(keys.begin(), keys.end(), [&keys](const std::pair<const std::string, std::string>& k) {
                            return !cluster::setting_key_clashes(keys, k.first).empty();
                        });
                        const int index = static_cast<int>(p);
                        sections->setTabText(index, q_(title + (twice ? "  !" : "")));
                        sections->tabBar()->setTabTextColor(index, twice ? QColor(q_(this->theme().warn)) : QColor());
                    });
            QTimer* marking = new QTimer(&dialog);
            QObject::connect(marking, &QTimer::timeout, &dialog, [&marks]() {
                for (const std::function<void()>& mark: marks)
                    mark();
            });
            marking->start(400);
            // the Services tab shown: read (live: when the core changes, else every 0.7 s; interval: every Delay; manual: by hand)
            QObject::connect(&dialog, &QDialog::finished, &dialog, [svc]() {svc->alive = false;});
            const std::function<std::string(const std::string&)> svcSetting = [&fields](const std::string& key) {
                for (const auto &[setting, value]: fields)
                    if (setting.section == "services" && setting.key == key) return value();
                return std::string();
            };
            QTimer* reading = new QTimer(&dialog);
            QObject::connect(reading, &QTimer::timeout, &dialog, [this, svc, sections, svcSetting]() {
                if (svc->reading || (sections->currentWidget() != svc->page && sections->currentWidget() != svc->profiles) || sections->currentWidget() == nullptr) return;
                const std::string mode = svcSetting("refresh");
                const std::int64_t delay = static_cast<std::int64_t>(std::max(1, std::atoi(svcSetting("delay").c_str()))) * 1000;
                const std::int64_t now = cluster::now_ms();
                const std::uint64_t version = this->manager.version();
                if (svc->at != 0 && !(mode == "live" && ((version != svc->seen && now - svc->at >= 300) || now - svc->at >= 700))
                    && !(mode == "interval" && now - svc->at >= delay)) return;
                svc->reading = true;
                svc->seen = version;
                this->worker.push([this, svc]() {
                    cluster::Json got;
                    try {
                        got = cluster::services(this->manager);
                    } catch (const utils::exception::IException& e) {
                        got = {{"error", e.info()}};
                    }
                    QMetaObject::invokeMethod(this, [svc, got]() {
                        svc->reading = false;
                        svc->at = cluster::now_ms();
                        if (!svc->alive) return;
                        for (const std::function<void(const cluster::Json&)>& fill: svc->fills)
                            fill(got);
                    }, Qt::QueuedConnection);
                });
            });
            reading->start(250);
            // a setting shown: its tab, scrolled to it, its field focused, its label lit a moment
            const std::function<void(std::size_t, std::size_t)> showAt = [&sections, &scrolls, &placed, this](const std::size_t p, const std::size_t i) {
                if (p >= placed.size() || i >= placed[p].size()) return;
                sections->setCurrentIndex(static_cast<int>(p));
                const auto [label, widget] = placed[p][i];
                scrolls[p]->ensureWidgetVisible(widget, 40, 80);
                widget->setFocus();
                label->setStyleSheet("background:" + q_(this->theme().accent) + ";color:" + q_(this->theme().bg) + ";border-radius:4px;padding:1px 4px;");
                QTimer::singleShot(2500, label, [label]() {label->setStyleSheet("");});
            };
            // the search: every page, by its name (c2dmp-hsm) far before its description; each hit says where it matched
            QLineEdit* find = new QLineEdit(&dialog);
            find->setPlaceholderText("Search a setting: its name or words of its description (Ctrl+F)  ·  ↓ / Enter: open it");
            find->setClearButtonEnabled(true);
            QListWidget* hits = new QListWidget(&dialog);
            hits->setVisible(false);
            hits->setMaximumHeight(220);
            hits->setMouseTracking(true);
            hits->setStyleSheet("QListWidget::item:hover{background:" + q_(this->theme().surface) + ";}");
            std::vector<cluster::SettingHit> found;
            QObject::connect(find, &QLineEdit::textChanged, &dialog, [find, hits, &found, &pages]() {
                found = cluster::settings_search(pages, find->text().toStdString());
                hits->clear();
                for (const cluster::SettingHit& hit: found) {
                    const cluster::Setting& s = pages[hit.page].settings[hit.item];
                    QListWidgetItem* item = new QListWidgetItem(q_(s.label + "   · " + pages[hit.page].title + (hit.name ? "" : "   (found in its description)")), hits);
                    item->setToolTip(q_(s.description));
                }
                hits->setVisible(!find->text().trimmed().isEmpty());
                if (!found.empty()) hits->setCurrentRow(0);
            });
            const std::function<void()> open = [hits, find, &found, &showAt]() {
                const int row = hits->currentRow();
                if (row < 0 || static_cast<std::size_t>(row) >= found.size()) return;
                const cluster::SettingHit hit = found[static_cast<std::size_t>(row)];
                hits->setVisible(false);
                find->clearFocus();
                showAt(hit.page, hit.item);
            };
            QObject::connect(find, &QLineEdit::returnPressed, &dialog, open);
            QObject::connect(hits, &QListWidget::itemActivated, &dialog, open);
            QObject::connect(hits, &QListWidget::itemClicked, &dialog, open);
            QShortcut* findDown = new QShortcut(QKeySequence(Qt::Key_Down), find, nullptr, nullptr, Qt::WidgetShortcut);
            QObject::connect(findDown, &QShortcut::activated, hits, [hits]() {hits->setFocus(); hits->setCurrentRow(std::min(hits->currentRow() + 1, hits->count() - 1));});
            QShortcut* findKey = new QShortcut(QKeySequence::Find, &dialog);
            QObject::connect(findKey, &QShortcut::activated, find, [find]() {find->setFocus(); find->selectAll();});
            layout->addWidget(find);
            layout->addWidget(hits);
            QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, &fields, &dialog, changed]() {
                std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> now;
                for (const auto &[setting, read]: fields)
                    if (const std::string value = read(); changed(setting, value)) now[{setting.section, setting.key}] = {setting, value};
                if (this->confirmDangers(now, &dialog)) dialog.accept(); // back: the page stays open
            });
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            if (cluster::docs_built()) { // the help of this page: its docs page in the browser of the desktop
                QPushButton* help = buttons->addButton("Help", QDialogButtonBox::HelpRole);
                QObject::connect(help, &QPushButton::clicked, &dialog, [this]() {this->openDocs("settings");});
            }
            // Ctrl+S: saved, the page stays (the values written become the current ones); F10: save and close
            QShortcut* saveKey = new QShortcut(QKeySequence("Ctrl+S"), &dialog);
            QObject::connect(saveKey, &QShortcut::activated, [this, &fields, &dialog, &opened, changed]() {
                std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> now;
                for (const auto &[setting, read]: fields) {
                    const std::string value = read();
                    if (changed(setting, value)) now[{setting.section, setting.key}] = {setting, value};
                }
                if (!this->confirmDangers(now, &dialog)) return; // back: nothing saved
                if (now.empty()) return;
                try {
                    cluster::settings_write(this->manager.config(), now);
                } catch (const utils::exception::IException& e) {
                    this->message(std::string("not saved: ") + e.info(), true);
                    return;
                }
                for (const char* key: {"layout", "style", "vim", "group_by_category"})
                    if (now.contains({"ui", key})) this->manager.setUi(key, now.at({"ui", key}).second);
                (void)this->manager.config().reload();
                for (const auto &[key, change]: now)
                    opened[key.first + "." + key.second] = change.second;
                dialog.setWindowTitle(q_("claude-cluster setup - " + std::to_string(now.size()) + " setting(s) saved"));
            });
            QShortcut* closeKey = new QShortcut(QKeySequence(Qt::Key_F10), &dialog);
            QObject::connect(closeKey, &QShortcut::activated, buttons, &QDialogButtonBox::accepted);
            layout->addWidget(sections, 1);
            layout->addWidget(new QLabel("* applied at the next start · hover an option for its help · Ctrl+S save · F10 save & close · Esc close (asks to save the changes)", &dialog));
            layout->addWidget(buttons);
            // Debug: CLAUDE_CLUSTER_SCREENSHOT also saves this page (<file>-settings.png), then closes it
            const char* screenshot = std::getenv("CLAUDE_CLUSTER_SCREENSHOT");
            if (screenshot && *screenshot) {
                const QString path = QString(screenshot).replace(".png", "-settings.png");
                if (const char* tab = std::getenv("CLAUDE_CLUSTER_SCREENSHOT_TAB"); tab && *tab) // that tab shown (its title)
                    for (int i = 0; i < sections->count(); ++i)
                        if (sections->tabText(i) == QString(tab)) sections->setCurrentIndex(i);
                QTimer::singleShot(1200, &dialog, [&dialog, path]() {
                    dialog.grab().save(path);
                    dialog.reject();
                });
            }
            if (!focus.empty())
                for (std::size_t p = 0; p < pages.size(); ++p)
                    for (std::size_t i = 0; i < pages[p].settings.size(); ++i)
                        if (pages[p].settings[i].section + "." + pages[p].settings[i].key == focus)
                            QTimer::singleShot(0, &dialog, [showAt, p, i]() {showAt(p, i);});
            if (search) QTimer::singleShot(0, find, [find]() {find->setFocus();});
            if (focus.empty() && !std::getenv("CLAUDE_CLUSTER_SCREENSHOT_TAB") && this->settingsTab >= 0 && this->settingsTab < sections->count()) sections->setCurrentIndex(this->settingsTab); // where it was left
            for (const auto &[setting, read]: fields)
                opened[setting.section + "." + setting.key] = read();
            for (const std::function<void()>& mark: marks)
                mark();
            const bool accepted = dialog.exec() == QDialog::Accepted;
            this->settingsTab = sections->currentIndex(); // closed, saved or not: reopened on this tab

            std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> changes;
            for (const auto &[setting, read]: fields) {
                const std::string value = read();
                if (changed(setting, value)) changes[{setting.section, setting.key}] = {setting, value};
            }
            if (changes.empty()) return;
            if (!accepted && !this->confirmDangers(changes, this)) { // closed: its dangerous ones asked now
                this->message("not saved", false);
                return;
            }
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
            for (const char* key: {"layout", "style", "vim", "group_by_category"})
                if (changes.contains({"ui", key})) this->manager.setUi(key, changes.at({"ui", key}).second);
            (void)this->manager.config().reload();
            this->bindKeys();
            this->signature.clear();
            this->message(std::to_string(changes.size()) + " setting(s) saved", false);
        }

        void move(const int delta)
        {
            // next / previous: the Tasks session first, then the sessions
            std::vector<std::string> ids = {TASKS_ID};
            for (const cluster::Snapshot& s: this->manager.shown())
                ids.push_back(s.spec.id);
            if (ids.empty()) return;
            auto it = std::find(ids.begin(), ids.end(), this->active);
            int index = it == ids.end() ? 0 : static_cast<int>(it - ids.begin());
            index = (index + delta + static_cast<int>(ids.size())) % static_cast<int>(ids.size());
            this->active = ids[static_cast<std::size_t>(index)];
            this->refresh(true);
        }

        void shortcut(const std::string& action)
        {
            if (action == "palette") this->palette();
            else if (action == "next_session") this->move(1);
            else if (action == "session_left") this->moveDir(-1, 0);
            else if (action == "session_right") this->moveDir(1, 0);
            else if (action == "session_up") this->moveDir(0, -1);
            else if (action == "session_down") this->moveDir(0, 1);
            else if (action == "grid_open") (void)this->gridOpen();
            else if (action == "prev_session") this->move(-1);
            else if (action == "global") {
                this->active = GLOBAL_ID;
                this->refresh(true);
            } else if (action == "new_session") this->choose({"new_session", "New session", "folder [backend]", ".", false});
            else if (action == "close_session") this->run({"close_session", "", "", "", false}, "");
            else if (action == "allow" || action == "deny" || action == "mute") this->run({action, "", "", "", false}, "");
            else if (action == "stats") this->run({"stats", "", "", "", false}, "");
            else if (action == "push_to_talk") this->run({"voice_toggle", "", "", "", false}, "");
            else if (action == "resume") this->run({"resume", "", "", "", false}, "");
            else if (action == "send_next") this->run({"send_next", "", "", "", false}, "");
            else if (action == "layout") {
                const std::string layout = this->manager.ui("layout", this->manager.config().layout);
                this->manager.setUi("layout", layout == "list" ? "grid" : layout == "grid" ? "graph" : layout == "graph" ? "hub" : layout == "hub" ? "tree" : "list");
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
                const std::optional<cluster::Draft> text = this->manager.stashPop(this->active);
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


        bool takeBackUp(void)
        {
            // ↑ on the first line of the prompt (not in the history): the queued prompt back to edit (out of the queue), else the
            // prompt sent but not answered yet (sending, like Claude Code): its turn stopped, the prompt back; the one written stashed
            if (this->historyAt != -1) return false;
            const std::optional<cluster::Snapshot> snap = this->manager.snapshot(this->active);
            if (!snap) return false;
            if (snap->queued > snap->queueGiven) {
                if (const std::optional<cluster::Queued> queued = this->manager.unqueue(this->active)) {
                    if (!cluster::trim(this->input->text().toStdString()).empty()) this->manager.stashPush(this->input->draft());
                    this->input->reset();
                    for (const std::string& image: queued->images)
                        this->input->addImage(image, false);
                    this->input->setText(q_(queued->text));
                    this->message("queued prompt back to edit (Enter: queue it again)", false);
                    return true;
                }
            }
            return false; // a prompt already given to the agent: Esc Esc only, ↑ is the history
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

        void rewind(const bool sent = false)
        {
            // the last turn leaves the conversation (claude started again: in the worker), its prompt comes back; sent: a prompt not
            // answered yet (its turn stopped by it), claude writes it in its transcript a moment after (asked again once)
            const std::string session = this->active;
            this->message("taking the last prompt back…", false);
            this->worker.push([this, session, sent]() {
                std::optional<cluster::Queued> prompt;
                std::string error;
                for (int attempt = 0; !prompt && attempt < (sent ? 2 : 1); ++attempt) {
                    if (attempt > 0) std::this_thread::sleep_for(std::chrono::milliseconds(800));
                    try {
                        prompt = this->manager.rewind(session);
                    } catch (const utils::exception::IException& e) {
                        error = e.info();
                    }
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

        void modeTarget(void)
        {
            // its mode shown by its target: ! a command of the shell (in the folder of the session), : a command of claude-cluster
            const std::string text = this->input->plain();
            const cluster::Style& st = this->theme();
            if (text.starts_with("!")) {
                const std::optional<cluster::Snapshot> here = this->manager.snapshot(this->active);
                this->target->setText(q_("$ shell · " + (here ? cluster::short_path(here->spec.cwd) : std::string("~"))));
                this->target->setStyleSheet("color:" + q_(st.warn) + ";font-weight:600;");
            } else if (text.starts_with(":") && !text.starts_with("::")) {
                this->target->setText(": command (:help)");
                this->target->setStyleSheet("color:" + q_(st.accent) + ";font-weight:600;");
            } else if (this->active == TASKS_ID) {
                this->target->setText("☰ Tasks: no prompt");
                this->target->setStyleSheet("color:" + q_(st.muted) + ";");
            } else {
                this->target->setText(q_(this->active == GLOBAL_ID ? "→ global" : "→ " + this->active));
                this->target->setStyleSheet("");
            }
        }

        void send(void)
        {
            // the prompt leaves at once (feedback), the send / the local command runs in the worker, its result comes back
            // here; an error: the prompt back, nothing lost
            const std::string text = cluster::trim(this->input->plain());
            if (text.empty() || this->active == TASKS_ID) return; // the Tasks session: no prompt
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
                    if (!local && session == TASKS_ID) local = cluster::tasks_prompt(this->manager, text); // a new task / reminder
                    else if (!local) this->manager.send(session, text, false, images);
                    this->manager.addHistory(draft, session); // typed here: in the history (not the ones of the global session / MCP)
                } catch (const utils::exception::IException& e) {
                    error = std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info());
                    const std::optional<cluster::Snapshot> snap = this->manager.snapshot(session);
                    (void)this->manager.fail(text.starts_with("/") ? cluster::one_line(text, 70) : "Send the prompt \"" + cluster::one_line(text, 50) + "\"",
                        snap ? snap->spec.name + " (" + session + ")" : session, error, [this, session, text, images]() {this->manager.send(session, text, false, images);});
                }
                QMetaObject::invokeMethod(this, [this, local, error, draft]() {
                    if (!error.empty()) {
                        this->message(error, true);
                        if (cluster::trim(this->input->text().toStdString()).empty()) this->input->loadDraft(draft);
                        return;
                    }
                    if (local) {
                        if (!local->focus.empty()) this->active = local->focus;
                        if (local->ui == "fill") this->input->loadDraft({local->fill, {}, {}}); // a :s of the last prompt, a :r: in the prompt
                        else if (local->ui == "quit") this->close();
                        else if (local->ui.starts_with("search:")) this->message("search: " + local->ui.substr(7) + " (Ctrl+F in the transcript)", false);
                        if (local->focus == TASKS_ID) QTimer::singleShot(0, this, [this]() {this->showTasks();});
                        if (local->ui.starts_with("term:")) this->openProgram(local->ui.substr(5));
                        else if (local->ui == "message") this->info(local->message);
                        else if (local->ui == "settings") this->settings();
                        else if (local->ui == "plugins") QTimer::singleShot(0, this, [this]() {this->pluginsDialog();});
                        else if (local->ui == "restore_menu") this->openRestore();
                        else if (local->ui == "rewind") this->rewind();
                        else if (local->ui.starts_with("pick:delete:")) this->confirmDelete(local->ui.substr(12)); // asked here
                        else if (local->ui.starts_with("pick:")) this->palette(q_(local->ui.substr(5))); // its choices to pick from
                        if (!local->message.empty()) this->message(local->message, local->error);
                    }
                    this->refresh(true);
                }, Qt::QueuedConnection);
            });
        }

        /* the simple mode */
        void simpleOpen(const std::string& id)
        {
            // a card clicked: its session on the right, the prompt to it
            this->active = id;
            this->simplePanel = true;
            if (this->simpleSide != nullptr) this->simpleSide->setVisible(true);
            this->simpleHint();
            this->refresh(true);
        }

        void simpleClose(void)
        {
            this->active = GLOBAL_ID;
            this->simplePanel = false;
            if (this->simpleSide != nullptr) this->simpleSide->setVisible(false);
            this->simpleHint();
            this->refresh(true);
        }

        void simpleHint(void)
        {
            const std::string lang = cluster::simple_lang(this->manager);
            if (this->input != nullptr) this->input->setPlaceholderText(q_(cluster::simple_word(this->active == GLOBAL_ID ? "prompt_hint" : "tell_it", lang) + "   ("
                + cluster::simple_word("prompt_modes", lang) + ")"));
            if (this->simpleSideName != nullptr) {
                const std::optional<cluster::Snapshot> snap = this->manager.snapshot(this->active);
                this->simpleSideName->setText(snap ? q_(snap->spec.global ? cluster::simple_word("assistant", lang) : snap->spec.name) : QString());
            }
        }

        void refreshSimple(void)
        {
            // the home: hello, the counts (a click filters), what needs you, a card per session, a new task; drawn again when it changed
            const cluster::Json s = cluster::simple_json(this->manager);
            const std::string lang = s.value("lang", std::string("en"));
            const std::optional<cluster::Ask> question = this->manager.asking().current();
            const int columns = std::max(1, (this->simpleHome->viewport()->width() - 24) / 270);
            const std::string key = s.dump() + "|" + this->simpleFilter + "|" + (question ? question->requestId : "") + "|" + std::to_string(columns) + "|" + this->theme().name;
            this->simpleHint();
            if (key == this->simpleKey) return;
            this->simpleKey = key;
            const cluster::Style& st = this->theme();
            const QString soft = "background:" + q_(st.surface) + ";border:1px solid " + q_(st.border) + ";border-radius:16px;";
            QWidget* page = new QWidget();
            QVBoxLayout* col = new QVBoxLayout(page);
            col->setContentsMargins(12, 12, 12, 12);
            col->setSpacing(12);
            // hello
            QLabel* hello = new QLabel(page);
            hello->setTextFormat(Qt::RichText);
            hello->setText("<span style=\"font-size:17pt;font-weight:700\">" + esc_(s.value("greeting", std::string())) + "</span><br>" + span_(st.muted, esc_(s.value("subtitle", std::string()))));
            col->addWidget(hello);
            // the counts
            QHBoxLayout* chips = new QHBoxLayout();
            chips->setSpacing(8);
            const cluster::Json counts = s.value("counts", cluster::Json::object());
            for (const auto &[kind, word]: std::vector<std::pair<std::string, std::string>>{{"working", "chip_working"}, {"needs", "chip_needs"}, {"done", "chip_done"}}) {
                const int n = counts.value(kind, 0);
                QPushButton* chip = new QPushButton(q_(n == 1 ? cluster::simple_word(word + "_one", lang) : cluster::simple_fill(cluster::simple_word(word, lang), {{"n", std::to_string(n)}})), page);
                const std::string color = kind == "working" ? st.ok : kind == "needs" ? st.warn : st.muted;
                chip->setCursor(Qt::PointingHandCursor);
                chip->setStyleSheet("QPushButton{border-radius:14px;padding:6px 14px;font-weight:600;color:" + q_(n ? color : st.muted) + ";border:2px solid "
                    + q_(this->simpleFilter == kind ? color : n ? st.surface : st.border) + ";background:" + q_(n ? st.surface : st.bg) + ";}");
                const std::string chosen = kind;
                QObject::connect(chip, &QPushButton::clicked, [this, chosen]() {
                    this->simpleFilter = this->simpleFilter == chosen ? std::string() : chosen;
                    this->refreshSimple();
                });
                chips->addWidget(chip);
            }
            chips->addStretch(1);
            col->addLayout(chips);
            // what needs you: a permission (Allow / Always / Not now), the question of a helper (Answer: its popup)
            const cluster::Json asks = s.value("asks", cluster::Json::array());
            QLabel* needsHead = new QLabel(q_("<b>" + cluster::simple_word("needs_you", lang) + "</b>"), page);
            col->addWidget(needsHead);
            if (asks.empty() && !question) {
                QLabel* none = new QLabel(q_(cluster::simple_word("nothing_waits", lang)), page);
                none->setStyleSheet("border:1px dashed " + q_(st.border) + ";border-radius:14px;padding:12px;color:" + q_(st.muted) + ";");
                col->addWidget(none);
            }
            const std::function<void(const std::string&, const std::string&, const std::vector<std::pair<std::string, std::function<void()>>>&)> askCard =
                [&](const std::string& what, const std::string& detail, const std::vector<std::pair<std::string, std::function<void()>>>& buttons) {
                    QFrame* card = new QFrame(page);
                    card->setStyleSheet("QFrame{" + soft + "border-left:4px solid " + q_(st.warn) + ";}");
                    QHBoxLayout* row = new QHBoxLayout(card);
                    QLabel* text = new QLabel(card);
                    text->setTextFormat(Qt::RichText);
                    text->setWordWrap(true);
                    text->setStyleSheet("border:0;");
                    text->setText("<b>" + esc_(what) + "</b>" + (detail.empty() ? QString() : "<br>" + span_(st.muted, "<code>" + esc_(detail) + "</code>")));
                    row->addWidget(text, 1);
                    for (std::size_t i = 0; i < buttons.size(); ++i) {
                        QPushButton* b = new QPushButton(q_(buttons[i].first), card);
                        if (i == 0) b->setObjectName("primary");
                        b->setCursor(Qt::PointingHandCursor);
                        QObject::connect(b, &QPushButton::clicked, buttons[i].second);
                        row->addWidget(b);
                    }
                    col->addWidget(card);
                };
            for (const cluster::Json& a: asks) {
                const std::string session = a.value("session", std::string());
                const std::string request = a.value("request_id", std::string());
                const std::string name = a.value("name", std::string());
                std::vector<std::pair<std::string, std::function<void()>>> buttons;
                for (const auto &[behavior, word]: std::vector<std::pair<std::string, std::string>>{{"allow", "allow"}, {"always", "always"}, {"deny", "not_now"}})
                    buttons.push_back({cluster::simple_word(word, lang), [this, session, request, behavior, name, lang]() {
                        try {
                            this->manager.answer(session, request, behavior);
                            this->message(cluster::simple_fill(cluster::simple_word(behavior == "deny" ? "refused" : "allowed", lang), {{"name", name}}), false);
                        } catch (const utils::exception::IException& e) {
                            this->message(e.info(), true);
                        }
                    }});
                askCard(a.value("what", std::string()), a.value("detail", std::string()), buttons);
            }
            if (question) {
                const std::string first = question->questions.is_array() && !question->questions.empty() && question->questions[0].is_object()
                    ? question->questions[0].value("question", std::string()) : "";
                const std::string session = question->session;
                askCard(cluster::simple_fill(cluster::simple_word("ask_question", lang), {{"name", question->name}}), first,
                    {{cluster::simple_word("answer", lang), [this, session]() {this->simpleOpen(session);}}});
            }
            // the work
            const cluster::Json all = s.value("cards", cluster::Json::array());
            cluster::Json cards = cluster::Json::array();
            for (const cluster::Json& c: all)
                if (this->simpleFilter.empty() || c.value("state", std::string()) == this->simpleFilter) cards.push_back(c);
            col->addWidget(new QLabel(q_("<b>" + cluster::simple_word("your_work", lang) + "</b> ") + span_(st.muted, QString::number(cards.size())), page));
            QGridLayout* grid = new QGridLayout();
            grid->setSpacing(12);
            static const std::map<std::string, std::string> states = {{"working", "st_working"}, {"needs", "st_needs"}, {"stuck", "st_stuck"}, {"done", "st_done"},
                {"ready", "st_ready"}, {"starting", "st_starting"}};
            int at = 0;
            for (const cluster::Json& c: cards) {
                const std::string state = c.value("state", std::string());
                const std::string color = state == "working" ? st.ok : state == "needs" ? st.warn : state == "stuck" ? st.error : state == "starting" ? st.accent : st.muted;
                SimpleTile* tile = new SimpleTile(page);
                tile->setObjectName("stile");
                tile->setStyleSheet("#stile{" + soft + "} #stile:hover{border-color:" + q_(st.accent) + ";} QLabel{border:0;background:transparent;}");
                tile->setMinimumHeight(120);
                QVBoxLayout* in = new QVBoxLayout(tile);
                QHBoxLayout* head = new QHBoxLayout();
                head->addWidget(new QLabel("<b>" + esc_(c.value("name", std::string())) + "</b>", tile), 1);
                const auto word = states.find(state);
                QLabel* pill = new QLabel(q_(cluster::simple_word(word != states.end() ? word->second : state, lang)), tile);
                pill->setStyleSheet("color:" + q_(color) + ";font-weight:600;");
                head->addWidget(pill);
                in->addLayout(head);
                QLabel* doing = new QLabel(q_(c.value("doing", std::string())), tile);
                doing->setWordWrap(true);
                doing->setStyleSheet("color:" + q_(st.muted) + ";");
                in->addWidget(doing, 1);
                const int done = c.value("done", 0);
                const int total = c.value("total", 0);
                if (total > 0) {
                    QProgressBar* bar = new QProgressBar(tile);
                    bar->setRange(0, total);
                    bar->setValue(done);
                    bar->setTextVisible(false);
                    bar->setFixedHeight(6);
                    in->addWidget(bar);
                }
                if (const int agents = c.value("agents", 0); agents > 0)
                    in->addWidget(new QLabel(span_(st.accent, "⧉ " + esc_(cluster::simple_fill(cluster::simple_word(agents == 1 ? "agent_one" : "agents_run", lang),
                        {{"n", std::to_string(agents)}, {"t", ""}}))), tile));
                QHBoxLayout* foot = new QHBoxLayout();
                const std::string tag = c.value("tag", std::string());
                foot->addWidget(new QLabel(tag.empty() ? QString() : span_(st.accent, "● ") + esc_(tag), tile), 1);
                const std::string id = c.value("id", std::string());
                const std::string name = c.value("name", std::string());
                if (state == "stuck") {
                    QPushButton* retry = new QPushButton(q_(cluster::simple_word("retry", lang) + " ›"), tile);
                    retry->setCursor(Qt::PointingHandCursor);
                    QObject::connect(retry, &QPushButton::clicked, [this, id, name, lang]() {
                        try {
                            (void)this->manager.resume(id);
                            this->message(cluster::simple_fill(cluster::simple_word("retried", lang), {{"name", name}}), false);
                        } catch (const utils::exception::IException& e) {
                            this->message(e.info(), true);
                        }
                    });
                    foot->addWidget(retry);
                } else {
                    foot->addWidget(new QLabel(span_(st.muted, esc_(total > 0 ? cluster::simple_fill(cluster::simple_word("steps", lang), {{"a", std::to_string(done)},
                        {"b", std::to_string(total)}}) : state == "done" ? c.value("ago", std::string()) : std::string())), tile));
                }
                in->addLayout(foot);
                tile->onClick = [this, id]() {this->simpleOpen(id);};
                grid->addWidget(tile, at / columns, at % columns);
                ++at;
            }
            SimpleTile* add = new SimpleTile(page);
            add->setObjectName("snew");
            add->setStyleSheet("#snew{border:2px dashed " + q_(st.border) + ";border-radius:16px;} #snew:hover{border-color:" + q_(st.accent) + ";} QLabel{border:0;}");
            add->setMinimumHeight(120);
            QVBoxLayout* addIn = new QVBoxLayout(add);
            QLabel* addText = new QLabel(q_(cluster::simple_word("new_task", lang)), add);
            addText->setAlignment(Qt::AlignCenter);
            addText->setStyleSheet("font-weight:600;color:" + q_(st.muted) + ";");
            addIn->addWidget(addText);
            add->onClick = [this]() {this->simpleNewTask();};
            grid->addWidget(add, at / columns, at % columns);
            for (int c = 0; c < columns; ++c)
                grid->setColumnStretch(c, 1);
            col->addLayout(grid);
            col->addStretch(1);
            const int scrolled = this->simpleHome->verticalScrollBar()->value();
            this->simpleHome->setWidget(page); // the old page deleted by the scroll area
            this->simpleHome->verticalScrollBar()->setValue(scrolled);
        }

        // a row of choices (one checked), its value given to change
        QWidget* simpleSeg(QWidget* parent, const std::vector<std::pair<std::string, std::string>>& options, const std::string& value,
            const std::function<void(const std::string&)>& change)
        {
            QWidget* box = new QWidget(parent);
            QHBoxLayout* row = new QHBoxLayout(box);
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(2);
            QButtonGroup* group = new QButtonGroup(box);
            const cluster::Style& st = this->theme();
            box->setStyleSheet("QPushButton{border-radius:10px;padding:5px 12px;} QPushButton:checked{background:" + q_(st.accent) + ";color:" + q_(st.bg)
                + ";font-weight:600;border-color:" + q_(st.accent) + ";}");
            for (const auto &[v, label]: options) {
                QPushButton* b = new QPushButton(q_(label), box);
                b->setCheckable(true);
                b->setChecked(v == value);
                b->setCursor(Qt::PointingHandCursor);
                group->addButton(b);
                const std::string chosen = v;
                QObject::connect(b, &QPushButton::clicked, [change, chosen]() {change(chosen);});
                row->addWidget(b);
            }
            row->addStretch(1);
            return box;
        }

        void simpleSettings(void)
        {
            // the useful settings in 6 pages (reopened on the last one); Save writes them, the full settings one click away
            const std::string lang = cluster::simple_lang(this->manager);
            const cluster::Config& config = this->manager.config();
            const std::vector<cluster::SettingsPage> pages = cluster::settings_pages(config);
            const std::function<const cluster::Setting*(const std::string&, const std::string&)> find = [&pages](const std::string& section, const std::string& key) -> const cluster::Setting* {
                for (const cluster::SettingsPage& page: pages)
                    for (const cluster::Setting& setting: page.settings)
                        if (setting.section == section && setting.key == key) return &setting;
                return nullptr;
            };
            auto changes = std::make_shared<std::map<std::pair<std::string, std::string>, std::string>>();
            const std::function<std::string(const std::string&, const std::string&)> value = [&, changes](const std::string& section, const std::string& key) {
                const auto changed = changes->find({section, key});
                if (changed != changes->end()) return changed->second;
                const cluster::Setting* s = find(section, key);
                return s != nullptr && s->value ? s->value(config) : std::string();
            };
            QDialog dialog(this);
            dialog.setWindowTitle(q_(cluster::simple_word("settings", lang)));
            dialog.resize(760, 540);
            QVBoxLayout* rootLayout = new QVBoxLayout(&dialog);
            QHBoxLayout* split = new QHBoxLayout();
            QListWidget* nav = new QListWidget(&dialog);
            nav->setFixedWidth(190);
            static const std::vector<std::pair<std::string, std::string>> names = {{"general", "set_general"}, {"account", "set_account"}, {"assistant", "set_assistant"},
                {"phone", "set_phone"}, {"keys", "set_keys"}, {"about", "set_about"}};
            for (const auto &[id, word]: names)
                nav->addItem(q_(cluster::simple_word(word, lang)));
            QScrollArea* area = new QScrollArea(&dialog);
            area->setWidgetResizable(true);
            area->setFrameShape(QFrame::NoFrame);
            split->addWidget(nav);
            split->addWidget(area, 1);
            rootLayout->addLayout(split, 1);
            QHBoxLayout* foot = new QHBoxLayout();
            foot->addWidget(new QLabel(span_(this->theme().muted, esc_(cluster::simple_word("reopens", lang))), &dialog), 1);
            QPushButton* close = new QPushButton(q_(cluster::simple_word("close", lang)), &dialog);
            QPushButton* save = new QPushButton(q_(cluster::simple_word("save", lang)), &dialog);
            save->setObjectName("primary");
            foot->addWidget(close);
            foot->addWidget(save);
            rootLayout->addLayout(foot);
            QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
            QObject::connect(save, &QPushButton::clicked, &dialog, &QDialog::accept);
            std::function<void()> draw;
            const std::function<QWidget*(QWidget*, const std::string&, const std::string&, QWidget*)> row = [this](QWidget* parent, const std::string& title, const std::string& help, QWidget* control) {
                QFrame* box = new QFrame(parent);
                box->setStyleSheet("QFrame{border:1px solid " + q_(this->theme().border) + ";border-radius:12px;} QLabel{border:0;}");
                QHBoxLayout* line = new QHBoxLayout(box);
                QLabel* words = new QLabel("<b>" + esc_(title) + "</b>" + (help.empty() ? QString() : "<br>" + span_(this->theme().muted, esc_(help))), box);
                words->setWordWrap(true);
                line->addWidget(words, 1);
                if (control != nullptr) line->addWidget(control);
                return box;
            };
            const std::function<void(const std::string&, const std::string&, const std::string&)> set = [changes](const std::string& section, const std::string& key, const std::string& v) {
                (*changes)[{section, key}] = v;
            };
            draw = [&]() {
                QWidget* body = new QWidget();
                QVBoxLayout* rows = new QVBoxLayout(body);
                rows->setSpacing(10);
                const std::string& page = this->simplePage;
                const std::function<QCheckBox*(const std::string&, const std::string&)> check = [&](const std::string& section, const std::string& key) {
                    QCheckBox* box = new QCheckBox(body);
                    box->setChecked(value(section, key) == "true");
                    QObject::connect(box, &QCheckBox::toggled, [set, section, key](bool on) {set(section, key, on ? "true" : "false");});
                    return box;
                };
                if (page == "general") {
                    rows->addWidget(row(body, cluster::simple_word("set_language", lang), cluster::simple_word("set_language_d", lang),
                        this->simpleSeg(body, {{"en", "English"}, {"fr", "Français"}}, value("ui", "language").empty() ? lang : value("ui", "language"),
                        [set](const std::string& v) {set("ui", "language", v);})));
                    QComboBox* style = new QComboBox(body);
                    if (const cluster::Setting* s = find("ui", "style"); s != nullptr)
                        for (const std::string& c: s->choices)
                            style->addItem(q_(c));
                    style->setCurrentText(q_(value("ui", "style")));
                    QObject::connect(style, &QComboBox::currentTextChanged, [set](const QString& v) {set("ui", "style", v.toStdString());});
                    rows->addWidget(row(body, cluster::simple_word("set_look", lang), cluster::simple_word("set_look_d", lang), style));
                    rows->addWidget(row(body, cluster::simple_word("set_notify", lang), cluster::simple_word("set_notify_d", lang), check("ui", "notify")));
                    rows->addWidget(row(body, cluster::simple_word("set_interface", lang), cluster::simple_word("set_interface_d", lang),
                        this->simpleSeg(body, {{"simple", cluster::simple_word("simple", lang)}, {"classic", cluster::simple_word("classic", lang)}}, "simple",
                        [set](const std::string& v) {set("ui", "mode", v);})));
                } else if (page == "account") {
                    QLineEdit* provider = new QLineEdit(q_(value("agent", "provider")), body);
                    provider->setPlaceholderText("claude");
                    QObject::connect(provider, &QLineEdit::textChanged, [set](const QString& v) {set("agent", "provider", v.trimmed().toStdString());});
                    rows->addWidget(row(body, cluster::simple_word("set_provider", lang), cluster::simple_word("set_provider_d", lang), provider));
                    for (const cluster::SettingsPage& p: pages)
                        for (const cluster::Setting& s: p.settings)
                            if (s.key.starts_with("account_") && !s.key.starts_with("account_rm_")) rows->addWidget(row(body, cluster::trim(s.label), s.value ? s.value(config) : "", nullptr));
                } else if (page == "assistant") {
                    const std::string orch = value("agent", "orchestration").empty() ? std::string("hybrid") : value("agent", "orchestration");
                    QLabel* why = new QLabel(q_(cluster::simple_word("orch_" + orch, lang)), body);
                    why->setWordWrap(true);
                    rows->addWidget(row(body, cluster::simple_word("set_orch", lang), cluster::simple_word("set_orch_d", lang), this->simpleSeg(body,
                        {{"compact", "Compact"}, {"hybrid", "Hybrid"}, {"ultra", "Ultra"}}, orch, [set, why, lang](const std::string& v) {
                            set("agent", "orchestration", v);
                            why->setText(q_(cluster::simple_word("orch_" + v, lang)));
                        })));
                    rows->addWidget(why);
                    rows->addWidget(row(body, cluster::simple_word("set_smart", lang), cluster::simple_word("set_smart_d", lang), this->simpleSeg(body,
                        {{"low", cluster::simple_word("smart_fast", lang)}, {"", cluster::simple_word("smart_balanced", lang)}, {"high", cluster::simple_word("smart_best", lang)}},
                        value("agent", "effort"), [set](const std::string& v) {set("agent", "effort", v);})));
                    rows->addWidget(row(body, cluster::simple_word("set_ask", lang), cluster::simple_word("set_ask_d", lang), this->simpleSeg(body,
                        {{"default", cluster::simple_word("ask_every", lang)}, {"acceptEdits", cluster::simple_word("ask_edits", lang)},
                        {"bypassPermissions", cluster::simple_word("ask_never", lang) + " ⚠"}}, value("agent", "permission_mode").empty() ? std::string("default")
                        : value("agent", "permission_mode"), [set](const std::string& v) {set("agent", "permission_mode", v);})));
                    rows->addWidget(row(body, cluster::simple_word("set_parallel", lang), cluster::simple_word("set_parallel_d", lang), this->simpleSeg(body,
                        {{"2", "2"}, {"4", "4"}, {"8", "8"}}, value("sessions", "max_parallel"), [set](const std::string& v) {set("sessions", "max_parallel", v);})));
                } else if (page == "phone") {
                    const std::optional<cluster::WebStatus> web = cluster::web_status(this->manager);
                    rows->addWidget(row(body, cluster::simple_word("set_web", lang), cluster::simple_word("set_web_d", lang) + (web && web->on ? " · " + web->url + " · code "
                        + web->pin : std::string()), nullptr));
                    rows->addWidget(row(body, cluster::simple_word("set_web_start", lang), cluster::simple_word("set_web_start_d", lang), check("web", "enabled")));
                } else if (page == "keys") {
                    QLabel* help = new QLabel(q_(cluster::simple_word("set_keys_d", lang)), body);
                    help->setWordWrap(true);
                    rows->addWidget(help);
                    std::map<std::string, std::string> keys = config.keys;
                    for (const auto &[k, v]: *changes)
                        if (k.first == "keys") keys[k.second] = v;
                    for (const cluster::SettingsPage& p: pages)
                        for (const cluster::Setting& s: p.settings) {
                            if (s.section != "keys") continue;
                            QLineEdit* field = new QLineEdit(q_(value("keys", s.key)), body);
                            field->setFixedWidth(220);
                            std::string twice;
                            for (const auto &[k, others]: cluster::setting_key_clashes(keys, s.key)) {
                                std::string names;
                                for (const std::string& o: others)
                                    names += (names.empty() ? "" : ", ") + o;
                                twice += (twice.empty() ? "" : " · ") + cluster::simple_fill(cluster::simple_word("keys_twice", lang), {{"key", k}, {"other", names}});
                            }
                            if (!twice.empty()) field->setStyleSheet("border:1px solid " + q_(this->theme().warn) + ";");
                            const std::string id = s.key;
                            QObject::connect(field, &QLineEdit::editingFinished, [set, id, field, &draw]() {
                                set("keys", id, field->text().trimmed().toStdString());
                                QTimer::singleShot(0, [&draw]() {draw();});
                            });
                            std::string words = cluster::trim(s.label);
                            std::replace(words.begin(), words.end(), '_', ' ');
                            rows->addWidget(row(body, words + (twice.empty() ? "" : " !"), twice, field));
                        }
                } else {
                    rows->addWidget(row(body, cluster::simple_word("about_version", lang), "claude-cluster " + std::string(CLUSTER_VERSION), nullptr));
                    QPushButton* docs = new QPushButton(q_(cluster::simple_word("open", lang)), body);
                    QObject::connect(docs, &QPushButton::clicked, [this, &dialog]() {
                        dialog.reject();
                        this->openDocs("");
                    });
                    rows->addWidget(row(body, cluster::simple_word("about_docs", lang), cluster::simple_word("about_docs_d", lang), docs));
                    QPushButton* full = new QPushButton(q_(cluster::simple_word("full_settings", lang)), body);
                    QObject::connect(full, &QPushButton::clicked, [this, &dialog]() {
                        dialog.reject();
                        QTimer::singleShot(0, this, [this]() {this->settings();});
                    });
                    rows->addWidget(row(body, cluster::simple_word("full_settings", lang), "", full));
                }
                rows->addStretch(1);
                area->setWidget(body);
            };
            for (std::size_t i = 0; i < names.size(); ++i)
                if (names[i].first == this->simplePage) nav->setCurrentRow(static_cast<int>(i));
            QObject::connect(nav, &QListWidget::currentRowChanged, [&](int at) {
                if (at < 0 || static_cast<std::size_t>(at) >= names.size()) return;
                this->simplePage = names[static_cast<std::size_t>(at)].first;
                draw();
            });
            draw();
            if (dialog.exec() != QDialog::Accepted) return;
            for (const auto &[where, v]: *changes) {
                const auto &[section, key] = where;
                const cluster::Setting* s = find(section, key);
                if (s != nullptr && s->value && s->value(config) == v) continue;
                if (s != nullptr) {
                    const std::string warning = cluster::setting_danger(*s, v);
                    if (!warning.empty() && QMessageBox::question(this, q_(s->label), q_(warning)) != QMessageBox::Yes) continue;
                }
                try {
                    (void)cluster::settings_set(this->manager.config(), section, key, v);
                    if (section == "ui" && (key == "mode" || key == "language" || key == "style")) this->manager.setUi(key, v);
                } catch (const utils::exception::IException& e) {
                    this->message(e.info(), true);
                    return;
                }
            }
            (void)this->manager.config().reload();
            this->message(cluster::simple_word("saved", lang), false);
            this->signature.clear(); // the language, the mode, the look: drawn again
            this->refresh(true);
        }

        void simpleStats(void)
        {
            // what is left (the 5 hours, the week), today and the 7 days, who used the most; the full stats one click away
            const std::string lang = cluster::simple_lang(this->manager);
            const cluster::Style& st = this->theme();
            this->manager.usageRefresh();
            const cluster::Json usage = this->manager.usageJson("");
            const cluster::Json vendors = usage.is_array() ? usage : usage.is_object() ? cluster::Json::array({usage}) : cluster::Json::array();
            const std::function<std::string(std::int64_t)> fmt = [](std::int64_t n) {
                return n >= 1000000 ? std::to_string(n / 100000 / 10.0).substr(0, std::to_string(n / 100000 / 10.0).find('.') + 2) + "M" : n >= 1000 ? std::to_string(n / 1000) + "k" : std::to_string(n);
            };
            QString html;
            for (const cluster::Json& v: vendors) {
                const std::string vendor = v.value("vendor", std::string());
                for (const auto &[field, word]: std::vector<std::pair<std::string, std::string>>{{"seven_day", "st_week"}, {"five_hour", "st_5h"}}) {
                    if (!v.contains(field) || !v[field].is_object()) continue;
                    const int left = std::max(0, 100 - static_cast<int>(v[field].value("used_percent", 0.0)));
                    html += span_(st.muted, esc_(cluster::simple_word(word, lang) + " · " + vendor)) + "<br><span style=\"font-size:16pt;font-weight:700\">" + QString::number(left) + "%</span><br><br>";
                }
                const cluster::Json today = v.value("today", cluster::Json::object());
                const cluster::Json week = v.value("last_7_days", cluster::Json::object());
                html += span_(st.muted, esc_(cluster::simple_word("st_today", lang) + " · " + vendor)) + "<br><span style=\"font-size:16pt;font-weight:700\">"
                    + q_(fmt(today.value("tokens", std::int64_t{0}))) + "</span> " + span_(st.muted, QString::number(today.value("turns", 0)) + " turns") + "<br><br>";
                html += span_(st.muted, esc_(cluster::simple_word("st_7days", lang) + " · " + vendor)) + "<br><span style=\"font-size:16pt;font-weight:700\">"
                    + q_(fmt(week.value("tokens", std::int64_t{0}))) + "</span><br><br>";
            }
            std::vector<cluster::Snapshot> sessions = this->manager.shown();
            std::erase_if(sessions, [](const cluster::Snapshot& s) {return s.spec.tokens <= 0;});
            std::sort(sessions.begin(), sessions.end(), [](const cluster::Snapshot& a, const cluster::Snapshot& b) {return a.spec.tokens > b.spec.tokens;});
            if (!sessions.empty()) html += "<b>" + esc_(cluster::simple_word("st_top", lang)) + "</b><br>";
            for (std::size_t i = 0; i < sessions.size() && i < 6; ++i)
                html += esc_(sessions[i].spec.global ? cluster::simple_word("assistant", lang) : sessions[i].spec.name) + " &nbsp; " + span_(st.accent,
                    QString(static_cast<int>(std::max<std::int64_t>(1, 24 * sessions[i].spec.tokens / sessions.front().spec.tokens)), QChar(0x2588))) + " "
                    + span_(st.muted, esc_(cluster::simple_fill(cluster::simple_word("st_tokens", lang), {{"n", fmt(sessions[i].spec.tokens)}}))) + "<br>";
            QDialog dialog(this);
            dialog.setWindowTitle(q_(cluster::simple_word("stats", lang)));
            dialog.resize(520, 480);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            QTextBrowser* text = new QTextBrowser(&dialog);
            text->setHtml(html);
            layout->addWidget(text, 1);
            QHBoxLayout* foot = new QHBoxLayout();
            QPushButton* full = new QPushButton(q_(cluster::simple_word("st_full", lang)), &dialog);
            QPushButton* close = new QPushButton(q_(cluster::simple_word("close", lang)), &dialog);
            foot->addWidget(full);
            foot->addStretch(1);
            foot->addWidget(close);
            layout->addLayout(foot);
            QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
            QObject::connect(full, &QPushButton::clicked, [this, &dialog]() {
                dialog.reject();
                this->run({"stats", "", "", "", false}, "");
            });
            dialog.exec();
        }

        void simpleNewTask(void)
        {
            // what to do, in which project (the projects of this computer suggested), how smart: a new helper on it
            const std::string lang = cluster::simple_lang(this->manager);
            QDialog dialog(this);
            dialog.setWindowTitle(q_(cluster::simple_word("nt_title", lang)));
            dialog.resize(520, 420);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            layout->addWidget(new QLabel("<b>" + esc_(cluster::simple_word("nt_what", lang)) + "</b>", &dialog));
            QPlainTextEdit* what = new QPlainTextEdit(&dialog);
            what->setPlaceholderText(q_(cluster::simple_word("nt_hint", lang)));
            layout->addWidget(what, 1);
            layout->addWidget(new QLabel(span_(this->theme().muted, esc_(cluster::simple_word("nt_skills", lang))), &dialog));
            layout->addWidget(new QLabel("<b>" + esc_(cluster::simple_word("nt_where", lang)) + "</b>", &dialog));
            QLineEdit* where = new QLineEdit(&dialog);
            where->setPlaceholderText("~/projects/app");
            QStringList paths;
            for (const cluster::Project& p: cluster::projects(""))
                paths << q_(cluster::short_path(p.path));
            QCompleter* completer = new QCompleter(paths, where);
            completer->setFilterMode(Qt::MatchContains);
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            where->setCompleter(completer);
            layout->addWidget(where);
            layout->addWidget(new QLabel("<b>" + esc_(cluster::simple_word("set_smart", lang)) + "</b>", &dialog));
            std::string effort;
            layout->addWidget(this->simpleSeg(&dialog, {{"low", cluster::simple_word("smart_fast", lang)}, {"", cluster::simple_word("smart_balanced", lang)},
                {"high", cluster::simple_word("smart_best", lang)}}, "", [&effort](const std::string& v) {effort = v;}));
            QHBoxLayout* foot = new QHBoxLayout();
            foot->addStretch(1);
            QPushButton* cancel = new QPushButton(q_(cluster::simple_word("cancel", lang)), &dialog);
            QPushButton* create = new QPushButton(q_(cluster::simple_word("nt_create", lang)), &dialog);
            create->setObjectName("primary");
            foot->addWidget(cancel);
            foot->addWidget(create);
            layout->addLayout(foot);
            QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
            QObject::connect(create, &QPushButton::clicked, [&]() {
                if (what->toPlainText().trimmed().isEmpty()) what->setFocus();
                else if (where->text().trimmed().isEmpty()) where->setFocus();
                else dialog.accept();
            });
            what->setFocus();
            if (dialog.exec() != QDialog::Accepted) return;
            const std::string text = cluster::trim(what->toPlainText().toStdString());
            std::string cwd = cluster::trim(where->text().toStdString());
            if (cwd.starts_with("~/")) cwd = (QDir::homePath().toStdString() + cwd.substr(1));
            std::string name;
            for (const std::string& word: cluster::split(text, ' ')) {
                if (word.empty()) continue;
                name += (name.empty() ? "" : "-") + cluster::lower(word);
                if (std::count(name.begin(), name.end(), '-') >= 2) break;
            }
            std::erase_if(name, [](char c) {return !std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.';});
            name = name.substr(0, 32);
            if (name.empty()) name = "task";
            this->worker.push([this, text, cwd, name, effort, lang]() {
                std::string error;
                try {
                    cluster::SpawnRequest request;
                    request.cwd = cwd;
                    request.name = name;
                    request.prompt = text;
                    request.effort = effort;
                    (void)this->manager.spawn(request);
                } catch (const utils::exception::IException& e) {
                    error = e.info();
                }
                QMetaObject::invokeMethod(this, [this, error, name, lang]() {
                    this->message(error.empty() ? cluster::simple_fill(cluster::simple_word("created", lang), {{"name", name}}) : error, !error.empty());
                    this->refresh(true);
                }, Qt::QueuedConnection);
            });
        }

        void rebuild(void)
        {
            // Central widget for the current layout and set of sessions
            const std::vector<cluster::Snapshot> sessions = this->manager.shown();
            const bool simple = cluster::simple_on(this->manager);
            const std::string layout = simple ? std::string("simple") : this->manager.ui("layout", this->manager.config().layout);
            std::string sig = layout + "|" + cluster::simple_lang(this->manager);
            for (const cluster::Snapshot& s: sessions)
                sig += "|" + s.spec.id;
            if (sig == this->signature) return;
            this->signature = sig;
            this->applyStyle();
            this->views.clear();
            this->list = nullptr;
            this->tabs = nullptr;
            this->tasksView = nullptr;
            this->hub = nullptr;
            this->hubNote = nullptr;
            this->folders = nullptr;
            this->gridArea = nullptr;
            this->simpleHome = nullptr;
            this->simpleSide = nullptr;
            this->simpleSideName = nullptr;
            this->simpleKey.clear();
            if (this->active != TASKS_ID && std::none_of(sessions.begin(), sessions.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->active;})
                && !sessions.empty()) // the Tasks session kept (not a session: it is never in the list)
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
            this->faceShown.clear(); // the title of this new bar drawn at once
            top->addSpacing(14);
            this->webLabel = new QLabel(topbar); // the web access: its address, its code, connected, mean ping (a click: the code copied)
            this->webLabel->setTextFormat(Qt::RichText);
            this->webLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
            this->webLabel->setVisible(false);
            QObject::connect(this->webLabel, &QLabel::linkActivated, this, [this](const QString& link) {this->webLink(link);});
            QFrame* segs = new QFrame(topbar);
            segs->setObjectName("segs");
            segs->setVisible(!simple); // the simple mode: no layouts
            QHBoxLayout* segRow = new QHBoxLayout(segs);
            segRow->setContentsMargins(3, 3, 3, 3);
            segRow->setSpacing(2);
            top->addWidget(segs);
            for (const auto &[name, label]: simple ? std::vector<std::pair<std::string, std::string>>{} : std::vector<std::pair<std::string, std::string>>{{"list", "List"}, {"grid", "Grid"},
                {"graph", "Graph"}, {"hub", "Hub"}, {"tree", "Tree"}}) {
                QPushButton* seg = new QPushButton(q_(label), segs);
                seg->setObjectName("seg");
                seg->setCheckable(true);
                seg->setChecked(layout == name);
                seg->setCursor(Qt::PointingHandCursor);
                const std::string chosen = name;
                QObject::connect(seg, &QPushButton::clicked, [this, chosen]() {
                    this->manager.setUi("layout", chosen);
                    this->refresh(true);
                });
                segRow->addWidget(seg);
            }
            top->addStretch(1);
            const std::string lang = cluster::simple_lang(this->manager);
            // the simple mode: its stats, the docs, its settings only
            const std::vector<std::pair<std::string, std::string>> topActions = simple ? std::vector<std::pair<std::string, std::string>>{{cluster::simple_word("stats", lang), "s_stats"},
                {"?", "docs"}, {cluster::simple_word("settings", lang), "s_settings"}} : std::vector<std::pair<std::string, std::string>>{{"Tasks", "tasks"}, {"Palette", "palette"},
                {"Devices", "devices"}, {"Settings", "settings"}, {"Docs", "docs"}};
            for (const auto &[label, action]: topActions) {
                if (action == "devices" && !cluster::web_status(this->manager)) continue;
                if (action == "docs" && !cluster::docs_built()) continue;
                QPushButton* button = new QPushButton(q_(label), topbar);
                button->setCursor(Qt::PointingHandCursor);
                const std::string name = action;
                QObject::connect(button, &QPushButton::clicked, [this, name]() {
                    if (name == "tasks") this->showTasks();
                    else if (name == "s_stats") this->simpleStats();
                    else if (name == "s_settings") this->simpleSettings();
                    else if (name == "devices") this->webDevices();
                    else if (name == "settings") this->settings();
                    else if (name == "docs") this->openDocs("");
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
            if (simple) {
                // the home (its cards) on the left, the session opened on the right (‹ back to the home)
                QSplitter* splitter = new QSplitter(central);
                this->simpleHome = new QScrollArea(splitter);
                this->simpleHome->setFrameShape(QFrame::NoFrame);
                this->simpleHome->setWidgetResizable(true);
                this->simpleHome->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                this->simpleHome->viewport()->installEventFilter(this); // resized: its columns of cards again
                this->simpleSide = new QWidget(splitter);
                QVBoxLayout* sideLayout = new QVBoxLayout(this->simpleSide);
                sideLayout->setContentsMargins(0, 0, 0, 0);
                QHBoxLayout* sideHead = new QHBoxLayout();
                QPushButton* back = new QPushButton("‹", this->simpleSide);
                back->setToolTip(q_(cluster::simple_word("back", lang)) + " (Esc)");
                back->setCursor(Qt::PointingHandCursor);
                QObject::connect(back, &QPushButton::clicked, [this]() {this->simpleClose();});
                this->simpleSideName = new QLabel(this->simpleSide);
                this->simpleSideName->setStyleSheet("font-weight:600;font-size:12pt;");
                QPushButton* stop = new QPushButton(q_(cluster::simple_word("stop", lang)), this->simpleSide);
                stop->setCursor(Qt::PointingHandCursor);
                QObject::connect(stop, &QPushButton::clicked, [this, lang]() {
                    if (QMessageBox::question(this, q_(cluster::simple_word("stop_ask", lang)), q_(cluster::simple_word("stop_why", lang))) != QMessageBox::Yes) return;
                    try {
                        this->manager.interrupt(this->active);
                    } catch (const utils::exception::IException& e) {
                        this->message(e.info(), true);
                    }
                });
                sideHead->addWidget(back);
                sideHead->addWidget(this->simpleSideName, 1);
                sideHead->addWidget(stop);
                sideLayout->addLayout(sideHead);
                SessionView* view = new SessionView(this, "", false);
                view->plain = true;
                this->views[""] = view;
                sideLayout->addWidget(view, 1);
                this->simpleSide->setVisible(this->simplePanel);
                splitter->setStretchFactor(0, 1);
                splitter->setStretchFactor(1, 1);
                root->addWidget(splitter, 1);
                QShortcut* home = new QShortcut(QKeySequence(Qt::Key_Escape), this->simpleSide, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
                QObject::connect(home, &QShortcut::activated, [this]() {this->simpleClose();});
            } else if (layout == "grid" && !sessions.empty()) {
                // the sessions, then the Tasks as a cell; as many columns as fit (a cell never narrower than GRID_CELL), the rows that do not fit scroll
                this->gridArea = new QScrollArea(central);
                this->gridArea->setFrameShape(QFrame::NoFrame);
                this->gridArea->setWidgetResizable(true);
                this->gridArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                QWidget* cells = new QWidget(this->gridArea);
                cells->setObjectName("gridCells");
                QGridLayout* grid = new QGridLayout(cells);
                grid->setContentsMargins(0, 0, 0, 0);
                const int fit = std::max(1, (this->width() - 40) / GRID_CELL.width());
                this->gridColumns = std::min(fit, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(sessions.size() + 1)))));
                const int columns = this->gridColumns;
                const std::function<void(QWidget*, const std::string&)> cell = [this](QWidget* widget, const std::string& id) {
                    widget->setMinimumSize(GRID_CELL);
                    widget->setProperty("ccCell", q_(id));
                    widget->installEventFilter(this);
                };
                for (std::size_t i = 0; i < sessions.size(); ++i) {
                    SessionView* view = new SessionView(this, sessions[i].spec.id, true);
                    this->views[sessions[i].spec.id] = view;
                    cell(view, sessions[i].spec.id);
                    for (QWidget* part: {static_cast<QWidget*>(view->header), view->log->viewport()}) {
                        part->setProperty("ccCell", q_(sessions[i].spec.id));
                        part->installEventFilter(this);
                    }
                    grid->addWidget(view, static_cast<int>(i) / columns, static_cast<int>(i) % columns);
                }
                this->tasksView = this->makeTasksView(cells);
                cell(this->tasksView, TASKS_ID);
                this->tasksView->viewport()->setProperty("ccCell", q_(TASKS_ID));
                this->tasksView->viewport()->installEventFilter(this);
                grid->addWidget(this->tasksView, static_cast<int>(sessions.size()) / columns, static_cast<int>(sessions.size()) % columns);
                for (int r = 0; r < grid->rowCount(); ++r)
                    grid->setRowStretch(r, 1);
                this->gridArea->setWidget(cells);
                root->addWidget(this->gridArea, 1);
            } else if (layout == "hub" || layout == "graph") {
                // the graph (the categories / tags between global and the sessions) or the hub (every session around global); a node clicked: its session
                // shown in the list layout
                this->hub = new HubView(central);
                this->hub->ring = layout == "hub";
                this->hub->onOpen = [this](const std::string& id) {
                    this->active = id;
                    this->manager.setUi("layout", "list");
                    this->refresh(true);
                };
                this->hub->onNode = [this](const std::string& node) {   // a node of the taxonomy: the list filtered to what is below it (again: all)
                    this->labelFilter = this->labelFilter == node ? std::string() : node;
                    this->refresh(true);
                };
                this->hub->onMenu = [this](const std::string& node, const QPoint& at) {this->hubMenu(node, at);};
                this->hub->onRedraw = [this]() {this->refresh(true);};
                this->hub->onClose = [this]() {
                    this->manager.setUi("layout", "list");
                    this->refresh(true);
                };
                this->hub->keys = this->manager.config().keys;
                root->addWidget(this->hub, 1);
                this->hubNote = new QLabel(central);
                this->hubNote->setAlignment(Qt::AlignCenter);
                this->hubNote->setWordWrap(true);
                root->addWidget(this->hubNote);
                QTimer::singleShot(0, this->hub, [hub = this->hub]() {hub->whole();});
            } else if (layout == "tree") {
                // the sessions by project folder; a session double clicked: shown in the list layout
                this->folders = new QTreeWidget(central);
                this->folders->setHeaderLabels({"Session", "State", "Labels", "What it does"});
                this->folders->setRootIsDecorated(true);
                this->folders->setUniformRowHeights(true);
                QObject::connect(this->folders, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) {   // double click / Enter
                    const std::string id = item->data(0, Qt::UserRole).toString().toStdString();
                    if (id.empty()) return;
                    this->active = id;
                    this->manager.setUi("layout", "list");
                    this->refresh(true);
                });
                root->addWidget(this->folders, 1);
            } else if (layout == "tabs") {
                this->tabs = new QTabBar(central);
                this->tabs->setContextMenuPolicy(Qt::CustomContextMenu);
                QObject::connect(this->tabs, &QTabBar::customContextMenuRequested, this, [this](const QPoint& pos) {
                    const int index = this->tabs->tabAt(pos) - 1; // the Tasks session first
                    const std::vector<cluster::Snapshot> all = this->manager.shown();
                    if (index >= 0 && static_cast<std::size_t>(index) < all.size()) this->sessionMenu(all[static_cast<std::size_t>(index)].spec.id, this->tabs->mapToGlobal(pos));
                });
                this->tabs->addTab("☰ Tasks");
                for (const cluster::Snapshot& s: sessions)
                    this->tabs->addTab(q_(s.spec.name));
                QObject::connect(this->tabs, &QTabBar::currentChanged, [this](int index) {
                    const std::vector<cluster::Snapshot> all = this->manager.shown();
                    if (index == 0) this->active = TASKS_ID;
                    else if (index > 0 && static_cast<std::size_t>(index - 1) < all.size()) this->active = all[static_cast<std::size_t>(index - 1)].spec.id;
                    this->refresh(true);
                });
                root->addWidget(this->tabs);
                SessionView* view = new SessionView(this, "", false);
                this->views[""] = view;
                root->addWidget(view, 1);
                this->tasksView = this->makeTasksView(central);
                root->addWidget(this->tasksView, 1);
            } else {
                QSplitter* splitter = new QSplitter(central);
                this->list = new QListWidget(splitter);
                this->list->setContextMenuPolicy(Qt::CustomContextMenu); // a session right-clicked: its menu
                this->list->setCursor(Qt::PointingHandCursor);
                QObject::connect(this->list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
                    const int row = this->list->indexAt(pos).row() - 1; // the Tasks session first
                    const std::vector<cluster::Snapshot> all = this->manager.shown();
                    if (row >= 0 && static_cast<std::size_t>(row) < all.size()) this->sessionMenu(all[static_cast<std::size_t>(row)].spec.id, this->list->viewport()->mapToGlobal(pos));
                });
                this->list->setMinimumWidth(220);
                this->list->viewport()->installEventFilter(this); // resized: its rows elided again
                this->list->setWordWrap(true);
                this->list->setTextElideMode(Qt::ElideMiddle);
                this->list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                QObject::connect(this->list, &QListWidget::currentRowChanged, [this](int row) {
                    const std::vector<cluster::Snapshot> all = this->manager.shown();
                    const std::string chosen = row == 0 ? std::string(TASKS_ID) : row > 0 && static_cast<std::size_t>(row - 1) < all.size() ? all[static_cast<std::size_t>(row - 1)].spec.id : "";
                    if (!chosen.empty() && chosen != this->active) {
                        this->active = chosen;
                        this->refresh(true);
                    }
                });
                SessionView* view = new SessionView(this, "", false);
                this->views[""] = view;
                splitter->addWidget(this->list);
                QWidget* side = new QWidget(splitter); // the session shown, or the Tasks session
                QVBoxLayout* sideLayout = new QVBoxLayout(side);
                sideLayout->setContentsMargins(0, 0, 0, 0);
                sideLayout->addWidget(view, 1);
                this->tasksView = this->makeTasksView(side);
                sideLayout->addWidget(this->tasksView, 1);
                splitter->addWidget(side);
                splitter->setStretchFactor(1, 1);
                root->addWidget(splitter, 1);
            }
            this->voiceLabel = new QLabel(central);
            this->voiceLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
            QObject::connect(this->voiceLabel, &QLabel::linkActivated, [this](const QString& link) {this->webLink(link);});
            root->addWidget(this->voiceLabel);
            QHBoxLayout* bar = new QHBoxLayout();
            this->input = new PromptEdit(this, central);
            QObject::connect(this->input, &QPlainTextEdit::textChanged, this, [this]() {if (this->target) this->modeTarget();}); // its mode (! / :) shown
            {
                // the prompt not sent yet: saved a moment after its last change (and at the close), given back at the next start
                QTimer* keep = new QTimer(this);
                keep->setSingleShot(true);
                keep->setInterval(700);
                QObject::connect(keep, &QTimer::timeout, this, [this]() {this->manager.draftSave("gui", this->input->draft());});
                QObject::connect(this->input, &QPlainTextEdit::textChanged, keep, [keep]() {keep->start();});
                QTimer::singleShot(0, this, [this]() { // once the prompt and its chips exist
                    if (!this->input->toPlainText().trimmed().isEmpty()) return;
                    if (const std::optional<cluster::Draft> kept = this->manager.draftLoad("gui")) {
                        this->input->loadDraft(*kept);
                        this->message("your unsent prompt is back", false);
                    }
                });
            }
            this->input->setPlaceholderText(simple ? q_(cluster::simple_word(this->active == GLOBAL_ID ? "prompt_hint" : "tell_it", lang) + "   (" + cluster::simple_word("prompt_modes", lang) + ")")
                : QString("Prompt (Enter to send, Shift / Alt+Enter new line) · @ file · / command · ! shell · : command · Ctrl+K palette · F1 keys"));
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
            QPushButton* sendButton = new QPushButton(simple ? q_(cluster::simple_word("send", lang)) : QString("Send"), central);
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
            QToolButton* clip = new QToolButton(central);
            clip->setText("📎");
            clip->setToolTip("Attach files, images (or drop / paste them)");
            clip->setCursor(Qt::PointingHandCursor);
            clip->setAutoRaise(true);
            QObject::connect(clip, &QToolButton::clicked, [this, clip]() {
                QMenu menu(clip);
                const bool link = this->manager.config().attach.link;
                menu.addAction("Files…");
                QAction* images = menu.addAction("Images…");
                QAction* linked = link ? menu.addAction("Link files (no copy)…") : nullptr;
                QAction* chosen = menu.exec(clip->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
                if (!chosen) return;
                const QString filter = chosen == images ? "Images (*.png *.jpg *.jpeg *.gif *.webp *.bmp *.heic);;All files (*)" : "All files (*)";
                const QStringList paths = QFileDialog::getOpenFileNames(this, chosen == linked ? "Link files" : "Attach files", QDir::homePath(), filter);
                if (!paths.isEmpty()) this->input->attachFiles(paths, chosen == linked ? 2 : 1);
                this->input->setFocus();
            });
            bar->addWidget(clip);
            bar->addWidget(this->input, 1);
            bar->addWidget(this->input->vimLabel);
            bar->addWidget(this->target);
            bar->addSpacing(6);
            bar->addWidget(sendButton);
            root->addWidget(composer);
            this->status = new QLabel(central);
            this->status->setObjectName("status");
            this->status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // a long one cut, the web access kept
            // the bottom line: the status / the last notice on the left, the web access on the right
            QHBoxLayout* bottom = new QHBoxLayout();
            bottom->setContentsMargins(0, 0, 14, 0);
            bottom->addWidget(this->status, 1);
            bottom->addWidget(this->webLabel);
            root->addLayout(bottom);
            this->setCentralWidget(central);
            this->showFace(); // its title at once (not at the next tick of the face: blank meanwhile)
            this->input->setFocus();
            if (this->hub) this->hub->setFocus(); // its keys at once (a click on the prompt to write)
            else if (this->folders) this->folders->setFocus();
            this->lastVersion = 0;
            this->faceShown.clear(); // the new top bar gets its face
        }

        void refresh(const bool force)
        {
            this->rebuild();
            const std::uint64_t version = this->manager.version() + this->app.voice->version();
            if (!force && version == this->lastVersion && !this->animating) return;
            this->lastVersion = version;
            const std::vector<cluster::Snapshot> sessions = this->manager.shown();
            const cluster::Style& st = this->theme();
            if (this->simpleHome != nullptr) this->refreshSimple();

            if (this->folders != nullptr) {
                // the tree: a folder, its sessions (their state, labels and what they do), kept open as it was
                std::map<std::string, std::vector<const cluster::Snapshot*>> byFolder;
                for (const cluster::Snapshot& s: sessions)
                    byFolder[cluster::short_path(s.spec.cwd)].push_back(&s);
                std::set<QString> closed;
                for (int i = 0; i < this->folders->topLevelItemCount(); ++i)
                    if (!this->folders->topLevelItem(i)->isExpanded()) closed.insert(this->folders->topLevelItem(i)->text(0));
                const QTreeWidgetItem* was = this->folders->currentItem();
                const QString chosen = was == nullptr ? QString() : was->data(0, Qt::UserRole).toString().isEmpty() ? "folder:" + was->text(0) : was->data(0, Qt::UserRole).toString();
                this->folders->clear();
                for (const auto &[folder, members]: byFolder) {
                    QTreeWidgetItem* top = new QTreeWidgetItem(this->folders, {q_(folder)});
                    top->setForeground(0, QColor(q_(st.accent)));
                    for (const cluster::Snapshot* s: members) {
                        std::string labels = s->spec.category;
                        for (const std::string& tag: s->spec.tags)
                            labels += (labels.empty() ? "#" : " #") + tag;
                        QTreeWidgetItem* row = new QTreeWidgetItem(top, {q_(s->spec.global ? "global" : s->spec.name), q_(!s->permissions.empty() ? "waiting for you"
                            : cluster::background_busy(*s) ? "~_~ background" : cluster::state_name(s->state)), q_(labels), q_(cluster::one_line(s->spec.summary, 80))});
                        row->setData(0, Qt::UserRole, q_(s->spec.id));
                        if (chosen == q_(s->spec.id)) this->folders->setCurrentItem(row);
                        row->setForeground(1, QColor(q_(!s->permissions.empty() ? st.warn : s->state == cluster::State::Working ? st.ok : s->state == cluster::State::Error
                            ? st.error : cluster::background_busy(*s) ? busy_color_(st) : st.muted)));
                    }
                    top->setExpanded(!closed.contains(q_(folder)));
                    if (chosen == "folder:" + q_(folder)) this->folders->setCurrentItem(top);
                }
                this->folders->resizeColumnToContents(0);
                this->folders->resizeColumnToContents(1);
            }
            if (this->hub != nullptr) {
                this->hub->filter = this->labelFilter;
                this->hub->populate(sessions, st, this->active, this->manager.taxonomy(), this->hub->archived ? this->manager.trash() : std::vector<cluster::SessionSpec>());
                const bool global = std::any_of(sessions.begin(), sessions.end(), [](const cluster::Snapshot& s) {return s.spec.id == GLOBAL_ID;});
                QString note;
                if (sessions.size() <= (global ? 1U : 0U)) note += "No sub-session yet: ask the global session, or create one (Ctrl+T). ";
                if (!global) note += "No global session (settings › Global session). ";
                this->hubNote->setText(esc_(note.toStdString()) + span_(st.muted, "drag · wheel · z q s d / arrows · e / a zoom · 0 whole · Tab: a node · Enter: open · F: its tree · Esc: the list"));
            }
            if (this->list) {
                const QSignalBlocker block(this->list);
                this->list->clear();
                {
                    // the Tasks session: first, what runs / waits / is scheduled
                    const std::vector<cluster::Task> tasks = this->manager.tasks();
                    const std::vector<cluster::Schedule> planned = this->manager.schedules();
                    const auto running = std::count_if(tasks.begin(), tasks.end(), [](const cluster::Task& t) {return t.status == "running";});
                    const auto queued = std::count_if(tasks.begin(), tasks.end(), [](const cluster::Task& t) {return t.status == "queued";});
                    const auto waiting = std::count_if(planned.begin(), planned.end(), [](const cluster::Schedule& w) {return w.status == "waiting";});
                    QLabel* row = new QLabel(this->list);
                    row->setTextFormat(Qt::RichText);
                    row->setAttribute(Qt::WA_TransparentForMouseEvents);
                    row->setContentsMargins(10, 7, 10, 7);
                    row->setText(span_(st.accent, "☰") + "&nbsp; <b>Tasks</b>" + (running > 0 ? "&nbsp; " + span_(st.ok, QString::number(running) + " ▶") : QString()) + "<br>"
                        + span_(st.muted, "<span style=\"font-family:monospace;font-size:8.5pt\">" + QString::number(queued) + " waiting · " + QString::number(waiting) + " scheduled</span>"));
                    QListWidgetItem* item = new QListWidgetItem(this->list);
                    item->setToolTip("the todo lists of the sessions, the task queue, the schedules");
                    item->setSizeHint(QSize(0, row->sizeHint().height() + 4));
                    this->list->setItemWidget(item, row);
                    if (this->active == TASKS_ID) this->list->setCurrentRow(0);
                }
                const bool grouped = this->manager.ui("group_by_category", this->manager.config().groupByCategory ? "true" : "false") == "true";
                const cluster::Taxonomy taxonomy = this->manager.taxonomy();
                const cluster::Snapshot* before = nullptr;
                this->listWidth = this->list->viewport()->width();
                const int room = std::max(120, this->listWidth - 26);
                QFont small = QFontDatabase::systemFont(QFontDatabase::FixedFont);
                small.setPointSizeF(8.5);
                QFont bold = this->list->font();
                bold.setBold(true);
                const QFontMetrics smallMetrics(small);
                const QFontMetrics boldMetrics(bold);
                for (const cluster::Snapshot& s: sessions) {
                    // grouped: the category over the first session of each group; its chips (category, tags) under its description
                    if (!this->labelFilter.empty() && !s.spec.global && !taxonomy.under(s.spec, this->labelFilter, true)) continue;   // a node chosen in the hub
                    QString header;
                    if (grouped && !s.spec.global) {
                        // the heads of the taxonomy: the levels of its path the previous session did not have, nested (none: "no category")
                        const std::vector<std::string> path = taxonomy.group(s.spec);
                        const std::vector<std::string> beforePath = before == nullptr || before->spec.global ? std::vector<std::string>{"\x01"} : taxonomy.group(before->spec);
                        if (path != beforePath)
                            for (std::size_t depth = path.empty() ? 0: cluster::Taxonomy::common_prefix(beforePath, path); depth < std::max<std::size_t>(path.size(), 1); ++depth) {
                                const std::string node = depth < path.size() ? path[depth] : std::string();
                                header += "<div style=\"margin-left:" + QString::number(static_cast<int>(depth) * 12) + "px\">" + span_(node.empty() ? st.muted : chip_color_(node),
                                    "<b>▾ " + esc_(node.empty() ? std::string("no category") : node) + "</b>") + "</div>";
                            }
                    }
                    before = &s;
                    QString chips;
                    if (!s.spec.category.empty()) chips += "<br>" + pill_(s.spec.category, st.bg, chip_color_(s.spec.category), false, true, 8.2);
                    for (const std::string& tag: s.spec.tags)
                        chips += (chips.isEmpty() ? "<br>" : " ") + pill_("#" + tag, chip_color_(tag), st.bg, false, false, 8.2, true);
                    const std::string dot = s.state == cluster::State::Working ? st.ok : s.state == cluster::State::Waiting || !s.permissions.empty() ? st.warn
                        : s.state == cluster::State::Error ? st.error : cluster::background_busy(s) ? busy_color_(st) : st.muted;
                    const char* face = !s.permissions.empty() ? "•_•" : s.state == cluster::State::Working ? ">_<" : s.state == cluster::State::Error ? "o_o" : cluster::background_busy(s) ? "~_~" : "-_-";
                    QLabel* row = new QLabel(this->list);
                    row->setTextFormat(Qt::RichText);
                    row->setAttribute(Qt::WA_TransparentForMouseEvents);
                    row->setContentsMargins(10, 7, 10, 7);
                    row->setWordWrap(true);
                    const QString name = boldMetrics.elidedText(q_(s.spec.name), Qt::ElideRight, std::max(40, room - 72));
                    const QString what = smallMetrics.elidedText(q_(s.spec.summary.empty() ? s.spec.backend.substr(0, s.spec.backend.find('/')) + " · " + cluster::short_path(s.spec.cwd)
                        : cluster::one_line(s.spec.summary, 400)), Qt::ElideRight, room);
                    row->setText("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td>" + span_(dot, "●") + "&nbsp; <b>" + name.toHtmlEscaped() + "</b>&nbsp; "
                        + pill_(face, dot, mix_(st.surface, dot, 0.22))
                        + (cluster::keep_warm_on(this->manager.config(), s) ? "&nbsp;" + span_(st.warn, "♨") : QString()) // kept warm
                        + (s.permissions.empty() ? QString() : "&nbsp;" + span_(st.warn, "<b>" + QString::number(s.permissions.size()) + "</b>"))
                        + "</td><td align=\"right\">" + span_(st.muted, "<span style=\"font-family:monospace;font-size:9pt\">" + esc_(cluster::human_tokens(s.metrics.total.total()))
                        + "</span>") + "</td></tr></table>" + span_(st.muted, "<span style=\"font-family:monospace;font-size:8.5pt;white-space:pre\">" + what.toHtmlEscaped()
                        + "</span>") + chips);
                    if (!header.isEmpty()) row->setText(header + row->text());
                    QListWidgetItem* item = new QListWidgetItem(this->list);
                    item->setToolTip(q_((s.spec.summary.empty() ? std::string() : s.spec.summary + "\n") + cluster::short_path(s.spec.cwd)
                        + (s.spec.category.empty() && s.spec.tags.empty() ? std::string() : "\n" + cluster::label_text(s.spec))));
                    item->setSizeHint(QSize(0, row->heightForWidth(this->listWidth) + 4));
                    this->list->setItemWidget(item, row);
                    if (s.spec.id == this->active) this->list->setCurrentRow(this->list->count() - 1);
                }
            }
            if (this->tabs) {
                const QSignalBlocker block(this->tabs);
                if (this->active == TASKS_ID) this->tabs->setCurrentIndex(0);
                for (std::size_t i = 0; i < sessions.size() && static_cast<int>(i) + 1 < this->tabs->count(); ++i) {
                    this->tabs->setTabText(static_cast<int>(i) + 1, q_(sessions[i].spec.name + (sessions[i].permissions.empty() ? "" : " !")));
                    if (sessions[i].spec.id == this->active) this->tabs->setCurrentIndex(static_cast<int>(i) + 1);
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
            this->showProgress();
            this->showFailure();
            // the Tasks session shown: its view in place of the session
            const bool gridCell = this->manager.ui("layout", this->manager.config().layout) == "grid";   // the grid: the Tasks always a cell of it
            const bool tasksShown = (this->active == TASKS_ID || gridCell) && this->tasksView != nullptr;
            if (this->views.contains("") && this->tasksView != nullptr) this->views[""]->setVisible(!tasksShown);
            if (this->tasksView != nullptr) {
                if (tasksShown && !gridCell && !this->tasksView->isVisible()) QTimer::singleShot(0, this->tasksView, [view = this->tasksView]() {view->setFocus();}); // its keys at once
                this->tasksView->setVisible(tasksShown);
                if (tasksShown) this->fillTasks(this->tasksView);
            }
            this->modeTarget();
            // the Tasks session: no prompt
            if (this->input != nullptr) {
                const bool off = this->active == TASKS_ID;
                if (this->input->isEnabled() == off) {
                    this->input->setEnabled(!off);
                    this->input->setPlaceholderText(off ? "Tasks: no prompt here · a click on a row: its actions · Ctrl+N / Ctrl+P: another session"
                        : "Prompt (Enter to send, Shift / Alt+Enter new line) · @ file · / command · ! shell · : command · Ctrl+K palette · F1 keys");
                }
            }

            // voice bar, last notice
            const cluster::Voice& voice = *this->app.voice;
            const cluster::VoiceConf& conf = this->manager.config().voice;
            QString voiceText;
            if (voice.listening()) voiceText = span_(st.ok, conf.mode == "wake" ? (voice.armed() ? "◉ armed: say the command" : q_("○ waiting for \"" + conf.wakeWord + "\"")) : "◉ listening", true);
            if (voice.listening() && !voice.ready()) voiceText = span_(st.warn, "… loading the speech model (~10 s)", true);
            const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            int waiting = 0;
            for (const cluster::Pending& p: voice.pending()) {
                if (p.target.empty()) { // live: what is being said, its end
                    voiceText += "  <i>" + span_(st.muted, esc_("🎙 " + cluster::tail_utf8(p.text, 160))) + "</i>";
                    continue;
                }
                ++waiting;
                const std::string when = p.paused ? " (paused while you speak)" : p.deadline > 0 ? " (sent in " + std::to_string(std::max<std::int64_t>(0, (p.deadline - now + 999) / 1000)) + "s)"
                    : " (F5 to end)";
                voiceText += "  " + span_(st.accent, esc_("→ " + (p.target == GLOBAL_ID ? std::string("global") : p.target) + ":")) + " " + span_(st.text, esc_("\"" + p.text + "\""), true)
                    + span_(p.paused ? st.warn : st.muted, esc_(when));
            }
            if (waiting > 0) // its validation: clicked, or Escape (the last one) / Ctrl+C (all)
                voiceText += "&nbsp;&nbsp;<a href=\"voice-send\" style=\"text-decoration:none\">" + span_(st.ok, "✓ Send now", true) + "</a>&nbsp;&nbsp;"
                    "<a href=\"voice-drop\" style=\"text-decoration:none\">" + span_(st.error, "✗ Drop", true) + "</a>&nbsp;&nbsp;"
                    "<a href=\"voice-edit\" style=\"text-decoration:none\">" + span_(st.accent, "✎ Correct", true) + "</a>&nbsp;&nbsp;" + span_(st.muted, "Escape: drop");
            if (voice.speaking()) voiceText += "  " + span_(st.muted, "speaking (" + q_(conf.wakeWord.empty() ? "" : "") + q_(this->manager.config().keys.at("mute")) + " mute)");
            // the web access: its address and code shown while it is open
            const std::optional<cluster::WebStatus> webState = cluster::web_status(this->manager);
            const std::string web = webState ? cluster::web_line(*webState) : "";
            if (this->webLabel != nullptr) {
                this->webLabel->setText(web.empty() ? QString() : "<a href=\"web-copy\" style=\"text-decoration:none\" title=\"copy the code\">" + span_(st.accent, esc_(web), true)
                    + "</a>&nbsp;&nbsp;<a href=\"web-devices\" style=\"text-decoration:none\">" + span_(st.muted, "devices ›") + "</a>");
                this->webLabel->setVisible(!web.empty());
            }
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
            const int picked = box.exec();
            if (cluster::quit_requested()) return; // closed by a quit: not answered (asked again at the next start)
            if (picked != QMessageBox::Yes) {
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
                if (cluster::quit_requested()) return;
                if (ok) (void)cluster::settings_set(this->manager.config(), "user", q.key, answer.trimmed().toStdString());
            }
            (void)this->manager.config().reload();
            this->manager.scanMachine();
            this->message("mapping this computer…", false);
        }

        void settingAction(const std::string& key)
        {
            // an action of the settings: the map of this computer, the system prompt of the global session or of the
            // sub-sessions (edited here), the showcase of the Debug page (in a thread: the window never frozen)
            if (key.starts_with("plugsrc_")) { // a source of Settings > Plugins: its page in the Plugins browser (or its field to add one)
                const std::string open = key == "plugsrc_add" ? std::string("+add") : key.substr(8);
                QTimer::singleShot(0, this, [this, open]() {this->pluginsDialog(open);});
                return;
            }
            if (key.starts_with("account_")) {
                // a provider: its key asked (without echo) and stored, removed, or its login run
                const bool remove = key.starts_with("account_rm_");
                const std::string name = key.substr(remove ? 11 : 8);
                const std::optional<cluster::Provider> provider = this->manager.auth().find(name);
                if (!provider) return;
                if (remove) this->run({"auth_logout:" + name, "", "", "", false}, "");
                else if (provider->auth == cluster::AuthKind::Key) this->choose({"auth_key:" + name, "Provider " + name + ": store the API key", "API key", "", false});
                else this->run({"auth_login:" + name, "", "", "", false}, "");
                return;
            }
            if (key.starts_with("showcase_")) {
                std::thread([this, what = key.substr(9)]() {
                    std::string done;
                    bool error = false;
                    try {
                        done = cluster::showcase(this->manager, what, "window");
                    } catch (const utils::exception::IException& e) {
                        done = e.info();
                        error = true;
                    }
                    QMetaObject::invokeMethod(this, [this, done, error]() {this->message(done, error);}, Qt::QueuedConnection);
                }).detach();
                return;
            }
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
            const bool show = this->active != TASKS_ID && (this->manager.config().questionsToGlobal ? this->active == GLOBAL_ID || ask->session == GLOBAL_ID
                : this->active == ask->session); // never over the Tasks session
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
            struct EnterTicks: QObject {
                bool eventFilter(QObject* watched, QEvent* event) override
                {
                    QKeyEvent* key = event->type() == QEvent::KeyPress ? static_cast<QKeyEvent*>(event) : nullptr;
                    QAbstractButton* button = qobject_cast<QAbstractButton*>(watched);
                    if (!key || !button || (key->key() != Qt::Key_Return && key->key() != Qt::Key_Enter)) return false;
                    button->toggle();
                    return true;
                }
            } enterTicks;
            QDialog dialog(this);
            dialog.setWindowTitle(q_(ask.name + " asks"));
            dialog.setMinimumWidth(620);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            std::vector<Row> rows;
            bool syncing = false;
            std::size_t lastIndex = ask.index; // the cursor of the model followed only when it moves (the voice), never as a default answer
            int lastCursor = ask.cursor;
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
                    if (multi) button->installEventFilter(&enterTicks); // Enter ticks / unticks (never the Answer button)
                    layout->addWidget(button);
                    row.options.push_back(button);
                }
                // an option with a preview (Claude Desktop): a box under the options showing the one chosen or pointed at
                std::vector<std::string> previews;
                for (const cluster::Json& option: q.value("options", cluster::Json::array()))
                    previews.push_back(option.value("preview", std::string()));
                if (std::any_of(previews.begin(), previews.end(), [](const std::string& p) {return !p.empty();})) {
                    QTextBrowser* previewBox = new QTextBrowser(&dialog);
                    previewBox->setMinimumHeight(190);
                    previewBox->setOpenLinks(false);
                    const cluster::Style style = this->theme();
                    const std::function<void(std::size_t)> show = [previewBox, previews, style](const std::size_t at) {
                        previewBox->setHtml(at < previews.size() && !previews[at].empty() ? preview_html_(previews[at], style) : QString("<i style=\"color:gray\">(no preview for this option)</i>"));
                    };
                    show(0);
                    for (std::size_t i = 0; i < row.options.size(); ++i) {
                        QObject::connect(row.options[i], &QAbstractButton::toggled, previewBox, [show, i](const bool on) {if (on) show(i);});
                        QObject::connect(row.options[i], &QAbstractButton::pressed, previewBox, [show, i]() {show(i);});
                    }
                    layout->addWidget(previewBox);
                }
                QHBoxLayout* other = new QHBoxLayout();
                row.other = multi ? static_cast<QAbstractButton*>(new QCheckBox("Other:", &dialog)) : new QRadioButton("Other:", &dialog);
                row.text = new QLineEdit(&dialog);
                group->addButton(row.other);
                QObject::connect(row.text, &QLineEdit::textEdited, row.other, [button = row.other]() {button->setChecked(true);});
                other->addWidget(row.other);
                other->addWidget(row.text, 1);
                layout->addLayout(other);
                // one choice: none ticked until the user picks one (the cursor of the model is never an answer)
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
            QLabel* voice = new QLabel(this->manager.config().voice.enabled ? "voice: a number or an option · valide / suivant · retour · autre <answer> · la globale · annule" : "", &dialog);
            voice->setStyleSheet("color: gray;");
            layout->addWidget(voice);
            QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            buttons->button(QDialogButtonBox::Ok)->setText("Answer");
            buttons->button(QDialogButtonBox::Cancel)->setText("Decline");
            // "Chat about this": a side chat about the questions, under them (the session untouched)
            QWidget* chatBox = new QWidget(&dialog);
            QVBoxLayout* chatLayout = new QVBoxLayout(chatBox);
            chatLayout->setContentsMargins(0, 6, 0, 0);
            QTextBrowser* chatLog = new QTextBrowser(chatBox);
            chatLog->setMinimumHeight(140);
            QLineEdit* chatInput = new QLineEdit(chatBox);
            chatInput->setPlaceholderText("Ask about these questions (Enter)");
            chatLayout->addWidget(chatLog);
            chatLayout->addWidget(chatInput);
            chatBox->hide();
            layout->insertWidget(layout->count() - 1, chatBox);
            QObject::connect(chatInput, &QLineEdit::returnPressed, &dialog, [this, chatInput]() {
                const std::string done = this->manager.asking().chat(chatInput->text().toStdString());
                if (done.empty()) chatInput->clear();
                else this->message(done, true);
            });
            if (cluster::docs_built()) {
                QPushButton* help = buttons->addButton("Help", QDialogButtonBox::HelpRole);
                QObject::connect(help, &QPushButton::clicked, &dialog, [this]() {this->openDocs("question");});
            }
            QPushButton* chatButton = buttons->addButton("Chat about this", QDialogButtonBox::ActionRole);
            QObject::connect(chatButton, &QPushButton::clicked, &dialog, [chatBox, chatInput]() {
                chatBox->setVisible(!chatBox->isVisible());
                if (chatBox->isVisible()) chatInput->setFocus();
            });
            if (ask.session != GLOBAL_ID) { // the whole set answered by the global session
                QPushButton* global = buttons->addButton("Global answers", QDialogButtonBox::ActionRole);
                global->setToolTip("The global session answers them all for you");
                QObject::connect(global, &QPushButton::clicked, &dialog, [&dialog]() {dialog.done(3);});
            }
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
                QString log; // its chat
                for (const auto &[role, said]: now->chat)
                    log += "<p><b>" + QString(role == "user" ? "you" : "✻") + "</b> " + q_(said).toHtmlEscaped() + "</p>";
                if (now->chatting) log += "<p>✻ …</p>";
                if (chatLog->toHtml() != log) {
                    chatLog->setHtml(log);
                    chatLog->moveCursor(QTextCursor::End);
                }
                syncing = true;
                for (std::size_t k = 0; k < rows.size() && k <= now->index; ++k) {
                    const std::string question = rows[k].question.value("question", std::string());
                    const std::string recorded = now->answers.value(question, std::string());
                    const int count = static_cast<int>(rows[k].options.size());
                    for (int i = 0; i < count; ++i) {
                        const std::string label = rows[k].question["options"][static_cast<std::size_t>(i)].value("label", std::string());
                        const bool multi = rows[k].question.value("multiSelect", false);
                        if (multi && k == now->index) {
                            rows[k].options[static_cast<std::size_t>(i)]->setChecked(now->picked.contains(i));
                        } else if (multi || k < now->index) { // a question left: its recorded answer
                            if (!recorded.empty()) rows[k].options[static_cast<std::size_t>(i)]->setChecked(multi ? recorded == label || recorded.find(label + ", ") != std::string::npos || recorded.ends_with(", " + label) : recorded == label);
                        } else if (now->cursor == i && (now->index != lastIndex || now->cursor != lastCursor)) { // moved by the voice
                            rows[k].options[static_cast<std::size_t>(i)]->setChecked(true);
                        }
                    }
                    if (k == now->index) {
                        if (now->cursor == count && (now->index != lastIndex || now->cursor != lastCursor)) rows[k].other->setChecked(true);
                        if (rows[k].text->text().toStdString() != now->other) rows[k].text->setText(q_(now->other));
                    }
                }
                lastIndex = now->index;
                lastCursor = now->cursor;
                syncing = false;
            });
            timer.start(200);
            int result = 0;
            cluster::Json answers = cluster::Json::object();
            for (;;) {
                result = dialog.exec();
                if (result != QDialog::Accepted) break;
                answers = cluster::Json::object();
                for (const Row& row: rows) {
                    std::string value;
                    const cluster::Json options = row.question.value("options", cluster::Json::array());
                    for (std::size_t i = 0; i < row.options.size(); ++i)
                        if (row.options[i]->isChecked()) value += (value.empty() ? "" : ", ") + options[i].value("label", std::string());
                    const std::string typed = cluster::trim(row.text->text().toStdString());
                    if (row.other->isChecked() && !typed.empty()) value += (value.empty() ? "" : ", ") + typed;
                    answers[row.question.value("question", std::string())] = value;
                }
                if (rows.size() < 2) break;
                // like Claude Code: every answer reviewed, Submit / Back (to the questions) / Cancel
                QString summary;
                for (const Row& row: rows) {
                    const std::string answer = answers.value(row.question.value("question", std::string()), std::string());
                    summary += "<p>" + q_(row.question.value("question", std::string())).toHtmlEscaped() + "<br><b>→ "
                        + (answer.empty() ? QString("(no answer)") : q_(answer).toHtmlEscaped()) + "</b></p>";
                }
                QMessageBox review(QMessageBox::Question, q_(ask.name + " asks · review your answers"), summary + "<p>Ready to submit your answers?</p>",
                    QMessageBox::NoButton, this);
                review.setTextFormat(Qt::RichText);
                QPushButton* submit = review.addButton("Submit", QMessageBox::AcceptRole);
                QPushButton* back = review.addButton("Back", QMessageBox::RejectRole);
                QPushButton* cancel = review.addButton("Cancel", QMessageBox::DestructiveRole);
                review.setDefaultButton(submit);
                review.exec();
                if (review.clickedButton() == back) continue;
                if (review.clickedButton() == cancel) result = QDialog::Rejected;
                break;
            }
            timer.stop();
            if (result == 2) {
                this->message("question answered", false);
                return;
            }
            if (result == 3) {
                const std::string done = this->manager.asking().toGlobal();
                this->message(done, done != "the global session answers it");
                return;
            }
            if (result != QDialog::Accepted) {
                this->manager.asking().decline();
                this->message("question declined", false);
                return;
            }
            try {
                this->manager.answerQuestions(ask.session, ask.requestId, answers);
                this->message("answered", false);
            } catch (const utils::exception::IException& e) {
                this->message(e.info(), true); // answered elsewhere meanwhile (MCP...)
            }
        }
        std::int64_t lastInterrupt = 0;     // Ctrl+C Ctrl+C on an empty prompt, nothing running: quit

        QMessageBox* linkBox = nullptr;   // the core lost: Reconnecting (Retry now / Quit), closed once it is back
        std::string upgradeAsked;         // the newer version already offered in this window

        void showUpgrade(void)
        {
            // a newer claude-cluster installed: Update (default) / Ignore this version / Quit
            const cluster::UpgradeOffer offer = this->manager.upgradeOffer();
            // an update waiting already: offered again only for a newer version than the one it goes to (Update moves it there)
            if (offer.version.empty() || offer.upgrading || (offer.started && offer.version == offer.target) || offer.version == this->upgradeAsked
                || QApplication::activeModalWidget() != nullptr)
                return;
            this->upgradeAsked = offer.version;
            std::string notes;
            for (std::size_t i = 0; i < offer.notes.size() && i < 10; ++i) {
                const std::size_t space = offer.notes[i].find(' ');
                const std::string kind = offer.notes[i].substr(0, space);
                notes += "<br>• <b>" + std::string(kind == "fix" ? "fix" : kind == "feat" ? "new" : kind) + "</b> " + esc_(cluster::one_line(space == std::string::npos
                    ? offer.notes[i] : offer.notes[i].substr(space + 1), 160)).toStdString();
            }
            if (offer.notes.size() > 10) notes += "<br>… " + std::to_string(offer.notes.size() - 10) + " more";
            const std::string jump = offer.jump.empty() ? std::string() : " &nbsp;<i>" + offer.jump + (offer.jump == "patch" ? " (fixes and small additions)" : offer.jump == "minor"
                ? " (new features)" : " (big changes)") + "</i>";
            QMessageBox box(QMessageBox::Information, "claude-cluster", q_(std::string(offer.started ? "<b>A newer claude-cluster came while the update waits</b>" : "<b>A newer claude-cluster is installed</b>")
                + "<br>" + std::string(CLUSTER_VERSION) + " → " + offer.version + jump + (offer.started ? "<br>the update waiting goes to " + offer.target + ": Update moves it to " + offer.version : std::string())
                + (notes.empty() ? std::string() : "<br><br><b>What it brings</b>" + notes)
                + "<br><br>Update: the turns running end first, the sessions are saved, the core and every terminal / window switch to "
                "the new version by themselves, the sessions resumed."), QMessageBox::NoButton, this);
            QPushButton* update = box.addButton("Update", QMessageBox::AcceptRole);
            QPushButton* ignore = box.addButton("Ignore this version", QMessageBox::RejectRole);
            QPushButton* quit = box.addButton("Quit", QMessageBox::DestructiveRole);
            box.setDefaultButton(update);
            box.exec();
            const std::string action = box.clickedButton() == update ? "update" : box.clickedButton() == ignore ? "ignore" : box.clickedButton() == quit ? "quit" : "";
            if (action.empty()) return;
            try {
                this->message(this->manager.upgradeAct(action), false);
            } catch (const utils::exception::IException& e) {
                this->message(e.info(), true);
            }
        }

        void showLink(void)
        {
            const cluster::Link link = this->manager.link();
            if (!link.lost) {
                if (this->linkBox != nullptr) {
                    this->linkBox->deleteLater();
                    this->linkBox = nullptr;
                    this->message("the core of claude-cluster is back", false);
                }
                return;
            }
            if (this->linkBox == nullptr) {
                this->linkBox = new QMessageBox(QMessageBox::Warning, "claude-cluster", "", QMessageBox::NoButton, this);
                QPushButton* retry = this->linkBox->addButton("Retry now", QMessageBox::AcceptRole);
                QPushButton* quit = this->linkBox->addButton("Quit", QMessageBox::RejectRole);
                QObject::disconnect(retry, nullptr, nullptr, nullptr); // its buttons never close it: only the core back does
                QObject::disconnect(quit, nullptr, nullptr, nullptr);
                QObject::connect(retry, &QPushButton::clicked, this, [this]() {this->manager.retryNow();});
                QObject::connect(quit, &QPushButton::clicked, this, [this]() {
                    this->close();
                    QCoreApplication::quit();
                });
                this->linkBox->setWindowModality(Qt::WindowModal);
                this->linkBox->show();
            }
            const std::int64_t now = cluster::now_ms();
            if (link.upgrading) {
                this->linkBox->setText(q_("<b>Updating claude-cluster…</b><br>the core runs the new version; this window switches to it as soon as it is up"));
                return;
            }
            this->linkBox->setText(q_("<b>Reconnecting to claude-cluster…</b><br>its core is not reached since " + cluster::took(now - link.since) + "<br>try "
                + std::to_string(link.tries) + " · the next one in " + std::to_string((std::max<std::int64_t>(0, link.next - now) + 999) / 1000)
                + " s (the core started again when it is gone: the sessions resumed)"));
        }

        void tick(void)
        {
            if (cluster::quit_requested()) { // Ctrl+C in the terminal, kill, the terminal closed: the normal quit
                // a dialog open (a question at start...): closed without its answer first, else it keeps its own loop running
                for (int i = 0; i < 8 && QApplication::activeModalWidget() != nullptr; ++i)
                    QApplication::activeModalWidget()->close();
                this->close();
                QCoreApplication::quit();
                return;
            }
            this->scanProgress();
            this->showLink();
            this->showUpgrade();
            // the global session asked to show a session (ui_focus) or another style (ui_style)
            const std::pair<std::uint64_t, std::string> focus = this->manager.focusRequest();
            if (this->manager.raiseRequest() != this->raiseSeen) { // a notification clicked: this window to the front
                this->raiseSeen = this->manager.raiseRequest();
                const std::string token = this->manager.raiseToken(); // Wayland: the focus is given with the token of the click
                if (!token.empty()) qputenv("XDG_ACTIVATION_TOKEN", QByteArray::fromStdString(token));
                if (this->isMinimized()) this->showNormal();
                this->show();
                this->raise();
                this->activateWindow();
                if (token.empty()) QApplication::alert(this);
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
        // a line kept above / below the cursor when the prompt scrolls: after the scroll of Qt (which stops at the edge line)
        QTimer::singleShot(0, this, [this]() {
            QScrollBar* bar = this->verticalScrollBar();
            const int line = this->fontMetrics().lineSpacing();
            const QRect at = this->cursorRect();
            if (at.top() < line && bar->value() > bar->minimum()) bar->setValue(bar->value() - 1);
            else if (at.bottom() > this->viewport()->height() - line && bar->value() < bar->maximum()) bar->setValue(bar->value() + 1);
        });
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
    const std::string prefix = this->completion.kind() == cluster::CompletionKind::Session ? "::" : this->completion.kind() == cluster::CompletionKind::File ? "@" : this->completion.kind() == cluster::CompletionKind::Command || this->completion.kind() == cluster::CompletionKind::Skill ? "/" : "";
    this->popup->clear();
    for (const cluster::Candidate& c: this->completion.items()) {
        if (c.source.empty()) {
            this->popup->addItem(q_(prefix + c.label + "    " + cluster::one_line(c.description, 80)));
            continue;
        }
        // a /command or a skill: its source in a coloured badge at the side
        QListWidgetItem* item = new QListWidgetItem();
        this->popup->addItem(item);
        QLabel* row = new QLabel("&nbsp;" + esc_(prefix + c.label) + "&nbsp;&nbsp;<span style=\"background:" + q_(cluster::source_color(c.source)) + ";color:#fff;font-weight:600\">&nbsp;"
            + esc_(c.source) + "&nbsp;</span>&nbsp;&nbsp;<small>" + esc_(cluster::one_line(c.description, 80)) + "</small>");
        row->setAttribute(Qt::WA_TransparentForMouseEvents);
        this->popup->setItemWidget(item, row);
        item->setSizeHint(row->sizeHint());
    }
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

void PromptEdit::attachOne(const cluster::Attachment& item)
{
    // an image as the atom [Image #N] (given to the agent as a picture), the others as @path (read by the agent)
    if (item.image) this->addImage(item.path);
    else this->insert(q_("@" + item.path + " "));
}

void PromptEdit::attachBytes(const std::string& name, const std::string& bytes)
{
    try {
        this->attachOne(cluster::attach_store(this->window->manager.config(), this->window->active, name, bytes));
    } catch (const utils::exception::IException& e) {
        this->window->message(std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info()), true);
    }
}

void PromptEdit::attachFiles(const QStringList& paths, const int mode)
{
    const cluster::Config& config = this->window->manager.config();
    if (!config.attach.enabled) {
        this->window->message("attachments are off (settings: Attachments)", true);
        return;
    }
    bool link = mode == 2;
    if (mode == 0 && config.attach.link) { // this computer: nothing to upload, the choice is yours
        QMessageBox box(QMessageBox::Question, "Attach", "Attach " + QString::number(paths.size()) + " file(s): link them (the files of this computer are read where they are, nothing copied) or copy them?",
            QMessageBox::NoButton, this);
        QPushButton* linked = box.addButton("Link", QMessageBox::AcceptRole);
        QPushButton* copied = box.addButton("Copy", QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == linked) link = true;
        else if (box.clickedButton() != copied) return;
    }
    // the files already in this prompt count: the images (atoms) and the @paths of the attachments
    const std::string root = cluster::attach_root(config).string();
    std::size_t count = this->completion.images().size();
    for (qsizetype at = this->text().indexOf(q_("@" + root)); at >= 0; at = this->text().indexOf(q_("@" + root), at + 1)) ++count;
    for (const QString& path: paths) {
        if (static_cast<int>(count) >= config.attach.maxFiles) {
            this->window->message("at most " + std::to_string(config.attach.maxFiles) + " files per prompt (settings: Attachments)", true);
            break;
        }
        try {
            if (link) {
                this->attachOne(cluster::attach_link(config, this->window->active, path.toStdString(), cluster::AttachScope::User));
            } else {
                std::ifstream file(path.toStdString(), std::ios::binary);
                const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                this->attachOne(cluster::attach_store(config, this->window->active, QFileInfo(path).fileName().toStdString(), bytes));
            }
            ++count;
        } catch (const utils::exception::IException& e) {
            this->window->message(std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info()), true);
        }
    }
}

void PromptEdit::insertFromMimeData(const QMimeData* source)
{
    if (source->hasUrls()) { // files dropped / pasted from a file manager: attached (the other urls: their text)
        QStringList files;
        for (const QUrl& url: source->urls())
            if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile()) files << url.toLocalFile();
        if (!files.isEmpty()) {
            this->attachFiles(files);
            return;
        }
    }
    if (source->hasImage() && !source->hasText()) { // a picture dragged from a browser / an editor
        const QImage image = qvariant_cast<QImage>(source->imageData());
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!image.isNull() && image.save(&buffer, "PNG")) {
            this->attachBytes("image.png", bytes.toStdString());
            return;
        }
    }
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
    const bool present = this->text().contains(q_(label));
    if (insert) this->insert(q_(label + " "));
    this->refreshChips();
    return !present;
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

const std::set<std::string>& PromptEdit::knownSlash(void)
{
    const std::pair<std::string, std::uint64_t> key{this->window->active, this->window->manager.version()};
    if (key != this->knownKey) {
        this->knownKey = key;
        this->known = cluster::slash_known(this->window->manager, this->window->active);
    }
    return this->known;
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
    if (!open && this->text().isEmpty() && this->window->gridKey(event)) return;
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
        const bool edge = !probe.movePosition(event->key() == Qt::Key_Up ? QTextCursor::Up : QTextCursor::Down);
        if (edge && event->key() == Qt::Key_Up && this->window->takeBackUp()) return; // a prompt queued or not answered yet: back first
        if (edge && this->window->historyStep(event->key() == Qt::Key_Up ? 1 : -1)) return;
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
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            if (!image.isNull() && image.save(&buffer, "PNG")) {
                this->attachBytes("clipboard.png", bytes.toStdString()); // stored with the attachments of the session (caps, EXIF, purge)
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
    const std::function<QString(const cluster::Json&)> label = [](const cluster::Json& v) {return v.is_string() ? q_(v.get<std::string>()) : q_(v.dump(-1, ' ', false, cluster::JSON_LENIENT));};
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
        const std::function<QString(const cluster::Json&)> date = [](const cluster::Json& v) {return v.is_string() ? q_(v.get<std::string>()) : q_(v.dump(-1, ' ', false, cluster::JSON_LENIENT));};
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
        const QString value = field.contains("value") ? q_(field["value"].is_string() ? field["value"].get<std::string>() : field["value"].dump(-1, ' ', false, cluster::JSON_LENIENT)) : QString();
        QWidget* widget = nullptr;
        std::function<cluster::Json()> read;
        if (type == "choice") {
            QComboBox* box = new QComboBox(&dialog);
            for (const cluster::Json& option: field.value("options", cluster::Json::array()))
                box->addItem(q_(option.is_string() ? option.get<std::string>() : option.dump(-1, ' ', false, cluster::JSON_LENIENT)));
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
    const std::function<QString(const cluster::Json&)> cell = [](const cluster::Json& v) {return esc_(v.is_string() ? v.get<std::string>() : v.dump(-1, ' ', false, cluster::JSON_LENIENT));};
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
            const std::string color = state == "done" ? st.ok : state == "current" ? st.accent : state == "error" ? st.error : state == "cancelled" || state == "stopped" ? st.warn : st.muted;
            const QString text = esc_((state == "done" ? "✔ " : state == "current" ? "● " : state == "error" ? "✗ " : state == "cancelled" ? "⊘ " : state == "stopped" ? "⏸ " : "○ ")
                + step.value("text", std::string()));
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
            const std::string delta = m.contains("delta") ? (m["delta"].is_string() ? m["delta"].get<std::string>() : m["delta"].dump(-1, ' ', false, cluster::JSON_LENIENT)) : "";
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

/* the agents and shells of a session: the ones it runs itself (its Agent tool, its commands in the background) */
_cold static std::string agent_time_(const cluster::SubAgent& a)
{
    if (a.started <= 0) return "-";
    const std::int64_t s = std::max<std::int64_t>(0, ((a.ended > 0 ? a.ended : cluster::now_ms()) - a.started) / 1000);
    if (s < 60) return std::to_string(s) + " s";
    if (s < 3600) return std::to_string(s / 60) + " min " + std::to_string(s % 60) + " s";
    return std::to_string(s / 3600) + " h " + std::to_string(s % 3600 / 60) + " min";
}

_cold static std::string agent_state_(const cluster::SubAgent& a)
{
    return a.stopping ? "stopping…" : a.status == "running" ? "● running" : a.status == "done" ? "✓ done" : a.status == "failed" ? "✗ failed" : "■ stopped";
}

_cold static void agent_stop_(Window* window, QWidget* parent, const std::string& session, const cluster::SubAgent& a)
{
    // its stop_task: the agent / the shell ends, its session goes on
    if (a.status != "running") return window->message("it already ended", false);
    if (a.taskId.empty()) return window->message("claude did not tell its task yet: stop the turn of the session instead", true);
    if (QMessageBox::question(parent, "Stop it?", q_("Stop " + std::string(a.kind == "shell" ? "the shell" : "the agent") + " '" + a.description + "'? Its session goes on."))
        != QMessageBox::Yes)
        return;
    try {
        window->manager.stopAgent(session, a.taskId);
        window->message("stopping it (its session goes on)", false);
    } catch (const utils::exception::IException& e) {
        window->message(e.info(), true);
    }
}

_cold static void agent_window_(Window* window, const std::string& session, const std::string& task)
{
    // one agent / shell: its activity (its words, its tools, their results) or the end of its output, live; small / big
    QDialog* dialog = new QDialog(window);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(560, 460);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* meta = new QLabel(dialog);
    meta->setWordWrap(true);
    QTextBrowser* body = new QTextBrowser(dialog);
    body->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    QHBoxLayout* buttons = new QHBoxLayout();
    QPushButton* size = new QPushButton("Big", dialog);
    QPushButton* stop = new QPushButton("Stop", dialog);
    QPushButton* close = new QPushButton("Close", dialog);
    buttons->addWidget(size);
    buttons->addStretch();
    buttons->addWidget(stop);
    buttons->addWidget(close);
    layout->addWidget(meta);
    layout->addWidget(body, 1);
    layout->addLayout(buttons);
    const std::function<std::optional<cluster::SubAgent>(void)> find = [window, session, task]() -> std::optional<cluster::SubAgent> {
        const std::optional<cluster::Snapshot> snap = window->manager.snapshot(session);
        if (!snap) return std::nullopt;
        for (const cluster::SubAgent& a: snap->agents)
            if (a.taskId == task || a.toolUseId == task) return a;
        return std::nullopt;
    };
    std::shared_ptr<std::string> last = std::make_shared<std::string>();
    const std::function<void(void)> refresh = [=]() {
        const std::optional<cluster::SubAgent> a = find();
        if (!a) {
            meta->setText("not kept any more");
            return;
        }
        dialog->setWindowTitle(q_((a->kind == "shell" ? "Shell: " : "Agent: ") + a->description));
        meta->setText(q_(agent_state_(*a) + " · " + (a->kind == "shell" ? std::string("$ shell in the background") : a->type + " · " + (a->model.empty() ? std::string("model not seen yet") : a->model))
            + " · ⏱ " + agent_time_(*a) + (a->tokens > 0 ? " · " + cluster::human_tokens(a->tokens) + " tokens" : std::string())));
        stop->setEnabled(a->status == "running" && !a->stopping && !a->taskId.empty());
        std::string text;
        if (!a->prompt.empty()) text += "asked › " + a->prompt + "\n\n";
        for (const cluster::Entry& e: a->entries) {
            if (e.kind == cluster::EntryKind::User && !a->prompt.empty()) continue;
            text += e.kind == cluster::EntryKind::Tool ? "▸ " + e.tool + " " + e.text + "\n" : e.kind == cluster::EntryKind::ToolResult || e.kind == cluster::EntryKind::Error ? "  " + e.text + "\n" : e.text + "\n\n";
        }
        if (!a->outputFile.empty()) {
            std::string out = cluster::read_small(a->outputFile);
            if (out.size() > 8000) out = out.substr(out.size() - 8000);
            text += cluster::clean_utf8(out);
        }
        if (!a->summary.empty()) text += "\n⤷ " + a->summary;
        if (text == *last) return;
        *last = text;
        const bool end = body->verticalScrollBar()->value() >= body->verticalScrollBar()->maximum() - 20;
        body->setPlainText(q_(text));
        if (end) body->verticalScrollBar()->setValue(body->verticalScrollBar()->maximum());
    };
    QObject::connect(size, &QPushButton::clicked, dialog, [dialog, size]() {
        const bool big = size->text() == "Big";
        const QRect screen = dialog->screen() ? dialog->screen()->availableGeometry() : QRect(0, 0, 1280, 800);
        dialog->resize(big ? QSize(screen.width() * 9 / 10, screen.height() * 9 / 10) : QSize(560, 460));
        if (big) dialog->move(screen.center() - dialog->rect().center());
        size->setText(big ? "Small" : "Big");
    });
    QObject::connect(stop, &QPushButton::clicked, dialog, [=]() {
        if (const std::optional<cluster::SubAgent> a = find()) agent_stop_(window, dialog, session, *a);
    });
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    QTimer* timer = new QTimer(dialog);
    QObject::connect(timer, &QTimer::timeout, dialog, refresh);
    timer->start(1000);
    refresh();
    body->verticalScrollBar()->setValue(body->verticalScrollBar()->maximum());
    dialog->show();
}

_cold static void agents_dialog_(Window* window, const std::string& session)
{
    // the list: each agent and shell of the session, its state, model, effort, time, tokens, tools; Open, Stop (live)
    QDialog* dialog = new QDialog(window);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(980, 420);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* note = new QLabel("What this session runs itself: its agents (its Agent tool, not the sessions of claude-cluster) and its shells in the background. "
        "Stop one: the session goes on.", dialog);
    note->setWordWrap(true);
    QTableWidget* table = new QTableWidget(0, 9, dialog);
    table->setHorizontalHeaderLabels({"state", "what", "kind", "model", "thinking", "time", "tokens", "tools", "last tool"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->verticalHeader()->hide();
    QHBoxLayout* buttons = new QHBoxLayout();
    QPushButton* open = new QPushButton("Open", dialog);
    QPushButton* stop = new QPushButton("Stop", dialog);
    QPushButton* close = new QPushButton("Close", dialog);
    buttons->addWidget(open);
    buttons->addWidget(stop);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addWidget(note);
    layout->addWidget(table, 1);
    layout->addLayout(buttons);
    std::shared_ptr<std::vector<cluster::SubAgent>> shown = std::make_shared<std::vector<cluster::SubAgent>>();
    const std::function<void(void)> refresh = [=]() {
        const std::optional<cluster::Snapshot> snap = window->manager.snapshot(session);
        if (!snap) return;
        dialog->setWindowTitle(q_("Agents and shells of " + (snap->spec.global ? std::string("the global session") : snap->spec.name)));
        const std::string effort = cluster::effort_label(snap->spec.effort, window->manager.config().effort, snap->spec.backend, snap->modelUsed);
        *shown = std::vector<cluster::SubAgent>(snap->agents.rbegin(), snap->agents.rend());
        const int keep = table->currentRow();
        table->setRowCount(static_cast<int>(shown->size()));
        for (int row = 0; row < static_cast<int>(shown->size()); ++row) {
            const cluster::SubAgent& a = (*shown)[static_cast<std::size_t>(row)];
            const bool shell = a.kind == "shell";
            const std::string tag = a.label.empty() ? std::string() : a.label == "dangerous" ? " · ⚠ dangerous" : " · safe"; // an update stops a young safe one
            const std::vector<std::string> cells = {agent_state_(a), a.description.empty() ? a.type : a.description, (shell ? std::string("$ shell") : a.type + (a.background ? " · background" : "")) + tag,
                shell ? std::string("-") : a.model.empty() ? std::string("not seen yet") : a.model, shell ? std::string("-") : (a.effort.empty() ? effort : "think " + a.effort) + " (its session)",
                agent_time_(a), a.tokens > 0 ? cluster::human_tokens(a.tokens) : std::string("-"), std::to_string(a.toolUses), a.lastTool};
            for (int col = 0; col < static_cast<int>(cells.size()); ++col) {
                QTableWidgetItem* item = table->item(row, col);
                if (item == nullptr) {
                    item = new QTableWidgetItem();
                    table->setItem(row, col, item);
                }
                item->setText(q_(cells[static_cast<std::size_t>(col)]));
                if (col == 1) item->setToolTip(q_(!a.error.empty() ? a.error : a.summary));
            }
        }
        if (keep >= 0 && keep < table->rowCount()) table->selectRow(keep);
        else if (table->rowCount() > 0 && keep < 0) table->selectRow(0);
    };
    const std::function<std::optional<cluster::SubAgent>(void)> chosen = [=]() -> std::optional<cluster::SubAgent> {
        const int row = table->currentRow();
        if (row < 0 || row >= static_cast<int>(shown->size())) return std::nullopt;
        return (*shown)[static_cast<std::size_t>(row)];
    };
    QObject::connect(open, &QPushButton::clicked, dialog, [=]() {
        if (const std::optional<cluster::SubAgent> a = chosen()) agent_window_(window, session, a->taskId.empty() ? a->toolUseId : a->taskId);
    });
    QObject::connect(table, &QTableWidget::cellDoubleClicked, dialog, [=]() {
        if (const std::optional<cluster::SubAgent> a = chosen()) agent_window_(window, session, a->taskId.empty() ? a->toolUseId : a->taskId);
    });
    QObject::connect(stop, &QPushButton::clicked, dialog, [=]() {
        if (const std::optional<cluster::SubAgent> a = chosen()) agent_stop_(window, dialog, session, *a);
    });
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    QTimer* timer = new QTimer(dialog);
    QObject::connect(timer, &QTimer::timeout, dialog, refresh);
    timer->start(1000);
    refresh();
    dialog->show();
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
        if (link == "cluster-agents:") {
            agents_dialog_(window, this->id);
            return;
        }
        if (link == "cluster-resume:") {
            try {
                window->message(window->manager.resume(this->id), false);
            } catch (const utils::exception::IException& e) {
                window->message(e.info(), true);
            }
        } else if (link == "cluster-folder:") {
            cluster::open_folder(folder);
        } else {
            const std::string terminal = cluster::open_terminal(folder);
            window->message(terminal.empty() ? "no terminal found" : terminal + " opened in " + folder.string(), terminal.empty());
        }
    });
    this->log = new QTextBrowser(this);
    this->log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    this->log->setOpenLinks(false);
    const std::function<void(const QUrl&)> anchorClicked = [window, this](const QUrl& url) {
        // a display: tick / click (cluster-act:<display>/<item>), its form (cluster-form:<display>), its audio (cluster-play:<display>)
        const std::string scheme = url.scheme().toStdString();
        const std::string path = url.path().toStdString();
        if (scheme == "cluster-picture") { // a picture a tool read or made: the viewer (ui.image_viewer, else the desktop's)
            const std::string viewer = cluster::trim(window->manager.config().imageViewer);
            if (viewer.empty()) QDesktopServices::openUrl(QUrl::fromLocalFile(q_(path)));
            else {
                std::vector<std::string> command = cluster::split(viewer, ' ');
                command.push_back(path);
                cluster::detach(command);
            }
            return;
        }
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
    };
    QObject::connect(this->log, &QTextBrowser::anchorClicked, anchorClicked);
    // the pictures of the displays: display:<entry> in the transcript
    const std::function<QVariant(const QUrl&)> picture = [this](const QUrl& url) -> QVariant {
        const std::map<QString, QImage>::const_iterator it = this->pictures.find(url.toString());
        return it == this->pictures.end() ? QVariant() : QVariant(it->second);
    };
    this->log->document()->setResourceProvider(picture);
    const std::function<void(const QUrl&)> hover = [](const QUrl& url) {
        if (url.isLocalFile() && !QImage(url.toLocalFile()).isNull()) QToolTip::showText(QCursor::pos(), "<img src=\"" + url.toLocalFile() + "\" width=\"420\">");
        else QToolTip::hideText();
    };
    QObject::connect(this->log, &QTextBrowser::highlighted, hover);
    // a right click on a message or a block: Pin / Unpin
    this->log->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(this->log, &QWidget::customContextMenuRequested, [window, this](const QPoint& at) {this->entryMenu(window, this->log, at);});
    // the pinned item: always above the prompt, live like the transcript, folded by its title, scrolling inside when it is tall
    this->pinBox = new QFrame(this);
    this->pinBox->setObjectName("pinBox");
    QVBoxLayout* pinLayout = new QVBoxLayout(this->pinBox);
    pinLayout->setContentsMargins(6, 3, 6, 4);
    pinLayout->setSpacing(2);
    QHBoxLayout* pinHead = new QHBoxLayout();
    this->pinFold = new QPushButton(this->pinBox);
    this->pinFold->setFlat(true);
    this->pinFold->setStyleSheet("text-align:left;border:none;font-weight:600;padding:2px 4px;");
    this->pinFold->setToolTip("Fold / unfold the pinned item");
    QPushButton* unpin = new QPushButton("Unpin", this->pinBox);
    unpin->setToolTip("Remove the pin (the item stays in the conversation)");
    QObject::connect(unpin, &QPushButton::clicked, [window, this]() {
        try {
            window->manager.unpin(this->id);
        } catch (const utils::exception::IException& e) {
            window->message(e.info(), true);
        }
    });
    pinHead->addWidget(this->pinFold, 1);
    pinHead->addWidget(unpin);
    this->pinLog = new QTextBrowser(this->pinBox);
    this->pinLog->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    this->pinLog->setOpenLinks(false);
    QObject::connect(this->pinLog, &QTextBrowser::anchorClicked, anchorClicked);
    this->pinLog->document()->setResourceProvider(picture);
    QObject::connect(this->pinLog, &QTextBrowser::highlighted, hover);
    QObject::connect(this->pinFold, &QPushButton::clicked, [this]() {
        this->pinFolded = !this->pinFolded;
        this->pinLog->setVisible(!this->pinFolded);
        this->pinKey.clear();   // the arrow drawn again
    });
    pinLayout->addLayout(pinHead);
    pinLayout->addWidget(this->pinLog);
    this->pinBox->setVisible(false);
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
    this->newCue = new QPushButton("↓ new output", this);
    this->newCue->setFlat(true);
    this->newCue->setCursor(Qt::PointingHandCursor);
    this->newCue->setVisible(false);
    QObject::connect(this->newCue, &QPushButton::clicked, [this]() {
        QScrollBar* end = this->log->verticalScrollBar();
        end->setValue(end->maximum());
        this->newCue->setVisible(false);
    });
    QObject::connect(this->log->verticalScrollBar(), &QScrollBar::valueChanged, [this](const int value) {
        if (value >= this->log->verticalScrollBar()->maximum() - 4) this->newCue->setVisible(false); // the end reached: followed again
    });
    inside->addWidget(this->newCue);
    inside->addWidget(this->pinBox);
    inside->addWidget(this->permBar);
    layout->addWidget(this->header);
    layout->addWidget(card, 1);
    this->outline = new Outline(this);
    this->outline->raise();
}

void SessionView::entryMenu(Window* window, QTextBrowser* from, const QPoint& at)
{
    // the entry under the mouse: the last anchor (cc-e<index>) before the cursor; Pin it, Unpin, Copy
    QMenu menu(from);
    const std::optional<cluster::Snapshot> snap = window->manager.snapshot(this->id);
    if (!snap) return;
    long entry = -1;
    if (from == this->log) {
        const int position = from->cursorForPosition(at).position();
        for (QTextBlock block = from->document()->begin(); block.isValid() && block.position() <= position; block = block.next())
            for (QTextBlock::iterator it = block.begin(); !it.atEnd() && it.fragment().position() <= position; ++it)
                for (const QString& name: it.fragment().charFormat().anchorNames())
                    if (name.startsWith("cc-e")) entry = name.mid(4).toLong();
    }
    if (entry >= 0 && static_cast<std::size_t>(entry) < snap->entries.size()) {
        const cluster::EntryKind kind = snap->entries[static_cast<std::size_t>(entry)].kind;
        if (kind == cluster::EntryKind::User || kind == cluster::EntryKind::Assistant || kind == cluster::EntryKind::System || kind == cluster::EntryKind::Display)
            menu.addAction(kind == cluster::EntryKind::Display ? "Pin this block (kept above the prompt)" : "Pin this message (kept above the prompt)", [window, this, entry]() {
                try {
                    (void)window->manager.pin(this->id, {{"index", entry}});
                } catch (const utils::exception::IException& e) {
                    window->message(e.info(), true);
                }
            });
    }
    if (snap->spec.pin.is_object())
        menu.addAction(q_("Unpin: " + snap->spec.pin.value("title", std::string("the pinned item"))), [window, this]() {
            try {
                window->manager.unpin(this->id);
            } catch (const utils::exception::IException& e) {
                window->message(e.info(), true);
            }
        });
    if (from->textCursor().hasSelection()) menu.addAction("Copy", [from]() {from->copy();});
    if (!menu.isEmpty()) menu.exec(from->mapToGlobal(at));
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
    const bool busy = cluster::background_busy(s); // its turn over, its background still running: purple, said
    const std::string tone = s.state == cluster::State::Error ? st.error : s.state == cluster::State::Working ? st.ok : s.state == cluster::State::Waiting ? st.warn
        : busy ? busy_color_(st) : st.muted;
    const std::function<QString(const std::string&, const std::string&)> chip = [&st](const std::string& color, const std::string& text) {
        return pill_(text, color, mix_(st.surface, color, 0.2));
    };
    QString head = "<span style=\"font-size:11.5pt\"><b>" + esc_(s.spec.name) + "</b></span>&nbsp;&nbsp;" + chip(tone, busy ? std::string("~_~ background") : std::string(cluster::state_name(s.state))) + "&nbsp; "
        + chip(st.accent, model) + "&nbsp; " + chip(manager.authKind(s.spec.backend) == "api" ? st.warn : st.ok, manager.authKind(s.spec.backend))
        + "&nbsp; " + chip(st.muted, cluster::effort_label(s.spec.effort, manager.config().effort, s.spec.backend, s.modelUsed)) + "&nbsp; " + chip(st.muted, "mode " + s.spec.mode)
        + (compact ? QString() : "&nbsp;&nbsp;<a href=\"cluster-folder:\" style=\"color:" + q_(st.muted) + ";text-decoration:none;font-family:monospace;font-size:9pt\">"
            + esc_(cluster::short_path(s.spec.cwd)) + "</a>" + span_(st.muted, "<span style=\"font-family:monospace;font-size:9pt\">" + esc_(
                (this->branch.empty() ? std::string() : "  ⎇ " + this->branch) + "  " + cluster::host_name() + (s.pid > 0 ? "  pid " + std::to_string(s.pid) : ""))
            + "</span>") + "&nbsp; <a href=\"cluster-terminal:\" style=\"color:" + q_(st.accent) + ";text-decoration:none;font-size:9pt\">terminal</a>"
            + (s.warning.empty() || cluster::now() - s.warningAt >= 60 ? QString() : "<br>" + span_(st.warn, "⚠ " + esc_(cluster::one_line(s.warning, 160))))
            + (cluster::resumable(s).empty() ? QString() : "&nbsp; <a href=\"cluster-resume:\" style=\"color:" + q_(st.warn) + ";text-decoration:none;font-weight:600\">"
                + (cluster::resumable(s) == "retry" ? QString("retry now") : QString("continue")) + "</a>"));
    if (!s.agents.empty()) { // what it runs itself: its agents and its shells (a click: their list)
        const long running = std::count_if(s.agents.begin(), s.agents.end(), [](const cluster::SubAgent& a) {return a.status == "running";});
        const long shells = std::count_if(s.agents.begin(), s.agents.end(), [](const cluster::SubAgent& a) {return a.kind == "shell";});
        const long agents = static_cast<long>(s.agents.size()) - shells;
        std::string mark = agents > 0 ? std::to_string(agents) + (agents > 1 ? " agents" : " agent") : std::string();
        if (shells > 0) mark += (mark.empty() ? "" : " · ") + std::to_string(shells) + (shells > 1 ? " shells" : " shell");
        head += "&nbsp; <a href=\"cluster-agents:\" style=\"text-decoration:none\">" + chip(st.accent, "⧉ " + mark + (running > 0 ? " · " + std::to_string(running) + " running" : "")) + "</a>";
    }
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
        + "|" + std::to_string(displays) + "|" + std::to_string(s.older) + "|" + std::to_string(this->shownEntries)
        + "|" + cluster::state_name(s.state) + "|" + st.name + "|" + (expanded ? "e" : "") + (s.state == cluster::State::Working ? "|" + std::to_string(ms / 150) : "");
    if (key != this->lastKey) {
        this->lastKey = key;
        QString html;
        const std::size_t start = s.entries.size() > this->shownEntries ? s.entries.size() - this->shownEntries : 0;
        const QString more = " (" + q_(config.keys.contains("expand") ? config.keys.at("expand") : std::string("Ctrl+O")) + " to expand)";
        if (start > 0 || s.older > 0) { // more above: drawn once the top is reached, else read again from the log of the core
            const std::size_t above = start + s.older;
            html += "<p>" + span_(st.muted, "<i>↑ " + QString::number(above) + (above == 1 ? " older entry" : " older entries")
                + (cluster::now_ms() - this->olderAsked < 3000 ? " - loading…" : " - scroll up to see them") + "</i>") + "</p>";
        }
        const std::function<QString(const std::string&)> pre = [](const std::string& text) {return "<pre style=\"margin:0;white-space:pre-wrap\">" + esc_(text) + "</pre>";};
        // an empty anchor at the start of each entry (cc-e<index>): the right click finds the entry under the mouse by it
        const std::function<QString(const std::size_t)> entryAnchor = [](const std::size_t index) {return "<a name=\"cc-e" + QString::number(index) + "\"></a>";};
        for (std::size_t i = start; i < frame.visible && i < s.entries.size(); ++i) {
            const cluster::Entry& e = s.entries[i];
            const std::string text = i == frame.typed ? e.text.substr(0, frame.typedBytes) : e.text;
            switch (e.kind) {
                case cluster::EntryKind::User: {
                    // [Image: path] -> link, previewed on hover
                    QString body;
                    static const std::regex image(R"(\[Image: ([^\]]+)\]|@(/[^\s]+))");
                    std::string rest = text.starts_with("[voice] ") ? text.substr(8) : text; // the mark is for the agent
                    std::smatch match;
                    while (std::regex_search(rest, match, image)) {
                        const bool file = match[1].str().empty(); // @/path of an attachment (a file that exists), else the [Image: path]
                        const std::string path = file ? match[2].str() : match[1].str();
                        std::error_code exists;
                        if (file && !std::filesystem::is_regular_file(path, exists)) { // not a file: left as typed
                            body += esc_(match.prefix().str() + match[0].str());
                            rest = match.suffix().str();
                            continue;
                        }
                        body += esc_(match.prefix().str()) + "<a href=\"" + QUrl::fromLocalFile(q_(path)).toString() + "\">" + (file ? "📄 " : "[Image: ")
                            + esc_(std::filesystem::path(path).filename().string()) + (file ? "" : "]") + "</a>";
                        rest = match.suffix().str();
                    }
                    body += esc_(rest);
                    // coloured by its origin (mcp / task), with its label; the user: the accent, as before
                    std::string ink = st.accent;
                    QString lead = "› ";
                    if (cluster::origin_colors().enabled) {
                        const std::string origin = cluster::prompt_origin(e.origin, e.text);
                        if (const std::string color = cluster::origin_color(origin, st.dark); !color.empty()) ink = color;
                        if (const std::string label = cluster::origin_label(origin); !label.empty()) lead += q_(label) + " · ";
                    }
                    html += "<p>" + entryAnchor(i) + span_(ink, lead + body, true) + "</p>";
                    break;
                }
                case cluster::EntryKind::Assistant: html += "<p>" + entryAnchor(i) + span_(st.text, "⏺ " + esc_(text)) + "</p>"; break;
                case cluster::EntryKind::Thinking:
                    // like Claude Code: folded, Ctrl+O shows it
                    if (compact) break;
                    html += "<div style=\"margin-top:10px\">" + span_(st.muted, "<i>∴ Thinking" + (expanded ? QString("…") : more) + "</i>") + "</div>";
                    if (expanded) html += "<div style=\"margin-left:16px\">" + span_(st.muted, "<i>" + pre(text) + "</i>") + "</div>";
                    break;
                case cluster::EntryKind::Tool:
                    if (e.tool == "TodoWrite") html += "<div style=\"margin-top:10px\">" + entryAnchor(i) + span_(st.ok, "⏺ <b>Update Todos</b>") + "</div><div style=\"margin-left:16px\">" + span_(st.text, pre(text)) + "</div>";
                    else html += "<div style=\"margin-top:10px\">" + entryAnchor(i) + span_(st.ok, "⏺ <b>" + esc_(e.tool) + "</b>") + span_(st.text, "(" + esc_(text) + ")") + "</div>";
                    break;
                case cluster::EntryKind::ToolResult: {
                    // the first lines (Ctrl+O: all)
                    if (compact) break;
                    QStringList parts = q_(text).split('\n');
                    const qsizetype total = parts.size();
                    if (!expanded && total > 3) parts = parts.mid(0, 3);
                    const QString shown = parts.join('\n').trimmed().isEmpty() ? QString("(no output)") : parts.join('\n');
                    html += "<div style=\"margin-left:12px\">" + entryAnchor(i) + span_(st.muted, pre(("⎿  " + shown).replace("\n", "\n   ").toStdString())) + "</div>";
                    if (total > parts.size()) html += "<div style=\"margin-left:12px\">" + span_(st.muted, pre("   … +" + std::to_string(total - parts.size()) + " lines" + more.toStdString())) + "</div>";
                    break;
                }
                case cluster::EntryKind::System: html += "<div style=\"margin-top:10px\">" + entryAnchor(i) + span_(st.muted, "· " + esc_(text)) + "</div>"; break;
                case cluster::EntryKind::Error: html += "<div style=\"margin-top:10px\">" + entryAnchor(i) + span_(st.error, "✗ " + esc_(text)) + "</div>"; break;
                case cluster::EntryKind::Display: {
                    const cluster::Json d = cluster::Json::parse(e.text, nullptr, false);
                    if (d.is_object()) {
                        QString block = this->displayHtml(d, s.spec.id + "-" + std::to_string(e.time) + "-" + std::to_string(i), st, expanded);
                        html += block.left(block.indexOf('>') + 1) + entryAnchor(i) + block.mid(block.indexOf('>') + 1);   // its anchor inside its first element
                    }
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
                for (const cluster::Json& round: turn["rounds"]) { // a line per request to the model, what it did, its pictures (a link opens one)
                    html += "<br>" + span_(round.value("current", false) ? st.accent : st.muted, esc_("  │ " + round.value("text", std::string())));
                    if (const std::string about = round.value("about", std::string()); !about.empty()) html += "<br>" + span_(st.text, esc_("  │    ✻ " + about)); // what it does
                    if (const std::string did = round.value("did", std::string()); !did.empty()) html += "<br>" + span_(st.muted, esc_("  │    ↳ " + did));
                    for (const cluster::Json& picture: round.value("pictures", cluster::Json::array()))
                        html += "<br>" + span_(st.muted, "  │    ") + "<a href=\"cluster-picture:" + QString::fromUtf8(QUrl::toPercentEncoding(q_(picture.get<std::string>())))
                            + "\">" + esc_("📷 " + cluster::short_path(picture.get<std::string>())) + "</a>";
                }
                if (turn.value("hidden_thinking", false)) html += "<br>" + span_(st.muted, "<i>the text of its thinking is not sent by the model: its tokens only</i>");
            }
            html += "</p>";
        }
        QScrollBar* bar = this->log->verticalScrollBar();
        const bool bottom = bar->value() >= bar->maximum() - 4;
        const int value = bar->value();
        const int maximum = bar->maximum();
        // scrolled up: the entry at the top of the view (the last anchor cc-e<index> before it) and how far the view is inside it, to keep that line where it is
        long topEntry = -1;
        int inside = 0;
        QTextDocument* document = this->log->document();
        if (!bottom) {
            const int position = this->log->cursorForPosition(QPoint(0, 0)).position();
            QTextBlock topBlock;
            for (QTextBlock block = document->begin(); block.isValid() && block.position() <= position; block = block.next())
                for (QTextBlock::iterator it = block.begin(); !it.atEnd() && it.fragment().position() <= position; ++it)
                    for (const QString& name: it.fragment().charFormat().anchorNames())
                        if (name.startsWith("cc-e")) {
                            topEntry = name.mid(4).toLong();
                            topBlock = block;
                        }
            if (topEntry >= 0) inside = value - static_cast<int>(document->documentLayout()->blockBoundingRect(topBlock).top());
        }
        this->log->setHtml(html);
        int target = bottom ? bar->maximum() : value;
        if (topEntry >= 0) // entries put back in front or dropped from it since: its index moved with them
            topEntry += static_cast<long>(this->olderSeen) - static_cast<long>(s.older);
        this->olderSeen = s.older;
        if (topEntry >= 0) {
            const QString wanted = "cc-e" + QString::number(topEntry);
            for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
                bool found = false;
                for (QTextBlock::iterator it = block.begin(); !it.atEnd() && !found; ++it)
                    found = it.fragment().charFormat().anchorNames().contains(wanted);
                if (found) {
                    target = static_cast<int>(document->documentLayout()->blockBoundingRect(block).top()) + inside;
                    break;
                }
            }
        }
        bar->setValue(target);
        // new output below the view while scrolled up: a cue under it (the view stays where it is)
        if (bottom) this->newCue->setVisible(false);
        else if (bar->maximum() > maximum) this->newCue->setVisible(true);
    }
    {
        // its top reached: more entries drawn, or more read again from the log of the core (every interface sees them)
        QScrollBar* bar = this->log->verticalScrollBar();
        const std::size_t start = s.entries.size() > this->shownEntries ? s.entries.size() - this->shownEntries : 0;
        if (bar->maximum() > 0 && bar->value() <= bar->minimum()) {
            if (start > 0) {
                this->shownEntries += 400;
                this->lastKey.clear();
            } else if (s.older > 0 && cluster::now_ms() - this->olderAsked >= 3000) {
                this->olderAsked = cluster::now_ms();
                this->shownEntries += 400;
                this->lastKey.clear();
                std::thread([m = &manager, id = s.spec.id]() {
                    try {
                        (void)m->loadOlder(id, 400);
                    } catch (const std::exception&) {} // the core gone: asked again at the top
                }).detach();
            }
        }
    }

    // the pinned item: the display as it is now (a checklist ticked, a form submitted: drawn again), or the message
    const cluster::Json pinned = compact ? cluster::Json() : cluster::display_pinned(s);
    if (!pinned.is_object()) {
        this->pinBox->setVisible(false);
        this->pinKey.clear();
    } else {
        const std::string title = s.spec.pin.value("title", std::string("pinned"));
        const std::string drawn = pinned.dump(-1, ' ', false, cluster::JSON_LENIENT) + "|" + title + "|" + st.name + "|" + (this->pinFolded ? "f" : "") + std::to_string(this->width() / 40);
        this->pinBox->setVisible(true);
        if (drawn != this->pinKey) {
            this->pinKey = drawn;
            this->pinFold->setText(QString(this->pinFolded ? "▸ " : "▾ ") + "📌 " + q_(cluster::one_line(title, 80)));
            this->pinLog->setVisible(!this->pinFolded);
            QString html;
            if (pinned.contains("role")) html = "<p>" + span_(st.text, esc_(pinned.value("text", std::string()))) + "</p>";
            else html = this->displayHtml(pinned, s.spec.id + "-pin-" + std::to_string(s.spec.pin.value("at", std::int64_t{0})), st, false);
            QScrollBar* pinBar = this->pinLog->verticalScrollBar();
            const int pinValue = pinBar->value();
            this->pinLog->setHtml(html);
            this->pinLog->document()->setTextWidth(std::max(200, this->width() - 70));
            // its height: the item whole up to a third of the view, scrolling inside past it
            const int wanted = static_cast<int>(this->pinLog->document()->size().height()) + 16;
            this->pinLog->setFixedHeight(std::clamp(wanted, 40, std::max(80, this->height() / 3)));
            pinBar->setValue(pinValue);
        }
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
        p += title("Tokens") + "<table width=\"100%\">" + line("last turn", split(m.last))
            + (m.last.total() > 0 ? "<tr><td colspan=\"2\">" + span_(st.muted, esc_(cluster::usage_detail2(m.last).first)) + "<br>" + span_(st.muted, esc_(cluster::usage_detail2(m.last).second)) + "</td></tr>" : QString())
            + line("current", split(m.current)) + line("total", split(m.total))
            + (m.total.total() > 0 ? "<tr><td colspan=\"2\">" + span_(st.muted, esc_(cluster::usage_detail2(m.total).first)) + "<br>" + span_(st.muted, esc_(cluster::usage_detail2(m.total).second)) + "</td></tr>" : QString())
            + line("turns", std::to_string(m.turns)) + "</table>";
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
    if (s.spec.panels.contains("tasks")) {
        p += title("Tasks");
        for (const cluster::Todo& d: s.todos)
            p += "<div>" + span_(d.status == "completed" ? st.ok : d.status == "in_progress" ? st.accent : st.muted, d.status == "completed" ? "☒ " : d.status == "in_progress" ? "◼ " : "☐ ")
                + esc_(cluster::one_line(d.content, 34)) + "</div>";
        for (const cluster::Task& task: manager.tasks())
            if (!task.archived && (task.session == s.spec.id || task.target == s.spec.id) && (task.status == "queued" || task.status == "running"))
                p += "<div>" + span_(st.muted, q_((task.status == "running" ? "▶ " : "◌ ") + task.id + " " + cluster::one_line(task.prompt, 30))) + "</div>";
        if (s.todos.empty()) p += "<div>" + span_(st.muted, "no todo list") + "</div>";
    }
    if (s.spec.panels.contains("files")) {
        p += title("Files");
        for (const std::string& file: s.files)
            p += "<div>" + esc_(cluster::one_line(cluster::short_path(file), 38)) + "</div>";
    }
    this->panels->setText(p);
    this->panels->setVisible(!p.isEmpty() && !this->plain);
    if (this->plain) this->header->setVisible(false);
}

} // namespace

/* entry */
_cold int cluster::gui_choice(const std::string& title, const std::string& text, const std::vector<std::string>& choices)
{
    // a question before the window (its own application, gone after)
    int argc = 1;
    char name[] = "claude-cluster";
    char* argv[] = {name, nullptr};
    QApplication::setApplicationName("claude-cluster");
    QGuiApplication::setDesktopFileName("claude-cluster");
    QApplication application(argc, argv);
    QMessageBox box(QMessageBox::Warning, q_(title), q_(text));
    std::vector<QAbstractButton*> buttons;
    for (const std::string& choice: choices)
        buttons.push_back(box.addButton(q_(choice), buttons.empty() ? QMessageBox::AcceptRole : QMessageBox::RejectRole));
    if (!buttons.empty()) box.setDefaultButton(static_cast<QPushButton*>(buttons.back()));
    (void)box.exec();
    for (std::size_t i = 0; i < buttons.size(); ++i)
        if (box.clickedButton() == buttons[i]) return static_cast<int>(i);
    return -1;
}

_cold int cluster::supervisor_gui(std::string& open)
{
    // every claude-cluster of this computer in a tree (F2: their sessions under them) and the agents running outside it, read again
    // every 1.5 s, in the colours of claude-cluster; the actions in a thread, their result under the list; a device asking to come in:
    // its popup; F1 the keys, F9 the web access (settings, devices), P the performance; quit: its web access closed with it
    int argc = 1;
    char name[] = "claude-cluster";
    char* argv[] = {name, nullptr};
    QApplication::setApplicationName("claude-cluster");
    QGuiApplication::setDesktopFileName("claude-cluster");
    QApplication application(argc, argv);
    cluster::Config config;
    config.load(); // the style chosen (a bare Config: the defaults)
    const cluster::Style st = config.currentStyle();
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(q_(st.bg)));
    palette.setColor(QPalette::Base, QColor(q_(st.surface)));
    palette.setColor(QPalette::AlternateBase, QColor(q_(st.bg)));
    palette.setColor(QPalette::Text, QColor(q_(st.text)));
    palette.setColor(QPalette::WindowText, QColor(q_(st.text)));
    palette.setColor(QPalette::Button, QColor(q_(st.surface)));
    palette.setColor(QPalette::ButtonText, QColor(q_(st.text)));
    palette.setColor(QPalette::Highlight, QColor(q_(mix_(st.surface, st.accent, 0.30))));
    palette.setColor(QPalette::HighlightedText, QColor(q_(st.text)));
    palette.setColor(QPalette::PlaceholderText, QColor(q_(st.muted)));
    QApplication::setPalette(palette);
    application.setStyleSheet(q_(
        "QWidget{background:" + st.bg + ";color:" + st.text + ";}"
        "QTreeWidget{background:" + st.surface + ";border:1px solid " + st.border + ";border-radius:10px;padding:4px;}"
        "QTreeWidget::item{padding:3px 2px;}"
        "QTreeWidget::item:selected{background:" + mix_(st.surface, st.accent, 0.28) + ";color:" + st.text + ";}"
        "QTreeWidget::item:hover{background:" + mix_(st.surface, st.accent, 0.12) + ";}"
        "QHeaderView::section{background:" + st.bg + ";color:" + st.muted + ";border:none;border-bottom:1px solid " + st.border + ";padding:4px 6px;}"
        "QPushButton{background:" + st.surface + ";border:1px solid " + st.border + ";border-radius:8px;padding:6px 12px;}"
        "QPushButton:hover{border-color:" + st.accent + ";color:" + st.accent + ";}"
        "QPushButton#risky:hover{border-color:" + st.warn + ";color:" + st.warn + ";}"
        "QLineEdit,QComboBox,QSpinBox{background:" + st.surface + ";border:1px solid " + st.border + ";border-radius:8px;padding:4px 8px;}"
        "QScrollBar:vertical{width:8px;background:transparent;}QScrollBar::handle:vertical{background:" + st.border + ";border-radius:4px;min-height:24px;}"
        "QScrollBar::add-line,QScrollBar::sub-line{height:0;}"));
    QWidget window;
    window.setWindowTitle("claude-cluster supervisor");
    window.resize(1180, 680);
    QVBoxLayout* layout = new QVBoxLayout(&window);
    QLabel* head = new QLabel(&window);
    head->setTextFormat(Qt::RichText);
    layout->addWidget(head);
    QTreeWidget* tree = new QTreeWidget(&window);
    tree->setHeaderLabels({"claude-cluster", "kind", "version", "state", "update"});
    tree->setColumnWidth(0, 250);
    tree->setColumnWidth(1, 110);
    tree->setColumnWidth(2, 80);
    tree->setColumnWidth(3, 560);
    tree->setRootIsDecorated(true);
    layout->addWidget(tree, 1);
    QLabel* note = new QLabel(&window);
    note->setWordWrap(true);
    note->setStyleSheet(q_("color:" + st.accent + ";"));
    layout->addWidget(note);
    QHBoxLayout* row = new QHBoxLayout();
    layout->addLayout(row);
    bool details = false;
    std::vector<cluster::SupAgent> outside;
    std::set<std::string> asked;
    const std::function<std::string(void)> chosen = [tree]() {
        QTreeWidgetItem* item = tree->currentItem();
        while (item != nullptr && item->parent() != nullptr)
            item = item->parent();
        return item == nullptr ? std::string() : item->data(0, Qt::UserRole).toString().toStdString();
    };
    const std::function<QString(const std::string&)> tint = [&st](const std::string& state) {
        return q_(state == "working" ? st.ok : state == "question" || state == "waiting" ? st.warn : state == "error" ? st.error : state == "background" ? std::string("#a46ad8") : st.muted);
    };
    const std::function<void(void)> draw = [&]() {
        const std::vector<cluster::SupInstance> all = cluster::supervisor_scan();
        outside = cluster::supervisor_agents();
        const std::string installed = cluster::supervisor_installed();
        const cluster::Json web = cluster::supervisor_web_state();
        const std::string keep = chosen();
        head->setText(q_("<b><span style=\"color:" + st.accent + "\">^_^ claude-cluster supervisor</span></b> <span style=\"color:" + st.muted + "\">" + std::string(CLUSTER_VERSION)
            + (installed != CLUSTER_VERSION ? " (installed " + installed + ")" : "") + " · " + std::to_string(all.size()) + " claude-cluster · " + (details ? "details" : "normal")
            + " (F2) · F1 help</span>  " + (web.value("on", false) ? "<span style=\"color:" + st.ok + "\">web " + esc_(web.value("url", std::string())).toStdString() + "  code "
            + web.value("pin", std::string()) + "</span>" : "<span style=\"color:" + st.muted + "\">web off</span>")));
        tree->clear();
        for (const cluster::SupInstance& it: all) {
            const std::string face = !it.alive ? "z_z" : it.upgrading ? "^_^" : it.waiting > 0 ? "•_• !" : it.errors > 0 ? "o_o" : it.working > 0 ? ">_<" : it.background > 0 ? ">_< ~" : "-_-";
            const std::string state = !it.alive ? "stopped · " + std::to_string(it.kept) + " session(s) kept" : std::to_string(it.sessions.size()) + " sessions · "
                + std::to_string(it.working) + " working · " + std::to_string(it.idle) + " idle · " + std::to_string(it.waiting) + " waiting for you · "
                + std::to_string(it.background) + " background" + (it.queued > 0 ? " · " + std::to_string(it.queued) + " queued" : "") + (it.web ? " · web on" : "");
            const std::string update = it.upgrading ? "⟳ updating" : it.alive && cluster::version_compare(it.version, installed) < 0 ? "⬆ " + installed : "";
            QTreeWidgetItem* item = new QTreeWidgetItem(tree, {q_(face + "   " + (it.main ? "◆ " : "◇ ") + it.name), q_(it.main ? "main" : "independent"), q_(it.alive ? it.version : "-"),
                q_(state), q_(update)});
            item->setData(0, Qt::UserRole, q_(it.name));
            item->setForeground(0, QColor(q_(it.main ? st.accent : mix_(st.accent, st.ok, 0.6))));
            item->setForeground(3, QColor(tint(!it.alive ? "idle" : it.waiting > 0 ? "question" : it.errors > 0 ? "error" : it.working > 0 ? "working" : it.background > 0 ? "background" : "idle")));
            QFont bold = item->font(0);
            bold.setBold(true);
            item->setFont(0, bold);
            if (it.name == keep) tree->setCurrentItem(item);
            if (!details) continue;
            for (const cluster::Json& s: it.sessions) {
                const bool asking = s.value("asking", 0) > 0;
                const std::string sessionState = asking ? "question" : s.value("background", false) ? "background" : s.value("state", std::string());
                std::string line = asking ? std::string("waits for you") : s.value("background", false) ? std::string("idle, its background runs") : s.value("state", std::string());
                if (s.value("queued", 0) > 0) line += " · " + std::to_string(s.value("queued", 0)) + " queued";
                if (s.value("agents", 0) > 0) line += " · " + std::to_string(s.value("agents", 0)) + " agent(s)";
                if (s.value("keep_warm", false)) line += " · ♨";
                QTreeWidgetItem* child = new QTreeWidgetItem(item, {q_((s.value("name", std::string()) == "global" ? "◆ " : "") + s.value("name", std::string())), QString(),
                    q_(s.value("model", std::string())), q_(line), QString()});
                child->setForeground(3, QColor(tint(sessionState)));
            }
            item->setExpanded(true);
        }
        QTreeWidgetItem* group = new QTreeWidgetItem(tree, {q_("Other agents on this computer"), QString(), QString(), q_(outside.empty() ? std::string("none running")
            : std::to_string(outside.size()) + " running · double click: open one in claude-cluster"), QString()});
        group->setForeground(0, QColor(q_(st.muted)));
        for (std::size_t i = 0; i < outside.size(); ++i) {
            const cluster::SupAgent& a = outside[i];
            QTreeWidgetItem* child = new QTreeWidgetItem(group, {q_("●  " + a.agent), q_(cluster::short_path(a.cwd)), q_("pid " + std::to_string(a.pid)),
                q_((a.state == "question" ? std::string("waits for you") : a.state == "background" ? std::string("its background runs") : a.state) + " · " + cluster::one_line(a.title, 70)), QString()});
            child->setData(0, Qt::UserRole + 1, static_cast<int>(i) + 1);
            child->setForeground(0, QColor(tint(a.state)));
            child->setForeground(3, QColor(tint(a.state)));
        }
        group->setExpanded(true);
        // a device asking to use the supervisor: its popup, once
        for (const cluster::Json& d: web.value("devices", cluster::Json::array())) {
            const std::string id = d.value("id", std::string());
            if (d.value("state", std::string()) != "waiting" || asked.contains(id)) continue;
            asked.insert(id);
            QMessageBox box(&window);
            box.setWindowTitle("A device asks to use the supervisor");
            box.setText(q_(d.value("name", std::string("a device")) + " · " + d.value("address", std::string()) + "\n" + cluster::one_line(d.value("agent", std::string()), 160)));
            QPushButton* allow = box.addButton("Allow", QMessageBox::AcceptRole);
            QPushButton* trust = box.addButton("Allow and trust", QMessageBox::AcceptRole);
            box.addButton("Deny", QMessageBox::RejectRole);
            (void)box.exec();
            note->setText(q_(cluster::supervisor_web_device("approve", id, box.clickedButton() == allow ? "allow" : box.clickedButton() == trust ? "trust" : "deny")));
        }
        if (cluster::version_compare(installed, CLUSTER_VERSION) > 0) { // its own update: run again on the new binary
            ::setenv("CLAUDE_CLUSTER_SUPERVISOR", "1", 1);
            cluster::upgrade_request(false);
            application.quit();
        }
    };
    const std::function<void(const std::string&, const std::string&)> act = [&](const std::string& target, const std::string& action) {
        if (target.empty()) return;
        note->setText("…");
        std::thread([&window, note, target, action]() {
            std::string done;
            try {
                done = cluster::supervisor_act(target, action);
            } catch (const std::exception& e) {
                done = e.what();
            }
            QMetaObject::invokeMethod(&window, [note, done]() {note->setText(q_(done));}, Qt::QueuedConnection);
        }).detach();
    };
    const std::function<bool(const QString&, const QString&)> sure = [&window](const QString& title, const QString& text) {
        return QMessageBox::question(&window, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
    };
    const std::function<QPushButton*(const QString&, const std::function<void(void)>&)> button = [&](const QString& label, const std::function<void(void)>& run) {
        QPushButton* b = new QPushButton(label, &window);
        QObject::connect(b, &QPushButton::clicked, &window, run);
        row->addWidget(b);
        return b;
    };
    const std::function<void(void)> openChosen = [&]() {
        if (const QTreeWidgetItem* item = tree->currentItem(); item != nullptr && item->data(0, Qt::UserRole + 1).toInt() > 0) { // an outside agent
            const cluster::SupAgent a = outside[static_cast<std::size_t>(item->data(0, Qt::UserRole + 1).toInt() - 1)];
            // its choices, Cancel the default (Enter / Esc never do an action by mistake)
            QMessageBox box(QMessageBox::Question, q_(a.agent + " outside claude-cluster"), q_(a.agent + " · " + cluster::short_path(a.cwd) + "\n" + a.title
                + (a.needs.empty() ? std::string() : "\n\nneeds: " + a.needs)), QMessageBox::NoButton, &window);
            QPushButton* cancel = box.addButton("Cancel", QMessageBox::RejectRole);
            QPushButton* agents = a.agent.starts_with("claude") ? box.addButton(a.state == "question" ? "Reply: Claude Code's agents view" : "Claude Code's agents view",
                QMessageBox::ActionRole) : nullptr;
            QPushButton* copy = a.id.empty() ? nullptr : box.addButton("Open in claude-cluster (a copy)", QMessageBox::ActionRole);
            QPushButton* terminal = a.cwd.empty() ? nullptr : box.addButton("A terminal in its folder", QMessageBox::ActionRole);
            QPushButton* folder = a.cwd.empty() ? nullptr : box.addButton("Copy its folder", QMessageBox::ActionRole);
            box.setDefaultButton(cancel);
            box.setEscapeButton(cancel);
            (void)box.exec();
            const QAbstractButton* clicked = box.clickedButton();
            const std::string where = a.cwd.empty() ? cluster::expand_home("~").string() : a.cwd;
            try {
                if (clicked == nullptr || clicked == cancel) return;
                if (clicked == copy) return note->setText(q_(cluster::supervisor_agent_open(a)));
                if (clicked == folder) {
                    QGuiApplication::clipboard()->setText(q_(a.cwd));
                    return note->setText(q_(a.cwd + " copied"));
                }
                const std::string opened = clicked == agents ? cluster::open_terminal_command(where, "claude agents") : cluster::open_terminal(where);
                note->setText(q_(opened.empty() ? "no terminal emulator found (Interface > Terminal emulator)" : "opened in " + opened));
            } catch (const std::exception& e) {
                note->setText(q_(e.what()));
            }
            return;
        }
        const std::string target = chosen();
        if (target.empty() || !sure("Open it?", q_("Open " + target + "? The supervisor closes, its interface opens."))) return;
        open = target;
        application.quit();
    };
    button("Open", openChosen);
    button("Update", [&]() {act(chosen(), "update");});
    button("Update all", [&]() {act("*", "update");});
    button("Force", [&]() {
        if (sure("Force the switch?", q_("Force the switch of " + chosen() + ": its turns and its background stopped now, then continued by the new version?"))) act(chosen(), "force");
    })->setObjectName("risky");
    button("Force all", [&]() {
        if (sure("Force the switch?", "Force the switch of every claude-cluster that is behind?")) act("*", "force");
    })->setObjectName("risky");
    button("Stop", [&]() {
        if (sure("Stop it?", q_("Stop " + chosen() + "? Its terminals / windows closed, its sessions kept."))) act(chosen(), "stop");
    })->setObjectName("risky");
    button("Start", [&]() {act(chosen(), "start");});
    button("Web on / off", [&]() {
        note->setText("…");
        std::thread([&window, note]() {
            const std::string done = cluster::supervisor_web_toggle();
            QMetaObject::invokeMethod(&window, [note, done]() {note->setText(q_(done));}, Qt::QueuedConnection);
        }).detach();
    });
    const std::function<void(void)> webDialog = [&]() {
        // F9: the settings of its web access (shared by every claude-cluster, but its port) and its devices
        QDialog dialog(&window);
        dialog.setWindowTitle("Web access of the supervisor");
        dialog.resize(760, 560);
        QVBoxLayout* inside = new QVBoxLayout(&dialog);
        QFormLayout* form = new QFormLayout();
        inside->addLayout(form);
        std::vector<std::pair<cluster::Setting, std::function<std::string(void)>>> fields;
        cluster::Config now;
        now.load();
        for (const cluster::SettingsPage& page: cluster::settings_pages(now))
            for (const cluster::Setting& s: page.settings) {
                if (s.section != "web" || s.kind == cluster::SettingKind::Action || s.key == "port" || s.key == "tty_port" || s.key == "enabled") continue;
                const std::string value = s.value(now);
                QWidget* field = nullptr;
                std::function<std::string(void)> read;
                if (s.kind == cluster::SettingKind::Bool) {
                    QCheckBox* box = new QCheckBox(&dialog);
                    box->setChecked(value == "true");
                    read = [box]() {return std::string(box->isChecked() ? "true" : "false");};
                    field = box;
                } else if (s.kind == cluster::SettingKind::Choice) {
                    QComboBox* box = new QComboBox(&dialog);
                    for (const std::string& c: s.choices)
                        box->addItem(q_(c));
                    box->setCurrentText(q_(value));
                    read = [box]() {return box->currentText().toStdString();};
                    field = box;
                } else {
                    QLineEdit* line = new QLineEdit(q_(value), &dialog);
                    read = [line]() {return line->text().toStdString();};
                    field = line;
                }
                field->setToolTip(q_(s.description));
                form->addRow(q_(s.label), field);
                fields.emplace_back(s, read);
            }
        // beyond the LAN: its address, else why not; the router / tailscale asked again
        {
            const cluster::Json web = cluster::supervisor_web_state();
            QLabel* outside = new QLabel(&dialog);
            outside->setWordWrap(true);
            outside->setText(q_(now.web.remote == "off" || now.web.remote.empty() ? std::string("outside: off (Beyond the LAN)") : "outside (" + now.web.remote + "): "
                + (web.value("remote_ok", false) ? web.value("remote_url", std::string()) : "✗ " + web.value("remote", std::string("not reached")))));
            outside->setStyleSheet(q_("color:" + (web.value("remote_ok", false) ? st.ok : st.warn) + ";"));
            inside->addWidget(outside);
            QHBoxLayout* remote = new QHBoxLayout();
            inside->addLayout(remote);
            for (const auto &[label, action]: std::vector<std::pair<QString, std::string>>{{"Ask the router / tailscale again", "reopen"}, {"Tailscale up", "tailscale_up"},
                {"Allow tailscale certificates", "tailscale_operator"}}) {
                QPushButton* b = new QPushButton(label, &dialog);
                QObject::connect(b, &QPushButton::clicked, &dialog, [note, action]() {note->setText(q_(cluster::supervisor_web_service(action)));});
                remote->addWidget(b);
            }
        }
        QLabel* shared = new QLabel("shared by every claude-cluster of this computer (but its port); the access closed and opened again uses them", &dialog);
        shared->setStyleSheet(q_("color:" + st.muted + ";"));
        inside->addWidget(shared);
        QTreeWidget* devices = new QTreeWidget(&dialog);
        devices->setHeaderLabels({"device", "address", "state"});
        devices->setRootIsDecorated(false);
        const cluster::Json web = cluster::supervisor_web_state();
        for (const char* list: {"devices", "trusted", "banned"})
            for (const cluster::Json& d: web.value(list, cluster::Json::array())) {
                QTreeWidgetItem* item = new QTreeWidgetItem(devices, {q_(d.value("name", std::string("device"))), q_(d.value("address", std::string())),
                    q_(std::string(list) == "devices" ? d.value("state", std::string()) : std::string(list) == "trusted" ? std::string("trusted") : std::string("banned"))});
                item->setData(0, Qt::UserRole, q_(d.value("id", std::string())));
            }
        inside->addWidget(devices, 1);
        QHBoxLayout* acts = new QHBoxLayout();
        inside->addLayout(acts);
        for (const auto &[label, what, answer]: std::vector<std::tuple<QString, std::string, std::string>>{{"Allow", "approve", "allow"}, {"Allow and trust", "approve", "trust"},
            {"Deny", "approve", "deny"}, {"Kick", "kick", ""}, {"Untrust", "untrust", ""}, {"Ban", "ban", ""}, {"Unban", "unban", ""}}) {
            QPushButton* b = new QPushButton(label, &dialog);
            QObject::connect(b, &QPushButton::clicked, &dialog, [devices, note, what, answer]() {
                if (devices->currentItem() == nullptr) return;
                note->setText(q_(cluster::supervisor_web_device(what, devices->currentItem()->data(0, Qt::UserRole).toString().toStdString(), answer)));
            });
            acts->addWidget(b);
        }
        QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, &dialog);
        inside->addWidget(box);
        QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QObject::connect(box, &QDialogButtonBox::accepted, &dialog, [&]() {
            cluster::Config writing;
            writing.load();
            std::string done;
            for (const auto &[s, read]: fields) {
                const std::string value = read();
                if (value == s.value(writing)) continue;
                const std::string error = cluster::setting_check(s, value);
                if (!error.empty()) {
                    QMessageBox::warning(&dialog, q_(s.label), q_(error));
                    return;
                }
                if (const std::string danger = cluster::setting_danger(s, value); !danger.empty() && !sure(q_(s.label), q_(danger))) continue;
                (void)cluster::settings_set(writing, "web", s.key, value);
                done += (done.empty() ? "" : ", ") + s.label;
            }
            note->setText(q_(done.empty() ? std::string("nothing changed") : "saved: " + done + " (the access closed and opened again uses them)"));
            dialog.accept();
        });
        (void)dialog.exec();
    };
    button("Web settings (F9)", webDialog);
    button("Copy the code", [&]() { // the code of its web access, ready to paste in the page
        std::string digits;
        for (const char c: cluster::supervisor_web_state().value("pin", std::string()))
            if (c >= '0' && c <= '9') digits += c;
        if (digits.empty()) return note->setText("no code: the web access is closed (Web on / off opens it)");
        QGuiApplication::clipboard()->setText(q_(digits));
        note->setText(q_("code " + digits.substr(0, 3) + " " + digits.substr(3) + " copied"));
    });
    const std::function<void(void)> perfDialog = [&]() {
        // P: the performance of the computer, read again every 1.5 s while it is open
        QDialog dialog(&window);
        dialog.setWindowTitle("Performance of this computer");
        dialog.resize(640, 520);
        QVBoxLayout* inside = new QVBoxLayout(&dialog);
        QLabel* text = new QLabel(&dialog);
        text->setTextFormat(Qt::RichText);
        text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        inside->addWidget(text, 1);
        std::shared_ptr<cluster::SupPerf> meter = std::make_shared<cluster::SupPerf>();
        const std::function<std::string(const std::string&, double, const std::string&)> bar = [&st](const std::string& label, double ratio, const std::string& value) {
            ratio = std::clamp(ratio, 0.0, 1.0);
            const std::string color = ratio > 0.9 ? st.error : ratio > 0.7 ? st.warn : st.ok;
            return "<tr><td style=\"padding-right:12px\">" + label + "</td><td><table cellspacing=0 cellpadding=0 width=260><tr><td width=" + std::to_string(static_cast<int>(ratio * 260))
                + " style=\"background:" + color + "\">&nbsp;</td><td style=\"background:" + st.border + "\">&nbsp;</td></tr></table></td><td style=\"padding-left:12px\">" + value + "</td></tr>";
        };
        const std::function<void(void)> fill = [&]() {
            cluster::supervisor_perf(*meter, cluster::supervisor_scan());
            const cluster::SupPerf& p = *meter;
            const std::function<std::string(double)> gb = [](const double mb) {
                std::ostringstream s;
                s.setf(std::ios::fixed);
                s.precision(1);
                s << mb / 1024.0;
                return s.str();
            };
            std::string html = "<b>" + esc_(p.cpu).toStdString() + "</b><br><span style=\"color:" + st.muted + "\">" + std::to_string(p.physical) + " cores · " + std::to_string(p.threads)
                + " threads" + (p.mhz > 0 ? " · " + std::to_string(p.mhz) + " MHz" : "") + (p.tempC > 0 ? " · " + std::to_string(static_cast<int>(p.tempC)) + " °C" : "") + "</span><table>";
            html += bar("CPU", p.cpuPercent / 100.0, std::to_string(static_cast<int>(p.cpuPercent)) + " %");
            html += bar("RAM", p.memTotalMb > 0 ? 1.0 - static_cast<double>(p.memAvailMb) / static_cast<double>(p.memTotalMb) : 0,
                gb(static_cast<double>(p.memTotalMb - p.memAvailMb)) + " / " + gb(static_cast<double>(p.memTotalMb)) + " GB");
            if (p.swapTotalMb > 0) html += bar("swap", static_cast<double>(p.swapUsedMb) / static_cast<double>(p.swapTotalMb), gb(static_cast<double>(p.swapUsedMb)) + " / " + gb(static_cast<double>(p.swapTotalMb)) + " GB");
            for (const cluster::SupGpu& g: p.gpus) {
                html += "<tr><td colspan=3><b>" + esc_(g.name).toStdString() + "</b></td></tr>";
                if (g.vramTotalMb > 0) html += bar("VRAM", g.vramUsedMb >= 0 ? static_cast<double>(g.vramUsedMb) / static_cast<double>(g.vramTotalMb) : 0,
                    (g.vramUsedMb >= 0 ? gb(static_cast<double>(g.vramUsedMb)) : std::string("?")) + " / " + gb(static_cast<double>(g.vramTotalMb)) + " GB");
                if (g.busy >= 0) html += bar("GPU", g.busy / 100.0, std::to_string(static_cast<int>(g.busy)) + " %");
            }
            if (p.diskTotalGb > 0) html += bar("disk", 1.0 - static_cast<double>(p.diskFreeGb) / static_cast<double>(p.diskTotalGb), std::to_string(p.diskTotalGb - p.diskFreeGb) + " / "
                + std::to_string(p.diskTotalGb) + " GB");
            html += "</table><br><span style=\"color:" + st.muted + "\">load " + std::to_string(p.load1).substr(0, 4) + " / " + std::to_string(p.load5).substr(0, 4) + " / "
                + std::to_string(p.load15).substr(0, 4) + " · " + p.power + " · up " + p.uptime + "</span>";
            for (const auto &[core, mb]: p.cores)
                html += "<br><span style=\"color:" + st.muted + "\">" + esc_(core).toStdString() + ": " + std::to_string(mb) + " MB</span>";
            text->setText(q_(html));
        };
        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, &dialog, fill);
        fill();
        timer.start(1500);
        (void)dialog.exec();
    };
    button("Performance (P)", perfDialog);
    button("Details (F2)", [&]() {
        details = !details;
        draw();
    });
    const std::function<void(void)> help = [&]() {
        QMessageBox::information(&window, "Keys", "Enter / double click: open (asked first)\nF2: normal / details (the sessions)\nF9: the web access of the supervisor (settings, devices)\n"
            "P: the performance of the computer\nF1: this help\n\nfaces: >_< working · •_• ! waits for you · >_< ~ background · o_o error · -_- idle · z_z stopped\n"
            "◆ the main claude-cluster · ◇ an independent instance · ● the agents running outside claude-cluster (their state in colour)\n\n"
            "Stop and Force are asked again; quitting closes the web access of the supervisor.");
    };
    for (const auto &[key, run]: std::vector<std::pair<QKeySequence, std::function<void(void)>>>{{QKeySequence(Qt::Key_F1), help}, {QKeySequence(Qt::Key_F9), webDialog},
        {QKeySequence(Qt::Key_P), perfDialog}, {QKeySequence(Qt::Key_Return), openChosen}, {QKeySequence(Qt::Key_F2), [&]() {details = !details; draw();}}}) {
        QShortcut* shortcut = new QShortcut(key, &window);
        QObject::connect(shortcut, &QShortcut::activated, &window, run);
    }
    QObject::connect(tree, &QTreeWidget::itemDoubleClicked, &window, [&]() {openChosen();});
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &window, draw);
    timer.start(1500);
    draw();
    window.show();
    if (const char* shot = std::getenv("CLAUDE_CLUSTER_SCREENSHOT"); shot != nullptr && *shot) // Debug: the window saved, then closed
        QTimer::singleShot(2500, &window, [&window, &application, shot]() {
            (void)window.grab().save(QString::fromUtf8(shot));
            application.quit();
        });
    (void)application.exec();
    (void)cluster::supervisor_web_close_mine(); // quit: its web access never left open
    return 0;
}

_cold int cluster::run_gui(cluster::App& app)
{
    int argc = 1;
    char name[] = "claude-cluster";
    char* argv[] = {name, nullptr};
    // named before the application exists: registered once with the portal under this id (named after: a second
    // registration refused, "Connection already associated with an application ID")
    QApplication::setApplicationName("claude-cluster");
    QGuiApplication::setDesktopFileName("claude-cluster"); // the dock: its .desktop and its icon
    QApplication application(argc, argv);
    cluster::debug("render", "window: Qt " + std::string(qVersion()) + " on " + QGuiApplication::platformName().toStdString() + ", scale "
        + std::to_string(application.devicePixelRatio()).substr(0, 4) + ", " + (app.options.values.contains("ttymode") || app.config.guiMode == "tty" ? "terminal mode" : "gui mode"));
    if (app.options.values.contains("ttymode") || app.config.guiMode == "tty") {
        // the terminal mode: the terminal interface in a pty fills the window (this process stays the core of the sessions)
        QMainWindow shell;
        shell.setWindowTitle("claude-cluster · terminal");
        TermView* term = new TermView(&shell);
        shell.setCentralWidget(term);
        shell.resize(1180, 740);
        shell.show();
        term->setFocus();
        if (const char* screenshot = std::getenv("CLAUDE_CLUSTER_SCREENSHOT"); screenshot && *screenshot) {
            const QString path = screenshot;
            QTimer::singleShot(4000, &shell, [&shell, path]() {
                shell.grab().save(path);
                QApplication::quit();
            });
        }
        QTimer* watch = new QTimer(&shell);
        QObject::connect(watch, &QTimer::timeout, &shell, [term, &shell]() {if (!term->alive()) shell.close();}); // the terminal quit: the window too
        watch->start(500);
        return application.exec();
    }
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
    if (cluster::quit_requested()) return 0; // a quit signal during a question at start: out (the window already closed)
    // Debug: CLAUDE_CLUSTER_SCREENSHOT=<file.png> saves the window after 3 s then quits (with QT_QPA_PLATFORM=offscreen)
    const char* screenshot = std::getenv("CLAUDE_CLUSTER_SCREENSHOT");
    if (screenshot && *screenshot) {
        const QString path = screenshot;
        if (std::getenv("CLAUDE_CLUSTER_SCREENSHOT_SETTINGS")) QTimer::singleShot(1000, &window, [&window]() {window.settings();});
        if (std::getenv("CLAUDE_CLUSTER_SCREENSHOT_TASKS")) QTimer::singleShot(600, &window, [&window]() {window.showTasks();});
        if (const char* typed = std::getenv("CLAUDE_CLUSTER_SCREENSHOT_TYPE")) QTimer::singleShot(1500, &window, [&window, typed]() {
            window.input->setText(typed); // debug: text typed in the prompt (completion list shown)
            window.input->refreshCompletion();
        });
        QTimer* done = new QTimer(&window);
        QObject::connect(done, &QTimer::timeout, &window, [&window, path]() {
            if (QApplication::activeModalWidget() != nullptr) return; // a dialog being shot (the settings): after it
            window.refresh(true);
            window.grab().save(path);
            QApplication::quit();
        });
        QTimer::singleShot(3000, done, [done]() {done->start(300);});
    }
    return QApplication::exec();
}
