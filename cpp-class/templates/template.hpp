{{HEADER}}

#ifndef {{GUARD}}_H
    #define {{GUARD}}_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include "{{ATTRIBUTE_INCLUDE}}"   // _cold, _hot, _nodiscard
    #include <type_traits>                  // std::is_integral_v
    #include <mutex>                        // std::mutex

namespace {{NAMESPACE}} { // namespace start
//----------------------------------------------------------------//
/* CLASS */

template<typename T>
class {{CLASS}} {
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    private:
        mutable std::mutex _lock; // Handling of multithreading
        T _value = 0;

        // ------------ Function ---------- //
        _hot T compute_(const bool safe_mode = true)
        {
            /* Nothing */
            return {};
        };

    public:
        // ------------ Function ---------- //
        _hot _nodiscard T compute(const bool safe_mode = true) {return this->compute_(safe_mode);};
        _cold _nodiscard T value(void) const {return this->_value;};

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
