/**************************************************************\
Edition:
##  @date 02/11/2026 by @author Tsukini

File Name:
##  @file JsonAssert.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef JSONASSERT_H
    #define JSONASSERT_H

// Given to every file before anything else (-include): what nlohmann::json only asserts - a key missing from a const value read with [], an iterator of
// another value - is undefined behaviour once built without asserts (a read past the end of its map: a crash, or anything). Here an exception, caught like
// the others: a JSON of an unexpected shape (an agent, a saved state) fails one request, never the core.
    #include <stdexcept>    // std::logic_error
    #define JSON_ASSERT(x) ((x) ? static_cast<void>(0) : throw std::logic_error("a JSON value of an unexpected shape (" #x ")"))

#endif /* JSONASSERT_H */
