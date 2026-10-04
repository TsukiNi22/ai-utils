# libutils `algorithms`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

## `utils/algorithms/c2dmp-hsm/algorithm/foptimized.hpp`

Algorithm used to determine the distance between 2 words  n = a.size() m = b.size() k = sizeof(UIntT) → can be 1, 2, 4 or 8  Time: best  → O(m + n) moy   → O(m + n) worst → O(m + n)  Memory: best  → O(1) → const (637) moy   → O(1) → const (271 * k + 366) worst → O(1) → const (2534)

Namespace: `utils::algorithms::c2dmp`

```cpp
#define C2DMP_HSM_NORMALIZE_LOOKUP_TABLE
#define C2DMP_HSM_UPPER_LIMIT_LOOKUP_TABLE

// namespace utils::algorithms::c2dmp
static consteval std::array<unsigned char, 256> makeLookupTable_(void);
_alignas(std::hardware_destructive_interference_size) static constexpr std::array<unsigned char, 256> lookupTable = makeLookupTable_(); // case insensitive lookup table
static unsigned char normalize_(const unsigned char c); // unused warning removed: always inlined, never really called
template<std::uint_fast8_t prefixDepthSearch = 3, typename UIntT = std::uint_fast8_t> float c2dmp_foptimized(const std::string_view a, const std::string_view b);
```

## `utils/algorithms/c2dmp-hsm/algorithm/fsimplified.hpp`

Algorithm used to determine the distance between 2 words  n = a.size() m = b.size() k = sizeof(UIntT) → can be 1, 2, 4 or 8  Time: best  → O(m + n) moy   → O(m + n) worst → O(m + n)  Memory: best  → O(1) → const (637) moy   → O(1) → const (271 * k + 366) worst → O(1) → const (2534)

Namespace: `utils::algorithms::c2dmp`

```cpp
#define C2DMP_HSM_NORMALIZE_LOOKUP_TABLE

// namespace utils::algorithms::c2dmp
static consteval std::array<unsigned char, 256> makeLookupTable_(void);
_alignas(std::hardware_destructive_interference_size) static constexpr std::array<unsigned char, 256> lookupTable = makeLookupTable_(); // case insensitive lookup table
static unsigned char normalize_(const unsigned char c); // unused warning removed: always inlined, never really called
template<std::uint_fast8_t prefixDepthSearch = 3, typename UIntT = std::uint_fast8_t> _deprecated("This version isn't the most optimized one, you should use c2dmp_foptimized or c2dmp") float c2dmp_fsimplified(const std::string_view a, const std::string_view b);
```

## `utils/algorithms/c2dmp-hsm/algorithm/optimized.hpp`

Algorithm used to determine the distance between 2 words  n = a.size() m = b.size() k = sizeof(UIntT) → can be 1, 2, 4 or 8  Time: best  → O(m + min(n, m)) moy   → O(m + min(n, m)) worst → O(m + min(n, m))  Memory: best  → O(1) → const (637) moy   → O(1) → const (271 * k + 366) worst → O(1) → const (2534)

Namespace: `utils::algorithms::c2dmp`

```cpp
#define C2DMP_HSM_NORMALIZE_LOOKUP_TABLE
#define C2DMP_HSM_UPPER_LIMIT_LOOKUP_TABLE

// namespace utils::algorithms::c2dmp
static consteval std::array<unsigned char, 256> makeLookupTable_(void);
_alignas(std::hardware_destructive_interference_size) static constexpr std::array<unsigned char, 256> lookupTable = makeLookupTable_(); // case insensitive lookup table
static unsigned char normalize_(const unsigned char c); // unused warning removed: always inlined, never really called
template<std::uint_fast8_t prefixDepthSearch = 3, typename UIntT = std::uint_fast8_t> float c2dmp_optimized(const std::string_view a, const std::string_view b);
```

## `utils/algorithms/c2dmp-hsm/algorithm/simplified.hpp`

