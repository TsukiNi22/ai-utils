# libutils `system`

Generated from libutils `v5.0.0` (commit `c129a5e`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/system/CApi.h`

C API of utils::system: gpu handlers, hybrid memories (gpu / ram), id handlers, load balancer & scheduler (C & C++ compatible) The templates are given for the fixed types of type/CApiTypes.h & as bytes (generic) Every function that can fail returns UTILS_OK / UTILS_KO (or NULL / 0), details in utils_exception

```c
#define C_API_HYBRID_MEMORY_DECL(name, ctype) // declares, for each type:
    typedef struct utils_hybrid_memory_<name> utils_hybrid_memory_<name>_t;
    utils_hybrid_memory_<name>_t* utils_hybrid_memory_<name>_create(size_t size, utils_memory_mode_t mode); // size 0: nothing allocated, NULL on error
    utils_hybrid_memory_<name>_t* utils_hybrid_memory_<name>_create_gpu(size_t size, const utils_gpu_t* gpu); // custom mode with a gpu handler, NULL on error
    void utils_hybrid_memory_<name>_destroy(utils_hybrid_memory_<name>_t* memory); // NULL: nothing
    int utils_hybrid_memory_<name>_allocate(utils_hybrid_memory_<name>_t* memory, size_t size); // the previous memory is freed (uninitialized)
    int utils_hybrid_memory_<name>_resize(utils_hybrid_memory_<name>_t* memory, size_t size); // the first values are kept
    int utils_hybrid_memory_<name>_free(utils_hybrid_memory_<name>_t* memory); // size 0, mode kept
    int utils_hybrid_memory_<name>_clear(utils_hybrid_memory_<name>_t* memory); // every byte at 0
    int utils_hybrid_memory_<name>_store(utils_hybrid_memory_<name>_t* memory, const ctype* values, size_t count, size_t offset);
    int utils_hybrid_memory_<name>_fetch(const utils_hybrid_memory_<name>_t* memory, ctype* out, size_t count, size_t offset);
    int utils_hybrid_memory_<name>_get(const utils_hybrid_memory_<name>_t* memory, size_t index, ctype* value);
    int utils_hybrid_memory_<name>_set(utils_hybrid_memory_<name>_t* memory, size_t index, ctype value);
    int utils_hybrid_memory_<name>_fill(utils_hybrid_memory_<name>_t* memory, ctype value);
    int utils_hybrid_memory_<name>_join(utils_hybrid_memory_<name>_t* memory); // wait for the pending gpu operations
    size_t utils_hybrid_memory_<name>_size(const utils_hybrid_memory_<name>_t* memory);
    size_t utils_hybrid_memory_<name>_bytes(const utils_hybrid_memory_<name>_t* memory);
    bool utils_hybrid_memory_<name>_empty(const utils_hybrid_memory_<name>_t* memory);
    bool utils_hybrid_memory_<name>_is_on_gpu(const utils_hybrid_memory_<name>_t* memory);
    utils_memory_mode_t utils_hybrid_memory_<name>_get_mode(const utils_hybrid_memory_<name>_t* memory);
    utils_memory_mode_t utils_hybrid_memory_<name>_get_requested_mode(const utils_hybrid_memory_<name>_t* memory);
    utils_gpu_t* utils_hybrid_memory_<name>_get_gpu(const utils_hybrid_memory_<name>_t* memory); // a new handle sharing the gpu (destroy it), NULL in ram or on error
    uintptr_t utils_hybrid_memory_<name>_native(const utils_hybrid_memory_<name>_t* memory); // device buffer (0 in ram)
    ctype* utils_hybrid_memory_<name>_data(utils_hybrid_memory_<name>_t* memory); // host memory (NULL on the gpu)
#define C_API_ID_HANDLER_DECL(name, ctype) // declares, for each type:
    typedef struct utils_id_handler_<name> utils_id_handler_<name>_t;
    utils_id_handler_<name>_t* utils_id_handler_<name>_create(void); // NULL on error
    void utils_id_handler_<name>_destroy(utils_id_handler_<name>_t* handler); // NULL: nothing
    ctype utils_id_handler_<name>_allocate(utils_id_handler_<name>_t* handler, bool safe_mode); // 0 on error
    int utils_id_handler_<name>_use(utils_id_handler_<name>_t* handler, ctype id, bool safe_mode); // force an id (refused when it's used)
    int utils_id_handler_<name>_free(utils_id_handler_<name>_t* handler, ctype id, bool safe_mode);
    int utils_id_handler_<name>_clear(utils_id_handler_<name>_t* handler, bool safe_mode); // free every id
    ctype utils_id_handler_<name>_id(const utils_id_handler_<name>_t* handler); // counter
    ctype utils_id_handler_<name>_actual(const utils_id_handler_<name>_t* handler, bool safe_mode);
    ctype utils_id_handler_<name>_preview(const utils_id_handler_<name>_t* handler, bool safe_mode); // the next one given by allocate (nothing changed)

// global
typedef struct utils_gpu utils_gpu_t; // std::shared_ptr<utils::system::IGpu> (a backend or a gpu of the C side)
typedef struct utils_hybrid_memory utils_hybrid_memory_t; // utils::system::HybridMemory<std::byte> (bytes)
typedef struct utils_load_balancer utils_load_balancer_t; // utils::system::LoadBalancer of utils_worker_t
typedef struct utils_scheduler utils_scheduler_t; // utils::system::Scheduler
typedef void (*utils_scheduler_task_t)(void* data); // task run in its own thread after the delay
typedef enum utils_memory_mode { C_API_MEMORY_MODE_AUTO = 0, C_API_MEMORY_MODE_GPU, C_API_MEMORY_MODE_RAM, C_API_MEMORY_MODE_CUDA, C_API_MEMORY_MODE_OPENCL, C_API_MEMORY_MODE_VULKAN, C_API_MEMORY_MODE_CUSTOM };
typedef struct utils_gpu_callbacks { bool (*is_available)(void* data); const char* (*get_name)(void* data); size_t (*get_memory)(void* data); uintptr_t (*allocate)(size_t bytes, void* data); void (*free)(uintptr_t buffer, void* data); int (*store)(uintptr_t dst, const void* src, size_t bytes, size_t offset, void* data); int (*fetch)(void* dst, uintptr_t src, size_t bytes, size_t offset, void* data); int (*memset)(uintptr_t buffer, uint8_t value, size_t bytes, size_t offset, void* data); int (*memcpy)(uintptr_t dst, uintptr_t src, size_t bytes, size_t dst_offset, size_t src_offset, void* data); int (*join)(void* data); };
utils_gpu_t* utils_gpu_create(utils_memory_mode_t mode); // NULL on error
utils_gpu_t* utils_gpu_create_custom(const utils_gpu_callbacks_t* callbacks, void* data); // gpu of the C side, NULL on error
void utils_gpu_destroy(utils_gpu_t* gpu); // NULL: nothing (released when the last memory using it is destroyed)
bool utils_gpu_is_available(const utils_gpu_t* gpu);
utils_memory_mode_t utils_gpu_get_mode(const utils_gpu_t* gpu); // C_API_MEMORY_MODE_RAM on error
const char* utils_gpu_get_name(const utils_gpu_t* gpu); // valid until the next call in the thread (NULL on error)
size_t utils_gpu_get_name_r(const utils_gpu_t* gpu, char* buf, size_t size); // copied in buf (truncated, always '\0' terminated), returns the full length like snprintf (0 on error)
size_t utils_gpu_get_memory(const utils_gpu_t* gpu); // in bytes (0: unknown or error)
uintptr_t utils_gpu_allocate(utils_gpu_t* gpu, size_t bytes); // 0 on error
int utils_gpu_free(utils_gpu_t* gpu, uintptr_t buffer);
int utils_gpu_store(utils_gpu_t* gpu, uintptr_t dst, const void* src, size_t bytes, size_t offset);
int utils_gpu_fetch(const utils_gpu_t* gpu, void* dst, uintptr_t src, size_t bytes, size_t offset);
int utils_gpu_memset(utils_gpu_t* gpu, uintptr_t buffer, uint8_t value, size_t bytes, size_t offset);
int utils_gpu_memcpy(utils_gpu_t* gpu, uintptr_t dst, uintptr_t src, size_t bytes, size_t dst_offset, size_t src_offset);
int utils_gpu_join(utils_gpu_t* gpu);
utils_hybrid_memory_t* utils_hybrid_memory_create(size_t bytes, utils_memory_mode_t mode); // NULL on error
utils_hybrid_memory_t* utils_hybrid_memory_create_gpu(size_t bytes, const utils_gpu_t* gpu); // NULL on error
void utils_hybrid_memory_destroy(utils_hybrid_memory_t* memory); // NULL: nothing
int utils_hybrid_memory_allocate(utils_hybrid_memory_t* memory, size_t bytes);
int utils_hybrid_memory_resize(utils_hybrid_memory_t* memory, size_t bytes);
int utils_hybrid_memory_free(utils_hybrid_memory_t* memory);
int utils_hybrid_memory_clear(utils_hybrid_memory_t* memory);
int utils_hybrid_memory_store(utils_hybrid_memory_t* memory, const void* data, size_t bytes, size_t offset);
int utils_hybrid_memory_fetch(const utils_hybrid_memory_t* memory, void* out, size_t bytes, size_t offset);
int utils_hybrid_memory_fill(utils_hybrid_memory_t* memory, uint8_t value);
int utils_hybrid_memory_join(utils_hybrid_memory_t* memory);
size_t utils_hybrid_memory_bytes(const utils_hybrid_memory_t* memory);
bool utils_hybrid_memory_empty(const utils_hybrid_memory_t* memory);
bool utils_hybrid_memory_is_on_gpu(const utils_hybrid_memory_t* memory);
utils_memory_mode_t utils_hybrid_memory_get_mode(const utils_hybrid_memory_t* memory);
utils_memory_mode_t utils_hybrid_memory_get_requested_mode(const utils_hybrid_memory_t* memory);
utils_gpu_t* utils_hybrid_memory_get_gpu(const utils_hybrid_memory_t* memory); // a new handle sharing the gpu (destroy it), NULL in ram or on error
uintptr_t utils_hybrid_memory_native(const utils_hybrid_memory_t* memory); // device buffer (0 in ram)
void* utils_hybrid_memory_data(utils_hybrid_memory_t* memory); // host memory (NULL on the gpu)
C_API_VALUE_TYPES(C_API_HYBRID_MEMORY_DECL)
C_API_INT_TYPES(C_API_ID_HANDLER_DECL)
utils_load_balancer_t* utils_load_balancer_create(size_t limit, uint64_t lifespan_ms); // limit 0: unlimited, lifespan 0: never killed, NULL on error
void utils_load_balancer_destroy(utils_load_balancer_t* balancer); // NULL: nothing (every worker destroyed)
utils_worker_t* utils_load_balancer_get_worker(utils_load_balancer_t* balancer); // wait for a free worker (set as working), NULL on error
int utils_load_balancer_spawn(utils_load_balancer_t* balancer, size_t n); // n new workers
int utils_load_balancer_kill(utils_load_balancer_t* balancer, bool forced); // every not working worker (forced: the working ones too, their pointer become invalid)
int utils_load_balancer_kill_n(utils_load_balancer_t* balancer, size_t n, bool forced); // n workers (the not working ones first)
int utils_load_balancer_set_limit(utils_load_balancer_t* balancer, size_t limit);
int utils_load_balancer_set_lifespan(utils_load_balancer_t* balancer, uint64_t lifespan_ms);
size_t utils_load_balancer_get_limit(const utils_load_balancer_t* balancer);
uint64_t utils_load_balancer_get_lifespan(const utils_load_balancer_t* balancer); // in ms
size_t utils_load_balancer_size(const utils_load_balancer_t* balancer); // number of workers
utils_scheduler_t* utils_scheduler_create(void); // NULL on error
void utils_scheduler_destroy(utils_scheduler_t* scheduler); // NULL: nothing (the waiting tasks are canceled)
size_t utils_scheduler_schedule(utils_scheduler_t* scheduler, uint64_t delay_ms, utils_scheduler_task_t task, void* data); // id of the task (0 on error)
int utils_scheduler_cancel(utils_scheduler_t* scheduler, size_t id);
int utils_scheduler_cancel_all(utils_scheduler_t* scheduler);
```

## `utils/system/CApi.hpp`

Handles of the C API of utils::system (usable in c++ to get the object)

```cpp
#define C_API_HYBRID_MEMORY_HANDLE(name, ctype) // declares, for each type:
    struct utils_hybrid_memory_<name>: public utils_handle, public utils::system::HybridMemory<ctype> {
    using utils::system::HybridMemory<ctype>::HybridMemory;
    };
#define C_API_ID_HANDLER_HANDLE(name, ctype) // declares, for each type:
    struct utils_id_handler_<name>: public utils_handle, public utils::system::IdHandler<ctype> {};
#define C_API_HYBRID_MEMORY_DEF(name, ctype) // declares, for each type:
    utils_hybrid_memory_<name>_t* utils_hybrid_memory_<name>_create(size_t size, utils_memory_mode_t mode)
    {return utils::exception::c_api::guard_fallback(nullptr, [&](void) -> utils_hybrid_memory_<name>_t* {return memory_new<utils_hybrid_memory_<name>_t>(size, mode);});}
    utils_hybrid_memory_<name>_t* utils_hybrid_memory_<name>_create_gpu(size_t size, const utils_gpu_t* gpu)
    {return utils::exception::c_api::guard_fallback(nullptr, [&](void) -> utils_hybrid_memory_<name>_t* {return new utils_hybrid_memory_<name>_t(size, utils::exception::c_api::not_null_ref(gpu, "gpu").gpu);});}
    void utils_hybrid_memory_<name>_destroy(utils_hybrid_memory_<name>_t* m) {(void)utils::exception::c_api::guard([&](void) {delete m;});};
    int utils_hybrid_memory_<name>_allocate(utils_hybrid_memory_<name>_t* m, size_t size) {return utils::exception::c_api::guard([&](void) {memory(m).allocate(size);});};
    int utils_hybrid_memory_<name>_resize(utils_hybrid_memory_<name>_t* m, size_t size) {return utils::exception::c_api::guard([&](void) {memory(m).resize(size);});};
    int utils_hybrid_memory_<name>_free(utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard([&](void) {memory(m).free();});};
    int utils_hybrid_memory_<name>_clear(utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard([&](void) {memory(m).clear();});};
    int utils_hybrid_memory_<name>_store(utils_hybrid_memory_<name>_t* m, const ctype* values, size_t count, size_t offset)
    {
    return utils::exception::c_api::guard([&](void) {
    if (count > 0) (void)utils::exception::c_api::not_null(values, "values");
    memory(m).store(std::span<const ctype>(values, count), offset);
    });
    }
    int utils_hybrid_memory_<name>_fetch(const utils_hybrid_memory_<name>_t* m, ctype* out, size_t count, size_t offset)
    {
    return utils::exception::c_api::guard([&](void) {
    if (count > 0) (void)utils::exception::c_api::not_null(out, "output");
    memory(m).fetch(std::span<ctype>(out, count), offset);
    });
    }
    int utils_hybrid_memory_<name>_get(const utils_hybrid_memory_<name>_t* m, size_t index, ctype* value)
    {return utils::exception::c_api::guard([&](void) {ctype& res = utils::exception::c_api::not_null_ref(value, "value"); res = memory(m).get(index);});}
    int utils_hybrid_memory_<name>_set(utils_hybrid_memory_<name>_t* m, size_t index, ctype value) {return utils::exception::c_api::guard([&](void) {memory(m).set(index, value);});};
    int utils_hybrid_memory_<name>_fill(utils_hybrid_memory_<name>_t* m, ctype value) {return utils::exception::c_api::guard([&](void) {memory(m).fill(value);});};
    int utils_hybrid_memory_<name>_join(utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard([&](void) {memory(m).join();});};
    size_t utils_hybrid_memory_<name>_size(const utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard_fallback(0, [&](void) -> std::size_t {return memory(m).size();});};
    size_t utils_hybrid_memory_<name>_bytes(const utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard_fallback(0, [&](void) -> std::size_t {return memory(m).bytes();});};
    bool utils_hybrid_memory_<name>_empty(const utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard_fallback(true, [&](void) -> bool {return memory(m).empty();});};
    bool utils_hybrid_memory_<name>_is_on_gpu(const utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard_fallback(false, [&](void) -> bool {return memory(m).isOnGpu();});};
    utils_memory_mode_t utils_hybrid_memory_<name>_get_mode(const utils_hybrid_memory_<name>_t* m)
    {return utils::exception::c_api::guard_fallback(C_API_MEMORY_MODE_RAM, [&](void) -> utils_memory_mode_t {return static_cast<utils_memory_mode_t>(memory(m).getMode());});}
    utils_memory_mode_t utils_hybrid_memory_<name>_get_requested_mode(const utils_hybrid_memory_<name>_t* m)
    {return utils::exception::c_api::guard_fallback(C_API_MEMORY_MODE_RAM, [&](void) -> utils_memory_mode_t {return static_cast<utils_memory_mode_t>(memory(m).getRequestedMode());});}
    utils_gpu_t* utils_hybrid_memory_<name>_get_gpu(const utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard_fallback(nullptr, [&](void) -> utils_gpu_t* {return gpu_handle(memory(m).getGpu());});};
    uintptr_t utils_hybrid_memory_<name>_native(const utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard_fallback(static_cast<uintptr_t>(0), [&](void) -> uintptr_t {return memory(m).native();});};
    ctype* utils_hybrid_memory_<name>_data(utils_hybrid_memory_<name>_t* m) {return utils::exception::c_api::guard_fallback(nullptr, [&](void) -> ctype* {return memory(m).data();});}
#define C_API_ID_HANDLER_DEF(name, ctype) // declares, for each type:
    utils_id_handler_<name>_t* utils_id_handler_<name>_create(void) {return utils::exception::c_api::guard_fallback(nullptr, [](void) -> utils_id_handler_<name>_t* {return new utils_id_handler_<name>_t();});};
    void utils_id_handler_<name>_destroy(utils_id_handler_<name>_t* handler) {(void)utils::exception::c_api::guard([&](void) {delete handler;});};
    ctype utils_id_handler_<name>_allocate(utils_id_handler_<name>_t* handler, bool safe_mode)
    {return utils::exception::c_api::guard_fallback(static_cast<ctype>(0), [&](void) -> ctype {return utils::exception::c_api::not_null_ref(handler, "handler").allocate(safe_mode);});}
    int utils_id_handler_<name>_use(utils_id_handler_<name>_t* handler, ctype id, bool safe_mode)
    {return utils::exception::c_api::guard([&](void) {utils::exception::c_api::not_null_ref(handler, "handler").use(id, safe_mode);});}
    int utils_id_handler_<name>_free(utils_id_handler_<name>_t* handler, ctype id, bool safe_mode)
    {return utils::exception::c_api::guard([&](void) {const ctype given = id; utils::exception::c_api::not_null_ref(handler, "handler").free(given, safe_mode);});}
    int utils_id_handler_<name>_clear(utils_id_handler_<name>_t* handler, bool safe_mode)
    {return utils::exception::c_api::guard([&](void) {utils::exception::c_api::not_null_ref(handler, "handler").clear(safe_mode);});}
    ctype utils_id_handler_<name>_id(const utils_id_handler_<name>_t* handler)
    {return utils::exception::c_api::guard_fallback(static_cast<ctype>(0), [&](void) -> ctype {return utils::exception::c_api::not_null_ref(handler, "handler").id();});}
    ctype utils_id_handler_<name>_actual(const utils_id_handler_<name>_t* handler, bool safe_mode)
    {return utils::exception::c_api::guard_fallback(static_cast<ctype>(0), [&](void) -> ctype {return utils::exception::c_api::not_null_ref(handler, "handler").actual(safe_mode);});}
    ctype utils_id_handler_<name>_preview(const utils_id_handler_<name>_t* handler, bool safe_mode)
    {return utils::exception::c_api::guard_fallback(static_cast<ctype>(0), [&](void) -> ctype {return utils::exception::c_api::not_null_ref(handler, "handler").preview(safe_mode);});}

// global
struct utils_gpu: public utils_handle {
    std::shared_ptr<utils::system::IGpu> gpu;
    utils_gpu(std::shared_ptr<utils::system::IGpu> g);
};
struct utils_hybrid_memory: public utils_handle, public utils::system::HybridMemory<std::byte> {
    using utils::system::HybridMemory<std::byte>::HybridMemory;
};
C_API_VALUE_TYPES(C_API_HYBRID_MEMORY_HANDLE)
C_API_INT_TYPES(C_API_ID_HANDLER_HANDLE)
struct utils_load_balancer: public utils_handle, public utils::system::LoadBalancer<utils_worker> {
    utils_load_balancer(const std::size_t limit, const std::chrono::milliseconds lifespan);
};
struct utils_scheduler: public utils_handle, public utils::system::Scheduler {
};
```

## `utils/system/HybridMemory.hpp`

Memory loaded on the gpu when available, else in the ram

Namespace: `utils::system`

```cpp
// namespace utils::system
utils::system::IGpu* make_raw_gpu(const utils::system::MemoryMode mode);
std::shared_ptr<utils::system::IGpu> make_shared_gpu(const utils::system::MemoryMode mode);
std::unique_ptr<utils::system::IGpu> make_unique_gpu(const utils::system::MemoryMode mode);
std::unique_ptr<utils::system::IGpu> make_gpu(const utils::system::MemoryMode mode = utils::system::MemoryMode::Auto);
template<typename T> class HybridMemory {
    void allocate(const std::size_t size); // free the previous memory then allocate size T (uninitialized)
    void resize(const std::size_t size); // new size, the first min(size, old size) T kept
    void free(void) noexcept; // release the memory (size 0, mode kept)
    void clear(void); // every byte set to 0, allocation kept
    void store(std::span<const T> values, const std::size_t offset = 0); // copy the values from the index offset (OutOfBounds)
    void fetch(std::span<T> out, const std::size_t offset = 0) const; // copy out.size() T from the index offset (OutOfBounds)
    T get(const std::size_t index) const; // one T (a transfer on the gpu: prefer fetch for many)
    void set(const std::size_t index, const T& value); // one T (a transfer on the gpu: prefer store for many)
    void fill(const T& value); // every T set to value
    std::vector<T> toVector(void) const; // whole memory copied in the ram
    void join(void); // wait for the pending gpu operations (nothing in ram)
    std::size_t bytes(void) const; // size in bytes
    bool empty(void) const; // nothing allocated
    bool isOnGpu(void) const; // memory stored by a gpu handler
    std::size_t size(void) const;
    utils::system::MemoryMode getMode(void) const;
    utils::system::MemoryMode getRequestedMode(void) const;
    std::shared_ptr<utils::system::IGpu> getGpu(void) const;
    utils::system::GpuBuffer native(void) const; // device buffer for the computes (0 in ram)
    T* data(void); // host memory (nullptr on the gpu)
    const T* data(void) const;
    T operator[](const std::size_t index) const;
    HybridMemory& operator=(HybridMemory&& other) noexcept; // take the memory and the handler of other
    HybridMemory(const utils::system::MemoryMode mode = utils::system::MemoryMode::Auto); // select the backend (make_gpu), nothing allocated
    HybridMemory(const std::size_t size, const utils::system::MemoryMode mode = utils::system::MemoryMode::Auto); // select the backend & allocate size T
    HybridMemory(const std::size_t size, std::shared_ptr<utils::system::IGpu> gpu); // Custom mode with a user handler (InvalidArgument when nullptr or unavailable)
    HybridMemory(HybridMemory&& other) noexcept; // take the memory and the handler of other
    ~HybridMemory() noexcept; // release the memory
};
```

## `utils/system/HybridMemoryDefine.hpp`

Modes of the HybridMemory (gpu / ram)

Namespace: `utils::system`

```cpp
// namespace utils::system
enum class MemoryMode {Auto, Gpu, Ram, Cuda, OpenCL, Vulkan, Custom}
```

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

## `utils/system/gpu/AGpu.hpp`

Shared part of the gpu handlers (driver loading, device info)

Namespace: `utils::system`

```cpp
#define GPU_API_CALL __stdcall
#define GPU_API_CALL

// namespace utils::system
class AGpu: public utils::system::IGpu {
    bool isAvailable(void) const;
    std::string getName(void) const;
    std::size_t getMemory(void) const;
    AGpu() = default;
    ~AGpu() = default;
};
```

## `utils/system/gpu/CudaGpu.hpp`

Cuda gpu handler of the HybridMemory

Namespace: `utils::system`

```cpp
// namespace utils::system
class CudaGpu : public utils::system::AGpu {
    utils::system::GpuBuffer allocate(const std::size_t bytes);
    void free(const utils::system::GpuBuffer buffer) noexcept;
    void store(const utils::system::GpuBuffer dst, const void* src, const std::size_t bytes, const std::size_t offset = 0);
    void fetch(void* dst, const utils::system::GpuBuffer src, const std::size_t bytes, const std::size_t offset = 0) const;
    void memset(const utils::system::GpuBuffer buffer, const std::byte value, const std::size_t bytes, const std::size_t offset = 0);
    void memcpy(const utils::system::GpuBuffer dst, const utils::system::GpuBuffer src, const std::size_t bytes, const std::size_t dstOffset = 0, const std::size_t srcOffset = 0);
    void join(void);
    utils::system::MemoryMode getMode(void) const;
    CudaGpu(); // load libcuda.so.1 (nvcuda.dll on Windows), unavailable without the driver or a device (never throw)
    ~CudaGpu() noexcept; // release the device objects
};
```

## `utils/system/gpu/Gpu.hpp`

Every gpu handler of the HybridMemory

## `utils/system/gpu/IGpu.hpp`

Interface of a gpu handler used by the HybridMemory

Namespace: `utils::system`

```cpp
// namespace utils::system
using GpuBuffer = std::uintptr_t; // opaque device buffer of a handler (CUdeviceptr, cl_mem, Vulkan allocation...), 0 = none
class IGpu {
    virtual bool isAvailable(void) const = 0; // backend loaded and a device ready
    virtual utils::system::MemoryMode getMode(void) const = 0; // Cuda | OpenCL | Vulkan | Custom
    virtual std::string getName(void) const = 0; // name of the device ("[None]" when unavailable)
    virtual std::size_t getMemory(void) const = 0; // memory of the device in bytes (0 = unknown)
    virtual utils::system::GpuBuffer allocate(const std::size_t bytes) = 0; // new device buffer (GpuAllocation error)
    virtual void free(const utils::system::GpuBuffer buffer) noexcept = 0; // release a buffer (0 = nothing to do)
    virtual void store(const utils::system::GpuBuffer dst, const void* src, const std::size_t bytes, const std::size_t offset = 0) = 0; // host -> device
    virtual void fetch(void* dst, const utils::system::GpuBuffer src, const std::size_t bytes, const std::size_t offset = 0) const = 0; // device -> host
    virtual void memset(const utils::system::GpuBuffer buffer, const std::byte value, const std::size_t bytes, const std::size_t offset = 0) = 0; // set every byte of the range
    virtual void memcpy(const utils::system::GpuBuffer dst, const utils::system::GpuBuffer src, const std::size_t bytes, const std::size_t dstOffset = 0, const std::size_t srcOffset = 0) = 0; // device -> device
    virtual void join(void) = 0; // wait for the pending operations of the device
    IGpu() = default;
    virtual ~IGpu() = default;
};
```

## `utils/system/gpu/OpenCLGpu.hpp`

OpenCL gpu handler of the HybridMemory

Namespace: `utils::system`

```cpp
// namespace utils::system
class OpenCLGpu : public utils::system::AGpu {
    utils::system::GpuBuffer allocate(const std::size_t bytes);
    void free(const utils::system::GpuBuffer buffer) noexcept;
    void store(const utils::system::GpuBuffer dst, const void* src, const std::size_t bytes, const std::size_t offset = 0);
    void fetch(void* dst, const utils::system::GpuBuffer src, const std::size_t bytes, const std::size_t offset = 0) const;
    void memset(const utils::system::GpuBuffer buffer, const std::byte value, const std::size_t bytes, const std::size_t offset = 0);
    void memcpy(const utils::system::GpuBuffer dst, const utils::system::GpuBuffer src, const std::size_t bytes, const std::size_t dstOffset = 0, const std::size_t srcOffset = 0);
    void join(void);
    utils::system::MemoryMode getMode(void) const;
    OpenCLGpu(); // load libOpenCL.so.1 (OpenCL.dll on Windows), unavailable without the driver or a device (never throw)
    ~OpenCLGpu() noexcept; // release the device objects
};
```

## `utils/system/gpu/VulkanGpu.hpp`

Vulkan gpu handler of the HybridMemory

Namespace: `utils::system`

```cpp
// namespace utils::system
class VulkanGpu : public utils::system::AGpu {
    utils::system::GpuBuffer allocate(const std::size_t bytes);
    void free(const utils::system::GpuBuffer buffer) noexcept;
    void store(const utils::system::GpuBuffer dst, const void* src, const std::size_t bytes, const std::size_t offset = 0);
    void fetch(void* dst, const utils::system::GpuBuffer src, const std::size_t bytes, const std::size_t offset = 0) const;
    void memset(const utils::system::GpuBuffer buffer, const std::byte value, const std::size_t bytes, const std::size_t offset = 0);
    void memcpy(const utils::system::GpuBuffer dst, const utils::system::GpuBuffer src, const std::size_t bytes, const std::size_t dstOffset = 0, const std::size_t srcOffset = 0);
    void join(void);
    utils::system::MemoryMode getMode(void) const;
    VulkanGpu(); // load libvulkan.so.1 (vulkan-1.dll on Windows), unavailable without the driver or a device (never throw)
    ~VulkanGpu() noexcept; // release the device objects
};
```
