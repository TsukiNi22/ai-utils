# libutils `type`

Generated from libutils `v5.0.0` (commit `6aac89f`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/type/CApi.h`

C API of utils::type: Freezable / Worker of any handle, vectors, matrices & bidirectional lookup tables (C & C++ compatible) The templates are given for the fixed types of type/CApiTypes.h (suffix: i8 ... u64, size, f32, f64, f128, bool, char, str) & as bytes (void* + size) Every function that can fail returns UTILS_OK / UTILS_KO (or NULL / 0), details in utils_exception

```c
#define C_API_VECTOR2_DECL(name, type) // declares, for each type:
    typedef struct utils_vector2_<name> {
    type x;
    type y;
    } utils_vector2_<name>_t;
    utils_vector2_<name>_t utils_vector2_<name>_add(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    utils_vector2_<name>_t utils_vector2_<name>_sub(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    utils_vector2_<name>_t utils_vector2_<name>_mul(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    utils_vector2_<name>_t utils_vector2_<name>_div(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    utils_vector2_<name>_t utils_vector2_<name>_add_scalar(utils_vector2_<name>_t v, type s);
    utils_vector2_<name>_t utils_vector2_<name>_sub_scalar(utils_vector2_<name>_t v, type s);
    utils_vector2_<name>_t utils_vector2_<name>_mul_scalar(utils_vector2_<name>_t v, type s);
    utils_vector2_<name>_t utils_vector2_<name>_div_scalar(utils_vector2_<name>_t v, type s);
    utils_vector2_<name>_t utils_vector2_<name>_neg(utils_vector2_<name>_t v);
    utils_vector2_<name>_t utils_vector2_<name>_min(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    utils_vector2_<name>_t utils_vector2_<name>_max(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    utils_vector2_<name>_t utils_vector2_<name>_clamp(utils_vector2_<name>_t v, utils_vector2_<name>_t min, utils_vector2_<name>_t max);
    utils_vector2_<name>_t utils_vector2_<name>_sign(utils_vector2_<name>_t v);
    utils_vector2_<name>_t utils_vector2_<name>_normalize(utils_vector2_<name>_t v); // zero vector: kept
    type utils_vector2_<name>_dot(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    type utils_vector2_<name>_cross(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    type utils_vector2_<name>_length(utils_vector2_<name>_t v); // truncated for the integer types
    type utils_vector2_<name>_length_squared(utils_vector2_<name>_t v);
    bool utils_vector2_<name>_equal(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    bool utils_vector2_<name>_less(utils_vector2_<name>_t a, utils_vector2_<name>_t b); // like operator< of the c++
    bool utils_vector2_<name>_less_equal(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    bool utils_vector2_<name>_greater(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    bool utils_vector2_<name>_greater_equal(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    type utils_vector2_<name>_get(utils_vector2_<name>_t v, size_t index); // 0 on error (index >= 2)
    int utils_vector2_<name>_set(utils_vector2_<name>_t* v, size_t index, type value);
#define C_API_VECTOR2_INT_DECL(name, type) // declares, for each type:
    utils_vector2_<name>_t utils_vector2_<name>_and(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    utils_vector2_<name>_t utils_vector2_<name>_or(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
    utils_vector2_<name>_t utils_vector2_<name>_xor(utils_vector2_<name>_t a, utils_vector2_<name>_t b);
#define C_API_VECTOR3_DECL(name, type) // declares, for each type:
    typedef struct utils_vector3_<name> {
    type x;
    type y;
    type z;
    } utils_vector3_<name>_t;
    utils_vector3_<name>_t utils_vector3_<name>_add(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    utils_vector3_<name>_t utils_vector3_<name>_sub(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    utils_vector3_<name>_t utils_vector3_<name>_mul(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    utils_vector3_<name>_t utils_vector3_<name>_div(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    utils_vector3_<name>_t utils_vector3_<name>_add_scalar(utils_vector3_<name>_t v, type s);
    utils_vector3_<name>_t utils_vector3_<name>_sub_scalar(utils_vector3_<name>_t v, type s);
    utils_vector3_<name>_t utils_vector3_<name>_mul_scalar(utils_vector3_<name>_t v, type s);
    utils_vector3_<name>_t utils_vector3_<name>_div_scalar(utils_vector3_<name>_t v, type s);
    utils_vector3_<name>_t utils_vector3_<name>_neg(utils_vector3_<name>_t v);
    utils_vector3_<name>_t utils_vector3_<name>_min(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    utils_vector3_<name>_t utils_vector3_<name>_max(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    utils_vector3_<name>_t utils_vector3_<name>_clamp(utils_vector3_<name>_t v, utils_vector3_<name>_t min, utils_vector3_<name>_t max);
    utils_vector3_<name>_t utils_vector3_<name>_sign(utils_vector3_<name>_t v);
    utils_vector3_<name>_t utils_vector3_<name>_normalize(utils_vector3_<name>_t v); // zero vector: kept
    utils_vector3_<name>_t utils_vector3_<name>_cross(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    type utils_vector3_<name>_dot(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    type utils_vector3_<name>_length(utils_vector3_<name>_t v); // truncated for the integer types
    type utils_vector3_<name>_length_squared(utils_vector3_<name>_t v);
    bool utils_vector3_<name>_equal(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    bool utils_vector3_<name>_less(utils_vector3_<name>_t a, utils_vector3_<name>_t b); // like operator< of the c++
    bool utils_vector3_<name>_less_equal(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    bool utils_vector3_<name>_greater(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    bool utils_vector3_<name>_greater_equal(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    type utils_vector3_<name>_get(utils_vector3_<name>_t v, size_t index); // 0 on error (index >= 3)
    int utils_vector3_<name>_set(utils_vector3_<name>_t* v, size_t index, type value);
#define C_API_VECTOR3_INT_DECL(name, type) // declares, for each type:
    utils_vector3_<name>_t utils_vector3_<name>_and(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    utils_vector3_<name>_t utils_vector3_<name>_or(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
    utils_vector3_<name>_t utils_vector3_<name>_xor(utils_vector3_<name>_t a, utils_vector3_<name>_t b);
#define C_API_MATRIX_OP2_DECL(name, op, arg) // declares, for each type:
    utils_matrix_<name>_t* utils_matrix_<name>_<op>(const utils_matrix_<name>_t* a, arg);
    int utils_matrix_<name>_<op>_r(const utils_matrix_<name>_t* a, arg, utils_matrix_<name>_t* out);
    int utils_matrix_<name>_<op>_assign(utils_matrix_<name>_t* a, arg);
#define C_API_MATRIX_OP1_DECL(name, op) // declares, for each type:
    utils_matrix_<name>_t* utils_matrix_<name>_<op>(const utils_matrix_<name>_t* a);
    int utils_matrix_<name>_<op>_r(const utils_matrix_<name>_t* a, utils_matrix_<name>_t* out);
    int utils_matrix_<name>_<op>_assign(utils_matrix_<name>_t* a);
#define C_API_MATRIX_DECL(name, type) // declares, for each type:
    typedef struct utils_matrix_<name> utils_matrix_<name>_t;
    utils_matrix_<name>_t* utils_matrix_<name>_create(size_t rows, size_t cols); // every value at 0, NULL on error
    utils_matrix_<name>_t* utils_matrix_<name>_copy(const utils_matrix_<name>_t* m); // NULL on error
    void utils_matrix_<name>_destroy(utils_matrix_<name>_t* m); // NULL: nothing
    size_t utils_matrix_<name>_rows(const utils_matrix_<name>_t* m);
    size_t utils_matrix_<name>_cols(const utils_matrix_<name>_t* m);
    int utils_matrix_<name>_get(const utils_matrix_<name>_t* m, size_t x, size_t y, type* value);
    int utils_matrix_<name>_set(utils_matrix_<name>_t* m, size_t x, size_t y, type value);
    int utils_matrix_<name>_set_values(utils_matrix_<name>_t* m, const type* values, size_t count, size_t x, size_t y); // row by row from (x, y)
    size_t utils_matrix_<name>_row_r(const utils_matrix_<name>_t* m, size_t n, type* buf, size_t count); // copied in buf (count max), returns the full length (0 on error)
    size_t utils_matrix_<name>_col_r(const utils_matrix_<name>_t* m, size_t n, type* buf, size_t count);
    int utils_matrix_<name>_swap(utils_matrix_<name>_t* m, size_t ax, size_t ay, size_t bx, size_t by);
    int utils_matrix_<name>_swap_row(utils_matrix_<name>_t* m, size_t a, size_t b);
    int utils_matrix_<name>_swap_col(utils_matrix_<name>_t* m, size_t a, size_t b);
    int utils_matrix_<name>_clear(utils_matrix_<name>_t* m); // every value at 0
    int utils_matrix_<name>_det(const utils_matrix_<name>_t* m, type* value); // square only
    int utils_matrix_<name>_trace(const utils_matrix_<name>_t* m, type* value); // square only
    bool utils_matrix_<name>_equal(const utils_matrix_<name>_t* a, const utils_matrix_<name>_t* b);
    C_API_MATRIX_OP2_DECL(name, add, const utils_matrix_<name>_t* b)
    C_API_MATRIX_OP2_DECL(name, sub, const utils_matrix_<name>_t* b)
    C_API_MATRIX_OP2_DECL(name, mul, const utils_matrix_<name>_t* b) // product
    C_API_MATRIX_OP2_DECL(name, mul_scalar, type s)
    C_API_MATRIX_OP2_DECL(name, div_scalar, type s)
    C_API_MATRIX_OP1_DECL(name, neg)
    C_API_MATRIX_OP1_DECL(name, transpose)
#define C_API_MATRIX_FLOAT_DECL(name, type)
#define C_API_BLT_SAME_DECL(name, type) // declares, for each type:
    typedef struct utils_blt_<name> utils_blt_<name>_t;
    utils_blt_<name>_t* utils_blt_<name>_create(void); // NULL on error
    void utils_blt_<name>_destroy(utils_blt_<name>_t* blt); // NULL: nothing
    int utils_blt_<name>_clear(utils_blt_<name>_t* blt);
    int utils_blt_<name>_set(utils_blt_<name>_t* blt, type left, type right, bool force); // force: override an existing one
    int utils_blt_<name>_add(utils_blt_<name>_t* blt, type left, type right);
    int utils_blt_<name>_remove(utils_blt_<name>_t* blt, type element);
    int utils_blt_<name>_remove_all(utils_blt_<name>_t* blt, type const* elements, size_t count);
    int utils_blt_<name>_get(const utils_blt_<name>_t* blt, type element, type* other); // the other side of element
#define C_API_BLT_DECL(name, left_type, right_type) // declares, for each type:
    typedef struct utils_blt_<name> utils_blt_<name>_t;
    utils_blt_<name>_t* utils_blt_<name>_create(void); // NULL on error
    void utils_blt_<name>_destroy(utils_blt_<name>_t* blt); // NULL: nothing
    int utils_blt_<name>_clear(utils_blt_<name>_t* blt);
    int utils_blt_<name>_set(utils_blt_<name>_t* blt, left_type left, right_type right, bool force); // force: override an existing one
    int utils_blt_<name>_add(utils_blt_<name>_t* blt, left_type left, right_type right);
    int utils_blt_<name>_remove_left(utils_blt_<name>_t* blt, left_type left);
    int utils_blt_<name>_remove_right(utils_blt_<name>_t* blt, right_type right);
    int utils_blt_<name>_remove_lefts(utils_blt_<name>_t* blt, left_type const* lefts, size_t count);
    int utils_blt_<name>_remove_rights(utils_blt_<name>_t* blt, right_type const* rights, size_t count);
    int utils_blt_<name>_get_right(const utils_blt_<name>_t* blt, left_type left, right_type* right);
    int utils_blt_<name>_get_left(const utils_blt_<name>_t* blt, right_type right, left_type* left);
#define C_API_BLT_STR_INT_DECL(name, type)

// global
typedef struct utils_worker utils_worker_t; // utils::type::Worker + data of the C side (element of a load balancer)
typedef struct utils_blt utils_blt_t; // utils::type::BidirectionalLookupTable of bytes (an element is found on both sides)
int utils_freezable_freeze(void* handle);
bool utils_freezable_is_frozen(const void* handle);
int utils_freezable_require_frozen(const void* handle); // UTILS_KO (ShouldBeFrozen) when not frozen
int utils_freezable_require_unfrozen(const void* handle); // UTILS_KO (Frozen) when frozen
utils_worker_t* utils_worker_create(void* data); // NULL on error
void utils_worker_destroy(utils_worker_t* worker); // NULL: nothing
void* utils_worker_get_data(const utils_worker_t* worker);
int utils_worker_set_data(utils_worker_t* worker, void* data);
int utils_worker_set_working_status(void* handle, bool status);
bool utils_worker_is_working(const void* handle);
uint64_t utils_worker_get_stoped_working_timestamp(const void* handle); // steady clock in ns (when the status was set to false), 0 on error
C_API_NUMERIC_TYPES(C_API_VECTOR2_DECL)
C_API_INT_TYPES(C_API_VECTOR2_INT_DECL)
C_API_NUMERIC_TYPES(C_API_VECTOR3_DECL)
C_API_INT_TYPES(C_API_VECTOR3_INT_DECL)
C_API_NUMERIC_TYPES(C_API_MATRIX_DECL)
C_API_FLOAT_TYPES(C_API_MATRIX_FLOAT_DECL)
utils_blt_t* utils_blt_create(void); // NULL on error
void utils_blt_destroy(utils_blt_t* blt); // NULL: nothing
int utils_blt_clear(utils_blt_t* blt);
int utils_blt_set(utils_blt_t* blt, const void* left, size_t left_size, const void* right, size_t right_size, bool force); // force: override an existing one
int utils_blt_add(utils_blt_t* blt, const void* left, size_t left_size, const void* right, size_t right_size);
int utils_blt_remove(utils_blt_t* blt, const void* element, size_t size);
const void* utils_blt_get(const utils_blt_t* blt, const void* element, size_t size, size_t* other_size); // the other side of element, valid until the table is changed (NULL on error), other_size: optional
C_API_BLT_SAME_DECL(str_str, const char*)
C_API_BLT_SAME_DECL(size_size, size_t)
C_API_BLT_SAME_DECL(i64_i64, int64_t)
C_API_INT_TYPES(C_API_BLT_STR_INT_DECL)
```

## `utils/type/CApi.hpp`

Handles of the C API of utils::type (usable in c++ to get the object)

```cpp
#define C_API_MATRIX_HANDLE(name, ctype) // declares, for each type:
    struct utils_matrix_<name>: public utils_handle {
    utils::type::OMatrix<ctype> matrix;
    utils_matrix_<name>(utils::type::OMatrix<ctype> m): matrix{std::move(m)} { /* Nothing */ };
    };
#define C_API_BLT_HANDLE(name, left_type, right_type) // declares, for each type:
    struct utils_blt_<name>: public utils_handle, public utils::type::BidirectionalLookupTable<utils::type::c_api::cpp_t<left_type>, utils::type::c_api::cpp_t<right_type>> {};
#define C_API_BLT_SAME_HANDLE(name, ctype) C_API_BLT_HANDLE(name, ctype, ctype)
#define C_API_BLT_STR_INT_HANDLE(name, ctype)
#define C_API_VECTOR2_DEF(name, ctype) // declares, for each type:
    utils_vector2_<name>_t utils_vector2_<name>_add(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return c2<utils_vector2_<name>_t>(o2<ctype>(a) + o2<ctype>(b));};
    utils_vector2_<name>_t utils_vector2_<name>_sub(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return c2<utils_vector2_<name>_t>(o2<ctype>(a) - o2<ctype>(b));};
    utils_vector2_<name>_t utils_vector2_<name>_mul(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return c2<utils_vector2_<name>_t>(o2<ctype>(a) * o2<ctype>(b));};
    utils_vector2_<name>_t utils_vector2_<name>_div(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return c2<utils_vector2_<name>_t>(o2<ctype>(a) / o2<ctype>(b));};
    utils_vector2_<name>_t utils_vector2_<name>_add_scalar(utils_vector2_<name>_t v, ctype s) {return c2<utils_vector2_<name>_t>(o2<ctype>(v) + s);};
    utils_vector2_<name>_t utils_vector2_<name>_sub_scalar(utils_vector2_<name>_t v, ctype s) {return c2<utils_vector2_<name>_t>(o2<ctype>(v) - s);};
    utils_vector2_<name>_t utils_vector2_<name>_mul_scalar(utils_vector2_<name>_t v, ctype s) {return c2<utils_vector2_<name>_t>(o2<ctype>(v) * s);};
    utils_vector2_<name>_t utils_vector2_<name>_div_scalar(utils_vector2_<name>_t v, ctype s) {return c2<utils_vector2_<name>_t>(o2<ctype>(v) / s);};
    utils_vector2_<name>_t utils_vector2_<name>_neg(utils_vector2_<name>_t v) {return c2<utils_vector2_<name>_t>(-o2<ctype>(v));};
    utils_vector2_<name>_t utils_vector2_<name>_min(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return c2<utils_vector2_<name>_t>(o2<ctype>(a).min(o2<ctype>(b)));};
    utils_vector2_<name>_t utils_vector2_<name>_max(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return c2<utils_vector2_<name>_t>(o2<ctype>(a).max(o2<ctype>(b)));};
    utils_vector2_<name>_t utils_vector2_<name>_clamp(utils_vector2_<name>_t v, utils_vector2_<name>_t min, utils_vector2_<name>_t max) {return c2<utils_vector2_<name>_t>(o2<ctype>(v).clamp(o2<ctype>(min), o2<ctype>(max)));};
    utils_vector2_<name>_t utils_vector2_<name>_sign(utils_vector2_<name>_t v) {return c2<utils_vector2_<name>_t>(o2<ctype>(v).sign());};
    utils_vector2_<name>_t utils_vector2_<name>_normalize(utils_vector2_<name>_t v) {return c2<utils_vector2_<name>_t>(o2<ctype>(v).normalize());};
    ctype utils_vector2_<name>_dot(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return o2<ctype>(a).dot(o2<ctype>(b));};
    ctype utils_vector2_<name>_cross(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return o2<ctype>(a).cross(o2<ctype>(b));};
    ctype utils_vector2_<name>_length(utils_vector2_<name>_t v) {return o2<ctype>(v).length();};
    ctype utils_vector2_<name>_length_squared(utils_vector2_<name>_t v) {return o2<ctype>(v).lengthSquared();};
    bool utils_vector2_<name>_equal(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return o2<ctype>(a) == o2<ctype>(b);};
    bool utils_vector2_<name>_less(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return o2<ctype>(a) < o2<ctype>(b);};
    bool utils_vector2_<name>_less_equal(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return o2<ctype>(a) <= o2<ctype>(b);};
    bool utils_vector2_<name>_greater(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return o2<ctype>(a) > o2<ctype>(b);};
    bool utils_vector2_<name>_greater_equal(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return o2<ctype>(a) >= o2<ctype>(b);};
    ctype utils_vector2_<name>_get(utils_vector2_<name>_t v, size_t index) {return utils::exception::c_api::guard_fallback(static_cast<ctype>(0), [&](void) -> ctype {return o2<ctype>(v).get(index);});};
    int utils_vector2_<name>_set(utils_vector2_<name>_t* v, size_t index, ctype value)
    {
    return utils::exception::c_api::guard([&](void) {
    utils::type::OVector2<ctype> res = o2<ctype>(utils::exception::c_api::not_null_ref(v, "vector"));
    res[index] = value;
    *v = c2<utils_vector2_<name>_t>(res);
    });
    }
#define C_API_VECTOR2_INT_DEF(name, ctype) // declares, for each type:
    utils_vector2_<name>_t utils_vector2_<name>_and(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return c2<utils_vector2_<name>_t>(o2<ctype>(a) & o2<ctype>(b));};
    utils_vector2_<name>_t utils_vector2_<name>_or(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return c2<utils_vector2_<name>_t>(o2<ctype>(a) | o2<ctype>(b));};
    utils_vector2_<name>_t utils_vector2_<name>_xor(utils_vector2_<name>_t a, utils_vector2_<name>_t b) {return c2<utils_vector2_<name>_t>(o2<ctype>(a) ^ o2<ctype>(b));}
#define C_API_VECTOR3_DEF(name, ctype) // declares, for each type:
    utils_vector3_<name>_t utils_vector3_<name>_add(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a) + o3<ctype>(b));};
    utils_vector3_<name>_t utils_vector3_<name>_sub(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a) - o3<ctype>(b));};
    utils_vector3_<name>_t utils_vector3_<name>_mul(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a) * o3<ctype>(b));};
    utils_vector3_<name>_t utils_vector3_<name>_div(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a) / o3<ctype>(b));};
    utils_vector3_<name>_t utils_vector3_<name>_add_scalar(utils_vector3_<name>_t v, ctype s) {return c3<utils_vector3_<name>_t>(o3<ctype>(v) + s);};
    utils_vector3_<name>_t utils_vector3_<name>_sub_scalar(utils_vector3_<name>_t v, ctype s) {return c3<utils_vector3_<name>_t>(o3<ctype>(v) - s);};
    utils_vector3_<name>_t utils_vector3_<name>_mul_scalar(utils_vector3_<name>_t v, ctype s) {return c3<utils_vector3_<name>_t>(o3<ctype>(v) * s);};
    utils_vector3_<name>_t utils_vector3_<name>_div_scalar(utils_vector3_<name>_t v, ctype s) {return c3<utils_vector3_<name>_t>(o3<ctype>(v) / s);};
    utils_vector3_<name>_t utils_vector3_<name>_neg(utils_vector3_<name>_t v) {return c3<utils_vector3_<name>_t>(-o3<ctype>(v));};
    utils_vector3_<name>_t utils_vector3_<name>_min(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a).min(o3<ctype>(b)));};
    utils_vector3_<name>_t utils_vector3_<name>_max(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a).max(o3<ctype>(b)));};
    utils_vector3_<name>_t utils_vector3_<name>_clamp(utils_vector3_<name>_t v, utils_vector3_<name>_t min, utils_vector3_<name>_t max) {return c3<utils_vector3_<name>_t>(o3<ctype>(v).clamp(o3<ctype>(min), o3<ctype>(max)));};
    utils_vector3_<name>_t utils_vector3_<name>_sign(utils_vector3_<name>_t v) {return c3<utils_vector3_<name>_t>(o3<ctype>(v).sign());};
    utils_vector3_<name>_t utils_vector3_<name>_normalize(utils_vector3_<name>_t v) {return c3<utils_vector3_<name>_t>(o3<ctype>(v).normalize());};
    utils_vector3_<name>_t utils_vector3_<name>_cross(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a).cross(o3<ctype>(b)));};
    ctype utils_vector3_<name>_dot(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return o3<ctype>(a).dot(o3<ctype>(b));};
    ctype utils_vector3_<name>_length(utils_vector3_<name>_t v) {return o3<ctype>(v).length();};
    ctype utils_vector3_<name>_length_squared(utils_vector3_<name>_t v) {return o3<ctype>(v).lengthSquared();};
    bool utils_vector3_<name>_equal(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return o3<ctype>(a) == o3<ctype>(b);};
    bool utils_vector3_<name>_less(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return o3<ctype>(a) < o3<ctype>(b);};
    bool utils_vector3_<name>_less_equal(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return o3<ctype>(a) <= o3<ctype>(b);};
    bool utils_vector3_<name>_greater(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return o3<ctype>(a) > o3<ctype>(b);};
    bool utils_vector3_<name>_greater_equal(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return o3<ctype>(a) >= o3<ctype>(b);};
    ctype utils_vector3_<name>_get(utils_vector3_<name>_t v, size_t index) {return utils::exception::c_api::guard_fallback(static_cast<ctype>(0), [&](void) -> ctype {return o3<ctype>(v).get(index);});};
    int utils_vector3_<name>_set(utils_vector3_<name>_t* v, size_t index, ctype value)
    {
    return utils::exception::c_api::guard([&](void) {
    utils::type::OVector3<ctype> res = o3<ctype>(utils::exception::c_api::not_null_ref(v, "vector"));
    res[index] = value;
    *v = c3<utils_vector3_<name>_t>(res);
    });
    }
#define C_API_VECTOR3_INT_DEF(name, ctype) // declares, for each type:
    utils_vector3_<name>_t utils_vector3_<name>_and(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a) & o3<ctype>(b));};
    utils_vector3_<name>_t utils_vector3_<name>_or(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a) | o3<ctype>(b));};
    utils_vector3_<name>_t utils_vector3_<name>_xor(utils_vector3_<name>_t a, utils_vector3_<name>_t b) {return c3<utils_vector3_<name>_t>(o3<ctype>(a) ^ o3<ctype>(b));}
#define C_API_MATRIX_OP2_DEF(name, ctype, op, arg_type, arg) // declares, for each type:
    utils_matrix_<name>_t* utils_matrix_<name>_<op>(const utils_matrix_<name>_t* a, arg_type b)
    {return matrix_new<utils_matrix_<name>_t>([&](void) {return MatrixOp<ctype>::op(matrix(a), arg);});}
    int utils_matrix_<name>_<op>_r(const utils_matrix_<name>_t* a, arg_type b, utils_matrix_<name>_t* out)
    {return matrix_r(out, [&](void) {return MatrixOp<ctype>::op(matrix(a), arg);});}
    int utils_matrix_<name>_<op>_assign(utils_matrix_<name>_t* a, arg_type b)
    {return matrix_r(a, [&](void) {return MatrixOp<ctype>::op(matrix(a), arg);});}
#define C_API_MATRIX_OP1_DEF(name, ctype, op) // declares, for each type:
    utils_matrix_<name>_t* utils_matrix_<name>_<op>(const utils_matrix_<name>_t* a)
    {return matrix_new<utils_matrix_<name>_t>([&](void) {return MatrixOp<ctype>::op(matrix(a));});}
    int utils_matrix_<name>_<op>_r(const utils_matrix_<name>_t* a, utils_matrix_<name>_t* out)
    {return matrix_r(out, [&](void) {return MatrixOp<ctype>::op(matrix(a));});}
    int utils_matrix_<name>_<op>_assign(utils_matrix_<name>_t* a)
    {return matrix_r(a, [&](void) {return MatrixOp<ctype>::op(matrix(a));});}
#define C_API_MATRIX_DEF(name, ctype) // declares, for each type:
    utils_matrix_<name>_t* utils_matrix_<name>_create(size_t rows, size_t cols)
    {return utils::exception::c_api::guard_fallback(nullptr, [&](void) -> utils_matrix_<name>_t* {return new utils_matrix_<name>_t(utils::type::OMatrix<ctype>(rows, cols));});}
    utils_matrix_<name>_t* utils_matrix_<name>_copy(const utils_matrix_<name>_t* m)
    {return matrix_new<utils_matrix_<name>_t>([&](void) {return matrix(m);});}
    void utils_matrix_<name>_destroy(utils_matrix_<name>_t* m) {(void)utils::exception::c_api::guard([&](void) {delete m;});};
    size_t utils_matrix_<name>_rows(const utils_matrix_<name>_t* m) {return utils::exception::c_api::guard_fallback(0, [&](void) -> std::size_t {return matrix(m).row();});};
    size_t utils_matrix_<name>_cols(const utils_matrix_<name>_t* m) {return utils::exception::c_api::guard_fallback(0, [&](void) -> std::size_t {return matrix(m).col();});};
    int utils_matrix_<name>_get(const utils_matrix_<name>_t* m, size_t x, size_t y, ctype* value)
    {return utils::exception::c_api::guard([&](void) {ctype& res = utils::exception::c_api::not_null_ref(value, "value"); res = matrix(m).at(x, y);});}
    int utils_matrix_<name>_set(utils_matrix_<name>_t* m, size_t x, size_t y, ctype value) {return utils::exception::c_api::guard([&](void) {matrix(m).set(x, y, value);});};
    int utils_matrix_<name>_set_values(utils_matrix_<name>_t* m, const ctype* values, size_t count, size_t x, size_t y)
    {
    return utils::exception::c_api::guard([&](void) {
    if (count > 0) (void)utils::exception::c_api::not_null(values, "values");
    matrix(m).set(values, values + count, x, y);
    });
    }
    size_t utils_matrix_<name>_row_r(const utils_matrix_<name>_t* m, size_t n, ctype* buf, size_t count)
    {
    return utils::exception::c_api::guard_fallback(0, [&](void) -> std::size_t {
    const std::vector<ctype> row = matrix(m).row(n);
    if (count > 0) std::copy_n(row.begin(), std::min(count, row.size()), utils::exception::c_api::not_null(buf, "buffer"));
    return row.size();
    });
    }
    size_t utils_matrix_<name>_col_r(const utils_matrix_<name>_t* m, size_t n, ctype* buf, size_t count)
    {
    return utils::exception::c_api::guard_fallback(0, [&](void) -> std::size_t {
    const std::vector<ctype> col = matrix(m).col(n);
    if (count > 0) std::copy_n(col.begin(), std::min(count, col.size()), utils::exception::c_api::not_null(buf, "buffer"));
    return col.size();
    });
    }
    int utils_matrix_<name>_swap(utils_matrix_<name>_t* m, size_t ax, size_t ay, size_t bx, size_t by)
    {return utils::exception::c_api::guard([&](void) {utils::type::OMatrix<ctype>& self = matrix(m); std::swap(self.at(ax, ay), self.at(bx, by));});}
    int utils_matrix_<name>_swap_row(utils_matrix_<name>_t* m, size_t a, size_t b)
    {
    return utils::exception::c_api::guard([&](void) {
    utils::type::OMatrix<ctype>& self = matrix(m);
    for (std::size_t y = 0; y < self.col(); ++y)
    std::swap(self.at(a, y), self.at(b, y));
    });
    }
    int utils_matrix_<name>_swap_col(utils_matrix_<name>_t* m, size_t a, size_t b) {return utils::exception::c_api::guard([&](void) {matrix(m).swapCol(a, b);});};
    int utils_matrix_<name>_clear(utils_matrix_<name>_t* m)
    {return utils::exception::c_api::guard([&](void) {utils::type::OMatrix<ctype>& self = matrix(m); self = utils::type::OMatrix<ctype>(self.row(), self.col());});}
    int utils_matrix_<name>_det(const utils_matrix_<name>_t* m, ctype* value)
    {return utils::exception::c_api::guard([&](void) {ctype& res = utils::exception::c_api::not_null_ref(value, "value"); res = matrix(m).det();});}
    int utils_matrix_<name>_trace(const utils_matrix_<name>_t* m, ctype* value)
    {return utils::exception::c_api::guard([&](void) {ctype& res = utils::exception::c_api::not_null_ref(value, "value"); res = matrix(m).trace();});}
    bool utils_matrix_<name>_equal(const utils_matrix_<name>_t* a, const utils_matrix_<name>_t* b)
    {return utils::exception::c_api::guard_fallback(false, [&](void) -> bool {return matrix(a) == matrix(b);});}
    C_API_MATRIX_OP2_DEF(name, ctype, add, const utils_matrix_<name>_t*, matrix(b))
    C_API_MATRIX_OP2_DEF(name, ctype, sub, const utils_matrix_<name>_t*, matrix(b))
    C_API_MATRIX_OP2_DEF(name, ctype, mul, const utils_matrix_<name>_t*, matrix(b))
    C_API_MATRIX_OP2_DEF(name, ctype, mul_scalar, ctype, b)
    C_API_MATRIX_OP2_DEF(name, ctype, div_scalar, ctype, b)
    C_API_MATRIX_OP1_DEF(name, ctype, neg)
    C_API_MATRIX_OP1_DEF(name, ctype, transpose)
#define C_API_MATRIX_FLOAT_DEF(name, ctype)
#define C_API_BLT_SAME_DEF(name, ctype) // declares, for each type:
    utils_blt_<name>_t* utils_blt_<name>_create(void) {return utils::exception::c_api::guard_fallback(nullptr, [](void) -> utils_blt_<name>_t* {return new utils_blt_<name>_t();});};
    void utils_blt_<name>_destroy(utils_blt_<name>_t* blt) {(void)utils::exception::c_api::guard([&](void) {delete blt;});};
    int utils_blt_<name>_clear(utils_blt_<name>_t* blt) {return utils::exception::c_api::guard([&](void) {table(blt).clear();});};
    int utils_blt_<name>_set(utils_blt_<name>_t* blt, ctype left, ctype right, bool force)
    {
    return utils::exception::c_api::guard([&](void) {
    utils_blt_<name>_t& self = table(blt);
    if (force) self.template setElement<true>(utils::type::c_api::Storage<ctype>::cpp(left), utils::type::c_api::Storage<ctype>::cpp(right));
    else self.template setElement<false>(utils::type::c_api::Storage<ctype>::cpp(left), utils::type::c_api::Storage<ctype>::cpp(right));
    });
    }
    int utils_blt_<name>_add(utils_blt_<name>_t* blt, ctype left, ctype right)
    {return utils::exception::c_api::guard([&](void) {table(blt).addElement(utils::type::c_api::Storage<ctype>::cpp(left), utils::type::c_api::Storage<ctype>::cpp(right));});}
    int utils_blt_<name>_remove(utils_blt_<name>_t* blt, ctype element)
    {return utils::exception::c_api::guard([&](void) {table(blt).removeElement(utils::type::c_api::Storage<ctype>::cpp(element));});}
    int utils_blt_<name>_remove_all(utils_blt_<name>_t* blt, ctype const* elements_, size_t count)
    {return utils::exception::c_api::guard([&](void) {utils_blt_<name>_t& self = table(blt); self.removeElements(elements(elements_, count));});}
    int utils_blt_<name>_get(const utils_blt_<name>_t* blt, ctype element, ctype* other)
    {
    return utils::exception::c_api::guard([&](void) {
    ctype& res = utils::exception::c_api::not_null_ref(other, "other");
    res = utils::type::c_api::Storage<ctype>::c(table(blt)[utils::type::c_api::Storage<ctype>::cpp(element)]);
    });
    }
#define C_API_BLT_DEF(name, left_type, right_type) // declares, for each type:
    utils_blt_<name>_t* utils_blt_<name>_create(void) {return utils::exception::c_api::guard_fallback(nullptr, [](void) -> utils_blt_<name>_t* {return new utils_blt_<name>_t();});};
    void utils_blt_<name>_destroy(utils_blt_<name>_t* blt) {(void)utils::exception::c_api::guard([&](void) {delete blt;});};
    int utils_blt_<name>_clear(utils_blt_<name>_t* blt) {return utils::exception::c_api::guard([&](void) {table(blt).clear();});};
    int utils_blt_<name>_set(utils_blt_<name>_t* blt, left_type left, right_type right, bool force)
    {
    return utils::exception::c_api::guard([&](void) {
    utils_blt_<name>_t& self = table(blt);
    if (force) self.template setElement<true>(utils::type::c_api::Storage<left_type>::cpp(left), utils::type::c_api::Storage<right_type>::cpp(right));
    else self.template setElement<false>(utils::type::c_api::Storage<left_type>::cpp(left), utils::type::c_api::Storage<right_type>::cpp(right));
    });
    }
    int utils_blt_<name>_add(utils_blt_<name>_t* blt, left_type left, right_type right)
    {return utils::exception::c_api::guard([&](void) {table(blt).addElement(utils::type::c_api::Storage<left_type>::cpp(left), utils::type::c_api::Storage<right_type>::cpp(right));});}
    int utils_blt_<name>_remove_left(utils_blt_<name>_t* blt, left_type left)
    {return utils::exception::c_api::guard([&](void) {table(blt).removeElement(utils::type::c_api::Storage<left_type>::cpp(left));});}
    int utils_blt_<name>_remove_right(utils_blt_<name>_t* blt, right_type right)
    {return utils::exception::c_api::guard([&](void) {table(blt).removeElement(utils::type::c_api::Storage<right_type>::cpp(right));});}
    int utils_blt_<name>_remove_lefts(utils_blt_<name>_t* blt, left_type const* lefts, size_t count)
    {return utils::exception::c_api::guard([&](void) {utils_blt_<name>_t& self = table(blt); self.removeElements(elements(lefts, count));});}
    int utils_blt_<name>_remove_rights(utils_blt_<name>_t* blt, right_type const* rights, size_t count)
    {return utils::exception::c_api::guard([&](void) {utils_blt_<name>_t& self = table(blt); self.removeElements(elements(rights, count));});}
    int utils_blt_<name>_get_right(const utils_blt_<name>_t* blt, left_type left, right_type* right)
    {
    return utils::exception::c_api::guard([&](void) {
    right_type& res = utils::exception::c_api::not_null_ref(right, "right");
    res = utils::type::c_api::Storage<right_type>::c(table(blt)[utils::type::c_api::Storage<left_type>::cpp(left)]);
    });
    }
    int utils_blt_<name>_get_left(const utils_blt_<name>_t* blt, right_type right, left_type* left)
    {
    return utils::exception::c_api::guard([&](void) {
    left_type& res = utils::exception::c_api::not_null_ref(left, "left");
    res = utils::type::c_api::Storage<left_type>::c(table(blt)[utils::type::c_api::Storage<right_type>::cpp(right)]);
    });
    }
#define C_API_BLT_STR_INT_DEF(name, ctype)

// global
struct utils_worker: public utils_handle, public utils::type::Worker {
    void* data = nullptr;
    utils_worker(void* d = nullptr); // default: a worker of a load balancer
};
struct utils_blt: public utils_handle, public utils::type::BidirectionalLookupTable<std::string, std::string> {
};
C_API_NUMERIC_TYPES(C_API_MATRIX_HANDLE)
C_API_BLT_SAME_HANDLE(str_str, const char*)
C_API_BLT_SAME_HANDLE(size_size, std::size_t)
C_API_BLT_SAME_HANDLE(i64_i64, std::int64_t)
C_API_INT_TYPES(C_API_BLT_STR_INT_HANDLE)
```

## `utils/type/CApiTypes.h`

Fixed types of the C API (X-macros: X(name, type) unrolled by each C API for its templates) (C & C++ compatible) name: suffix of the functions & types (utils_vector2_f64_t, utils_blt_str_size_*), str: const char* in C (std::string in c++, copied)

```c
#define C_API_INT_TYPES(X)
#define C_API_FLOAT_TYPES(X)
#define C_API_NUMERIC_TYPES(X)
#define C_API_VALUE_TYPES(X)
#define C_API_TYPES(X)

```

## `utils/type/CApiTypes.hpp`

C++ side of the fixed types of the C API: storage of a C type (str: std::string) & the conversions

Namespace: `utils::type::c_api`

```cpp
// namespace utils::type::c_api
template<typename C> struct Storage {
    using Type = C;
    static C cpp(const C value);
    static C c(const Type& value);
};
template<> struct Storage<const char*> {
    using Type = std::string;
    static std::string cpp(const char* value);
    static const char* c(const std::string& value);
};
```

## `utils/type/Freezable.hpp`

Namespace: `utils::type`

```cpp
// namespace utils::type
class Freezable: private utils::security::observer::Observer<"Freezable"> {
    void freeze(void);
    bool isFrozen(void) const;
    void requireFrozen(void) const;
    void requireUnfrozen(void) const;
    Freezable& operator=(const Freezable& other);
    Freezable& operator=(Freezable&& other);
    Freezable(const bool frozen = false);
    Freezable(const Freezable& other);
    Freezable(Freezable&& other);
    ~Freezable() = default;
};
```

## `utils/type/Worker.hpp`

Namespace: `utils::type`

```cpp
// namespace utils::type
class Worker: private utils::security::observer::Observer<"Worker"> {
    void setWorkingStatus(const bool status);
    std::chrono::steady_clock::time_point getStopedWorkingTimestamp(void) const;
    bool isWorking(void) const;
    Worker& operator=(const Worker& other);
    Worker& operator=(Worker&& other);
    Worker() = default;
    Worker(const Worker& other);
    Worker(Worker&& other);
    ~Worker() = default;
};
```

## `utils/type/blt/BidirectionalLookupTable.hpp`

Global bidirectional lookup table include

## `utils/type/blt/BidirectionalLookupTable_t-t.hpp`

Class used for a bidirectional lookup table

Namespace: `utils::type`

```cpp
// namespace utils::type
template< typename L, typename R, typename HashL = std::hash<L>, typename HashR = std::hash<R>, typename EqualL = std::equal_to<L>, typename EqualR = std::equal_to<R> > class BidirectionalLookupTable: public utils::type::Freezable, private utils::security::observer::Observer<"BidirectionalLookupTable"> {
    void clear(void);
    void removeElement(const L& left);
    void removeElements(const std::vector<L>& lefts);
    void removeElement(const R& right);
    void removeElements(const std::vector<R>& rights);
    void addElement(const L& left, const R& right);
    void addElement(const R& right, const L& left);
    template<bool force = false> void setElement(const L& left, const R& right);
    template<bool force = false> void setElement(const R& right, const L& left);
    BidirectionalLookupTable& operator=(BidirectionalLookupTable&& other) = default;
    const R& operator[](const L& left) const;
    const L& operator[](const R& right) const;
    BidirectionalLookupTable() = default;
    BidirectionalLookupTable(BidirectionalLookupTable&& other) = default;
    ~BidirectionalLookupTable() = default;
};
```

## `utils/type/blt/BidirectionalLookupTable_t.hpp`

Class used for a bidirectional lookup table specialized for one type

Namespace: `utils::type`

```cpp
#define BLT_TYPE(T) T, T, std::hash<T>, std::hash<T>, std::equal_to<T>, std::equal_to<T>

// namespace utils::type
template< typename T, typename Hash, typename Equal > class BidirectionalLookupTable<T, T, Hash, Hash, Equal, Equal>: public utils::type::Freezable, private utils::security::observer::Observer<"BidirectionalLookupTable"> {
    void clear(void);
    void removeElement(const T& element); // throw if frozen
    void removeElements(const std::vector<T>& elements);
    void addElement(const T& left, const T& right);
    template<bool force = false> void setElement(const T& left, const T& right);
    BidirectionalLookupTable& operator=(BidirectionalLookupTable&& other) = default;
    const T& operator[](const T& element) const;
    BidirectionalLookupTable() = default;
    BidirectionalLookupTable(BidirectionalLookupTable&& other) = default;
    ~BidirectionalLookupTable() = default;
};
```

## `utils/type/matrix/Matrix.hpp`

Matrix that contains x * y value of undefined type x -> row & y -> column (_matrix[x][y])

Namespace: `utils::type`

```cpp
// namespace utils::type
template<typename T> class Matrix: private utils::security::observer::Observer<"Matrix"> {
    template<typename It> void set(It begin, It end, const std::size_t x = 0, const std::size_t y = 0) requires std::input_iterator<It> && std::same_as<std::iter_value_t<It>, T>; // fill row by row from (x, y)
    void set(const std::size_t x, const std::size_t y, const T& value);
    void set(const std::size_t x, const std::size_t y, T&& value);
    void swap(const std::pair<std::size_t, std::size_t>& a, const std::pair<std::size_t, std::size_t>& b);
    void swapCol(const std::size_t a, const std::size_t b);
    void swapRow(const std::size_t a, const std::size_t b);
    void clear(void); // reinit all value to default
    void transpose(void);
    void invert(void) requires utils::concepts::Arithmetic<T> && utils::concepts::EqualityComparable<T> && (!std::is_integral_v<T>); // disabled for integer (the result would be truncated)
    T det(void) const requires utils::concepts::Arithmetic<T> && utils::concepts::EqualityComparable<T> && utils::concepts::Negatable<T>;
    T trace(void) const requires utils::concepts::Addable<T>;
    T& at(const std::size_t x, const std::size_t y);
    const T& at(const std::size_t x, const std::size_t y) const;
    std::vector<T> col(const std::size_t n) const;
    std::vector<T> row(const std::size_t n) const;
    std::pair<std::size_t, std::size_t> size(void) const;
    std::size_t col(void) const;
    std::size_t row(void) const;
    T& operator()(const std::size_t x, const std::size_t y);
    const T& operator()(const std::size_t x, const std::size_t y) const;
    template<typename U> utils::type::Matrix<std::common_type_t<T, U>> operator+(const utils::type::Matrix<U>& m) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> utils::type::Matrix<std::common_type_t<T, U>> operator-(const utils::type::Matrix<U>& m) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> utils::type::Matrix<std::common_type_t<T, U>> operator*(const utils::type::Matrix<U>& m) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddableWith<T, U>;
    template<typename U> utils::type::Matrix<std::common_type_t<T, U>> operator/(const utils::type::Matrix<U>& m) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddableWith<T, U> && (!std::is_integral_v<std::common_type_t<T, U>>); // this * m^-1
    template<typename U> utils::type::Matrix<std::common_type_t<T, U>> operator*(const U& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> utils::type::Matrix<std::common_type_t<T, U>> operator/(const U& v) const requires utils::concepts::DivisibleWith<T, U>;
    Matrix& operator=(const Matrix& other) = default;
    Matrix& operator=(Matrix&& other);
    template<typename U> Matrix& operator+=(const utils::type::Matrix<U>& m) requires utils::concepts::AddAssignableWith<T, U>;
    template<typename U> Matrix& operator-=(const utils::type::Matrix<U>& m) requires utils::concepts::SubtractAssignableWith<T, U>;
    template<typename U> Matrix& operator*=(const utils::type::Matrix<U>& m) requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddAssignable<T>;
    template<typename U> Matrix& operator/=(const utils::type::Matrix<U>& m) requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddAssignable<T> && (!std::is_integral_v<std::common_type_t<T, U>>); // this * m^-1
    template<typename U> Matrix& operator*=(const U& v) requires utils::concepts::MultiplyAssignableWith<T, U>;
    template<typename U> Matrix& operator/=(const U& v) requires utils::concepts::DivideAssignableWith<T, U>;
    template<typename U> bool operator==(const utils::type::Matrix<U>& m) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator!=(const utils::type::Matrix<U>& m) const requires utils::concepts::EqualityComparableWith<T, U>;
    Matrix operator-(void) const requires utils::concepts::Negatable<T>;
    explicit Matrix(const std::size_t n);
    Matrix(const std::size_t x, const std::size_t y);
    template<typename U> Matrix(const utils::type::Matrix<U>& m) requires std::constructible_from<T, U>;
    Matrix(const Matrix& other) = default;
    Matrix(Matrix&& other);
    ~Matrix() = default;
};
template<typename T, typename U> utils::type::Matrix<std::common_type_t<T, U>> operator*(const T& lhs, const utils::type::Matrix<U>& rhs) requires utils::concepts::MultipliableWith<T, U>;
template<typename T> std::ostream& operator<<(std::ostream& os, const utils::type::Matrix<T>& m);
```

## `utils/type/matrix/OMatrix.hpp`

Matrix that contains x * y value of undefined type x -> row & y -> column (row-major contiguous storage) Optimized version

Namespace: `utils::type`

```cpp
// namespace utils::type
template<typename T> class OMatrix {
    template<typename It> void set(It begin, It end, const std::size_t x = 0, const std::size_t y = 0) requires std::input_iterator<It> && std::same_as<std::iter_value_t<It>, T>; // fill row by row from (x, y)
    void set(const std::size_t x, const std::size_t y, const T& value);
    void set(const std::size_t x, const std::size_t y, T&& value);
    void swap(const std::pair<std::size_t, std::size_t>& a, const std::pair<std::size_t, std::size_t>& b);
    void swapCol(const std::size_t a, const std::size_t b);
    void swapRow(const std::size_t a, const std::size_t b);
    void clear(void); // reinit all value to default
    void transpose(void);
    void invert(void); // integer type will be truncated (Be careful!!!)
    T det(void) const;
    T trace(void) const;
    T& at(const std::size_t x, const std::size_t y);
    const T& at(const std::size_t x, const std::size_t y) const;
    std::vector<T> col(const std::size_t n) const;
    std::vector<T> row(const std::size_t n) const;
    std::pair<std::size_t, std::size_t> size(void) const;
    std::size_t col(void) const;
    std::size_t row(void) const;
    T& operator()(const std::size_t x, const std::size_t y); // no bound check
    const T& operator()(const std::size_t x, const std::size_t y) const; // no bound check
    template<typename U> OMatrix operator+(const utils::type::OMatrix<U>& m) const;
    template<typename U> OMatrix operator-(const utils::type::OMatrix<U>& m) const;
    template<typename U> OMatrix operator*(const utils::type::OMatrix<U>& m) const;
    template<typename U> OMatrix operator/(const utils::type::OMatrix<U>& m) const; // this * m^-1
    template<typename U> OMatrix operator*(const U& v) const;
    template<typename U> OMatrix operator/(const U& v) const;
    OMatrix& operator=(const OMatrix& other) = default;
    OMatrix& operator=(OMatrix&& other) noexcept;
    template<typename U> OMatrix& operator+=(const utils::type::OMatrix<U>& m);
    template<typename U> OMatrix& operator-=(const utils::type::OMatrix<U>& m);
    template<typename U> OMatrix& operator*=(const utils::type::OMatrix<U>& m);
    template<typename U> OMatrix& operator/=(const utils::type::OMatrix<U>& m);
    template<typename U> OMatrix& operator*=(const U& v);
    template<typename U> OMatrix& operator/=(const U& v);
    template<typename U> bool operator==(const utils::type::OMatrix<U>& m) const;
    template<typename U> bool operator!=(const utils::type::OMatrix<U>& m) const;
    OMatrix operator-(void) const;
    explicit OMatrix(const std::size_t n);
    OMatrix(const std::size_t x, const std::size_t y);
    template<typename U> OMatrix(const utils::type::OMatrix<U>& m);
    OMatrix(const OMatrix& other) = default;
    OMatrix(OMatrix&& other) noexcept;
    ~OMatrix() = default;
};
template<typename T, typename U> utils::type::OMatrix<U> operator*(const T& lhs, const utils::type::OMatrix<U>& rhs);
template<typename T> std::ostream& operator<<(std::ostream& os, const utils::type::OMatrix<T>& m);
```

## `utils/type/vector/IVector.hpp`

Interface for the cutomized vector

Namespace: `utils::type`

```cpp
// namespace utils::type
template<typename T> class IVector: private utils::security::observer::Observer<"IVector"> {
    virtual T get(std::size_t index) const = 0;
    IVector& operator=(const IVector& other) = default;
    IVector& operator=(IVector&& other) = default;
    IVector() = default;
    IVector(const IVector& other) = default;
    IVector(IVector&& other) = default;
    virtual ~IVector() = default;
};
```

## `utils/type/vector/OVector2.hpp`

Vector hat contains 2 value respectivly x & y of undefined type Optimized version

Namespace: `utils::type`

```cpp
#define MAX_INDEX_OVECTOR2 2

// namespace utils::type
template<typename T> class OVector2 {
    T x;
    T y;
    T get(std::size_t index) const;
    OVector2 min(const OVector2& min) const;
    OVector2 max(const OVector2& max) const;
    OVector2 clamp(const OVector2& min, const OVector2& max) const;
    template<typename U> T dot(const OVector2<U>& v) const;
    template<typename U> T cross(const OVector2<U>& v) const;
    T length(void) const;
    T lengthSquared(void) const;
    OVector2 sign(void) const;
    OVector2 normalize(void) const;
    T& operator[](std::size_t index);
    const T& operator[](std::size_t index) const;
    template<typename U> OVector2 operator+(const U& v) const;
    template<typename U> OVector2 operator+(const OVector2<U>& v) const;
    template<typename U> OVector2 operator-(const U& v) const;
    template<typename U> OVector2 operator-(const OVector2<U>& v) const;
    template<typename U> OVector2 operator*(const U& v) const;
    template<typename U> OVector2 operator*(const OVector2<U>& v) const;
    template<typename U> OVector2 operator/(const U& v) const;
    template<typename U> OVector2 operator/(const OVector2<U>& v) const;
    OVector2& operator++(void);
    OVector2 operator++(int);
    OVector2& operator--(void);
    OVector2 operator--(int);
    template<typename U> OVector2 operator&(const OVector2<U>& v) const;
    template<typename U> OVector2 operator|(const OVector2<U>& v) const;
    template<typename U> OVector2 operator^(const OVector2<U>& v) const;
    template<typename U> OVector2& operator=(const OVector2<U>& v);
    template<typename U> OVector2& operator=(OVector2<U>&& v);
    template<typename U> OVector2& operator+=(const U& v);
    template<typename U> OVector2& operator+=(const OVector2<U>& v);
    template<typename U> OVector2& operator-=(const U& v);
    template<typename U> OVector2& operator-=(const OVector2<U>& v);
    template<typename U> OVector2& operator*=(const U& v);
    template<typename U> OVector2& operator*=(const OVector2<U>& v);
    template<typename U> OVector2& operator/=(const U& v);
    template<typename U> OVector2& operator/=(const OVector2<U>& v);
    template<typename U> bool operator==(const U& v) const;
    template<typename U> bool operator==(const OVector2<U>& v) const;
    template<typename U> bool operator!=(const U& v) const;
    template<typename U> bool operator!=(const OVector2<U>& v) const;
    template<typename U> bool operator<(const U& v) const;
    template<typename U> bool operator<(const OVector2<U>& v) const;
    template<typename U> bool operator<=(const U& v) const;
    template<typename U> bool operator<=(const OVector2<U>& v) const;
    template<typename U> bool operator>(const U& v) const;
    template<typename U> bool operator>(const OVector2<U>& v) const;
    template<typename U> bool operator>=(const U& v) const;
    template<typename U> bool operator>=(const OVector2<U>& v) const;
    OVector2 operator-(void) const;
    OVector2() = default;
    template<typename U, typename R> OVector2(U x, R y);
    template<typename U> OVector2(const OVector2<U>& v);
    template<typename U> OVector2(OVector2<U>&& v);
    ~OVector2() = default;
};
template<typename T, typename U> utils::type::OVector2<U> operator+(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<U> operator-(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<U> operator*(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<U> operator/(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<U> operator&(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<U> operator|(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<U> operator^(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> bool operator==(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> bool operator!=(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> bool operator<(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> bool operator<=(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> bool operator>(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> bool operator>=(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T> std::ostream& operator<<(std::ostream& os, const utils::type::OVector2<T>& v);
```

## `utils/type/vector/OVector3.hpp`

Vector hat contains 3 value respectivly x, y & z of undefined type Optimized version

Namespace: `utils::type`

```cpp
#define MAX_INDEX_OVECTOR3 3

// namespace utils::type
template<typename T> class OVector3 {
    T x;
    T y;
    T z;
    T get(std::size_t index) const;
    OVector3 min(const OVector3& min) const;
    OVector3 max(const OVector3& max) const;
    OVector3 clamp(const OVector3& min, const OVector3& max) const;
    template<typename U> T dot(const OVector3<U>& v) const;
    template<typename U> OVector3 cross(const OVector3<U>& v) const;
    T length(void) const;
    T lengthSquared(void) const;
    OVector3 sign(void) const;
    OVector3 normalize(void) const;
    T& operator[](std::size_t index);
    const T& operator[](std::size_t index) const;
    template<typename U> OVector3 operator+(const U& v) const;
    template<typename U> OVector3 operator+(const OVector3<U>& v) const;
    template<typename U> OVector3 operator-(const U& v) const;
    template<typename U> OVector3 operator-(const OVector3<U>& v) const;
    template<typename U> OVector3 operator*(const U& v) const;
    template<typename U> OVector3 operator*(const OVector3<U>& v) const;
    template<typename U> OVector3 operator/(const U& v) const;
    template<typename U> OVector3 operator/(const OVector3<U>& v) const;
    OVector3& operator++(void);
    OVector3 operator++(int);
    OVector3& operator--(void);
    OVector3 operator--(int);
    template<typename U> OVector3 operator&(const OVector3<U>& v) const;
    template<typename U> OVector3 operator|(const OVector3<U>& v) const;
    template<typename U> OVector3 operator^(const OVector3<U>& v) const;
    template<typename U> OVector3& operator=(const OVector3<U>& v);
    template<typename U> OVector3& operator=(OVector3<U>&& v);
    template<typename U> OVector3& operator+=(const U& v);
    template<typename U> OVector3& operator+=(const OVector3<U>& v);
    template<typename U> OVector3& operator-=(const U& v);
    template<typename U> OVector3& operator-=(const OVector3<U>& v);
    template<typename U> OVector3& operator*=(const U& v);
    template<typename U> OVector3& operator*=(const OVector3<U>& v);
    template<typename U> OVector3& operator/=(const U& v);
    template<typename U> OVector3& operator/=(const OVector3<U>& v);
    template<typename U> bool operator==(const U& v) const;
    template<typename U> bool operator==(const OVector3<U>& v) const;
    template<typename U> bool operator!=(const U& v) const;
    template<typename U> bool operator!=(const OVector3<U>& v) const;
    template<typename U> bool operator<(const U& v) const;
    template<typename U> bool operator<(const OVector3<U>& v) const;
    template<typename U> bool operator<=(const U& v) const;
    template<typename U> bool operator<=(const OVector3<U>& v) const;
    template<typename U> bool operator>(const U& v) const;
    template<typename U> bool operator>(const OVector3<U>& v) const;
    template<typename U> bool operator>=(const U& v) const;
    template<typename U> bool operator>=(const OVector3<U>& v) const;
    OVector3 operator-(void) const;
    OVector3() = default;
    template<typename U, typename R, typename J> OVector3(U x, R y, J z);
    template<typename U> OVector3(const OVector3<U>& v);
    template<typename U> OVector3(OVector3<U>&& v);
    ~OVector3() = default;
};
template<typename T, typename U> utils::type::OVector3<U> operator+(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<U> operator-(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<U> operator*(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<U> operator/(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<U> operator&(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<U> operator|(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<U> operator^(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> bool operator==(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> bool operator!=(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> bool operator<(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> bool operator<=(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> bool operator>(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> bool operator>=(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T> std::ostream& operator<<(std::ostream& os, const utils::type::OVector3<T>& v);
```

## `utils/type/vector/Vector.hpp`

Include for all the different vector

## `utils/type/vector/Vector2.hpp`

Vector hat contains 2 value respectivly x & y of undefined type

Namespace: `utils::type`

```cpp
#define MAX_INDEX_VECTOR2 2

// namespace utils::type
template<typename T> class Vector2: public utils::type::IVector<T> {
    T x;
    T y;
    T get(std::size_t index) const;
    Vector2 min(const Vector2& min) const requires utils::concepts::Comparable<T>;
    Vector2 max(const Vector2& max) const requires utils::concepts::Comparable<T>;
    Vector2 clamp(const Vector2& min, const Vector2& max) const requires utils::concepts::Comparable<T>;
    template<typename U> std::common_type_t<T, U> dot(const Vector2<U>& v) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddableWith<T, U>;
    template<typename U> std::common_type_t<T, U> cross(const Vector2<U>& v) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::SubtractableWith<T, U>;
    T length(void) const requires utils::concepts::Multipliable<T>;
    T lengthSquared(void) const requires utils::concepts::Multipliable<T> && utils::concepts::Addable<T>;
    Vector2 sign(void) const requires utils::concepts::ComparableWith<T, int>;
    Vector2 normalize(void) const requires utils::concepts::Divisible<T>;
    T& operator[](std::size_t index);
    const T& operator[](std::size_t index) const;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator+(const U& v) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator+(const Vector2<U>& v) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator-(const U& v) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator-(const Vector2<U>& v) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator*(const U& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator*(const Vector2<U>& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator/(const U& v) const requires utils::concepts::DivisibleWith<T, U>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator/(const Vector2<U>& v) const requires utils::concepts::DivisibleWith<T, U>;
    Vector2& operator++(void) requires utils::concepts::Incrementable<T>;
    Vector2 operator++(int) requires utils::concepts::Incrementable<T>;
    Vector2& operator--(void) requires utils::concepts::Decrementable<T>;
    Vector2 operator--(int) requires utils::concepts::Decrementable<T>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator&(const Vector2<U>& v) const requires utils::concepts::BitwiseAndableWith<T, U>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator|(const Vector2<U>& v) const requires utils::concepts::BitwiseOrableWith<T, U>;
    template<typename U> utils::type::Vector2<std::common_type_t<T, U>> operator^(const Vector2<U>& v) const requires utils::concepts::BitwiseXorableWith<T, U>;
    template<typename U> Vector2& operator=(const Vector2<U>& v) requires std::assignable_from<T&, U>;
    template<typename U> Vector2& operator=(Vector2<U>&& v) requires std::assignable_from<T&, U>;
    template<typename U> Vector2& operator+=(const U& v) requires utils::concepts::AddAssignableWith<T, U>;
    template<typename U> Vector2& operator+=(const Vector2<U>& v) requires utils::concepts::AddAssignableWith<T, U>;
    template<typename U> Vector2& operator-=(const U& v) requires utils::concepts::SubtractAssignableWith<T, U>;
    template<typename U> Vector2& operator-=(const Vector2<U>& v) requires utils::concepts::SubtractAssignableWith<T, U>;
    template<typename U> Vector2& operator*=(const U& v) requires utils::concepts::MultiplyAssignableWith<T, U>;
    template<typename U> Vector2& operator*=(const Vector2<U>& v) requires utils::concepts::MultiplyAssignableWith<T, U>;
    template<typename U> Vector2& operator/=(const U& v) requires utils::concepts::DivideAssignableWith<T, U>;
    template<typename U> Vector2& operator/=(const Vector2<U>& v) requires utils::concepts::DivideAssignableWith<T, U>;
    template<typename U> bool operator==(const U& v) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator==(const Vector2<U>& v) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator!=(const U& v) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator!=(const Vector2<U>& v) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator<(const U& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator<(const Vector2<U>& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator<=(const U& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator<=(const Vector2<U>& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator>(const U& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator>(const Vector2<U>& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator>=(const U& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator>=(const Vector2<U>& v) const requires utils::concepts::ComparableWith<T, U>;
    Vector2 operator-(void) const requires utils::concepts::Negatable<T>;
    Vector2() = default;
    template<typename U, typename R> Vector2(U x, R y) requires std::constructible_from<T, U> && std::constructible_from<T, R>;
    template<typename U> Vector2(const Vector2<U>& v) requires std::constructible_from<T, U>;
    template<typename U> Vector2(Vector2<U>&& v) requires std::constructible_from<T, U&&>;
    ~Vector2() = default;
};
template<typename T, typename U> utils::type::Vector2<std::common_type_t<T, U>> operator+(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::AddableWith<T, U>;
template<typename T, typename U> utils::type::Vector2<std::common_type_t<T, U>> operator-(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::SubtractableWith<T, U>;
template<typename T, typename U> utils::type::Vector2<std::common_type_t<T, U>> operator*(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::MultipliableWith<T, U>;
template<typename T, typename U> utils::type::Vector2<std::common_type_t<T, U>> operator/(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::DivisibleWith<T, U>;
template<typename T, typename U> utils::type::Vector2<std::common_type_t<T, U>> operator&(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::BitwiseAndableWith<T, U>;
template<typename T, typename U> utils::type::Vector2<std::common_type_t<T, U>> operator|(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::BitwiseOrableWith<T, U>;
template<typename T, typename U> utils::type::Vector2<std::common_type_t<T, U>> operator^(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::BitwiseXorableWith<T, U>;
template<typename T, typename U> bool operator==(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::EqualityComparableWith<T, U>;
template<typename T, typename U> bool operator!=(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::EqualityComparableWith<T, U>;
template<typename T, typename U> bool operator<(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T, typename U> bool operator<=(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T, typename U> bool operator>(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T, typename U> bool operator>=(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T> std::ostream& operator<<(std::ostream& os, const utils::type::Vector2<T>& v);
```

## `utils/type/vector/Vector3.hpp`

Vector hat contains 3 value respectivly x, y & z of undefined type

Namespace: `utils::type`

```cpp
#define MAX_INDEX_VECTOR3 3

// namespace utils::type
template<typename T> class Vector3: public utils::type::IVector<T> {
    T x;
    T y;
    T z;
    T get(std::size_t index) const;
    Vector3 min(const Vector3& min) const requires utils::concepts::Comparable<T>;
    Vector3 max(const Vector3& max) const requires utils::concepts::Comparable<T>;
    Vector3 clamp(const Vector3& min, const Vector3& max) const requires utils::concepts::Comparable<T>;
    template<typename U> std::common_type_t<T, U> dot(const Vector3<U>& v) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddableWith<T, U>;
    template<typename U> Vector3<std::common_type_t<T, U>> cross(const Vector3<U>& v) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::SubtractableWith<T, U>;
    T length(void) const requires utils::concepts::Multipliable<T>;
    T lengthSquared(void) const requires utils::concepts::Multipliable<T> && utils::concepts::Addable<T>;
    Vector3 sign(void) const requires utils::concepts::ComparableWith<T, int>;
    Vector3 normalize(void) const requires utils::concepts::Divisible<T>;
    T& operator[](std::size_t index);
    const T& operator[](std::size_t index) const;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator+(const U& v) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator+(const Vector3<U>& v) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator-(const U& v) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator-(const Vector3<U>& v) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator*(const U& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator*(const Vector3<U>& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator/(const U& v) const requires utils::concepts::DivisibleWith<T, U>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator/(const Vector3<U>& v) const requires utils::concepts::DivisibleWith<T, U>;
    Vector3& operator++(void) requires utils::concepts::Incrementable<T>;
    Vector3 operator++(int) requires utils::concepts::Incrementable<T>;
    Vector3& operator--(void) requires utils::concepts::Decrementable<T>;
    Vector3 operator--(int) requires utils::concepts::Decrementable<T>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator&(const Vector3<U>& v) const requires utils::concepts::BitwiseAndableWith<T, U>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator|(const Vector3<U>& v) const requires utils::concepts::BitwiseOrableWith<T, U>;
    template<typename U> utils::type::Vector3<std::common_type_t<T, U>> operator^(const Vector3<U>& v) const requires utils::concepts::BitwiseXorableWith<T, U>;
    template<typename U> Vector3& operator=(const Vector3<U>& v) requires std::assignable_from<T&, U>;
    template<typename U> Vector3& operator=(Vector3<U>&& v) requires std::assignable_from<T&, U>;
    template<typename U> Vector3& operator+=(const U& v) requires utils::concepts::AddAssignableWith<T, U>;
    template<typename U> Vector3& operator+=(const Vector3<U>& v) requires utils::concepts::AddAssignableWith<T, U>;
    template<typename U> Vector3& operator-=(const U& v) requires utils::concepts::SubtractAssignableWith<T, U>;
    template<typename U> Vector3& operator-=(const Vector3<U>& v) requires utils::concepts::SubtractAssignableWith<T, U>;
    template<typename U> Vector3& operator*=(const U& v) requires utils::concepts::MultiplyAssignableWith<T, U>;
    template<typename U> Vector3& operator*=(const Vector3<U>& v) requires utils::concepts::MultiplyAssignableWith<T, U>;
    template<typename U> Vector3& operator/=(const U& v) requires utils::concepts::DivideAssignableWith<T, U>;
    template<typename U> Vector3& operator/=(const Vector3<U>& v) requires utils::concepts::DivideAssignableWith<T, U>;
    template<typename U> bool operator==(const U& v) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator==(const Vector3<U>& v) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator!=(const U& v) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator!=(const Vector3<U>& v) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator<(const U& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator<(const Vector3<U>& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator<=(const U& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator<=(const Vector3<U>& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator>(const U& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator>(const Vector3<U>& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator>=(const U& v) const requires utils::concepts::ComparableWith<T, U>;
    template<typename U> bool operator>=(const Vector3<U>& v) const requires utils::concepts::ComparableWith<T, U>;
    Vector3 operator-(void) const requires utils::concepts::Negatable<T>;
    Vector3() = default;
    template<typename U, typename R, typename J> Vector3(U x, R y, J z) requires std::constructible_from<T, U> && std::constructible_from<T, R> && std::constructible_from<T, J>;
    template<typename U> Vector3(const Vector3<U>& v) requires std::constructible_from<T, U>;
    template<typename U> Vector3(Vector3<U>&& v) requires std::constructible_from<T, U&&>;
    ~Vector3() = default;
};
template<typename T, typename U> utils::type::Vector3<std::common_type_t<T, U>> operator+(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::AddableWith<T, U>;
template<typename T, typename U> utils::type::Vector3<std::common_type_t<T, U>> operator-(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::SubtractableWith<T, U>;
template<typename T, typename U> utils::type::Vector3<std::common_type_t<T, U>> operator*(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::MultipliableWith<T, U>;
template<typename T, typename U> utils::type::Vector3<std::common_type_t<T, U>> operator/(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::DivisibleWith<T, U>;
template<typename T, typename U> utils::type::Vector3<std::common_type_t<T, U>> operator&(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::BitwiseAndableWith<T, U>;
template<typename T, typename U> utils::type::Vector3<std::common_type_t<T, U>> operator|(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::BitwiseOrableWith<T, U>;
template<typename T, typename U> utils::type::Vector3<std::common_type_t<T, U>> operator^(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::BitwiseXorableWith<T, U>;
template<typename T, typename U> bool operator==(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::EqualityComparableWith<T, U>;
template<typename T, typename U> bool operator!=(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::EqualityComparableWith<T, U>;
template<typename T, typename U> bool operator<(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T, typename U> bool operator<=(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T, typename U> bool operator>(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T, typename U> bool operator>=(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T> std::ostream& operator<<(std::ostream& os, const utils::type::Vector3<T>& v);
```
