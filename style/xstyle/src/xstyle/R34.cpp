/**************************************************************\
Edition:
##  @date 22/10/2026 by @author Tsukini

File Name:
##  @file R34.cpp

File Description:
##  The text of R34.md, written by xstyle --fix --force to acknowledge the rule R34
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/Tools.hpp"
#include "xstyle/R34.hpp"
#include <system_error>

/* R34 */
_cold std::string xstyle::r34_document(void)
{
    return R"R34(# Rule 34

> If it exists, there is porn of it. No exceptions.

This file was written by `xstyle --fix --force` to acknowledge the rule **R34** of xstyle. As long as it is next to the
place where xstyle is run, the rule is considered read, understood and accepted. It is never replaced: edit it, extend
it, delete it if the rule must be shown again.

## Table of Contents
 - [Statement](#statement)
 - [Origin](#origin)
 - [The rules of the Internet](#the-rules-of-the-internet)
 - [Spread and reception](#spread-and-reception)
 - [Interpretation](#interpretation)
 - [Rule 34 in xstyle](#rule-34-in-xstyle)
 - [Sources](#sources)

## Statement
Rule 34 is an Internet meme claiming that some form of pornography exists about every possible subject. It is usually
used with a mix of resignation and humor when someone finds fan made adult content about a subject that was never meant
to have any: a cartoon character, an object, a brand, a news event, a programming language.

The original caption was **"Rule #34: There is porn of it. No exceptions."** The later and most common versions are
shorter, **"Rule 34: If it exists, there is porn of it."**, and many of them drop the "No exceptions". The rule is a
joke and an observation, not a law: nothing proves it and nothing enforces it.

The content the rule talks about can be anything an artist or a writer can produce: drawings, fan fiction, animations,
images or short looping clips. This document does not describe any of it.

## Origin
The phrase was coined by **Peter Morley-Souter**, who published under the name *TangoStari*, in a webcomic dated
**13 August 2003**. The comic expresses his shock at discovering adult parodies of the comic strip *Calvin and Hobbes*,
and states the rule as the conclusion of the discovery.

The comic itself faded into obscurity, but its caption did not: it was copied, shortened and reposted on forums and
image boards until it had lost its author and become part of the folklore of the Internet.

## The rules of the Internet
The rule was soon included in a list of joking maxims known as the **rules of the Internet**, associated with the
image board 4chan. The list is not an official document and has many versions, but a few rules are quoted again and
again. Two corollaries are tied to Rule 34:

| Rule | Statement | Meaning |
| ---- | --------- | ------- |
| Rule 34 | If it exists, there is porn of it | Every subject has its adult version |
| Rule 35 | If there is no porn of it, it will be made | The gaps do not last |
| Rule 63 | There is a gender swapped version of every character | Another rule of the same list, often quoted with Rule 34 |

## Spread and reception
 - In **2008**, users of 4chan posted many cartoons and parodies illustrating the rule, which helped it leave the
   forums.
 - In **2009**, the British newspaper *The Daily Telegraph* listed it third in an article about the "top 10" laws and
   rules of the Internet.
 - In **2013**, a story of the news channel CNN called it "likely the most famous" rule of the Internet, one that had
   entered mainstream culture.
 - Fan fiction has used the rule as a punchline about public events: the 2016 United States presidential election, the
   blocking of the Suez Canal in 2021 or Brexit are some of the examples quoted.

## Interpretation
Several authors tried to explain why the rule feels true to everyone who has spent time online:
 - The researchers **Ogi Ogas** and **Sai Gaddam** suggest that it resonates because it matches the experience of
   anyone who has browsed the Internet long enough. Ogas also argues that the consolidation of the video sites reduced
   how visible niche content is, which does not make it disappear.
 - The scholar **John Paul Stadler** reads it as a sign that sexual preferences have become part of how people build
   their social identity online.

A more practical reading is statistical: the Internet connects a very large number of people who produce a very large
amount of content, so a subject with zero adult content is the rare case, and rare cases are the ones that become
jokes when they are found.

## Rule 34 in xstyle
xstyle shows a reminder of the rule, **R34**, with the severity **mandatory**:
 - it is shown **once per run**, for the whole project, with a link to the article of the rule;
 - it is **never counted**: not in the totals, not in the exit status, not in `--fail-on`, not for the pre-commit hook;
 - it is acknowledged by **the integer 34**, or **r34** (any case) in an identifier, a string or a comment of a checked
   file, by a file named **R34.md** (this one), by the flag **--r34** or by `XSTYLE_R34=1`;
 - `xstyle -x R34` explains it again and `xstyle --fix --force` writes this file.

Reading this far counts as having read it. Do not worry about the exceptions: there are none.

## Sources
 - Wikipedia, [Rule 34](https://en.wikipedia.org/wiki/Rule_34), the article this text is written from.
)R34";
}

_cold bool xstyle::r34_file_exists(const std::filesystem::path& directory)
{
    std::error_code error;

    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(directory, error))
        if (xstyle::lower(entry.path().filename().string()) == xstyle::lower(R34_FILE)) return true;
    return false;
}
