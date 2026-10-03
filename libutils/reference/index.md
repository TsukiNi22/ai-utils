# libutils API index

Generated from libutils `v2.14.0` (commit `7506acd`, 2026-10-01) by `scripts/gen_api.py`, do not edit by hand.

Include path = `"<header>"` (ex: `#include "utils/system/IdHandler.hpp"`). Details of a section: `api/<section>.md`.

## algorithms

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/algorithms/c2dmp-hsm/algorithm/foptimized.hpp` | `utils::algorithms::c2dmp` | `c2dmp_foptimized`, `make_lookup_table`, `normalize` | Algorithm used to determine the distance between 2 word  n = a.size() m = a.size() k = sizeof(UINT) → can be 1, 2, 4 or 8  Time: bast → O(m + n) moy  → O(m + n) wort → O(m + n)  Memory: best → O(1) → const (637) moy  → O(1) → const (271 * k + 366) wort → O(1) → const (2534) |
| `utils/algorithms/c2dmp-hsm/algorithm/fsimplified.hpp` | `utils::algorithms::c2dmp` | `c2dmp_fsimplified`, `make_lookup_table`, `normalize` | Algorithm used to determine the distance between 2 word  n = a.size() m = a.size() k = sizeof(UINT) → can be 1, 2, 4 or 8  Time: bast → O(m + n) moy  → O(m + n) wort → O(m + n)  Memory: best → O(1) → const (637) moy  → O(1) → const (271 * k + 366) wort → O(1) → const (2534) |
| `utils/algorithms/c2dmp-hsm/algorithm/optimized.hpp` | `utils::algorithms::c2dmp` | `c2dmp_optimized`, `make_lookup_table`, `normalize` | Algorithm used to determine the distance between 2 word  n = a.size() m = a.size() k = sizeof(UINT) → can be 1, 2, 4 or 8  Time: bast → O(m + min(n, m)) moy  → O(m + min(n, m)) wort → O(m + min(n, m))  Memory: best → O(1) → const (637) moy  → O(1) → const (271 * k + 366) wort → O(1) → const (2534) |
| `utils/algorithms/c2dmp-hsm/algorithm/simplified.hpp` | `utils::algorithms::c2dmp` | `c2dmp_simplified`, `make_lookup_table`, `normalize` | Algorithm used to determine the distance between 2 word  n = a.size() m = a.size() k = sizeof(UINT) → can be 1, 2, 4 or 8  Time: bast → O(m + min(n, m)) moy  → O(m + min(n, m)) wort → O(m + min(n, m))  Memory: best → O(1) → const (637) moy  → O(1) → const (271 * k + 366) wort → O(1) → const (2534) |
| `utils/algorithms/c2dmp-hsm/c2dmp-hsm.hpp` | `utils::algorithms::c2dmp` | `c2dmp` | Header for include all the different algorithm |
| `utils/algorithms/sos/algorithm/embed_optimized.hpp` | `utils::algorithms::sos::algorithm` | `sos_embed_optimized` | Optimized embed version of the s.o.s algorithm |
| `utils/algorithms/sos/algorithm/extract_optimized.hpp` | `utils::algorithms::sos::algorithm` | `sos_extract_optimized` | Optimized extract version of the s.o.s algorithm |
| `utils/algorithms/sos/sos.hpp` | `utils::algorithms::sos` | `sos_embed`, `sos_extract` | Header for include all the different algorithm |
| `utils/algorithms/sos/sosDefine.hpp` | `utils::algorithms::sos` | `Option` | Different define & macro used for limits and computing |
| `utils/algorithms/sos/sosType.hpp` | `utils::algorithms::sos` | - | Default type used by the s.o.s algorithm |
| `utils/algorithms/sos/tools/convert.hpp` | `utils::algorithms::sos::tools` | `bytes_to`, `to_bytes` | Include of the convertion tools |
| `utils/algorithms/sos/tools/hash.hpp` | `utils::algorithms::sos::tools` | `DirectSeedSequence`, `hash`, `make_generator` | Include of the hash generation tools |
| `utils/algorithms/sos/tools/noise.hpp` | `utils::algorithms::sos::tools` | `noise` | Include of the noise generation tools |
| `utils/algorithms/sos/tools/threshold.hpp` | `utils::algorithms::sos::tools` | `getThresholdIndex`, `removeThreshold` | Include of the threshold tools |

## arguments

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/arguments/ArgParser.hpp` | `utils::arguments` | `ArgParser`, `defaultBoolParsingHook`, `defaultDirectoryParsingHook`, `defaultDoubleParsingHook`, `defaultFileParsingHook`, `defaultHelpHook`, `defaultInt32ParsingHook`, `defaultSizetParsingHook`, `defaultTrueParsingHook` ... | Declaration of the ArgParser class for arguments handling |
| `utils/arguments/ArgParserType.hpp` | `utils::arguments` | `ParsedUsageFull`, `ParsedUsage`, `Usage`, `Option`, `Flag` | Declaration of the ArgParser type for void & non void function |
| `utils/arguments/Setting.hpp` | `utils::arguments` | `Setting`, `demangle` | Declaration of the Setting class used in Settings |
| `utils/arguments/Settings.hpp` | `utils::arguments` | `Settings` | Declaration of the Settings class used for settings handling |
| `utils/arguments/SettingsDefine.hpp` | `utils::arguments` | `CastType` | Enum & Include handling used in settings handling |

