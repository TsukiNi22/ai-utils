# libutils `pool`

Generated from libutils `v5.0.0` (commit `6aac89f`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/pool/CApi.h`

C API of utils::pool: clusters & middlewares (C & C++ compatible) The templates are given for the fixed types of type/CApiTypes.h & as bytes (generic) Every function that can fail returns UTILS_OK / UTILS_KO (or NULL / 0), details in utils_exception Every callback returns UTILS_OK or UTILS_KO (info: utils_fail / utils_exception_set)

```c
#define C_API_CLUSTER_DECL(name, ctype) // declares, for each type:
    typedef struct utils_cluster_<name> utils_cluster_<name>_t;
    typedef int (*utils_cluster_<name>_fn_t)(ctype* element, void* data);
    utils_cluster_<name>_t* utils_cluster_<name>_create(void); // NULL on error
    void utils_cluster_<name>_destroy(utils_cluster_<name>_t* cluster); // NULL: nothing
    int utils_cluster_<name>_spawn(utils_cluster_<name>_t* cluster, size_t n, ctype value); // n new elements set to value
    int utils_cluster_<name>_kill(utils_cluster_<name>_t* cluster); // every element
    int utils_cluster_<name>_kill_n(utils_cluster_<name>_t* cluster, size_t n); // the n last ones
    size_t utils_cluster_<name>_size(const utils_cluster_<name>_t* cluster);
    int utils_cluster_<name>_apply(utils_cluster_<name>_t* cluster, utils_cluster_<name>_fn_t fn, void* data); // fn on each element (stops at the first error)
#define C_API_MIDDLEWARES_DECL(name, ctype) // declares, for each type:
    typedef struct utils_middlewares_<name> utils_middlewares_<name>_t;
    typedef int (*utils_middleware_<name>_t)(ctype arg, void* data);
    utils_middlewares_<name>_t* utils_middlewares_<name>_create(void); // NULL on error
    void utils_middlewares_<name>_destroy(utils_middlewares_<name>_t* middlewares); // NULL: nothing
    int utils_middlewares_<name>_clear(utils_middlewares_<name>_t* middlewares);
    int utils_middlewares_<name>_add_before(utils_middlewares_<name>_t* middlewares, utils_middleware_<name>_t fn, void* data);
    int utils_middlewares_<name>_add_after(utils_middlewares_<name>_t* middlewares, utils_middleware_<name>_t fn, void* data);
    int utils_middlewares_<name>_call_before(const utils_middlewares_<name>_t* middlewares, ctype arg); // error: MiddlewareCall with the info of the callback
    int utils_middlewares_<name>_call_after(const utils_middlewares_<name>_t* middlewares, ctype arg);
    size_t utils_middlewares_<name>_before_count(const utils_middlewares_<name>_t* middlewares);
    size_t utils_middlewares_<name>_after_count(const utils_middlewares_<name>_t* middlewares);

// global
typedef struct utils_cluster utils_cluster_t; // utils::pool::Cluster of bytes (an element = data + size)
typedef struct utils_middlewares utils_middlewares_t; // utils::pool::Middlewares<void, void>
typedef struct utils_middlewares_bytes utils_middlewares_bytes_t; // utils::pool::Middlewares of bytes (an argument = data + size)
typedef int (*utils_cluster_fn_t)(void* element, size_t size, void* data); // the bytes of the element can be changed (not its size)
typedef int (*utils_middleware_t)(void* data);
typedef int (*utils_middleware_bytes_t)(const void* arg, size_t size, void* data);
utils_cluster_t* utils_cluster_create(void); // NULL on error
void utils_cluster_destroy(utils_cluster_t* cluster); // NULL: nothing
int utils_cluster_spawn(utils_cluster_t* cluster, size_t n, const void* value, size_t size); // n new elements: a copy of value (size bytes)
int utils_cluster_kill(utils_cluster_t* cluster); // every element
int utils_cluster_kill_n(utils_cluster_t* cluster, size_t n); // the n last ones
size_t utils_cluster_size(const utils_cluster_t* cluster);
int utils_cluster_apply(utils_cluster_t* cluster, utils_cluster_fn_t fn, void* data); // fn on each element (stops at the first error)
C_API_NUMERIC_TYPES(C_API_CLUSTER_DECL)
C_API_CLUSTER_DECL(char, char)
utils_middlewares_t* utils_middlewares_create(void); // NULL on error
void utils_middlewares_destroy(utils_middlewares_t* middlewares); // NULL: nothing
int utils_middlewares_clear(utils_middlewares_t* middlewares);
int utils_middlewares_add_before(utils_middlewares_t* middlewares, utils_middleware_t fn, void* data);
int utils_middlewares_add_after(utils_middlewares_t* middlewares, utils_middleware_t fn, void* data);
int utils_middlewares_call_before(const utils_middlewares_t* middlewares); // error: MiddlewareCall with the info of the callback
int utils_middlewares_call_after(const utils_middlewares_t* middlewares);
size_t utils_middlewares_before_count(const utils_middlewares_t* middlewares);
size_t utils_middlewares_after_count(const utils_middlewares_t* middlewares);
utils_middlewares_bytes_t* utils_middlewares_bytes_create(void); // NULL on error
void utils_middlewares_bytes_destroy(utils_middlewares_bytes_t* middlewares); // NULL: nothing
int utils_middlewares_bytes_clear(utils_middlewares_bytes_t* middlewares);
int utils_middlewares_bytes_add_before(utils_middlewares_bytes_t* middlewares, utils_middleware_bytes_t fn, void* data);
int utils_middlewares_bytes_add_after(utils_middlewares_bytes_t* middlewares, utils_middleware_bytes_t fn, void* data);
int utils_middlewares_bytes_call_before(const utils_middlewares_bytes_t* middlewares, const void* arg, size_t size);
int utils_middlewares_bytes_call_after(const utils_middlewares_bytes_t* middlewares, const void* arg, size_t size);
size_t utils_middlewares_bytes_before_count(const utils_middlewares_bytes_t* middlewares);
size_t utils_middlewares_bytes_after_count(const utils_middlewares_bytes_t* middlewares);
C_API_TYPES(C_API_MIDDLEWARES_DECL)
```

## `utils/pool/CApi.hpp`

Handles of the C API of utils::pool (usable in c++ to get the object)

```cpp
#define C_API_CLUSTER_HANDLE(name, ctype) // declares, for each type:
    struct utils_cluster_<name>: public utils_handle, public utils::pool::Cluster<ctype> {};
#define C_API_MIDDLEWARES_HANDLE(name, ctype) // declares, for each type:
    struct utils_middlewares_<name>: public utils_handle, public utils::pool::Middlewares<utils::type::c_api::cpp_t<ctype>, utils::type::c_api::cpp_t<ctype>> {};
#define C_API_CLUSTER_DEF(name, ctype) // declares, for each type:
    utils_cluster_<name>_t* utils_cluster_<name>_create(void) {return utils::exception::c_api::guard_fallback(nullptr, [](void) -> utils_cluster_<name>_t* {return new utils_cluster_<name>_t();});};
    void utils_cluster_<name>_destroy(utils_cluster_<name>_t* c) {(void)utils::exception::c_api::guard([&](void) {delete c;});};
    int utils_cluster_<name>_spawn(utils_cluster_<name>_t* c, size_t n, ctype value) {return utils::exception::c_api::guard([&](void) {cluster(c).spawn(n, value);});};
    int utils_cluster_<name>_kill(utils_cluster_<name>_t* c) {return utils::exception::c_api::guard([&](void) {cluster(c).kill();});};
    int utils_cluster_<name>_kill_n(utils_cluster_<name>_t* c, size_t n) {return utils::exception::c_api::guard([&](void) {cluster(c).kill(n);});};
    size_t utils_cluster_<name>_size(const utils_cluster_<name>_t* c) {return utils::exception::c_api::guard_fallback(0, [&](void) -> std::size_t {return cluster(c).size();});};
    int utils_cluster_<name>_apply(utils_cluster_<name>_t* c, utils_cluster_<name>_fn_t fn, void* data)
    {
    return utils::exception::c_api::guard([&](void) {
    utils::pool::Cluster<ctype>& self = cluster(c);
    (void)utils::exception::c_api::not_null(fn, "callback");
    self.apply([fn, data](ctype& element) {call([&](void) {return fn(&element, data);});});
    });
    }
#define C_API_MIDDLEWARES_DEF(name, ctype) // declares, for each type:
    utils_middlewares_<name>_t* utils_middlewares_<name>_create(void) {return utils::exception::c_api::guard_fallback(nullptr, [](void) -> utils_middlewares_<name>_t* {return new utils_middlewares_<name>_t();});};
    void utils_middlewares_<name>_destroy(utils_middlewares_<name>_t* m) {(void)utils::exception::c_api::guard([&](void) {delete m;});};
    int utils_middlewares_<name>_clear(utils_middlewares_<name>_t* m) {return utils::exception::c_api::guard([&](void) {middlewares(m).clear();});};
    int utils_middlewares_<name>_add_before(utils_middlewares_<name>_t* m, utils_middleware_<name>_t fn, void* data)
    {
    return utils::exception::c_api::guard([&](void) {
    (void)utils::exception::c_api::not_null(fn, "middleware");
    middlewares(m).addBefore([fn, data](utils::type::c_api::cpp_t<ctype> arg) {call([&](void) {return fn(utils::type::c_api::Storage<ctype>::c(arg), data);});});
    });
    }
    int utils_middlewares_<name>_add_after(utils_middlewares_<name>_t* m, utils_middleware_<name>_t fn, void* data)
    {
    return utils::exception::c_api::guard([&](void) {
    (void)utils::exception::c_api::not_null(fn, "middleware");
    middlewares(m).addAfter([fn, data](utils::type::c_api::cpp_t<ctype> arg) {call([&](void) {return fn(utils::type::c_api::Storage<ctype>::c(arg), data);});});
    });
    }
    int utils_middlewares_<name>_call_before(const utils_middlewares_<name>_t* m, ctype arg)
    {return utils::exception::c_api::guard([&](void) {middlewares(m).callBefore(utils::type::c_api::Storage<ctype>::cpp(arg));});}
    int utils_middlewares_<name>_call_after(const utils_middlewares_<name>_t* m, ctype arg)
    {return utils::exception::c_api::guard([&](void) {middlewares(m).callAfter(utils::type::c_api::Storage<ctype>::cpp(arg));});}
    size_t utils_middlewares_<name>_before_count(const utils_middlewares_<name>_t* m)
    {return utils::exception::c_api::guard_fallback(0, [&](void) -> std::size_t {const utils_middlewares_<name>_t& self = middlewares(m); std::shared_lock lock(self._lock); return self.before.size();});}
    size_t utils_middlewares_<name>_after_count(const utils_middlewares_<name>_t* m)
    {return utils::exception::c_api::guard_fallback(0, [&](void) -> std::size_t {const utils_middlewares_<name>_t& self = middlewares(m); std::shared_lock lock(self._lock); return self.after.size();});}

// global
struct utils_cluster: public utils_handle, public utils::pool::Cluster<std::string> {
};
struct utils_middlewares: public utils_handle, public utils::pool::Middlewares<void, void> {
};
struct utils_middlewares_bytes: public utils_handle, public utils::pool::Middlewares<std::string, std::string> {
};
C_API_NUMERIC_TYPES(C_API_CLUSTER_HANDLE)
C_API_CLUSTER_HANDLE(char, char)
C_API_TYPES(C_API_MIDDLEWARES_HANDLE)
```

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
