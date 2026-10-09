/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file main.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Core.hpp"
#include <algorithm>
#include <exception>
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <string>

_cold static bool rtk_mode(const int argc, char* argv[])
{
    // --rtk / CLAUDE_CLUSTER_RTK=1: errors on one plain line too (the parser may fail before the options exist)
    const char* env = std::getenv("CLAUDE_CLUSTER_RTK");
    return std::any_of(argv, argv + argc, [](const char* arg) {return std::string(arg) == "--rtk";}) || (env && std::string(env) != "0" && std::string(env) != "");
}

_cold int main(int argc, char* argv[])
{
    // Init core class ...
    cluster::Core core;
    std::signal(SIGPIPE, SIG_IGN); // a dead agent must not kill the cluster

    try {
        // Call to the main endpoint
        core.init(argc, argv);
        core.run();
    } catch (const utils::exception::IException& e) { // Custom error
        if (e.isNone() && e.getCode() == utils::exception::InternalCode::Exit) return OK; // Exit - no error
        if (rtk_mode(argc, argv)) std::cerr << "claude-cluster: error: " << e.what() << ": " << e.info() << std::endl; // one plain line
        else std::cerr << "claude-cluster: " << e.what() << ": " << e.info() << std::endl;
        return KO;
    } catch (const std::exception& e) { // Standard error
        std::cerr << "claude-cluster: " << e.what() << std::endl;
        return KO;
    }

    return core.exit();
}
