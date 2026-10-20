/**************************************************************\
Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Attachments.hpp

File Description:
##  Files, images and photos attached to a prompt (window, web
##  page, app, MCP): stored per session in the data folder (or a
##  symlink to a file of this computer), checked (size, type,
##  path), images downscaled / stripped of their EXIF
\**************************************************************/

#ifndef ATTACHMENTS_H
    #define ATTACHMENTS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Config.hpp"           // cluster::Config
    #include "Types.hpp"            // cluster::Json
    #include <filesystem>           // std::filesystem::path
    #include <cstdint>              // std::uint64_t
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class AttachScope {
    User,       // the user chose it (window, page, terminal): any file they can read
    Global,     // the global session (MCP): the project folder and the home of the user only
};

//----------------------------------------------------------------//
/* STRUCT */

struct Attachment {
    std::string id;
    std::string name;       // as shown
    std::string path;       // the file (stored here, or the symlink to the file linked)
    std::string mime;       // sniffed from the content, never from the name
    std::uint64_t size = 0;
    bool image = false;     // png / jpeg / gif / webp: given to the agent as an image block
    bool linked = false;    // a symlink to a file of this computer (nothing copied)
    bool stripped = false;  // re-encoded without its EXIF (images)

    _cold _nodiscard cluster::Json json(void) const;
};

//----------------------------------------------------------------//
/* FUNCTION */

// the folder of the attachments (settings: stored where), and the one of a session
_cold _nodiscard std::filesystem::path attach_root(const cluster::Config& config);
_cold _nodiscard std::filesystem::path attach_dir(const cluster::Config& config, const std::string& session);

// the type of a content (magic numbers, text or binary): "image/png", "application/pdf", "text/plain"... ("" unknown binary)
_cold _nodiscard std::string attach_sniff(const std::string& bytes);

// stores the bytes of a file for a session: size / count / type checked, the name made safe, the image downscaled and
// stripped (settings); throws a message for the user when it is refused
_cold _nodiscard cluster::Attachment attach_store(const cluster::Config& config, const std::string& session, const std::string& name, const std::string& bytes);

// links a file of this computer (a symlink in the folder of the session): checked by its scope
_cold _nodiscard cluster::Attachment attach_link(const cluster::Config& config, const std::string& session, const std::string& path,
    const cluster::AttachScope scope, const std::string& cwd = "");

_cold _nodiscard std::vector<cluster::Attachment> attach_list(const cluster::Config& config, const std::string& session);
_cold bool attach_remove(const cluster::Config& config, const std::string& session, const std::string& id);   // the link only, never its target
_cold void attach_purge(const cluster::Config& config, const std::string& session);                           // a session deleted for good

// a path of a prompt / transcript as an attachment of THIS session (never another one), else nothing
_cold _nodiscard std::string attach_resolve(const cluster::Config& config, const std::string& session, const std::string& path);

// the marks of a prompt: its images as "[Image #N]" (in the order of `images`), its files as "@path"; the text with them appended
_cold _nodiscard std::string attach_prompt(const std::string& text, const std::vector<cluster::Attachment>& list, std::vector<std::string>& images);

} // namespace end

#endif /* ATTACHMENTS_H */
