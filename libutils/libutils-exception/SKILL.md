---
name: libutils-exception
description: libutils exception handling (utils::exception) - IException/AException, Error/Warning/None/Fatal/CustomException, InternalCode vs ExternalCode, Type, restrictions, formated() output, throw/catch/print patterns - and how to add new exception codes to a project (cmake/config/exceptions/*.json, generate_exception_header.py + const.py, CMake custom command, include/exception/generated_external_exception_header.hpp). Use whenever throwing, catching or printing a libutils exception, choosing an exception code, or adding/editing exception codes, the exception JSON config, the generator script or the generated_*_exception_header of a project.
---

# libutils exceptions

Based on the libutils sources (`include/utils/exception/`, `src/utils/exception/AException.cpp`,
`cmake/scripts/`), the wiki page *Exception (usage)* and the real setup of `context-forge`.
Existing codes: `../libutils/reference/exception-codes.md` (the `libutils` skill).

## 1. Classes
All in `utils::exception`, included by `<utils/utils.hpp>` (or `"utils/exception/basic/ErrorException.hpp"`...).

| Class | Type set | Use |
|---|---|---|
| `IException` (interface, inherits `std::exception`) | - | **catch type** |
| `AException` (abstract) | - | base of every exception, restriction check, `formated()` |
| `ErrorException` | `Error` | recoverable error (most used) |
| `WarningException` | `Warning` | printed, not thrown in general |
| `NoneException` | `None` | not an error: exit / stop the flow (`InternalCode::Exit`) |
| `FatalException` | `Error \| Fatal` (or `type \| Fatal`) | prints and **`std::abort()` in its constructor**: it is never caught, no RAII cleanup |
| `CustomException` | given `type` | any combination of `Type` |

Constructors (each one also exists with `ExternalCode` when the project generated its header;
`loc` defaults to `std::source_location::current()`, never pass it by hand):
```cpp
ErrorException(InternalCode code = InternalCode::Undefined, loc);              // same for Warning/None
ErrorException(InternalCode code, std::string info, loc);
FatalException(InternalCode code = Undefined, loc);
FatalException(InternalCode code, std::string info, loc);
FatalException(Type type, InternalCode code = Undefined, std::string info = "[None]", loc);
FatalException(const IException& e);                                           // abort with an existing exception
CustomException(Type type = None, InternalCode code = Undefined, std::string info = "[None]", loc);
CustomException(Type type, std::string info, loc);                            // code Undefined
```
Methods (`IException`): `formated()`, `what()` (code message), `info()`, `getType()`, `getCode()`
(always an `InternalCode`, an external code is cast), `isNone()`, `isFatal()`, `loc()`.

`Type` (bit flags, `|`, `&`, `^`, `~` defined): `None = 0b0001`, `Fatal = 0b0010`, `Error = 0b0100`, `Warning = 0b1000`.
`OK` (`0`) / `KO` (`-1`) are defined by `ExceptionDefine.hpp` (if not already defined).

`formated()` output (colored, file path as a terminal hyperlink, module name when thrown from another binary/.so):
```
[Error] ...src/forge/Forge.cpp:97 (module) -> <message of the code>
-------------------------------------------
<function signature> = <info>
-------------------------------------------
```

## 2. Usage patterns
```cpp
// Throw: the code gives the message, the info gives the context
throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "got: " + value);
throw utils::exception::ErrorException(utils::exception::ExternalCode::Rules, "Parse error at " + file);

// Warning: print it, don't throw it
std::cerr << utils::exception::WarningException(utils::exception::InternalCode::UnknownId, id).formated() << std::endl;
onDebugVerboseC(std::cerr, utils::exception::ErrorException(utils::exception::ExternalCode::InvalidDirectory, "...").formated());

// Failsafe parameter: warn instead of throw
if (failsafe) {std::cerr << utils::exception::WarningException(code, s).formated() << std::endl; return false;}
else throw utils::exception::ErrorException(code, s);

// Catch: always the interface, print formated()
try {this->loadPlugin(path);}
catch (const utils::exception::IException& e) {onDebugVerboseC(std::cerr, e.formated());}

// main(): None + Exit = clean exit
} catch (const utils::exception::IException& e) {
    if (e.isNone() && e.getCode() == utils::exception::InternalCode::Exit) return OK;
    std::cerr << e.formated() << std::endl;
    return KO;
}

// Compare with an external code
e.getCode() == static_cast<utils::exception::InternalCode>(utils::exception::ExternalCode::Rules)

// Wrap a third party / std error with context
catch (const std::exception& e) {throw utils::exception::ErrorException(utils::exception::InternalCode::ArgParserHook, e.what());}
```
- `info` given at the throw replaces the default `info` of the JSON; no info -> JSON `info` or `"[None]"`.
- `FatalException`: only for unrecoverable states (it aborts at construction, `throw` is never reached).
  Define `FATALEXCEPTION_USAGE_WARNING` (or `_Warning`) to get a compile warning on its use,
  `NO_FATALEXCEPTION_USAGE_WARNING` / `_NoWarning` to silence it.
- **Restrictions**: each code allows some types. At construction `(type & restriction) != type` ->
  `FatalException(InternalCode::ExceptionCodeRestriction)` = **abort**. A `FatalException` has the type
  `Error | Fatal`: its code must allow **both** `Fatal` and `Error`. Check the restrictions of a code
  (`exception-codes.md` or the JSON) before using it with another exception class.

## 3. Add exception codes to a project (external codes)
Internal codes = libutils ones (`InternalCode`), external = the project ones (`ExternalCode`).

Checklist (files to copy are in `templates/`, identical to `cpp_project_template` / `context-forge`):
1. **Scripts**: copy `templates/cmake/scripts/{const.py,generate_exception_header.py,requirements.txt}` to
   `cmake/scripts/`. `const.py` holds the paths (`CONFIG_EXCEPTION = "cmake/config/exceptions/"`,
   `GENERATED_EXCEPTION_HEADER = "include/exception/generated_external_exception_header.hpp"`) and the
   `External*` names: edit it only to change those paths. The script must run from the project root.
2. **JSON**: one or more `*.json` anywhere under `cmake/config/exceptions/` (read recursively), example
   `templates/cmake/config/exceptions/global.json`:
   ```json
   {
       "errors": [
           {
               "code": "InvalidConfig",
               "message": "Error during the configuration parsing",
               "info": "Default info (optional, \"[None]\" when missing)",
               "restrictions": ["Fatal", "Error", "Warning"]
           }
       ]
   }
   ```
   - `code`: PascalCase, unique in the whole project (a duplicate -> the script fails, `exit 1`).
   - `message`: returned by `what()`, short, describes the error family.
   - `info` (optional): default context returned by `info()`.
   - `restrictions` (optional, missing = all types allowed): any of `"None"`, `"Warning"`, `"Error"`, `"Fatal"`.
     Usual sets: `["Fatal", "Error"]`, `["Fatal", "Error", "Warning"]`, `["None"]`.
3. **CMake** (`templates/exception.cmake`, see the `cmake-style` skill for the placement):
   `find_package(Python3 REQUIRED)`, the `file(GLOB_RECURSE ... CONFIGURE_DEPENDS ...json)` +
   `add_custom_command(OUTPUT <header> ...)` + `add_custom_target(generated_external_exception_header ...)`
   in *Special Targets*, then for **every target** using libutils (main target, plugins, `unit_tests`):
   ```cmake
   target_include_directories(${TARGET} PRIVATE include include/exception)
   add_dependencies(${TARGET} generated_external_exception_header)
   ```
   `include/exception` must be an include directory: libutils includes the header with
   `#if __has_include(<generated_external_exception_header.hpp>)`, and its guard
   `GENERATED_EXTERNAL_EXCEPTION_HEADER_H` enables every `ExternalCode` constructor.
4. **Git**: add `include/exception/generated_external_exception_header.hpp` to `.gitignore` (regenerated at
   each build when a JSON or the script changes).
5. **Use**: `utils::exception::ExternalCode::InvalidConfig` with any exception class. Build once
   (`cmake --build`) before the IDE sees the new codes.

Notes:
- The generated header defines `ExternalCode` (values = 64-bit hash of `<code>ExternalCode`),
  `ExternalMessages`, `ExternalInfo`, `ExternalRestriction`; `AException` merges them with the internal maps.
- A project code with the same name as an internal one is a **different** code (different hash, its own
  message/info/restrictions): don't duplicate a libutils code, use `InternalCode::X` directly.
- The script runs `pip install -r requirements.txt` (empty) when not root; Python 3 only needs the stdlib.
- To get the codes as internal ones instead, rebuild libutils itself with the extra JSON (wiki tip).

## 4. Add a code inside libutils (internal)
1. Add the entry in the right file of `cmake/config/exceptions/` (`global/*.json` for generic codes,
   `utils/<section>.json` for a section), same format, unique name across all the files.
2. The libutils `const.py` targets `include/utils/exception/generated_internal_exception_header.hpp`
   (`Internal*` names, also reads the `Type` enum of `ExceptionDefine.hpp`), already wired in its CMake
   (`generated_internal_exception_header` target).
3. Rebuild, use `utils::exception::InternalCode::X`, document it in the CHANGELOG (`### Added`) and regenerate
   the `libutils` skill reference (`libutils/scripts/update.sh`) after the commit.
