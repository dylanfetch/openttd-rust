/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file math-extension-comparison.cpp Library-specific accepted wide integer API. */
#include "stdafx.h"
#include "core/math_func.hpp"
#include <cstdio>

/* The standalone command must match the validated native build width. */
static_assert(sizeof(void *) == OPENTTD_MATH_POINTER_BYTES);

/* numeric_limits permits a destination with observable constructor selection. */
struct IntegerDestination {
	uint32_t value;
	uint32_t constructor_width;
	constexpr IntegerDestination(uint8_t value) : value(value), constructor_width(8) {}
	constexpr IntegerDestination(uint64_t value) : value(static_cast<uint32_t>(value)), constructor_width(64) {}
	constexpr bool operator <(const IntegerDestination &other) const { return this->value < other.value; }
};
namespace std {
	template <> struct numeric_limits<IntegerDestination> {
		static constexpr bool is_integer = true;
		static constexpr bool is_signed = false;
		static constexpr int digits = 32;
		static constexpr IntegerDestination lowest() { return IntegerDestination(uint64_t(0)); }
		static constexpr IntegerDestination max() { return IntegerDestination(uint64_t(UINT32_MAX)); }
	};
}
static_assert(ClampTo<IntegerDestination>(uint8_t(7)).constructor_width == 8);

#ifdef __SIZEOF_INT128__
using U = unsigned __int128;
static_assert(ClampTo<U>(uint64_t(7)) == 7);
static_assert(ClampTo<U>(UINT64_MAX) == U(UINT64_MAX));
static_assert(ClampTo<U>(true) == 1);
static void Emit(U value)
{
	std::printf("%llu:%llu\n", static_cast<unsigned long long>(value >> 64), static_cast<unsigned long long>(value));
}
#ifdef _LIBCPP_VERSION
using S = __int128;
static_assert(std::is_integral_v<S> && std::is_integral_v<U>);
static_assert(ClampTo<S>(int64_t(-1)) == -1);
static_assert(ClampTo<U>(int64_t(-1)) == 0);
static_assert(ClampTo<uint64_t>(U(1) << 100) == UINT64_MAX);
static_assert(SoftClamp<U>(0, std::numeric_limits<U>::max(), 0) == (U(1) << 127));
#endif
#endif

int main()
{
	volatile uint8_t input = 7;
	const auto destination = ClampTo<IntegerDestination>(input);
	std::printf("integer destination: value=%u constructor=%u\n", destination.value, destination.constructor_width);
#ifdef __SIZEOF_INT128__
	volatile uint64_t source = UINT64_MAX;
	Emit(ClampTo<U>(source));
	volatile uint8_t small = UINT8_MAX;
	Emit(ClampTo<U>(small));
	Emit(ClampTo<U>(false)); Emit(ClampTo<U>(true));
#ifdef _LIBCPP_VERSION
	std::printf("libc++ accepted wide inputs, signed destinations and SoftClamp\n");
	volatile U large = (U(1) << 100) + 7;
	volatile S negative = std::numeric_limits<S>::lowest();
	volatile int64_t signed_source = -1;
	Emit(ClampTo<S>(signed_source)); Emit(ClampTo<S>(source)); Emit(ClampTo<U>(signed_source));
	Emit(ClampTo<uint64_t>(large)); Emit(static_cast<U>(ClampTo<int64_t>(large)));
	Emit(ClampTo<uint64_t>(negative)); Emit(static_cast<U>(ClampTo<int64_t>(negative)));
	Emit(ClampTo<S>(large)); Emit(ClampTo<U>(negative));
	Emit(SoftClamp<U>(large, 0, std::numeric_limits<U>::max()));
	Emit(SoftClamp<U>(large, std::numeric_limits<U>::max(), 0));
	Emit(SoftClamp<S>(negative, std::numeric_limits<S>::max(), std::numeric_limits<S>::lowest()));
	Emit(SoftClamp<S>(negative, S(-1), S(-4)));
	Emit(SoftClamp<S>(negative, S(-4), S(-4)));
#else
	std::printf("wide unsigned destination; additional domain is library-dependent\n");
#endif
#else
	std::printf("compiler has no 128-bit extension\n");
#endif
}
