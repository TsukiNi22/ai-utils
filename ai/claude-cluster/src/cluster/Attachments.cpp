/**************************************************************\
Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Attachments.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Attachments.hpp"
#include "cluster/Tools.hpp"
#include <filesystem>
#include <functional>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <random>
#include <cstdio>

/* tools */
[[noreturn]] _cold static void refuse_(const std::string& why)
{
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, why);
}

_cold static std::string safe_session_(const std::string& session)
{
    std::string out;
    for (const char c: session)
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') out += c;
    if (out.empty() || out.size() != session.size()) refuse_("bad session id for an attachment");
    return out;
}

_cold static std::string safe_name_(const std::string& name)
{
    // the last component, letters / digits / . _ - only (no space: the name goes in an @path), never hidden, 80 characters
    std::string base = name.substr(name.find_last_of("/\\") == std::string::npos ? 0 : name.find_last_of("/\\") + 1);
    std::string out;
    for (const char c: base)
        out += std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-' ? c : '_';
    while (!out.empty() && out.front() == '.') out.erase(out.begin());
    if (out.size() > 80) out = out.substr(out.size() - 80);
    return out.empty() ? "file" : out;
}

_cold static std::string new_id_(void)
{
    static std::mt19937_64 engine{std::random_device{}()};
    static const char* digits = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 8; ++i) id += digits[engine() % 16];
    return id;
}

_cold static std::string image_extension_(const std::string& mime)
{
    return mime == "image/png" ? ".png" : mime == "image/jpeg" ? ".jpg" : mime == "image/gif" ? ".gif" : mime == "image/webp" ? ".webp" : "";
}

_cold static bool within_(const std::filesystem::path& file, const std::filesystem::path& folder)
{
    // component-wise: /home/me/x is not within /home/m
    auto it = file.begin();
    for (auto base = folder.begin(); base != folder.end(); ++base, ++it)
        if (it == file.end() || *it != *base) return false;
    return true;
}

_cold static std::string read_head_(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::string head(4096, '\0');
    file.read(head.data(), static_cast<std::streamsize>(head.size()));
    head.resize(static_cast<std::size_t>(file.gcount()));
    return head;
}

_cold static cluster::Attachment describe_(const std::filesystem::path& path)
{
    // an entry of the folder of a session: <id>-<name>
    std::error_code error;
    cluster::Attachment out;
    const std::string file = path.filename().string();
    const std::size_t dash = file.find('-');
    out.id = dash == std::string::npos ? file : file.substr(0, dash);
    out.name = dash == std::string::npos ? file : file.substr(dash + 1);
    out.path = path.string();
    out.linked = std::filesystem::is_symlink(path, error);
    out.size = std::filesystem::is_regular_file(path, error) ? static_cast<std::uint64_t>(std::filesystem::file_size(path, error)) : 0;
    out.mime = cluster::attach_sniff(read_head_(path));
    out.image = !image_extension_(out.mime).empty();
    return out;
}

_cold static void trim_kept_(const cluster::Config& config, const std::string& session)
{
    // the oldest beyond the limit go (the transcript keeps their name, not their thumbnail)
    if (config.attach.keep <= 0) return;
    std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> files;
    std::error_code error;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(cluster::attach_dir(config, session), error))
        files.push_back({std::filesystem::last_write_time(entry.path(), error), entry.path()});
    std::sort(files.begin(), files.end());
    for (std::size_t i = 0; files.size() > static_cast<std::size_t>(config.attach.keep) && i < files.size() - static_cast<std::size_t>(config.attach.keep); ++i)
        std::filesystem::remove(files[i].second, error);
}

_cold static std::uint64_t cap_(const cluster::Config& config)
{
    return static_cast<std::uint64_t>(std::max(1, config.attach.maxMb)) * 1024U * 1024U;
}

/* function */
_cold cluster::Json cluster::Attachment::json(void) const
{
    return {{"id", this->id}, {"name", this->name}, {"path", this->path}, {"mime", this->mime}, {"size", this->size}, {"image", this->image},
        {"linked", this->linked}, {"stripped", this->stripped}};
}

_cold std::filesystem::path cluster::attach_root(const cluster::Config& config)
{
    return config.attach.dir.empty() ? cluster::data_dir() / "attachments" : cluster::expand_home(config.attach.dir);
}

_cold std::filesystem::path cluster::attach_dir(const cluster::Config& config, const std::string& session)
{
    return cluster::attach_root(config) / safe_session_(session);
}

