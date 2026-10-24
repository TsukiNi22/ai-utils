/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file TtyGraph.cpp

File Description:
##  The graph view of the terminal: a radial layout (the root in
##  the middle, the children around it), the lines in braille
##  dots, the nodes as boxes, a minimap, the keys and the mouse
\**************************************************************/

/* include */
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/TtyGraph.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <array>
#include <cmath>

namespace {

constexpr double PITCH = 24.0;          // the columns one node takes on its ring
constexpr double RING = 30.0;           // the least distance between two rings
constexpr double SQUASH = 0.85;         // the rings a little flat: the screens are wide
constexpr double ZOOM_MIN = 0.15;
constexpr double ZOOM_MAX = 6.0;
constexpr int DOUBLE_CLICK_MS = 450;
constexpr int CHROME = 3;               // the lines around the canvas: the title, the details, the keys
constexpr int MAP_WIDTH = 26;           // the minimap (borders included)
constexpr int MAP_HEIGHT = 7;

// The colours of a cell, by number (made into a colour at the end)
enum Paint {PaintNone, PaintText, PaintMuted, PaintAccent, PaintOk, PaintWarn, PaintError, PaintSurface, PaintBusy};

struct Cell {
    std::string glyph = " ";
    int fg = PaintNone;
    int bg = PaintNone;
    bool bold = false;
    bool dim = false;

    _nodiscard bool same(const Cell& other) const {return this->fg == other.fg && this->bg == other.bg && this->bold == other.bold && this->dim == other.dim;};
};

// The layer of braille dots (2 x 4 per cell)
struct Dots {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> mask;
    std::vector<int> paint;

    Dots(const int w, const int h): width(w), height(h), mask(static_cast<std::size_t>(std::max(0, w * h)), 0), paint(static_cast<std::size_t>(std::max(0, w * h)), PaintNone) {}

    void set(const int x, const int y, const int color)
    {
        if (x < 0 || y < 0 || x >= this->width * 2 || y >= this->height * 4) return;
        static const unsigned char bit[2][4] = {{0x01, 0x02, 0x04, 0x40}, {0x08, 0x10, 0x20, 0x80}};
        const std::size_t at = static_cast<std::size_t>(y / 4 * this->width + x / 2);
        this->mask[at] |= bit[x % 2][y % 4];
        this->paint[at] = color;
    }

