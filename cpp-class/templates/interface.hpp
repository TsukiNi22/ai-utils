{{HEADER}}

#ifndef I{{GUARD}}_H
    #define I{{GUARD}}_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include <string>   // std::string

namespace {{NAMESPACE}} { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class I{{NAME}} {
    protected:
        // ---------- Pre-Function -------- //
        virtual void internal(void) = 0; // short description

    public:
        // ---------- Pre-Function -------- //
        /* setter */
        virtual void setName(const std::string& name) = 0;

        /* getter */
        virtual const std::string& getName(void) const = 0;

        virtual void run(void) = 0; // short description

        // ------------ Operator ---------- //
        I{{NAME}}& operator=(const I{{NAME}}& other) = delete;
        I{{NAME}}& operator=(I{{NAME}}&& other) = delete;

        // ---------- Constructor --------- //
        I{{NAME}}() = default;
        I{{NAME}}(const I{{NAME}}& other) = delete;
        I{{NAME}}(I{{NAME}}&& other) = delete;

        // ----------- Destructor --------- //
        virtual ~I{{NAME}}() = default;
};

} // namespace end
#endif /* I{{GUARD}}_H */
