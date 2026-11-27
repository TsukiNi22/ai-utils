# libutils `platform`

Generated from libutils `v5.0.0` (commit `c129a5e`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/platform/CApi.h`

C API of utils::platform: the POSIX calls of libutils for Unix & Windows (C & C++ compatible) Same contract as POSIX (-1 & errno, NULL & dlerror, EAI_* code), utils_exception is never used Unix: the POSIX structures need the POSIX features (_POSIX_C_SOURCE >= 200112L, default of gnu11 / gnu23)

```c
// global
typedef struct utils_platform_dl_info { const char* dli_fname; void* dli_fbase; const char* dli_sname; void* dli_saddr; };
int utils_platform_close(int fd); // any fd (Windows: socket, CRT fd or emulated epoll)
ssize_t utils_platform_read(int fd, void* buf, size_t count); // Windows: recv on a socket
ssize_t utils_platform_write(int fd, const void* buf, size_t count); // Windows: send on a socket
int utils_platform_pipe(int fds[2]); // Windows: binary mode
int utils_platform_dup(int fd);
int utils_platform_dup2(int fd, int fd2);
int utils_platform_isatty(int fd); // Windows: console
int utils_platform_poll(struct pollfd* fds, nfds_t nfds, int timeout); // Windows: sockets (WSAPoll), console / pipe / file input
pid_t utils_platform_getpid(void);
ssize_t utils_platform_readlink(const char* path, char* buf, size_t size); // Windows: "/proc/self/exe" = GetModuleFileName, else a symbolic link
const char* utils_platform_strerror(int errnum); // Windows: also the socket errors (WSA) translated in errno
void* utils_platform_dlopen(const char* path, int flags); // NULL on error (utils_platform_dlerror)
void* utils_platform_dlsym(void* handle, const char* name); // NULL on error (utils_platform_dlerror)
int utils_platform_dlclose(void* handle); // 0 on success
char* utils_platform_dlerror(void); // last error of the thread, NULL if none (cleared by the call)
int utils_platform_dladdr(const void* addr, utils_platform_dl_info_t* info); // 0 on error
int utils_platform_epoll_create1(int flags);
int utils_platform_epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
int utils_platform_epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);
int utils_platform_socket(int domain, int type, int protocol);
int utils_platform_connect(int fd, const struct sockaddr* addr, socklen_t len);
int utils_platform_bind(int fd, const struct sockaddr* addr, socklen_t len);
int utils_platform_listen(int fd, int backlog);
int utils_platform_accept(int fd, struct sockaddr* addr, socklen_t* len);
ssize_t utils_platform_recv(int fd, void* buf, size_t len, int flags);
ssize_t utils_platform_send(int fd, const void* buf, size_t len, int flags);
int utils_platform_setsockopt(int fd, int level, int name, const void* value, socklen_t len); // Windows: SO_SNDTIMEO / SO_RCVTIMEO take a timeval, SO_REUSEADDR is the default (no-op)
int utils_platform_getsockname(int fd, struct sockaddr* addr, socklen_t* len);
int utils_platform_getpeername(int fd, struct sockaddr* addr, socklen_t* len);
int utils_platform_getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res); // EAI_* error code (utils_platform_gai_strerror)
void utils_platform_freeaddrinfo(struct addrinfo* res);
const char* utils_platform_gai_strerror(int code);
int utils_platform_tcgetattr(int fd, struct termios* term); // Windows: ENOTTY when the fd is not a console
int utils_platform_tcsetattr(int fd, int action, const struct termios* term); // Windows: without ICANON the input gives the escape sequences (VT input)
```

## `utils/platform/Dlfcn.hpp`

Dynamic loading calls of <dlfcn.h> for Unix & Windows (same names, same contract) Unix: the system call, Windows: LoadLibrary / GetProcAddress / FreeLibrary

Namespace: `utils::platform`

```cpp
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
// namespace utils::platform
int tcgetattr(int fd, struct termios* term); // Windows: ENOTTY when the fd is not a console
int tcsetattr(int fd, int action, const struct termios* term); // Windows: without ICANON the input gives the escape sequences (VT input)
```

## `utils/platform/Types.h`

Windows versions of the POSIX defines, structures & types used by utils::platform (C & C++ compatible) Same names, values & layout as Linux; nothing on Unix (the system headers give them)

```c
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define RTLD_LAZY 0x00001
#define RTLD_NOW 0x00002
#define RTLD_GLOBAL 0x00100
#define RTLD_LOCAL 0
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
#define MSG_NOSIGNAL 0 // no SIGPIPE on Windows
#define NCCS 32
#define ISIG 0000001
#define ICANON 0000002
#define ECHO 0000010
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

// global
typedef unsigned long nfds_t;
typedef int socklen_t; // <ws2tcpip.h>
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef struct Dl_info { const char* dli_fname; void* dli_fbase; const char* dli_sname; void* dli_saddr; };
typedef union epoll_data { void* ptr; int fd; uint32_t u32; uint64_t u64; };
struct epoll_event {
    uint32_t events;
    union epoll_data data;
};
struct termios {
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
```

## `utils/platform/Unistd.hpp`

POSIX calls of <unistd.h> & co for Unix & Windows (same names, same contract: -1 & errno) Unix: the system call, Windows: CRT / Win32 implementation

Namespace: `utils::platform`

```cpp
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