    // Bresenham, the dots kept inside; dashed: three dots out of six
    void line(int x0, int y0, const int x1, const int y1, const int color, const bool dashed)
    {
        const int dx = std::abs(x1 - x0);
        const int dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        for (int step = 0;; ++step) {
            if (!dashed || step % 6 < 3) this->set(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            const int twice = 2 * error;
            if (twice >= dy) {
                error += dy;
                x0 += sx;
            }
            if (twice <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }
};

_cold std::string braille_(const unsigned char mask)
{
    const int code = 0x2800 + mask;
    std::string out;
    out += static_cast<char>(0xE0 | (code >> 12));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (code & 0x3F));
    return out;
}

// The glyphs (code points) of a text
_cold std::vector<std::string> glyphs_(const std::string& text)
{
    std::vector<std::string> list;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        const std::size_t len = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
        list.push_back(text.substr(i, len));
        i += len;
    }
    return list;
}

_cold std::string cut_(const std::string& text, const std::size_t width)
{
    const std::vector<std::string> list = glyphs_(text);
    if (list.size() <= width) return text;
    std::string out;
    for (std::size_t i = 0; i + 1 < width; ++i) out += list[i];
    return width == 0 ? std::string() : out + "…";
}

_cold std::string tokens_(const std::int64_t count)
{
    if (count >= 1000000) return std::to_string(count / 1000000) + "." + std::to_string(count / 100000 % 10) + "M";
    if (count >= 1000) return std::to_string(count / 1000) + "k";
    return std::to_string(count);
}

_cold std::string first_(const std::map<std::string, std::string>& keys, const std::string& action)
{
    const auto it = keys.find(action);
    if (it == keys.end() || it->second.empty()) return "";
    return cluster::split(it->second, '|').front();
}

_cold int paint_(const cluster::GraphTone tone)
{
    if (tone == cluster::GraphTone::Working) return PaintOk;
    if (tone == cluster::GraphTone::Waiting) return PaintWarn;
    if (tone == cluster::GraphTone::Busy) return PaintBusy;
    return tone == cluster::GraphTone::Error ? PaintError : PaintMuted;
}

_cold const char* glyph_(const cluster::GraphNode& node)
{
    if (node.kind == cluster::GraphKind::Global) return "◉";
    if (node.kind == cluster::GraphKind::Label) return "◇";
    return node.tone == cluster::GraphTone::Working || node.tone == cluster::GraphTone::Busy ? "●" : node.tone == cluster::GraphTone::Waiting ? "◆"
        : node.tone == cluster::GraphTone::Error ? "✖" : "○";
}

_cold bool token_(const std::string& token, const ftxui::Event& event, const std::string& name)
{
    // a key of the config: "Z", "Up", "Tab", "Shift+Tab", "Enter", "Ctrl+X"...
    if (token.empty()) return false;
    const std::string wanted = cluster::lower(token);
    if (!name.empty() && wanted == cluster::lower(name)) return true;
    if (wanted == "up") return event == ftxui::Event::ArrowUp;
    if (wanted == "down") return event == ftxui::Event::ArrowDown;
    if (wanted == "left") return event == ftxui::Event::ArrowLeft;
    if (wanted == "right") return event == ftxui::Event::ArrowRight;
    if (wanted == "enter") return event == ftxui::Event::Return;
    if (wanted == "home") return event == ftxui::Event::Home;
    if (wanted == "end") return event == ftxui::Event::End;
    if (wanted == "pageup") return event == ftxui::Event::PageUp;
    if (wanted == "pagedown") return event == ftxui::Event::PageDown;
    if (wanted == "space") return event == ftxui::Event::Character(' ');
    return event.is_character() && cluster::lower(event.character()) == wanted;
}

// What a label shows at a level (0 title + status, 1 title, 2 / 3 title shortened, 4 a dot), its size in cells; full: the selected one
struct Look {
    std::string name;
    std::string status;
    int width = 1;
    int height = 1;
};

_cold Look look_(const cluster::GraphNode& node, const int level, const bool full)
{
    Look look;
    if (level >= 4 && !full) return look;
    const std::size_t nameMax = full ? 30 : level <= 1 ? 20 : level == 2 ? 9 : 5;
    look.name = cut_(node.name, nameMax) + (node.count > 0 ? " (" + std::to_string(node.count) + ")" : "");
    look.width = static_cast<int>(glyphs_(look.name).size()) + 6;      // "│ ● name │"
    look.height = 3;
    if ((level == 0 || full) && !node.detail.empty()) {
        look.width = std::max(look.width, std::min(static_cast<int>(glyphs_(node.detail).size()) + 6, 28));
        look.status = cut_(node.detail, static_cast<std::size_t>(look.width - 4));
        look.height = 4;
    }
    return look;
}

} // namespace

/* layout */
_cold void cluster::GraphView::setNodes(std::vector<cluster::GraphNode> nodes)
{
    std::string signature;
    for (const cluster::GraphNode& node: nodes) signature += node.id + ">" + node.parent + ";";
    std::string labels;
    for (const cluster::GraphNode& node: nodes) labels += node.name + "\x1f" + node.detail + "\x1f" + std::to_string(node.count) + "\x1e";
    if (labels != this->_labelKey) {
        this->_labelKey = labels;
        this->_planValid = false;
    }
    this->_nodes = std::move(nodes);
    if (signature != this->_signature) {
        this->_signature = signature;
        this->layout_();
    }
    if (this->find_(this->_selected) < 0) this->_selected.clear();
}

_cold int cluster::GraphView::find_(const std::string& id) const
{
    for (std::size_t i = 0; i < this->_nodes.size(); ++i)
        if (this->_nodes[i].id == id) return static_cast<int>(i);
    return -1;
}

_cold void cluster::GraphView::layout_(void)
{
    // radial: the root in the middle (several roots: around a middle left empty), a node's children share its angle by their
    // leaves, a ring wide enough for the nodes it holds
    const std::size_t count = this->_nodes.size();
    this->_places.assign(count, Place{});
    std::map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < count; ++i) index[this->_nodes[i].id] = i;
    std::vector<std::vector<std::size_t>> kids(count);
    std::vector<std::size_t> roots;
    for (std::size_t i = 0; i < count; ++i) {
        const auto parent = index.find(this->_nodes[i].parent);
        if (parent == index.end() || parent->second == i) roots.push_back(i);
        else kids[parent->second].push_back(i);
    }
    std::vector<int> depth(count, 0);
    std::vector<int> leaves(count, 1);
    std::vector<int> perDepth(count + 2, 0);
    const bool single = roots.size() == 1;
    const std::function<void(std::size_t, int)> measure = [&](const std::size_t i, const int d) {
        depth[i] = d;
        ++perDepth[static_cast<std::size_t>(d)];
        int total = 0;
        for (const std::size_t kid: kids[i]) {
            measure(kid, d + 1);
            total += leaves[kid];
        }
        leaves[i] = std::max(1, total);
    };
    int total = 0;
    for (const std::size_t root: roots) {
        measure(root, single ? 0 : 1);
        total += leaves[root];
    }
    std::vector<double> radius(perDepth.size(), 0.0);
    for (std::size_t d = 1; d < radius.size(); ++d)
        radius[d] = perDepth[d] == 0 ? radius[d - 1] : std::max(radius[d - 1] + RING, perDepth[d] * PITCH / (2.0 * M_PI));
    const std::function<void(std::size_t, double, double)> place = [&](const std::size_t i, const double a0, const double a1) {
        const double mid = (a0 + a1) / 2.0;
        const double r = radius[static_cast<std::size_t>(depth[i])];
        this->_places[i] = {std::cos(mid) * r, std::sin(mid) * r * SQUASH};
        double at = a0;
        for (const std::size_t kid: kids[i]) {
            const double span = (a1 - a0) * leaves[kid] / leaves[i];
            place(kid, at, at + span);
            at += span;
        }
    };
    if (single) {
        place(roots.front(), -M_PI / 2.0, 3.0 * M_PI / 2.0);
    } else {
        double at = -M_PI / 2.0;
        for (const std::size_t root: roots) {
            const double span = 2.0 * M_PI * leaves[root] / std::max(1, total);
            place(root, at, at + span);
            at += span;
        }
    }
    this->_fitted = false;
    this->_planValid = false;
}

