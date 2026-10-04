{{HEADER}}

#include "{{HPP_INCLUDE_ROOT}}"
#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include <exception>
#include <iostream>

_cold int main(int argc, char* argv[])
{
    // Init core class ...
    {{NAMESPACE}}::{{CLASS}} core;

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
