/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_bytes.hpp Native storage facades over Rust-owned town bytes. */
#ifndef RUST_TOWN_BYTES_HPP
#define RUST_TOWN_BYTES_HPP
#include "town_ffi.h"
#include "../strings_type.h"
/** Custom-name storage is Rust-owned. This native string is derived presentation
 * storage only: its stable object preserves GetCachedName's const-reference
 * contract across assignment/reset, and is never read for simulation policy. */
class RustTownName {
	OpenTTDTownState *owner;
	std::string cached{};
public:
	explicit RustTownName(OpenTTDTownState *owner) : owner(owner) {}
	bool empty() const { return openttd_rust_town_name(this->owner).size == 0; }
	std::string_view view() const
	{
		auto bytes = openttd_rust_town_name(this->owner);
		return {reinterpret_cast<const char *>(bytes.data), bytes.size};
	}
	RustTownName &operator=(std::string_view value)
	{
		openttd_rust_town_set_name(this->owner, {reinterpret_cast<const uint8_t *>(value.data()), value.size()});
		this->cached.assign(this->view());
		return *this;
	}
	void clear() { *this = std::string_view{}; }
	operator const std::string &() const { return this->cached; }
	friend bool operator==(const RustTownName &lhs, std::string_view rhs) { return lhs.view() == rhs; }
	friend bool operator==(std::string_view lhs, const RustTownName &rhs) { return lhs == rhs.view(); }
};
/** Encoded text has no native authoritative copy. Decoding makes the same cold
 * temporary encoded string as presentation code needs, after copying its bytes. */
class RustTownText {
	OpenTTDTownState *owner;
public:
	explicit RustTownText(OpenTTDTownState *owner) : owner(owner) {}
	bool empty() const { return openttd_rust_town_text(this->owner).size == 0; }
	EncodedString encoded() const
	{
		auto bytes = openttd_rust_town_text(this->owner);
		return EncodedString::FromEncodedView({reinterpret_cast<const char *>(bytes.data), bytes.size});
	}
	std::string GetDecodedString() const { return this->encoded().GetDecodedString(); }
	void clear() { openttd_rust_town_set_text(this->owner, {nullptr, 0}); }
	RustTownText &operator=(const EncodedString &value)
	{
		auto bytes = value.GetEncodedView();
		openttd_rust_town_set_text(this->owner, {reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()});
		return *this;
	}
};
#endif /* RUST_TOWN_BYTES_HPP */
