/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗ 
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 02/10/2026 by @author Tsukini

File Name:
##  @file ASocket.cpp

File Description:
##  Definition of basic method for any socket
\**************************************************************/

#include "utils/attribute/Attribute.hpp"
#include "utils/exception/ExceptionDefine.hpp"
#include "utils/exception/basic/ErrorException.hpp"
#include "utils/exception/basic/NoneException.hpp"
#include "utils/network/socket/ASocket.hpp"
#include "utils/verbose/Verbose.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <cerrno>
#include <cstddef>
#include <vector>
#include <string>

_cold _nodiscard bool utils::network::is_ip(const std::string& s)
{
    struct in_addr buf{};
    return ::inet_pton(AF_INET, s.c_str(), &buf) == 1;
    //static const std::regex regex(IP_REGEX);
    //return std::regex_match(s, regex);
}

_cold _nodiscard std::string utils::network::resolve_hostname(const std::string& hostname)
{
    struct addrinfo* res = nullptr;
    struct addrinfo settings{};
    char ip[INET_ADDRSTRLEN] = {'\0'};
    int status = 0;

    // Set the parameter
    settings.ai_family = AF_INET;
    settings.ai_socktype = SOCK_STREAM;

    // Get the information
    onAdvancedVerbose("Get the correponding ipv4 from the hostname '" << hostname << "'...");
    if ((status = ::getaddrinfo(hostname.c_str(), nullptr, &settings, &res)) != 0)
        throw utils::exception::ErrorException(utils::exception::InternalCode::SocketInit, gai_strerror(status));

    // Convert the result in an ip
    onAdvancedVerbose("Convert the result into a valid ipv4...");
    if (!::inet_ntop(AF_INET, &((struct sockaddr_in*) res->ai_addr)->sin_addr, ip, sizeof(ip))) {
        ::freeaddrinfo(res);
        throw utils::exception::ErrorException(utils::exception::InternalCode::SocketInit, strerror(errno));
    }

    // Clean the memory
    ::freeaddrinfo(res);

    onAdvancedVerbose("Hostname resolved! ('" << hostname << "' -> '" << ip << "')");
    return std::string(ip);
}

_cold void utils::network::resolve_address(utils::network::Address& address)
{
    if (address.ip.first.empty() || !utils::network::is_ip(address.ip.first)) {
        onBasicVerbose("Resolving the hostname...");
        if (address.ip.second.empty()) address.ip.second = address.ip.first; // Store the hostname
        else address.ip.first = address.ip.second;
        address.ip.first = utils::network::resolve_hostname(address.ip.first);
    }
}

_cold void utils::network::ASocket::close(void) noexcept
{
    if (this->_fd == -1) return; // Ignore invalid socket

    // Terminate the socket
    onAdvancedVerbose("Close the socket...");
    ::close(this->_fd);
    this->_fd = -1;

    // Forget the buffers (a new connection could reuse the same fd number)
    this->_buffersRecv.clear();
    this->_buffersSend.clear();
    onBasicVerbose((this->_mode ? "Server closed!" : "Client closed!"));
}

_cold void utils::network::ASocket::reset(void)
{
    this->_fd = -1;
    this->_buffersSend.clear();
    this->_buffersRecv.clear();
}

_hot _nodiscard bool utils::network::ASocket::empty(int fd) const
{
    // Automatic redirection on self
    if (fd == -1) {
        if ((fd = this->_fd) == -1) _unlikely { // Check if the redirect fd is valid
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidFd, std::to_string(fd));
        }
    }

    // Try to know if there is still a valid payload in the fd's buffer
    auto it = this->_buffersRecv.find(fd);
    if (it == this->_buffersRecv.end()) return true;
    if (this->_separator.empty()) return it->second.empty(); // raw mode: any data is a payload
    return it->second.find(this->_separator) == std::string::npos;
}

_cold void utils::network::ASocket::discard(int fd)
{
    // Every fd
    if (fd == -1) {
        this->_buffersRecv.clear();
        this->_buffersSend.clear();
        return;
    }

    this->_buffersRecv.erase(fd);
    this->_buffersSend.erase(fd);
}

_hot _nodiscard std::size_t utils::network::ASocket::receive(int fd)
{
    // Automatic redirection on self
    if (fd == -1) {
        if ((fd = this->_fd) == -1) _unlikely { // Check if the redirect fd is valid
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidFd);
        }
    }

    // Read once (the fd is readable: no wait)
    std::vector<char> buffer(this->_chunk);
    ssize_t bytes = this->recv(fd, buffer.data(), buffer.size());
    if (bytes < 0) _unlikely {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
        throw utils::exception::ErrorException(utils::exception::InternalCode::Socket, strerror(errno));
    } else if (bytes == 0) _unlikely {
        throw utils::exception::NoneException(utils::exception::InternalCode::SocketClosed);
    }

    // Store to the internal buffer & detect 'overflow' (0 = unlimited)
    std::string& storage = this->_buffersRecv[fd];
    storage.append(buffer.begin(), buffer.begin() + bytes);
    std::size_t pos = this->_separator.empty() ? storage.size() : storage.find(this->_separator);
    std::size_t size = (pos == std::string::npos) ? storage.size() : pos;
    if (this->_overflow != 0 && size > this->_overflow) _unlikely {
        throw utils::exception::ErrorException(utils::exception::InternalCode::Socket, "Overflow detected, maximum char accepted by payload is '" + std::to_string(this->_overflow) + "', but got: " + std::to_string(size));
    }
    return static_cast<std::size_t>(bytes);
}

