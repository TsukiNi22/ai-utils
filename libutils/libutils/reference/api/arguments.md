# libutils `arguments`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

## `utils/arguments/ArgParser.hpp`

Declaration of the ArgParser class for arguments handling

Namespace: `utils::arguments`

```cpp
// namespace utils::arguments
void defaultHelpHook(const utils::arguments::ArgParser& parser);
std::optional<std::string> defaultBoolParsingHook(const std::string& option); // Parse boolean (0, 1, fase, true)
std::optional<std::string> defaultInt32ParsingHook(const std::string& option); // Parse std::int32_t
std::optional<std::string> defaultSizetParsingHook(const std::string& option); // Parse std::size_t
std::optional<std::string> defaultDoubleParsingHook(const std::string& option); // Parse double
std::optional<std::string> defaultFileParsingHook(const std::string& option); // Check for file reading (only!)
std::optional<std::string> defaultDirectoryParsingHook(const std::string& option); // Check for directory reading (only!)
std::optional<std::string> defaultWritableParsingHook(const std::string& option); // Check if the path/file is readable & writable (only!)
std::optional<std::string> defaultTrueParsingHook(const std::string&);
class ArgParser: private utils::security::observer::Observer<"ArgParser"> {
    void help(void) const; // Help display (using hook)
    utils::arguments::ParsedUsages parse(const int argc, const char *const argv[], const bool failsafe = false) const;
    utils::arguments::ParsedUsages parse(const std::vector<std::string>& argv, const bool failsafe = false) const;
    void removeUsage(const std::string& id);
    void removeUsages(const std::vector<std::string>& ids);
    void removeOption(const std::string& id);
    void removeOptions(const std::vector<std::string>& ids);
    void removeFlag(const std::string& id);
    void removeFlags(const std::vector<std::string>& ids);
    void setDefaultUsage(void);
    template<bool force = false> void setUsage(const std::string& id, const std::string& name, const bool ordered, const std::vector<std::pair<std::string, bool>>& ids, const std::string& description = "[None]");
    void resetUsages(void);
    template<bool force = false> void setOption(const std::string& id, const std::string& name, std::function<std::optional<std::string>(const std::string&)> check, const std::string& description = "[None]");
    template<bool force = false> void setOption(const std::string& id, const std::string& name, const std::string& description = "[None]");
    void resetOptions(void);
    template<bool force = false> void setFlag(const std::string& id, const std::tuple<std::string, std::string, std::string, std::string>& flag, const std::vector<std::tuple<std::string, bool, std::function<std::optional<std::string>(const std::string&)>>>& options, const std::string& description = "[None]", const bool unlimited = false, const bool ignore_case = false);
    void resetFlags(void);
    void setHelpHook(std::function<void(const utils::arguments::ArgParser& parser)> hook);
    void resetHelpHook(void);
    void disableHelp(void);
    void setBinary(const std::string& binary);
    void setDescription(const std::string& description);
    const std::string& getBinary(void) const;
    const std::string& getDescription(void) const;
    const std::unordered_map<std::string, utils::arguments::Usage>& getUsages(void) const;
    const std::unordered_map<std::string, utils::arguments::Option>& getOptions(void) const;
    const std::unordered_map<std::string, utils::arguments::Flag>& getFlags(void) const;
    ArgParser(const std::string& binary = "[None]", const std::string& description = "...");
    ~ArgParser() = default;
};
```

## `utils/arguments/ArgParserType.hpp`

Declaration of the ArgParser type for void & non void function

Namespace: `utils::arguments`

