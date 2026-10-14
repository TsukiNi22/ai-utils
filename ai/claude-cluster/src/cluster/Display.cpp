/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file Display.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _SManip
#include <utils/utils.hpp>
#include "cluster/Display.hpp"
#include "cluster/Tools.hpp"
#include <filesystem>
#include <functional>
#include <algorithm>
#include <unistd.h>
#include <iterator>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <chrono>
#include <regex>
#include <ctime>
#include <array>
#include <map>
#include <set>

/* tools */

_cold static std::string read_(const std::filesystem::path& path, const std::uintmax_t max)
{
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "not readable: " + path.string());
    if (size > max) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, path.string() + ": too big (" + std::to_string(size / 1024) + " KB)");
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

_cold static std::string mime_(const std::string& bytes, const std::filesystem::path& path)
{
    if (bytes.starts_with("\x89PNG")) return "image/png";
    if (bytes.starts_with("\xFF\xD8")) return "image/jpeg";
    if (bytes.starts_with("GIF8")) return "image/gif";
    if (bytes.size() > 12 && bytes.substr(0, 4) == "RIFF" && bytes.substr(8, 4) == "WEBP") return "image/webp";
    if (path.extension() == ".svg" || bytes.find("<svg") != std::string::npos) return "image/svg+xml";
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "not an image (png, jpeg, gif, webp, svg): " + path.string());
}

_cold static std::filesystem::path temporary_(const std::string& name)
{
    static std::atomic<int> count{0};
    return std::filesystem::temp_directory_path() / ("claude-cluster-" + std::to_string(::getpid()) + "-" + std::to_string(++count) + "-" + name);
}

_cold static std::string dot_(const std::string& source, const std::string& format)
{
    // Graphviz: the source in a file, the drawing in another (its warnings apart)
    const std::filesystem::path in = temporary_("diagram.dot");
    const std::filesystem::path out = temporary_("diagram." + format);
    std::ofstream(in) << source;
    const cluster::Captured run = cluster::capture({"dot", "-T" + format, "-Gdpi=110", in.string(), "-o", out.string()}, "", 20);
    std::ifstream file(out, std::ios::binary);
    std::string drawing((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::error_code error;
    std::filesystem::remove(in, error);
    std::filesystem::remove(out, error);
    if (run.code != 0 || drawing.empty())
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "diagram: dot failed: " + cluster::one_line(run.out, 200));
    return drawing;
}

_cold static cluster::Json tree_(const std::filesystem::path& root, const int depth, std::size_t& count, const std::map<std::string, std::string>& git,
    const std::filesystem::path& base)
{
    // a folder: its folders first then its files (dot files left out), depth levels, 400 entries at most
    cluster::Json children = cluster::Json::array();
    std::error_code error;
    std::vector<std::filesystem::directory_entry> entries;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, error))
        if (!entry.path().filename().string().starts_with(".")) entries.push_back(entry);
    std::sort(entries.begin(), entries.end(), [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
        std::error_code e;
        if (a.is_directory(e) != b.is_directory(e)) return a.is_directory(e);
        return a.path().filename() < b.path().filename();
    });
    for (const std::filesystem::directory_entry& entry: entries) {
        if (++count > 400) break;
        const bool dir = entry.is_directory(error);
        const std::string relative = std::filesystem::relative(entry.path(), base, error).string();
        cluster::Json node = {{"name", entry.path().filename().string()}, {"dir", dir}};
        if (!dir) node["size"] = entry.file_size(error);
        if (git.contains(relative)) node["git"] = git.at(relative);
        if (dir && depth > 1) node["children"] = tree_(entry.path(), depth - 1, count, git, base);
        children.push_back(node);
    }
    return children;
}

_cold static void require_(const cluster::Json& display, const char* key)
{
    if (!display.contains(key)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, std::string("display: '") + key + "' missing");
}

