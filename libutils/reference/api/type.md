# libutils `type`

Generated from libutils `v2.14.0` (commit `7506acd`, 2026-10-01) by `scripts/gen_api.py`, do not edit by hand.

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
    Freezable(const bool frozen);
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
    void removeElement(const T& element) noexcept;
    void removeElements(const std::vector<T>& elements) noexcept;
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
    template<typename U> Matrix<std::common_type_t<T, U>> operator+(const Matrix<U>& m) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> Matrix<std::common_type_t<T, U>> operator-(const Matrix<U>& m) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> Matrix<std::common_type_t<T, U>> operator*(const Matrix<U>& m) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddableWith<T, U>;
    template<typename U> Matrix<std::common_type_t<T, U>> operator/(const Matrix<U>& m) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddableWith<T, U> && (!std::is_integral_v<std::common_type_t<T, U>>); // this * m^-1
    template<typename U> Matrix<std::common_type_t<T, U>> operator*(const U& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> Matrix<std::common_type_t<T, U>> operator/(const U& v) const requires utils::concepts::DivisibleWith<T, U>;
    Matrix& operator=(const Matrix& other) = default;
    Matrix& operator=(Matrix&& other);
    template<typename U> Matrix& operator+=(const Matrix<U>& m) requires utils::concepts::AddAssignableWith<T, U>;
    template<typename U> Matrix& operator-=(const Matrix<U>& m) requires utils::concepts::SubtractAssignableWith<T, U>;
    template<typename U> Matrix& operator*=(const Matrix<U>& m) requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddAssignable<T>;
    template<typename U> Matrix& operator/=(const Matrix<U>& m) requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddAssignable<T> && (!std::is_integral_v<std::common_type_t<T, U>>); // this * m^-1
    template<typename U> Matrix& operator*=(const U& v) requires utils::concepts::MultiplyAssignableWith<T, U>;
    template<typename U> Matrix& operator/=(const U& v) requires utils::concepts::DivideAssignableWith<T, U>;
    template<typename U> bool operator==(const Matrix<U>& m) const requires utils::concepts::EqualityComparableWith<T, U>;
    template<typename U> bool operator!=(const Matrix<U>& m) const requires utils::concepts::EqualityComparableWith<T, U>;
    Matrix operator-(void) const requires utils::concepts::Negatable<T>;
    explicit Matrix(const std::size_t n);
    Matrix(const std::size_t x, const std::size_t y);
    template<typename U> Matrix(const Matrix<U>& m) requires std::constructible_from<T, U>;
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
    template<typename U> OMatrix operator+(const OMatrix<U>& m) const;
    template<typename U> OMatrix operator-(const OMatrix<U>& m) const;
    template<typename U> OMatrix operator*(const OMatrix<U>& m) const;
    template<typename U> OMatrix operator/(const OMatrix<U>& m) const; // this * m^-1
    template<typename U> OMatrix operator*(const U& v) const;
    template<typename U> OMatrix operator/(const U& v) const;
    OMatrix& operator=(const OMatrix& other) = default;
    OMatrix& operator=(OMatrix&& other) noexcept;
    template<typename U> OMatrix& operator+=(const OMatrix<U>& m);
    template<typename U> OMatrix& operator-=(const OMatrix<U>& m);
    template<typename U> OMatrix& operator*=(const OMatrix<U>& m);
    template<typename U> OMatrix& operator/=(const OMatrix<U>& m);
    template<typename U> OMatrix& operator*=(const U& v);
    template<typename U> OMatrix& operator/=(const U& v);
    template<typename U> bool operator==(const OMatrix<U>& m) const;
    template<typename U> bool operator!=(const OMatrix<U>& m) const;
    OMatrix operator-(void) const;
    explicit OMatrix(const std::size_t n);
    OMatrix(const std::size_t x, const std::size_t y);
    template<typename U> OMatrix(const OMatrix<U>& m);
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
template<typename T, typename U> utils::type::OVector2<T> operator+(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<T> operator-(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<T> operator*(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<T> operator/(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<T> operator&(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<T> operator|(const T& lhs, const utils::type::OVector2<U>& rhs);
template<typename T, typename U> utils::type::OVector2<T> operator^(const T& lhs, const utils::type::OVector2<U>& rhs);
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
template<typename T, typename U> utils::type::OVector3<T> operator+(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<T> operator-(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<T> operator*(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<T> operator/(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<T> operator&(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<T> operator|(const T& lhs, const utils::type::OVector3<U>& rhs);
template<typename T, typename U> utils::type::OVector3<T> operator^(const T& lhs, const utils::type::OVector3<U>& rhs);
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
    template<typename U> T dot(const Vector2<U>& v) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddableWith<T, U>;
    template<typename U> T cross(const Vector2<U>& v) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::SubtractableWith<T, U>;
    T length(void) const requires utils::concepts::Multipliable<T>;
    T lengthSquared(void) const requires utils::concepts::Multipliable<T> && utils::concepts::Addable<T>;
    Vector2 sign(void) const requires utils::concepts::ComparableWith<T, int>;
    Vector2 normalize(void) const requires utils::concepts::Divisible<T>;
    T& operator[](std::size_t index);
    const T& operator[](std::size_t index) const;
    template<typename U> auto operator+(const U& v) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> auto operator+(const Vector2<U>& v) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> auto operator-(const U& v) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> auto operator-(const Vector2<U>& v) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> auto operator*(const U& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> auto operator*(const Vector2<U>& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> auto operator/(const U& v) const requires utils::concepts::DivisibleWith<T, U>;
    template<typename U> auto operator/(const Vector2<U>& v) const requires utils::concepts::DivisibleWith<T, U>;
    Vector2& operator++(void) requires utils::concepts::Incrementable<T>;
    Vector2 operator++(int) requires utils::concepts::Incrementable<T>;
    Vector2& operator--(void) requires utils::concepts::Decrementable<T>;
    Vector2 operator--(int) requires utils::concepts::Decrementable<T>;
    template<typename U> auto operator&(const Vector2<U>& v) const requires utils::concepts::BitwiseAndableWith<T, U>;
    template<typename U> auto operator|(const Vector2<U>& v) const requires utils::concepts::BitwiseOrableWith<T, U>;
    template<typename U> auto operator^(const Vector2<U>& v) const requires utils::concepts::BitwiseXorableWith<T, U>;
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
template<typename T, typename U> auto operator+(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::AddableWith<T, U>;
template<typename T, typename U> auto operator-(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::SubtractableWith<T, U>;
template<typename T, typename U> auto operator*(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::MultipliableWith<T, U>;
template<typename T, typename U> auto operator/(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::DivisibleWith<T, U>;
template<typename T, typename U> auto operator&(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::BitwiseAndableWith<T, U>;
template<typename T, typename U> auto operator|(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::BitwiseOrableWith<T, U>;
template<typename T, typename U> auto operator^(const T& lhs, const utils::type::Vector2<U>& rhs) requires utils::concepts::BitwiseXorableWith<T, U>;
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
    template<typename U> T dot(const Vector3<U>& v) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::AddableWith<T, U>;
    template<typename U> Vector3 cross(const Vector3<U>& v) const requires utils::concepts::MultipliableWith<T, U> && utils::concepts::SubtractableWith<T, U>;
    T length(void) const requires utils::concepts::Multipliable<T>;
    T lengthSquared(void) const requires utils::concepts::Multipliable<T> && utils::concepts::Addable<T>;
    Vector3 sign(void) const requires utils::concepts::ComparableWith<T, int>;
    Vector3 normalize(void) const requires utils::concepts::Divisible<T>;
    T& operator[](std::size_t index);
    const T& operator[](std::size_t index) const;
    template<typename U> auto operator+(const U& v) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> auto operator+(const Vector3<U>& v) const requires utils::concepts::AddableWith<T, U>;
    template<typename U> auto operator-(const U& v) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> auto operator-(const Vector3<U>& v) const requires utils::concepts::SubtractableWith<T, U>;
    template<typename U> auto operator*(const U& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> auto operator*(const Vector3<U>& v) const requires utils::concepts::MultipliableWith<T, U>;
    template<typename U> auto operator/(const U& v) const requires utils::concepts::DivisibleWith<T, U>;
    template<typename U> auto operator/(const Vector3<U>& v) const requires utils::concepts::DivisibleWith<T, U>;
    Vector3& operator++(void) requires utils::concepts::Incrementable<T>;
    Vector3 operator++(int) requires utils::concepts::Incrementable<T>;
    Vector3& operator--(void) requires utils::concepts::Decrementable<T>;
    Vector3 operator--(int) requires utils::concepts::Decrementable<T>;
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
template<typename T, typename U> auto operator+(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::AddableWith<T, U>;
template<typename T, typename U> auto operator-(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::SubtractableWith<T, U>;
template<typename T, typename U> auto operator*(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::MultipliableWith<T, U>;
template<typename T, typename U> auto operator/(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::DivisibleWith<T, U>;
template<typename T, typename U> auto operator&(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::BitwiseAndableWith<T, U>;
template<typename T, typename U> auto operator|(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::BitwiseOrableWith<T, U>;
template<typename T, typename U> auto operator^(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::BitwiseXorableWith<T, U>;
template<typename T, typename U> bool operator==(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::EqualityComparableWith<T, U>;
template<typename T, typename U> bool operator!=(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::EqualityComparableWith<T, U>;
template<typename T, typename U> bool operator<(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T, typename U> bool operator<=(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T, typename U> bool operator>(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T, typename U> bool operator>=(const T& lhs, const utils::type::Vector3<U>& rhs) requires utils::concepts::ComparableWith<T, U>;
template<typename T> std::ostream& operator<<(std::ostream& os, const utils::type::Vector3<T>& v);
```
