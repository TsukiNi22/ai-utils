# libutils `arguments`

Generated from libutils `v5.0.0` (commit `6aac89f`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

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
    utils::arguments::ParsedUsages parse(const int argc, const char* const argv[], const bool failsafe = false) const;
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
using ParsedUsages = std::vector<utils::arguments::ParsedUsage>;
```

## `utils/arguments/CApi.h`

C API of utils::arguments: ArgParser (usages, options, flags, parsing) & Settings (typed settings) (C & C++ compatible) Every function that can fail returns UTILS_OK / UTILS_KO (or NULL / 0), details in utils_exception Every check callback returns UTILS_OK (valid) or UTILS_KO (its message: the info of utils_fail / utils_exception_set)

```c
#define C_API_SETTINGS_DECL(name, ctype) // declares, for each type:
    int utils_settings_add_<name>(utils_settings_t* settings, const char* id, ctype value); // an existing one is refused
    int utils_settings_set_<name>(utils_settings_t* settings, const char* id, ctype value, bool force); // force: override an existing one
    int utils_settings_get_<name>(const utils_settings_t* settings, const char* id, ctype* value); // UTILS_KO: unknown id or another type
    bool utils_settings_is_<name>(const utils_settings_t* settings, const char* id);

// global
typedef struct utils_argparser utils_argparser_t; // utils::arguments::ArgParser
typedef struct utils_argparser_result utils_argparser_result_t; // utils::arguments::ParsedUsages (return of the parsing)
typedef struct utils_settings utils_settings_t; // utils::arguments::Settings
typedef int (*utils_argparser_check_t)(const char* option, void* data); // UTILS_OK: valid, UTILS_KO: the message is the info of utils_fail
typedef int (*utils_argparser_help_hook_t)(const utils_argparser_t* parser, void* data); // display the help
typedef enum utils_settings_cast { C_API_SETTINGS_CAST_NONE = 0, C_API_SETTINGS_CAST_BYTE, C_API_SETTINGS_CAST_BOOL, C_API_SETTINGS_CAST_INT8, C_API_SETTINGS_CAST_INT16, C_API_SETTINGS_CAST_INT32, C_API_SETTINGS_CAST_INT64, C_API_SETTINGS_CAST_UINT8, C_API_SETTINGS_CAST_UINT16, C_API_SETTINGS_CAST_UINT32, C_API_SETTINGS_CAST_UINT64, C_API_SETTINGS_CAST_FLOAT16, C_API_SETTINGS_CAST_FLOAT32, C_API_SETTINGS_CAST_FLOAT64, C_API_SETTINGS_CAST_FLOAT128, C_API_SETTINGS_CAST_CHAR8, C_API_SETTINGS_CAST_CHAR16, C_API_SETTINGS_CAST_CHAR32, C_API_SETTINGS_CAST_U8STRING, C_API_SETTINGS_CAST_U16STRING, C_API_SETTINGS_CAST_U32STRING, C_API_SETTINGS_CAST_WCHAR, C_API_SETTINGS_CAST_WSTRING, C_API_SETTINGS_CAST_PATH, C_API_SETTINGS_CAST_OTHER = 1000 };
typedef struct utils_argparser_flag_option { const char* name; bool mandatory; utils_argparser_check_t check; void* data; };
utils_argparser_t* utils_argparser_create(const char* binary, const char* description); // NULL: "[None]" / "...", NULL on error
void utils_argparser_destroy(utils_argparser_t* parser); // NULL: nothing
int utils_argparser_help(const utils_argparser_t* parser); // display the help (help hook)
utils_argparser_result_t* utils_argparser_parse(const utils_argparser_t* parser, int argc, const char* const* argv, bool failsafe);
int utils_argparser_set_default_usage(utils_argparser_t* parser); // "default": every flag & option
int utils_argparser_set_usage(utils_argparser_t* parser, const char* id, const char* name, bool ordered, const char* const* ids, const bool* mandatory, size_t count, const char* description, bool force); // description: NULL = "[None]"
int utils_argparser_set_option(utils_argparser_t* parser, const char* id, const char* name, utils_argparser_check_t check, void* data, const char* description, bool force); // check: NULL = no check
int utils_argparser_set_flag(utils_argparser_t* parser, const char* id, const char* short_flag, const char* flag, const char* long_flag, const char* env, const utils_argparser_flag_option_t* options, size_t count, const char* description, bool unlimited, bool ignore_case, bool force); // NULL flag name: "" (not used)
int utils_argparser_remove_usage(utils_argparser_t* parser, const char* id);
int utils_argparser_remove_usages(utils_argparser_t* parser, const char* const* ids, size_t count);
int utils_argparser_remove_option(utils_argparser_t* parser, const char* id);
int utils_argparser_remove_options(utils_argparser_t* parser, const char* const* ids, size_t count);
int utils_argparser_remove_flag(utils_argparser_t* parser, const char* id);
int utils_argparser_remove_flags(utils_argparser_t* parser, const char* const* ids, size_t count);
int utils_argparser_reset_usages(utils_argparser_t* parser);
int utils_argparser_reset_options(utils_argparser_t* parser);
int utils_argparser_reset_flags(utils_argparser_t* parser);
int utils_argparser_set_help_hook(utils_argparser_t* parser, utils_argparser_help_hook_t hook, void* data);
int utils_argparser_reset_help_hook(utils_argparser_t* parser);
int utils_argparser_default_help_hook(const utils_argparser_t* parser);
int utils_argparser_check_true(const char* option, void* data); // always valid
int utils_argparser_check_bool(const char* option, void* data); // 0, 1, false, true
int utils_argparser_check_int32(const char* option, void* data);
int utils_argparser_check_sizet(const char* option, void* data);
int utils_argparser_check_double(const char* option, void* data);
int utils_argparser_check_file(const char* option, void* data); // readable file
int utils_argparser_check_directory(const char* option, void* data); // readable directory
int utils_argparser_check_writable(const char* option, void* data); // readable & writable path
int utils_argparser_disable_help(utils_argparser_t* parser); // no default -h
int utils_argparser_set_binary(utils_argparser_t* parser, const char* binary);
int utils_argparser_set_description(utils_argparser_t* parser, const char* description);
const char* utils_argparser_get_binary(const utils_argparser_t* parser);
const char* utils_argparser_get_description(const utils_argparser_t* parser);
size_t utils_argparser_usages_count(const utils_argparser_t* parser);
const char* utils_argparser_usage_id(const utils_argparser_t* parser, size_t i);
int utils_argparser_get_usage(const utils_argparser_t* parser, const char* id, const char** name, bool* ordered, const char** description); // each output: optional
size_t utils_argparser_usage_ids_count(const utils_argparser_t* parser, const char* id);
int utils_argparser_usage_get_id(const utils_argparser_t* parser, const char* id, size_t i, const char** option_id, bool* mandatory);
size_t utils_argparser_options_count(const utils_argparser_t* parser);
const char* utils_argparser_option_id(const utils_argparser_t* parser, size_t i);
int utils_argparser_get_option(const utils_argparser_t* parser, const char* id, const char** name, bool* exact, const char** description); // each output: optional
size_t utils_argparser_flags_count(const utils_argparser_t* parser);
const char* utils_argparser_flag_id(const utils_argparser_t* parser, size_t i);
int utils_argparser_get_flag(const utils_argparser_t* parser, const char* id, const char** short_flag, const char** flag, const char** long_flag, const char** env, const char** description, bool* unlimited); // each output: optional
size_t utils_argparser_flag_options_count(const utils_argparser_t* parser, const char* id);
int utils_argparser_flag_get_option(const utils_argparser_t* parser, const char* id, size_t i, const char** name, bool* mandatory);
void utils_argparser_result_destroy(utils_argparser_result_t* result); // NULL: nothing
size_t utils_argparser_result_count(const utils_argparser_result_t* result); // number of usages
const char* utils_argparser_result_usage(const utils_argparser_result_t* result, size_t i); // id of the usage i ("": nothing), NULL on error
size_t utils_argparser_result_arguments_count(const utils_argparser_result_t* result, size_t i);
const char* utils_argparser_result_argument_id(const utils_argparser_result_t* result, size_t i, size_t j); // NULL on error
bool utils_argparser_result_argument_is_option(const utils_argparser_result_t* result, size_t i, size_t j); // option (true) or flag (false)
size_t utils_argparser_result_values_count(const utils_argparser_result_t* result, size_t i, size_t j);
const char* utils_argparser_result_value(const utils_argparser_result_t* result, size_t i, size_t j, size_t k); // NULL on error
utils_settings_t* utils_settings_create(void); // NULL on error
void utils_settings_destroy(utils_settings_t* settings); // NULL: nothing
int utils_settings_auto_cast(utils_settings_t* settings, const char* id, const char* setting, bool force, utils_settings_cast_t* type); // type found & stored (type: optional)
int utils_settings_cast(utils_settings_t* settings, const char* id, const char* setting, utils_settings_cast_t type, bool force);
int utils_settings_remove(utils_settings_t* settings, const char* id, bool failsafe); // failsafe: an unknown id is not an error
int utils_settings_clear(utils_settings_t* settings);
bool utils_settings_contains(const utils_settings_t* settings, const char* id);
utils_settings_cast_t utils_settings_get_type(const utils_settings_t* settings, const char* id); // C_API_SETTINGS_CAST_OTHER: unknown id (error) or a type without cast
C_API_TYPES(C_API_SETTINGS_DECL)
C_API_SETTINGS_DECL(byte, uint8_t) // std::byte
C_API_SETTINGS_DECL(char8, uint8_t) // char8_t
C_API_SETTINGS_DECL(char16, uint_least16_t) // char16_t
C_API_SETTINGS_DECL(char32, uint_least32_t) // char32_t
C_API_SETTINGS_DECL(wchar, wchar_t)
C_API_SETTINGS_DECL(u8string, const char*) // std::u8string (utf-8)
C_API_SETTINGS_DECL(path, const char*) // std::filesystem::path (the path given back: valid until the next utils_settings_get_path of the thread)
int utils_settings_set_u16string(utils_settings_t* settings, const char* id, const uint_least16_t* str, size_t size, bool force); // std::u16string
int utils_settings_get_u16string(const utils_settings_t* settings, const char* id, const uint_least16_t** str, size_t* size);
bool utils_settings_is_u16string(const utils_settings_t* settings, const char* id);
int utils_settings_set_u32string(utils_settings_t* settings, const char* id, const uint_least32_t* str, size_t size, bool force); // std::u32string
int utils_settings_get_u32string(const utils_settings_t* settings, const char* id, const uint_least32_t** str, size_t* size);
bool utils_settings_is_u32string(const utils_settings_t* settings, const char* id);
int utils_settings_set_wstring(utils_settings_t* settings, const char* id, const wchar_t* str, size_t size, bool force); // std::wstring
int utils_settings_get_wstring(const utils_settings_t* settings, const char* id, const wchar_t** str, size_t* size);
bool utils_settings_is_wstring(const utils_settings_t* settings, const char* id);
int utils_settings_set_bytes(utils_settings_t* settings, const char* id, const void* data, size_t size, bool force);
int utils_settings_get_bytes(const utils_settings_t* settings, const char* id, const void** data, size_t* size);
bool utils_settings_is_bytes(const utils_settings_t* settings, const char* id);
```

## `utils/arguments/CApi.hpp`

Handles of the C API of utils::arguments (usable in c++ to get the object)

```cpp
#define C_API_SETTINGS_DEF(name, ctype) // declares, for each type:
    int utils_settings_add_<name>(utils_settings_t* settings, const char* id, ctype value)
    {return utils::exception::c_api::guard([&](void) {store(settings_of(settings), id, utils::type::c_api::Storage<ctype>::cpp(value), true, false);});}
    int utils_settings_set_<name>(utils_settings_t* settings, const char* id, ctype value, bool force)
    {return utils::exception::c_api::guard([&](void) {store(settings_of(settings), id, utils::type::c_api::Storage<ctype>::cpp(value), false, force);});}
    int utils_settings_get_<name>(const utils_settings_t* settings, const char* id, ctype* value)
    {
    return utils::exception::c_api::guard([&](void) {
    ctype& res = utils::exception::c_api::not_null_ref(value, "value");
    res = utils::type::c_api::Storage<ctype>::c(settings_of(settings).template get<utils::type::c_api::cpp_t<ctype>>(utils::exception::c_api::not_null(id, "id")));
    });
    }
    bool utils_settings_is_<name>(const utils_settings_t* settings, const char* id)
    {return utils::exception::c_api::guard_fallback(false, [&](void) -> bool {return setting_is<utils::type::c_api::cpp_t<ctype>>(settings, id);});}
#define C_API_SETTINGS_CAST_DEF(name, ctype, cpptype, to_cpp, to_c) // declares, for each type:
    int utils_settings_add_<name>(utils_settings_t* settings, const char* id, ctype value)
    {return utils::exception::c_api::guard([&](void) {store(settings_of(settings), id, cpptype(to_cpp), true, false);});}
    int utils_settings_set_<name>(utils_settings_t* settings, const char* id, ctype value, bool force)
    {return utils::exception::c_api::guard([&](void) {store(settings_of(settings), id, cpptype(to_cpp), false, force);});}
    int utils_settings_get_<name>(const utils_settings_t* settings, const char* id, ctype* value)
    {
    return utils::exception::c_api::guard([&](void) {
    ctype& res = utils::exception::c_api::not_null_ref(value, "value");
    const cpptype& stored = settings_of(settings).template get<cpptype>(utils::exception::c_api::not_null(id, "id"));
    res = (to_c);
    });
    }
    bool utils_settings_is_<name>(const utils_settings_t* settings, const char* id)
    {return utils::exception::c_api::guard_fallback(false, [&](void) -> bool {return setting_is<cpptype>(settings, id);});}
#define C_API_SETTINGS_STRING_DEF(name, ctype, cpptype) // declares, for each type:
    int utils_settings_set_<name>(utils_settings_t* settings, const char* id, const ctype* str, size_t size, bool force)
    {
    return utils::exception::c_api::guard([&](void) {
    if (size > 0) (void)utils::exception::c_api::not_null(str, "string");
    store(settings_of(settings), id, (size > 0 ? cpptype(reinterpret_cast<const cpptype::value_type*>(str), size) : cpptype()), false, force);
    });
    }
    int utils_settings_get_<name>(const utils_settings_t* settings, const char* id, const ctype** str, size_t* size)
    {
    return utils::exception::c_api::guard([&](void) {
    const ctype*& res = utils::exception::c_api::not_null_ref(str, "string");
    const cpptype& stored = settings_of(settings).template get<cpptype>(utils::exception::c_api::not_null(id, "id"));
    res = reinterpret_cast<const ctype*>(stored.c_str());
    if (size) *size = stored.size();
    });
    }
    bool utils_settings_is_<name>(const utils_settings_t* settings, const char* id)
    {return utils::exception::c_api::guard_fallback(false, [&](void) -> bool {return setting_is<cpptype>(settings, id);});}

// global
static const char* path_c(const std::filesystem::path& path);
struct utils_argparser: public utils_handle, public utils::arguments::ArgParser {
    utils_argparser(const std::string& binary, const std::string& description);
};
struct utils_argparser_result: public utils_handle {
    utils::arguments::ParsedUsages usages;
};
struct utils_settings: public utils_handle, public utils::arguments::Settings {
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
