# libutils `encapsulation`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

## `utils/encapsulation/Dup.hpp`

Basic encapsulation for pipe

Namespace: `utils::encapsulation`

```cpp
// namespace utils::encapsulation
class Dup: private utils::security::observer::Observer<"Dup"> {
    void trigger(void);
    void closeOrigin(void) noexcept;
    void closeClone(void) noexcept;
    void close(void) noexcept;
    void clear(void);
    int getOrigin(void) const;
    int getClone(void) const;
    void setOrigin(const int fd = -1);
    void setClone(const int fd = -1);
    Dup& operator=(Dup&& other);
    Dup(const int origin = -1, const int clone = -1);
    Dup(Dup&& other);
    ~Dup() = default;
};
```

## `utils/encapsulation/Pipe.hpp`

Basic encapsulation for pipe

Namespace: `utils::encapsulation`

```cpp
// namespace utils::encapsulation
class Pipe: private utils::security::observer::Observer<"Pipe"> {
    void trigger(void);
    void closeRead(void) noexcept;
    void closeWrite(void) noexcept;
    void close(void) noexcept;
    void clear(void);
    const std::array<int, 2>& getFds(void) const;
    int getRead(void) const;
    int getWrite(void) const;
    void setFds(const std::array<int, 2>& fds);
    void setRead(const int fd = -1);
    void setWrite(const int fd = -1);
    Pipe& operator=(Pipe&& other);
    Pipe(const int fds[2]);
    Pipe(const std::array<int, 2>& fds);
    Pipe(const int read = -1, const int write = -1);
    Pipe(Pipe&& other);
    ~Pipe();
};
```

## `utils/encapsulation/Poll.hpp`

Encapsulation of the epoll

Namespace: `utils::encapsulation`

```cpp
// namespace utils::encapsulation
class Poll: private utils::security::observer::Observer<"Poll"> {
    void init(int flags = EPOLL_CLOEXEC);
    void close(void);
    void link(int fd, std::uint32_t events, void* data = nullptr);
    void edit(int fd, std::uint32_t events, void* data = nullptr) const;
    void unlink(int fd);
    std::vector<struct epoll_event> wait(int delay = -1, std::size_t limits = 0) const;
    std::size_t size(void) const;
    bool contains(int fd) const;
    int getFd(void) const;
    Poll& operator=(Poll&& other);
    Poll(int flags = EPOLL_CLOEXEC);
    Poll(Poll&& other);
    ~Poll();
};
```

## `utils/encapsulation/Process.hpp`

Process encapsulation class

Namespace: `utils::encapsulation`

```cpp
// namespace utils::encapsulation
struct Status {
    bool unknown = false;
    bool exited = false;
    int code = 0; // exited
    int sig = 0; // killed
};
class Process: private utils::security::observer::Observer<"Process"> {
    bool is(void) const;
    pid_t spawn(void); // fork
    pid_t spawn(const std::string& path, const std::vector<std::string>& args); // execvp
    _noreturn void replace(const std::string& path, const std::vector<std::string>& args); // execvp (never return: exec or _exit)
    utils::encapsulation::Status wait(void); // waitpid
    void kill(void); // -s 9
    void clear(void); // clear Pipe/Dup (close fd)
    void pipe(const int read, const int write);
    void dup(const int origin, const int clone = -1);
    void pipe(utils::encapsulation::Pipe& pipe);
    void dup(utils::encapsulation::Dup& dup);
    void pipe(std::vector<utils::encapsulation::Pipe>& pipes);
    void dup(std::vector<utils::encapsulation::Dup>& dups);
    std::vector<utils::encapsulation::Pipe>& getPipes(void);
    std::vector<utils::encapsulation::Dup>& getDups(void);
    const std::vector<utils::encapsulation::Pipe>& getPipes(void) const;
    const std::vector<utils::encapsulation::Dup>& getDups(void) const;
    bool isChild(void) const;
    bool isParent(void) const;
    pid_t getPid(void) const;
    Process& operator=(Process&& other);
    Process() = default;
    Process(Process&& other): utils::security::observer::Observer<"Process">(std::move(other)), _pipes;
    ~Process(); // only the parent kill the child
};
```

## `utils/encapsulation/SharedMemory.hpp`

Encapsulation for shared memory

Namespace: `std`, `utils::encapsulation`, `utils::encapsulation::shm`

