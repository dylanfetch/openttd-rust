/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file admin-conversion-comparison.cpp Real-VM comparisons for scoped coverage gaps. */
#include "stdafx.h"
#include "script/admin_conversion.hpp"
#include "script/api/script_event_types.hpp"
#include "script/api/script_log.hpp"
#include "script/script_fatalerror.hpp"
#include "rust/admin_conversion_ffi.h"
#include <iostream>
#include <sstream>
#include <utility>

extern bool OriginalOutgoing(nlohmann::json &, HSQUIRRELVM, SQInteger, int);
extern SQInteger OriginalGetObject(HSQUIRRELVM, const std::string &);
extern SQInteger OriginalFromJSON(HSQUIRRELVM, nlohmann::json &);
extern bool ScriptAdminMakeJSON(nlohmann::json &, HSQUIRRELVM, SQInteger, int);
extern void BeginAllocator();
extern size_t EndAllocator();

static size_t fail_size = 0;
static unsigned created = 0, destroyed = 0;
static bool throw_log = false;
static HSQUIRRELVM logging_vm = nullptr;
static std::vector<std::string> trace;

void *operator new(size_t size)
{
	if (size == fail_size && std::exchange(fail_size, 0) != 0) throw std::bad_alloc();
	if (void *result = std::malloc(size == 0 ? 1 : size)) return result;
	throw std::bad_alloc();
}
void operator delete(void *pointer) noexcept { std::free(pointer); }
void operator delete(void *pointer, size_t) noexcept { std::free(pointer); }

extern "C" OpenTTDRustAdminConversion *__real_openttd_rust_admin_conversion_create(uint8_t, int64_t, int32_t);
extern "C" void __real_openttd_rust_admin_conversion_destroy(OpenTTDRustAdminConversion *);
extern "C" OpenTTDRustAdminConversion *__wrap_openttd_rust_admin_conversion_create(uint8_t direction, int64_t index, int32_t depth)
{
	++created;
	return __real_openttd_rust_admin_conversion_create(direction, index, depth);
}
extern "C" void __wrap_openttd_rust_admin_conversion_destroy(OpenTTDRustAdminConversion *owner)
{
	++destroyed;
	__real_openttd_rust_admin_conversion_destroy(owner);
}

static void Require(bool condition) { if (!condition) throw std::runtime_error("fixture requirement"); }
[[noreturn]] void AssertFailedError(std::string_view, const std::source_location) { throw std::runtime_error("assertion"); }
[[noreturn]] void NOT_REACHED(const std::source_location) { throw std::runtime_error("not reached"); }
[[noreturn]] void FatalErrorI(const std::string &) { throw std::runtime_error("fatal"); }
void DebugPrint(std::string_view, int, std::string &&) { throw std::runtime_error("unexpected debug"); }

static std::string Hex(std::string_view value)
{
	std::string result;
	for (unsigned char byte : value) {
		result += "0123456789abcdef"[byte >> 4];
		result += "0123456789abcdef"[byte & 15];
	}
	return result;
}

/* Direct typed inspection has no conversion depth limit or stringification. */
static std::string VMValue(HSQUIRRELVM vm, SQInteger index)
{
	const auto type = sq_gettype(vm, index);
	switch (type) {
		case OT_NULL: return "null";
		case OT_BOOL: { SQBool value; sq_getbool(vm, index, &value); return value ? "true" : "false"; }
		case OT_INTEGER: { SQInteger value; sq_getinteger(vm, index, &value); return "i" + std::to_string(value); }
		case OT_STRING: { std::string_view value; sq_getstring(vm, index, value); return "s" + Hex(value); }
		case OT_ARRAY:
		case OT_TABLE: {
			const auto top = sq_gettop(vm);
			std::vector<std::string> children;
			/* Resolve the original index before adding our iteration slot. */
			const auto absolute = index > 0 ? index : top + index + 1;
			sq_pushnull(vm);
			while (SQ_SUCCEEDED(sq_next(vm, absolute))) {
				children.push_back(VMValue(vm, -2) + ":" + VMValue(vm, -1));
				sq_pop(vm, 2);
			}
			sq_settop(vm, top);
			if (type == OT_TABLE) std::sort(children.begin(), children.end());
			std::string result = type == OT_TABLE ? "{" : "[";
			for (auto &child : children) result += child + ";";
			return result + (type == OT_TABLE ? "}" : "]");
		}
		default: return "type" + std::to_string(type);
	}
}

