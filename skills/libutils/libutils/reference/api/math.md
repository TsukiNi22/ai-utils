# libutils `math`

Generated from libutils `v5.0.0` (commit `6aac89f`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/math/CApi.h`

C API of utils::math: the math types (utils::math::Type = double), geometry & trigonometry (C & C++ compatible) The structures are given by value (same fields as the utils::math types), the math functions can't fail

```c
// global
typedef double utils_math_type_t; // utils::math::Type: coord, angle, direction, ...
typedef uint64_t utils_math_utype_t; // utils::math::UType: unsigned coord, angle, direction, ...
typedef utils_math_type_t utils_math_angle_t; // utils::math::Angle: generaly in deg
typedef struct utils_math_coord2d { utils_math_type_t x; utils_math_type_t y; };
typedef struct utils_math_ucoord2d { utils_math_utype_t x; utils_math_utype_t y; };
typedef struct utils_math_coord { utils_math_type_t x; utils_math_type_t y; utils_math_type_t z; };
typedef struct utils_math_ucoord { utils_math_utype_t x; utils_math_utype_t y; utils_math_utype_t z; };
typedef utils_math_coord_t utils_math_direction_t; // utils::math::Direction: generaly normalized
typedef struct utils_math_chunk { int32_t x; int32_t y; int32_t z; };
typedef struct utils_math_cframe { utils_math_coord_t position; utils_math_direction_t orientation; utils_math_direction_t look; };
utils_math_type_t utils_math_deg_to_rad(utils_math_angle_t deg);
utils_math_angle_t utils_math_rad_to_deg(utils_math_type_t rad);
utils_math_direction_t utils_math_to_look(utils_math_direction_t orientation); // orientation in deg -> look vector normalized
utils_math_coord2d_t utils_math_rotate_point_2d(utils_math_coord2d_t origin, utils_math_coord2d_t point, utils_math_angle_t angle, bool rad);
utils_math_coord_t utils_math_rotate_point_3d(utils_math_coord_t origin, utils_math_coord_t point, utils_math_direction_t orientation, bool rad);
```

## `utils/math/MathType.hpp`

Type definition used in math computing

Namespace: `utils::math`

```cpp
// namespace utils::math
using Type = double; // Type used everywhere for coord, angle, direction, ...
using UType = std::uint64_t; // Type used everywhere for unsigned coord, angle, direction, ...
using V2Type = utils::type::OVector2<utils::math::Type>;
using Coord2D = utils::type::OVector2<utils::math::Type>;
using V2UType = utils::type::OVector2<utils::math::UType>;
using UCoord2D = utils::type::OVector2<utils::math::UType>;
using V3Type = utils::type::OVector3<utils::math::Type>;
using Coord = utils::type::OVector3<utils::math::Type>;
using V3UType = utils::type::OVector3<utils::math::UType>;
using UCoord = utils::type::OVector3<utils::math::UType>;
using Angle = utils::math::Type; // Generaly in deg
using Direction = utils::type::OVector3<utils::math::Angle>; // Generaly normalized
using Chunk = utils::type::OVector3<std::int32_t>; // Used for spacial partitionning
struct CFrame {
    utils::math::Coord position = {0.0, 0.0, 0.0}; // Coord
    utils::math::Direction orientation = {0.0, 0.0, 0.0}; // Orientation in deg
    utils::math::Direction look = {0.0, 0.0, 0.0}; // Orientation normalized
};
```

## `utils/math/geometry/Angle.hpp`

Prototype for angle computing

Namespace: `utils::math::geometry`

```cpp
// namespace utils::math::geometry
utils::math::Direction to_look(const utils::math::Direction& orientation);
```

## `utils/math/geometry/Point.hpp`

Prototype for point computing

Namespace: `utils::math::geometry`

```cpp
// namespace utils::math::geometry
utils::math::Coord2D rotate_point_2D(const utils::math::Coord2D& origin, const utils::math::Coord2D& point, utils::math::Angle angle, const bool rad = false);
utils::math::Coord rotate_point_3D(const utils::math::Coord& origin, const utils::math::Coord& point, const utils::math::Direction& orientation, const bool rad = false);
```

## `utils/math/trigo/Convertion.hpp`

Angle unit conversion (deg <-> rad)

Namespace: `utils::math::trigo`

```cpp
// namespace utils::math::trigo
utils::math::Type deg_to_rad(utils::math::Angle deg);
utils::math::Angle rad_to_deg(utils::math::Type rad);
```
