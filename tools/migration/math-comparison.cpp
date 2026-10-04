/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file math-comparison.cpp Same public-API fixture for unchanged C++ and Rust. */

#include "stdafx.h"
#include "core/math_func.hpp"
#include "core/strong_typedef_type.hpp"
#include "core/overflowsafe_type.hpp"
#include "landscape.h"
#include "slope_func.h"
#include <cstdio>

using Tagged = StrongType::Typedef<int64_t, struct MathProbeTag>;
static_assert(ClampTo<uint8_t>(-1) == 0);
static_assert(ClampTo<int8_t>(uint64_t(-1)) == 127);
static_assert(ClampTo<uint64_t>(int64_t(-1)) == 0);
static_assert(ClampTo<uint8_t>(Tagged{256}) == 255);
static_assert(ClampTo<uint8_t>(OverflowSafeInt<int64_t>{256}) == 255);
static_assert(SoftClamp(0, 1500000000, -1500000000) == 0);
static_assert(SoftClamp<int8_t>(0, -1, -3) == 126);
static_assert(SoftClamp<int16_t>(0, -1, -3) == 32766);
static_assert(SoftClamp<uint64_t>(0, UINT64_MAX, 0) == (uint64_t(1) << 63));

#ifdef __SIZEOF_INT128__
using WideUnsigned = unsigned __int128;
static_assert(ClampTo<WideUnsigned>(uint64_t(7)) == 7);
static_assert(ClampTo<WideUnsigned>(UINT64_MAX) == WideUnsigned(UINT64_MAX));
static_assert(ClampTo<WideUnsigned>(true) == 1);
#endif

#ifdef MATH_ROUTE_PROBE
static uint64_t sqrt_calls = 0, clamp_calls = 0, soft_calls = 0;
extern "C" uint32_t __real_openttd_rust_int_sqrt(uint32_t);
extern "C" uint64_t __real_openttd_rust_clamp_to(uint64_t, uint8_t, uint8_t, uint8_t, uint8_t);
extern "C" uint64_t __real_openttd_rust_soft_clamp(uint64_t, uint64_t, uint64_t, uint8_t, uint8_t);
extern "C" uint32_t __wrap_openttd_rust_int_sqrt(uint32_t value)
{
	sqrt_calls++;
	return __real_openttd_rust_int_sqrt(value);
}
extern "C" uint64_t __wrap_openttd_rust_clamp_to(uint64_t value, uint8_t fw, uint8_t fs, uint8_t tw, uint8_t ts)
{
	clamp_calls++;
	return __real_openttd_rust_clamp_to(value, fw, fs, tw, ts);
}
extern "C" uint64_t __wrap_openttd_rust_soft_clamp(uint64_t value, uint64_t min, uint64_t max, uint8_t width, uint8_t sign)
{
	soft_calls++;
	return __real_openttd_rust_soft_clamp(value, min, max, width, sign);
}
#endif

static uint64_t digest = 14695981039346656037ULL, cases = 0;
static void Emit(uint64_t value)
{
	/* Unsigned wrapping is defined; digest includes case order and result bits. */
	std::printf("%llu:%llu\n", static_cast<unsigned long long>(cases), static_cast<unsigned long long>(value));
	digest = (digest ^ value) * 1099511628211ULL;
	cases++;
}

template <typename To, typename From> static void ClampCases()
{
	if constexpr (!(std::is_same_v<From, bool> && std::is_signed_v<To> && sizeof(To) == 1)) {
		const uint64_t bits[] = {0, 1, 2, 127, 128, 255, 256, 32767, 32768, 65535, 65536,
				INT32_MAX, uint64_t(INT32_MAX) + 1, UINT32_MAX, uint64_t(INT64_MAX), uint64_t(INT64_MAX) + 1,
				UINT64_MAX - 1, UINT64_MAX, uint64_t(-127), uint64_t(-128), uint64_t(-129),
				uint64_t(-32767), uint64_t(-32768), uint64_t(-32769),
				uint64_t(-2147483647LL), uint64_t(-2147483648LL), uint64_t(-2147483649LL)};
		for (uint64_t value : bits) Emit(static_cast<uint64_t>(ClampTo<To>(static_cast<From>(value))));
		Emit(static_cast<uint64_t>(ClampTo<To>(std::numeric_limits<From>::lowest())));
		Emit(static_cast<uint64_t>(ClampTo<To>(std::numeric_limits<From>::max())));
		Emit(static_cast<uint64_t>(ClampTo<To>(static_cast<From>(std::numeric_limits<From>::lowest() + 1))));
		Emit(static_cast<uint64_t>(ClampTo<To>(static_cast<From>(std::numeric_limits<From>::max() - 1))));
	}
}

