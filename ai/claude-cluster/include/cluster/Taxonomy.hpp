/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Taxonomy.hpp

File Description:
##  The taxonomy of the labels of the sessions: categories and tags form a forest (a node has an optional parent that is
##  another label, none: it hangs under the root "global"); a session hangs under its primary label (its category, else its
##  first tag, else the root). Pure data: no manager, no file (the manager keeps taxonomy.json)
\**************************************************************/

#ifndef TAXONOMY_H
    #define TAXONOMY_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* project */
    #include "Types.hpp"                    // cluster::Json, cluster::SessionSpec

    /* type */
    #include <map>                          // std::map
    #include <set>                          // std::set
    #include <string>                       // std::string
    #include <vector>                       // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class Taxonomy {
    private:
        std::map<std::string, std::string> _parents;    // node -> its parent ("": under the root); a label of a session missing here is under the root

    public:
        // ---------- Function -------- //
        _cold _nodiscard static cluster::Taxonomy from_json(const cluster::Json& json); // {"parents": {node: parent}}; cycles and bad names dropped
        _cold _nodiscard cluster::Json json(void) const;

        _cold _nodiscard static std::string root(void);                                 // "global"
        _cold _nodiscard static std::string clean(const std::string& name);             // a node name: a cleaned label; "global" and "": the root ("")
        _cold _nodiscard bool known(const std::string& node) const;                     // it has an entry (a parent set, or a child)
        _cold _nodiscard std::string parent(const std::string& node) const;             // "": the root
        _cold _nodiscard std::vector<std::string> path(const std::string& node) const;  // from the top to the node included (a node under the root: itself alone)
        _cold _nodiscard std::vector<std::string> children(const std::string& node) const;           // the sorted nodes with this parent ("": the ones under the root)
        _cold _nodiscard std::set<std::string> descendants(const std::string& node) const;           // every node below it (itself not)
        _cold _nodiscard std::vector<std::string> nodes(void) const;                    // every node with an entry, sorted

        // the group of a session: its category, else its first tag, else "" (the root)
        _cold _nodiscard static std::string primary(const cluster::SessionSpec& spec);
        _cold _nodiscard std::vector<std::string> group(const cluster::SessionSpec& spec) const;     // the path of its primary label (empty: under the root)
        _cold _nodiscard std::set<std::string> scope(const std::string& node, const bool descendants) const;   // the node, and (descendants) every node below it
        _cold _nodiscard static std::size_t common_prefix(const std::vector<std::string>& a, const std::vector<std::string>& b);   // the group heads shared by two paths
        // the session has this label, or (descendants) a label below it
        _cold _nodiscard bool under(const cluster::SessionSpec& spec, const std::string& node, const bool descendants) const;

        // the changes: false and why (a cycle, itself as parent, an unknown or empty name...)
        _cold bool setParent(const std::string& node, const std::string& parent, std::string& why);
        _cold bool rename(const std::string& from, const std::string& to, std::string& why);         // its children follow it; a name already used: refused (merge)
        _cold bool remove(const std::string& node, std::string& why);                                // its children go to its parent
        _cold bool merge(const std::string& from, const std::string& into, std::string& why);        // from disappears, its children go under into
};

} // namespace end
#endif /* TAXONOMY_H */
