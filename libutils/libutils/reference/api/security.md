# libutils `security`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

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
[deprecated ~v4.0.0] std::string keyToString(const std::vector<std::uint8_t>& data);
[deprecated ~v4.0.0] std::vector<std::uint8_t> stringToKey(const std::string& s);
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
