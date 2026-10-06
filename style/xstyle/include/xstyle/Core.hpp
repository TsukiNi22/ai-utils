/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Core.hpp

File Description:
##  Core of xstyle: arguments, file collection, check & fix loop
\**************************************************************/

#ifndef CORE_H
    #define CORE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #define _Arguments
    #include <utils/utils.hpp>      // _cold, _nodiscard, utils::arguments::ArgParser
    #include "SourceFile.hpp"       // xstyle::SourceFile
    #include "XStyleType.hpp"       // xstyle::Options, xstyle::Language, xstyle::AppliedFix
    #include "Checker.hpp"          // xstyle::Checker
    #include <filesystem>           // std::filesystem::path
    #include <optional>             // std::optional
    #include <utility>              // std::pair
    #include <string>               // std::string
    #include <vector>               // std::vector

    //----------------------------------------------------------------//
    /* DEFINE */

    #define XSTYLE_VERSION "1.0.0" // Version of xstyle
    #define MAX_FILE_SIZE (2 * 1024 * 1024) // Bigger files are skipped (generated / minified)

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* TYPEDEF */

using Files = std::vector<std::pair<std::filesystem::path, xstyle::Language>>;

//----------------------------------------------------------------//
/* CLASS */

class Core {
    private:
        xstyle::Options _options;
        utils::arguments::ArgParser _parser{"xstyle", "Check (and fix) Tsukini's coding style: C++ first (cpp-style, cpp-comments, libutils), "
            "then Python, shell, Rust and the generic rules of every language.\nPaths: files or directories given as bare arguments (default: .), "
            "directories are scanned recursively with -r (always with no path)."};
        int _exit = 0;
        bool _listRules = false;
        bool _version = false;
        std::string _completion; // shell of the completion script to print
        std::optional<std::pair<std::string, std::string>> _headerAnswer; // <banner, description> kept for the next files (empty banner: skip)
        std::string _explain;
        std::vector<std::pair<std::string, std::vector<std::string>>> _sections; // <title, flag ids> of the help, in order

        // ---------- Pre-Function -------- //
        _cold void setup_(void);
        _cold _nodiscard std::vector<std::string> extractPaths_(const int argc, char* argv[]);
        _cold void apply_(const std::string& id, const std::vector<std::string>& values);
        _cold _nodiscard xstyle::Files collect_(void) const;
        _cold void listRules_(void) const;
        _cold void explain_(void) const;
        _cold void help_(const utils::arguments::ArgParser& parser) const;
        _cold void completion_(void) const; // Core-Completion.cpp
        _cold _nodiscard std::vector<xstyle::AppliedFix> header_(const xstyle::Checker& checker, xstyle::SourceFile& source);

    public:
        // ---------- Pre-Function -------- //
        _cold void init(const int argc, char* argv[]);
        _cold void run(void);

        // ------------ Function ---------- //
        _cold _nodiscard inline int exit(void) const {return this->_exit;};

        // ------------ Operator ---------- //
        Core& operator=(const Core& other) = delete;
        Core& operator=(Core&& other) = delete;

        // ---------- Constructor --------- //
        Core() = default;
        Core(const Core& other) = delete;
        Core(Core&& other) = delete;

        // ----------- Destructor --------- //
        ~Core() = default;
};

} // namespace end
#endif /* CORE_H */