_cold std::string cluster::attach_sniff(const std::string& bytes)
{
    const std::function<bool(const char*, const std::size_t, const std::size_t)> starts = [&bytes](const char* magic, const std::size_t size, const std::size_t at) {
        return bytes.size() >= at + size && bytes.compare(at, size, magic, size) == 0;
    };
    if (starts("\x89PNG\r\n\x1a\n", 8, 0)) return "image/png";
    if (starts("\xff\xd8\xff", 3, 0)) return "image/jpeg";
    if (starts("GIF87a", 6, 0) || starts("GIF89a", 6, 0)) return "image/gif";
    if (starts("RIFF", 4, 0) && starts("WEBP", 4, 8)) return "image/webp";
    if (starts("%PDF-", 5, 0)) return "application/pdf";
    if (starts("PK\x03\x04", 4, 0)) return "application/zip";
    if (starts("\x1f\x8b", 2, 0)) return "application/gzip";
    if (starts("\x7f""ELF", 4, 0)) return "application/x-executable";
    if (starts("MZ", 2, 0)) return "application/x-dosexec";
    if (starts("\xcf\xfa\xed\xfe", 4, 0) || starts("\xca\xfe\xba\xbe", 4, 0)) return "application/x-executable";
    if (bytes.empty()) return "text/plain";
    // text: no NUL, no control character but the usual ones, in the first 4 KB
    const std::size_t end = std::min<std::size_t>(bytes.size(), 4096);
    for (std::size_t i = 0; i < end; ++i) {
        const unsigned char c = static_cast<unsigned char>(bytes[i]);
        if (c == 0 || (c < 32 && c != '\n' && c != '\r' && c != '\t' && c != '\f' && c != 27)) return "application/octet-stream";
    }
    const std::string head = cluster::lower(bytes.substr(0, 512));
    if (head.find("<svg") != std::string::npos) return "image/svg+xml";
    return "text/plain";
}

_cold cluster::Attachment cluster::attach_store(const cluster::Config& config, const std::string& session, const std::string& name, const std::string& bytes)
{
    if (!config.attach.enabled) refuse_("attachments are off (settings: Attachments)");
    if (bytes.empty()) refuse_("empty file: " + name);
    if (bytes.size() > cap_(config)) refuse_(name + " is too big (" + std::to_string(bytes.size() / 1048576) + " MB, the limit is " + std::to_string(config.attach.maxMb) + " MB)");
    const std::string mime = cluster::attach_sniff(bytes);
    if (mime == "application/x-executable" || mime == "application/x-dosexec") refuse_(name + ": a program is never attached");
    const std::filesystem::path dir = cluster::attach_dir(config, session);
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    ::chmod(dir.parent_path().c_str(), 0700);
    ::chmod(dir.c_str(), 0700);
    std::string safe = safe_name_(name);
    const std::string extension = image_extension_(mime);
    if (!extension.empty()) { // the extension follows the content (the agent reads the type from it)
        const std::size_t dot = safe.rfind('.');
        safe = (dot == std::string::npos ? safe : safe.substr(0, dot)) + extension;
    }
    const std::string id = new_id_();
    const std::filesystem::path path = dir / (id + "-" + safe);
    bool stripped = false;
    std::string data = bytes;
    if (!extension.empty() && mime != "image/gif" && (config.attach.stripExif || config.attach.maxPixels > 0)) {
        // magick: oriented (the EXIF rotation applied), stripped, bounded; kept as it is when magick is missing / fails
        const std::filesystem::path source = dir / ("." + id + "-in" + extension);
        const std::filesystem::path result = dir / ("." + id + "-out" + extension);
        std::ofstream(source, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        std::vector<std::string> args = {"magick", source.string(), "-auto-orient"};
        if (config.attach.maxPixels > 0) args.insert(args.end(), {"-resize", std::to_string(config.attach.maxPixels) + "x" + std::to_string(config.attach.maxPixels) + ">"});
        if (config.attach.stripExif) args.push_back("-strip");
        if (mime == "image/jpeg") args.insert(args.end(), {"-quality", "92"});
        args.push_back(result.string());
        const cluster::Captured run = cluster::capture(args, "", 60);
        std::ifstream converted(result, std::ios::binary);
        const std::string out((std::istreambuf_iterator<char>(converted)), std::istreambuf_iterator<char>());
        if (run.code == 0 && !out.empty() && cluster::attach_sniff(out) == mime) {
            data = out;
            stripped = config.attach.stripExif;
        }
        std::filesystem::remove(source, error);
        std::filesystem::remove(result, error);
    }
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!file) refuse_("cannot write " + path.string());
    }
    ::chmod(path.c_str(), 0600); // never executable
    trim_kept_(config, session);
    cluster::Attachment out = describe_(path);
    out.name = safe;
    out.stripped = stripped;
    return out;
}

