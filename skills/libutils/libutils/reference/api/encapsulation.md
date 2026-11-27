# libutils `encapsulation`

Generated from libutils `v5.0.0` (commit `c129a5e`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/encapsulation/CApi.h`

C API of utils::encapsulation: Dup, Pipe, Poll, Process, SharedObject & SharedMemory (C & C++ compatible) Every function that can fail returns UTILS_OK / UTILS_KO (or NULL / 0 / -1), details in utils_exception

```c
// global
typedef struct utils_dup utils_dup_t; // utils::encapsulation::Dup
typedef struct utils_pipe utils_pipe_t; // utils::encapsulation::Pipe
typedef struct utils_poll utils_poll_t; // utils::encapsulation::Poll
typedef struct utils_process utils_process_t; // utils::encapsulation::Process
typedef struct utils_shared_object utils_shared_object_t; // utils::encapsulation::SharedObject
typedef struct utils_shm utils_shm_t; // utils::encapsulation::SharedMemory
typedef struct utils_shm_messages utils_shm_messages_t; // messages read from a shared memory (id -> payloads)
typedef enum utils_shm_read_filter { C_API_SHM_READ_ALL = 0, C_API_SHM_READ_ZERO_ONLY, C_API_SHM_READ_NON_ZERO_ONLY, C_API_SHM_READ_LAST_ONLY };
typedef enum utils_shm_layout { C_API_SHM_LAYOUT_COMPACT = 0, C_API_SHM_LAYOUT_COMPACT_SEMI_ALIGNED, C_API_SHM_LAYOUT_INTERLEAVED, C_API_SHM_LAYOUT_INTERLEAVED_ALIGNED };
typedef struct utils_process_status { bool unknown; bool exited; int code; int sig; };
typedef struct utils_shm_id { size_t id; pid_t ownership; };
typedef struct utils_shm_target { pid_t sender; size_t limit; pid_t ownership; bool global; };
utils_dup_t* utils_dup_create(int origin, int clone); // NULL on error
void utils_dup_destroy(utils_dup_t* dup); // NULL: nothing (the fds are not closed)
int utils_dup_trigger(utils_dup_t* dup);
int utils_dup_close_origin(utils_dup_t* dup);
int utils_dup_close_clone(utils_dup_t* dup);
int utils_dup_close(utils_dup_t* dup);
int utils_dup_clear(utils_dup_t* dup); // forget the fds (not closed)
int utils_dup_get_origin(const utils_dup_t* dup); // -1 on error
int utils_dup_get_clone(const utils_dup_t* dup); // -1 on error
int utils_dup_set_origin(utils_dup_t* dup, int fd);
int utils_dup_set_clone(utils_dup_t* dup, int fd);
utils_pipe_t* utils_pipe_create(int read, int write); // NULL on error
void utils_pipe_destroy(utils_pipe_t* pipe); // NULL: nothing (closes the fds)
int utils_pipe_trigger(utils_pipe_t* pipe);
int utils_pipe_close_read(utils_pipe_t* pipe);
int utils_pipe_close_write(utils_pipe_t* pipe);
int utils_pipe_close(utils_pipe_t* pipe);
int utils_pipe_clear(utils_pipe_t* pipe); // forget the fds (not closed)
int utils_pipe_get_read(const utils_pipe_t* pipe); // -1 on error
int utils_pipe_get_write(const utils_pipe_t* pipe); // -1 on error
int utils_pipe_set_read(utils_pipe_t* pipe, int fd);
int utils_pipe_set_write(utils_pipe_t* pipe, int fd);
utils_poll_t* utils_poll_create(int flags); // EPOLL_CLOEXEC by default in c++, NULL on error
void utils_poll_destroy(utils_poll_t* poll); // NULL: nothing (closes it)
int utils_poll_init(utils_poll_t* poll, int flags);
int utils_poll_close(utils_poll_t* poll);
int utils_poll_link(utils_poll_t* poll, int fd, uint32_t events, void* data); // data: NULL = the fd in the event
int utils_poll_edit(const utils_poll_t* poll, int fd, uint32_t events, void* data);
int utils_poll_unlink(utils_poll_t* poll, int fd);
size_t utils_poll_wait(const utils_poll_t* poll, int delay, struct epoll_event* events, size_t max); // events filled (max at most), returns their number (0: timeout or error, see utils_exception.exception)
size_t utils_poll_size(const utils_poll_t* poll);
bool utils_poll_contains(const utils_poll_t* poll, int fd);
int utils_poll_get_fd(const utils_poll_t* poll); // -1 on error
utils_process_t* utils_process_create(void); // NULL on error
void utils_process_destroy(utils_process_t* process); // NULL: nothing (the parent kills the child)
bool utils_process_is(const utils_process_t* process); // the child is running
pid_t utils_process_spawn(utils_process_t* process); // fork (Windows: error, no fork), -1 on error
pid_t utils_process_spawn_exec(utils_process_t* process, const char* path, const char* const* args, size_t count); // execvp (argv[0] = path, args after it), -1 on error
void utils_process_replace(utils_process_t* process, const char* path, const char* const* args, size_t count); // execvp in this process (never return: exec or _exit(127))
int utils_process_wait(utils_process_t* process, utils_process_status_t* status); // waitpid
int utils_process_kill(utils_process_t* process); // -s 9
int utils_process_clear(utils_process_t* process); // clear the pipes / dups (fds closed)
int utils_process_pipe(utils_process_t* process, int read, int write);
int utils_process_dup(utils_process_t* process, int origin, int clone);
int utils_process_add_pipe(utils_process_t* process, utils_pipe_t* pipe); // the fds are moved in the process (pipe cleared)
int utils_process_add_dup(utils_process_t* process, utils_dup_t* dup); // the fds are moved in the process (dup cleared)
size_t utils_process_pipes_count(const utils_process_t* process);
size_t utils_process_dups_count(const utils_process_t* process);
int utils_process_get_pipe(const utils_process_t* process, size_t i, int* read, int* write);
int utils_process_get_dup(const utils_process_t* process, size_t i, int* origin, int* clone);
bool utils_process_is_child(const utils_process_t* process);
bool utils_process_is_parent(const utils_process_t* process);
pid_t utils_process_get_pid(const utils_process_t* process); // -1: not spawned or error
utils_shared_object_t* utils_shared_object_create(const char* path); // loaded (RTLD_NOW), NULL on error
void utils_shared_object_destroy(utils_shared_object_t* so); // NULL: nothing (dlclose)
bool utils_shared_object_is_loaded(const utils_shared_object_t* so);
const char* utils_shared_object_path(const utils_shared_object_t* so); // valid while so lives (NULL on error)
void* utils_shared_object_get(const utils_shared_object_t* so); // handle of dlopen (NULL on error)
void* utils_shared_object_load_function(utils_shared_object_t* so, const char* name); // cast it in the function type (NULL on error)
utils_shm_t* utils_shm_create(pid_t ownership); // NULL on error
utils_shm_t* utils_shm_create_default(void); // ownership: getpid(), NULL on error
void utils_shm_destroy(utils_shm_t* shm); // NULL: nothing (closes it)
int utils_shm_init(utils_shm_t* shm, const char* name, size_t size, size_t queue, bool create, utils_shm_layout_t layout, size_t group_size, bool forced);
int utils_shm_close(utils_shm_t* shm);
pid_t utils_shm_get_ownership(const utils_shm_t* shm); // -1 on error
int utils_shm_set_ownership(utils_shm_t* shm, pid_t ownership);
int utils_shm_send(utils_shm_t* shm, const void* bytes, size_t size, const utils_shm_target_t* target, bool last_sending, bool last_transmission, bool failsafe, utils_shm_id_t* id); // a new id for the request (id: optional)
int utils_shm_send_id(utils_shm_t* shm, const void* bytes, size_t size, utils_shm_id_t id, const utils_shm_target_t* target, bool last_sending, bool last_transmission, bool failsafe); // force an id
int utils_shm_send_own_id(utils_shm_t* shm, const void* bytes, size_t size, size_t id, const utils_shm_target_t* target, bool last_sending, bool last_transmission, bool failsafe); // id of this ownership
int utils_shm_trigger(utils_shm_t* shm); // read now (automatic in the normal case)
bool utils_shm_readable(const utils_shm_t* shm, utils_shm_read_filter_t filter);
bool utils_shm_readable_id(const utils_shm_t* shm, utils_shm_id_t id);
int utils_shm_join(const utils_shm_t* shm, bool last); // wait for anything to be read (or any last)
int utils_shm_join_id(const utils_shm_t* shm, utils_shm_id_t id, bool last); // wait for a specific id
utils_shm_messages_t* utils_shm_read(utils_shm_t* shm, utils_shm_read_filter_t filter); // NULL: nothing read (utils_exception.exception false) or error
utils_shm_messages_t* utils_shm_read_id(utils_shm_t* shm, utils_shm_id_t id); // NULL: nothing read (utils_exception.exception false) or error
void utils_shm_messages_destroy(utils_shm_messages_t* messages); // NULL: nothing
size_t utils_shm_messages_count(const utils_shm_messages_t* messages); // number of ids
int utils_shm_messages_id(const utils_shm_messages_t* messages, size_t i, utils_shm_id_t* id);
size_t utils_shm_messages_size(const utils_shm_messages_t* messages, size_t i); // number of payloads of the id i
const void* utils_shm_messages_get(const utils_shm_messages_t* messages, size_t i, size_t j, size_t* size); // payload j of the id i, valid while messages lives (NULL on error)
```

## `utils/encapsulation/CApi.hpp`

Handles of the C API of utils::encapsulation (usable in c++ to get the object)

```cpp
// global
struct utils_dup: public utils_handle, public utils::encapsulation::Dup {
    utils_dup(const int origin, const int clone);
};
struct utils_pipe: public utils_handle, public utils::encapsulation::Pipe {
    utils_pipe(const int read, const int write);
};
struct utils_poll: public utils_handle, public utils::encapsulation::Poll {
    utils_poll(const int flags);
};
struct utils_process: public utils_handle, public utils::encapsulation::Process {
};
struct utils_shared_object: public utils_handle, public utils::encapsulation::SharedObject {
    utils_shared_object(const std::string& path);
};
struct utils_shm: public utils_handle, public utils::encapsulation::SharedMemory {
    utils_shm(const pid_t ownership);
};
struct utils_shm_messages: public utils_handle {
    std::vector<std::pair<utils::encapsulation::shm::Id, std::vector<std::vector<std::byte>>>> messages;
};
```

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
    pid_t spawn(void); // fork (Windows: FatalException UndefinedCall, no fork)
    pid_t spawn(const std::string& path, const std::vector<std::string>& args); // execvp (Windows: CreateProcess, only the fds 0, 1 & 2 can be dup)
    void replace(const std::string& path, const std::vector<std::string>& args); // execvp (Windows: _execvp) (never return: exec or _exit)
    utils::encapsulation::Status wait(void); // waitpid (Windows: WaitForSingleObject, never killed by a signal)
    void kill(void); // -s 9 (Windows: TerminateProcess)
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
    SharedMemory(pid_t ownership = utils::platform::getpid());
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
    template<typename T> T loadFunction(const std::string& name);
    SharedObject& operator=(SharedObject&& other);
    SharedObject(const std::string& path);
    SharedObject(SharedObject&& other);
    ~SharedObject() noexcept;
};
```
