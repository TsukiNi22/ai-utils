# libutils `cli`

Generated from libutils `v5.0.0` (commit `6aac89f`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/cli/CApi.h`

C API of utils::cli::Cli (C & C++ compatible) Every function that can fail returns UTILS_OK / UTILS_KO (or NULL / 0), details in utils_exception (utils_exception.exception: failure of the last call) Every callback returns UTILS_OK, UTILS_KO (info: utils_fail / utils_exception_set) or C_API_CLI_EXIT

```c
#define C_API_CLI_EXIT 1 // stop the cli (utils::exception::NoneException(Exit), like the exit command)
#define C_API_CLI_NO_INPUT 2 // getc hook only: no char available yet (called again)
#define C_API_CLI_NO_LIMIT -1 // no min / max number of arguments

// global
typedef struct utils_cli utils_cli_t; // utils::cli::Cli (+ its thread)
typedef struct utils_cli_parsed utils_cli_parsed_t; // utils::cli::ParsedData
typedef int (*utils_cli_parsed_command_t)(const utils_cli_t* cli, size_t argc, const char* const* argv, void* data); // argv[0] = the command
typedef int (*utils_cli_raw_command_t)(const utils_cli_t* cli, const char* input, void* data);
typedef int (*utils_cli_prompt_hook_t)(const utils_cli_t* cli, uint8_t code, void* data); // print the prompt
typedef int (*utils_cli_parser_hook_t)(const char* input, bool trim, bool logic, bool parse, utils_cli_parsed_t* out, void* data); // fill out (empty at the call)
typedef int (*utils_cli_getc_hook_t)(char* c, void* data); // UTILS_OK: *c read ('\0' = EOF), C_API_CLI_NO_INPUT: nothing yet
typedef int (*utils_cli_middleware_t)(void* data);
typedef int (*utils_cli_middleware_code_t)(uint8_t code, void* data);
typedef int (*utils_cli_middleware_char_t)(char c, void* data);
typedef int (*utils_cli_middleware_str_t)(const char* str, void* data);
typedef int (*utils_cli_middleware_parsed_t)(const utils_cli_parsed_t* parsed, void* data);
typedef enum utils_cli_flag { C_API_CLI_FLAG_DEBUG = 1 << 0, C_API_CLI_FLAG_NOECHO = 1 << 1, C_API_CLI_FLAG_CATCH = 1 << 2, C_API_CLI_FLAG_EMPTY_INPUT = 1 << 3, C_API_CLI_FLAG_TRIM = 1 << 4, C_API_CLI_FLAG_PARSED = 1 << 5, C_API_CLI_FLAG_PROMPT = 1 << 6, C_API_CLI_FLAG_LOGIC = 1 << 7, C_API_CLI_FLAG_ARROW = 1 << 8, C_API_CLI_FLAG_HISTORY = 1 << 9, C_API_CLI_FLAG_PERSISTENT = 1 << 10, C_API_CLI_FLAG_HINT = 1 << 11, C_API_CLI_FLAG_AUTO_COMPLETION = 1 << 12, C_API_CLI_FLAG_MANUAL = 1 << 13, C_API_CLI_FLAG_THREAD = 1 << 14, C_API_CLI_FLAG_DETACHED = 1 << 15, C_API_CLI_FLAG_NO_TTY = 1 << 16 };
typedef enum utils_cli_flags { C_API_CLI_FLAGS_ALL = C_API_CLI_FLAG_DEBUG | C_API_CLI_FLAG_CATCH | C_API_CLI_FLAG_NOECHO | C_API_CLI_FLAG_TRIM | C_API_CLI_FLAG_EMPTY_INPUT | C_API_CLI_FLAG_PARSED | C_API_CLI_FLAG_PROMPT | C_API_CLI_FLAG_LOGIC | C_API_CLI_FLAG_ARROW | C_API_CLI_FLAG_HISTORY | C_API_CLI_FLAG_HINT | C_API_CLI_FLAG_AUTO_COMPLETION | C_API_CLI_FLAG_MANUAL | C_API_CLI_FLAG_THREAD | C_API_CLI_FLAG_DETACHED, C_API_CLI_FLAGS_DEFAULT = C_API_CLI_FLAG_CATCH | C_API_CLI_FLAG_EMPTY_INPUT | C_API_CLI_FLAG_TRIM | C_API_CLI_FLAG_PROMPT | C_API_CLI_FLAG_ARROW, C_API_CLI_FLAGS_DUMB = 0, C_API_CLI_FLAGS_TERM1 = C_API_CLI_FLAG_CATCH | C_API_CLI_FLAG_EMPTY_INPUT | C_API_CLI_FLAG_TRIM | C_API_CLI_FLAG_PARSED | C_API_CLI_FLAG_PROMPT | C_API_CLI_FLAG_LOGIC | C_API_CLI_FLAG_ARROW | C_API_CLI_FLAG_HISTORY, C_API_CLI_FLAGS_TERM2 = C_API_CLI_FLAGS_TERM1 | C_API_CLI_FLAG_HINT | C_API_CLI_FLAG_AUTO_COMPLETION, C_API_CLI_FLAGS_TERM3 = C_API_CLI_FLAGS_TERM2 | C_API_CLI_FLAG_THREAD, C_API_CLI_FLAGS_LOG = C_API_CLI_FLAGS_TERM3 | C_API_CLI_FLAG_DETACHED | C_API_CLI_FLAG_NO_TTY, C_API_CLI_FLAGS_DEV = C_API_CLI_FLAGS_TERM2 | C_API_CLI_FLAG_DEBUG, C_API_CLI_FLAGS_MULTI_THREADING = C_API_CLI_FLAG_THREAD | C_API_CLI_FLAG_DETACHED };
typedef struct utils_cli_parsed_command_entry { const char* name; utils_cli_parsed_command_t fn; int16_t min; int16_t max; void* data; };
typedef struct utils_cli_raw_command_entry { const char* name; utils_cli_raw_command_t fn; void* data; };
utils_cli_t* utils_cli_create(bool sig); // NULL on error, sig: catch of ctrl-c & ctrl-z
void utils_cli_destroy(utils_cli_t* cli); // kill & join a running cli
int utils_cli_start(utils_cli_t* cli, const char* const* inputs, size_t count, size_t call); // inputs executed on start (NULL / 0: none)
void utils_cli_join(utils_cli_t* cli); // join the thread kept in the handle (THREAD) or wait the end of the cli
void utils_cli_interrupt(utils_cli_t* cli);
void utils_cli_kill(utils_cli_t* cli); // can't be undone
bool utils_cli_is_running(const utils_cli_t* cli);
bool utils_cli_was_interrupted(const utils_cli_t* cli);
bool utils_cli_was_killed(const utils_cli_t* cli);
bool utils_cli_was_stopped(const utils_cli_t* cli);
void utils_cli_reset_flags(utils_cli_t* cli);
void utils_cli_sub_flags(utils_cli_t* cli, uint32_t flags);
void utils_cli_add_flags(utils_cli_t* cli, uint32_t flags);
void utils_cli_set_flags(utils_cli_t* cli, uint32_t flags);
int utils_cli_set_parsed_command(utils_cli_t* cli, const char* name, utils_cli_parsed_command_t fn, int16_t min, int16_t max, void* data, bool force); // force: override an existing one
int utils_cli_set_raw_command(utils_cli_t* cli, const char* name, utils_cli_raw_command_t fn, void* data, bool force);
int utils_cli_set_parsed_commands(utils_cli_t* cli, const utils_cli_parsed_command_entry_t* commands, size_t count, bool force);
int utils_cli_set_raw_commands(utils_cli_t* cli, const utils_cli_raw_command_entry_t* commands, size_t count, bool force);
int utils_cli_del_command(utils_cli_t* cli, const char* name);
int utils_cli_del_commands(utils_cli_t* cli, const char* const* names, size_t count);
int utils_cli_reset_commands(utils_cli_t* cli); // default commands (exit, ...)
int utils_cli_clear_commands(utils_cli_t* cli);
int utils_cli_reset_hooks(utils_cli_t* cli);
int utils_cli_reset_prompt_hook(utils_cli_t* cli);
int utils_cli_reset_parser_hook(utils_cli_t* cli);
int utils_cli_reset_getc_hook(utils_cli_t* cli);
int utils_cli_set_prompt_hook(utils_cli_t* cli, utils_cli_prompt_hook_t hook, void* data);
int utils_cli_set_parser_hook(utils_cli_t* cli, utils_cli_parser_hook_t hook, void* data);
int utils_cli_set_getc_hook(utils_cli_t* cli, utils_cli_getc_hook_t hook, void* data);
int utils_cli_default_prompt_hook(const utils_cli_t* cli, uint8_t code, void* data);
int utils_cli_default_parser_hook(const char* input, bool trim, bool logic, bool parse, utils_cli_parsed_t* out, void* data);
int utils_cli_default_getc_hook(char* c, void* data);
int utils_cli_reset_middlewares(utils_cli_t* cli);
int utils_cli_cli_middlewares_add_before(utils_cli_t* cli, utils_cli_middleware_t fn, void* data); // When the cli start & end
int utils_cli_cli_middlewares_add_after(utils_cli_t* cli, utils_cli_middleware_t fn, void* data);
int utils_cli_cli_middlewares_clear(utils_cli_t* cli);
int utils_cli_error_middlewares_add_before(utils_cli_t* cli, utils_cli_middleware_code_t fn, void* data); // When an error is triggered
int utils_cli_error_middlewares_add_after(utils_cli_t* cli, utils_cli_middleware_code_t fn, void* data);
int utils_cli_error_middlewares_clear(utils_cli_t* cli);
int utils_cli_prompt_middlewares_add_before(utils_cli_t* cli, utils_cli_middleware_t fn, void* data); // When the prompt is displayed
int utils_cli_prompt_middlewares_add_after(utils_cli_t* cli, utils_cli_middleware_t fn, void* data);
int utils_cli_prompt_middlewares_clear(utils_cli_t* cli);
int utils_cli_input_middlewares_add_before(utils_cli_t* cli, utils_cli_middleware_t fn, void* data); // When a key is pressed
int utils_cli_input_middlewares_add_after(utils_cli_t* cli, utils_cli_middleware_char_t fn, void* data);
int utils_cli_input_middlewares_clear(utils_cli_t* cli);
int utils_cli_parser_middlewares_add_before(utils_cli_t* cli, utils_cli_middleware_str_t fn, void* data); // When the parser is called
int utils_cli_parser_middlewares_add_after(utils_cli_t* cli, utils_cli_middleware_parsed_t fn, void* data);
int utils_cli_parser_middlewares_clear(utils_cli_t* cli);
int utils_cli_exec_middlewares_add_before(utils_cli_t* cli, utils_cli_middleware_parsed_t fn, void* data); // When the parsed data is executed
int utils_cli_exec_middlewares_add_after(utils_cli_t* cli, utils_cli_middleware_parsed_t fn, void* data);
int utils_cli_exec_middlewares_clear(utils_cli_t* cli);
int utils_cli_command_middlewares_add_before(utils_cli_t* cli, utils_cli_middleware_str_t fn, void* data); // When a command is executed
int utils_cli_command_middlewares_add_after(utils_cli_t* cli, utils_cli_middleware_str_t fn, void* data);
int utils_cli_command_middlewares_clear(utils_cli_t* cli);
void utils_cli_set_input_delimitor(utils_cli_t* cli, char c);
char utils_cli_get_input_delimitor(const utils_cli_t* cli);
uint8_t utils_cli_get_code(const utils_cli_t* cli);
uint32_t utils_cli_get_flags(const utils_cli_t* cli);
const char* utils_cli_strcode(const utils_cli_t* cli, uint8_t code);
const char* utils_cli_history_get(const utils_cli_t* cli, size_t i);
size_t utils_cli_history_size(const utils_cli_t* cli);
size_t utils_cli_strcode_r(const utils_cli_t* cli, uint8_t code, char* buf, size_t size);
size_t utils_cli_history_get_r(const utils_cli_t* cli, size_t i, char* buf, size_t size);
utils_cli_parsed_t* utils_cli_parsed_create(void); // NULL on error
void utils_cli_parsed_destroy(utils_cli_parsed_t* parsed);
int utils_cli_parsed_clear(utils_cli_parsed_t* parsed);
int utils_cli_parsed_add_group(utils_cli_parsed_t* parsed); // new command
int utils_cli_parsed_add_arg(utils_cli_parsed_t* parsed, const char* arg); // to the last group
size_t utils_cli_parsed_groups(const utils_cli_parsed_t* parsed);
size_t utils_cli_parsed_size(const utils_cli_parsed_t* parsed, size_t group);
const char* utils_cli_parsed_get(const utils_cli_parsed_t* parsed, size_t group, size_t i); // valid until parsed is changed (NULL on error)
```

## `utils/cli/CApi.hpp`

C++ side of the C API of utils::cli::Cli (handle & checks)

```cpp
// global
struct utils_cli: public utils_handle {
    utils::cli::Cli cli;
    std::optional<std::thread> thread; // returned by start with THREAD (joined by utils_cli_join / utils_cli_destroy)
    utils_cli(const bool sig); // Enable/Disable catch of ctrl-c & ctrl-z signal
};
```

## `utils/cli/Cli.hpp`

Cli class used for a customizable command line interface

Namespace: `utils::cli`

```cpp
#define HISTORY_FILE ".utils-cli_history"
#define HISTORY_LIMITS 10000

// namespace utils::cli
using ParsedData = std::vector< std::vector< std::string > >;
void defaultPromptHook(const utils::cli::Cli& cli, std::uint8_t code);
utils::cli::ParsedData defaultParserHook(const std::string& input, const bool trim, const bool logic, const bool parse);
bool defaultGetCHook(char& c);
class Cli: private utils::security::observer::Observer<"Cli"> {
    utils::pool::Middlewares<void, void> cliMiddlewares; // When the cli start & end
    utils::pool::Middlewares<std::uint8_t, std::uint8_t> errorMiddlewares; // When an error is triggered
    utils::pool::Middlewares<void, void> promptMiddlewares; // When the prompt is displayed
    utils::pool::Middlewares<void, char> inputMiddlewares; // When a key is pressed (only after is used)
    utils::pool::Middlewares<const std::string&, const utils::cli::ParsedData&> parserMiddlewares; // When the parser is called
    utils::pool::Middlewares<const utils::cli::ParsedData&, const utils::cli::ParsedData&> execMiddlewares; // When the parsed data is executed
    utils::pool::Middlewares<const std::string&, const std::string&> commandMiddlewares; // When a command is executed
    void join(void) const noexcept; // Yield until the cli stop running
    std::optional<std::thread> start(const std::size_t call = 1, const bool failsafe = false);
    std::optional<std::thread> start(const std::string& input, const std::size_t call = 1, const bool failsafe = false); // Execute an input on start
    std::optional<std::thread> start(const std::vector<std::string>& inputs, const std::size_t call = 1, const bool failsafe = false); // Execute multiple input on start
    void resetCommands(void);
    void clearCommands(void);
    void delCommand(const std::string& command);
    void delCommands(const std::vector<std::string>& commands);
    void resetHooks(void); // Reset all hooks
    void resetMiddlewares(void); // Reset all middlewares
    std::string strcode(std::uint8_t code) const;
    void setInputDelimitor(const char c);
    void interrupt(void);
    void kill(void);
    bool isRunning(void) const;
    bool wasInterrupted(void) const;
    bool wasKilled(void) const;
    bool wasStopped(void) const;
    void resetFlags(void);
    void subFlags(std::uint32_t flags);
    void addFlags(std::uint32_t flags);
    void setFlags(std::uint32_t flags);
    template<bool force = false> void setCommand(const std::string& command, const std::tuple<std::function<void(const utils::cli::Cli&, const std::vector<std::string>&)>, std::int16_t, std::int16_t>& tup);
    template<bool force = false> void setCommand(const std::string& command, const std::function<void(const utils::cli::Cli&, const std::string&)>& fn);
    template<bool force = false> void setCommands(const std::unordered_map<std::string, std::tuple<std::function<void(const utils::cli::Cli&, const std::vector<std::string>&)>, std::int16_t, std::int16_t>>& commands);
    template<bool force = false> void setCommands(const std::unordered_map<std::string, std::function<void(const utils::cli::Cli&, const std::string&)>>& commands);
    void resetPromptHook(void);
    void resetParserHook(void);
    void resetGetCHook(void);
    void setPromptHook(const std::function<void(const utils::cli::Cli&, std::uint8_t)>& hook); // Called to print the prompt
    void setParserHook(const std::function<utils::cli::ParsedData(const std::string&, bool, bool, bool)>& hook); // Called to parse the input
    void setGetCHook(const std::function<bool(char&)>& hook); // Called to get a char of the input
    std::uint8_t getCode(void) const;
    std::uint32_t getFlags(void) const;
    char getInputDelimitor(void) const;
    std::vector<std::string> getHistory(void) const;
    Cli(const bool sig = false); // Enable/Disable catch of ctrl-c & ctrl-z signal
    ~Cli();
};
```

## `utils/cli/Flags.hpp`

Definition of the flags used to customize the cli

Namespace: `utils::cli`, `utils::cli::flags`

```cpp
// namespace utils::cli
enum Flag {DEBUG, NOECHO, CATCH, EMPTY_INPUT, TRIM, PARSED, PROMPT, LOGIC, ARROW, HISTORY, PERSISTENT, HINT, AUTO_COMPLETION, MANUAL, THREAD, DETACHED, NO_TTY}
// namespace utils::cli::flags
constexpr std::uint32_t ALL = DEBUG | CATCH | NOECHO | TRIM | EMPTY_INPUT | PARSED | PROMPT | LOGIC | ARROW | HISTORY | HINT | AUTO_COMPLETION | MANUAL | THREAD | DETACHED;
constexpr std::uint32_t DEFAULT = CATCH | EMPTY_INPUT | TRIM | PROMPT | ARROW;
constexpr std::uint32_t DUMB = 0;
constexpr std::uint32_t TERM1 = CATCH | EMPTY_INPUT | TRIM | PARSED | PROMPT | LOGIC | ARROW | HISTORY;
constexpr std::uint32_t TERM2 = TERM1 | HINT | AUTO_COMPLETION;
constexpr std::uint32_t TERM3 = TERM2 | THREAD;
constexpr std::uint32_t LOG = TERM3 | DETACHED | NO_TTY;
constexpr std::uint32_t DEV = TERM2 | DEBUG;
constexpr std::uint32_t MULTI_THREADING = THREAD | DETACHED;
```
