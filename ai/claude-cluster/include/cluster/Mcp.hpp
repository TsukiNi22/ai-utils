/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Mcp.hpp

File Description:
##  MCP server (stdio, JSON-RPC 2.0) started by the global session:
##  its tools are forwarded to the control socket of claude-cluster
\**************************************************************/

#ifndef CLUSTER_MCP_H
    #define CLUSTER_MCP_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Json

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard cluster::Json mcp_tools(void);                             // tools/list
_cold _nodiscard cluster::Json mcp_call(const std::string& name, const cluster::Json& arguments);   // tools/call -> control request
_cold int mcp_serve(void);                                                  // loop on stdin / stdout until the end of stdin

} // namespace end

#endif /* CLUSTER_MCP_H */
