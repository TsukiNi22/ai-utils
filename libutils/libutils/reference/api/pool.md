# libutils `pool`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

## `utils/pool/Cluster.hpp`

Cluster class definition

Namespace: `utils::pool`

```cpp
// namespace utils::pool
template<typename T> class Cluster: private utils::security::observer::Observer<"Cluster"> {
    std::size_t size(void) const;
    void apply(std::function<void(T&)> fn);
    template<typename... Args> requires std::constructible_from<T, Args&&...> void spawn(Args&&... args);
    template<typename... Args> void spawn(std::size_t n, Args&&... args); // spawn n
    void kill(void); // kill all
    void kill(std::size_t n); // kill n last
    Cluster& operator=(Cluster&& other) = default;
    Cluster() = default;
    Cluster(Cluster&& other) = default;
    ~Cluster() = default;
};
```

## `utils/pool/middleware/Middlewares.hpp`

Global middlewares include

## `utils/pool/middleware/MiddlewaresType.hpp`

Declaration of the Middleware type for void & non void function

Namespace: `utils::pool`

```cpp
// namespace utils::pool
template<typename T> struct MiddlewareType {
    using type = std::function<void(T)>;
};
template<> struct MiddlewareType<void> {
    using type = std::function<void(void)>;
};
```

## `utils/pool/middleware/Middlewares_t-t.hpp`

Declaration of the Middlewares<T, U>

Namespace: `utils::pool`

```cpp
// namespace utils::pool
template<typename T, typename U> class Middlewares: private utils::security::observer::Observer<"Middlewares"> {
    std::vector<utils::pool::Middleware<T>> before;
    std::vector<utils::pool::Middleware<U>> after;
    void clear(void);
    void addBefore(const utils::pool::Middleware<T>& toAdd);
    void addBefore(const std::vector<utils::pool::Middleware<T>>& toAdds);
    void addAfter(const utils::pool::Middleware<U>& toAdd);
    void addAfter(const std::vector<utils::pool::Middleware<U>>& toAdds);
    void callBefore(T arg) const;
    void callAfter(U arg) const;
    Middlewares& operator=(const Middlewares& other);
    Middlewares& operator=(Middlewares&& other);
    Middlewares() = default;
    Middlewares(const Middlewares& other);
    Middlewares(Middlewares&& other);
    ~Middlewares() = default;
};
```

## `utils/pool/middleware/Middlewares_t-void.hpp`

Declaration of the Middlewares<T, void>

Namespace: `utils::pool`

```cpp
// namespace utils::pool
template<typename T> class Middlewares<T, void>: private utils::security::observer::Observer<"Middlewares"> {
    std::vector<utils::pool::Middleware<T>> before;
    std::vector<utils::pool::Middleware<void>> after;
    void clear(void);
    void addBefore(const utils::pool::Middleware<T>& toAdd);
    void addBefore(const std::vector<utils::pool::Middleware<T>>& toAdds);
    void addAfter(const utils::pool::Middleware<void>& toAdd);
    void addAfter(const std::vector<utils::pool::Middleware<void>>& toAdds);
    void callBefore(T arg) const;
    void callAfter(void) const;
    Middlewares& operator=(const Middlewares& other);
    Middlewares& operator=(Middlewares&& other);
    Middlewares() = default;
    Middlewares(const Middlewares& other);
    Middlewares(Middlewares&& other);
    ~Middlewares() = default;
};
```

## `utils/pool/middleware/Middlewares_void-t.hpp`

Declaration of the Middlewares<void, T>

Namespace: `utils::pool`

```cpp
// namespace utils::pool
template<typename U> class Middlewares<void, U>: private utils::security::observer::Observer<"Middlewares"> {
    std::vector<utils::pool::Middleware<void>> before;
    std::vector<utils::pool::Middleware<U>> after;
    void clear(void);
    void addBefore(const utils::pool::Middleware<void>& toAdd);
    void addBefore(const std::vector<utils::pool::Middleware<void>>& toAdds);
    void addAfter(const utils::pool::Middleware<U>& toAdd);
    void addAfter(const std::vector<utils::pool::Middleware<U>>& toAdds);
    void callBefore(void) const;
    void callAfter(U arg) const;
    Middlewares& operator=(const Middlewares& other);
    Middlewares& operator=(Middlewares&& other);
    Middlewares() = default;
    Middlewares(const Middlewares& other);
    Middlewares(Middlewares&& other);
    ~Middlewares() = default;
};
```

## `utils/pool/middleware/Middlewares_void-void.hpp`

Declaration of the Middlewares<void, void>

Namespace: `utils::pool`

```cpp
// namespace utils::pool
template<> class Middlewares<void, void>: private utils::security::observer::Observer<"Middlewares"> {
    std::vector<utils::pool::Middleware<void>> before;
    std::vector<utils::pool::Middleware<void>> after;
    void clear(void);
    void addBefore(const utils::pool::Middleware<void>& toAdd);
    void addBefore(const std::vector<utils::pool::Middleware<void>>& toAdds);
    void addAfter(const utils::pool::Middleware<void>& toAdd);
    void addAfter(const std::vector<utils::pool::Middleware<void>>& toAdds);
    void callBefore(void) const;
    void callAfter(void) const;
    Middlewares& operator=(const Middlewares& other);
    Middlewares& operator=(Middlewares&& other);
    Middlewares() = default;
    Middlewares(const Middlewares& other);
    Middlewares(Middlewares&& other);
    ~Middlewares() = default;
};
```