_cold void cluster::GraphView::plan_(void)
{
    // the label of every node is an obstacle: placed one after the other (global first), a label that meets a placed one is moved to a
    // free place around its node (below, above, right, left), else its status goes, then its title is shortened, last a dot; a sweep
    // on the boxes sorted by their left side keeps it cheap. Made once per zoom (the positions scale with it, the pan moves all)
    const std::size_t count = this->_nodes.size();
    this->_plan.assign(count, Plan{});
    const int start = this->_zoom < 0.45 ? 4 : this->_zoom < 0.8 ? 2 : this->_zoom < 1.2 ? 1 : 0;
    std::vector<std::size_t> order(count);
    for (std::size_t i = 0; i < count; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [this](const std::size_t a, const std::size_t b) {
        return this->_nodes[a].kind == GraphKind::Global && this->_nodes[b].kind != GraphKind::Global;
    });
    std::multimap<int, std::array<int, 4>> placed;     // left side -> left, right, top, bottom
    int widest = 1;
    for (const std::size_t i: order) {
        const int cx = static_cast<int>(std::floor(this->_places[i].x * this->_zoom));
        const int cy = static_cast<int>(std::floor(this->_places[i].y * this->_zoom / 2.0));
        for (int level = start; level <= 4; ++level) {
            const Look look = look_(this->_nodes[i], level, false);
            const int w = look.width;
            const int h = look.height;
            const int side = w / 2 + 1;
            const std::vector<std::pair<int, int>> shifts = {{0, 0}, {0, h}, {0, -h}, {side, 0}, {-side, 0}, {side, h}, {-side, h}, {side, -h}, {-side, -h}};
            bool done = false;
            for (const auto &[dx, dy]: shifts) {
                const int x0 = cx + dx - w / 2;
                const int y0 = cy + dy - h / 2;
                bool vacant = true;
                for (auto it = placed.lower_bound(x0 - widest - 1); vacant && it != placed.end() && it->first <= x0 + w; ++it) {
                    const std::array<int, 4>& r = it->second;
                    vacant = !(r[1] + 1 >= x0 && r[0] <= x0 + w && r[3] >= y0 && r[2] <= y0 + h - 1);   // a cell of margin on the sides
                }
                if (!vacant && level < 4) continue;
                this->_plan[i] = {level, dx, dy};
                if (level < 4) {
                    placed.insert({x0, {x0, x0 + w - 1, y0, y0 + h - 1}});
                    widest = std::max(widest, w);
                }
                done = true;
                break;
            }
            if (done) break;
        }
    }
    this->_planZoom = this->_zoom;
    this->_planValid = true;
}

