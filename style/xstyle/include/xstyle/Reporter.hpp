/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Reporter.hpp

File Description:
##  Output of the issues: terminal (colors, hyperlinks), report file (text, Markdown, JSON) and summary
\**************************************************************/

#ifndef REPORTER_H
    #define REPORTER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #define _IOManip
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard, utils::iomanip::Color
    #include "XStyleType.hpp"       // xstyle::Options, xstyle::ProjectInfo, xstyle::Issue, xstyle::AppliedFix
    #include "SourceFile.hpp"       // xstyle::SourceFile
    #include <ostream>              // std::ostream
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <map>                  // std::map

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class Reporter {
    private:
        const xstyle::Options& _options;
        const xstyle::ProjectInfo& _project;
        bool _libutils = false; // libutils rules enabled
        std::size_t _files = 0;
        std::map<xstyle::Language, std::size_t> _languages; // language -> files
        std::vector<xstyle::Issue> _issues;
        std::vector<std::pair<std::string, xstyle::AppliedFix>> _fixes; // <file, fix>

        // ---------- Pre-Function -------- //
        _cold _nodiscard std::string paint_(const std::string& text, const utils::iomanip::Color color, const bool tty, const bool bold = false) const;
        _cold _nodiscard std::string severity_(const xstyle::Severity severity, const bool tty) const;
        _cold _nodiscard std::string location_(const xstyle::Issue& issue, const bool tty) const;
        _cold _nodiscard std::string issues_(const bool tty) const;
        _cold _nodiscard std::string fixes_(const bool tty) const;
        _cold _nodiscard std::string summary_(const bool tty) const;
        _cold _nodiscard std::string markdown_(void) const;
        _cold _nodiscard std::string json_(void) const;

    public:
        // ---------- Pre-Function -------- //
        /* setup */
        _cold void addFile(const xstyle::SourceFile& file);
        _cold void addFixes(const std::string& file, const std::vector<xstyle::AppliedFix>& fixes);

        /* output */
        _cold void print(std::ostream& out) const; // terminal
        _cold void write(void) const; // report file (--report)
        _cold _nodiscard int exitCode(void) const; // 1 when an issue >= --fail-on is left
        _cold _nodiscard std::string topLanguage(void) const;

        // ------------ Function ---------- //
        /* setup */
        _cold inline void addIssues(const std::vector<xstyle::Issue>& issues) {this->_issues.insert(this->_issues.end(), issues.begin(), issues.end());};

        // ------------ Operator ---------- //
        Reporter& operator=(const Reporter& other) = delete;
        Reporter& operator=(Reporter&& other) = delete;

        // ---------- Constructor --------- //
        Reporter(const xstyle::Options& options, const xstyle::ProjectInfo& project, const bool libutils);
        Reporter(const Reporter& other) = delete;
        Reporter(Reporter&& other) = delete;

        // ----------- Destructor --------- //
        ~Reporter() = default;
};

} // namespace end
#endif /* REPORTER_H */