static std::string Stack(HSQUIRRELVM vm)
{
	std::string result;
	for (SQInteger i = 1; i <= sq_gettop(vm); ++i) result += VMValue(vm, i) + "|";
	return result;
}

static std::string JSONValue(const nlohmann::json &json)
{
	if (json.is_string()) return "s" + Hex(json.get_ref<const std::string &>());
	if (json.is_object()) {
		std::string result = "{";
		for (auto &[key, value] : json.items()) result += Hex(key) + ":" + JSONValue(value) + ";";
		return result + "}";
	}
	if (json.is_array()) {
		std::string result = "[";
		for (auto &value : json) result += JSONValue(value) + ";";
		return result + "]";
	}
	return json.dump();
}

void ScriptLog::Error(const std::string &message)
{
	trace.push_back("log:" + message + ":" + Stack(logging_vm));
	if (throw_log) throw std::runtime_error("injected logger");
}

static SQInteger Reenter(HSQUIRRELVM vm)
{
	SQInteger key;
	sq_getinteger(vm, 2, &key);
	trace.push_back("key:" + std::to_string(key));
	const auto top = sq_gettop(vm);
	sq_pushinteger(vm, key);
	nlohmann::json json;
	Require(ScriptAdminConvertToJSON(json, vm, -1, 26));
	Require(json == key);
	sq_settop(vm, top);
	return 0;
}

static void CompileValue(HSQUIRRELVM vm, std::string_view expression)
{
	std::string buffer = "return " + std::string(expression);
	Require(sq_compilebuffer(vm, buffer, "admin-gap", SQTrue) == SQ_OK);
	sq_pushroottable(vm);
	Require(sq_call(vm, 1, SQTrue, SQTrue) == SQ_OK);
}

template <typename Call>
static std::string Observe(HSQUIRRELVM vm, nlohmann::json &json, Call call, size_t allocation, bool log_failure)
{
	trace.clear();
	created = destroyed = 0;
	logging_vm = vm;
	throw_log = log_failure;
	fail_size = allocation;
	std::string outcome;
	try { outcome = call() ? "true" : "false"; }
	catch (const Script_FatalError &error) { outcome = "Script_FatalError:" + error.GetErrorMessage(); }
	catch (const std::bad_alloc &) { outcome = "bad_alloc"; }
	catch (const nlohmann::json::exception &error) { outcome = "json_exception:" + std::to_string(error.id); }
	catch (const std::runtime_error &error) { outcome = "runtime_error:" + std::string(error.what()); }
	fail_size = 0;
	throw_log = false;
	Require(created == destroyed);
	Require(allocation == 0 || outcome == "bad_alloc");
	Require(!log_failure || outcome == "runtime_error:injected logger");
	outcome += ":json=" + JSONValue(json) + ":stack=" + Stack(vm);
	for (auto &event : trace) outcome += ":" + event;
	return outcome;
}

template <typename Setup>
static void Outgoing(std::string_view name, Setup setup, int depth = 0, size_t allocation = 0, bool log_failure = false)
{
	BeginAllocator();
	auto vm = sq_open(1024);
	sq_pushinteger(vm, 55); // State below the tested root must remain untouched.
	setup(vm);
	const auto top = sq_gettop(vm);
	nlohmann::json original = "untouched", candidate = original;
	auto expected = Observe(vm, original, [&] { return OriginalOutgoing(original, vm, -1, depth); }, allocation, log_failure);
	sq_settop(vm, top); // Fixture reset after inspecting the original exception state.
	auto actual = Observe(vm, candidate, [&] { return ScriptAdminMakeJSON(candidate, vm, -1, depth); }, allocation, log_failure);
	if (actual != expected) { std::cerr << name << "\nexpected " << expected << "\nactual " << actual << '\n'; std::exit(1); }
	if (expected.starts_with("true:") && original.is_object()) {
		/* The other mixed pair: unchanged outgoing feeds the two incoming paths. */
		OriginalFromJSON(vm, original);
		auto mixed = VMValue(vm, -1);
		sq_settop(vm, top);
		ScriptAdminConvertFromJSON(original, vm);
		Require(VMValue(vm, -1) == mixed);
		expected += ":mixed=" + mixed;
	}
	Require(created == destroyed);
	sq_close(vm);
	Require(EndAllocator() == 0);
	std::cout << "out " << name << " " << expected << '\n';
}

