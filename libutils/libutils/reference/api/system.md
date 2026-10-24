# libutils `system`

Generated from libutils `v4.3.1` (commit `7913070`, 2026-10-22) by `scripts/gen_api.py`, do not edit by hand.

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