/* functions */
_cold cluster::Json cluster::display_prepare(const cluster::Json& input)
{
    if (!input.is_object()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "display: an object {type, ...}");
    cluster::Json display = input;
    const std::string type = display.value("type", std::string());
    static const std::set<std::string> types = {"markdown", "table", "chart", "image", "diagram", "svg", "callout", "progress", "code", "diff", "json",
        "tree", "terminal", "checklist", "steps", "timeline", "kanban", "actions", "form", "details", "compare", "metrics", "heatmap", "audio", "math", "file"};
    if (!types.contains(type))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "display: unknown type '" + type + "' (" + cluster::display_help() + ")");
    if (type == "markdown" || type == "callout") require_(display, "text");
    if (type == "code") require_(display, "code");
    if (type == "table") {
        require_(display, "rows");
        if (!display["rows"].is_array()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "table: rows is a list of lists");
    }
    if (type == "chart") {
        require_(display, "series");
        if (!display["series"].is_array() || display["series"].empty())
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "chart: series is [{name, values: [numbers]}]");
        for (const cluster::Json& s: display["series"])
            if (!s.is_object() || !s.contains("values") || !s["values"].is_array())
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "chart: each series {name, values: [numbers]}");
        if (!display.contains("kind")) display["kind"] = "bar";
    }
    if (type == "progress") {
        require_(display, "value");
        if (!display.contains("max")) display["max"] = 100;
    }
    if (type == "image") {
        // a file read here (the browser and the window cannot reach it), or an image given as data
        if (display.contains("path")) {
            const std::filesystem::path path = cluster::expand_home(display.value("path", std::string()));
            const std::string bytes = read_(path, 4 * 1024 * 1024);
            display["mime"] = mime_(bytes, path);
            display["data"] = utils::smanip::codec::Base64Codec().encode(bytes);
        } else if (!display.contains("data")) {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "image: path (a file of this computer) or data (base64) and mime");
        }
        if (display.value("data", std::string()).size() > 6 * 1024 * 1024)
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "image: 4 MB at most");
    }
    if (type == "diagram") {
        // Graphviz DOT drawn here: an SVG (the browser), a PNG (the window, the terminal)
        require_(display, "dot");
        if (!cluster::has_command("dot")) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "diagram: Graphviz (dot) is not installed");
        const std::string source = display.value("dot", std::string());
        display["svg"] = dot_(source, "svg");
        display["png"] = utils::smanip::codec::Base64Codec().encode(dot_(source, "png"));
    }
    if (type == "svg") {
        require_(display, "svg");
        if (display.value("svg", std::string()).find("<svg") == std::string::npos)
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "svg: an <svg>...</svg> document");
        if (display.value("svg", std::string()).size() > 1024 * 1024) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "svg: 1 MB at most");
    }
    const std::function<void(const char*)> list = [&display](const char* key) {
        if (!display.contains(key) || !display[key].is_array())
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, display.value("type", std::string()) + ": '" + key + "' is a list");
    };
    if (type == "diff" && !display.contains("diff")) {
        // before / after: their unified diff made here
        require_(display, "before");
        require_(display, "after");
        const std::filesystem::path a = temporary_("before");
        const std::filesystem::path b = temporary_("after");
        std::ofstream(a) << display.value("before", std::string());
        std::ofstream(b) << display.value("after", std::string());
        display["diff"] = cluster::capture({"diff", "-u", "--label", display.value("file", std::string("before")), "--label",
            display.value("file", std::string("after")), a.string(), b.string()}, "", 10).out;
        std::error_code error;
        std::filesystem::remove(a, error);
        std::filesystem::remove(b, error);
    }
    if (type == "json") {
        require_(display, "data");
        if (display["data"].is_string()) { // a JSON text: read
            const cluster::Json parsed = cluster::Json::parse(display.value("data", std::string()), nullptr, false);
            if (!parsed.is_discarded()) display["data"] = parsed;
        }
    }
    if (type == "tree") {
        if (display.contains("path")) {
            const std::filesystem::path root = cluster::expand_home(display.value("path", std::string()));
            std::error_code error;
            if (!std::filesystem::is_directory(root, error))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "tree: not a folder: " + root.string());
            std::map<std::string, std::string> git;
            const cluster::Captured status = cluster::capture({"git", "-C", root.string(), "status", "--porcelain", "--untracked-files=all"}, "", 10);
            if (status.code == 0)
                for (const std::string& line: cluster::split(status.out, '\n'))
                    if (line.size() > 3) git[cluster::trim(line.substr(3))] = cluster::trim(line.substr(0, 2));
            std::size_t count = 0;
            display["root"] = {{"name", cluster::short_path(root.string())}, {"dir", true},
                {"children", tree_(root, std::clamp(display.value("depth", 3), 1, 8), count, git, root)}};
            if (count > 400) display["cut"] = true;
        } else {
            require_(display, "root");
        }
    }
    if (type == "terminal") {
        require_(display, "output");
        if (display.value("output", std::string()).size() > 200000)
            display["output"] = "[...]\n" + display.value("output", std::string()).substr(display.value("output", std::string()).size() - 200000);
    }
    if (type == "checklist") {
        list("items");
        for (cluster::Json& item: display["items"])
            if (item.is_string()) item = {{"text", item.get<std::string>()}, {"done", false}};
    }
    if (type == "steps") {
        list("steps");
        for (cluster::Json& step: display["steps"])
            if (step.is_string()) step = {{"text", step.get<std::string>()}, {"state", "todo"}};
    }
    if (type == "timeline") {
        list("tasks");
        // dates (YYYY-MM-DD) or numbers: numbers (days) for the drawing, the dates kept for the labels
        const std::function<double(const cluster::Json&)> day = [](const cluster::Json& v) -> double {
            if (v.is_number()) return v.get<double>();
            std::tm tm{};
            std::istringstream in(v.is_string() ? v.get<std::string>() : "");
            in >> std::get_time(&tm, "%Y-%m-%d");
            return in.fail() ? 0.0 : static_cast<double>(::timegm(&tm)) / 86400.0;
        };
        for (cluster::Json& task: display["tasks"]) {
            if (!task.is_object() || !task.contains("start") || !task.contains("end"))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "timeline: each task {name, start, end}");
            task["from"] = day(task["start"]);
            task["to"] = std::max(day(task["end"]), day(task["start"]));
        }
    }
    if (type == "kanban") {
        list("columns");
        for (cluster::Json& column: display["columns"]) {
            if (!column.contains("cards")) column["cards"] = cluster::Json::array();
            for (cluster::Json& card: column["cards"])
                if (card.is_object()) card = card.value("text", card.dump());
        }
    }
    if (type == "actions") {
        list("buttons");
        for (cluster::Json& button: display["buttons"])
            if (button.is_string()) button = {{"label", button.get<std::string>()}, {"prompt", button.get<std::string>()}};
    }
    if (type == "form") {
        list("fields");
        for (cluster::Json& field: display["fields"]) {
            if (field.is_string()) field = {{"name", field.get<std::string>()}};
            if (!field.contains("type")) field["type"] = field.contains("options") ? "choice" : "text";
            if (!field.contains("label")) field["label"] = field.value("name", std::string());
        }
        display["submitted"] = false;
    }
    if (type == "details") {
        require_(display, "summary");
        require_(display, "text");
    }
    if (type == "compare") {
        if (display.contains("before")) display["left"] = {{"title", "before"}, {"text", display.value("before", std::string())}};
        if (display.contains("after")) display["right"] = {{"title", "after"}, {"text", display.value("after", std::string())}};
        require_(display, "left");
        require_(display, "right");
    }
    if (type == "metrics") list("items");
    if (type == "heatmap") {
        if (display.contains("days") && display["days"].is_object()) {
            // a calendar: a column per week, a row per day (Monday first)
            double first = 1e18;
            double last = -1e18;
            std::map<long, double> values;
            for (const auto &[date, value]: display["days"].items()) {
                std::tm tm{};
                std::istringstream in(date);
                in >> std::get_time(&tm, "%Y-%m-%d");
                if (in.fail()) continue;
                const long day = static_cast<long>(::timegm(&tm) / 86400);
                values[day] = value.is_number() ? value.get<double>() : 0.0;
                first = std::min(first, static_cast<double>(day));
                last = std::max(last, static_cast<double>(day));
            }
            if (values.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "heatmap: days {\"YYYY-MM-DD\": value}");
            const long start = static_cast<long>(first) - ((static_cast<long>(first) + 3) % 7); // its Monday (1970-01-01: a Thursday)
            const long weeks = (static_cast<long>(last) - start) / 7 + 1;
            cluster::Json rows = cluster::Json::array();
            for (int d = 0; d < 7; ++d) {
                cluster::Json row = cluster::Json::array();
                for (long w = 0; w < weeks; ++w)
                    row.push_back(values.contains(start + w * 7 + d) ? cluster::Json(values[start + w * 7 + d]) : cluster::Json(nullptr));
                rows.push_back(row);
            }
            display["rows"] = rows;
            display["ylabels"] = {"Mon", "", "Wed", "", "Fri", "", "Sun"};
        }
        list("rows");
    }
    if (type == "audio") {
        require_(display, "path");
        const std::filesystem::path path = cluster::expand_home(display.value("path", std::string()));
        const std::string bytes = read_(path, 10 * 1024 * 1024);
        const std::string ext = path.extension().string();
        display["mime"] = ext == ".mp3" ? "audio/mpeg" : ext == ".ogg" || ext == ".opus" ? "audio/ogg" : ext == ".flac" ? "audio/flac" : "audio/wav";
        display["data"] = utils::smanip::codec::Base64Codec().encode(bytes);
        display.erase("path");
    }
    if (type == "file") {
        // its files: name, size, type; a preview for an image (3 MB at most) and the first page of a PDF (pdftoppm)
        std::vector<std::string> paths;
        if (display.contains("path") && display["path"].is_string()) paths.push_back(display.value("path", std::string()));
        if (display.contains("paths") && display["paths"].is_array())
            for (const cluster::Json& p: display["paths"])
                if (p.is_string()) paths.push_back(p.get<std::string>());
        const std::filesystem::path base = display.value("cwd", std::string()).empty() ? std::filesystem::current_path() : std::filesystem::path(display.value("cwd", std::string()));
        static const std::map<std::string, std::string> types_ = {{".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".gif", "image/gif"},
            {".webp", "image/webp"}, {".svg", "image/svg+xml"}, {".pdf", "application/pdf"}, {".zip", "application/zip"}, {".gz", "application/gzip"},
            {".tgz", "application/gzip"}, {".7z", "application/x-7z-compressed"}, {".apk", "application/vnd.android.package-archive"}, {".mp3", "audio/mpeg"},
            {".wav", "audio/wav"}, {".ogg", "audio/ogg"}, {".mp4", "video/mp4"}, {".webm", "video/webm"}, {".csv", "text/csv"}, {".html", "text/html"},
            {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"}, {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
            {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"}, {".txt", "text/plain"}, {".md", "text/markdown"}, {".json", "application/json"}};
        cluster::Json files = cluster::Json::array();
        for (const std::string& raw: paths) {
            std::filesystem::path path = cluster::expand_home(raw);
            if (path.is_relative()) path = base / path;
            std::error_code error;
            path = std::filesystem::weakly_canonical(path, error);
            if (!std::filesystem::is_regular_file(path, error)) continue;
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});
            const std::uintmax_t size = std::filesystem::file_size(path, error);
            cluster::Json file = {{"path", path.string()}, {"name", path.filename().string()}, {"size", size}, {"mime", types_.contains(ext) ? types_.at(ext) : "application/octet-stream"}};
            if (file.value("mime", std::string()).starts_with("image/") && size <= 3 * 1024 * 1024) {
                file["preview"] = utils::smanip::codec::Base64Codec().encode(read_(path, 3 * 1024 * 1024));
                file["preview_mime"] = file.value("mime", std::string());
            } else if (ext == ".pdf" && cluster::has_command("pdftoppm")) {
                const std::filesystem::path out = temporary_("page");
                if (cluster::capture({"pdftoppm", "-png", "-singlefile", "-f", "1", "-l", "1", "-scale-to", "360", path.string(), out.string()}, "", 15).code == 0) {
                    const std::filesystem::path png = out.string() + ".png";
                    if (std::filesystem::exists(png, error)) {
                        file["preview"] = utils::smanip::codec::Base64Codec().encode(read_(png, 4 * 1024 * 1024));
                        file["preview_mime"] = "image/png";
                        std::filesystem::remove(png, error);
                    }
                }
            }
            files.push_back(file);
        }
        if (files.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "file: no such file (path / paths)");
        display["files"] = files;
        display.erase("path");
        display.erase("paths");
        display.erase("cwd");
    }
    if (type == "math") {
        require_(display, "latex");
        display["unicode"] = cluster::display_math(display.value("latex", std::string()));
    }
    if (!display.contains("id")) { // its id: updated (session_display_update), clicked, ticked
        static std::atomic<unsigned> next{0};
        display["id"] = "d" + std::to_string(::time(nullptr) % 1000000) + "-" + std::to_string(++next);
    }
    if (display.dump().size() > 16 * 1024 * 1024) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "display: 8 MB at most");
    return display;
}

