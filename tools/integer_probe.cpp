/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file integer_probe.cpp Narrow oracle probe; compile against unchanged reference or candidate headers/source. */

#include "stdafx.h"
#include "core/string_consumer.hpp"
#include "core/string_builder.hpp"
#include <iostream>
#include <iomanip>

static StringConsumer *logging_consumer = nullptr;
static std::vector<std::pair<size_t, std::string>> messages;

static std::string Hex(std::string_view bytes)
{
	std::string result;
	for (unsigned char byte : bytes) {
		result += "0123456789abcdef"[byte >> 4];
		result += "0123456789abcdef"[byte & 15];
	}
	return result;
}

void DebugPrint(std::string_view, int, std::string &&message)
{
	messages.emplace_back(logging_consumer->GetBytesRead(), std::move(message));
}

#if defined(STRGEN) || defined(SETTINGSGEN)
[[noreturn]] void FatalErrorI(const std::string &message)
{
	std::cout << "fatal " << logging_consumer->GetBytesRead() << ' ' << Hex(message) << '\n';
	std::exit(2);
}
#endif

template <typename T>
static uint64_t Bits(T value)
{
	return static_cast<std::make_unsigned_t<T>>(value);
}

template <typename T>
static void Probe(std::string_view src, int base, bool clamp)
{
	StringConsumer peek(src), attempt(src), read(src), skip(src);
	auto [len, value] = peek.PeekIntegerBase<T>(base, clamp);
	auto tried = attempt.TryReadIntegerBase<T>(base, clamp);
	logging_consumer = &read;
	messages.clear();
	T result = read.ReadIntegerBase<T>(base, 42, clamp);
	skip.SkipIntegerBase(base);
	auto whole = ParseInteger<T>(src, base, clamp);
	std::cout << len << ' ' << Bits(value) << ' ' << peek.GetBytesRead() << ' '
		<< tried.has_value() << ' ' << Bits(tried.value_or(42)) << ' ' << attempt.GetBytesRead() << ' '
		<< Bits(result) << ' ' << read.GetBytesRead() << ' ' << skip.GetBytesRead() << ' '
		<< whole.has_value() << ' ' << Bits(whole.value_or(42)) << ' ' << messages.size();
	for (const auto &[position, message] : messages) std::cout << ' ' << position << ' ' << Hex(message);
	std::cout << '\n';
}

/** Record each synchronous virtual call, including zero-length calls. */
class RecordingBuilder final : public BaseStringBuilder {
public:
	struct Event {
		bool inside_call;
		size_t phase;
		std::string bytes;
	};
	bool active = false;
	size_t phase = 0;
	std::vector<Event> events;

	void PutBuffer(std::span<const char> bytes) override
	{
		this->events.push_back({this->active, this->phase, {bytes.data(), bytes.size()}});
	}
};

static void DumpBuilder(const RecordingBuilder &builder)
{
	std::cout << builder.events.size();
	for (const auto &event : builder.events) {
		auto hex = Hex(event.bytes);
		std::cout << ' ' << event.inside_call << ' ' << event.phase << ' ' << event.bytes.size() << ' ' << (hex.empty() ? "-" : hex);
	}
	std::cout << '\n';
}

template <typename T>
static void FormatProbe(uint64_t bits, int base)
{
	using Unsigned = std::make_unsigned_t<T>;
	T value = std::bit_cast<T>(static_cast<Unsigned>(bits));
	RecordingBuilder builder;
	builder.active = true;
	builder.PutIntegerBase<T>(value, base);
	builder.active = false;
	DumpBuilder(builder);
}