static void Incoming(std::string_view name, nlohmann::json source, bool parsed = false, size_t allocation = 0, bool log_failure = false)
{
	std::string results[2];
	for (unsigned candidate = 0; candidate != 2; ++candidate) {
		BeginAllocator();
		auto vm = sq_open(1024);
		sq_pushinteger(vm, 55);
		auto json = source;
		results[candidate] = Observe(vm, json, [&] {
			if (parsed) {
				const auto raw = source.get<std::string>();
				return (candidate ? ScriptEventAdminPort(raw).GetObject(vm) : OriginalGetObject(vm, raw)) == 1;
			}
			if (candidate) ScriptAdminConvertFromJSON(json, vm); else OriginalFromJSON(vm, json);
			return true;
		}, allocation, log_failure);
		if (name == "script-allocation-limit") Require(results[candidate].starts_with("Script_FatalError:"));
		if (name == "deep-40") Require(sq_gettype(vm, -1) == OT_TABLE);
		/* Mixed pair: successful input uses original outgoing, never candidate-only round-trip. */
		if (!allocation && !log_failure && sq_gettype(vm, -1) == OT_TABLE && name != "deep-40") {
			nlohmann::json round_trip;
			Require(OriginalOutgoing(round_trip, vm, -1, 0));
			results[candidate] += ":mixed=" + JSONValue(round_trip);
		}
		sq_close(vm);
		Require(EndAllocator() == 0);
	}
	if (results[0] != results[1]) { std::cerr << name << "\nexpected " << results[0] << "\nactual " << results[1] << '\n'; std::exit(1); }
	std::cout << "in " << name << " " << results[0] << '\n';
}

int main()
{
	for (int depth : {24, 25, 26, -1}) Outgoing("scalar-depth-" + std::to_string(depth), [](auto vm) { sq_pushinteger(vm, INT64_MIN); }, depth);
	for (int depth : {24, 25, 26}) Outgoing("array-depth-" + std::to_string(depth), [](auto vm) { CompileValue(vm, "[1]"); }, depth);
	Outgoing("partial-array", [](auto vm) { CompileValue(vm, "[1,2,function(){}]"); });
	Outgoing("partial-table", [](auto vm) { CompileValue(vm, "{a=1,b=[2,function(){}],c=3}"); });
	Outgoing("logger-throw", [](auto vm) { CompileValue(vm, "[1,function(){}]"); }, 0, 0, true);
	Outgoing("bytes", [](auto vm) {
		sq_newtable(vm);
		sq_pushstring(vm, std::string_view("key\0\xff", 5));
		sq_pushstring(vm, std::string_view("value\0\xff", 7));
		sq_rawset(vm, -3);
	});
	Outgoing("key-copy-throw", [](auto vm) {
		sq_newtable(vm);
		sq_pushstring(vm, std::string(16128, 'k'));
		sq_pushinteger(vm, 7);
		sq_rawset(vm, -3);
	}, 0, 16129);
	Outgoing("key-order-collision-reentrant", [](auto vm) {
		sq_pushroottable(vm);
		sq_pushstring(vm, "reenter");
		sq_newclosure(vm, Reenter, 0);
		sq_newslot(vm, -3, SQFalse);
		sq_pop(vm, 1);
		CompileValue(vm, R"sq((function(){class Key {i=0;constructor(v){i=v;}function _tostring(){reenter(i);return "x";}}return {[Key(1)]=10,[Key(2)]=20,x=30};})())sq");
	});
	Incoming("extrema", {{"min", INT64_MIN}, {"max", INT64_MAX}, {"u63", uint64_t{1} << 63}, {"u64", UINT64_MAX}});
	Incoming("bytes", {{std::string("key\0\xff", 5), std::string("value\0\xff", 7)}});
	Incoming("partial-float", {{"a", 1}, {"b", nlohmann::json::array({2, 1.5})}, {"c", 3}});
	Incoming("logger-throw", {{"a", 1}, {"b", 1.5}}, false, 0, true);
	Incoming("string-copy-throw", {{"a", 1}, {"b", std::string(16128, 's')}}, false, 16129);
	Incoming("script-allocation-limit", {{"a", 1}, {"b", std::string(1024 * 1024, 's')}});
	nlohmann::json deep = 1;
	for (unsigned i = 0; i < 40; ++i) deep = nlohmann::json::array({std::move(deep)});
	Incoming("deep-40", {{"deep", std::move(deep)}});
	for (auto raw : {"{\"a\":1e0}", "{\"a\":1,\"b\":[2,3.5]}", "[]", "true", "{", "{\"a\":9223372036854775808}"}) Incoming(raw, raw, true);
	std::cout << "owners all-created-destroyed\n";
}