_cold void cluster::GraphView::fit(void)
{
    if (this->_places.empty()) return;
    double minX = this->_places[0].x;
    double maxX = minX;
    double minY = this->_places[0].y;
    double maxY = minY;
    for (const Place& p: this->_places) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    const double zx = maxX - minX < 1.0 ? ZOOM_MAX : std::max(1, this->_width - 26) / (maxX - minX);
    const double zy = maxY - minY < 1.0 ? ZOOM_MAX : 2.0 * std::max(1, this->_height - 5) / (maxY - minY);
    this->_zoom = std::clamp(std::min(zx, zy), ZOOM_MIN, 2.0);
    this->_x = (minX + maxX) / 2.0;
    this->_y = (minY + maxY) / 2.0;
    this->_fitted = true;
}

/* view */
_cold void cluster::GraphView::zoomAt_(const double factor, const double px, const double py)
{
    const double zoom = std::clamp(this->_zoom * factor, ZOOM_MIN, ZOOM_MAX);
    if (zoom == this->_zoom) return;
    const double wx = (px - this->_width / 2.0) / this->_zoom + this->_x;     // the point under the pointer stays there
    const double wy = (py - this->_height / 2.0) * 2.0 / this->_zoom + this->_y;
    this->_zoom = zoom;
    this->_x = wx - (px - this->_width / 2.0) / zoom;
    this->_y = wy - (py - this->_height / 2.0) * 2.0 / zoom;
}

_cold void cluster::GraphView::pan_(const double columns, const double lines)
{
    this->_x += columns / this->_zoom;
    this->_y += lines * 2.0 / this->_zoom;
}

_cold void cluster::GraphView::center_(const std::size_t node)
{
    if (node >= this->_places.size()) return;
    this->_x = this->_places[node].x;
    this->_y = this->_places[node].y;
}

_cold void cluster::GraphView::step_(const int delta)
{
    const int count = static_cast<int>(this->_nodes.size());
    if (count == 0) return;
    const int now = this->find_(this->_selected);
    const int next = now < 0 ? (delta > 0 ? 0 : count - 1) : (now + delta + count) % count;
    this->_selected = this->_nodes[static_cast<std::size_t>(next)].id;
    const Place& p = this->_places[static_cast<std::size_t>(next)];
    const double sx = (p.x - this->_x) * this->_zoom + this->_width / 2.0;      // out of the view: brought to the middle
    const double sy = (p.y - this->_y) * this->_zoom / 2.0 + this->_height / 2.0;
    if (sx < 6 || sx > this->_width - 6 || sy < 2 || sy > this->_height - 2) this->center_(static_cast<std::size_t>(next));
}

/* input */
_cold std::string cluster::GraphView::action_(const ftxui::Event& event, const std::string& key, const std::map<std::string, std::string>& keys) const
{
    static const std::vector<std::string> actions = {"graph_up", "graph_down", "graph_left", "graph_right", "graph_zoom_in", "graph_zoom_out", "graph_fit",
        "graph_focus", "graph_next", "graph_prev", "graph_open", "graph_legend", "graph_map", "graph_close"};
    for (const std::string& action: actions) {
        const auto it = keys.find(action);
        if (it == keys.end()) continue;
        for (const std::string& token: cluster::split(it->second, '|'))
            if (token_(token, event, key)) return action;
    }
    return "";
}

_cold void cluster::GraphView::act_(const std::string& action, Result& result)
{
    const double columns = 6.0;
    const double lines = 3.0;
    if (action == "graph_up") this->pan_(0, -lines);
    else if (action == "graph_down") this->pan_(0, lines);
    else if (action == "graph_left") this->pan_(-columns, 0);
    else if (action == "graph_right") this->pan_(columns, 0);
    else if (action == "graph_zoom_in") this->zoomAt_(1.25, this->_width / 2.0, this->_height / 2.0);
    else if (action == "graph_zoom_out") this->zoomAt_(1 / 1.25, this->_width / 2.0, this->_height / 2.0);
    else if (action == "graph_fit") this->fit();
    else if (action == "graph_focus") {
        const int at = this->find_(this->_selected);
        if (at >= 0) {
            this->center_(static_cast<std::size_t>(at));
            this->_zoom = std::max(this->_zoom, 1.0);
        }
    } else if (action == "graph_next") this->step_(1);
    else if (action == "graph_prev") this->step_(-1);
    else if (action == "graph_open") {
        if (this->find_(this->_selected) >= 0 && this->onOpen) this->onOpen(this->_selected);
    } else if (action == "graph_legend") this->_legend = !this->_legend;
    else if (action == "graph_map") this->_map = !this->_map;
    else if (action == "graph_close") result = Result::Close;
}

