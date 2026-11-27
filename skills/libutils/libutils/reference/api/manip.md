# libutils `manip`

Generated from libutils `v5.0.0` (commit `c129a5e`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/manip/iomanip/ANSI.hpp`

Definition of ANSI escape sequences

Namespace: `utils::iomanip`

```cpp
// namespace utils::iomanip
std::string set_style(std::initializer_list<utils::iomanip::Style> styles);
std::string reset_style(std::initializer_list<utils::iomanip::ResetStyle> styles);
bool enable_ansi(void); // Windows: enable the virtual terminal mode of the console (ANSI sequences), Unix: nothing to do (true)
constexpr std::string esc(void);
constexpr std::string csi(const std::string& code);
constexpr std::string file_hyperlink(const std::string& display, const std::string& path);
constexpr std::string hyperlink(const std::string& display, const std::string& link);
constexpr std::string reset(void);
constexpr std::string strong_reset(void);
constexpr std::string dark_reset(void);
constexpr std::string italic_reset(void);
constexpr std::string underlined_reset(void);
constexpr std::string flashing_fast_reset(void);
constexpr std::string flashing_slow_reset(void);
constexpr std::string reversed_reset(void);
constexpr std::string hide_reset(void);
constexpr std::string bar_reset(void);
constexpr std::string framed_encircled_reset(void);
constexpr std::string overlined_reset(void);
constexpr std::string underline_color_reset(void);
constexpr std::string exposant_indice_reset(void);
constexpr std::string reset_style(utils::iomanip::ResetStyle style);
constexpr std::string strong(void);
constexpr std::string dark(void);
constexpr std::string italic(void);
constexpr std::string underlined(void);
constexpr std::string flashing_fast(void);
constexpr std::string flashing_slow(void);
constexpr std::string reversed(void);
constexpr std::string hide(void);
constexpr std::string bar(void);
constexpr std::string monospace(void);
constexpr std::string framed(void); // Rarely supported
constexpr std::string encircled(void); // Rarely supported
constexpr std::string overlined(void);
constexpr std::string exposant(void); // Rarely supported
constexpr std::string indice(void); // Rarely supported
constexpr std::string set_style(utils::iomanip::Style style);
constexpr std::string color(utils::iomanip::Color c);
constexpr std::string color(utils::iomanip::BackColor c);
constexpr std::string color_id(std::uint8_t id);
constexpr std::string back_color_id(std::uint8_t id);
constexpr std::string underline_color_id(std::uint8_t id);
constexpr std::string color_rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b);
constexpr std::string back_color_rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b);
constexpr std::string underline_color_rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b);
constexpr std::string load_cur(void);
constexpr std::string save_cur(void);
constexpr std::string up(std::size_t n);
constexpr std::string down(std::size_t n);
constexpr std::string right(std::size_t n);
constexpr std::string left(std::size_t n);
constexpr std::string next_line(std::size_t n);
constexpr std::string previous_line(std::size_t n);
constexpr std::string column(std::size_t col);
constexpr std::string pos(std::size_t row, std::size_t col);
constexpr std::string scroll_up(std::size_t n);
constexpr std::string scroll_down(std::size_t n);
constexpr std::string screen_end(void);
constexpr std::string screen_start(void);
constexpr std::string screen(void);
constexpr std::string scrollback_buffer(void); // Can delete the term history
constexpr std::string line_end(void);
constexpr std::string line_start(void);
constexpr std::string line(void);
constexpr std::string inverted_color_enable(void);
constexpr std::string inverted_color_disable(void);
constexpr std::string wrapping_enable(void);
constexpr std::string wrapping_disable(void);
constexpr std::string show_cur(void);
constexpr std::string hide_cur(void);
constexpr std::string save_screen(void);
constexpr std::string load_screen(void);
constexpr std::string get_pos(void); // Reports cursor position as "ESC[row;colR"
constexpr std::string mouse_tracking_enable(void);
constexpr std::string mouse_tracking_disable(void);
constexpr std::string mouse_move_tracking_enable(void);
constexpr std::string mouse_move_tracking_disable(void);
constexpr std::string mouse_adv_tracking_enable(void);
constexpr std::string mouse_adv_tracking_disable(void);
constexpr std::string report_focus_enable(void); // Focus in: "ESC[I" | Focus out: "ESC[O"
constexpr std::string report_focus_disabled(void);
constexpr std::string report_past_enable(void); // Reports for pasted data: "ESC[200~{data}ESC[201~"
constexpr std::string report_past_disable(void);
enum class MouseButton {Left, Right, Middle, Release, Unknown}
struct MouseEvent {
    utils::iomanip::MouseButton button = utils::iomanip::MouseButton::Unknown;
    std::size_t x = 0;
    std::size_t y = 0;
};
struct AdvancedMouseEvent {
    utils::iomanip::MouseButton button = utils::iomanip::MouseButton::Unknown;
    std::size_t x = 0;
    std::size_t y = 0;
    bool pressed = false;
};
std::pair<int, int> read_cursor_position(void);
utils::iomanip::MouseEvent read_mouse_event(void);
utils::iomanip::AdvancedMouseEvent read_advanced_mouse_event(void);
```

## `utils/manip/iomanip/CApi.h`

C API of utils::iomanip: the ANSI sequences as strings, the reports of the terminal (C & C++ compatible) Sequences without argument: static strings (always valid), with arguments: valid until the next call of the same function in the thread (NULL on error), _r: copied in the buffer of the user

```c
// global
typedef enum utils_iomanip_char { C_API_IOMANIP_CHAR_NUL = 0, C_API_IOMANIP_CHAR_SOH = 1, C_API_IOMANIP_CHAR_STX = 2, C_API_IOMANIP_CHAR_ETX = 3, C_API_IOMANIP_CHAR_EOT = 4, C_API_IOMANIP_CHAR_ENQ = 5, C_API_IOMANIP_CHAR_ACK = 6, C_API_IOMANIP_CHAR_BEL = 7, C_API_IOMANIP_CHAR_BS = 8, C_API_IOMANIP_CHAR_HT = 9, C_API_IOMANIP_CHAR_LF = 10, C_API_IOMANIP_CHAR_VT = 11, C_API_IOMANIP_CHAR_FF = 12, C_API_IOMANIP_CHAR_CR = 13, C_API_IOMANIP_CHAR_SO = 14, C_API_IOMANIP_CHAR_SI = 15, C_API_IOMANIP_CHAR_DLE = 16, C_API_IOMANIP_CHAR_DC1 = 17, C_API_IOMANIP_CHAR_DC2 = 18, C_API_IOMANIP_CHAR_DC3 = 19, C_API_IOMANIP_CHAR_DC4 = 20, C_API_IOMANIP_CHAR_NAK = 21, C_API_IOMANIP_CHAR_SYN = 22, C_API_IOMANIP_CHAR_ETB = 23, C_API_IOMANIP_CHAR_CAN = 24, C_API_IOMANIP_CHAR_EM = 25, C_API_IOMANIP_CHAR_SUB = 26, C_API_IOMANIP_CHAR_ESC = 27, C_API_IOMANIP_CHAR_FS = 28, C_API_IOMANIP_CHAR_GS = 29, C_API_IOMANIP_CHAR_RS = 30, C_API_IOMANIP_CHAR_US = 31, C_API_IOMANIP_CHAR_DEL = 127 };
typedef enum utils_iomanip_color { C_API_IOMANIP_COLOR_BLACK = 30, C_API_IOMANIP_COLOR_RED = 31, C_API_IOMANIP_COLOR_GREEN = 32, C_API_IOMANIP_COLOR_YELLOW = 33, C_API_IOMANIP_COLOR_BLUE = 34, C_API_IOMANIP_COLOR_MAGENTA = 35, C_API_IOMANIP_COLOR_CYAN = 36, C_API_IOMANIP_COLOR_WHITE = 37, C_API_IOMANIP_COLOR_BRIGHT_BLACK = 90, C_API_IOMANIP_COLOR_BRIGHT_RED = 91, C_API_IOMANIP_COLOR_BRIGHT_GREEN = 92, C_API_IOMANIP_COLOR_BRIGHT_YELLOW = 93, C_API_IOMANIP_COLOR_BRIGHT_BLUE = 94, C_API_IOMANIP_COLOR_BRIGHT_MAGENTA = 95, C_API_IOMANIP_COLOR_BRIGHT_CYAN = 96, C_API_IOMANIP_COLOR_BRIGHT_WHITE = 97, C_API_IOMANIP_COLOR_DEFAULT = 39 };
typedef enum utils_iomanip_back_color { C_API_IOMANIP_BACK_COLOR_BLACK = 40, C_API_IOMANIP_BACK_COLOR_RED = 41, C_API_IOMANIP_BACK_COLOR_GREEN = 42, C_API_IOMANIP_BACK_COLOR_YELLOW = 43, C_API_IOMANIP_BACK_COLOR_BLUE = 44, C_API_IOMANIP_BACK_COLOR_MAGENTA = 45, C_API_IOMANIP_BACK_COLOR_CYAN = 46, C_API_IOMANIP_BACK_COLOR_WHITE = 47, C_API_IOMANIP_BACK_COLOR_BRIGHT_BLACK = 100, C_API_IOMANIP_BACK_COLOR_BRIGHT_RED = 101, C_API_IOMANIP_BACK_COLOR_BRIGHT_GREEN = 102, C_API_IOMANIP_BACK_COLOR_BRIGHT_YELLOW = 103, C_API_IOMANIP_BACK_COLOR_BRIGHT_BLUE = 104, C_API_IOMANIP_BACK_COLOR_BRIGHT_MAGENTA = 105, C_API_IOMANIP_BACK_COLOR_BRIGHT_CYAN = 106, C_API_IOMANIP_BACK_COLOR_BRIGHT_WHITE = 107, C_API_IOMANIP_BACK_COLOR_DEFAULT = 49 };
typedef enum utils_iomanip_style { C_API_IOMANIP_STYLE_STRONG = 1, C_API_IOMANIP_STYLE_DARK = 2, C_API_IOMANIP_STYLE_ITALIC = 3, C_API_IOMANIP_STYLE_UNDERLINED = 4, C_API_IOMANIP_STYLE_FLASHING_SLOW = 5, C_API_IOMANIP_STYLE_FLASHING_FAST = 6, C_API_IOMANIP_STYLE_REVERSED = 7, C_API_IOMANIP_STYLE_HIDE = 8, C_API_IOMANIP_STYLE_BAR = 9, C_API_IOMANIP_STYLE_MONOSPACE = 50, C_API_IOMANIP_STYLE_FRAMED = 51, C_API_IOMANIP_STYLE_ENCIRCLED = 52, C_API_IOMANIP_STYLE_OVERLINED = 53, C_API_IOMANIP_STYLE_EXPOSANT = 73, C_API_IOMANIP_STYLE_INDICE = 74 };
typedef enum utils_iomanip_reset_style { C_API_IOMANIP_RESET_STYLE_ALL = 0, C_API_IOMANIP_RESET_STYLE_STRONG = 22, C_API_IOMANIP_RESET_STYLE_DARK = 22, C_API_IOMANIP_RESET_STYLE_ITALIC = 23, C_API_IOMANIP_RESET_STYLE_UNDERLINED = 24, C_API_IOMANIP_RESET_STYLE_FLASHING_FAST = 25, C_API_IOMANIP_RESET_STYLE_FLASHING_SLOW = 25, C_API_IOMANIP_RESET_STYLE_REVERSED = 27, C_API_IOMANIP_RESET_STYLE_HIDE = 28, C_API_IOMANIP_RESET_STYLE_BAR = 29, C_API_IOMANIP_RESET_STYLE_FRAMED_ENCIRCLED = 54, C_API_IOMANIP_RESET_STYLE_OVERLINED = 55, C_API_IOMANIP_RESET_STYLE_UNDERLINE_COLOR = 59, C_API_IOMANIP_RESET_STYLE_EXPOSANT_INDICE = 75 };
typedef enum utils_iomanip_mouse_button { C_API_IOMANIP_MOUSE_BUTTON_LEFT = 0, C_API_IOMANIP_MOUSE_BUTTON_RIGHT, C_API_IOMANIP_MOUSE_BUTTON_MIDDLE, C_API_IOMANIP_MOUSE_BUTTON_RELEASE, C_API_IOMANIP_MOUSE_BUTTON_UNKNOWN };
typedef struct utils_iomanip_mouse_event { utils_iomanip_mouse_button_t button; size_t x; size_t y; };
typedef struct utils_iomanip_advanced_mouse_event { utils_iomanip_mouse_button_t button; size_t x; size_t y; bool pressed; };
bool utils_iomanip_enable_ansi(void); // Windows: enable the virtual terminal mode of the console (ANSI sequences), Unix: nothing to do (true)
int utils_iomanip_read_cursor_position(int* row, int* col); // after utils_iomanip_get_pos (-1, -1 when nothing was read)
int utils_iomanip_read_mouse_event(utils_iomanip_mouse_event_t* event); // after utils_iomanip_mouse_tracking_enable
int utils_iomanip_read_advanced_mouse_event(utils_iomanip_advanced_mouse_event_t* event); // after utils_iomanip_mouse_adv_tracking_enable
const char* utils_iomanip_esc(void);
const char* utils_iomanip_reset(void);
const char* utils_iomanip_strong_reset(void);
const char* utils_iomanip_dark_reset(void);
const char* utils_iomanip_italic_reset(void);
const char* utils_iomanip_underlined_reset(void);
const char* utils_iomanip_flashing_fast_reset(void);
const char* utils_iomanip_flashing_slow_reset(void);
const char* utils_iomanip_reversed_reset(void);
const char* utils_iomanip_hide_reset(void);
const char* utils_iomanip_bar_reset(void);
const char* utils_iomanip_framed_encircled_reset(void);
const char* utils_iomanip_overlined_reset(void);
const char* utils_iomanip_underline_color_reset(void);
const char* utils_iomanip_exposant_indice_reset(void);
const char* utils_iomanip_strong(void);
const char* utils_iomanip_dark(void);
const char* utils_iomanip_italic(void);
const char* utils_iomanip_underlined(void);
const char* utils_iomanip_flashing_fast(void);
const char* utils_iomanip_flashing_slow(void);
const char* utils_iomanip_reversed(void);
const char* utils_iomanip_hide(void);
const char* utils_iomanip_bar(void);
const char* utils_iomanip_monospace(void);
const char* utils_iomanip_framed(void); // Rarely supported
const char* utils_iomanip_encircled(void); // Rarely supported
const char* utils_iomanip_overlined(void);
const char* utils_iomanip_exposant(void); // Rarely supported
const char* utils_iomanip_indice(void); // Rarely supported
const char* utils_iomanip_load_cur(void);
const char* utils_iomanip_save_cur(void);
const char* utils_iomanip_screen_end(void);
const char* utils_iomanip_screen_start(void);
const char* utils_iomanip_screen(void);
const char* utils_iomanip_scrollback_buffer(void); // Can delete the term history
const char* utils_iomanip_line_end(void);
const char* utils_iomanip_line_start(void);
const char* utils_iomanip_line(void);
const char* utils_iomanip_inverted_color_enable(void);
const char* utils_iomanip_inverted_color_disable(void);
const char* utils_iomanip_wrapping_enable(void);
const char* utils_iomanip_wrapping_disable(void);
const char* utils_iomanip_show_cur(void);
const char* utils_iomanip_hide_cur(void);
const char* utils_iomanip_save_screen(void);
const char* utils_iomanip_load_screen(void);
const char* utils_iomanip_get_pos(void); // Reports cursor position as "ESC[row;colR" (utils_iomanip_read_cursor_position)
const char* utils_iomanip_mouse_tracking_enable(void);
const char* utils_iomanip_mouse_tracking_disable(void);
const char* utils_iomanip_mouse_move_tracking_enable(void);
const char* utils_iomanip_mouse_move_tracking_disable(void);
const char* utils_iomanip_mouse_adv_tracking_enable(void); // Reports "ESC[<b;x;y(M|m)" (utils_iomanip_read_advanced_mouse_event)
const char* utils_iomanip_mouse_adv_tracking_disable(void);
const char* utils_iomanip_report_focus_enable(void); // Focus in: "ESC[I" | Focus out: "ESC[O"
const char* utils_iomanip_report_focus_disabled(void);
const char* utils_iomanip_report_past_enable(void); // Reports for pasted data: "ESC[200~{data}ESC[201~"
const char* utils_iomanip_report_past_disable(void);
const char* utils_iomanip_csi(const char* code); // ESC[ + code
size_t utils_iomanip_csi_r(const char* code, char* buf, size_t size);
const char* utils_iomanip_hyperlink(const char* display, const char* link);
size_t utils_iomanip_hyperlink_r(const char* display, const char* link, char* buf, size_t size);
const char* utils_iomanip_file_hyperlink(const char* display, const char* path);
size_t utils_iomanip_file_hyperlink_r(const char* display, const char* path, char* buf, size_t size);
const char* utils_iomanip_set_style(utils_iomanip_style_t style);
size_t utils_iomanip_set_style_r(utils_iomanip_style_t style, char* buf, size_t size);
const char* utils_iomanip_set_styles(const utils_iomanip_style_t* styles, size_t count); // every style in one sequence (utils::iomanip::set_style({...}))
size_t utils_iomanip_set_styles_r(const utils_iomanip_style_t* styles, size_t count, char* buf, size_t size);
const char* utils_iomanip_reset_style(utils_iomanip_reset_style_t style);
size_t utils_iomanip_reset_style_r(utils_iomanip_reset_style_t style, char* buf, size_t size);
const char* utils_iomanip_reset_styles(const utils_iomanip_reset_style_t* styles, size_t count); // every reset in one sequence (utils::iomanip::reset_style({...}))
size_t utils_iomanip_reset_styles_r(const utils_iomanip_reset_style_t* styles, size_t count, char* buf, size_t size);
const char* utils_iomanip_color(utils_iomanip_color_t color);
size_t utils_iomanip_color_r(utils_iomanip_color_t color, char* buf, size_t size);
const char* utils_iomanip_back_color(utils_iomanip_back_color_t color);
size_t utils_iomanip_back_color_r(utils_iomanip_back_color_t color, char* buf, size_t size);
const char* utils_iomanip_color_id(uint8_t id); // 256 colors
size_t utils_iomanip_color_id_r(uint8_t id, char* buf, size_t size);
const char* utils_iomanip_back_color_id(uint8_t id);
size_t utils_iomanip_back_color_id_r(uint8_t id, char* buf, size_t size);
const char* utils_iomanip_underline_color_id(uint8_t id);
size_t utils_iomanip_underline_color_id_r(uint8_t id, char* buf, size_t size);
const char* utils_iomanip_color_rgb(uint8_t r, uint8_t g, uint8_t b);
size_t utils_iomanip_color_rgb_r(uint8_t r, uint8_t g, uint8_t b, char* buf, size_t size);
const char* utils_iomanip_back_color_rgb(uint8_t r, uint8_t g, uint8_t b);
size_t utils_iomanip_back_color_rgb_r(uint8_t r, uint8_t g, uint8_t b, char* buf, size_t size);
const char* utils_iomanip_underline_color_rgb(uint8_t r, uint8_t g, uint8_t b);
size_t utils_iomanip_underline_color_rgb_r(uint8_t r, uint8_t g, uint8_t b, char* buf, size_t size);
const char* utils_iomanip_up(size_t n);
size_t utils_iomanip_up_r(size_t n, char* buf, size_t size);
const char* utils_iomanip_down(size_t n);
size_t utils_iomanip_down_r(size_t n, char* buf, size_t size);
const char* utils_iomanip_right(size_t n);
size_t utils_iomanip_right_r(size_t n, char* buf, size_t size);
const char* utils_iomanip_left(size_t n);
size_t utils_iomanip_left_r(size_t n, char* buf, size_t size);
const char* utils_iomanip_next_line(size_t n);
size_t utils_iomanip_next_line_r(size_t n, char* buf, size_t size);
const char* utils_iomanip_previous_line(size_t n);
size_t utils_iomanip_previous_line_r(size_t n, char* buf, size_t size);
const char* utils_iomanip_column(size_t col);
size_t utils_iomanip_column_r(size_t col, char* buf, size_t size);
const char* utils_iomanip_pos(size_t row, size_t col);
size_t utils_iomanip_pos_r(size_t row, size_t col, char* buf, size_t size);
const char* utils_iomanip_scroll_up(size_t n);
size_t utils_iomanip_scroll_up_r(size_t n, char* buf, size_t size);
const char* utils_iomanip_scroll_down(size_t n);
size_t utils_iomanip_scroll_down_r(size_t n, char* buf, size_t size);
```

## `utils/manip/iomanip/CApi.hpp`

Macros of the C API of utils::iomanip (expanded in the .cpp)

```cpp
#define C_API_STATIC_SEQUENCE(call) utils::exception::c_api::guard_fallback(nullptr, [](void) -> const char* {static const std::string seq = (call); return seq.c_str();})