static int BuilderMain()
{
	unsigned width, is_signed;
	int base;
	std::string bits_hex;
	while (std::cin >> width >> is_signed >> base >> bits_hex) {
		uint64_t bits = std::stoull(bits_hex, nullptr, 16);
		if (is_signed) {
			switch (width) {
				case 8: FormatProbe<int8_t>(bits, base); break;
				case 16: FormatProbe<int16_t>(bits, base); break;
				case 32: FormatProbe<int32_t>(bits, base); break;
				case 64: FormatProbe<int64_t>(bits, base); break;
			}
		} else {
			switch (width) {
				case 8: FormatProbe<uint8_t>(bits, base); break;
				case 16: FormatProbe<uint16_t>(bits, base); break;
				case 32: FormatProbe<uint32_t>(bits, base); break;
				case 64: FormatProbe<uint64_t>(bits, base); break;
			}
		}
	}
	// The standard integer aliases include types distinct from uint64_t on this
	// host (long long), plus plain char's native signedness and promoted characters.
	std::cout << "char "; FormatProbe<char>(0x80, 10);
	std::cout << "unsigned-short "; FormatProbe<unsigned short>(UINT16_MAX, 36);
	std::cout << "long-long "; FormatProbe<long long>(uint64_t{1} << 63, 36);
	std::cout << "unsigned-long-long "; FormatProbe<unsigned long long>(UINT64_MAX, 36);
	std::cout << "char16 "; FormatProbe<char16_t>(UINT16_MAX, 16);
	std::cout << "char32 "; FormatProbe<char32_t>(UINT32_MAX, 16);

	// Unscoped enums were accepted by std::to_chars through integral promotion.
	enum SignedEnum : int64_t { NegativeEnum = INT64_MIN };
	enum UnsignedEnum : uint64_t { MaximumEnum = UINT64_MAX };
	enum BoolEnum : bool { TrueEnum = true };
	auto converted_probe = [](std::string_view label, auto value) {
		RecordingBuilder builder;
		builder.active = true;
		builder.PutIntegerBase(value, 36);
		builder.active = false;
		std::cout << label << ' ';
		DumpBuilder(builder);
	};
	converted_probe("signed-enum", NegativeEnum);
	converted_probe("unsigned-enum", MaximumEnum);
	converted_probe("bool-enum", TrueEnum);
	struct ImplicitSigned { operator int() const { return -42; } };
	struct ImplicitUnsigned { operator uint64_t() const { return UINT64_MAX; } };
	struct OverloadedPlus {
		operator int() const { return 42; }
		int operator+() const { return 999; }
	};
	converted_probe("implicit-signed", ImplicitSigned{});
	converted_probe("implicit-unsigned", ImplicitUnsigned{});
	converted_probe("overloaded-plus", OverloadedPlus{});

	RecordingBuilder sequence;
	auto call = [&](auto operation) {
		++sequence.phase;
		sequence.active = true;
		operation();
		sequence.active = false;
	};
	call([&] { sequence.PutUint8(0); });
	call([&] { sequence.PutSint8(INT8_MIN); });
	call([&] { sequence.PutUint8(UINT8_MAX); });
	call([&] { sequence.PutUint16LE(0); });
	call([&] { sequence.PutSint16LE(INT16_MIN); });
	call([&] { sequence.PutUint16LE(UINT16_MAX); });
	call([&] { sequence.PutUint32LE(0); });
	call([&] { sequence.PutSint32LE(INT32_MIN); });
	call([&] { sequence.PutUint32LE(UINT32_MAX); });
	call([&] { sequence.PutUint64LE(0); });
	call([&] { sequence.PutSint64LE(INT64_MIN); });
	call([&] { sequence.PutUint64LE(UINT64_MAX); });
	call([&] { sequence.PutChar(static_cast<char>(0xFF)); });
	call([&] { sequence.PutUtf8(0); });
	call([&] { sequence.PutUtf8(0xD800); });
	call([&] { sequence.PutUtf8(0x10FFFF); });
	call([&] { sequence.PutUtf8(0x110000); }); // One zero-length call, distinct from formatting failure.
	call([&] { sequence.PutUtf8(UINT32_MAX); });
	call([&] { sequence.PutIntegerBase<uint64_t>(uint64_t{1} << 32, 2); }); // 33 bytes: no call.
	call([&] { sequence.PutIntegerBase<uint32_t>(UINT32_MAX, 2); }); // Exactly 32 bytes: one call.
	call([&] { sequence.PutIntegerBase<int32_t>(INT32_MIN, 2); }); // Minus sign makes 33: no call.
	call([&] { sequence.PutIntegerBase<int32_t>(INT32_MIN + 1, 2); }); // Minus sign makes 32: one call.
	sequence.active = false;
	std::cout << "ordered-sinks "; DumpBuilder(sequence);
	return 0;
}

