{{HEADER}}

#include "{{HPP_INCLUDE_ROOT}}"
#include <gtest/gtest.h>

// The core class can be built and destroyed
TEST({{CLASS}}, Construct)
{
    EXPECT_NO_THROW({{NAMESPACE}}::{{CLASS}} core;);
}