```cpp
// namespace utils::encapsulation::shm
struct ShmMetadata {
    std::size_t size = 0;
    std::size_t queue = 0;
    std::atomic<pid_t> connected{0}; // number of connected
    std::atomic<std::uint32_t> readable{1}; // (signal) awake the reader
    std::atomic<std::uint64_t> sequence{0}; // last sequence number given to a request (0 = none)
};
struct Id {
    std::size_t id = 0; // 0 == invalid/unset id
    pid_t ownership = 0; // ownership of the id
    bool operator==(const utils::encapsulation::shm::Id& other) const;
    std::strong_ordering operator<=>(const utils::encapsulation::shm::Id&) const = default;
    Id() = default;
    Id(std::size_t id);
    Id(std::size_t id, pid_t ownership);
};
struct Target {
    pid_t sender = 0; // ownership of the sender
    std::size_t limit = 0; // number of people who will read it (0 = every connected reader for a global request)
    pid_t ownership = 0; // to only a specific reader
    bool global = false; // to all reader
    std::atomic<std::size_t> readed{0}; // number of time readed
    Target() = default;
    Target(const std::size_t limit);
    Target(const std::size_t limit, const pid_t ownership);
};
struct ShmRequestMetadata {
    std::atomic<std::uint8_t> flag{0}; // request flag | 0 = no data / readed, 1 = in writting/reading (unique), 2 = data / to read (shared), 3 = reset in process
    std::atomic<std::size_t> reader{0}; // number of reader (if value need to be edited wait until reader == 1)
    std::atomic<std::size_t> waiting{0}; // number of reader in waiting status (reset of data)
    utils::encapsulation::shm::Id id;
    utils::encapsulation::shm::Target target;
    std::pair<bool, bool> last = {false, false}; // [last sending] only trigger the join | [last transmition] free the id on read if it's is ownership, otherwhise remove it from it's storage
    std::size_t size = 0; // total size of the bytes to not read the whole 'slot' with garbage from before
    std::uint64_t sequence = 0; // unique number of the request, used by a reader to not read twice the same request
};
struct Slot {
    utils::encapsulation::shm::ShmRequestMetadata* metadata = nullptr;
    std::byte* bytes = nullptr;
    bool operator!(void) const;
    Slot() = default;
    Slot(utils::encapsulation::shm::ShmRequestMetadata* metadata, std::byte* bytes);
};
enum class ReadFilter {All, ZeroOnly, NonZeroOnly, LastOnly}
enum class LayoutPolicy {Compact, CompactSemiAligned, Interleaved, InterleavedAligned}
constexpr std::size_t align_up(const std::size_t size, const std::size_t alignment);
constexpr std::size_t interleaved_stride(const std::size_t size);
std::size_t align_ceil(const std::size_t size);
// namespace std
template<> struct hash<utils::encapsulation::shm::Id> {
    std::size_t operator()(const utils::encapsulation::shm::Id& id) const;
};
// namespace utils::encapsulation
class SharedMemory: private utils::security::observer::Observer<"SharedMemory"> {
    void close(void);
    void send(const std::vector<std::byte>& bytes, const utils::encapsulation::shm::Id& id, const utils::encapsulation::shm::Target& target, bool lastSending = false, bool lastTransmission = false, bool failsafe = false); // force an id for the request
    utils::encapsulation::shm::Id send(const std::vector<std::byte>& bytes, const utils::encapsulation::shm::Target& target, bool lastSending = false, bool lastTransmission = false, bool failsafe = false); // allocate an id for this request
    std::optional<std::unordered_map<utils::encapsulation::shm::Id, std::vector<std::vector<std::byte>>>> read(const utils::encapsulation::shm::ReadFilter filter = utils::encapsulation::shm::ReadFilter::All);
    std::optional<std::vector<std::vector<std::byte>>> read(const utils::encapsulation::shm::Id& id);
    bool readable(const utils::encapsulation::shm::ReadFilter filter = utils::encapsulation::shm::ReadFilter::All) const;
    void join(bool last = false) const; // wait for anything to be readed (or any last to be readed)
    void join(const utils::encapsulation::shm::Id& id, bool last = false) const; // for a specifc id to be readed (wait until the last awnser or just any)
    void ownership(pid_t ownership);
    pid_t ownership(void) const;
    template<bool create = false, utils::encapsulation::shm::LayoutPolicy policy = utils::encapsulation::shm::LayoutPolicy::Compact, std::size_t groupSize = 2, bool forced = false> void init(const std::string& name, std::size_t size = 1, std::size_t queue = 1); // size/queue ignored in 'client' mode | size is only for a single section, real size: sizeof(IdHandler lock) + (size + sizeof(metadata)) * queue
    void trigger(void); // auto in normal case
    void send(const std::vector<std::byte>& bytes, std::size_t id, const utils::encapsulation::shm::Target& target, bool lastSending = false, bool lastTransmission = false, bool failsafe = false); // own id
    bool readable(const utils::encapsulation::shm::Id& id) const;
    SharedMemory(std::uint16_t ownership);
    SharedMemory(pid_t ownership = ::getpid());
    ~SharedMemory();
};
```

## `utils/encapsulation/SharedObject.hpp`

Definition of the encapsulation for shared object (.so)

Namespace: `utils::encapsulation`

```cpp
// namespace utils::encapsulation
class SharedObject: private utils::security::observer::Observer<"SharedObject"> {
    bool isLoaded(void) const;
    std::string_view path(void) const;
    void* get(void) const;
    [deprecated ~v4.0.0] bool isloaded(void) const;
    template<typename T> T loadFunction(const std::string& name);
    SharedObject& operator=(SharedObject&& other);
    SharedObject(const std::string& path);
    SharedObject(SharedObject&& other);
    ~SharedObject() noexcept;
};
```