_cold std::string cluster::display_text(const cluster::Json& d)
{
    const std::string type = d.value("type", std::string());
    const std::string title = d.value("title", std::string());
    std::string text = title.empty() ? "" : title + "\n";
    if (type == "markdown" || type == "callout") return text + d.value("text", std::string());
    if (type == "code") return text + d.value("code", std::string());
    if (type == "table") {
        if (d.contains("columns")) {
            for (const cluster::Json& c: d["columns"])
                text += (c.is_string() ? c.get<std::string>() : c.dump()) + " | ";
            text += "\n";
        }
        for (const cluster::Json& row: d["rows"]) {
            for (const cluster::Json& c: row)
                text += (c.is_string() ? c.get<std::string>() : c.dump()) + " | ";
            text += "\n";
        }
        return text;
    }
    if (type == "chart") {
        text += "[" + d.value("kind", std::string("bar")) + " chart]";
        for (const cluster::Json& s: d["series"])
            text += " " + s.value("name", std::string()) + ": " + s["values"].dump();
        return text;
    }
    if (type == "progress") return text + "[progress " + d["value"].dump() + " / " + d["max"].dump() + "] " + d.value("label", std::string());
    if (type == "diagram") return text + "[diagram]\n" + d.value("dot", std::string());
    if (type == "diff") return text + d.value("diff", std::string());
    if (type == "json") return text + d["data"].dump(2);
    if (type == "terminal") return text + std::regex_replace(d.value("output", std::string()), std::regex("\x1b\\[[0-9;]*[A-Za-z]"), "");
    if (type == "math") return text + d.value("unicode", std::string());
    if (type == "file") {
        for (const cluster::Json& f: d.value("files", cluster::Json::array()))
            text += "[file] " + f.value("path", std::string()) + " (" + std::to_string(f.value("size", static_cast<std::uintmax_t>(0)) / 1024) + " KB)\n";
        return text;
    }
    if (type == "details") return text + d.value("summary", std::string()) + "\n" + d.value("text", std::string());
    if (type == "compare") return text + d["left"].value("title", std::string()) + ":\n" + d["left"].value("text", std::string()) + "\n"
        + d["right"].value("title", std::string()) + ":\n" + d["right"].value("text", std::string());
    const std::map<std::string, std::string> lists = {{"checklist", "items"}, {"steps", "steps"}, {"timeline", "tasks"}, {"kanban", "columns"},
        {"actions", "buttons"}, {"form", "fields"}, {"metrics", "items"}};
    if (lists.contains(type)) return text + "[" + type + "] " + d[lists.at(type)].dump() + (d.contains("values") ? " values: " + d["values"].dump() : "");
    return text + "[" + type + "] " + d.value("alt", d.value("caption", std::string()));
}

