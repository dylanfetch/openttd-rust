/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string_consumer.cpp Implementation of string parsing. */

#include "../stdafx.h"
#include "string_consumer.hpp"

#include "utf8.hpp"
#include "string_builder.hpp"

#include "../string_func.h"

#if defined(STRGEN) || defined(SETTINGSGEN)
#include "../error_func.h"
#else
#include "../debug.h"
#endif

#include "../safeguards.h"

/* static */ const std::string_view StringConsumer::WHITESPACE_NO_NEWLINE = "\t\v\f\r ";
/* static */ const std::string_view StringConsumer::WHITESPACE_OR_NEWLINE = "\t\n\v\f\r ";

/* static */ void StringConsumer::LogError(std::string &&msg)
{
#if defined(STRGEN) || defined(SETTINGSGEN)
	FatalErrorI(std::move(msg));
#else
	DebugPrint("misc", 0, std::move(msg));
#endif
}

std::optional<uint8_t> StringConsumer::PeekUint8() const
{
#ifdef WITH_RUST
	auto result = this->PeekBinary(sizeof(uint8_t));
	if (result.length == 0) return std::nullopt;
	return static_cast<uint8_t>(result.value_bits);
#else
	if (this->GetBytesLeft() < 1) return std::nullopt;
	return static_cast<uint8_t>(this->src[this->position]);
#endif
}

std::optional<uint16_t> StringConsumer::PeekUint16LE() const
{
#ifdef WITH_RUST
	auto result = this->PeekBinary(sizeof(uint16_t));
	if (result.length == 0) return std::nullopt;
	return static_cast<uint16_t>(result.value_bits);
#else
	if (this->GetBytesLeft() < 2) return std::nullopt;
	return static_cast<uint8_t>(this->src[this->position]) |
		static_cast<uint8_t>(this->src[this->position + 1]) << 8;
#endif
}

std::optional<uint32_t> StringConsumer::PeekUint32LE() const
{
#ifdef WITH_RUST
	auto result = this->PeekBinary(sizeof(uint32_t));
	if (result.length == 0) return std::nullopt;
	return static_cast<uint32_t>(result.value_bits);
#else
	if (this->GetBytesLeft() < 4) return std::nullopt;
	return static_cast<uint8_t>(this->src[this->position]) |
		static_cast<uint8_t>(this->src[this->position + 1]) << 8 |
		static_cast<uint8_t>(this->src[this->position + 2]) << 16 |
		static_cast<uint8_t>(this->src[this->position + 3]) << 24;
#endif
}

std::optional<uint64_t> StringConsumer::PeekUint64LE() const
{
#ifdef WITH_RUST
	auto result = this->PeekBinary(sizeof(uint64_t));
	if (result.length == 0) return std::nullopt;
	return static_cast<uint64_t>(result.value_bits);
#else
	if (this->GetBytesLeft() < 8) return std::nullopt;
	return static_cast<uint64_t>(static_cast<uint8_t>(this->src[this->position])) |
		static_cast<uint64_t>(static_cast<uint8_t>(this->src[this->position + 1])) << 8 |
		static_cast<uint64_t>(static_cast<uint8_t>(this->src[this->position + 2])) << 16 |
		static_cast<uint64_t>(static_cast<uint8_t>(this->src[this->position + 3])) << 24 |
		static_cast<uint64_t>(static_cast<uint8_t>(this->src[this->position + 4])) << 32 |
		static_cast<uint64_t>(static_cast<uint8_t>(this->src[this->position + 5])) << 40 |
		static_cast<uint64_t>(static_cast<uint8_t>(this->src[this->position + 6])) << 48 |
		static_cast<uint64_t>(static_cast<uint8_t>(this->src[this->position + 7])) << 56;
#endif
}

std::optional<char> StringConsumer::PeekChar() const
{
	auto result = this->PeekUint8();
	if (!result.has_value()) return {};
	return static_cast<char>(*result);
}

std::pair<StringConsumer::size_type, char32_t> StringConsumer::PeekUtf8() const
{
	auto buf = this->src.substr(this->position);
	return DecodeUtf8(buf);
}

std::string_view StringConsumer::Peek(size_type len) const
{
#ifdef WITH_RUST
	auto bounds = openttd_rust_consumer_bound(this->src.size(), this->position, len);
	return this->src.substr(this->position, bounds.length);
#else
	auto buf = this->src.substr(this->position);
	if (len == std::string_view::npos) {
		len = buf.size();
	} else if (len > buf.size()) {
		len = buf.size();
	}
	return buf.substr(0, len);
#endif
}

