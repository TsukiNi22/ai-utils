/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file main.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/Core.hpp"
#include <exception>
#include <iostream>

_cold int main(int argc, char* argv[])
{
    // Init core class ...
    xstyle::Core core;

    try {
        // Call to the main endpoint
        core.init(argc, argv);
        core.run();
    } catch (const utils::exception::IException& e) { // Custom error
        if (e.isNone() && e.getCode() == utils::exception::InternalCode::Exit) return OK; // Exit - no error
        std::cerr << e.formated() << std::endl;
        return KO;
    } catch (const std::exception& e) { // Standard error
        std::cerr << e.what() << std::endl;
        return KO;
    }

    return core.exit();
}