_cold cluster::Attachment cluster::attach_link(const cluster::Config& config, const std::string& session, const std::string& path,
    const cluster::AttachScope scope, const std::string& cwd)
{
    if (!config.attach.enabled) refuse_("attachments are off (settings: Attachments)");
    if (scope == cluster::AttachScope::User && !config.attach.link) refuse_("linking a file is off (settings: Attachments > Link)");
    if (path.empty() || path.find('\0') != std::string::npos) refuse_("no path");
    std::error_code error;
    const std::filesystem::path target = std::filesystem::canonical(cluster::expand_home(path), error); // symlinks and .. resolved: what is checked is where it is
    if (error || !std::filesystem::is_regular_file(target, error)) refuse_("not a file: " + path);
    if (::access(target.c_str(), R_OK) != 0) refuse_("not readable: " + path);
    const std::uintmax_t size = std::filesystem::file_size(target, error);
    if (size == 0) refuse_("empty file: " + path);
    if (size > cap_(config)) refuse_(path + " is too big (the limit is " + std::to_string(config.attach.maxMb) + " MB)");
    // never the secrets of claude-cluster itself (its tokens, its config), nor an attachment of another session
    for (const std::filesystem::path& forbidden: {std::filesystem::weakly_canonical(cluster::config_dir(), error), std::filesystem::weakly_canonical(cluster::data_dir(), error)}) {
        const std::filesystem::path root = std::filesystem::weakly_canonical(cluster::attach_root(config), error);
        if (within_(target, forbidden) && !(within_(target, root) && within_(target, std::filesystem::weakly_canonical(cluster::attach_dir(config, session), error))))
            refuse_("a file of claude-cluster itself is not attached: " + path);
    }
    if (scope == cluster::AttachScope::Global) {
        const std::filesystem::path home = std::filesystem::weakly_canonical(cluster::expand_home("~"), error);
        const std::filesystem::path project = cwd.empty() ? std::filesystem::path() : std::filesystem::weakly_canonical(cluster::expand_home(cwd), error);
        const bool inProject = !project.empty() && within_(target, project);
        bool inHome = within_(target, home);
        if (inHome && !inProject) // the hidden folders of the home (.ssh, .gnupg, .aws, .config...) never
            for (auto it = std::next(target.begin(), static_cast<std::ptrdiff_t>(std::distance(home.begin(), home.end()))); it != target.end(); ++it)
                if (it->string().starts_with(".")) inHome = false;
        if (!inProject && !inHome) refuse_("the global session attaches the files of the project folder or of the home of the user only (settings: Attachments): " + path);
    }
    const std::filesystem::path dir = cluster::attach_dir(config, session);
    std::filesystem::create_directories(dir, error);
    ::chmod(dir.parent_path().c_str(), 0700);
    ::chmod(dir.c_str(), 0700);
    const std::string id = new_id_();
    std::string linkName = safe_name_(target.filename().string());
    if (const std::string extension = image_extension_(cluster::attach_sniff(read_head_(target))); !extension.empty()) { // the extension follows the content
        const std::size_t dot = linkName.rfind('.');
        linkName = (dot == std::string::npos ? linkName : linkName.substr(0, dot)) + extension;
    }
    const std::filesystem::path link = dir / (id + "-" + linkName);
    std::filesystem::create_symlink(target, link, error);
    if (error) refuse_("cannot link " + path + ": " + error.message());
    trim_kept_(config, session);
    cluster::Attachment out = describe_(link);
    if (out.mime == "application/x-executable" || out.mime == "application/x-dosexec") {
        std::filesystem::remove(link, error);
        refuse_(path + ": a program is never attached");
    }
    return out;
}

_cold std::vector<cluster::Attachment> cluster::attach_list(const cluster::Config& config, const std::string& session)
{
    std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> files;
    std::error_code error;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(cluster::attach_dir(config, session), error))
        if (!entry.path().filename().string().starts_with(".")) files.push_back({std::filesystem::last_write_time(entry.path(), error), entry.path()});
    std::sort(files.begin(), files.end());
    std::vector<cluster::Attachment> out;
    for (const std::pair<std::filesystem::file_time_type, std::filesystem::path>& file: files) out.push_back(describe_(file.second));
    return out;
}

_cold bool cluster::attach_remove(const cluster::Config& config, const std::string& session, const std::string& id)
{
    if (id.empty() || id.find_first_not_of("0123456789abcdef") != std::string::npos) return false;
    std::error_code error;
    bool removed = false;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(cluster::attach_dir(config, session), error))
        if (entry.path().filename().string().starts_with(id + "-")) removed = std::filesystem::remove(entry.path(), error) || removed; // a symlink: itself, never its target
    return removed;
}

_cold void cluster::attach_purge(const cluster::Config& config, const std::string& session)
{
    std::error_code error;
    std::filesystem::remove_all(cluster::attach_dir(config, session), error); // does not follow the links
}

_cold std::string cluster::attach_resolve(const cluster::Config& config, const std::string& session, const std::string& path)
{
    std::error_code error;
    const std::filesystem::path file = std::filesystem::path(path).lexically_normal();
    const std::filesystem::path dir = cluster::attach_dir(config, session).lexically_normal();
    if (!file.is_absolute() || file.parent_path() != dir) return "";
    if (file.filename().string().starts_with(".") || !std::filesystem::is_regular_file(file, error)) return "";
    return file.string();
}

_cold std::string cluster::attach_prompt(const std::string& text, const std::vector<cluster::Attachment>& list, std::vector<std::string>& images)
{
    std::string out = text;
    for (const cluster::Attachment& item: list) {
        out += std::string(out.empty() || out.back() == '\n' || out.back() == ' ' ? "" : " ");
        if (item.image) {
            images.push_back(item.path);
            out += "[Image #" + std::to_string(images.size()) + "] ";
        } else {
            out += "@" + item.path + " ";
        }
    }
    return cluster::trim(out);
}
