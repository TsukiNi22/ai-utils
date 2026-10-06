/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file SourceFile.hpp

File Description:
##  Source file loaded in memory: lines, masked code (no comment / string), comments, C / C++ scopes
\**************************************************************/

#ifndef SOURCEFILE_H
    #define SOURCEFILE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include "XStyleType.hpp"       // xstyle::Language, xstyle::Fix
    #include <filesystem>           // std::filesystem::path
    #include <cstdint>              // std::uint8_t
    #include <string>               // std::string
    #include <vector>               // std::vector

    //----------------------------------------------------------------//
    /* DEFINE */

    #define NO_INDEX static_cast<std::size_t>(-1) // No brace / no line

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class Scope: std::uint8_t {
    Global,
    Namespace,
    Extern, // extern "C" {
    Class,
    Struct, // struct / union
    Enum,
    Function,
    Lambda,
    Control, // if / for / while / switch / do / try / catch / else
    Block, // bare block / case block
    Init, // brace initialization
};

//----------------------------------------------------------------//
/* STRUCT */

struct Brace {
    xstyle::Scope kind = xstyle::Scope::Block;
    std::size_t openLine = 0;
    std::size_t openColumn = 0;
    std::size_t closeLine = NO_INDEX; // NO_INDEX = never closed
    std::size_t closeColumn = 0;
    std::size_t headerLine = NO_INDEX; // first line of the text before the brace
    std::string header; // masked text before the brace (trimmed, lines joined by a space)
    std::string name; // class / namespace name
    bool isStatic = false;
    std::size_t parent = NO_INDEX;
};

struct LineInfo {
    std::size_t brace = NO_INDEX; // innermost brace open at the start of the line
    bool inFunction = false; // inside a function / lambda body
    std::size_t parenDepth = 0; // open parentheses at the start of the line
    bool preprocessor = false; // part of a preprocessor directive
    bool inComment = false; // starts inside a block comment
};

//----------------------------------------------------------------//
/* CLASS */

class SourceFile {
    private:
        std::filesystem::path _path;
        std::string _display; // path shown in the reports
        xstyle::Language _language = xstyle::Language::Other;
        std::vector<std::string> _lines;
        std::vector<std::string> _code; // comments & string contents replaced by spaces (same columns)
        std::vector<std::string> _comments; // only the comments (the rest replaced by spaces)
        std::vector<xstyle::LineInfo> _info;
        std::vector<xstyle::Brace> _braces;
        bool _finalNewline = true;
        bool _crlf = false;

        // ---------- Pre-Function -------- //
        _hot void analyze_(void);
        _hot void maskCLike_(void);
        _hot void maskHash_(void);
        _hot void maskLua_(void);
        _hot void scopes_(void);

    public:
        // ---------- Pre-Function -------- //
        /* fix */
        _hot void apply(std::vector<xstyle::Fix> fixes); // non-overlapping fixes, re-analyze after
        _cold void save(void) const;

        /* tools */
        _hot _nodiscard xstyle::Scope scopeAt(const std::size_t line) const; // innermost scope at the start of the line
        _hot _nodiscard bool isHeader(void) const;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline const std::filesystem::path& getPath(void) const          {return this->_path;};
        _cold _nodiscard inline const std::string& getDisplay(void) const                 {return this->_display;};
        _cold _nodiscard inline xstyle::Language getLanguage(void) const                  {return this->_language;};
        _cold _nodiscard inline const std::vector<std::string>& getLines(void) const      {return this->_lines;};
        _cold _nodiscard inline const std::vector<std::string>& getCode(void) const       {return this->_code;};
        _cold _nodiscard inline const std::vector<std::string>& getComments(void) const   {return this->_comments;};
        _cold _nodiscard inline const std::vector<xstyle::LineInfo>& getInfo(void) const  {return this->_info;};
        _cold _nodiscard inline const std::vector<xstyle::Brace>& getBraces(void) const   {return this->_braces;};
        _cold _nodiscard inline bool hasFinalNewline(void) const                          {return this->_finalNewline;};
        _cold _nodiscard inline bool hasCrlf(void) const                                  {return this->_crlf;};
        /* tools */
        _hot _nodiscard inline bool isCLike(void) const {return this->_language == xstyle::Language::Cpp || this->_language == xstyle::Language::C;};

        // ------------ Operator ---------- //
        SourceFile& operator=(const SourceFile& other) = delete;
        SourceFile& operator=(SourceFile&& other) = default;

        // ---------- Constructor --------- //
        SourceFile(const std::filesystem::path& path, const std::string& display, const xstyle::Language language);
        SourceFile(const SourceFile& other) = delete;
        SourceFile(SourceFile&& other) = default;

        // ----------- Destructor --------- //
        ~SourceFile() = default;
};

} // namespace end
#endif /* SOURCEFILE_H */
