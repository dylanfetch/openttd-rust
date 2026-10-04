/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_adapter.hpp Nonowning C++ views of Rust industry allocations. */
#ifndef RUST_INDUSTRY_ADAPTER_HPP
#define RUST_INDUSTRY_ADAPTER_HPP
#include "industry_ffi.h"
#include <memory>
#include <iterator>
using RustIndustryOwner = std::unique_ptr<OpenTTDIndustry, decltype(&openttd_rust_industry_destroy)>;
using RustIndustryBuilderOwner = std::unique_ptr<OpenTTDIndustryBuilder, decltype(&openttd_rust_industry_builder_destroy)>;
/** Optional history is allocated and freed by the Rust accepted slot. */
template <typename T> struct RustIndustryHistory {
	T *pointer = nullptr;

	T *get() const { return this->pointer; }
	T *operator->() const { return this->pointer; }
	T &operator*() const { return *this->pointer; }
	bool operator==(std::nullptr_t) const { return this->pointer == nullptr; }
};
/** Views never own or destroy slot records. Mutations may invalidate all iterators. */
template <typename T, bool Produced> class RustIndustryVector {
	OpenTTDIndustry *owner;
	OpenTTDIndustrySlots View() const { return openttd_rust_industry_slots(this->owner, Produced, 0, 0); }
public:
	using value_type = T;
	using iterator = T *;
	using const_iterator = const T *;
	explicit RustIndustryVector(OpenTTDIndustry *owner) : owner(owner) {}
	T *data() { return static_cast<T *>(this->View().data); }
	const T *data() const { return static_cast<const T *>(this->View().data); }
	size_t size() const { return this->View().size; }
	bool empty() const { return this->size() == 0; }
	iterator begin() { return this->data(); }
	const_iterator begin() const { return this->data(); }
	iterator end() { auto v = this->View(); return static_cast<T *>(v.data) + v.size; }
	const_iterator end() const { auto v = this->View(); return static_cast<const T *>(v.data) + v.size; }
	auto rbegin() { return std::reverse_iterator(this->end()); }
	auto rend() { return std::reverse_iterator(this->begin()); }
	T &operator[](size_t n) { return this->data()[n]; }
	const T &operator[](size_t n) const { return this->data()[n]; }
	T &front() { return *this->begin(); }
	const T &front() const { return *this->begin(); }
	T &back() { return this->data()[this->size() - 1]; }
	const T &back() const { return this->data()[this->size() - 1]; }
	void reserve(size_t n) { openttd_rust_industry_slots(this->owner, Produced, 1, n); }
	void resize(size_t n) { openttd_rust_industry_slots(this->owner, Produced, 2, n); }
	T &emplace_back() { auto v = openttd_rust_industry_slots(this->owner, Produced, 3, 0); return static_cast<T *>(v.data)[v.size - 1]; }
	void push_back(const T &value) { this->emplace_back() = value; }
	void push_back(T &&value) { this->emplace_back() = std::move(value); }
	void clear() { this->resize(0); }
	void shrink_to_fit() { openttd_rust_industry_slots(this->owner, Produced, 4, 0); }
	iterator erase(iterator first, iterator last) { auto n = first - this->begin(); assert(last == this->end()); this->resize(n); return this->end(); }
};
#endif