```cpp
// namespace utils::arguments
struct ParsedUsageFull {
    std::string id; // empty -> nothing
    bool valid = true; // keep the usage for the final return
    bool ordered = false; // is the usage ordered
    std::deque<std::pair<std::string, bool>> ids; // ids that are still not used in the arguments
    std::vector<std::tuple<std::string, bool, std::vector<std::string>>> arguments; // <id, option(true)|flag(false), {option}>
};
struct ParsedUsage {
    std::string id; // empty -> nothing
    std::vector<std::tuple<std::string, bool, std::vector<std::string>>> arguments; // <id, option(true)|flag(false), {option}>
};
using ParsedUsages = std::vector<utils::arguments::ParsedUsage>;
struct Usage {
    std::string name; // special name: default -> allow all flag (like no usage defined)
    bool ordered = false; // The order of the option will always matter, Raw option will be forced in mandatory
    std::vector<std::pair<std::string, bool>> ids; // List of ids allowed by this usage <id, mandatory>
    std::string description = "[None]";
};
struct Option {
    std::string name;
    bool exact = false;
    std::function<std::optional<std::string>(const std::string&)> check;
    std::string description = "[None]";
};
struct Flag {
    std::tuple<std::string, std::string, std::string, std::string> flag; // <short, flag, long, env>
    std::pair<bool, bool> unlimited = {false, false}; // <ulimited, ignore flag>
    std::vector<std::tuple<std::string, bool, std::function<std::optional<std::string>(const std::string&)>>> options; // <name, mandatory, check>, the order matter
    std::string description = "[None]";
};
```

## `utils/arguments/Setting.hpp`

Declaration of the Setting class used in Settings

Namespace: `utils::arguments`

```cpp
// namespace utils::arguments
std::string demangle(const char* mangledName);
class Setting: private utils::security::observer::Observer<"Setting"> {
    template<typename T> const T& get(void) const;
    template<typename T> T& get(void); // edit in place
    template<typename T> bool is(void) const;
    template<typename T> void assign(T&& setting); // reuse the node (no Observer relink)
    Setting& operator=(Setting&& other) = default;
    template<typename T> operator T(void) const;
    template<typename T> requires (!std::same_as<std::remove_cvref_t<T>, utils::arguments::Setting>) Setting(T&& setting);
    Setting(Setting&& other) = default;
    ~Setting() = default;
};
```

## `utils/arguments/Settings.hpp`

Declaration of the Settings class used for settings handling

Namespace: `utils::arguments`

```cpp
// namespace utils::arguments
struct SettingsHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view key) const noexcept;
};
class Settings: private utils::security::observer::Observer<"Settings"> {
    const utils::arguments::Setting& at(std::string_view id) const;
    utils::arguments::Setting& at(std::string_view id);
    template<bool force = false> utils::arguments::CastType autoCast(const std::string& id, const std::string& setting);
    template<utils::arguments::CastType type, bool force = false> void cast(const std::string& id, const std::string& setting);
    template<typename T> void add(const std::string& id, T&& setting);
    template<typename T> void add(std::string&& id, T&& setting);
    template<bool force = true, typename T> void set(const std::string& id, T&& setting);
    template<bool force = true, typename T> void set(std::string&& id, T&& setting);
    template<bool failsafe = false> void remove(std::string_view id);
    void clear(void);
    template<typename T> const T& get(std::string_view id) const;
    template<typename T> T& get(std::string_view id);
    const utils::arguments::Setting& get(std::string_view id) const;
    bool contains(std::string_view id) const;
    template<bool force = false> [deprecated ~v4.0.0] utils::arguments::CastType auto_cast(const std::string& id, const std::string& setting);
    Settings& operator=(Settings&& other) = default;
    utils::arguments::Setting& operator[](std::string_view id);
    const utils::arguments::Setting& operator[](std::string_view id) const;
    Settings() = default;
    Settings(Settings&& other) = default;
    ~Settings() = default;
};
```

## `utils/arguments/SettingsDefine.hpp`

Enum & Include handling used in settings handling

Namespace: `utils::arguments`

```cpp
// namespace utils::arguments
using float16_t = std::float16_t;
using float32_t = std::float32_t;
using float64_t = std::float64_t;
using float128_t = std::float128_t;
using float16_t = float;
using float32_t = float;
using float64_t = double;
using float128_t = long double;
enum class CastType: std::size_t {None, Byte, Bool, Int8, Int16, Int32, Int64, UInt8, UInt16, UInt32, UInt64, Float16, Float32, Float64, Float128, Char8, Char16, Char32, U8String, U16String, U32String, WChar, WString, Path}
```
