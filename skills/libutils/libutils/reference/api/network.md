# libutils `network`

Generated from libutils `v5.0.0` (commit `6aac89f`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/network/CApi.h`

C API of utils::network: sockets (TCP or of the C side), Client & Server (C & C++ compatible) Every function that can fail returns UTILS_OK / UTILS_KO (or NULL / 0 / -1), details in utils_exception Every callback returns what is asked or -1 / UTILS_KO (info: utils_fail / utils_exception_set)

```c
#define C_API_NETWORK_DEFAULT_IP "localhost"
#define C_API_NETWORK_DEFAULT_PORT 8080
#define C_API_NETWORK_SELF -1 // fd of the socket itself (the default fd of the c++)

// global
typedef struct utils_socket utils_socket_t; // std::shared_ptr<utils::network::ISocket> (TCPSocket or a socket of the C side)
typedef struct utils_client utils_client_t; // utils::network::Client
typedef struct utils_server utils_server_t; // utils::network::Server
typedef struct utils_payloads utils_payloads_t; // utils::network::Payloads (list of payloads)
typedef struct utils_network_messages utils_network_messages_t; // payloads of each client of a server (fd -> payloads)
typedef enum utils_network_status { C_API_NETWORK_STATUS_UP = 0, C_API_NETWORK_STATUS_DOWN, C_API_NETWORK_STATUS_TERMINATED, C_API_NETWORK_STATUS_CRASHED };
typedef struct utils_network_address { const char* ip; const char* hostname; uint16_t port; };
typedef struct utils_socket_callbacks { int (*connect)(const utils_network_address_t* address, void* data); int (*listen)(const utils_network_address_t* address, void* data); int (*accept)(int fd, struct sockaddr* addr, socklen_t* len, void* data); ssize_t (*recv)(int fd, char* buf, size_t len, void* data); ssize_t (*send)(int fd, const char* buf, size_t len, void* data); };
bool utils_network_is_ip(const char* str);
const char* utils_network_resolve_hostname(const char* hostname); // ipv4 of the hostname, valid until the next call in the thread (NULL on error)
size_t utils_network_resolve_hostname_r(const char* hostname, char* buf, size_t size); // copied in buf (truncated, always '\0' terminated), returns the full length like snprintf (0 on error)
utils_payloads_t* utils_payloads_create(void); // NULL on error
void utils_payloads_destroy(utils_payloads_t* payloads); // NULL: nothing
size_t utils_payloads_count(const utils_payloads_t* payloads);
const char* utils_payloads_get(const utils_payloads_t* payloads, size_t i, size_t* size); // valid until payloads is changed, size: optional (NULL on error)
utils_network_messages_t* utils_network_messages_create(void); // NULL on error
void utils_network_messages_destroy(utils_network_messages_t* messages); // NULL: nothing
size_t utils_network_messages_count(const utils_network_messages_t* messages); // number of fds
int utils_network_messages_fd(const utils_network_messages_t* messages, size_t i); // -1 on error
size_t utils_network_messages_size(const utils_network_messages_t* messages, size_t i); // number of payloads of the fd i
const char* utils_network_messages_get(const utils_network_messages_t* messages, size_t i, size_t j, size_t* size); // valid until messages is changed, size: optional (NULL on error)
utils_socket_t* utils_socket_create_tcp(void); // utils::network::TCPSocket, NULL on error
utils_socket_t* utils_socket_create(const utils_socket_callbacks_t* callbacks, void* data); // socket of the C side (connect & listen mandatory), NULL on error
void utils_socket_destroy(utils_socket_t* socket); // NULL: nothing (closed when the last client / server using it is destroyed)
int utils_socket_set_payload_separator(utils_socket_t* socket, const char* separator); // default: "\n"
int utils_socket_set_chunk_size(utils_socket_t* socket, size_t size); // default: 4096
int utils_socket_set_overflow(utils_socket_t* socket, size_t overflow); // size without a valid payload before an error, default: 4096 (0 = unlimited)
int utils_socket_get_fd(const utils_socket_t* socket); // -1: closed or error
bool utils_socket_has_accept_overload(const utils_socket_t* socket);
bool utils_socket_has_recv_overload(const utils_socket_t* socket);
bool utils_socket_has_send_overload(const utils_socket_t* socket);
int utils_socket_connect(utils_socket_t* socket, const utils_network_address_t* address); // once (close to reuse it)
int utils_socket_listen(utils_socket_t* socket, const utils_network_address_t* address); // once (close to reuse it)
int utils_socket_reset(utils_socket_t* socket); // reset the fd & the buffers (the fd is NOT closed)
int utils_socket_close(utils_socket_t* socket);
int utils_socket_accept(utils_socket_t* socket); // server mode only: the fd of the new connection, -1 on error
int utils_socket_accept_raw(const utils_socket_t* socket, int fd, struct sockaddr* addr, socklen_t* len); // -1 on error
ssize_t utils_socket_recv_raw(const utils_socket_t* socket, int fd, char* buf, size_t len); // -1 on error
ssize_t utils_socket_send_raw(const utils_socket_t* socket, int fd, const char* buf, size_t len); // -1 on error
bool utils_socket_empty(const utils_socket_t* socket, int fd); // no valid payload in the buffer
int utils_socket_recv(utils_socket_t* socket, int fd, utils_string_t* payload); // the next payload (payload replaced)
int utils_socket_recv_all(utils_socket_t* socket, int fd, utils_payloads_t* payloads); // every valid payload (payloads replaced)
int utils_socket_flush(utils_socket_t* socket, int fd); // send the buffer
size_t utils_socket_receive(utils_socket_t* socket, int fd); // read once (after a poll event) in the buffer, never wait for a full payload (0: nothing or error)
int utils_socket_discard(utils_socket_t* socket, int fd); // forget the buffers of a fd, -1: every fd
int utils_socket_send(utils_socket_t* socket, const char* data, size_t size, int fd); // send it now
int utils_socket_send_buffered(utils_socket_t* socket, const char* data, size_t size, int fd); // store it in the buffer
utils_client_t* utils_client_create(const utils_socket_t* socket, const utils_network_address_t* address); // address: NULL = the default one, NULL on error
void utils_client_destroy(utils_client_t* client); // NULL: nothing (killed)
int utils_client_start(utils_client_t* client); // start / restart
int utils_client_stop(utils_client_t* client); // can be restarted (thread safe)
int utils_client_kill(utils_client_t* client); // can't be restarted (thread safe)
int utils_client_listen(utils_client_t* client, utils_payloads_t* payloads); // the payloads received (payloads replaced)
int utils_client_join(utils_client_t* client); // wait until the next listen event
int utils_client_flush(utils_client_t* client); // send the buffered payloads
int utils_client_send(utils_client_t* client, const char* payload, size_t size); // send it now
int utils_client_send_buffered(utils_client_t* client, const char* payload, size_t size); // sent at the next send / flush
utils_network_status_t utils_client_get_status(const utils_client_t* client); // C_API_NETWORK_STATUS_CRASHED on error
utils_server_t* utils_server_create(const utils_socket_t* socket, const utils_network_address_t* address); // address: NULL = the default one, NULL on error
void utils_server_destroy(utils_server_t* server); // NULL: nothing (killed)
int utils_server_start(utils_server_t* server); // start / restart
int utils_server_stop(utils_server_t* server); // can be restarted (thread safe)
int utils_server_kill(utils_server_t* server); // can't be restarted (thread safe)
int utils_server_join(utils_server_t* server, int fd); // wait until the next listen event on this fd (-1: any)
int utils_server_flush(utils_server_t* server); // send the buffered payloads of every client
int utils_server_flush_fd(utils_server_t* server, int fd); // of a client
int utils_server_send(utils_server_t* server, int fd, const char* payload, size_t size); // send it now
int utils_server_send_buffered(utils_server_t* server, int fd, const char* payload, size_t size); // sent at the next send / flush
int utils_server_listen(utils_server_t* server, utils_network_messages_t* messages); // the payloads of every client (messages replaced)
int utils_server_listen_fd(utils_server_t* server, int fd, utils_payloads_t* payloads); // the payloads of a client (payloads replaced, empty if it was removed)
size_t utils_server_get_fds(const utils_server_t* server, int* fds, size_t max); // fds of the clients copied (max at most), returns their number (0 on error)
utils_network_status_t utils_server_get_status(const utils_server_t* server); // C_API_NETWORK_STATUS_CRASHED on error
```

## `utils/network/CApi.hpp`

Handles of the C API of utils::network (usable in c++ to get the object)

```cpp
// global
struct utils_socket: public utils_handle {
    std::shared_ptr<utils::network::ISocket> socket;
    utils_socket(std::shared_ptr<utils::network::ISocket> s);
};
struct utils_client: public utils_handle, public utils::network::Client {
    utils_client(const std::shared_ptr<utils::network::ISocket>& socket, const utils::network::Address& address);
};
struct utils_server: public utils_handle, public utils::network::Server {
    utils_server(const std::shared_ptr<utils::network::ISocket>& socket, const utils::network::Address& address);
};
struct utils_payloads: public utils_handle {
    utils::network::Payloads payloads;
};
struct utils_network_messages: public utils_handle {
    std::vector<std::pair<int, utils::network::Payloads>> messages;
};
```

## `utils/network/Client.hpp`

Definition of the client class for custom network

Namespace: `utils::network`

```cpp
// namespace utils::network
class Client: private utils::security::observer::Observer<"Client"> {
    void start(void); // start/restart the client
    void stop(void); // stop the client (can be restarted, same has error)
    void kill(void); // terminate the client (can't be restarted)
    const utils::network::Payloads& listen(void);
    void join(void); // Await until the next listen event
    void flush(void); // send all the stack
    template<bool buffered = false> void send(const utils::network::Payload& payload);
    utils::network::Status getStatus(void) const;
    Client() = default;
    Client(const std::shared_ptr<utils::network::ISocket>& socket, const utils::network::Address& address = {});
    ~Client();
};
```

## `utils/network/NetworkDefine.hpp`

Different definition of values for socket definition

Namespace: `utils::network`

```cpp
#define OVERFLOW_LIMIT 4048 // Number of char before detecting an 'overflow'
#define IP_REGEX R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.)){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
#define DEFAULT_IP "localhost"
#define DEFAULT_PORT 8080
#define SOCKET_TIMEOUT 7 // Timeout in seconds
#define SOCKET_CHUNK_SIZE 4096 // Size of the chunck readed at recv call

// namespace utils::network
enum class Status {Up, Down, Terminated, Crashed}
```

## `utils/network/NetworkType.hpp`

Namespace: `utils::network`

```cpp
// namespace utils::network
using Ip = std::pair<std::string, std::string>; // <ipv4 (hostname by default), hostname>
using Payload = std::string; // Not parsed (raw from the socket)
using Payloads = std::vector<utils::network::Payload>;
struct Address {
    utils::network::Ip ip = {DEFAULT_IP, ""}; // Ignored on server side
    std::uint16_t port = DEFAULT_PORT;
};
```

## `utils/network/Server.hpp`

Definition of the server class for custom network

Namespace: `utils::network`

```cpp
// namespace utils::network
class Server: private utils::security::observer::Observer<"Server"> {
    void start(void); // start/restart the server
    void stop(void); // stop the server (can be restarted, same has error)
    void kill(void); // terminate the server (can't be restarted)
    void join(const int fd = -1); // Await until the next listen event on this precise fd or every one (-1)
    void flush(void); // send all the stack
    void flush(const int fd); // send all the stack of the specified client
    template<bool buffered = false> void send(const int fd, const utils::network::Payload& payload);
    const utils::network::Payloads& listen(const int fd);
    std::vector<int> getFds(void) const;
    const std::unordered_map<int, utils::network::Payloads>& listen(void);
    utils::network::Status getStatus(void) const;
    Server() = default;
    Server(const std::shared_ptr<utils::network::ISocket>& socket, const utils::network::Address& address = {});
    ~Server();
};
```

## `utils/network/socket/ASocket.hpp`

Abstract for socket handling

Namespace: `utils::network`

```cpp
// namespace utils::network
bool is_ip(const std::string& s);
std::string resolve_hostname(const std::string& hostname);
void resolve_address(utils::network::Address& address);
class ASocket: public utils::network::ISocket {
    int accept(void); // accept a new connection (only server mode), fd is used as an id in server
    void close(void) noexcept; // reallow the use of connect/listen
    void reset(void); // reset fd and buffers (DOES NOT CLOSE FD!!!)
    bool empty(int fd = -1) const; // is there no valid payload in the stored buffer ?
    std::string recv(int fd = -1); // (default) read by chunck of 4096, store the payload overflow into a buffer
    std::vector<std::string> recvAll(int fd = -1); // read by chunck of 4096, return all valid payloads, store the overflow into a buffer
    void flush(int fd = -1); // send the internal buffer
    std::size_t receive(int fd = -1); // read once (after a poll event) into the internal buffer, never wait for a full payload
    void discard(int fd = -1); // forget the internal buffers of a fd (closed connection), -1 = every fd
    void send(const std::string& s, int fd = -1); // (default) send it now
    void sendBuffered(const std::string& s, int fd = -1); // store in a buffer
    void setPayloadSeparator(char c = '\n'); // default: '\n'
    void setPayloadSeparator(std::string s = "\n");
    void setChunckSize(std::size_t size = SOCKET_CHUNK_SIZE); // default: 4096
    void setOverflow(std::size_t overflow = OVERFLOW_LIMIT); // size without a valid payload before throw, default: 4096 (0 = unlimited)
    int getFd(void) const; // fd of the socket
    bool hasAcceptOverload(void) const;
    bool hasRecvOverload(void) const;
    bool hasSendOverload(void) const;
    int accept(int fd, sockaddr* addr, socklen_t* len) const;
    ssize_t recv(int fd, char* buf, std::size_t len) const;
    ssize_t send(int fd, const char* buf, std::size_t len) const;
    ASocket() = default;
    ~ASocket() noexcept;
};
```

## `utils/network/socket/ISocket.hpp`

Interface for socket handling

Namespace: `utils::network`

```cpp
// namespace utils::network
class ISocket: private utils::security::observer::Observer<"ISocket"> {
    virtual void setPayloadSeparator(char c) = 0; // default: '\n'
    virtual void setPayloadSeparator(std::string s) = 0;
    virtual void setChunckSize(std::size_t size) = 0; // default: 4096
    virtual void setOverflow(std::size_t overflow) = 0; // size without a valid payload before throw, default: 4096 (0 = unlimited)
    virtual int getFd(void) const = 0; // fd of the socket (-1 == closed)
    virtual bool hasAcceptOverload(void) const = 0;
    virtual bool hasRecvOverload(void) const = 0;
    virtual bool hasSendOverload(void) const = 0;
    virtual void connect(const utils::network::Address& address) = 0; // build a connection as a client
    virtual void listen(const utils::network::Address& address) = 0; // build a connection as a server
    virtual void reset(void) = 0; // reset fd (DOES NOT CLOSE!!!)
    virtual void close(void) noexcept = 0; // reallow the use of connect/listen
    virtual int accept(void) = 0; // accept a new connection (only server mode), fd is used as an id in server
    virtual int accept(int fd, sockaddr* addr, socklen_t* len) const = 0;
    virtual ssize_t recv(int fd, char* buf, std::size_t len) const = 0;
    virtual ssize_t send(int fd, const char* buf, std::size_t len) const = 0;
    virtual bool empty(int fd = -1) const = 0; // is there no valid payload in the stored buffer ?
    virtual std::string recv(int fd = -1) = 0; // (default) read by chunck of 4096, store the payload overflow into a buffer
    virtual std::vector<std::string> recvAll(int fd = -1) = 0; // read by chunck of 4096, return all valid payloads from the buffer, store the overflow into a buffer
    virtual void flush(int fd = -1) = 0; // send the internal buffer
    virtual std::size_t receive(int fd = -1) = 0; // read once (after a poll event) into the internal buffer, never wait for a full payload
    virtual void discard(int fd = -1) = 0; // forget the internal buffers of a fd (closed connection), -1 = every fd
    virtual void send(const std::string& s, int fd = -1) = 0; // (default) send it now
    virtual void sendBuffered(const std::string& s, int fd = -1) = 0; // store in a buffer
    ISocket() = default;
    virtual ~ISocket() = default;
};
```

## `utils/network/socket/Socket.hpp`

Include for all the different sockets

## `utils/network/socket/TCPSocket.hpp`

Socket that handle tcp communication

Namespace: `utils::network`

```cpp
// namespace utils::network
class TCPSocket: public utils::network::ASocket {
    void connect(const utils::network::Address& address); // build a connection as a client
    void listen(const utils::network::Address& address); // build a connection as a server
    bool hasAcceptOverload(void) const;
    bool hasRecvOverload(void) const;
    bool hasSendOverload(void) const;
    using utils::network::ASocket::accept;
    using utils::network::ASocket::recv;
    using utils::network::ASocket::send;
    int accept(int fd, sockaddr* addr, socklen_t* len) const;
    ssize_t recv(int fd, char* buf, std::size_t len) const;
    ssize_t send(int fd, const char* buf, std::size_t len) const;
    TCPSocket() = default;
    ~TCPSocket() = default;
};
```
