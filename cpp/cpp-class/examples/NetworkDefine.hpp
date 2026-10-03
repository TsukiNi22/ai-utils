/**************************************************************\
Edition:
##  @date 15/08/2026 by @author Tsukini

File Name:
##  @file Define.hpp

File Description:
##  Different definition of values for socket definition
\**************************************************************/

#ifndef DEFINE_H
    #define DEFINE_H

    //----------------------------------------------------------------//
    /* DEFINE */

    /* limits */
    #define OVERFLOW_LIMIT 4048 // Number of char before detecting an 'overflow'

    /* socket */
    #define IP_REGEX R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.)){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
    #define DEFAULT_IP "localhost"
    #define DEFAULT_PORT 8080
    #define SOCKET_TIMEOUT 7 // Timeout in seconds
    #define SOCKET_CHUNK_SIZE 4096 // Size of the chunck readed at recv call

namespace utils::network { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class Status {
    Up,
    Down,
    Terminated,
    Crashed,
};

} // namespace end
#endif /* DEFINE_H */
