/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file line_editor.cpp

File Description:
##  Unit test of the line editor of the terminal front-end
##  (cluster::Editor cells / kill ring / words, cluster::LineEditor
##  keys, scrolling and drawing). No framework: exit code 0 = ok.
##  Build: cmake -S . -B /tmp/cc-test -DCLUSTER_TESTS=ON, then
##  cmake --build /tmp/cc-test --target line_editor_test
##  and run /tmp/cc-test/line_editor_test
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/LineEditor.hpp"
#include "cluster/Tools.hpp"
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <string>

static int failures = 0;

static void check(const bool ok, const std::string& what, const int line)
{
    if (ok) return;
    ++failures;
    std::cerr << "FAIL line " << line << ": " << what << std::endl;
}

#define CHECK(condition) check((condition), #condition, __LINE__)

static ftxui::Event key(const std::string& input)
{
    return ftxui::Event::Special(input);
}

static std::string drawn(const cluster::LineEditor& editor, const int width, const int rows)
{
    ftxui::Screen screen(width, rows);
    ftxui::Render(screen, editor.render(width, rows, cluster::LineColors{}, true));
    return screen.ToString();
}

int main(void)
{
    // cells: an emoji, a combining accent and a wide character move as one
    {
        cluster::Editor editor;
        editor.insert("a" "e\xCC\x81" "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD" "b");    // a, e + acute, thumbs up + skin tone, b
        CHECK(editor.cursor() == editor.text().size());
        editor.left(false);
        editor.left(false);
        CHECK(editor.text().substr(editor.cursor()) == "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD" "b");    // on the emoji (its skin tone included)
        editor.left(false);
        CHECK(editor.text().substr(editor.cursor(), 3) == "e\xCC\x81");                             // on the accented letter
        editor.erase();
        CHECK(editor.text() == "a" "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD" "b");                      // erased as one cell
        editor.right(false);
        editor.backspace();
        CHECK(editor.text() == "ab");
    }
    // words across lines: a line break is a stop - at the end of a line, the start of the next one (not after its first word); at the
    // start of a line, the end of the one before (not before its last word); the selection made so stops there too
    {
        cluster::Editor editor;
        editor.insert("one two\nthree four");
        editor.place(8, false);                 // the start of "three four"
        CHECK(editor.text().substr(editor.cursor()) == "three four");
        editor.wordLeft(true);
        CHECK(editor.cursor() == 7);            // the end of "one two", the line break selected only
        editor.wordLeft(true);
        CHECK(editor.cursor() == 4);            // "two"
        editor.place(7, false);                 // the end of the first line
        editor.wordRight(true);
        CHECK(editor.cursor() == 8);            // the start of "three four", not after "three"
        editor.wordRight(true);
        CHECK(editor.cursor() == 13);           // after "three"
    }
    // the hidden id of a /entry and the marks of a paste: invisible, one cell with the letter before, told apart for the core
    {
        const std::string tagged = "/export" + cluster::slash_tag("s") + " now";
        std::string id;
        CHECK(cluster::slash_untag(tagged, &id) == "/export now");
        CHECK(id == "s");
        const std::string pasted = "see " + cluster::paste_mark("a pasted line") + " ok";
        CHECK(cluster::paste_reveal(pasted) == "see <pasted>a pasted line</pasted> ok");
        CHECK(cluster::slash_untag(pasted) == "see a pasted line ok");
        CHECK(cluster::paste_keep(tagged + pasted).find(cluster::slash_tag("s")) == std::string::npos);
        CHECK(cluster::paste_reveal(cluster::paste_keep(tagged + pasted)) == "/export nowsee <pasted>a pasted line</pasted> ok");
        cluster::Editor editor;
        editor.paste("xyz");
        CHECK(cluster::paste_reveal(editor.text()) == "<pasted>xyz</pasted>");
        CHECK(editor.shown(editor.text()).find("xyz") != std::string::npos);
        cluster::Editor command;
        command.paste("!ls -la");               // a command starting the prompt: its mode kept, no mark
        CHECK(command.text() == "!ls -la");
    }
    // words, kill ring, undo / redo, selection replaced by typing
    {
        cluster::Editor editor;
        editor.insert("one two three");
        editor.killWord(false);
        CHECK(editor.text() == "one two ");
        CHECK(editor.killed() == "three");
        editor.yank();
        CHECK(editor.text() == "one two three");
        editor.home(false);
        editor.killWord(true);
        CHECK(editor.text() == " two three");
        editor.end(false);
        editor.killLine(false);
        CHECK(editor.text().empty());
        editor.yank();
        CHECK(editor.text() == " two three");
        editor.undo();
        CHECK(editor.text().empty());
        editor.redo();
        CHECK(editor.text() == " two three");
        editor.selectWord(2);
        CHECK(editor.selected() == "two");
        editor.insert("2");
        CHECK(editor.text() == " 2 three");
    }
    // the keys of the line editor
    {
        cluster::LineEditor editor;
        for (const char* c: {"h", "e", "l", "l", "o", " ", "w", "o", "r", "l", "d"})
            CHECK(editor.key(ftxui::Event::Character(c)));
        CHECK(editor.text() == "hello world");
        CHECK(editor.key(key("\x1b[1;5D")));                    // Ctrl+Left // xstyle: ignore LU-ANSI (sent to the editor)
        CHECK(editor.cursor() == 6);
        CHECK(editor.key(key("\x1b[3;5~")));                    // Ctrl+Delete: the next word // xstyle: ignore LU-ANSI (sent to the editor)
        CHECK(editor.text() == "hello ");
        CHECK(editor.key(key("\x17")));                         // Ctrl+W: the word before
        CHECK(editor.text().empty());
        CHECK(editor.key(key("\x19")));                         // Ctrl+Y: back
        CHECK(editor.text() == "hello ");
        CHECK(editor.key(key("\x01")));                         // Ctrl+A
        CHECK(editor.cursor() == 0);
        CHECK(editor.key(key("\x1b[1;2C")));                    // Shift+Right: selects // xstyle: ignore LU-ANSI (sent to the editor)
        CHECK(editor.editor().selected() == "h");
        CHECK(editor.key(ftxui::Event::Character("J")));        // replaces the selection
        CHECK(editor.text() == "Jello ");
        CHECK(!editor.key(ftxui::Event::ArrowUp));              // a single line keeps Up for its list
        CHECK(!editor.key(ftxui::Event::Escape));
        CHECK(!editor.key(ftxui::Event::Return));
    }
    // several lines: Up / Down by visual line, the first / last row keeps them
    {
        cluster::LineEditor editor(true);
        editor.set("abcdef\nxy");
        (void)drawn(editor, 20, 4);
        CHECK(editor.key(ftxui::Event::ArrowUp));
        CHECK(editor.cursor() == 2);                            // the column of the end of "xy"
        CHECK(!editor.key(ftxui::Event::ArrowUp));
        CHECK(editor.key(ftxui::Event::ArrowDown));
        CHECK(editor.cursor() == 9);
        CHECK(!editor.key(ftxui::Event::ArrowDown));
    }
    // drawing: a long single line scrolls around the cursor with … marks
    {
        cluster::LineEditor editor;
        editor.set("0123456789abcdefghij");
        const std::string end = drawn(editor, 10, 1);
        CHECK(end.find("…") != std::string::npos);
        CHECK(end.find("j") != std::string::npos);              // the cursor is at the end: visible
        CHECK(end.find("0") == std::string::npos);
        (void)editor.key(key("\x01"));
        const std::string start = drawn(editor, 10, 1);
        CHECK(start.find("0") != std::string::npos);
        CHECK(start.find("j") == std::string::npos);
    }
    if (failures == 0) std::cout << "line_editor: ok" << std::endl;
    return failures == 0 ? 0 : 1;
}