## attribute

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/attribute/Attribute.hpp` | `std` | - | Include the right file from the c++ version used or selected |
| `utils/attribute/c++14.hpp` | `-` | - | Different attribute used for optimisation & other thing Version for c++14 and above |
| `utils/attribute/c++17.hpp` | `-` | - | Different attribute used for optimisation & other thing Version for c++17 and above |
| `utils/attribute/c++20.hpp` | `-` | - | Different attribute used for optimisation & other thing Version for c++20 and above |
| `utils/attribute/fallback.hpp` | `-` | - | Different attribute used for optimisation & other thing Fallback if the version used is inferior to c++14 |

## cli

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/cli/Cli.hpp` | `utils::cli` | `Cli`, `defaultGetCHook`, `defaultParserHook`, `defaultPromptHook` | Cli class used for a customizable command line interface |
| `utils/cli/Flags.hpp` | `utils::cli`, `utils::cli::Flags` | `Flag` | Definition of the flags used to customize the cli |

## concepts

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/concepts/GlobalConcepts.hpp` | `utils::concepts` | `Convertible`, `Streamable`, `Swappable`, `convertible_to` | Definition of the different global concepts |
| `utils/concepts/OperationConcepts.hpp` | `utils::concepts` | `AddAssignable`, `AddAssignableWith`, `Addable`, `AddableWith`, `BitwiseAndable`, `BitwiseAndableWith`, `BitwiseOrable`, `BitwiseOrableWith` ... | Definition of the different operation concepts |

## encapsulation

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/encapsulation/Dup.hpp` | `utils::encapsulation` | `Dup` | Basic encapsulation for pipe |
| `utils/encapsulation/Pipe.hpp` | `utils::encapsulation` | `Pipe` | Basic encapsulation for pipe |
| `utils/encapsulation/Poll.hpp` | `utils::encapsulation` | `Poll` | Encapsulation of the epoll |
| `utils/encapsulation/Process.hpp` | `utils::encapsulation` | `Status`, `Process` | Process encapsulation class |
| `utils/encapsulation/SharedMemory.hpp` | `std`, `utils::encapsulation`, `utils::encapsulation::shm` | `ShmMetadata`, `Id`, `Target`, `ShmRequestMetadata`, `Slot`, `ReadFilter`, `LayoutPolicy`, `hash`, `SharedMemory`, `align_ceil` | Encapsulation for shared memory |
| `utils/encapsulation/SharedObject.hpp` | `utils::encapsulation` | `SharedObject` | Definition of the encapsulation for shared object (.so) |

