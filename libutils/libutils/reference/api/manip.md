# libutils `manip`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

## `utils/manip/iomanip/ANSI.hpp`

Definition of ANSI escape sequences

Namespace: `utils::iomanip`

```cpp
// namespace utils::iomanip
std::string set_style(std::initializer_list<utils::iomanip::Style> styles);
std::string reset_style(std::initializer_list<utils::iomanip::ResetStyle> styles);
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
[deprecated ~v4.0.0] std::string setStyle(std::initializer_list<utils::iomanip::Style> styles);
[deprecated ~v4.0.0] std::string setStyle(utils::iomanip::Style style);
[deprecated ~v4.0.0] std::string resetStyle(std::initializer_list<utils::iomanip::ResetStyle> styles);
[deprecated ~v4.0.0] std::string resetStyle(utils::iomanip::ResetStyle style);
[deprecated ~v4.0.0] std::pair<int, int> readCursorPosition(void);
[deprecated ~v4.0.0] utils::iomanip::MouseEvent readMouseEvent(void);
[deprecated ~v4.0.0] utils::iomanip::AdvancedMouseEvent readAdvancedMouseEvent(void);
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

## `utils/manip/smanip/fixed_string.hpp`

Old name of the FixedString (kept for backward compatibility)

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