Algorithm used to determine the distance between 2 words  n = a.size() m = b.size() k = sizeof(UIntT) → can be 1, 2, 4 or 8  Time: best  → O(m + min(n, m)) moy   → O(m + min(n, m)) worst → O(m + min(n, m))  Memory: best  → O(1) → const (637) moy   → O(1) → const (271 * k + 366) worst → O(1) → const (2534)

Namespace: `utils::algorithms::c2dmp`

```cpp
#define C2DMP_HSM_NORMALIZE_LOOKUP_TABLE

// namespace utils::algorithms::c2dmp
static consteval std::array<unsigned char, 256> makeLookupTable_(void);
_alignas(std::hardware_destructive_interference_size) static constexpr std::array<unsigned char, 256> lookupTable = makeLookupTable_(); // case insensitive lookup table
static unsigned char normalize_(const unsigned char c); // unused warning removed: always inlined, never really called
template<std::uint_fast8_t prefixDepthSearch = 3, typename UIntT = std::uint_fast8_t> _deprecated("This version isn't the most optimized one, you should use c2dmp_optimized or c2dmp") float c2dmp_simplified(const std::string_view a, const std::string_view b);
```

## `utils/algorithms/c2dmp-hsm/c2dmp-hsm.hpp`

Header including all the different algorithms

Namespace: `utils::algorithms::c2dmp`

```cpp
// namespace utils::algorithms::c2dmp
template<std::uint_fast8_t prefixDepthSearch = 3, typename UIntT = std::uint_fast8_t, bool full = false> float c2dmp(const std::string_view a, const std::string_view b);
```

## `utils/algorithms/sos/algorithm/embed_optimized.hpp`

Optimized embed version of the s.o.s algorithm

Namespace: `utils::algorithms::sos::algorithm`

```cpp
// namespace utils::algorithms::sos::algorithm
template<utils::algorithms::sos::Option options = utils::algorithms::sos::Option::None, std::uint8_t magic = MAGIC, typename ByteT> void sos_embed_optimized(std::vector<ByteT>& carrier, const std::vector<ByteT>& payload, const std::optional<std::vector<ByteT>>& key = std::nullopt);
```

## `utils/algorithms/sos/algorithm/extract_optimized.hpp`

Optimized extract version of the s.o.s algorithm

Namespace: `utils::algorithms::sos::algorithm`

```cpp
// namespace utils::algorithms::sos::algorithm
template<std::uint8_t magic = MAGIC, typename ByteT> std::vector<ByteT> sos_extract_optimized(const std::vector<ByteT>& carrier, const std::optional<std::vector<ByteT>>& key = std::nullopt);
```

## `utils/algorithms/sos/sos.hpp`

Header for include all the different algorithm

Namespace: `utils::algorithms::sos`

```cpp
// namespace utils::algorithms::sos
template<utils::algorithms::sos::Option options = utils::algorithms::sos::Option::None, std::uint8_t magic = MAGIC, typename ByteT> void sos_embed(std::vector<ByteT>& carrier, const std::vector<ByteT>& payload);
template<utils::algorithms::sos::Option options = utils::algorithms::sos::Option::None, std::uint8_t magic = MAGIC, typename ByteT> void sos_embed(std::vector<ByteT>& carrier, const std::vector<ByteT>& payload, const std::vector<ByteT>& key);
template<std::uint8_t magic = MAGIC, typename ByteT> std::vector<ByteT> sos_extract(const std::vector<ByteT>& carrier);
template<std::uint8_t magic = MAGIC, typename ByteT> std::vector<ByteT> sos_extract(const std::vector<ByteT>& carrier, const std::vector<ByteT>& key);
```

## `utils/algorithms/sos/sosDefine.hpp`

Different define & macro used for limits and computing

Namespace: `utils::algorithms::sos`