## exception

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/exception/AException.hpp` | `utils::exception` | `AException` | Absract for the cutomized exception handling |
| `utils/exception/ExceptionDefine.hpp` | `-` | - | Definition of the error code&message |
| `utils/exception/IException.hpp` | `utils::exception` | `IException` | Interface for the cutomized exception handling |
| `utils/exception/basic/ErrorException.hpp` | `utils::exception` | `ErrorException` | Exception class used for Error |
| `utils/exception/basic/NoneException.hpp` | `utils::exception` | `NoneException` | Exception class used for None (like in simple exit) |
| `utils/exception/basic/WarningException.hpp` | `utils::exception` | `WarningException` | Exception class used for Warning |
| `utils/exception/custom/CustomException.hpp` | `utils::exception` | `CustomException` | Exception class used for custom ones |
| `utils/exception/custom/FatalException.hpp` | `utils::exception` | `FatalException` | Exception class used for custom ones |

## manip

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/manip/iomanip/ANSI.hpp` | `utils::iomanip` | `MouseButton`, `MouseEvent`, `AdvancedMouseEvent`, `back_color_id`, `back_color_rgb`, `bar`, `bar_reset`, `color`, `color_id`, `color_rgb`, `column` ... | Definition of ANSI escape sequences |
| `utils/manip/iomanip/Char.hpp` | `utils::iomanip` | `Char` | Definition of some special char |
| `utils/manip/iomanip/Color.hpp` | `utils::iomanip` | `Color`, `BackColor` | Definition of color used in ANSI escape sequences |
| `utils/manip/iomanip/Style.hpp` | `utils::iomanip` | `Style`, `ResetStyle` | Define of the different style used in ANSI |
| `utils/manip/smanip/codec/Base64Codec.hpp` | `utils::smanip::codec` | `Base64Codec` | Definition of the base 64 codec |
| `utils/manip/smanip/codec/Codec.hpp` | `-` | - | Include for all the different codec |
| `utils/manip/smanip/codec/ICodec.hpp` | `utils::smanip::codec` | `ICodec` | Declaration of the interface used for different codec (base64, ...) |
| `utils/manip/smanip/fixed_string.hpp` | `utils::smanip` | `fixed_string`, `fixed_string` | Fixed string used in template definition |
| `utils/manip/smanip/format.hpp` | `utils::iomanip`, `utils::smanip` | `format` | Definition of the utils::iomanip::format & explication |
| `utils/manip/smanip/parser/AParser.hpp` | `utils::smanip::parser` | `AParser` | Declaration of the abstract used for different parser (2etp, ...) |
| `utils/manip/smanip/parser/EETPParser.hpp` | `utils::smanip::parser` | `EETPContent`, `EETPParser` | Declaration of the parser used for the 2etp protocol |
| `utils/manip/smanip/parser/IParser.hpp` | `utils::smanip::parser` | `IParser` | Declaration of the interface used for different parser (2etp, ...) |
| `utils/manip/smanip/parser/Parser.hpp` | `-` | - | Include for all the different parser |

## math

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/math/MathType.hpp` | `utils::math` | `CFrame` | Type definition used in math computing |
| `utils/math/geometry/Angle.hpp` | `utils::math::geometry` | `to_look` | Prototype for angle computing |
| `utils/math/geometry/Point.hpp` | `utils::math::geometry` | `rotate_point_2D`, `rotate_point_3D` | Prototype for point computing |
| `utils/math/trigo/Convertion.hpp` | `utils::math::trigo` | `deg_to_rad`, `rad_to_deg` | Prototype for point computing |

## network

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/network/Client.hpp` | `utils::network` | `Client` | Definition of the client class for custom network |
| `utils/network/NetworkDefine.hpp` | `utils::network` | `Status` | Different definition of values for socket definition |
| `utils/network/NetworkType.hpp` | `utils::network` | `Address` |  |
| `utils/network/Server.hpp` | `utils::network` | `Server` | Definition of the server class for custom network |
| `utils/network/socket/ASocket.hpp` | `utils::network::socket` | `ASocket`, `is_ip`, `resolve_address`, `resolve_hostname` | Abstract for socket handling |
| `utils/network/socket/ISocket.hpp` | `utils::network::socket` | `ISocket` | Interface for socket handling |
| `utils/network/socket/Socket.hpp` | `-` | - | Include for all the different sockets |
| `utils/network/socket/TCPSocket.hpp` | `utils::network::socket` | `TCPSocket` | Socket that handle tcp communication |

## pool

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/pool/Cluster.hpp` | `utils::pool` | `Cluster` | Cluster class definition |
| `utils/pool/middleware/Middlewares.hpp` | `-` | - | Global middlewares include |
| `utils/pool/middleware/MiddlewaresType.hpp` | `utils::pool` | `MiddlewareType`, `MiddlewareType` | Declaration of the Middleware type for void & non void function |
| `utils/pool/middleware/Middlewares_t-t.hpp` | `utils::pool` | `Middlewares` | Declaration of the Middlewares<T, U> |
| `utils/pool/middleware/Middlewares_t-void.hpp` | `utils::pool` | `Middlewares` | Declaration of the Middlewares<T, void> |
| `utils/pool/middleware/Middlewares_void-t.hpp` | `utils::pool` | `Middlewares` | Declaration of the Middlewares<void, T> |
| `utils/pool/middleware/Middlewares_void-void.hpp` | `utils::pool` | `Middlewares` | Declaration of the Middlewares<void, void> |

## security

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/security/encryption/AESKey.hpp` | `utils::security::encryption` | `KeyAES`, `AESKey` | Declaration of the key used for the AES |
| `utils/security/encryption/AKey.hpp` | `utils::security::encryption` | `AKey`, `keyToString`, `stringToKey` | Declaration of the abstract used for different key (RSA, AES, ...) |
| `utils/security/encryption/CommonRSAKey.hpp` | `utils::security::encryption` | `CommonRSAKey` | Declaration of the key used for the common RSA |
| `utils/security/encryption/IKey.hpp` | `utils::security::encryption` | `IKey` | Declaration of the interface used for different key (RSA, AES, ...) |
| `utils/security/encryption/Key.hpp` | `-` | - | Include for all the different key |
| `utils/security/encryption/RSAKey.hpp` | `utils::security::encryption` | `KeyPair`, `RSAKey` | Declaration of the key used for the RSA |
| `utils/security/observer/ANotifier.hpp` | `utils::security::observer` | `ANotifier` | Abstract of the different notifiers |
| `utils/security/observer/AObserver.hpp` | `utils::security::observer` | `AObserver` | Abstract version of the different observers |
| `utils/security/observer/INotifier.hpp` | `utils::security::observer` | `INotifier` | Interface of the different notifiers |
| `utils/security/observer/IObserver.hpp` | `utils::security::observer` | `IObserver` | Interface of the different observers |
| `utils/security/observer/Instances.hpp` | `utils::security::observer::instances` | - | Different static instance used by the observer |
| `utils/security/observer/MemoryLeakNotifier.hpp` | `utils::security::observer` | `MemoryLeakNotifier` | Notifier for meamory leak |
| `utils/security/observer/Observer.hpp` | `utils::security::observer` | `Observer` | Observer used for the different warning |
| `utils/security/observer/UnsafeObserver.hpp` | `utils::security::observer` | `UnsafeObserver` | UnsafeObserver used for the different warning |

