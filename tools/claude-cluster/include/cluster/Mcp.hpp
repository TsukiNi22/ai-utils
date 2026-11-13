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
_cold _nodiscard cluster::Json mcp_sub_tools(const std::string& session);                  // tools/list of a sub-session (CLAUDE_CLUSTER_SESSION): its own tasks / schedules
_cold _nodiscard cluster::Json mcp_call(const std::string& name, const cluster::Json& arguments);   // tools/call -> control request
_cold int mcp_serve(void);
// the custom servers of [mcp] servers as entries of an mcpServers object ("name: command args" -> stdio, "name: https://..." -> http)
_cold _nodiscard cluster::Json mcp_servers(const std::vector<std::string>& servers);                                                  // loop on stdin / stdout until the end of stdin

} // namespace end

#endif /* CLUSTER_MCP_H */