_cold std::vector<std::vector<cluster::Cell>> cluster::display_cells(const std::string& bytes, const int columns, const int rows)
{
    // chafa: the image in blocks of 24-bit colors; its escape sequences read back as runs (fg / bg / reverse)
    std::vector<std::vector<cluster::Cell>> lines;
    if (!cluster::has_command("chafa") || bytes.empty()) return lines;
    const std::filesystem::path in = temporary_(bytes.find("<svg") != std::string::npos ? "picture.svg" : "picture");
    std::ofstream(in, std::ios::binary) << bytes;
    const cluster::Captured out = cluster::capture({"chafa", "--format", "symbols", "--colors", "full", "--polite", "on", "--probe", "off", "--animate", "off",
        "--size", std::to_string(std::max(4, columns)) + "x" + std::to_string(std::max(2, rows)), in.string()}, "", 10);
    std::error_code error;
    std::filesystem::remove(in, error);
    if (out.code != 0) return lines;
    return cluster::ansi_cells(out.out);
}

_cold std::vector<std::vector<cluster::Cell>> cluster::ansi_cells(const std::string& text)
{
    // the escape sequences of colors (16, 256, 24-bit, reverse) read back as runs of one color, per line; the others dropped
    static const std::array<std::uint32_t, 16> base = {0x000000, 0xcd3131, 0x0dbc79, 0xe5e510, 0x2472c8, 0xbc3fbc, 0x11a8cd, 0xe5e5e5,
        0x666666, 0xf14c4c, 0x23d18b, 0xf5f543, 0x3b8eea, 0xd670d6, 0x29b8db, 0xffffff};
    const std::function<std::uint32_t(int)> palette = [](const int n) -> std::uint32_t {
        if (n < 16) return base[static_cast<std::size_t>(n)];
        if (n >= 232) {
            const std::uint32_t g = static_cast<std::uint32_t>(8 + (n - 232) * 10);
            return (g << 16) | (g << 8) | g;
        }
        const int c = n - 16;
        const std::function<std::uint32_t(int)> level = [](const int v) {return static_cast<std::uint32_t>(v == 0 ? 0 : 55 + v * 40);};
        return (level(c / 36) << 16) | (level(c / 6 % 6) << 8) | level(c % 6);
    };
    std::vector<std::vector<cluster::Cell>> lines;
    std::vector<cluster::Cell> line;
    cluster::Cell style;
    bool reverse = false;
    const std::function<void(void)> flush = [&]() {
        if (!style.text.empty()) {
            cluster::Cell cell = style;
            if (reverse) {
                std::swap(cell.fg, cell.bg);
                std::swap(cell.hasFg, cell.hasBg);
            }
            line.push_back(cell);
        }
        style.text.clear();
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') {
            std::size_t end = i + 2;
            while (end < text.size() && !std::isalpha(static_cast<unsigned char>(text[end]))) ++end;
            if (end >= text.size()) break;
            if (text[end] == 'm') {
                flush();
                std::vector<int> codes;
                for (const std::string& part: cluster::split(text.substr(i + 2, end - i - 2), ';'))
                    codes.push_back(part.empty() ? 0 : std::atoi(part.c_str()));
                if (codes.empty()) codes.push_back(0);
                for (std::size_t k = 0; k < codes.size(); ++k) {
                    const int code = codes[k];
                    if (code == 0) {
                        style.hasFg = style.hasBg = reverse = false;
                    } else if (code == 7) {
                        reverse = true;
                    } else if (code == 27) {
                        reverse = false;
                    } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97)) {
                        style.fg = base[static_cast<std::size_t>(code >= 90 ? code - 90 + 8 : code - 30)];
                        style.hasFg = true;
                    } else if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107)) {
                        style.bg = base[static_cast<std::size_t>(code >= 100 ? code - 100 + 8 : code - 40)];
                        style.hasBg = true;
                    } else if ((code == 38 || code == 48) && k + 2 < codes.size() && codes[k + 1] == 5) {
                        (code == 38 ? style.fg : style.bg) = palette(std::clamp(codes[k + 2], 0, 255));
                        (code == 38 ? style.hasFg : style.hasBg) = true;
                        k += 2;
                    } else if ((code == 38 || code == 48) && k + 4 < codes.size() && codes[k + 1] == 2) {
                        (code == 38 ? style.fg : style.bg) = (static_cast<std::uint32_t>(codes[k + 2] & 255) << 16) | (static_cast<std::uint32_t>(codes[k + 3] & 255) << 8)
                            | static_cast<std::uint32_t>(codes[k + 4] & 255);
                        (code == 38 ? style.hasFg : style.hasBg) = true;
                        k += 4;
                    } else if (code == 39) {
                        style.hasFg = false;
                    } else if (code == 49) {
                        style.hasBg = false;
                    }
                }
            }
            i = end;
            continue;
        }
        if (c == '\n') {
            flush();
            lines.push_back(line);
            line.clear();
            continue;
        }
        if (c != '\r') style.text += c;
    }
    flush();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

