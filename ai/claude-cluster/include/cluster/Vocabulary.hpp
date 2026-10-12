/**************************************************************\
Edition:
##  @date 12/10/2026 by @author Tsukini

File Name:
##  @file Vocabulary.hpp

File Description:
##  Vocabulary of the voice: the proper names the speech
##  recognition bends (projects, sessions, the user's names, an
##  editable list) written back in every sentence heard
\**************************************************************/

#ifndef CLUSTER_VOCABULARY_H
    #define CLUSTER_VOCABULARY_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Tools.hpp"            // cluster::trim, cluster::config_dir, cluster::expand_home
    #include <filesystem>           // std::filesystem::path
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct VoiceWords {
    std::vector<std::string> cancel;    // said alone: the transcription waiting dropped (else the answer read stopped)
    std::vector<std::string> send;      // said alone: the transcription waiting sent now
};

struct Term {
    std::string text;               // as it is written (libutils, claude-cluster)
    std::vector<std::string> keys;  // its key and the keys of the ways it is heard (vocabulary_key)
};

//----------------------------------------------------------------//
/* FUNCTION */

// the vocabulary file: [voice] vocabulary, empty: vocabulary.txt of the config folder
_cold _nodiscard inline std::filesystem::path vocabulary_path(const std::string& configured) {return cluster::trim(configured).empty() ? cluster::config_dir() / "vocabulary.txt" : cluster::expand_home(configured);};

// the file of the voice words: [voice] words, empty: voice-words.txt of the config folder
_cold _nodiscard inline std::filesystem::path voice_words_path(const std::string& configured) {return cluster::trim(configured).empty() ? cluster::config_dir() / "voice-words.txt" : cluster::expand_home(configured);};

// its words: one per line under [cancel] / [send], # comments
_cold _nodiscard cluster::VoiceWords voice_words(const std::filesystem::path& path);

// lower case, no accent, letters and digits only: "Lib utils" and "libutils" -> "libutils"
_cold _nodiscard std::string vocabulary_key(const std::string& text);

// a term and the ways it is heard ("libutils = lib utils, lib you tils"), # comments
_cold _nodiscard std::vector<cluster::Term> vocabulary_file(const std::filesystem::path& path);

// the folder names of the git repositories of the map of this computer (machine.md, its Projects table)
_cold _nodiscard std::vector<cluster::Term> vocabulary_projects(const std::filesystem::path& machine);

// a term from a name (its key only), nothing when too short to be told apart (< 3 characters)
_cold void vocabulary_add(std::vector<cluster::Term>& terms, const std::string& name);

// the sentence with the terms written back: 1 to 4 words heard -> the term (exact key, an alias, or close enough for a
// long one)
_cold _nodiscard std::string vocabulary_correct(const std::string& text, const std::vector<cluster::Term>& terms);

} // namespace end

#endif /* CLUSTER_VOCABULARY_H */