_hot _nodiscard int utils::network::ASocket::accept(void)
{
    // Check if the fd & mode is valid
    if (this->_fd == -1) _unlikely {
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidFd);
    } else if (!this->_mode) _unlikely {
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction);
    }

    sockaddr_storage storage{};
    socklen_t len = sizeof(storage);
    int fd = -1;

    // Accept the client
    onAdvancedVerbose("Accepting the new connection...");
    if ((fd = this->accept(this->_fd, (sockaddr *)&(storage), &len)) < 0)
        throw utils::exception::ErrorException(utils::exception::InternalCode::Accept, strerror(errno));
    const sockaddr_in& in = reinterpret_cast<const sockaddr_in&>(storage);
    onBasicVerbose("New client '" << ::inet_ntoa(in.sin_addr) << ":" << ::ntohs(in.sin_port) << "'");

    return fd;
}

_hot _nodiscard std::string utils::network::ASocket::recv(int fd)
{
    // Automatic redirection on self
    if (fd == -1) {
        if ((fd = this->_fd) == -1) _unlikely { // Check if the redirect fd is valid
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidFd);
        }
    }

    std::vector<char> buffer(this->_chunk);
    std::string& storage = this->_buffersRecv[fd];

    // Read the socket while there is no '\n' encountered (raw mode: while there is nothing)
    std::size_t pos = (this->_separator.empty() ? (storage.empty() ? std::string::npos : storage.size()) : storage.find(this->_separator));
    while (pos == std::string::npos) {

        // Read the socket
        ssize_t bytes = this->recv(fd, buffer.data(), buffer.size());
        if (bytes < 0) _unlikely {
            throw utils::exception::ErrorException(utils::exception::InternalCode::Socket, strerror(errno));
        } else if (bytes == 0) _unlikely {
            throw utils::exception::NoneException(utils::exception::InternalCode::SocketClosed);
        }

        // Store to the internal buffer
        storage.append(buffer.begin(), buffer.begin() + bytes);

        // Search for any '\n'
        pos = (this->_separator.empty() ? storage.size() : storage.find(this->_separator));

        // Detect 'overflow' (0 = unlimited)
        std::size_t size = (pos == std::string::npos) ? storage.size() : pos;
        if (this->_overflow != 0 && size > this->_overflow) _unlikely {
            throw utils::exception::ErrorException(utils::exception::InternalCode::Socket, "Overflow detected, maximum char accepted by payload is '" + std::to_string(this->_overflow) + "', but got: " + std::to_string(size));
        }
    }

    // Extract the first data section until '\n'
    std::string line = storage.substr(0, pos); // exclude '\n'
    storage.erase(0, pos + this->_separator.size());

    return line;
}

_hot _nodiscard std::vector<std::string> utils::network::ASocket::recvAll(int fd)
{
    // Automatic redirection on self
    if (fd == -1) {
        if ((fd = this->_fd) == -1) _unlikely { // Check if the redirect fd is valid
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidFd);
        }
    }

    // While there is data to read
    std::vector<std::string> lines;
    while (!this->empty(fd)) lines.push_back(this->recv(fd));
    return lines;
}

_hot void utils::network::ASocket::flush(int fd)
{
    // Automatic redirection on self
    if (fd == -1) {
        if ((fd = this->_fd) == -1) _unlikely { // Check if the redirect fd is valid
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidFd);
        }
    }

    // Get the buffer
    std::string& buffer = this->_buffersSend[fd];

    // While the data wasn't fully sended
    ssize_t total = 0;
    ssize_t size = buffer.size();
    while (total < size) {
        ssize_t sent = this->send(fd, buffer.data() + total, size - total);
        if (sent < 0) _unlikely {
            throw utils::exception::ErrorException(utils::exception::InternalCode::Socket, strerror(errno));
        } else if (sent == 0) _unlikely {
            throw utils::exception::NoneException(utils::exception::InternalCode::SocketClosed);
        }
        total += sent;
    }

    // Clear the internal buffer after sending it's content
    buffer.clear();
}

_hot void utils::network::ASocket::buffered(const std::string& s, int fd)
{
    // Automatic redirection on self
    if (fd == -1) {
        if ((fd = this->_fd) == -1) _unlikely { // Check if the redirect fd is valid
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidFd);
        }
    }

    // Check overflow (0 = unlimited)
    if (this->_overflow != 0 && s.size() > this->_overflow) _unlikely {
        throw utils::exception::ErrorException(utils::exception::InternalCode::Socket, "Overflow detected, maximum char accepted by payload is '" + std::to_string(this->_overflow) + "', but got: " + std::to_string(s.size()));
    }

    // Ensure separator
    std::string sub = s;
    if (sub.empty() || !sub.ends_with(this->_separator)) sub += this->_separator;

    // Store the new payload
    this->_buffersSend[fd] += sub;
}