_cold cluster::GraphView::Result cluster::GraphView::key(const ftxui::Event& event, const std::string& name, const std::map<std::string, std::string>& keys)
{
    const std::string action = this->action_(event, name, keys);
    if (action.empty()) return Result::Ignored;
    Result result = Result::Handled;
    this->act_(action, result);
    return result;
}

_cold int cluster::GraphView::hitAt_(const int x, const int y) const
{
    for (auto it = this->_hits.rbegin(); it != this->_hits.rend(); ++it)   // the last drawn is on top
        if (x >= it->box.x_min && x <= it->box.x_max && y >= it->box.y_min && y <= it->box.y_max) return static_cast<int>(it->node);
    return -1;
}

_cold bool cluster::GraphView::mouse(const ftxui::Mouse& mouse)
{
    const int x = mouse.x - this->_box.x_min;
    const int y = mouse.y - this->_box.y_min;
    const bool inside = x >= 0 && y >= 0 && x < this->_width && y < this->_height;
    if (mouse.button == ftxui::Mouse::WheelUp || mouse.button == ftxui::Mouse::WheelDown) {
        if (inside) this->zoomAt_(mouse.button == ftxui::Mouse::WheelUp ? 1.15 : 1 / 1.15, x + 0.5, y + 0.5);   // around the pointer
        return inside;
    }
    if (mouse.motion == ftxui::Mouse::Pressed) {
        if (!inside) return false;
        if (mouse.button == ftxui::Mouse::Left) {
            this->_pressed = true;
            this->_moved = false;
            this->_lastX = x;
            this->_lastY = y;
            this->_downHit = this->hitAt_(x, y);
        } else if (mouse.button == ftxui::Mouse::Right) {
            const int hit = this->hitAt_(x, y);
            if (hit >= 0) {
                this->_selected = this->_nodes[static_cast<std::size_t>(hit)].id;
                if (this->_nodes[static_cast<std::size_t>(hit)].kind == GraphKind::Session && this->onMenu) this->onMenu(this->_selected);
            }
        }
        return true;
    }
    if (mouse.motion == ftxui::Mouse::Moved && this->_pressed) {    // a drag: the graph follows the pointer
        if (x != this->_lastX || y != this->_lastY) {
            this->_moved = true;
            this->pan_(this->_lastX - x, this->_lastY - y);
            this->_lastX = x;
            this->_lastY = y;
        }
        return true;
    }
    if (mouse.motion == ftxui::Mouse::Released && this->_pressed) {
        this->_pressed = false;
        if (this->_moved) return true;
        if (this->_downHit < 0) {
            this->_selected.clear();
            this->_clickId.clear();
            return true;
        }
        const std::string id = this->_nodes[static_cast<std::size_t>(this->_downHit)].id;
        const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const bool twice = id == this->_clickId && now - this->_clickAt < DOUBLE_CLICK_MS;
        this->_selected = id;
        this->_clickId = twice ? std::string() : id;
        this->_clickAt = now;
        if (twice && this->onOpen) this->onOpen(id);
        return true;
    }
    return false;
}