```

## `utils/manip/iomanip/Char.hpp`

Definition of some special char

Namespace: `utils::iomanip`

```cpp
// namespace utils::iomanip
enum class Char: char {EOF, NUL, SOH, STX, ETX, EOT, ENQ, ACK, BEL, BS, HT, LF, VT, FF, CR, SO, SI, DLE, DC1, DC2, DC3, DC4, NAK, SYN, ETB, CAN, EM, SUB, ESC, FS, GS, RS, US, DEL}
```

## `utils/manip/iomanip/Color.hpp`

Definition of color used in ANSI escape sequences

Namespace: `utils::iomanip`

```cpp
// namespace utils::iomanip
enum class Color: std::uint8_t {Black, Red, Green, Yellow, Blue, Magenta, Cyan, White, BrightBlack, BrightRed, BrightGreen, BrightYellow, BrightBlue, BrightMagenta, BrightCyan, BrightWhite, Default}
enum class BackColor: std::uint8_t {Black, Red, Green, Yellow, Blue, Magenta, Cyan, White, BrightBlack, BrightRed, BrightGreen, BrightYellow, BrightBlue, BrightMagenta, BrightCyan, BrightWhite, Default}
```

## `utils/manip/iomanip/Style.hpp`

Define of the different style used in ANSI

Namespace: `utils::iomanip`

```cpp
// namespace utils::iomanip
enum class Style: std::uint8_t {Strong, Dark, Italic, Underlined, FlashingSlow, FlashingFast, Reversed, Hide, Bar, Monospace, Framed, Encircled, Overlined, Exposant, Indice}
enum class ResetStyle: std::uint8_t {All, Strong, Dark, Italic, Underlined, FlashingFast, FlashingSlow, Reversed, Hide, Bar, FramedEncircled, Overlined, UnderlineColor, ExposantIndice}
```

## `utils/manip/smanip/CApi.h`

C API of utils::smanip: byte strings, format, codecs & EETP parser (C & C++ compatible) Every function that can fail returns UTILS_OK / UTILS_KO (or NULL / 0), details in utils_exception Every callback returns UTILS_OK or UTILS_KO (info: utils_fail / utils_exception_set)

```c
// global
typedef struct utils_string utils_string_t; // std::string (bytes, can hold '\0')
typedef struct utils_codec utils_codec_t; // utils::smanip::codec::ICodec (Base64Codec or a codec of the C side)
typedef struct utils_eetp_content utils_eetp_content_t; // utils::smanip::parser::EETPContent
typedef struct utils_eetp_parser utils_eetp_parser_t; // utils::smanip::parser::EETPParser
typedef int (*utils_codec_fn_t)(const char* in, size_t size, utils_string_t* out, void* data);
utils_string_t* utils_string_create(const char* data, size_t size); // NULL / 0: empty, NULL on error
void utils_string_destroy(utils_string_t* str); // NULL: nothing
int utils_string_set(utils_string_t* str, const char* data, size_t size);
int utils_string_append(utils_string_t* str, const char* data, size_t size);
int utils_string_clear(utils_string_t* str);
const char* utils_string_data(const utils_string_t* str); // NULL on error
size_t utils_string_size(const utils_string_t* str);
size_t utils_string_copy_r(const utils_string_t* str, char* buf, size_t size); // copied in buf (truncated, always '\0' terminated), returns the full length like snprintf (0 on error)
const char* utils_smanip_format(const char* str); // valid until the next call in the thread (NULL on error)
size_t utils_smanip_format_r(const char* str, char* buf, size_t size); // copied in buf (truncated, always '\0' terminated), returns the full length like snprintf (0 on error)
utils_codec_t* utils_codec_create_base64(void); // utils::smanip::codec::Base64Codec, NULL on error
utils_codec_t* utils_codec_create(utils_codec_fn_t encode, utils_codec_fn_t decode, void* data); // codec of the C side, NULL on error
void utils_codec_destroy(utils_codec_t* codec); // NULL: nothing
int utils_codec_encode(const utils_codec_t* codec, const char* in, size_t size, utils_string_t* out); // out replaced
int utils_codec_decode(const utils_codec_t* codec, const char* in, size_t size, utils_string_t* out); // out replaced
utils_eetp_content_t* utils_eetp_content_create(const char* type); // NULL on error
void utils_eetp_content_destroy(utils_eetp_content_t* content); // NULL: nothing
int utils_eetp_content_set_type(utils_eetp_content_t* content, const char* type);
const char* utils_eetp_content_get_type(const utils_eetp_content_t* content); // valid until content is changed (NULL on error)
int utils_eetp_content_add_data(utils_eetp_content_t* content, const char* data, size_t size);
int utils_eetp_content_clear_data(utils_eetp_content_t* content);
size_t utils_eetp_content_data_count(const utils_eetp_content_t* content);
const char* utils_eetp_content_get_data(const utils_eetp_content_t* content, size_t i, size_t* size); // valid until content is changed, size: optional (NULL on error)
utils_eetp_parser_t* utils_eetp_parser_create(void); // Base64Codec, type size 1 (needs the common RSA key ~/.ssh/common), NULL on error
utils_eetp_parser_t* utils_eetp_parser_create_with(utils_codec_t* codec, size_t type_size); // NULL on error (the codec is still owned by the caller)
void utils_eetp_parser_destroy(utils_eetp_parser_t* parser); // NULL: nothing
int utils_eetp_parser_set_codec(utils_eetp_parser_t* parser, utils_codec_t* codec);
int utils_eetp_parser_set_type_size(utils_eetp_parser_t* parser, size_t type_size); // at least 1
bool utils_eetp_parser_has_id_overload(const utils_eetp_parser_t* parser);
bool utils_eetp_parser_has_no_id_overload(const utils_eetp_parser_t* parser);
int utils_eetp_parser_format(utils_eetp_parser_t* parser, const char* id, const utils_eetp_content_t* content, utils_string_t* out); // out replaced
int utils_eetp_parser_parse(utils_eetp_parser_t* parser, const char* id, const char* in, size_t size, utils_eetp_content_t* out); // out replaced
```

## `utils/manip/smanip/CApi.hpp`

String tools of the C API (copy in the fixed buffers of the status & in the buffers of the user) & handles of the C API of smanip

Namespace: `utils::smanip::c_api`

```cpp
// namespace utils::smanip::c_api
template<std::size_t N> void copy(char (&dst)[N], const char* src) noexcept;
std::size_t copy_r(const std::string& str, char* buf, const std::size_t size);
std::string bytes(const void* data, const std::size_t size, const char* name = "data");
// global
struct utils_string: public utils_handle {
    std::string str;
    utils_string(std::string s = "");
};
struct utils_codec: public utils_handle {
    std::unique_ptr<utils::smanip::codec::ICodec> codec;
    utils_codec(std::unique_ptr<utils::smanip::codec::ICodec> c);
};
struct utils_eetp_content: public utils_handle {
    utils::smanip::parser::EETPContent content;
    utils_eetp_content(utils::smanip::parser::EETPContent c);
};
struct utils_eetp_parser: public utils_handle {
    utils::smanip::parser::EETPParser parser;
};
```

## `utils/manip/smanip/FixedString.hpp`

Fixed string used in template definition

Namespace: `utils::smanip`

```cpp
// namespace utils::smanip
template<std::size_t N> struct FixedString {
    constexpr std::string_view view(void) const noexcept;
    constexpr std::size_t size(void) const noexcept;
    consteval FixedString(const char (&str)[N]);
};
template<std::size_t N> FixedString(const char (&)[N]) -> FixedString<N>;
```

## `utils/manip/smanip/codec/Base64Codec.hpp`

Definition of the base 64 codec

Namespace: `utils::smanip::codec`

```cpp
// namespace utils::smanip::codec
class Base64Codec: public utils::smanip::codec::ICodec {
    std::string encode(std::string s) const;
    std::string decode(std::string s) const;
    Base64Codec& operator=(const Base64Codec& other) = default;
    Base64Codec& operator=(Base64Codec&& other) = default;
    Base64Codec() = default;
    Base64Codec(const Base64Codec& other) = default;
    Base64Codec(Base64Codec&& other) = default;
    ~Base64Codec() = default;
};
```

## `utils/manip/smanip/codec/Codec.hpp`

Include for all the different codec

## `utils/manip/smanip/codec/ICodec.hpp`

Declaration of the interface used for different codec (base64, ...)

Namespace: `utils::smanip::codec`

```cpp
// namespace utils::smanip::codec
class ICodec: private utils::security::observer::Observer<"ICodec"> {
    virtual std::string encode(std::string s) const = 0;
    virtual std::string decode(std::string s) const = 0;
    ICodec& operator=(const ICodec& other) = default;
    ICodec& operator=(ICodec&& other) = default;
    ICodec() = default;
    ICodec(const ICodec& other) = default;
    ICodec(ICodec&& other) = default;
    virtual ~ICodec() = default;
};
```

## `utils/manip/smanip/format.hpp`

Definition of the utils::smanip::format & explication

Namespace: `utils::smanip`

```cpp
// namespace utils::smanip
std::string format(const std::string& s);
```

## `utils/manip/smanip/parser/AParser.hpp`

Declaration of the abstract used for different parser (2etp, ...)

Namespace: `utils::smanip::parser`

```cpp
// namespace utils::smanip::parser
template<typename T> class AParser: public utils::smanip::parser::IParser<T> {
    std::string format(T content) const;
    T parse(std::string s) const;
    std::string format(std::string id, T content);
    T parse(std::string id, std::string s);
    bool hasIdOverload(void) const;
    bool hasNoIdOverload(void) const;
    AParser& operator=(AParser&& other) = default;
    AParser() = default;
    AParser(AParser&& other) = default;
    virtual ~AParser() = default;
};
```

## `utils/manip/smanip/parser/EETPParser.hpp`

Declaration of the parser used for the 2etp protocol

Namespace: `utils::smanip::parser`

```cpp
#define EETP_AES_KEY_SIZE 32 // bytes (AES-256)
#define EETP_AES_IV_SIZE 12 // bytes (GCM nonce)
#define EETP_AES_TAG_SIZE 16 // bytes (GCM tag)