_cold std::string cluster::display_math(const std::string& latex)
{
    // LaTeX read as Unicode: Greek letters, operators, ^ and _ (digits, signs, some letters), \frac, \sqrt; braces dropped
    static const std::vector<std::pair<std::string, std::string>> words = {
        {"\\alpha", "α"}, {"\\beta", "β"}, {"\\gamma", "γ"}, {"\\delta", "δ"}, {"\\epsilon", "ε"}, {"\\varepsilon", "ε"}, {"\\zeta", "ζ"}, {"\\eta", "η"},
        {"\\theta", "θ"}, {"\\iota", "ι"}, {"\\kappa", "κ"}, {"\\lambda", "λ"}, {"\\mu", "μ"}, {"\\nu", "ν"}, {"\\xi", "ξ"}, {"\\pi", "π"}, {"\\rho", "ρ"},
        {"\\sigma", "σ"}, {"\\tau", "τ"}, {"\\phi", "φ"}, {"\\varphi", "φ"}, {"\\chi", "χ"}, {"\\psi", "ψ"}, {"\\omega", "ω"}, {"\\Gamma", "Γ"},
        {"\\Delta", "Δ"}, {"\\Theta", "Θ"}, {"\\Lambda", "Λ"}, {"\\Pi", "Π"}, {"\\Sigma", "Σ"}, {"\\Phi", "Φ"}, {"\\Psi", "Ψ"}, {"\\Omega", "Ω"},
        {"\\sum", "∑"}, {"\\prod", "∏"}, {"\\int", "∫"}, {"\\oint", "∮"}, {"\\partial", "∂"}, {"\\nabla", "∇"}, {"\\infty", "∞"}, {"\\leq", "≤"},
        {"\\le", "≤"}, {"\\geq", "≥"}, {"\\ge", "≥"}, {"\\neq", "≠"}, {"\\ne", "≠"}, {"\\approx", "≈"}, {"\\equiv", "≡"}, {"\\sim", "∼"},
        {"\\pm", "±"}, {"\\mp", "∓"}, {"\\times", "×"}, {"\\cdot", "·"}, {"\\div", "÷"}, {"\\to", "→"}, {"\\rightarrow", "→"}, {"\\leftarrow", "←"},
        {"\\Rightarrow", "⇒"}, {"\\Leftrightarrow", "⇔"}, {"\\implies", "⇒"}, {"\\iff", "⇔"}, {"\\in", "∈"}, {"\\notin", "∉"}, {"\\subset", "⊂"},
        {"\\subseteq", "⊆"}, {"\\cup", "∪"}, {"\\cap", "∩"}, {"\\emptyset", "∅"}, {"\\forall", "∀"}, {"\\exists", "∃"}, {"\\neg", "¬"},
        {"\\land", "∧"}, {"\\lor", "∨"}, {"\\langle", "⟨"}, {"\\rangle", "⟩"}, {"\\ldots", "…"}, {"\\cdots", "⋯"}, {"\\quad", "  "}, {"\\,", " "},
        {"\\left", ""}, {"\\right", ""}, {"\\mathbb{R}", "ℝ"}, {"\\mathbb{N}", "ℕ"}, {"\\mathbb{Z}", "ℤ"}, {"\\mathbb{Q}", "ℚ"}, {"\\mathbb{C}", "ℂ"}};
    static const std::map<char, std::string> up = {{'0', "⁰"}, {'1', "¹"}, {'2', "²"}, {'3', "³"}, {'4', "⁴"}, {'5', "⁵"}, {'6', "⁶"}, {'7', "⁷"},
        {'8', "⁸"}, {'9', "⁹"}, {'+', "⁺"}, {'-', "⁻"}, {'=', "⁼"}, {'(', "⁽"}, {')', "⁾"}, {'n', "ⁿ"}, {'i', "ⁱ"}, {'x', "ˣ"}, {'y', "ʸ"}, {'T', "ᵀ"}};
    static const std::map<char, std::string> down = {{'0', "₀"}, {'1', "₁"}, {'2', "₂"}, {'3', "₃"}, {'4', "₄"}, {'5', "₅"}, {'6', "₆"}, {'7', "₇"},
        {'8', "₈"}, {'9', "₉"}, {'+', "₊"}, {'-', "₋"}, {'=', "₌"}, {'(', "₍"}, {')', "₎"}, {'i', "ᵢ"}, {'j', "ⱼ"}, {'k', "ₖ"}, {'n', "ₙ"}, {'x', "ₓ"}};
    std::string s = latex;
    // \frac{a}{b} -> (a)/(b), \sqrt{x} -> √(x)
    s = std::regex_replace(s, std::regex(R"(\\frac\s*\{([^{}]*)\}\s*\{([^{}]*)\})"), "($1)/($2)");
    s = std::regex_replace(s, std::regex(R"(\\sqrt\s*\{([^{}]*)\})"), "√($1)");
    for (const auto &[word, symbol]: words)
        for (std::size_t at = s.find(word); at != std::string::npos; at = s.find(word, at + symbol.size())) {
            const std::size_t after = at + word.size();
            if (after < s.size() && std::isalpha(static_cast<unsigned char>(s[after])) && word.back() != '}') {
                at = after;
                continue;
            }
            s.replace(at, word.size(), symbol);
        }
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if ((s[i] == '^' || s[i] == '_') && i + 1 < s.size()) {
            const char mark = s[i];
            const std::map<char, std::string>& table = mark == '^' ? up : down;
            std::string group;
            if (s[i + 1] == '{') {
                const std::size_t close = s.find('}', i + 2);
                group = s.substr(i + 2, close == std::string::npos ? std::string::npos : close - i - 2);
                i = close == std::string::npos ? s.size() : close;
            } else {
                group = s.substr(i + 1, 1);
                ++i;
            }
            std::string mapped;
            bool all = true;
            for (const char g: group) {
                if (!table.contains(g)) all = false;
                else mapped += table.at(g);
            }
            out += all ? mapped : std::string(1, mark) + "(" + group + ")";
            continue;
        }
        if (s[i] == '{' || s[i] == '}' || s[i] == '$') continue;
        out += s[i];
    }
    return out;
}

