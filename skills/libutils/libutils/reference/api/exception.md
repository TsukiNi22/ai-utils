# libutils `exception`

Generated from libutils `v5.0.0` (commit `6aac89f`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/exception/AException.hpp`

Absract for the cutomized exception handling

Namespace: `utils::exception`

```cpp
// namespace utils::exception
std::string format(const utils::exception::Type type, const char* file, const std::size_t line, const std::string& module, const char* message, const char* function, const char* info) noexcept;
class AException: public utils::exception::IException {
    std::string formated(void) const noexcept;
    std::string module(void) const noexcept;
    utils::exception::Type getType(void) const noexcept;
    utils::exception::InternalCode getCode(void) const noexcept;
    bool isNone(void) const noexcept;
    bool isFatal(void) const noexcept;
    const char* what(void) const noexcept;
    const char* info(void) const noexcept;
    const std::source_location& loc(void) const noexcept;
    AException(std::source_location loc = std::source_location::current(), utils::exception::Type type = utils::exception::Type::None, utils::exception::InternalCode code = utils::exception::InternalCode::Undefined, std::string info = "[None]");
    ~AException() = default;
};
```

## `utils/exception/CApi.h`

C side of the exception status filled by the C API (C & C++ compatible)

```c
#define C_API_MAX_CHAR_EXCEPTION_INFORMATION 512 // limits for the string in the exception status
#define C_API_NO_EXCEPTION 0 // default value, no error (utils::exception::InternalCode::Undefined)
#define C_API_OTHER_EXCEPTION_CODE UINT64_MAX // not a libutils exception (std::exception, ...)
#define utils_exception_set(type, code, what, info) utils_exception_set_at((type), (code), (what), (info), __FILE__, __LINE__, __func__)
#define utils_fail(code, info) utils_exception_set(C_API_EXCEPTION_TYPE_ERROR, (code), "C error", (info)) // error of a C function

// global
typedef uint8_t utils_exception_type_t; // utils::exception::Type (bit flags)
typedef uint8_t utils_exception_origin_t; // where the exception comes from (C_API_EXCEPTION_ORIGIN_*)
enum {C_API_EXCEPTION_TYPE_NONE, C_API_EXCEPTION_TYPE_FATAL, C_API_EXCEPTION_TYPE_ERROR, C_API_EXCEPTION_TYPE_WARNING, C_API_EXCEPTION_TYPE_SYSTEM}
enum {C_API_EXCEPTION_ORIGIN_NONE, C_API_EXCEPTION_ORIGIN_INTERNAL, C_API_EXCEPTION_ORIGIN_EXTERNAL, C_API_EXCEPTION_ORIGIN_STD, C_API_EXCEPTION_ORIGIN_UNKNOWN, C_API_EXCEPTION_ORIGIN_C}
typedef struct utils_exception_status { bool exception; utils_exception_origin_t origin; utils_exception_type_t type; uint64_t code; char what[C_API_MAX_CHAR_EXCEPTION_INFORMATION]; char info[C_API_MAX_CHAR_EXCEPTION_INFORMATION]; uint64_t line; char function[C_API_MAX_CHAR_EXCEPTION_INFORMATION]; char file[C_API_MAX_CHAR_EXCEPTION_INFORMATION]; char module[C_API_MAX_CHAR_EXCEPTION_INFORMATION]; };
extern utils_exception_status_t utils_exception;
void utils_exception_clear(void); // reset the whole status of the thread
int utils_exception_set_at(utils_exception_type_t type, uint64_t code, const char* what, const char* info, const char* file, uint64_t line, const char* function); // origin C, returns UTILS_KO (see utils_exception_set / utils_fail)
const char* utils_exception_format(void); // valid until the next call in the thread (NULL on error)
size_t utils_exception_format_r(char* buf, size_t size); // copied in buf (truncated, always '\0' terminated), returns the full length like snprintf (0 on error)
```

## `utils/exception/CApi.hpp`

Handler used for the C API on c++ exception

Namespace: `utils::exception::c_api`

```cpp
#define C_API_HANDLE_MAGIC 0x68616E646C65ull

// global
struct utils_handle {
    std::uint64_t magic = C_API_HANDLE_MAGIC;
    utils_handle& operator=(const utils_handle& other);
    utils_handle& operator=(utils_handle&& other);
    utils_handle() = default;
    utils_handle(const utils_handle& other);
    utils_handle(utils_handle&& other);
    virtual ~utils_handle() noexcept;
};
// namespace utils::exception::c_api
void clear_status(void) noexcept;
void set_status(const utils::exception::IException& e) noexcept; // libutils exception (origin internal / external)
void set_status(const char* what, const utils_exception_origin_t origin) noexcept; // other exception (C_API_OTHER_EXCEPTION_CODE)
template<typename T> T* not_null(T* ptr, const char* name, const std::source_location loc = std::source_location::current());
template<typename T> T& not_null_ref(T* ptr, const char* name, const std::source_location loc = std::source_location::current());
template<typename T> T& handle_base(void* handle, const char* name, const std::source_location loc = std::source_location::current());
template<typename T> const T& handle_base(const void* handle, const char* name, const std::source_location loc = std::source_location::current());
void throw_callback_error(void);
void check_callback(const int ret);
void reset_status(void) noexcept;
template<typename F> int guard(F&& f) noexcept;
template<typename F> std::invoke_result_t<F> guard_fallback(const std::type_identity_t<std::invoke_result_t<F>> fallback, F&& f) noexcept;
```

## `utils/exception/Common.hpp`

Defines common to the c++ exceptions and the C API (C & C++ compatible)

```cpp
#define UTILS_OK 0 // Valid
#define UTILS_KO -1 // Error
#define _UTILS_OK_DEFINED
#define OK UTILS_OK
#define _UTILS_KO_DEFINED
#define KO UTILS_KO

```

## `utils/exception/ExceptionDefine.hpp`

Definition of the error code&message

Namespace: `utils::exception`

```cpp
// namespace utils::exception
enum Type: std::uint8_t {None, Fatal, Error, Warning}
constexpr utils::exception::Type operator|(utils::exception::Type lhs, utils::exception::Type rhs);
constexpr utils::exception::Type operator&(utils::exception::Type lhs, utils::exception::Type rhs);
constexpr utils::exception::Type operator^(utils::exception::Type lhs, utils::exception::Type rhs);
constexpr utils::exception::Type operator~(utils::exception::Type f);
utils::exception::Type& operator|=(utils::exception::Type& lhs, utils::exception::Type rhs);
utils::exception::Type& operator&=(utils::exception::Type& lhs, utils::exception::Type rhs);
utils::exception::Type& operator^=(utils::exception::Type& lhs, utils::exception::Type rhs);
```

## `utils/exception/IException.hpp`

Interface for the cutomized exception handling

Namespace: `utils::exception`

```cpp
// namespace utils::exception
class IException: public std::exception {
    virtual utils::exception::Type getType(void) const noexcept = 0;
    virtual utils::exception::InternalCode getCode(void) const noexcept = 0;
    virtual bool isNone(void) const noexcept = 0;
    virtual bool isFatal(void) const noexcept = 0;
    virtual const char* what(void) const noexcept = 0;
    virtual const char* info(void) const noexcept = 0;
    virtual const std::source_location& loc(void) const noexcept = 0;
    virtual std::string module(void) const noexcept = 0; // Module (shared library) that thrown it, empty -> the binary itself
    virtual std::string formated(void) const noexcept = 0;
    IException() = default;
    virtual ~IException() = default;
};
```

## `utils/exception/basic/ErrorException.hpp`

Exception class used for Error

Namespace: `utils::exception`

```cpp
// namespace utils::exception
class ErrorException: public utils::exception::AException {
    explicit ErrorException(utils::exception::ExternalCode code, std::source_location loc = std::source_location::current());
    ErrorException(utils::exception::ExternalCode code, std::string info, std::source_location loc = std::source_location::current());
    explicit ErrorException(utils::exception::InternalCode code = utils::exception::InternalCode::Undefined, std::source_location loc = std::source_location::current());
    ErrorException(utils::exception::InternalCode code, std::string info, std::source_location loc = std::source_location::current());
    ~ErrorException() = default;
};
```

## `utils/exception/basic/NoneException.hpp`

Exception class used for None (like in simple exit)

Namespace: `utils::exception`

```cpp
// namespace utils::exception
class NoneException: public utils::exception::AException {
    explicit NoneException(utils::exception::ExternalCode code, std::source_location loc = std::source_location::current());
    NoneException(utils::exception::ExternalCode code, std::string info, std::source_location loc = std::source_location::current());
    explicit NoneException(utils::exception::InternalCode code = utils::exception::InternalCode::Undefined, std::source_location loc = std::source_location::current());
    NoneException(utils::exception::InternalCode code, std::string info, std::source_location loc = std::source_location::current());
    ~NoneException() = default;
};
```

## `utils/exception/basic/WarningException.hpp`

Exception class used for Warning

Namespace: `utils::exception`

```cpp
// namespace utils::exception
class WarningException: public utils::exception::AException {
    explicit WarningException(utils::exception::ExternalCode code, std::source_location loc = std::source_location::current());
    WarningException(utils::exception::ExternalCode code, std::string info, std::source_location loc = std::source_location::current());
    explicit WarningException(utils::exception::InternalCode code = utils::exception::InternalCode::Undefined, std::source_location loc = std::source_location::current());
    WarningException(utils::exception::InternalCode code, std::string info, std::source_location loc = std::source_location::current());
    ~WarningException() = default;
};
```

## `utils/exception/custom/CustomException.hpp`

Exception class used for custom ones

Namespace: `utils::exception`

```cpp
// namespace utils::exception
class CustomException: public utils::exception::AException {
    explicit CustomException(utils::exception::Type type, utils::exception::ExternalCode code, std::string info = "[None]", std::source_location loc = std::source_location::current());
    explicit CustomException(utils::exception::Type type = utils::exception::Type::None, utils::exception::InternalCode code = utils::exception::InternalCode::Undefined, std::string info = "[None]", std::source_location loc = std::source_location::current());
    CustomException(utils::exception::Type type, std::string info, std::source_location loc = std::source_location::current());
    ~CustomException() = default;
};
```

## `utils/exception/custom/FatalException.hpp`

Exception class used for custom ones

Namespace: `utils::exception`

```cpp
#define EXCEPTION_ABORTED_HEADER "[ABORTED] FatalException call"
#define EXCEPTION_ABORTED_MESSAGE "Program terminated without proper RAII cleanup"

// namespace utils::exception
class FatalException: public utils::exception::AException {
    void display(void) const noexcept;
    void display(const utils::exception::IException& e) const noexcept;
    explicit FatalException(utils::exception::ExternalCode code, std::source_location loc = std::source_location::current()) noexcept;
    explicit FatalException(utils::exception::ExternalCode code, std::string info, std::source_location loc = std::source_location::current()) noexcept;
    FatalException(utils::exception::Type type, utils::exception::ExternalCode code, std::string info = "[None]", std::source_location loc = std::source_location::current()) noexcept;
    explicit FatalException(utils::exception::InternalCode code = utils::exception::InternalCode::Undefined, std::source_location loc = std::source_location::current()) noexcept;
    explicit FatalException(utils::exception::InternalCode code, std::string info, std::source_location loc = std::source_location::current()) noexcept;
    FatalException(utils::exception::Type type, utils::exception::InternalCode code = utils::exception::InternalCode::Undefined, std::string info = "[None]", std::source_location loc = std::source_location::current()) noexcept;
    FatalException(const utils::exception::IException& e) noexcept;
    ~FatalException() = default;
};
```
