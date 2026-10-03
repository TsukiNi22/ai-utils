{{HEADER}}

#include "{{INCLUDE}}"
#include <gtest/gtest.h>
#include <unistd.h>
#include <cstdlib>
#include <ostream>
#include <string>

/* ------------------------------- {{CLASS}} ------------------------------- */
TEST({{CLASS}}, DefaultState) {
    {{NAMESPACE}}::{{CLASS}} object;
    EXPECT_EQ(object.size(), 0u);
}

TEST({{CLASS}}, Nominal) {
    {{NAMESPACE}}::{{CLASS}} object;
    ASSERT_NO_THROW(object.add("value"));
    EXPECT_EQ(object.size(), 1u);
}

TEST({{CLASS}}, UnknownIdThrows) {
    {{NAMESPACE}}::{{CLASS}} object;
    try {
        object.remove(42);
        FAIL() << "Expected an exception";
    } catch (const utils::exception::IException& e) {
        EXPECT_EQ(e.getCode(), utils::exception::InternalCode::UnknownId);
    }
}

/* ------------------------------- Fixture --------------------------------- */
class {{CLASS}}Test : public ::testing::Test {
    protected:
        {{NAMESPACE}}::{{CLASS}} _object;

        void SetUp(void) override {this->_object.add("first");}; // shared starting state
        void TearDown(void) override {}; // reset any global state touched by the tests
};

TEST_F({{CLASS}}Test, KeepsExistingValues) {
    this->_object.add("second");
    EXPECT_EQ(this->_object.size(), 2u);
}

/* ------------------------------ Parametrized ----------------------------- */
struct {{CLASS}}Case {
    std::string input;
    bool valid;
};
std::ostream& operator<<(std::ostream& os, const {{CLASS}}Case& c) {return os << '"' << c.input << '"';}

class {{CLASS}}Validation : public ::testing::TestWithParam<{{CLASS}}Case> {};

TEST_P({{CLASS}}Validation, Input) {
    {{NAMESPACE}}::{{CLASS}} object;
    EXPECT_EQ(object.check(GetParam().input), GetParam().valid);
}

INSTANTIATE_TEST_SUITE_P(Cases, {{CLASS}}Validation,
    ::testing::Values(
        {{CLASS}}Case{"value", true},
        {{CLASS}}Case{"", false}
    )
);

/* ----------------------------- Isolated cases ---------------------------- */
// Run the scenario in a sub-process with a timeout: a hang (or crash) is reported as a failure
#define ISOLATED(...) EXPECT_EXIT({::alarm(10); __VA_ARGS__; std::exit(::testing::Test::HasFailure() ? 1 : 0);}, ::testing::ExitedWithCode(0), "")

static void DestructionDuringWorkScenario(void)
{
    {{NAMESPACE}}::{{CLASS}} object;
    object.start();
    EXPECT_TRUE(object.running());
}
TEST({{CLASS}}, DestructionDuringWork) {ISOLATED(DestructionDuringWorkScenario());}

/* ------------------------------ Known bugs ------------------------------- */
// Expected behavior: fails until the bug is fixed in the sources (never changed to make the test pass)
TEST({{CLASS}}, ClearKeepsCapacity) {
    {{NAMESPACE}}::{{CLASS}} object;
    object.add("value");
    object.clear();
    EXPECT_EQ(object.size(), 0u);
}