## system

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/system/IdHandler.hpp` | `utils::system` | `IdHandler` | Handle id allocation |
| `utils/system/LoadBalancer.hpp` | `utils::system` | `LoadBalancer` | LoadBalancer and sub class definition |
| `utils/system/Scheduler.hpp` | `utils::system` | `Scheduler` | Scheduler definition |

## type

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/type/Freezable.hpp` | `utils::type` | `Freezable` |  |
| `utils/type/Worker.hpp` | `utils::type` | `Worker` |  |
| `utils/type/blt/BidirectionalLookupTable.hpp` | `-` | - | Global bidirectional lookup table include |
| `utils/type/blt/BidirectionalLookupTable_t-t.hpp` | `utils::type` | `BidirectionalLookupTable` | Class used for a bidirectional lookup table |
| `utils/type/blt/BidirectionalLookupTable_t.hpp` | `utils::type` | `BidirectionalLookupTable` | Class used for a bidirectional lookup table specialized for one type |
| `utils/type/matrix/Matrix.hpp` | `utils::type` | `Matrix`, `operator*`, `operator<<` | Matrix that contains x * y value of undefined type x -> row & y -> column (_matrix[x][y]) |
| `utils/type/matrix/OMatrix.hpp` | `utils::type` | `OMatrix`, `operator*`, `operator<<` | Matrix that contains x * y value of undefined type x -> row & y -> column (row-major contiguous storage) Optimized version |
| `utils/type/vector/IVector.hpp` | `utils::type` | `IVector` | Interface for the cutomized vector |
| `utils/type/vector/OVector2.hpp` | `utils::type` | `OVector2`, `operator!=`, `operator&`, `operator*`, `operator+`, `operator-`, `operator/`, `operator<`, `operator<<` ... | Vector hat contains 2 value respectivly x & y of undefined type Optimized version |
| `utils/type/vector/OVector3.hpp` | `utils::type` | `OVector3`, `operator!=`, `operator&`, `operator*`, `operator+`, `operator-`, `operator/`, `operator<`, `operator<<` ... | Vector hat contains 3 value respectivly x, y & z of undefined type Optimized version |
| `utils/type/vector/Vector.hpp` | `-` | - | Include for all the different vector |
| `utils/type/vector/Vector2.hpp` | `utils::type` | `Vector2`, `operator!=`, `operator&`, `operator*`, `operator+`, `operator-`, `operator/`, `operator<`, `operator<<` ... | Vector hat contains 2 value respectivly x & y of undefined type |
| `utils/type/vector/Vector3.hpp` | `utils::type` | `Vector3`, `operator!=`, `operator&`, `operator*`, `operator+`, `operator-`, `operator/`, `operator<`, `operator<<` ... | Vector hat contains 3 value respectivly x, y & z of undefined type |

## root

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/utils.hpp` | `-` | - | Main include for every part of the utils lib |
| `utils/version.hpp.in` | `utils` | - | File generated by cmake that store the actual version used format (string): v<major>.<minor>.<fix> on error: [unknown] |

## verbose

| Header | Namespace | Public types / functions | Description |
|---|---|---|---|
| `utils/verbose/Verbose.hpp` | `utils::verbose` | `Verbose` | Marco & Define used for verbose usage |
