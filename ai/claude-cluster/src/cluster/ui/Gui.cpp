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
#include "cluster/Services.hpp"
#include "cluster/Showcase.hpp"
#include "cluster/Display.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Session.hpp"
#include "cluster/Machine.hpp"
#include "cluster/WebTty.hpp"
#include "cluster/Worker.hpp"
#include "cluster/Editor.hpp"
#include "cluster/Reveal.hpp"
#include "cluster/Usage.hpp"
#include "cluster/Icons.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Core.hpp"
#include "cluster/Git.hpp"
#include "cluster/Vt.hpp"
#include <QGraphicsSimpleTextItem>
#include <QGraphicsEllipseItem>
#include <QSyntaxHighlighter>
#include <QGraphicsLineItem>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QGraphicsScene>
#include <QFontDatabase>
#include <QTextDocument>
#include <QGestureEvent>
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
#include <QMenu>
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
        AtomHighlighter(PromptEdit* owner) : QSyntaxHighlighter(owner->document()), _edit(owner) { /* Nothing */ };
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

// The terminal interface inside the window: claude-cluster --tty in a pty, its screen drawn (the emulator), every key
// and the mouse of a terminal given to it, resized with the window
class TermView: public QWidget {
    private:
        cluster::Vt _vt{120, 36};
        cluster::Pty _pty;
        std::thread _reader;
        std::mutex _mutex;
        std::string _pending;           // what the pty wrote, not fed yet
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
                std::lock_guard<std::mutex> lock(this->_mutex);
                if (!this->_pending.empty()) {
                    this->_vt.feed(this->_pending);
                    this->_pending.clear();
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
            if (event->key() == Qt::Key_Left) this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() - step);
            else if (event->key() == Qt::Key_Right) this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() + step);
            else if (event->key() == Qt::Key_Up) this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - step);
            else if (event->key() == Qt::Key_Down) this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() + step);
            else if (event->key() == Qt::Key_A || event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) this->zoomBy(1.2);
            else if (event->key() == Qt::Key_E || event->key() == Qt::Key_Minus) this->zoomBy(1 / 1.2);
            else if (event->key() == Qt::Key_0) this->whole();
            else {
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
            if (!this->_pressedId.empty() && (event->pos() - this->_pressed).manhattanLength() < 6 && this->onOpen) this->onOpen(this->_pressedId);
            this->_pressedId.clear();
        }

        void mouseDoubleClickEvent(QMouseEvent* event) override
        {
            if (this->items(event->pos()).empty()) this->whole();
            else QGraphicsView::mouseDoubleClickEvent(event);
        }

        bool event(QEvent* event) override
        {
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
            this->setToolTip("drag · wheel · arrows · a / e zoom · two fingers · double click: whole");
        }

        void whole(void)
        {
            this->resetTransform();
            this->centerOn(0, 0);
        }

        void populate(const std::vector<cluster::Snapshot>& sessions, const cluster::Style& st, const std::string& active)
        {
            // drawn again (the view kept where it is): the lines, the nodes around, the middle one
            this->_scene.clear();
            this->_scene.setBackgroundBrush(QColor(QString::fromStdString(st.bg)));
            const cluster::Snapshot* global = nullptr;
            std::vector<const cluster::Snapshot*> others;
            for (const cluster::Snapshot& s: sessions)
                (s.spec.id == GLOBAL_ID ? global : others.emplace_back()) = &s;
            const std::function<QColor(const cluster::Snapshot&)> color = [&st](const cluster::Snapshot& s) {
                return QColor(QString::fromStdString(!s.permissions.empty() ? st.warn : s.state == cluster::State::Working || s.state == cluster::State::Starting ? st.ok
                    : s.state == cluster::State::Error ? st.error : st.muted));
            };
            const double radius = std::max(170.0, static_cast<double>(others.size()) * 92.0 / (2.0 * M_PI));
            const std::function<void(const cluster::Snapshot&, double, double, double)> node = [this, &st, &color, &active](const cluster::Snapshot& s, const double x, const double y, const double r) {
                QGraphicsEllipseItem* ring = this->_scene.addEllipse(x - r, y - r, 2 * r, 2 * r, QPen(color(s), s.spec.id == active ? 3.5 : 2), QBrush(QColor(QString::fromStdString(st.surface))));
                ring->setData(0, QString::fromStdString(s.spec.id));
                ring->setCursor(Qt::PointingHandCursor);
                ring->setToolTip(QString::fromStdString(s.spec.name + " · " + cluster::state_name(s.state) + " · " + cluster::short_path(s.spec.cwd)));
                const char* face = !s.permissions.empty() ? "•_•" : s.state == cluster::State::Working ? ">_<" : s.state == cluster::State::Error ? "o_o" : "-_-";
                QGraphicsSimpleTextItem* faceItem = this->_scene.addSimpleText(face, QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), r > 40 ? 13 : 10));
                faceItem->setBrush(color(s));
                faceItem->setPos(x - faceItem->boundingRect().width() / 2, y - faceItem->boundingRect().height() / 2);
                faceItem->setData(0, QString::fromStdString(s.spec.id));
                QFont bold;
                bold.setBold(true);
                QGraphicsSimpleTextItem* name = this->_scene.addSimpleText(QString::fromStdString(cluster::one_line(s.spec.global ? std::string("global") : s.spec.name, 22)), bold);
                name->setBrush(QColor(QString::fromStdString(st.text)));
                name->setPos(x - name->boundingRect().width() / 2, y + r + 6);
                name->setData(0, QString::fromStdString(s.spec.id));
                QGraphicsSimpleTextItem* state = this->_scene.addSimpleText(QString::fromStdString(s.state == cluster::State::Working ? cluster::turn_state(s).value("verb", std::string("working"))
                    : cluster::state_name(s.state)));
                state->setBrush(QColor(QString::fromStdString(st.muted)));
                state->setPos(x - state->boundingRect().width() / 2, y + r + 24);
            };
            for (std::size_t i = 0; i < others.size(); ++i) {
                const double a = (static_cast<double>(i) / static_cast<double>(std::max<std::size_t>(1, others.size()))) * 2.0 * M_PI - M_PI / 2.0;
                const double x = std::cos(a) * radius;
                const double y = std::sin(a) * radius * 0.78;
                QPen pen(color(*others[i]), others[i]->state == cluster::State::Working ? 2.5 : 1.5);
                if (others[i]->state != cluster::State::Working) pen.setStyle(Qt::DashLine);
                this->_scene.addLine(0, 0, x, y, pen);
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
            QMainWindow::closeEvent(event);
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
            box->setDefaultButton(ok);
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->setWindowModality(Qt::NonModal);
            QObject::connect(box, &QMessageBox::finished, this, [this, box, retry, id = f.id]() {
                const bool again = retry != nullptr && box->clickedButton() == retry;
                this->failureBox = nullptr;
                this->manager.failureAnswer(id, again);
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
            this->progressBox->move((this->width() - this->progressBox->width()) / 2, 70);
            this->progressBox->show();
            this->progressBox->raise();
            this->animating = this->animating || !p->done;
        }

        QTextBrowser* tasksView = nullptr;      // the Tasks session (list / tabs)
        HubView* hub = nullptr;                 // the hub layout: the graph
        QLabel* hubNote = nullptr;              // its notes at the bottom (no sub-session, the keys)
        QDialog* tasksWindow = nullptr;         // the Tasks session in a window of its own (grid)
        QTextBrowser* tasksBrowser = nullptr;
        std::string tasksShownHtml;

        QTextBrowser* makeTasksView(QWidget* parent)
        {
            QTextBrowser* browser = new QTextBrowser(parent);
            browser->setOpenLinks(false);
            browser->setFrameShape(QFrame::NoFrame);
            browser->setVisible(false);
            // without the mouse: its links by the keys (↑↓ / j k / Tab: the next one, Enter: its actions)
            browser->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard | Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
            browser->setTabChangesFocus(false);
            browser->installEventFilter(this);
            QObject::connect(browser, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
                // a task / a schedule / the archive: its actions (the palette: its reason asked when it takes one); a session: shown
                const QString link = url.toString();
                if (link.startsWith("focus:")) {
                    this->active = link.mid(6).toStdString();
                    this->refresh(true);
                } else {
                    this->palette(link);
                }
            });
            return browser;
        }

        bool eventFilter(QObject* watched, QEvent* event) override
        {
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
            // the Tasks session: shown in place of a session (list, tabs), a window of its own in the grid
            if (this->manager.ui("layout", this->manager.config().layout) != "grid") {
                this->active = TASKS_ID;
                this->refresh(true);
                return;
            }
            if (this->tasksWindow == nullptr) {
                this->tasksWindow = new QDialog(this);
                this->tasksWindow->setWindowTitle("Tasks");
                this->tasksWindow->resize(820, 640);
                QVBoxLayout* layout = new QVBoxLayout(this->tasksWindow);
                this->tasksBrowser = this->makeTasksView(this->tasksWindow);
                this->tasksBrowser->setVisible(true);
                layout->addWidget(this->tasksBrowser);
            }
            this->fillTasks(this->tasksBrowser);
            this->tasksWindow->show();
            this->tasksWindow->raise();
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
            for (const cluster::Snapshot& s: this->manager.list(true)) {
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
                const std::string mark = t.status == "running" ? "▶" : t.status == "queued" ? (t.forced ? "»" : "◌") : t.status == "done" ? "✓" : t.status == "failed" ? "✗" : "⊘";
                const std::string where = !t.session.empty() ? (this->manager.snapshot(t.session) ? this->manager.snapshot(t.session)->spec.name : t.session) : t.target.empty() ? "any" : t.target;
                const std::string age = t.status == "queued" ? "waits " + span(now - t.created) : t.status == "running" ? span(now - t.started) : span(now - t.ended) + " ago";
                html += "<tr><td width=18>" + span_(tint, q_(mark)) + "</td><td width=\"55%\">" + link("task:" + t.id) + span_(st.muted, q_(t.id)) + " " + esc_(cluster::one_line(t.prompt, 90)) + "</a>"
                    + (t.reason.empty() ? QString() : "<br>" + span_(st.warn, "↳ " + esc_(t.reason))) + "</td><td class=m>→ " + esc_(where) + "</td><td class=m>by "
                    + esc_(who(t.by)) + "</td><td class=m>" + q_(age) + "</td></tr>";
            }
            html += "</table>" + (tasks.size() > 12 ? "<p class=m>… +" + QString::number(tasks.size() - 12) + " more</p>" : QString())
                + (tasks.empty() ? "<p class=m>empty: type a prompt below</p>" : QString());
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
                const std::string when = open ? "in " + span(w.at - now) + " (" + cluster::clock_time(w.at) + ")" : w.status == "done" ? "run " + span(now - w.last) + " ago" : "cancelled";
                html += "<tr><td width=18>" + span_(open ? st.accent : w.status == "done" ? st.muted : st.warn, open ? "⏱" : w.status == "done" ? "✓" : "⊘") + "</td><td>" + link("sched:" + w.id)
                    + span_(st.muted, q_(w.id)) + " " + esc_(cluster::one_line(w.text, 80)) + "</a>" + (w.reason.empty() ? QString() : "<br>" + span_(st.warn, "↳ " + esc_(w.reason)))
                    + "</td><td class=m width=\"22%\">" + q_(when + (w.every > 0 ? " · every " + span(static_cast<std::int64_t>(w.every) * 60) : "")) + "</td><td class=m>"
                    + esc_(w.action == "notify" ? std::string("a notification") : "→ " + (w.session == GLOBAL_ID ? std::string("global") : w.session)) + "</td><td class=m>by "
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
            const std::string key = std::string(cluster::face_name(face.face)) + std::to_string(face.badge) + (face.urgent ? "!" : "");
            cluster::dock_badge(face.badge, face.urgent);
            if (key == this->faceShown) return;
            this->faceShown = key;
            QPixmap pixmap;
            pixmap.loadFromData(QByteArray::fromStdString(cluster::face_svg(face.face, face.badge, face.urgent)), "SVG");
            this->setWindowIcon(QIcon(pixmap));
            this->setWindowTitle(QString("(") + QString::fromUtf8(cluster::face_text(face.face)) + ") " + (face.badge > 0 ? QString::number(face.badge) + " · " : QString())
                + "claude-cluster");
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
                    if (result.ui.starts_with("pick:delete:")) { // a session deleted for good: asked here
                        this->confirmDelete(result.ui.substr(12));
                    } else if (result.ui.starts_with("pick:")) { // a next step to pick (where it applies), once this palette is closed
                        const QString next = q_(result.ui.substr(5));
                        QTimer::singleShot(0, this, [this, next]() {this->palette(next);});
                    }
                    this->refresh(true);
                }, Qt::QueuedConnection);
            });
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
                    } else if (setting.kind == cluster::SettingKind::Action && page.section == "services") {
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
                                const std::string action = button->property("action").toString().toStdString();
                                const std::string label = button->text().toStdString();
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
                                    QMetaObject::invokeMethod(this, [this, done, failed, button, svc]() {
                                        this->message(done, failed);
                                        if (!svc->alive) return;
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
                    }
                    widget->setToolTip(tip);
                    QLabel* label = new QLabel(tab);
                    label->setTextFormat(Qt::RichText);
                    label->setToolTip(tip);
                    const cluster::Style st = this->theme();
                    const std::function<void()> mark = [label, setting, read, st]() { // ⚠: it can be dangerous; red: its value is
                        const bool now = !cluster::setting_danger(setting, read()).empty();
                        label->setText((now ? "<span style=\"color:" + q_(st.error) + ";font-weight:600\">" + esc_(setting.label) + "</span>" : esc_(setting.label))
                            + (setting.restart ? " *" : "") + (setting.danger ? " <span style=\"color:#e5b400\" title=\"can be dangerous\">⚠</span>" : ""));
                    };
                    mark();
                    if (setting.danger) marks.push_back(mark);
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
                scrolls.push_back(scroll);
                scroll->setWidget(tab);
                scroll->setWidgetResizable(true);
                scroll->setFrameShape(QFrame::NoFrame);
                sections->addTab(scroll, q_(page.title));
            }
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
                if (svc->reading || svc->page == nullptr || sections->currentWidget() != svc->page) return;
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
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, &fields, &dialog]() {
                std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> now;
                for (const auto &[setting, read]: fields)
                    if (const std::string value = read(); value != setting.value(this->manager.config())) now[{setting.section, setting.key}] = {setting, value};
                if (this->confirmDangers(now, &dialog)) dialog.accept(); // back: the page stays open
            });
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            // Ctrl+S: saved, the page stays (the values written become the current ones); F10: save and close
            QShortcut* saveKey = new QShortcut(QKeySequence("Ctrl+S"), &dialog);
            QObject::connect(saveKey, &QShortcut::activated, [this, &fields, &dialog]() {
                std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> now;
                for (const auto &[setting, read]: fields) {
                    const std::string value = read();
                    if (value != setting.value(this->manager.config())) now[{setting.section, setting.key}] = {setting, value};
                }
                if (!this->confirmDangers(now, &dialog)) return; // back: nothing saved
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
            const bool accepted = dialog.exec() == QDialog::Accepted;

            std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> changes;
            for (const auto &[setting, read]: fields) {
                const std::string value = read();
                if (value != setting.value(config)) changes[{setting.section, setting.key}] = {setting, value};
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
            for (const char* key: {"layout", "style", "vim"})
                if (changes.contains({"ui", key})) this->manager.setUi(key, changes.at({"ui", key}).second);
            (void)this->manager.config().reload();
            this->bindKeys();
            this->signature.clear();
            this->message(std::to_string(changes.size()) + " setting(s) saved", false);
        }

        void move(const int delta)
        {
            // next / previous: the Tasks session first (not in the grid: its own window), then the sessions
            std::vector<std::string> ids;
            if (this->manager.ui("layout", this->manager.config().layout) != "grid") ids.push_back(TASKS_ID);
            for (const cluster::Snapshot& s: this->manager.list(true))
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
            else if (action == "prev_session") this->move(-1);
            else if (action == "global") {
                this->active = GLOBAL_ID;
                this->refresh(true);
            } else if (action == "new_session") this->choose({"new_session", "New session", "folder [backend]", ".", false});
            else if (action == "close_session") this->run({"close_session", "", "", "", false}, "");
            else if (action == "allow" || action == "deny" || action == "mute") this->run({action, "", "", "", false}, "");
            else if (action == "push_to_talk") this->run({"voice_toggle", "", "", "", false}, "");
            else if (action == "resume") this->run({"resume", "", "", "", false}, "");
            else if (action == "send_next") this->run({"send_next", "", "", "", false}, "");
            else if (action == "layout") {
                const std::string layout = this->manager.ui("layout", this->manager.config().layout);
                this->manager.setUi("layout", layout == "list" ? "grid" : layout == "grid" ? "tabs" : layout == "tabs" ? "hub" : "list");
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

        void modeTarget(void)
        {
            // its mode shown by its target: ! a command of the shell (in the folder of the session), : a command of claude-cluster
            const std::string text = this->input->plain();
            const cluster::Style& st = this->theme();
            if (text.starts_with("!")) {
                const std::optional<cluster::Snapshot> here = this->manager.snapshot(this->active);
                this->target->setText(q_("$ shell · " + (here ? cluster::short_path(here->spec.cwd) : std::string("~"))));
                this->target->setStyleSheet("color:" + q_(st.warn) + ";font-weight:600;");
            } else if (text.starts_with(":")) {
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
                        if (local->ui == "settings") this->settings();
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
            this->tasksView = nullptr;
            this->hub = nullptr;
            this->hubNote = nullptr;
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
            this->webLabel = new QLabel(topbar); // the web access: its address, its code, connected, mean ping (a click: the code copied)
            this->webLabel->setTextFormat(Qt::RichText);
            this->webLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
            this->webLabel->setVisible(false);
            QObject::connect(this->webLabel, &QLabel::linkActivated, this, [this](const QString& link) {this->webLink(link);});
            for (const auto &[name, label]: std::vector<std::pair<std::string, std::string>>{{"list", "List"}, {"grid", "Grid"}, {"tabs", "Tabs"}, {"hub", "Hub"}}) {
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
            for (const auto &[label, action]: std::vector<std::pair<std::string, std::string>>{{"Tasks", "tasks"}, {"Palette", "palette"}, {"Devices", "devices"}, {"Settings", "settings"}}) {
                if (action == "devices" && !cluster::web_status(this->manager)) continue;
                QPushButton* button = new QPushButton(q_(label), topbar);
                button->setCursor(Qt::PointingHandCursor);
                const std::string name = action;
                QObject::connect(button, &QPushButton::clicked, [this, name]() {
                    if (name == "tasks") this->showTasks();
                    else if (name == "devices") this->webDevices();
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
            } else if (layout == "hub") {
                // the graph; a node clicked: its session shown in the list layout
                this->hub = new HubView(central);
                this->hub->onOpen = [this](const std::string& id) {
                    this->active = id;
                    this->manager.setUi("layout", "list");
                    this->refresh(true);
                };
                root->addWidget(this->hub, 1);
                this->hubNote = new QLabel(central);
                this->hubNote->setAlignment(Qt::AlignCenter);
                this->hubNote->setWordWrap(true);
                root->addWidget(this->hubNote);
                QTimer::singleShot(0, this->hub, [hub = this->hub]() {hub->whole();});
            } else if (layout == "tabs") {
                this->tabs = new QTabBar(central);
                this->tabs->setContextMenuPolicy(Qt::CustomContextMenu);
                QObject::connect(this->tabs, &QTabBar::customContextMenuRequested, this, [this](const QPoint& pos) {
                    const int index = this->tabs->tabAt(pos) - 1; // the Tasks session first
                    const std::vector<cluster::Snapshot> all = this->manager.list(true);
                    if (index >= 0 && static_cast<std::size_t>(index) < all.size()) this->sessionMenu(all[static_cast<std::size_t>(index)].spec.id, this->tabs->mapToGlobal(pos));
                });
                this->tabs->addTab("☰ Tasks");
                for (const cluster::Snapshot& s: sessions)
                    this->tabs->addTab(q_(s.spec.name));
                QObject::connect(this->tabs, &QTabBar::currentChanged, [this](int index) {
                    const std::vector<cluster::Snapshot> all = this->manager.list(true);
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
                    const std::vector<cluster::Snapshot> all = this->manager.list(true);
                    if (row >= 0 && static_cast<std::size_t>(row) < all.size()) this->sessionMenu(all[static_cast<std::size_t>(row)].spec.id, this->list->viewport()->mapToGlobal(pos));
                });
                this->list->setMinimumWidth(220);
                this->list->setWordWrap(true);
                this->list->setTextElideMode(Qt::ElideMiddle);
                this->list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                QObject::connect(this->list, &QListWidget::currentRowChanged, [this](int row) {
                    const std::vector<cluster::Snapshot> all = this->manager.list(true);
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
            this->input->setPlaceholderText("Prompt (Enter to send, Shift / Alt+Enter new line) · @ file · / command · ! shell · : command · Ctrl+K palette · F1 keys");
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
            this->status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // a long one cut, the web access kept
            // the bottom line: the status / the last notice on the left, the web access on the right
            QHBoxLayout* bottom = new QHBoxLayout();
            bottom->setContentsMargins(0, 0, 14, 0);
            bottom->addWidget(this->status, 1);
            bottom->addWidget(this->webLabel);
            root->addLayout(bottom);
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

            if (this->hub != nullptr) {
                this->hub->populate(sessions, st, this->active);
                const bool global = std::any_of(sessions.begin(), sessions.end(), [](const cluster::Snapshot& s) {return s.spec.id == GLOBAL_ID;});
                QString note;
                if (sessions.size() <= (global ? 1U : 0U)) note += "No sub-session yet: ask the global session, or create one (Ctrl+T). ";
                if (!global) note += "No global session (settings › Global session). ";
                this->hubNote->setText(esc_(note.toStdString()) + span_(st.muted, "drag · wheel · arrows · a / e zoom · two fingers · double click: whole"));
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
            const bool tasksShown = this->active == TASKS_ID && this->tasksView != nullptr;
            if (this->views.contains("") && this->tasksView != nullptr) this->views[""]->setVisible(!tasksShown);
            if (this->tasksView != nullptr) {
                if (tasksShown && !this->tasksView->isVisible()) QTimer::singleShot(0, this->tasksView, [view = this->tasksView]() {view->setFocus();}); // its keys at once
                this->tasksView->setVisible(tasksShown);
                if (tasksShown) this->fillTasks(this->tasksView);
            }
            if (this->tasksWindow != nullptr && this->tasksWindow->isVisible()) this->fillTasks(this->tasksBrowser);
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
    QObject::connect(this->log, &QTextBrowser::anchorClicked, [window, this](const QUrl& url) {
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
            + "</span>") + "&nbsp; <a href=\"cluster-terminal:\" style=\"color:" + q_(st.accent) + ";text-decoration:none;font-size:9pt\">terminal</a>"
            + (s.warning.empty() || cluster::now() - s.warningAt >= 60 ? QString() : "<br>" + span_(st.warn, "⚠ " + esc_(cluster::one_line(s.warning, 160))))
            + (cluster::resumable(s).empty() ? QString() : "&nbsp; <a href=\"cluster-resume:\" style=\"color:" + q_(st.warn) + ";text-decoration:none;font-weight:600\">"
                + (cluster::resumable(s) == "retry" ? QString("retry now") : QString("continue")) + "</a>"));
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
                for (const cluster::Json& round: turn["rounds"]) { // a line per request to the model, what it did, its pictures (a link opens one)
                    html += "<br>" + span_(round.value("current", false) ? st.accent : st.muted, esc_("  │ " + round.value("text", std::string())));
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
    this->panels->setVisible(!p.isEmpty());
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
        QTimer::singleShot(3000, &window, [&window, path]() {
            window.refresh(true);
            window.grab().save(path);
            QApplication::quit();
        });
    }
    return QApplication::exec();
}