/* Only the audited short-buffer, borrowed-offset, and fatal-timing gaps. */
static int ConsumerMain()
{
	unsigned operation;
	size_t offset, requested;
	std::string hex;
	while (std::cin >> operation >> offset >> requested >> hex) {
		std::string input;
		if (hex != "-") {
			for (size_t i = 0; i < hex.size(); i += 2) input += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
		}
		std::string_view src = input.empty() ? std::string_view{} : std::string_view(input);
		StringConsumer consumer(src);
		logging_consumer = &consumer;
		messages.clear();
		consumer.Skip(offset);
		std::string observation;
		bool pointer_at_cursor = true;
		const char *expected_pointer = src.data();
		if (offset != 0) expected_pointer += offset;
		switch (operation) {
			case 0:
			case 1: {
				auto view = operation == 0 ? consumer.Peek(requested) : consumer.Read(requested);
				observation = Hex(view);
				pointer_at_cursor = view.data() == expected_pointer;
				break;
			}
			case 2: consumer.Skip(requested); break;
			case 3: observation = std::to_string(consumer.ReadUint8(42)); break;
			case 4: observation = std::to_string(consumer.ReadUint16LE(42)); break;
			case 5: observation = std::to_string(consumer.ReadUint32LE(42)); break;
			case 6: observation = std::to_string(consumer.ReadUint64LE(42)); break;
			case 7: {
				auto value = consumer.TryReadUint32LE();
				observation = std::to_string(value.has_value()) + ":" + std::to_string(value.value_or(42));
				break;
			}
			case 8: observation = std::to_string(Bits(consumer.ReadChar(42))); break;
			case 9: {
				auto value = consumer.TryReadUint16LE();
				observation = std::to_string(value.has_value()) + ":" + std::to_string(value.value_or(42));
				break;
			}
			case 10: {
				auto value = consumer.TryReadSint32LE();
				observation = std::to_string(value.has_value()) + ":" + std::to_string(Bits(value.value_or(42)));
				break;
			}
			case 11: {
				auto value = consumer.TryReadSint64LE();
				observation = std::to_string(value.has_value()) + ":" + std::to_string(Bits(value.value_or(42)));
				break;
			}
			case 12: {
				auto [length, value] = consumer.PeekUtf8();
				observation = std::to_string(length) + ":" + std::to_string(value);
				break;
			}
			case 13: observation = std::to_string(consumer.ReadUtf8(42)); break;
			case 14: consumer.SkipUtf8(); break;
			case 15: {
				auto value = consumer.TryReadUtf8();
				observation = std::to_string(value.has_value()) + ":" + std::to_string(value.value_or(42));
				break;
			}
			default: std::abort();
		}
		std::cout << operation << ' ' << (observation.empty() ? "-" : observation) << ' ' << pointer_at_cursor << ' ' << consumer.GetBytesRead() << ' ' << messages.size();
		for (const auto &[position, message] : messages) std::cout << ' ' << position << ' ' << Hex(message);
		std::cout << '\n';
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && std::string_view(argv[1]) == "--builder") return BuilderMain();
	if (argc == 2 && std::string_view(argv[1]) == "--consumer") return ConsumerMain();

	unsigned width, is_signed, base, clamp;
	std::string hex;
	while (std::cin >> width >> is_signed >> base >> clamp >> hex) {
		std::string input;
		if (hex != "-") {
			for (size_t i = 0; i < hex.size(); i += 2) input += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
		}
		std::string_view src = input.empty() ? std::string_view{} : std::string_view(input);
		if (is_signed) {
			switch (width) {
				case 8: Probe<int8_t>(src, base, clamp); break;
				case 16: Probe<int16_t>(src, base, clamp); break;
				case 32: Probe<int32_t>(src, base, clamp); break;
				case 64: Probe<int64_t>(src, base, clamp); break;
			}
		} else {
			switch (width) {
				case 8: Probe<uint8_t>(src, base, clamp); break;
				case 16: Probe<uint16_t>(src, base, clamp); break;
				case 32: Probe<uint32_t>(src, base, clamp); break;
				case 64: Probe<uint64_t>(src, base, clamp); break;
			}
		}
	}
}