_cold std::string cluster::display_help(void)
{
    return "markdown {text}; table {columns, rows: [[...]]}; chart {kind: bar | line | pie, labels, series: [{name, values}]}; "
        "image {path | data + mime, alt}; diagram {dot: Graphviz source}; svg {svg: a drawing, a logo}; callout {level: info | success | warning | error, text}; "
        "progress {value, max, label}; code {language, code}; diff {diff: unified | before + after, file}; json {data}; tree {path, depth | root}; "
        "terminal {output: with its ANSI colors, command}; checklist {items: [text | {text, done}]}; steps {steps: [text | {text, state: done | "
        "current | todo | error}]}; timeline {tasks: [{name, start, end}] (YYYY-MM-DD or numbers)}; kanban {columns: [{name, cards: [text]}]}; "
        "actions {buttons: [{label, prompt, session}]: clicked, the prompt sent to the session (default: the one shown)}; form {fields: [{name, label, "
        "type: text | number | choice | bool | date, options, value}], submit}: its values come back to you; details {summary, text}; compare "
        "{left: {title, text}, right: {...}} | {before, after}; metrics {items: [{label, value, unit, delta, trend: [numbers]}]}; heatmap {rows: [[n]], "
        "xlabels, ylabels} | {days: {YYYY-MM-DD: n}}; audio {path | text: spoken by the voice of claude-cluster}; math {latex}; file {path | paths: "
        "files of this computer to open / download (png, pdf, zip, apk...: a preview for images and PDFs)}; each with an "
        "optional title; an id given back (session_display_update)";
}

