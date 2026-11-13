# libutils `platform`

Generated from libutils `v4.3.1` (commit `7913070`, 2026-10-22) by `scripts/gen_api.py`, do not edit by hand.

## `utils/platform/Dlfcn.hpp`

Dynamic loading calls of <dlfcn.h> for Unix & Windows (same names, same contract) Unix: the system call, Windows: LoadLibrary / GetProcAddress / FreeLibrary

Namespace: `utils::platform`

```cpp
#define RTLD_LAZY 0x00001
#define RTLD_NOW 0x00002
#define RTLD_GLOBAL 0x00100
#define RTLD_LOCAL 0

// global
struct Dl_info { // xstyle: ignore CPP-CLASS-NAME (POSIX name)
    const char* dli_fname; // path of the module (thread local storage, valid until the next dladdr of the thread)
    void* dli_fbase; // base address of the module
    const char* dli_sname; // always nullptr on Windows
    void* dli_saddr; // always nullptr on Windows
};
// namespace utils::platform
void* dlopen(const char* path, int flags); // nullptr on error (dlerror)
void* dlsym(void* handle, const char* name); // nullptr on error (dlerror)
int dlclose(void* handle); // 0 on success
char* dlerror(void); // last error of the thread, nullptr if none (cleared by the call)
int dladdr(const void* addr, Dl_info* info); // 0 on error
```

## `utils/platform/Epoll.hpp`

epoll for Unix & Windows (same names, same behavior) Unix: <sys/epoll.h>, Windows: emulation over WSAPoll (sockets only)

Namespace: `utils::platform`

```cpp
#define EPOLLIN 0x001u
#define EPOLLPRI 0x002u
#define EPOLLOUT 0x004u
#define EPOLLERR 0x008u
#define EPOLLHUP 0x010u
#define EPOLLRDNORM 0x040u
#define EPOLLWRNORM 0x100u
#define EPOLLRDHUP 0x2000u
#define EPOLLONESHOT (1u << 30)
#define EPOLLET (1u << 31) // not available on Windows (level triggered only): EINVAL
#define EPOLL_CLOEXEC 02000000 // ignored on Windows
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

// global
union epoll_data { // xstyle: ignore CPP-CLASS-NAME (POSIX name)
    void* ptr;
    int fd;
    std::uint32_t u32;
    std::uint64_t u64;
};
struct epoll_event { // xstyle: ignore CPP-CLASS-NAME (POSIX name)
    std::uint32_t events;
    union epoll_data data;
};
using epoll_data_t = union epoll_data;
// namespace utils::platform
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);
```

## `utils/platform/Platform.hpp`

Detection of the target platform (Unix or Windows) UTILS_UNIX / UTILS_WINDOWS, UTILS_PLATFORM

```cpp
#define UTILS_WINDOWS
#define UTILS_PLATFORM "windows"
#define UTILS_UNIX
#define UTILS_PLATFORM "unix"

```

## `utils/platform/Socket.hpp`

BSD socket calls for Unix & Windows (same names, same contract: -1 & errno) Unix: the system call, Windows: Winsock 2 (started on the first use, errors translated in errno)

Namespace: `utils::platform`

```cpp
#define MSG_NOSIGNAL 0 // no SIGPIPE on Windows

// global
using socklen_t = int; // <ws2tcpip.h>
// namespace utils::platform
int socket(int domain, int type, int protocol);
int connect(int fd, const struct sockaddr* addr, socklen_t len);
int bind(int fd, const struct sockaddr* addr, socklen_t len);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr* addr, socklen_t* len);
ssize_t recv(int fd, void* buf, std::size_t len, int flags);
ssize_t send(int fd, const void* buf, std::size_t len, int flags);
int setsockopt(int fd, int level, int name, const void* value, socklen_t len); // Windows: SO_SNDTIMEO / SO_RCVTIMEO take a timeval, SO_REUSEADDR is the default (no-op)
int getsockname(int fd, struct sockaddr* addr, socklen_t* len);
int getpeername(int fd, struct sockaddr* addr, socklen_t* len);
int getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res); // EAI_* error code (gai_strerror)
void freeaddrinfo(struct addrinfo* res);
const char* gai_strerror(int code);
const char* gai_strerrorA(int code); // call renamed by the <ws2tcpip.h> macro
```

## `utils/platform/Termios.hpp`

Terminal calls of <termios.h> for Unix & Windows (same names, same contract: -1 & errno) Unix: the system call, Windows: console mode (ICANON = line input, ECHO = echo, ISIG = ctrl+c)

Namespace: `utils::platform`

```cpp
#define NCCS 32
#define ISIG 0000001
#define ICANON 0000002
#define ECHO 0000010
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

// global
using tcflag_t = unsigned int;
using cc_t = unsigned char;
using speed_t = unsigned int;
struct termios { // xstyle: ignore CPP-CLASS-NAME (POSIX name)
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag; // ISIG, ICANON, ECHO -> console mode
    cc_t c_line;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
    unsigned long c_console; // Windows: console mode read by tcgetattr (restored as is by tcsetattr)
};
// namespace utils::platform
int tcgetattr(int fd, struct termios* term); // Windows: ENOTTY when the fd is not a console
int tcsetattr(int fd, int action, const struct termios* term); // Windows: without ICANON the input gives the escape sequences (VT input)
```

## `utils/platform/Unistd.hpp`

POSIX calls of <unistd.h> & co for Unix & Windows (same names, same contract: -1 & errno) Unix: the system call, Windows: CRT / Win32 implementation

Namespace: `utils::platform`

```cpp
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// global
using nfds_t = unsigned long;
// namespace utils::platform
int close(int fd); // any fd (Windows: socket, CRT fd or emulated epoll)
ssize_t read(int fd, void* buf, std::size_t count); // Windows: recv on a socket
ssize_t write(int fd, const void* buf, std::size_t count); // Windows: send on a socket
int pipe(int fds[2]); // Windows: binary mode
int dup(int fd);
int dup2(int fd, int fd2);
int isatty(int fd); // Windows: console
int poll(struct pollfd* fds, nfds_t nfds, int timeout); // Windows: sockets (WSAPoll), console / pipe / file input
pid_t getpid(void);
ssize_t readlink(const char* path, char* buf, std::size_t size); // Windows: "/proc/self/exe" = GetModuleFileName, else a symbolic link
const char* strerror(int errnum); // Windows: also the socket errors (WSA) translated in errno
```
