{{HEADER}}

#ifndef A{{GUARD}}_H
    #define A{{GUARD}}_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include {{UTILS_INCLUDE}}         // _cold, _hot, _nodiscard
    #include "I{{NAME}}.hpp"                // {{NAMESPACE}}::I{{NAME}}
    #include <string>                       // std::string

namespace {{NAMESPACE}} { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class A{{NAME}}: public {{NAMESPACE}}::I{{NAME}} {
    protected:
        std::string _name = "[None]";

        // ---------- Pre-Function -------- //
        void internalTask_(void) final; // shared implementation (in the .cpp)

    public:
        // ------------ Function ---------- //
        /* setter */
        _cold void setName(const std::string& name) final {this->_name = name;};

        /* getter */
        _cold _nodiscard const std::string& getName(void) const final {return this->_name;};

        // ------------ Operator ---------- //
        A{{NAME}}& operator=(const A{{NAME}}& other) = delete;
        A{{NAME}}& operator=(A{{NAME}}&& other) = delete;

        // ---------- Constructor --------- //
        A{{NAME}}() = default;
        A{{NAME}}(const A{{NAME}}& other) = delete;
        A{{NAME}}(A{{NAME}}&& other) = delete;

        // ----------- Destructor --------- //
        ~A{{NAME}}() = default;
};

} // namespace end
#endif /* A{{GUARD}}_H */
