/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Checks.hpp

File Description:
##  Prototypes of the checks (one file per family) and the issue helpers
\**************************************************************/

#ifndef CHECKS_H
    #define CHECKS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include "../XStyleType.hpp"    // xstyle::Issue, xstyle::Fix, xstyle::ProjectInfo
    #include "../SourceFile.hpp"    // xstyle::SourceFile
    #include <string_view>          // std::string_view
    #include <optional>             // std::optional
    #include <string>               // std::string
    #include <vector>               // std::vector

    //----------------------------------------------------------------//
    /* DEFINE */

    #define PREVIEW_LINES 6 // Lines of a fix shown in the report

namespace xstyle::check { // namespace start
//----------------------------------------------------------------//
/* TYPEDEF */

using Issues = std::vector<xstyle::Issue>;

//----------------------------------------------------------------//
/* STRUCT */

struct TypeGuess {
    std::string type; // empty = unknown
    bool certain = false; // false: guessed from the usage / the name, can be wrong
};

//----------------------------------------------------------------//
/* PROTOTYPE */

/* checks */
_hot void generic(const xstyle::SourceFile& file, xstyle::check::Issues& issues);
_hot void cpp(const xstyle::SourceFile& file, const xstyle::ProjectInfo& project, xstyle::check::Issues& issues);
_hot void libutils(const xstyle::SourceFile& file, const xstyle::ProjectInfo& project, xstyle::check::Issues& issues);
_hot void python(const xstyle::SourceFile& file, xstyle::check::Issues& issues);
_hot void shell(const xstyle::SourceFile& file, xstyle::check::Issues& issues);
_hot void rust(const xstyle::SourceFile& file, xstyle::check::Issues& issues);
_hot void cmake(const xstyle::SourceFile& file, xstyle::check::Issues& issues);
_hot void comments(const xstyle::SourceFile& file, xstyle::check::Issues& issues); // cpp-comments structure

/* python typing (check/typing.cpp), on the masked code */
_hot _nodiscard xstyle::check::TypeGuess python_expression_type(const std::string& expression);
_hot _nodiscard xstyle::check::TypeGuess python_return_type(const std::vector<std::string>& code, const std::size_t def, const std::string& inlineBody, const bool future);
_hot _nodiscard xstyle::check::TypeGuess python_parameter_type(const std::vector<std::string>& code, const std::size_t def, const std::string& name,
    const std::string& value); // value: default value (empty: none)

/* issue */
_hot _nodiscard xstyle::Issue make_issue(const xstyle::SourceFile& file, const std::size_t line, const std::size_t column, std::string_view code,
    const std::string& message, const std::string& suggestion = "", const std::optional<xstyle::Fix>& fix = std::nullopt); // line / column 0-based, line NO_INDEX = whole file
_hot _nodiscard inline xstyle::Fix replace_line(const std::size_t line, const std::string& content)                                    {return xstyle::Fix{xstyle::FixKind::Replace, line, 1, {content}};};
_hot _nodiscard inline xstyle::Fix replace_lines(const std::size_t line, const std::size_t count, const std::vector<std::string>& content) {return xstyle::Fix{xstyle::FixKind::Replace, line, count, content};};

} // namespace end
#endif /* CHECKS_H */
