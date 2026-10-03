{{HEADER}}

#ifndef {{GUARD}}TYPE_H
    #define {{GUARD}}TYPE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include <vector>   // std::vector
    #include <string>   // std::string

namespace {{NAMESPACE}} { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct {{NAME}}Entry {
    std::string name; // empty -> nothing
    bool enabled = true; // default value set at the declaration
    std::vector<std::pair<std::string, bool>> ids; // <id, mandatory>
    std::string description = "[None]";
};

using {{NAME}}Entries = std::vector<{{NAMESPACE}}::{{NAME}}Entry>;

} // namespace end
#endif /* {{GUARD}}TYPE_H */
