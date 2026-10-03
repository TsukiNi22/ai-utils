# libutils `math`

Generated from libutils `v2.14.0` (commit `7506acd`, 2026-10-01) by `scripts/gen_api.py`, do not edit by hand.

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

Prototype for point computing

Namespace: `utils::math::trigo`

```cpp
// namespace utils::math::trigo
utils::math::Type deg_to_rad(utils::math::Angle deg);
utils::math::Angle rad_to_deg(utils::math::Type rad);
```