/* drawing */
_cold ftxui::Element cluster::GraphView::render(const int width, const int height, const cluster::GraphColors& colors, const std::string& active, const std::map<std::string, std::string>& keys)
{
    const std::vector<ftxui::Color> palette = {ftxui::Color::Default, colors.text, colors.muted, colors.accent, colors.ok, colors.warn, colors.error, colors.surface, colors.busy};
    const int cols = std::max(20, width);
    const int rows = std::max(4, height - CHROME - (this->_legend ? 1 : 0));
    this->_width = cols;
    this->_height = rows;
    if (!this->_fitted) this->fit();
    const std::size_t count = this->_nodes.size();

    std::vector<Cell> cells(static_cast<std::size_t>(cols * rows));
    const std::function<Cell&(const int, const int)> at = [&](const int x, const int y) -> Cell& {return cells[static_cast<std::size_t>(y * cols + x)];};
    const std::function<bool(const int, const int)> inside = [&](const int x, const int y) {return x >= 0 && y >= 0 && x < cols && y < rows;};
    std::vector<std::pair<int, int>> centers(count);
    for (std::size_t i = 0; i < count; ++i) {
        centers[i] = {static_cast<int>(std::floor((this->_places[i].x - this->_x) * this->_zoom + cols / 2.0)),
            static_cast<int>(std::floor((this->_places[i].y - this->_y) * this->_zoom / 2.0 + rows / 2.0))};
    }

    // the links, as braille dots (the idle ones dashed)
    Dots dots(cols, rows);
    for (std::size_t i = 0; i < count; ++i) {
        const int parent = this->find_(this->_nodes[i].parent);
        if (parent < 0 || static_cast<std::size_t>(parent) == i) continue;
        const bool working = this->_nodes[i].tone == GraphTone::Working;
        const bool busy = this->_nodes[i].tone == GraphTone::Busy;     // its background runs: a solid link like a running one, purple
        dots.line(centers[static_cast<std::size_t>(parent)].first * 2 + 1, centers[static_cast<std::size_t>(parent)].second * 4 + 2, centers[i].first * 2 + 1, centers[i].second * 4 + 2,
            working ? PaintOk : busy ? PaintBusy : PaintMuted, !working && !busy);
    }
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x) {
            const std::size_t d = static_cast<std::size_t>(y * cols + x);
            if (dots.mask[d] == 0) continue;
            Cell& cell = at(x, y);
            cell.glyph = braille_(dots.mask[d]);
            cell.fg = dots.paint[d];
            cell.dim = dots.paint[d] == PaintMuted;
        }

    // the nodes: boxes placed by plan_ (title first, the status goes first when the room is short), the selected one last: on top, whole
    if (!this->_planValid || this->_planZoom != this->_zoom) this->plan_();
    this->_hits.clear();
    const std::function<void(const std::size_t)> draw = [&](const std::size_t i) {
        const GraphNode& node = this->_nodes[i];
        const bool selected = node.id == this->_selected;
        const bool here = node.id == active;
        const int tint = node.kind == GraphKind::Global || node.kind == GraphKind::Label ? PaintAccent : paint_(node.tone);
        const auto [cx, cy] = centers[i];
        const Plan plan = selected ? Plan{0, 0, 0} : this->_plan[i];
        if (plan.level >= 4) {    // far or crowded: a dot
            if (inside(cx, cy)) at(cx, cy) = {glyph_(node), tint, PaintNone, true, false};
            this->_hits.push_back({ftxui::Box{cx - 1, cx + 1, cy, cy}, i});
            return;
        }
        const Look look = look_(node, plan.level, selected);
        const int boxWidth = look.width;
        const int boxHeight = look.height;
        const int x0 = cx + plan.dx - boxWidth / 2;
        const int y0 = cy + plan.dy - boxHeight / 2;
        this->_hits.push_back({ftxui::Box{x0, x0 + boxWidth - 1, y0, y0 + boxHeight - 1}, i});
        if (x0 + boxWidth < 0 || x0 >= cols || y0 + boxHeight < 0 || y0 >= rows) return;
        const char* edge[2][3] = {{"╭", "─", "╮"}, {"┏", "━", "┓"}};
        const char* side[2] = {"│", "┃"};
        const char* low[2][3] = {{"╰", "─", "╯"}, {"┗", "━", "┛"}};
        const int style = selected ? 1 : 0;
        const std::function<void(const int, const int, const std::string&, const int, const bool)> put = [&](const int x, const int y, const std::string& glyph, const int fg, const bool bold) {
            if (!inside(x, y)) return;
            at(x, y) = {glyph, fg, PaintSurface, bold, false};
        };
        for (int row = 0; row < boxHeight; ++row) {    // every cell of the box is painted: nothing shows through it
            const bool top = row == 0;
            const bool bottom = row == boxHeight - 1;
            for (int k = 0; k < boxWidth; ++k) {
                const int part = k == 0 ? 0 : k == boxWidth - 1 ? 2 : 1;
                if (top) put(x0 + k, y0 + row, edge[style][part], tint, selected);
                else if (bottom) put(x0 + k, y0 + row, low[style][part], tint, selected);
                else put(x0 + k, y0 + row, part == 1 ? " " : side[style], part == 1 ? PaintText : tint, selected);
            }
        }
        put(x0 + 2, y0 + 1, glyph_(node), tint, true);
        const std::vector<std::string> letters = glyphs_(look.name);
        for (std::size_t k = 0; k < letters.size(); ++k)
            put(x0 + 4 + static_cast<int>(k), y0 + 1, letters[k], here ? PaintAccent : PaintText, here || selected || node.kind == GraphKind::Global);
        const std::vector<std::string> status = glyphs_(look.status);
        for (std::size_t k = 0; k < status.size(); ++k) put(x0 + 2 + static_cast<int>(k), y0 + 2, status[k], PaintMuted, false);
    };
    for (std::size_t i = 0; i < count; ++i)
        if (this->_nodes[i].id != this->_selected) draw(i);
    if (const int s = this->find_(this->_selected); s >= 0) draw(static_cast<std::size_t>(s));

    // the minimap: the nodes as dots, the view as a rectangle
    if (this->_map && cols >= 50 && rows >= 14 && count > 0) {
        const int innerWidth = MAP_WIDTH - 2;
        const int innerHeight = MAP_HEIGHT - 2;
        double minX = this->_x - cols / 2.0 / this->_zoom;
        double maxX = this->_x + cols / 2.0 / this->_zoom;
        double minY = this->_y - rows / this->_zoom;
        double maxY = this->_y + rows / this->_zoom;
        for (const Place& p: this->_places) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        const double scale = std::min((innerWidth * 2 - 1) / std::max(1.0, maxX - minX), (innerHeight * 4 - 1) / std::max(1.0, maxY - minY));
        const int offsetX = static_cast<int>((innerWidth * 2 - (maxX - minX) * scale) / 2);
        const int offsetY = static_cast<int>((innerHeight * 4 - (maxY - minY) * scale) / 2);
        const std::function<int(double)> mx = [&](const double wx) {return offsetX + static_cast<int>((wx - minX) * scale);};
        const std::function<int(double)> my = [&](const double wy) {return offsetY + static_cast<int>((wy - minY) * scale);};
        Dots map(innerWidth, innerHeight);
        const int left = mx(this->_x - cols / 2.0 / this->_zoom);
        const int right = mx(this->_x + cols / 2.0 / this->_zoom);
        const int top = my(this->_y - rows / this->_zoom);
        const int bottom = my(this->_y + rows / this->_zoom);
        map.line(left, top, right, top, PaintAccent, false);
        map.line(right, top, right, bottom, PaintAccent, false);
        map.line(right, bottom, left, bottom, PaintAccent, false);
        map.line(left, bottom, left, top, PaintAccent, false);
        for (std::size_t i = 0; i < count; ++i) {
            const int color = this->_nodes[i].kind == GraphKind::Global ? PaintAccent : paint_(this->_nodes[i].tone);
            const int dx = mx(this->_places[i].x);
            const int dy = my(this->_places[i].y);
            for (int a = 0; a < 2; ++a)
                for (int b = 0; b < 2; ++b) map.set(dx + a, dy + b, color);
        }
        const int x0 = cols - MAP_WIDTH - 1;
        const int y0 = rows - MAP_HEIGHT;
        for (int y = 0; y < MAP_HEIGHT; ++y)
            for (int x = 0; x < MAP_WIDTH; ++x) {
                const bool border = x == 0 || y == 0 || x == MAP_WIDTH - 1 || y == MAP_HEIGHT - 1;
                Cell cell = {" ", PaintMuted, PaintSurface, false, false};
                if (border) {
                    const bool l = x == 0;
                    const bool r = x == MAP_WIDTH - 1;
                    const bool t = y == 0;
                    const bool b = y == MAP_HEIGHT - 1;
                    cell.glyph = (t && l) ? "╭" : (t && r) ? "╮" : (b && l) ? "╰" : (b && r) ? "╯" : (t || b) ? "─" : "│";
                } else {
                    const std::size_t d = static_cast<std::size_t>((y - 1) * innerWidth + (x - 1));
                    if (map.mask[d] != 0) {
                        cell.glyph = braille_(map.mask[d]);
                        cell.fg = map.paint[d];
                    }
                }
                if (inside(x0 + x, y0 + y)) at(x0 + x, y0 + y) = cell;
            }
    }

    // the cells as runs of one style
    const std::function<ftxui::Element(const Cell*, const int)> line = [&](const Cell* row, const int length) {
        ftxui::Elements runs;
        int start = 0;
        for (int x = 1; x <= length; ++x) {
            if (x < length && row[x].same(row[start])) continue;
            std::string text;
            for (int k = start; k < x; ++k) text += row[k].glyph;
            ftxui::Element run = ftxui::text(text);
            if (row[start].fg != PaintNone) run = run | ftxui::color(palette[static_cast<std::size_t>(row[start].fg)]);
            if (row[start].bg != PaintNone) run = run | ftxui::bgcolor(palette[static_cast<std::size_t>(row[start].bg)]);
            if (row[start].bold) run = run | ftxui::bold;
            if (row[start].dim) run = run | ftxui::dim;
            runs.push_back(run);
            start = x;
        }
        return ftxui::hbox(runs);
    };
    ftxui::Elements canvas;
    for (int y = 0; y < rows; ++y) canvas.push_back(line(&cells[static_cast<std::size_t>(y * cols)], cols));

    // the title (the view), the legend, the selected node, the keys
    const std::size_t sessions = static_cast<std::size_t>(std::count_if(this->_nodes.begin(), this->_nodes.end(), [](const GraphNode& n) {return n.kind == GraphKind::Session;}));
    char zoomText[16];
    std::snprintf(zoomText, sizeof(zoomText), "%.2f", this->_zoom);
    const ftxui::Element head = ftxui::hbox({ftxui::text(" ◎ " + this->title + " ") | ftxui::bold | ftxui::color(colors.bg) | ftxui::bgcolor(colors.accent),
        ftxui::text(" " + std::to_string(sessions) + " session" + (sessions == 1 ? "" : "s") + " · zoom " + zoomText + "× · view " + std::to_string(static_cast<int>(this->_x))
            + ", " + std::to_string(static_cast<int>(this->_y))) | ftxui::color(colors.muted), ftxui::filler(),
        ftxui::text(first_(keys, "graph_legend") + " legend · " + first_(keys, "graph_map") + " map · " + first_(keys, "graph_close") + " close ") | ftxui::color(colors.muted)});
    std::string details = " click or Tab a node to select it · double click / Enter opens a session";
    if (const int s = this->find_(this->_selected); s >= 0) {
        const GraphNode& node = this->_nodes[static_cast<std::size_t>(s)];
        details = " " + node.name + " · " + node.detail + (node.info.empty() ? "" : " · " + node.info) + (node.tokens > 0 ? " · " + tokens_(node.tokens) + " tokens" : "")
            + (node.count > 0 ? " · " + std::to_string(node.count) + " under it" : "");
    }
    const std::string hint = " " + first_(keys, "graph_up") + " " + first_(keys, "graph_left") + " " + first_(keys, "graph_down") + " " + first_(keys, "graph_right") + " pan · "
        + first_(keys, "graph_zoom_in") + " / " + first_(keys, "graph_zoom_out") + " zoom · " + first_(keys, "graph_fit") + " fit · " + first_(keys, "graph_focus") + " focus · "
        + first_(keys, "graph_next") + " next · " + first_(keys, "graph_open") + " open · mouse: drag, wheel, click, double click, right click";
    ftxui::Elements all = {head};
    all.push_back(ftxui::vbox(canvas) | ftxui::reflect(this->_box));
    if (this->_legend)
        all.push_back(ftxui::hbox({ftxui::text(" ● working") | ftxui::color(colors.ok), ftxui::text("  ◆ waiting for you") | ftxui::color(colors.warn), ftxui::text("  ✖ error") | ftxui::color(colors.error), ftxui::text("  ● ~_~ background") | ftxui::color(colors.busy),
            ftxui::text("  ○ idle") | ftxui::color(colors.muted), ftxui::text("  ◉ global") | ftxui::color(colors.accent) | ftxui::bold,
            ftxui::text(cols >= 150 ? "  ── working link  ┄ idle link  (n) = sessions under a node  bold name = the session shown" : cols >= 100 ? "  ── working  ┄ idle link  (n) = sessions under" : "")
                | ftxui::color(colors.muted)}));  // the marks never squeezed: the explanation shortened, then dropped, when the width is short
    all.push_back(ftxui::text(cut_(details, static_cast<std::size_t>(cols))) | ftxui::color(colors.text));
    all.push_back(ftxui::text(cut_(hint, static_cast<std::size_t>(cols))) | ftxui::color(colors.muted) | ftxui::dim);
    return ftxui::vbox(all);
}
