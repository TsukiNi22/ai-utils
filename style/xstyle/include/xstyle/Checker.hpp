/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Checker.hpp

File Description:
##  Runs the checks on a file (selection, suppression comments) and applies the automatic fixes
\**************************************************************/

#ifndef CHECKER_H
    #define CHECKER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include "XStyleType.hpp"       // xstyle::Options, xstyle::ProjectInfo, xstyle::Issue, xstyle::AppliedFix
    #include "SourceFile.hpp"       // xstyle::SourceFile
    #include <string_view>          // std::string_view
    #include <vector>               // std::vector

    //----------------------------------------------------------------//
    /* DEFINE */

    #define FIX_PASSES 32 // Maximum passes of the fix loop (a fix can reveal another issue)

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class Checker {
    private:
        const xstyle::Options& _options;
        const xstyle::ProjectInfo& _project;
        bool _libutils = false; // libutils rules enabled

        // ---------- Pre-Function -------- //
        _hot _nodiscard bool selected_(std::string_view code, const bool fix) const;
        _hot _nodiscard bool suppressed_(const xstyle::SourceFile& file, const xstyle::Issue& issue) const;
        _hot _nodiscard std::vector<xstyle::Issue> run_(const xstyle::SourceFile& file, const bool fix) const;

    public:
        // ---------- Pre-Function -------- //
        _hot std::vector<xstyle::AppliedFix> fix(xstyle::SourceFile& file) const; // fix loop, returns what was changed

        // ------------ Function ---------- //
        _hot _nodiscard inline std::vector<xstyle::Issue> check(const xstyle::SourceFile& file) const {return this->run_(file, false);};
        _cold _nodiscard inline bool libutils(void) const                                           {return this->_libutils;};

        // ------------ Operator ---------- //
        Checker& operator=(const Checker& other) = delete;
        Checker& operator=(Checker&& other) = delete;

        // ---------- Constructor --------- //
        Checker(const xstyle::Options& options, const xstyle::ProjectInfo& project);
        Checker(const Checker& other) = delete;
        Checker(Checker&& other) = delete;

        // ----------- Destructor --------- //
        ~Checker() = default;
};

} // namespace end
#endif /* CHECKER_H */
