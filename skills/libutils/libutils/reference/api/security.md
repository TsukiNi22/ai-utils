# libutils `security`

Generated from libutils `v5.0.0` (commit `6aac89f`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/security/CApi.h`

C API of utils::security: AES & RSA keys (encryption) and the observers of the C side (memory leak report) (C & C++ compatible) Every function that can fail returns UTILS_OK / UTILS_KO (or NULL / 0), details in utils_exception

```c
#define C_API_SECURITY_DEFAULT_COMMON_RSA_PATH "~/.ssh/common"

// global
typedef struct utils_aes_key utils_aes_key_t; // utils::security::encryption::AESKey
typedef struct utils_aes_data utils_aes_data_t; // utils::security::encryption::KeyAES (key, iv, tag)
typedef struct utils_rsa_key utils_rsa_key_t; // utils::security::encryption::CommonRSAKey (an RSAKey that can load the common key)
typedef struct utils_observer utils_observer_t; // instance of the C side linked to the notifiers (memory leak report)
utils_aes_key_t* utils_aes_key_create(void); // NULL on error
void utils_aes_key_destroy(utils_aes_key_t* key); // NULL: nothing
int utils_aes_key_random_bytes(const utils_aes_key_t* key, uint16_t size, utils_string_t* out); // out replaced
int utils_aes_key_encrypt(const utils_aes_key_t* key, const char* in, size_t size, utils_aes_data_t* data, utils_string_t* out); // out replaced
int utils_aes_key_decrypt(const utils_aes_key_t* key, const char* in, size_t size, utils_aes_data_t* data, utils_string_t* out); // out replaced
bool utils_aes_key_has_generate_overload(const utils_aes_key_t* key);
bool utils_aes_key_has_set_overload(const utils_aes_key_t* key);
bool utils_aes_key_has_get_overload(const utils_aes_key_t* key);
utils_aes_data_t* utils_aes_data_create(void); // NULL on error
void utils_aes_data_destroy(utils_aes_data_t* data); // NULL: nothing
int utils_aes_data_set_key(utils_aes_data_t* data, const char* bytes, size_t size); // 32 bytes (AES-256)
int utils_aes_data_set_iv(utils_aes_data_t* data, const char* bytes, size_t size);
int utils_aes_data_set_tag(utils_aes_data_t* data, const char* bytes, size_t size);
const char* utils_aes_data_get_key(const utils_aes_data_t* data, size_t* size); // NULL on error
const char* utils_aes_data_get_iv(const utils_aes_data_t* data, size_t* size); // NULL on error
const char* utils_aes_data_get_tag(const utils_aes_data_t* data, size_t* size); // NULL on error
utils_rsa_key_t* utils_rsa_key_create(void); // NULL on error
utils_rsa_key_t* utils_rsa_key_create_common(const char* path); // the common key loaded (path: NULL = C_API_SECURITY_DEFAULT_COMMON_RSA_PATH), NULL on error
void utils_rsa_key_destroy(utils_rsa_key_t* key); // NULL: nothing
int utils_rsa_key_load_common(utils_rsa_key_t* key, const char* path); // path: NULL = C_API_SECURITY_DEFAULT_COMMON_RSA_PATH
int utils_rsa_key_generate(utils_rsa_key_t* key);
int utils_rsa_key_set(utils_rsa_key_t* key, const char* private_key, const char* public_key); // NULL: ""
const char* utils_rsa_key_get_private(const utils_rsa_key_t* key); // NULL on error
const char* utils_rsa_key_get_public(const utils_rsa_key_t* key); // NULL on error
int utils_rsa_key_random_bytes(const utils_rsa_key_t* key, uint16_t size, utils_string_t* out); // out replaced
int utils_rsa_key_encrypt(const utils_rsa_key_t* key, const char* in, size_t size, utils_string_t* out); // with the public key, out replaced
int utils_rsa_key_decrypt(const utils_rsa_key_t* key, const char* in, size_t size, utils_string_t* out); // with the private key, out replaced
bool utils_rsa_key_has_generate_overload(const utils_rsa_key_t* key);
bool utils_rsa_key_has_set_overload(const utils_rsa_key_t* key);
bool utils_rsa_key_has_get_overload(const utils_rsa_key_t* key);
utils_observer_t* utils_observer_create(const char* instance, bool safe_mode); // linked to the notifiers, NULL on error
void utils_observer_destroy(utils_observer_t* observer); // NULL: nothing (unlinked)
int utils_observer_trigger(void); // force every notifier to check its status now (memory leak: report the instances still linked)
```

## `utils/security/CApi.hpp`

Handles of the C API of utils::security (usable in c++ to get the object)

```cpp
// global
struct utils_aes_key: public utils_handle, public utils::security::encryption::AESKey {
};
struct utils_aes_data: public utils_handle {
    utils::security::encryption::KeyAES data;
};
struct utils_rsa_key: public utils_handle, public utils::security::encryption::CommonRSAKey {
};
struct utils_observer: public utils_handle {
    std::uint64_t id = 0;
    std::string instance;
    bool safe = true;
};
```

## `utils/security/encryption/AESKey.hpp`

Declaration of the key used for the AES

Namespace: `utils::security::encryption`

```cpp
#define AES_KEY_SIZE 32 // 256 bits
#define AES_MIN_IV_SIZE 12 // 96 bits (GCM nonce)
#define AES_TAG_SIZE 16 // 128 bits

// namespace utils::security::encryption
struct KeyAES {
    std::string AES;
    std::string iv;
    std::string tag;
};
class AESKey: public utils::security::encryption::AKey<utils::security::encryption::KeyAES> {
    std::string encrypt(const std::string& s, utils::security::encryption::KeyAES& key) const;
    std::string decrypt(const std::string& s, utils::security::encryption::KeyAES& key) const;
    AESKey& operator=(AESKey&& other) = default;
    AESKey() = default;
    AESKey(AESKey&& other) = default;
    virtual ~AESKey() = default;
};
```

## `utils/security/encryption/AKey.hpp`

Declaration of the abstract used for different key (RSA, AES, ...)

Namespace: `utils::security::encryption`

```cpp
// namespace utils::security::encryption
std::string key_to_string(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> string_to_key(const std::string& s);
template<typename T> class AKey: public utils::security::encryption::IKey<T> {
    std::string generateRandomBytes(std::uint16_t size) const;
    void generate(void);
    void set(const T& data);
    const T& get(void) const;
    std::string encrypt(const std::string& s) const;
    std::string decrypt(const std::string& s) const;
    std::string encrypt(const std::string& s, T& data) const;
    std::string decrypt(const std::string& s, T& data) const;
    bool hasGenerateOverload(void) const;
    bool hasSetOverload(void) const;
    bool hasGetOverload(void) const;
    AKey& operator=(const AKey& other) = default;
    AKey& operator=(AKey&& other) = default;
    AKey() = default;
    AKey(const AKey& other) = default;
    AKey(AKey&& other) = default;
    virtual ~AKey() = default;
};
```

## `utils/security/encryption/CommonRSAKey.hpp`

Declaration of the key used for the common RSA

Namespace: `utils::security::encryption`

```cpp
#define DEFAULT_COMMON_RSA_PATH "~/.ssh/common" // common (priv) | common.pub (pub)

// namespace utils::security::encryption
class CommonRSAKey: public utils::security::encryption::RSAKey {
    void loadCommon(std::string path = DEFAULT_COMMON_RSA_PATH);
    CommonRSAKey& operator=(const CommonRSAKey& other) = default;
    CommonRSAKey& operator=(CommonRSAKey&& other) = default;
    CommonRSAKey() = default;
    CommonRSAKey(std::string path);
    CommonRSAKey(const CommonRSAKey& other) = default;
    CommonRSAKey(CommonRSAKey&& other) = default;
    virtual ~CommonRSAKey() = default;
};
```

## `utils/security/encryption/IKey.hpp`

Declaration of the interface used for different key (RSA, AES, ...)

Namespace: `utils::security::encryption`

```cpp
// namespace utils::security::encryption
template<typename T> class IKey: private utils::security::observer::Observer<"IKey"> {
    virtual std::string generateRandomBytes(std::uint16_t size = 32) const = 0;
    virtual void generate(void) = 0;
    virtual void set(const T& data) = 0;
    virtual const T& get(void) const = 0;
    virtual std::string encrypt(const std::string& s) const = 0;
    virtual std::string decrypt(const std::string& s) const = 0;
    virtual std::string encrypt(const std::string& s, T& data) const = 0;
    virtual std::string decrypt(const std::string& s, T& data) const = 0;
    virtual bool hasGenerateOverload(void) const = 0;
    virtual bool hasSetOverload(void) const = 0;
    virtual bool hasGetOverload(void) const = 0;
    IKey& operator=(const IKey& other) = default;
    IKey& operator=(IKey&& other) = default;
    IKey() = default;
    IKey(const IKey& other) = default;
    IKey(IKey&& other) = default;
    virtual ~IKey() = default;
};
```

## `utils/security/encryption/Key.hpp`

Include for all the different key

## `utils/security/encryption/RSAKey.hpp`

Declaration of the key used for the RSA

Namespace: `utils::security::encryption`

```cpp
#define RSA_OAEP_PADDING_SIZE 42 // 2 * hash size + 2

// namespace utils::security::encryption
struct KeyPair {
    std::string priv;
    std::string pub;
};
class RSAKey: public utils::security::encryption::AKey<utils::security::encryption::KeyPair> {
    void generate(void);
    std::string encrypt(const std::string& s) const;
    std::string decrypt(const std::string& s) const;
    void set(const utils::security::encryption::KeyPair& keys);
    const utils::security::encryption::KeyPair& get(void) const;
    bool hasGenerateOverload(void) const;
    bool hasSetOverload(void) const;
    bool hasGetOverload(void) const;
    RSAKey& operator=(const RSAKey& other) = default;
    RSAKey& operator=(RSAKey&& other) = default;
    RSAKey() = default;
    RSAKey(const RSAKey& other) = default;
    RSAKey(RSAKey&& other) = default;
    virtual ~RSAKey() = default;
};
```

## `utils/security/observer/ANotifier.hpp`

Abstract of the different notifiers

Namespace: `utils::security::observer`

```cpp
// namespace utils::security::observer
class ANotifier: public utils::security::observer::INotifier {
    void link(const std::uint64_t id, std::string_view instance, const bool safe_mode);
    void unlink(const std::uint64_t id, const bool safe_mode);
    void clear(const bool safe_mode);
    ANotifier() noexcept;
    ~ANotifier() = default;
};
```

## `utils/security/observer/AObserver.hpp`

Abstract version of the different observers

Namespace: `utils::security::observer`

```cpp
// namespace utils::security::observer
template<utils::smanip::FixedString instance = "[unknown]", bool safe_mode = true> class AObserver: public utils::security::observer::IObserver {
    AObserver& operator=(const AObserver& other);
    AObserver& operator=(AObserver&& other);
    AObserver();
    AObserver(const AObserver& other);
    AObserver(AObserver&& other);
    ~AObserver();
};
```

## `utils/security/observer/INotifier.hpp`

Interface of the different notifiers

Namespace: `utils::security::observer`

```cpp
// namespace utils::security::observer
class INotifier {
    virtual void link(const std::uint64_t id, std::string_view instance, const bool safe_mode) = 0;
    virtual void unlink(const std::uint64_t id, const bool safe_mode) = 0;
    virtual void clear(const bool safe_mode) = 0; // Free the stored if from the handler
    virtual void trigger(void) = 0; // Force notifiers to check there internal status and trigger there 'notification'
    INotifier() = default;
    virtual ~INotifier() = default;
};
```

## `utils/security/observer/IObserver.hpp`

Interface of the different observers

Namespace: `utils::security::observer`

```cpp
// namespace utils::security::observer
class IObserver {
    IObserver& operator=(const IObserver& other) = default;
    IObserver& operator=(IObserver&& other) = default;
    IObserver() = default;
    IObserver(const IObserver& other) = default;
    IObserver(IObserver&& other) = default;
    virtual ~IObserver() = default;
};
```

## `utils/security/observer/Instances.hpp`

Different static instance used by the observer

Namespace: `utils::security::observer::instances`

```cpp
// namespace utils::security::observer::instances
utils::system::IdHandler<std::uint64_t>& id_handler(void);
std::array<std::unique_ptr<utils::security::observer::INotifier>, 1>& notifiers(void);
```

## `utils/security/observer/MemoryLeakNotifier.hpp`

Notifier for meamory leak

Namespace: `utils::security::observer`

```cpp
// namespace utils::security::observer
class MemoryLeakNotifier: public utils::security::observer::ANotifier {
    void trigger(void);
    MemoryLeakNotifier() = default;
    ~MemoryLeakNotifier() noexcept;
};
```

## `utils/security/observer/Observer.hpp`

Observer used for the different warning

Namespace: `utils::security::observer`

```cpp
// namespace utils::security::observer
template<utils::smanip::FixedString instance> class Observer: public utils::security::observer::AObserver<instance, true> {
    Observer& operator=(const Observer& other) = default;
    Observer& operator=(Observer&& other) = default;
    Observer() = default;
    Observer(const Observer& other) = default;
    Observer(Observer&& other) = default;
    ~Observer() = default;
};
```

## `utils/security/observer/UnsafeObserver.hpp`

UnsafeObserver used for the different warning

Namespace: `utils::security::observer`

```cpp
// namespace utils::security::observer
template<utils::smanip::FixedString instance> class UnsafeObserver: public utils::security::observer::AObserver<instance, false> {
    UnsafeObserver& operator=(const UnsafeObserver& other) = default;
    UnsafeObserver& operator=(UnsafeObserver&& other) = default;
    UnsafeObserver() = default;
    UnsafeObserver(const UnsafeObserver& other) = default;
    UnsafeObserver(UnsafeObserver&& other) = default;
    ~UnsafeObserver() = default;
};
```