template <typename From> static void ClampDestinations()
{
	ClampCases<int8_t, From>(); ClampCases<uint8_t, From>();
	ClampCases<int16_t, From>(); ClampCases<uint16_t, From>();
	ClampCases<int32_t, From>(); ClampCases<uint32_t, From>();
	ClampCases<int64_t, From>(); ClampCases<uint64_t, From>(); ClampCases<bool, From>();
	ClampCases<char, From>(); ClampCases<wchar_t, From>(); ClampCases<char8_t, From>();
	ClampCases<char16_t, From>(); ClampCases<char32_t, From>();
	ClampCases<size_t, From>(); ClampCases<ptrdiff_t, From>();
}

#ifdef __SIZEOF_INT128__
template <typename From> static void WideUnsignedCases()
{
	const From values[] = {From(0), From(1), From(2), From(std::numeric_limits<From>::max() - 1), std::numeric_limits<From>::max()};
	for (From value : values) {
		const WideUnsigned result = ClampTo<WideUnsigned>(value);
		Emit(static_cast<uint64_t>(result));
		Emit(static_cast<uint64_t>(result >> 64));
	}
}
#endif

template <typename T> static void SoftCases()
{
	const T low = std::numeric_limits<T>::lowest(), high = std::numeric_limits<T>::max();
	const T bounds[] = {low, T(low + 1), T(-3), T(-1), T(0), T(1), T(3), T(high - 1), high};
	for (T min : bounds) for (T max : bounds) for (T value : bounds) Emit(static_cast<uint64_t>(SoftClamp(value, min, max)));
}

struct UnsupportedSlope {};
/* Keep the production fatal dispatch observable without terminating the corpus. */
[[noreturn]] void NOT_REACHED(const std::source_location)
{
	throw UnsupportedSlope{};
}

static bool FitsInt(int64_t value)
{
	return value >= INT32_MIN && value <= INT32_MAX;
}

/* Exclude only signed overflow in expressions actually evaluated by the source.
 * TILE_SIZE/TILE_HEIGHT arithmetic is unsigned and needs no such exclusion. */
static bool DefinedHeightInput(int x, int y, Slope corners)
{
	const int64_t sum = int64_t(x) + y;
	if (IsHalftileSlope(corners)) {
		switch (GetHalftileSlopeCorner(corners)) {
			case CORNER_W: if (x > y) return true; break;
			case CORNER_S: if (!FitsInt(sum)) return false; if (sum >= 16) return true; break;
			case CORNER_E: if (x <= y) return true; break;
			case CORNER_N: if (!FitsInt(sum)) return false; if (sum < 16) return true; break;
			default: return true;
		}
	}
	const auto east = [&] { return FitsInt(1LL + y) && FitsInt(1LL + y - x); };
	const auto west = [&] { return FitsInt(int64_t(x) - y); };
	const auto south = [&] { return FitsInt(1LL + x) && FitsInt(1 + sum); };
	switch (RemoveHalftileSlope(corners)) {
		case SLOPE_N: case SLOPE_WSE: return FitsInt(sum);
		case SLOPE_S: case SLOPE_ENW: return FitsInt(sum) && (sum < 16 || south());
		case SLOPE_NS: return FitsInt(sum) && (sum < 16 || south());
		case SLOPE_E: return y < x || east();
		case SLOPE_NWS: return x >= y || east();
		case SLOPE_W: return x < y || west();
		case SLOPE_SEN: return y >= x || west();
		case SLOPE_EW: return x >= y ? west() : east();
		case SLOPE_SE: return FitsInt(1LL + y);
		case SLOPE_SW: return FitsInt(1LL + x);
		case SLOPE_STEEP_S: return south();
		default: return true;
	}
}

static void HeightCase(int x, int y, Slope corners)
{
	if (!DefinedHeightInput(x, y, corners)) return;
	Emit(static_cast<uint32_t>(x)); Emit(static_cast<uint32_t>(y)); Emit(corners);
	try {
		Emit(GetPartialPixelZ(x, y, corners));
	} catch (const UnsupportedSlope &) {
		Emit(UINT64_MAX);
	}
}

