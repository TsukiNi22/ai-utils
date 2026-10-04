# libutils `network`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

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

Namespace: `utils::network`, `utils::network::socket`

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
// namespace utils::network::socket
[deprecated ~v4.0.0] bool is_ip(const std::string& s);
[deprecated ~v4.0.0] std::string resolve_hostname(const std::string& hostname);
[deprecated ~v4.0.0] void resolve_address(utils::network::Address& address);
using ASocket [deprecated ~v4.0.0] = utils::network::ASocket;
```

## `utils/network/socket/ISocket.hpp`

Interface for socket handling

Namespace: `utils::network`, `utils::network::socket`

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
// namespace utils::network::socket
using ISocket [deprecated ~v4.0.0] = utils::network::ISocket;
```

## `utils/network/socket/Socket.hpp`

Include for all the different sockets

## `utils/network/socket/TCPSocket.hpp`

Socket that handle tcp communication

Namespace: `utils::network`, `utils::network::socket`

```cpp
// namespace utils::network
class TCPSocket: public utils::network::ASocket {
    void connect(const utils::network::Address& address); // build a connection as a client
    void listen(const utils::network::Address& address); // build a connection as a server
    bool hasAcceptOverload(void) const;
    bool hasRecvOverload(void) const;
    bool hasSendOverload(void) const;
    int accept(int fd, sockaddr* addr, socklen_t* len) const;
    ssize_t recv(int fd, char* buf, std::size_t len) const;
    ssize_t send(int fd, const char* buf, std::size_t len) const;
    TCPSocket() = default;
    ~TCPSocket() = default;
};
// namespace utils::network::socket
using TCPSocket [deprecated ~v4.0.0] = utils::network::TCPSocket;
```
