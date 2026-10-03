{{HEADER}}

#ifndef {{GUARD}}_H
    #define {{GUARD}}_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include "{{ATTRIBUTE_INCLUDE}}"   // _cold, _hot, _nodiscard
    #include <string>                       // std::string

namespace {{NAMESPACE}} { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class {{CLASS}} {
    private:
        std::string _name = "[None]"; // short description of the member

        // ---------- Pre-Function -------- //
        void internal(void); // implemented in the .cpp

    public:
        // ---------- Pre-Function -------- //
        void run(void); // implemented in the .cpp

        // ------------ Function ---------- //
        /* setter */
        _cold void setName(const std::string& name) {this->_name = name;};

        /* getter */
        _cold _nodiscard const std::string& getName(void) const {return this->_name;};

        // ------------ Operator ---------- //
        {{CLASS}}& operator=(const {{CLASS}}& other) = delete;
        {{CLASS}}& operator=({{CLASS}}&& other) = delete;

        // ---------- Constructor --------- //
        {{CLASS}}() = default;
        {{CLASS}}(const {{CLASS}}& other) = delete;
        {{CLASS}}({{CLASS}}&& other) = delete;

        // ----------- Destructor --------- //
        ~{{CLASS}}() = default;
};

} // namespace end
#endif /* {{GUARD}}_H */