_cold std::string cluster::display_bytes(const cluster::Json& d)
{
    const std::string type = d.value("type", std::string());
    if (type == "image") return utils::smanip::codec::Base64Codec().decode(d.value("data", std::string()));
    if (type == "diagram") return utils::smanip::codec::Base64Codec().decode(d.value("png", std::string()));
    if (type == "svg") return d.value("svg", std::string());
    if (type == "audio") return utils::smanip::codec::Base64Codec().decode(d.value("data", std::string()));
    return "";
}

_cold std::string cluster::display_spark(const cluster::Json& values, const std::size_t width)
{
    static const std::array<const char*, 8> levels = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    if (!values.is_array() || values.empty()) return "";
    double low = 1e300;
    double high = -1e300;
    for (const cluster::Json& v: values)
        if (v.is_number()) {
            low = std::min(low, v.get<double>());
            high = std::max(high, v.get<double>());
        }
    std::string spark;
    const std::size_t step = std::max<std::size_t>(1, (values.size() + width - 1) / std::max<std::size_t>(1, width));
    for (std::size_t i = 0; i < values.size(); i += step)
        spark += levels[values[i].is_number() && high > low ? static_cast<std::size_t>((values[i].get<double>() - low) / (high - low) * 7.0 + 0.5) : 0];
    return spark;
}

_cold std::vector<std::pair<std::string, char>> cluster::display_json_lines(const cluster::Json& data, const int depth, const std::size_t max)
{
    std::vector<std::pair<std::string, char>> lines;
    const std::function<void(const std::string&, const cluster::Json&, const std::string&, int)> walk =
        [&](const std::string& key, const cluster::Json& value, const std::string& indent, const int left) {
            if (lines.size() >= max) return;
            const std::string head = indent + (key.empty() ? "" : key + ": ");
            if (value.is_object() || value.is_array()) {
                const bool object = value.is_object();
                if (left <= 0 || value.empty()) {
                    lines.emplace_back(head + (object ? "{" : "[") + (value.empty() ? "" : "… " + std::to_string(value.size())) + (object ? "}" : "]"), 'o');
                    return;
                }
                lines.emplace_back(head + (object ? "{" : "[") + "  (" + std::to_string(value.size()) + ")", 'k');
                std::size_t i = 0;
                for (const auto &[k, v]: value.items())
                    walk(object ? k : "[" + std::to_string(i++) + "]", v, indent + "  ", left - 1);
                lines.emplace_back(indent + (object ? "}" : "]"), 'o');
                return;
            }
            lines.emplace_back(head + value.dump(), value.is_string() ? 's' : value.is_number() ? 'n' : 'o');
        };
    walk("", data, "", depth);
    if (lines.size() >= max) lines.emplace_back("…", 'o');
    return lines;
}

_cold std::vector<std::pair<std::string, char>> cluster::display_tree_lines(const cluster::Json& root)
{
    std::vector<std::pair<std::string, char>> lines = {{root.value("name", std::string(".")) + "/", 'd'}};
    const std::function<void(const cluster::Json&, const std::string&)> walk = [&](const cluster::Json& children, const std::string& prefix) {
        for (std::size_t i = 0; i < children.size(); ++i) {
            const cluster::Json& node = children[i];
            const bool last = i + 1 == children.size();
            const bool dir = node.value("dir", false);
            const std::string git = node.value("git", std::string());
            std::string line = prefix + (last ? "└── " : "├── ") + node.value("name", std::string()) + (dir ? "/" : "");
            if (!git.empty()) line += "  " + git;
            lines.emplace_back(line, dir ? 'd' : git == "??" || git.find('A') != std::string::npos ? 'n' : git.empty() ? 'f' : 'm');
            if (dir && node.contains("children")) walk(node["children"], prefix + (last ? "    " : "│   "));
        }
    };
    if (root.contains("children")) walk(root["children"], "");
    return lines;
}

_cold std::string cluster::took(const std::int64_t ms)
{
    const std::int64_t sec = std::max<std::int64_t>(0, ms / 1000);
    if (sec < 60) return std::to_string(sec) + "s";
    if (sec < 3600) return std::to_string(sec / 60) + "m " + std::to_string(sec % 60) + "s";
    return std::to_string(sec / 3600) + "h " + std::to_string(sec / 60 % 60) + "m";
}

