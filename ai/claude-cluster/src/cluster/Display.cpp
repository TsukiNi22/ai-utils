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
#include <unistd.h>
#include <iterator>
#include <cstring>
#include <fstream>
#include <sstream>
#include <atomic>
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
    static const std::set<std::string> types = {"markdown", "table", "chart", "image", "diagram", "svg", "callout", "progress", "code"};
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
    if (display.dump().size() > 8 * 1024 * 1024) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "display: 8 MB at most");
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
    for (std::size_t i = 0; i < out.out.size(); ++i) {
        const char c = out.out[i];
        if (c == '\x1b' && i + 1 < out.out.size() && out.out[i + 1] == '[') {
            std::size_t end = i + 2;
            while (end < out.out.size() && !std::isalpha(static_cast<unsigned char>(out.out[end]))) ++end;
            if (end >= out.out.size()) break;
            if (out.out[end] == 'm') {
                flush();
                std::vector<int> codes;
                for (const std::string& part: cluster::split(out.out.substr(i + 2, end - i - 2), ';'))
                    codes.push_back(part.empty() ? 0 : std::atoi(part.c_str()));
                for (std::size_t k = 0; k < codes.size(); ++k) {
                    if (codes[k] == 0) {
                        style.hasFg = style.hasBg = reverse = false;
                    } else if (codes[k] == 7) {
                        reverse = true;
                    } else if ((codes[k] == 38 || codes[k] == 48) && k + 4 < codes.size() && codes[k + 1] == 2) {
                        const std::uint32_t rgb = (static_cast<std::uint32_t>(codes[k + 2]) << 16) | (static_cast<std::uint32_t>(codes[k + 3]) << 8)
                            | static_cast<std::uint32_t>(codes[k + 4]);
                        if (codes[k] == 38) {
                            style.fg = rgb;
                            style.hasFg = true;
                        } else {
                            style.bg = rgb;
                            style.hasBg = true;
                        }
                        k += 4;
                    } else if (codes[k] == 39) {
                        style.hasFg = false;
                    } else if (codes[k] == 49) {
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
        style.text += c;
    }
    flush();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

_cold std::string cluster::display_help(void)
{
    return "markdown {text}; table {columns, rows: [[...]]}; chart {kind: bar | line | pie, labels, series: [{name, values}]}; "
        "image {path | data + mime, alt}; diagram {dot: Graphviz source}; svg {svg: a drawing, a logo}; callout {level: info | success | warning | error, text}; "
        "progress {value, max, label}; code {language, code}; each with an optional title";
}

_cold std::string cluster::display_bytes(const cluster::Json& d)
{
    const std::string type = d.value("type", std::string());
    if (type == "image") return utils::smanip::codec::Base64Codec().decode(d.value("data", std::string()));
    if (type == "diagram") return utils::smanip::codec::Base64Codec().decode(d.value("png", std::string()));
    if (type == "svg") return d.value("svg", std::string());
    return "";
}