// namespace utils::smanip::parser
struct EETPContent {
    std::string type;
    std::vector<std::string> data;
};
class EETPParser: public utils::smanip::parser::AParser<utils::smanip::parser::EETPContent> {
    std::string format(std::string id, utils::smanip::parser::EETPContent content);
    utils::smanip::parser::EETPContent parse(std::string id, std::string s);
    void setCodec(std::unique_ptr<utils::smanip::codec::ICodec> codec); // default: Base64Codec
    void setTypeSize(std::size_t typeSize); // default: 1 (0-127)
    bool hasIdOverload(void) const;
    EETPParser& operator=(EETPParser&& other) = default;
    EETPParser();
    EETPParser(std::unique_ptr<utils::smanip::codec::ICodec> codec, std::size_t typeSize = 1);
    EETPParser(EETPParser&& other) = default;
    virtual ~EETPParser() = default;
};
```

## `utils/manip/smanip/parser/IParser.hpp`

Declaration of the interface used for different parser (2etp, ...)

Namespace: `utils::smanip::parser`

```cpp
// namespace utils::smanip::parser
template<typename T> class IParser: private utils::security::observer::Observer<"IParser"> {
    virtual std::string format(T content) const = 0;
    virtual T parse(std::string s) const = 0;
    virtual std::string format(std::string id, T content) = 0;
    virtual T parse(std::string id, std::string s) = 0;
    virtual bool hasIdOverload(void) const = 0;
    virtual bool hasNoIdOverload(void) const = 0;
    IParser& operator=(IParser&& other) = default;
    IParser() = default;
    IParser(IParser&& other) = default;
    virtual ~IParser() = default;
};
```

## `utils/manip/smanip/parser/Parser.hpp`

Include for all the different parser
