{{HEADER}}

#include "{{HPP_INCLUDE_ROOT}}"
#include <exception>
#include <iostream>
#include <cstdlib>

[[gnu::cold]] [[nodiscard]] int main(int argc, const char* argv[])
{
    // Init core class ...
    {{NAMESPACE}}::{{CLASS}} core;

    try {
        // Call to the main endpoint
        core.init(argc, argv);
        core.run();
    } catch (const std::exception& e) { // Any error
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return core.exit();
}
