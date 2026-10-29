/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file StashPopup.hpp

File Description:
##  The list of the stashed prompts of the terminal (Ctrl+L):
##  load one (it leaves the stash), remove one (undo), clear
##  all (two steps: No by default, then Yes)
\**************************************************************/

#ifndef CLUSTER_STASHPOPUP_H
    #define CLUSTER_STASHPOPUP_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "PromptStash.hpp"      // cluster::StashEntry
    #include "Editor.hpp"           // cluster::Editor
    #include "Popup.hpp"            // cluster::PopupColors, cluster::PopupPress
    #include "Types.hpp"            // cluster::Draft
    #include <optional>             // std::optional
    #include <map>                  // std::map
    #include <cstddef>              // std::size_t
    #include <utility>              // std::pair
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class StashAsk {
    None,
    Load,       // a prompt is being written: Replace (it goes to the stash) / Append / Cancel
    Clear,      // clear all: No (default) / Yes
};

//----------------------------------------------------------------//
/* STRUCT */

struct StashResult {
    bool close = false;
    std::string status;                         // told in the status line
    bool error = false;
    std::optional<cluster::Draft> load;         // a stashed prompt to put in the editor
    bool append = false;                        // at the end of the text of the editor (else: in its place)
    bool stashCurrent = false;                  // in its place: the text of the editor goes to the stash (never lost)
};

//----------------------------------------------------------------//
/* CLASS */

class StashPopup {
    private:
        bool _open = false;
        cluster::StashAsk _ask = cluster::StashAsk::None;
        int _choice = 0;                                    // the button of the question asked
        int _selected = 0;
        std::vector<cluster::StashEntry> _stash;            // the prompts of the session first (like its history), then the others; newest first
        std::vector<std::size_t> _index;                    // a row -> its index in stash_read()
        std::string _session;                               // the session the prompts are loaded into
        std::map<std::string, std::string> _names;          // session id -> its name (the session of each prompt shown by name)
        int _max = 10;
        std::string _note;                                  // last thing done (removed: u undoes it)
        std::optional<std::pair<std::size_t, cluster::StashEntry>> _undo;   // where it was in stash_read()

        // ---------- Pre-Function -------- //
        _cold void reload_(void);
        _cold _nodiscard cluster::StashResult keyAsk_(const ftxui::Event& event);
        _cold _nodiscard cluster::StashResult load_(const bool append, const bool replace);

        // ---------- Function -------- //
        _nodiscard inline std::size_t stored_(const int shown) const {return this->_index.at(static_cast<std::size_t>(shown));};  // a row -> its index in stash_read()

    public:
        // ---------- Pre-Function -------- //
        _cold void open(const std::string& session, const int max, const std::map<std::string, std::string>& names = {});
        _cold _nodiscard cluster::StashResult key(const ftxui::Event& event, const bool editorEmpty);
        _cold _nodiscard ftxui::Element render(const cluster::PopupColors& colors, const cluster::PopupPress& press) const;

        // ---------- Function -------- //
        _nodiscard inline bool active(void) const {return this->_open;};
        _cold inline void select(const int row)   {this->_selected = row;};
        _cold inline void close(void)             {this->_open = false; this->_ask = cluster::StashAsk::None;};

        // ---------- Constructor -------- //
        StashPopup(void) = default;

        // ----------- Destructor --------- //
        ~StashPopup() = default;
};

//----------------------------------------------------------------//
/* FUNCTION */

// a stashed prompt added at the end of the editor: its images and pasted texts are new blocks of it
_cold void append_draft(cluster::Editor& editor, const cluster::Draft& draft);

} // namespace end

#endif /* CLUSTER_STASHPOPUP_H */
