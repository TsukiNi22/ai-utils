/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Rules.hpp

File Description:
##  Catalogue of the rules (code, severity, fixable, languages, description)
\**************************************************************/

#ifndef RULES_H
    #define RULES_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "XStyleType.hpp"       // xstyle::Rule, xstyle::Severity, xstyle::Language
    #include <string_view>          // std::string_view
    #include <optional>             // std::optional
    #include <string>            // std::string
    #include <vector>               // std::vector

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

/* catalogue */
_cold _nodiscard const std::vector<xstyle::Rule>& rules(void);
_cold _nodiscard const xstyle::Rule* find_rule(std::string_view code); // nullptr when unknown
_cold _nodiscard bool rule_applies(const xstyle::Rule& rule, const xstyle::Language language);
_cold _nodiscard bool match_code(std::string_view code, std::string_view pattern); // exact, prefix (CPP / CPP-*) or *

/* names */
_cold _nodiscard std::string_view severity_name(const xstyle::Severity severity);
_cold _nodiscard std::optional<xstyle::Severity> parse_severity(const std::string& name);
_cold _nodiscard std::string_view language_name(const xstyle::Language language);
_cold _nodiscard std::string_view fix_mode_name(const xstyle::FixMode mode); // auto | force | ask | -
_cold _nodiscard std::string_view language_id(const xstyle::Language language); // cpp, c, py...
_cold _nodiscard std::optional<xstyle::Language> parse_language(const std::string& name);

} // namespace end
#endif /* RULES_H */
