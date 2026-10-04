# libutils `concepts`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

## `utils/concepts/GlobalConcepts.hpp`

Definition of the different global concepts

Namespace: `utils::concepts`

```cpp
// namespace utils::concepts
template<typename T, typename U> concept Convertible = requires(T a, U b);
template<typename T> concept Swappable = requires(T a, T b);
template<typename T> concept Streamable = requires(std::ostream& os, T a);
template<typename T, typename U> concept ConvertibleTo = std::is_convertible_v<T, U> && requires { static_cast<U>(std::declval<T>()); };
```

## `utils/concepts/OperationConcepts.hpp`

Definition of the different operation concepts

Namespace: `utils::concepts`

```cpp
// namespace utils::concepts
template<typename T> concept Addable = requires(T a, T b);
template<typename T, typename U> concept AddableWith = requires(T a, U b);
template<typename T> concept Subtractable = requires(T a, T b);
template<typename T, typename U> concept SubtractableWith = requires(T a, U b);
template<typename T> concept Multipliable = requires(T a, T b);
template<typename T, typename U> concept MultipliableWith = requires(T a, U b);
template<typename T> concept Divisible = requires(T a, T b);
template<typename T, typename U> concept DivisibleWith = requires(T a, U b);
template<typename T> concept Incrementable = requires(T& a);
template<typename T> concept Decrementable = requires(T& a);
template<typename T> concept BitwiseAndable = requires(T a, T b);
template<typename T, typename U> concept BitwiseAndableWith = requires(T a, U b);
template<typename T> concept BitwiseOrable = requires(T a, T b);
template<typename T, typename U> concept BitwiseOrableWith = requires(T a, U b);
template<typename T> concept BitwiseXorable = requires(T a, T b);
template<typename T, typename U> concept BitwiseXorableWith = requires(T a, U b);
template<typename T> concept Shiftable = requires(T a, T b);
template<typename T, typename U> concept ShiftableWith = requires(T a, U b);
template<typename T> concept AddAssignable = requires(T a, T b);
template<typename T, typename U> concept AddAssignableWith = requires(T a, U b);
template<typename T> concept SubtractAssignable = requires(T a, T b);
template<typename T, typename U> concept SubtractAssignableWith = requires(T a, U b);
template<typename T> concept MultiplyAssignable = requires(T a, T b);
template<typename T, typename U> concept MultiplyAssignableWith = requires(T a, U b);
template<typename T> concept DivideAssignable = requires(T a, T b);
template<typename T, typename U> concept DivideAssignableWith = requires(T a, U b);
template<typename T> concept EqualityComparable = requires(T a, T b);
template<typename T, typename U> concept EqualityComparableWith = requires(T a, U b);
template<typename T> concept Comparable = requires(T a, T b);
template<typename T, typename U> concept ComparableWith = requires(T a, U b);
template<typename T> concept Negatable = requires(T a);
```
