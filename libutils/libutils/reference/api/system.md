# libutils `system`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

## `utils/system/IdHandler.hpp`

Handle id allocation

Namespace: `utils::system`

```cpp
// namespace utils::system
template<typename T> class IdHandler {
    void use(T id, const bool safe_mode = true);
    T id(void) const;
    T actual(const bool safe_mode = true) const;
    T preview(const bool safe_mode = true) const;
    T allocate(T& id, const bool safe_mode = true);
    T allocate(const bool safe_mode = true);
    void free(T& id, const bool safe_mode = true);
    void free(const T& id, const bool safe_mode = true);
    void free(void); // free every id
    void clear(const bool safe_mode = true); // free every id
    IdHandler() = default;
    ~IdHandler() = default;
};
```

## `utils/system/LoadBalancer.hpp`

LoadBalancer and sub class definition

Namespace: `utils::system`

```cpp
// namespace utils::system
template<typename T> class LoadBalancer: private utils::security::observer::Observer<"LoadBalancer"> {
    std::future<T&> getWorker(void); // (async)
    template<bool mode_forced = false> void kill(void); // kill all the not working workers
    template<bool mode_forced = false> void kill(std::size_t n); // kill n workers (the not working ones first)
    void spawn(std::size_t n = 1); // spawn n new workers
    void setLimit(std::size_t limit);
    void setLifespan(std::chrono::milliseconds lifespan);
    std::size_t getLimit(void) const;
    std::chrono::milliseconds getLifespan(void) const;
    std::size_t size(void) const;
    LoadBalancer(std::size_t limit = 1, std::chrono::milliseconds lifespan = std::chrono::milliseconds{0});
    ~LoadBalancer() = default;
};
```

## `utils/system/Scheduler.hpp`

Scheduler definition

Namespace: `utils::system`

```cpp
// namespace utils::system
class Scheduler: private utils::security::observer::Observer<"Scheduler"> {
    void cancel(std::size_t id); // cancel given task
    void cancel(void);
    template<typename Fn> std::size_t schedule(std::chrono::milliseconds delay, const Fn& fn); // schedule a new task
    Scheduler() = default;
    ~Scheduler() = default;
};
```
