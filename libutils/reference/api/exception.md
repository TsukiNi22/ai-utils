# libutils `exception`

Generated from libutils `v2.14.0` (commit `7506acd`, 2026-10-01) by `scripts/gen_api.py`, do not edit by hand.

## `utils/exception/AException.hpp`

Absract for the cutomized exception handling

Namespace: `utils::exception`

```cpp
// namespace utils::exception
class AException: public utils::exception::IException {
    std::string formated(void) const noexcept;
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

## `utils/exception/ExceptionDefine.hpp`

Definition of the error code&message

```cpp
#define OK 0 // Valid
#define KO -1 // Error

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
    CustomException(utils::exception::Type type = utils::exception::Type::None, std::string info = "[None]", std::source_location loc = std::source_location::current());
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