static void HeightCases()
{
	/* Exhaust all slope bytes, including fatal bases and half-tile early returns,
	 * over an extended tile grid, then probe full-width arithmetic boundaries. */
	const int edge[] = {INT32_MIN, INT32_MIN + 1, INT32_MIN + 16, -65536, -17, -2, -1,
			0, 1, 15, 16, 17, 65536, INT32_MAX - 16, INT32_MAX - 1, INT32_MAX};
	for (uint corners = 0; corners <= UINT8_MAX; corners++) {
		for (int x = -32; x <= 32; x++) for (int y = -32; y <= 32; y++) HeightCase(x, y, static_cast<Slope>(corners));
		for (int x : edge) for (int y : edge) HeightCase(x, y, static_cast<Slope>(corners));
	}
}

int main()
{
#ifdef MATH_ROUTE_PROBE
	/* Literal runtime calls must cross Rust even under -O2. */
	Emit(IntSqrt(25)); Emit(ClampTo<uint8_t>(256)); Emit(SoftClamp(0, 1500, 1000));
	if (sqrt_calls != 1 || clamp_calls != 1 || soft_calls != 1) return 2;
#else
	Emit(IntSqrt(25)); Emit(ClampTo<uint8_t>(256)); Emit(SoftClamp(0, 1500, 1000));
#endif
#ifdef __SIZEOF_INT128__
#ifdef MATH_ROUTE_PROBE
	const uint64_t wide_calls_before = clamp_calls;
#endif
	WideUnsignedCases<uint8_t>(); WideUnsignedCases<uint16_t>();
	WideUnsignedCases<uint32_t>(); WideUnsignedCases<uint64_t>(); WideUnsignedCases<bool>();
	WideUnsignedCases<char8_t>(); WideUnsignedCases<char16_t>(); WideUnsignedCases<char32_t>();
	WideUnsignedCases<size_t>();
#ifdef MATH_ROUTE_PROBE
	if (clamp_calls != wide_calls_before + 45) return 4;
#endif
#endif
	HeightCases();
	uint64_t roots = 0;
	for (uint64_t k = 0; k <= 65535; k++) {
		const uint64_t square = k * k;
		const uint64_t values[] = {square, square + k, square + k + 1, square ? square - 1 : 0};
		for (uint64_t value : values) if (value <= UINT32_MAX) { Emit(IntSqrt(static_cast<uint32_t>(value))); roots++; }
	}
	Emit(IntSqrt(UINT32_MAX));
	ClampDestinations<int8_t>(); ClampDestinations<uint8_t>();
	ClampDestinations<int16_t>(); ClampDestinations<uint16_t>();
	ClampDestinations<int32_t>(); ClampDestinations<uint32_t>();
	ClampDestinations<int64_t>(); ClampDestinations<uint64_t>(); ClampDestinations<bool>();
	ClampDestinations<char>(); ClampDestinations<wchar_t>(); ClampDestinations<char8_t>();
	ClampDestinations<char16_t>(); ClampDestinations<char32_t>();
	ClampDestinations<size_t>(); ClampDestinations<ptrdiff_t>();
	Emit(ClampTo<uint8_t>(Tagged{256})); Emit(ClampTo<uint8_t>(OverflowSafeInt<int64_t>{-1}));
	SoftCases<int8_t>(); SoftCases<uint8_t>(); SoftCases<int16_t>(); SoftCases<uint16_t>();
	SoftCases<int32_t>(); SoftCases<uint32_t>(); SoftCases<int64_t>(); SoftCases<uint64_t>();
	/* Exhaust only the bounded narrow signed promotion corner, not all math. */
	for (int min = INT8_MIN; min <= INT8_MAX; min++) for (int max = INT8_MIN; max < min; max++) {
		Emit(static_cast<uint64_t>(SoftClamp<int8_t>(0, static_cast<int8_t>(min), static_cast<int8_t>(max))));
	}
	std::printf("root_cases=%llu cases=%llu digest=%llu\n", static_cast<unsigned long long>(roots),
			static_cast<unsigned long long>(cases), static_cast<unsigned long long>(digest));
#ifdef MATH_ROUTE_PROBE
	std::fflush(stdout);
	std::fprintf(stderr, "Rust routing: sqrt=%llu clamp=%llu soft=%llu\n", static_cast<unsigned long long>(sqrt_calls),
			static_cast<unsigned long long>(clamp_calls), static_cast<unsigned long long>(soft_calls));
	if (!sqrt_calls || !clamp_calls || !soft_calls) return 3;
#endif
}