_cold cluster::Json cluster::turn_state(const cluster::Snapshot& s)
{
    const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    cluster::Json tools = cluster::Json::array();
    for (const cluster::ToolRun& run: s.tools)
        if (!run.done && run.time * 1000 >= s.turnSince - 2000) tools.push_back({{"name", run.name}, {"summary", run.summary}, {"ms", (cluster::now() - run.time) * 1000}});
    const std::string step = s.step;
    std::string verb = "Waiting for the model";
    if (step == "thinking" || !s.thinking.empty()) verb = "Thinking";
    else if (step == "writing" || !s.partial.empty()) verb = "Writing";
    else if (step == "tools") verb = tools.size() > 1 ? "Running " + std::to_string(tools.size()) + " tools" : tools.size() == 1 ? "Running " + tools[0].value("name", std::string()) : "Running a tool";
    else if (step == "requesting") verb = "Sending the request";
    else if (step == "reading") verb = "Reading the request";
    else if (step == "compacting") verb = "Compacting the context";
    else if (step == "retry") verb = "Retrying the API";
    else if (step == "hook") verb = "Running a hook";
    else if (!step.empty() && step != "model") verb = "Writing the input of " + step;
    const std::int64_t output = s.metrics.current.output;
    const std::int64_t estimate = static_cast<std::int64_t>(s.streamed / 4);
    std::string tokens;
    if (std::max(output, estimate) > 0)
        tokens = std::string(estimate > output ? "~" : "") + cluster::human_tokens(std::max(output, estimate)) + " tokens"
            + (s.thinkingTokens > 0 ? " (" + cluster::human_tokens(s.thinkingTokens) + " thinking)" : std::string());
    // the steps in words (model: the API awaited)
    static const std::map<std::string, std::string> words = {{"model", "waiting"}, {"requesting", "request"}, {"reading", "reading"}, {"compacting", "compacting"},
        {"retry", "retry"}, {"hook", "hook"}, {"tools", "tools"}, {"thinking", "thinking"}, {"writing", "writing"}};
    const std::function<std::string(const std::string&)> word = [](const std::string& name) {return words.contains(name) ? words.at(name) : "input of " + name;};
    cluster::Json steps = cluster::Json::array();
    for (const auto &[name, duration]: s.steps)
        steps.push_back({{"name", word(name)}, {"ms", duration}});
    // the rounds: one request to the model each (a new one at each request sent), told as one line
    struct Round {
        std::int64_t wait = 0;      // sent, read, awaited
        std::int64_t thinking = 0;
        std::int64_t writing = 0;
        std::int64_t running = 0;   // its tools running
        std::int64_t other = 0;     // hooks, retries, compacting
        std::vector<std::pair<std::string, int>> tools;  // their names, how many times
        std::vector<std::string> notes;                  // a retry, a hook, the context compacted
    };
    std::vector<Round> rounds;
    std::vector<std::pair<std::string, std::int64_t>> all = s.steps;
    if (!step.empty()) all.emplace_back(step, s.stepSince > 0 ? now - s.stepSince : 0);
    for (const auto &[name, duration]: all) {
        if (rounds.empty() || (name == "requesting" && (rounds.back().thinking + rounds.back().writing + rounds.back().running > 0 || !rounds.back().tools.empty())))
            rounds.emplace_back();
        Round& r = rounds.back();
        if (name == "model" || name == "requesting" || name == "reading") r.wait += duration;
        else if (name == "thinking") r.thinking += duration;
        else if (name == "writing") r.writing += duration;
        else if (name == "tools") r.running += duration;
        else if (name == "retry" || name == "hook" || name == "compacting") {
            r.other += duration;
            const std::string note = name == "retry" ? "API retried" : name == "hook" ? "a hook" : "context compacted";
            if (std::find(r.notes.begin(), r.notes.end(), note) == r.notes.end()) r.notes.push_back(note);
        } else { // the input of a tool written: the tool called
            std::string tool = name.starts_with("mcp__") ? name.substr(name.rfind("__") + 2) : name;
            auto it = std::find_if(r.tools.begin(), r.tools.end(), [&tool](const std::pair<std::string, int>& t) {return t.first == tool;});
            if (it == r.tools.end()) r.tools.emplace_back(tool, 1);
            else ++it->second;
            r.writing += duration;
        }
    }
    cluster::Json lines = cluster::Json::array();
    const std::size_t first = rounds.size() > 8 ? rounds.size() - 8 : 0;
    if (first > 0) lines.push_back({{"text", std::to_string(first) + " earlier round(s)"}, {"current", false}});
    for (std::size_t i = first; i < rounds.size(); ++i) {
        const Round& r = rounds[i];
        std::vector<std::string> parts;
        if (r.wait >= 1000) parts.push_back("wait " + cluster::took(r.wait));
        if (r.thinking > 0) parts.push_back("thinking " + cluster::took(r.thinking));
        if (r.tools.empty() && r.writing > 0) parts.push_back("writing " + cluster::took(r.writing));
        if (!r.tools.empty()) {
            std::string called;
            for (const auto &[tool, count]: r.tools)
                called += (called.empty() ? "" : ", ") + tool + (count > 1 ? " ×" + std::to_string(count) : "");
            parts.push_back(called + (r.running > 0 ? " (ran " + cluster::took(r.running) + ")" : ""));
        }
        for (const std::string& note: r.notes)
            parts.push_back(note);
        if (parts.empty()) parts.push_back(i + 1 == rounds.size() ? "waiting for the model" : "-");
        std::string text = "#" + std::to_string(i + 1);
        for (std::size_t k = 0; k < parts.size(); ++k)
            text += (k == 0 ? "  " : " · ") + parts[k];
        lines.push_back({{"text", text + (i + 1 == rounds.size() && !step.empty() ? " …" : "")}, {"current", i + 1 == rounds.size() && !step.empty()}});
    }
    return {{"verb", verb}, {"step", step.empty() ? std::string() : word(step)}, {"detail", s.stepDetail}, {"step_ms", s.stepSince > 0 ? now - s.stepSince : 0}, {"turn_ms", s.turnSince > 0 ? now - s.turnSince : 0},
        {"tokens", tokens}, {"tools", tools}, {"steps", steps}, {"rounds", lines}, {"hidden_thinking", (step == "thinking") && s.thinking.empty() && s.stepDetail.empty()}};
}
