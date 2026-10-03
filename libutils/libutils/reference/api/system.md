# libutils `system`

Generated from libutils `v2.14.0` (commit `7506acd`, 2026-10-01) by `scripts/gen_api.py`, do not edit by hand.

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
    void free(const bool safe_mode = true);
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
    std::future<T&> getWorker(void); // return an worker that can do the work (async)
    void kill(void); // kill all workers
    void kill(std::size_t n = 1); // kill n workers
    void spawn(std::size_t n = 1); // spawn n new workers
    void setLimit(std::size_t limit);
    void setLifespan(std::chrono::milliseconds lifespan);
    std::size_t getLimit(void) const;
    std::chrono::milliseconds getLifespan(void) const;
    LoadBalancer& operator=(LoadBalancer&& other) = default;
    LoadBalancer();
    LoadBalancer(std::size_t limit = 1, std::chrono::milliseconds lifespan = std::chrono::milliseconds{0});
    LoadBalancer(LoadBalancer&& other) = default;
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
