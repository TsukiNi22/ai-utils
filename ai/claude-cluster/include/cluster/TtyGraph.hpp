/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file TtyGraph.hpp

File Description:
##  The graph view of the terminal (the hub of the web page and
##  of the window): the global session in the middle, the sessions
##  around it, drawn with braille lines and boxes, moved and
##  zoomed by the mouse or the keyboard
\**************************************************************/

#ifndef TTYGRAPH_H
    #define TTYGRAPH_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard
    #include <ftxui/component/event.hpp>    // ftxui::Event
    #include <ftxui/component/mouse.hpp>    // ftxui::Mouse
    #include <ftxui/screen/color.hpp>       // ftxui::Color
    #include <ftxui/dom/elements.hpp>       // ftxui::Element
    #include <ftxui/screen/box.hpp>         // ftxui::Box
    #include <functional>                   // std::function
    #include <cstdint>                      // std::int64_t
    #include <cstddef>                      // std::size_t
    #include <string>                       // std::string
    #include <vector>                       // std::vector
    #include <map>                          // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// The colours of the theme the graph is drawn with
struct GraphColors {
    ftxui::Color bg;
    ftxui::Color surface;
    ftxui::Color text;
    ftxui::Color muted;
    ftxui::Color accent;
    ftxui::Color ok;
    ftxui::Color warn;
    ftxui::Color error;
};

enum class GraphKind {Global, Session, Label};   // Label: a category / tag of the taxonomy
enum class GraphTone {Idle, Working, Waiting, Error};   // the colours of the sessions (muted, ok, warn, error)

// One node: what the core tells of a session (the model of the web hub: the parent links make the tree)
struct GraphNode {
    std::string id;
    std::string name;
    std::string detail;             // its state, in a word
    std::string info;               // its folder, what it does
    std::int64_t tokens = 0;
    GraphKind kind = GraphKind::Session;
    GraphTone tone = GraphTone::Idle;
    std::string parent;             // "" the root
    int count = 0;                  // a badge (the sessions under it), 0: none
};

//----------------------------------------------------------------//
/* CLASS */

class GraphView {
    public:
        enum class Result {Ignored, Handled, Close};

    private:
        struct Place {
            double x = 0.0;         // the world: x in columns, y in lines * 2 (a cell is twice as high as wide)
            double y = 0.0;
        };
        struct Hit {
            ftxui::Box box;         // where its node was drawn (relative to the canvas)
            std::size_t node = 0;
        };

        std::vector<GraphNode> _nodes;
        std::vector<Place> _places;
        std::string _signature;     // the ids and the parents: the layout is made again when it changes
        struct Plan {
            int level = 1;          // what of its label is shown: 0 title + status, 1 title, 2 / 3 title shortened, 4 a dot
            int dx = 0;             // its label moved off its node (cells), when the place under it is taken
            int dy = 0;
        };

        std::vector<Hit> _hits;
        std::vector<Plan> _plan;    // the labels placed (made again when the nodes or the zoom change)
        double _planZoom = 0.0;
        bool _planValid = false;
        std::string _labelKey;      // the names, details and badges: the plan is made again when they change
        ftxui::Box _box{0, 0, 0, 0};    // the canvas on the screen (the mouse is given on the screen)
        int _width = 80;
        int _height = 20;
        double _x = 0.0;            // the middle of the view in the world
        double _y = 0.0;
        double _zoom = 1.0;
        std::string _selected;
        bool _legend = false;
        bool _map = true;
        bool _fitted = false;
        bool _pressed = false;
        bool _moved = false;
        int _downHit = -1;          // the node under the press (a click selects it at the release)
        int _lastX = 0;
        int _lastY = 0;
        std::string _clickId;       // the last node clicked and when: a second click is a double click
        std::int64_t _clickAt = 0;

        void layout_(void);
        void plan_(void);
        _nodiscard int find_(const std::string& id) const;
        _nodiscard int hitAt_(const int x, const int y) const;
        void zoomAt_(const double factor, const double px, const double py);
        void pan_(const double columns, const double lines);
        void center_(const std::size_t node);
        void step_(const int delta);
        void act_(const std::string& action, Result& result);
        _nodiscard std::string action_(const ftxui::Event& event, const std::string& key, const std::map<std::string, std::string>& keys) const;

    public:
        std::function<void(const std::string&)> onOpen;     // a session opened (Enter, a double click)
        std::function<void(const std::string&)> onMenu;     // a session right clicked
        std::string title = "Graph";                        // the view it draws (Graph: the taxonomy between; Hub: every session around)

        // The nodes to draw (the view kept where it is; the layout made again when the tree changed)
        void setNodes(std::vector<GraphNode> nodes);

        // The whole graph in the view
        void fit(void);

        // A key: moved, zoomed, selected, opened... (the keys of the config, [keys] graph_*)
        _nodiscard Result key(const ftxui::Event& event, const std::string& name, const std::map<std::string, std::string>& keys);

        // The mouse (on the screen): drag, wheel, click, double click, right click
        _nodiscard bool mouse(const ftxui::Mouse& mouse);

        // The view of that size; active: the session shown in the other views (marked)
        _nodiscard ftxui::Element render(const int width, const int height, const GraphColors& colors, const std::string& active, const std::map<std::string, std::string>& keys);

        _nodiscard const std::string& selected(void) const {return this->_selected;};
};

} // namespace end

#endif /* TTYGRAPH_H */