void StringConsumer::Skip(size_type len)
{
#ifdef WITH_RUST
	auto bounds = openttd_rust_consumer_bound(this->src.size(), this->position, len);
	if (bounds.shortfall) LogError(fmt::format("Source buffer too short: {} > {}", len, bounds.length));
	this->position = bounds.position; // No cursor change before a fatal logger.
#else
	if (len == std::string_view::npos) {
		this->position = this->src.size();
	} else if (size_type max_len = GetBytesLeft(); len > max_len) {
		LogError(fmt::format("Source buffer too short: {} > {}", len, max_len));
		this->position = this->src.size();
	} else {
		this->position += len;
	}
#endif
}

StringConsumer::size_type StringConsumer::Find(std::string_view str) const
{
#ifdef WITH_RUST
	assert(!str.empty());
	return this->FindBytes(str, 0);
#else
	assert(!str.empty());
	auto buf = this->src.substr(this->position);
	return buf.find(str);
#endif
}

StringConsumer::size_type StringConsumer::FindUtf8(char32_t c) const
{
	auto [data, len] = EncodeUtf8(c);
	return this->Find({data, len});
}

StringConsumer::size_type StringConsumer::FindCharIn(std::string_view chars) const
{
#ifdef WITH_RUST
	assert(!chars.empty());
	return this->FindBytes(chars, 1);
#else
	assert(!chars.empty());
	auto buf = this->src.substr(this->position);
	return buf.find_first_of(chars);
#endif
}

StringConsumer::size_type StringConsumer::FindCharNotIn(std::string_view chars) const
{
#ifdef WITH_RUST
	assert(!chars.empty());
	return this->FindBytes(chars, 2);
#else
	assert(!chars.empty());
	auto buf = this->src.substr(this->position);
	return buf.find_first_not_of(chars);
#endif
}

std::string_view StringConsumer::PeekUntil(std::string_view str, SeparatorUsage sep) const
{
#ifdef WITH_RUST
	assert(!str.empty());
	auto lengths = this->SeparatorResult(str, sep);
	return this->src.substr(this->position, lengths.result_length);
#else
	assert(!str.empty());
	auto buf = this->src.substr(this->position);
	auto len = buf.find(str);
	if (len != std::string_view::npos) {
		switch (sep) {
			case READ_ONE_SEPARATOR:
				if (buf.compare(len, str.size(), str) == 0) len += str.size();
				break;
			case READ_ALL_SEPARATORS:
				while (buf.compare(len, str.size(), str) == 0) len += str.size();
				break;
			default:
				break;
		}
	}
	return buf.substr(0, len);
#endif
}

std::string_view StringConsumer::PeekUntilUtf8(char32_t c, SeparatorUsage sep) const
{
	auto [data, len] = EncodeUtf8(c);
	return PeekUntil({data, len}, sep);
}

std::string_view StringConsumer::ReadUntilUtf8(char32_t c, SeparatorUsage sep)
{
	auto [data, len] = EncodeUtf8(c);
	return ReadUntil({data, len}, sep);
}

void StringConsumer::SkipUntilUtf8(char32_t c, SeparatorUsage sep)
{
	auto [data, len] = EncodeUtf8(c);
	return SkipUntil({data, len}, sep);
}

void StringConsumer::SkipIntegerBase(int base)
{
#ifdef WITH_RUST
	assert(base == 0 || base == 8 || base == 10 || base == 16);
	auto input = this->GetLeftData();
	this->Skip(openttd_rust_skip_integer(reinterpret_cast<const uint8_t *>(input.data()), input.size(), base));
#else
	this->SkipIf("-");
	if (base == 0) {
		if (this->ReadIf("0x") || this->ReadIf("0X")) { // boolean short-circuit ensures only one prefix is read
			base = 16;
		} else {
			base = 10;
		}
	}
	switch (base) {
		default:
			assert(false);
			break;
		case 8:
			this->SkipUntilCharNotIn("01234567");
			break;
		case 10:
			this->SkipUntilCharNotIn("0123456789");
			break;
		case 16:
			this->SkipUntilCharNotIn("0123456789abcdefABCDEF");
			break;
	}
#endif
}
