# libutils `cli`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

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