```cpp
#define MAGIC 0x22
#define NOISE_COEF 0.0025 // 0.001 < Studio < 0.005 < Mic < 0.02 (noise coef for amplitude)
#define SEED_ELEMENT_COUNT 256 // number of index used for the seed creation
#define RANGE_PERCENTAGE 0.05 // percentage used for the range usage limits
#define PAYLOAD_PERCENTAGE_LIMIT 0.075 // The payload can only be x percent of the total signal at max
#define UINTN_MIN(ByteT) std::numeric_limits<ByteT>::min()
#define UINTN_MAX(ByteT) std::numeric_limits<ByteT>::max()
#define RMS_LIMIT(ByteT) (250.0 * (static_cast<double>(UINTN_MAX(ByteT)) / static_cast<double>(UINT16_MAX))) // 100 ~ 5000 normal (scaled on a base of uint16_t)
#define THRESHOLD_MIN(ByteT) ((1ull << (sizeof(ByteT) * 8 / 2)) - 1ull)
#define THRESHOLD_MAX(ByteT) (UINTN_MAX(ByteT) - THRESHOLD_MIN(ByteT))
#define RANGE_USED_MIN(ByteT) (std::min(2048.0, static_cast<double>(UINTN_MAX(ByteT)) * RANGE_PERCENTAGE)) // Need at least x percentage of the whole range to ensure some security
#define RANGE_USED_MAX(ByteT) (static_cast<double>(UINTN_MAX(ByteT)) * (1.0 - RANGE_PERCENTAGE)) // Need less than x percentage of the whole range to ensure some security

// namespace utils::algorithms::sos
enum Option {None, Noise, GlobalNoise}
```

## `utils/algorithms/sos/sosType.hpp`

Default type used by the s.o.s algorithm

Namespace: `utils::algorithms::sos`

```cpp
// namespace utils::algorithms::sos
using Byte = std::uint16_t; // Default Byte type
using Bytes = std::vector<utils::algorithms::sos::Byte>;
using Key = utils::algorithms::sos::Bytes;
```

## `utils/algorithms/sos/tools/convert.hpp`

Include of the convertion tools

Namespace: `utils::algorithms::sos::tools`

```cpp
// namespace utils::algorithms::sos::tools
template<typename ByteT = utils::algorithms::sos::Byte, std::ranges::input_range Range> std::vector<ByteT> to_bytes(const Range& range);
template<std::ranges::input_range Range, typename ByteT> Range bytes_to(const std::vector<ByteT>& bytes);
```

## `utils/algorithms/sos/tools/hash.hpp`

Include of the hash generation tools

Namespace: `utils::algorithms::sos::tools`

```cpp
// namespace utils::algorithms::sos::tools
struct DirectSeedSequence {
    using result_type = std::uint32_t;
    const std::array<std::uint32_t, std::mt19937::state_size>& data;
    std::size_t size(void) const noexcept;
    template<typename It> void generate(const It first, const It last) const;
};
template<typename ByteT> std::uint_fast32_t hash(const std::vector<std::uint_fast32_t>& index, const std::vector<ByteT>& bytes);
template<typename ByteT> std::mt19937 make_generator(const std::uint_fast32_t baseSeed, const std::optional<std::vector<ByteT>>& key);
```

## `utils/algorithms/sos/tools/noise.hpp`

Include of the noise generation tools

Namespace: `utils::algorithms::sos::tools`

```cpp
// namespace utils::algorithms::sos::tools
template<typename ByteT> ByteT clamp_to_byte(const double value);
template<typename ByteT> void noise(std::vector<ByteT>& bytes);
template<typename ByteT> void noise(std::vector<ByteT>& bytes, const std::vector<std::uint_fast32_t>& index);
```

## `utils/algorithms/sos/tools/threshold.hpp`

Include of the threshold tools

Namespace: `utils::algorithms::sos::tools`

```cpp
// namespace utils::algorithms::sos::tools
template<typename ByteT> void get_threshold_index(std::vector<std::uint_fast32_t>& index, const std::vector<ByteT>& bytes);
template<typename ByteT> void remove_threshold(std::vector<ByteT>& bytes);
```
