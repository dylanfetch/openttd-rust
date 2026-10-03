/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_list.cpp Implementation of ScriptList. */

#include "../../stdafx.h"
#include "script_list.hpp"
#include "../../debug.h"
#include "../../script/squirrel.hpp"

#include "../../safeguards.h"

#ifndef WITH_RUST
/**
 * Base class for any ScriptList sorter.
 */
class ScriptListSorter {
protected:
	const ScriptList *list; ///< The list that's being sorted.
	bool has_no_more_items = true; ///< Whether we have more items to iterate over.
	std::optional<SQInteger> item_next = std::nullopt; ///< The next item we will show, or std::nullopt if there are no more items to iterate over.

	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorter(const ScriptList *list) : list(list) {}

	/**
	 * Actually try to find the next item.
	 */
	virtual void FindNext() = 0;

	/**
	 * Retarget sorter internal iterator after retargeting the list.
	 */
	virtual void RetargetIterator() = 0;

public:
	/**
	 * Virtual dtor, needed to mute warnings.
	 */
	virtual ~ScriptListSorter() = default;

	/**
	 * Get the first item of the sorter.
	 */
	virtual std::optional<SQInteger> Begin() = 0;

	/**
	 * Stop iterating a sorter.
	 */
	void End()
	{
		this->item_next = std::nullopt;
		this->has_no_more_items = true;
	}

	/**
	 * Get the next item of the sorter.
	 * @return Optional containing the next item, or std::nullopt when there is none.
	 */
	std::optional<SQInteger> Next()
	{
		if (this->IsEnd()) return std::nullopt;

		std::optional<SQInteger> item_current = this->item_next;
		this->FindNext();
		return item_current;
	}

	/**
	 * See if the sorter has reached the end.
	 */
	bool IsEnd() const
	{
		return this->list->items.empty() || this->has_no_more_items;
	}

	/**
	 * Callback from the list if an item gets removed.
	 */
	void Remove(SQInteger item)
	{
		if (this->IsEnd()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) this->FindNext();
	}

	/**
	 * Attach the sorter to a new list and update internal iterator so it remains valid
	 * in the context of the new list. This assumes the content of the old list has been
	 * moved to the new list.
	 * @param new_list New list to attach to and update internal iterator.
	 */
	void Retarget(const ScriptList *new_list)
	{
		this->list = new_list;
		this->RetargetIterator();
	}
};

/**
 * Sort by value, ascending.
 */
class ScriptListSorterValueAscending : public ScriptListSorter {
private:
	ScriptList::ScriptListSet::const_iterator value_iter; ///< The iterator over the value/item pairs in the set.

public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterValueAscending(const ScriptList *list) : ScriptListSorter(list) {}

	std::optional<SQInteger> Begin() override
	{
		if (this->list->values.empty()) {
			this->item_next = std::nullopt;
			return std::nullopt;
		}
		this->has_no_more_items = false;

		this->value_iter = this->list->values.begin();
		this->item_next = this->value_iter->second;

		std::optional<SQInteger> item_current = this->item_next;
		this->FindNext();
		return item_current;
	}

	void FindNext() override
	{
		this->item_next = std::nullopt;
		if (this->value_iter == this->list->values.end()) {
			this->has_no_more_items = true;
			return;
		}
		++this->value_iter;
		if (this->value_iter != this->list->values.end()) this->item_next = this->value_iter->second;
	}

	void RetargetIterator() override
	{
		if (this->item_next.has_value()) {
			auto item_iter = this->list->items.find(this->item_next.value());
			this->value_iter = this->list->values.find({item_iter->second, this->item_next.value()});
		} else {
			this->value_iter = this->list->values.end();
		}
	}
};

/**
 * Sort by value, descending.
 */
class ScriptListSorterValueDescending : public ScriptListSorter {
private:
	/* Note: We cannot use reverse_iterator.
	 *       The iterators must only be invalidated when the element they are pointing to is removed.
	 *       This only holds for forward iterators. */
	ScriptList::ScriptListSet::const_iterator value_iter; ///< The iterator over the value/item pairs in the set.

public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterValueDescending(const ScriptList *list) : ScriptListSorter(list) {}

	std::optional<SQInteger> Begin() override
	{
		if (this->list->values.empty()) {
			this->item_next = std::nullopt;
			return std::nullopt;
		}
		this->has_no_more_items = false;

		this->value_iter = this->list->values.end();
		--this->value_iter;
		this->item_next = this->value_iter->second;

		std::optional<SQInteger> item_current = this->item_next;
		this->FindNext();
		return item_current;
	}

	void FindNext() override
	{
		this->item_next = std::nullopt;
		if (this->value_iter == this->list->values.end()) {
			this->has_no_more_items = true;
			return;
		}
		if (this->value_iter == this->list->values.begin()) {
			/* Use 'end' as marker for 'beyond begin' */
			this->value_iter = this->list->values.end();
		} else {
			--this->value_iter;
		}
		if (this->value_iter != this->list->values.end()) this->item_next = this->value_iter->second;
	}

	void RetargetIterator() override
	{
		if (this->item_next.has_value()) {
			auto item_iter = this->list->items.find(this->item_next.value());
			this->value_iter = this->list->values.find({item_iter->second, this->item_next.value()});
		} else {
			this->value_iter = this->list->values.end();
		}
	}
};

/**
 * Sort by item, ascending.
 */
class ScriptListSorterItemAscending : public ScriptListSorter {
private:
	ScriptList::ScriptListMap::const_iterator item_iter; ///< The iterator over the items in the map.

public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterItemAscending(const ScriptList *list) : ScriptListSorter(list) {}

	std::optional<SQInteger> Begin() override
	{
		if (this->list->items.empty()) {
			this->item_next = std::nullopt;
			return std::nullopt;
		}
		this->has_no_more_items = false;

		this->item_iter = this->list->items.begin();
		this->item_next = this->item_iter->first;

		std::optional<SQInteger> item_current = this->item_next;
		this->FindNext();
		return item_current;
	}

	void FindNext() override
	{
		this->item_next = std::nullopt;
		if (this->item_iter == this->list->items.end()) {
			this->has_no_more_items = true;
			return;
		}
		++this->item_iter;
		if (this->item_iter != this->list->items.end()) this->item_next = this->item_iter->first;
	}

	void RetargetIterator() override
	{
		if (this->item_next.has_value()) {
			this->item_iter = this->list->items.find(this->item_next.value());
		} else {
			this->item_iter = this->list->items.end();
		}
	}
};

/**
 * Sort by item, descending.
 */
class ScriptListSorterItemDescending : public ScriptListSorter {
private:
	/* Note: We cannot use reverse_iterator.
	 *       The iterators must only be invalidated when the element they are pointing to is removed.
	 *       This only holds for forward iterators. */
	ScriptList::ScriptListMap::const_iterator item_iter; ///< The iterator over the items in the map.

public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterItemDescending(const ScriptList *list) : ScriptListSorter(list) {}

	std::optional<SQInteger> Begin() override
	{
		if (this->list->items.empty()) {
			this->item_next = std::nullopt;
			return std::nullopt;
		}
		this->has_no_more_items = false;

		this->item_iter = this->list->items.end();
		--this->item_iter;
		this->item_next = this->item_iter->first;

		std::optional<SQInteger> item_current = this->item_next;
		this->FindNext();
		return item_current;
	}

	void FindNext() override
	{
		this->item_next = std::nullopt;
		if (this->item_iter == this->list->items.end()) {
			this->has_no_more_items = true;
			return;
		}
		if (this->item_iter == this->list->items.begin()) {
			/* Use 'end' as marker for 'beyond begin' */
			this->item_iter = this->list->items.end();
		} else {
			--this->item_iter;
		}
		if (this->item_iter != this->list->items.end()) this->item_next = this->item_iter->first;
	}

	void RetargetIterator() override
	{
		if (this->item_next.has_value()) {
			this->item_iter = this->list->items.find(this->item_next.value());
		} else {
			this->item_iter = this->list->items.end();
		}
	}
};



#endif

#ifdef WITH_RUST
static_assert(sizeof(SQInteger) == sizeof(int64_t) && std::is_signed_v<SQInteger>);
static_assert(sizeof(int) == sizeof(int32_t));
#endif

#ifdef WITH_RUST
static_assert(sizeof(SQRESULT) == sizeof(int64_t) && std::is_signed_v<SQRESULT>);
static_assert(sizeof(SQBool) == sizeof(uint64_t) && std::is_unsigned_v<SQBool>);
static_assert(OT_NULL == 0x01000001 && OT_INTEGER == 0x05000002 && OT_BOOL == 0x01000008);
static_assert(OT_TABLE == 0x0A000020 && OT_ARRAY == 0x08000040);
static_assert(OT_CLOSURE == 0x08000100 && OT_NATIVECLOSURE == 0x08000200);
static_assert(SQ_ERROR == -1 && SQTrue == 1 && SQFalse == 0);

ScriptList::VMControl::VMControl(HSQUIRRELVM vm, ScriptList *list, uint8_t operation) :
	vm(vm), list(list), control(openttd_rust_list_control_new(operation), openttd_rust_list_control_destroy)
{
}

SQInteger ScriptList::VMControl::Drive()
{
	this->needs_index = false;
	for (;;) {
		OpenTTDListControlAction action{};
		openttd_rust_list_control_step(this->control.get(), this->list->owner.get(), &this->input, &action);
		this->input = {};
		/* No Rust stack or borrow exists while these real VM/RAII operations
		 * execute. Exceptions retain the original stack and partial list state. */
		switch (action.kind) {
			case LC_RETURN: return action.a;
			case LC_ITEM: return 0;
			case LC_INDEX: this->needs_index = true; return 0;
			case LC_TOP: this->input.a = sq_gettop(this->vm); break;
			case LC_TYPE: this->input.kind = static_cast<uint32_t>(sq_gettype(this->vm, action.a)); break;
			case LC_GET_INT: {
				SQInteger value;
				/* _nexti ignores failure and the value. Do not read that local
				 * on failure; load's historical ignored getters stay below. */
				if (SQ_SUCCEEDED(sq_getinteger(this->vm, action.a, &value))) this->input.a = value;
				break;
			}
			case LC_GET_BOOL: {
				SQBool value;
				sq_getbool(this->vm, action.a, &value);
				this->input.flag = value;
				break;
			}
			case LC_GET_PAIR: {
				/* Preserve the original AND type predicate and ignored getter
				 * results. Defined mixed numeric values convert in the actual VM.
				 * A nonnumeric side paired with integer can read an uninitialized
				 * local upstream, and remains outside the defined-input contract. */
				SQInteger key, value;
				sq_getinteger(this->vm, -2, &key);
				sq_getinteger(this->vm, -1, &value);
				this->input.a = key;
				this->input.b = value;
				break;
			}
			case LC_PUSH: sq_push(this->vm, action.a); break;
			case LC_ROOT: sq_pushroottable(this->vm); break;
			case LC_PUSH_INT: sq_pushinteger(this->vm, action.a); break;
			case LC_PUSH_BOOL: sq_pushbool(this->vm, action.a != 0 ? SQTrue : SQFalse); break;
			case LC_PUSH_NULL: sq_pushnull(this->vm); break;
			case LC_TAG: sq_pushstring(this->vm, "List"); break;
			case LC_NEW_ARRAY: sq_newarray(this->vm, 0); break;
			case LC_NEW_TABLE: sq_newtable(this->vm); break;
			case LC_APPEND: sq_arrayappend(this->vm, action.a); break;
			case LC_RAW_SET: sq_rawset(this->vm, action.a); break;
			case LC_NEXT: this->input.flag = SQ_SUCCEEDED(sq_next(this->vm, action.a)); break;
			case LC_POP: sq_pop(this->vm, action.a); break;
			case LC_POP_TOP: sq_poptop(this->vm); break;
			case LC_CALL: this->input.flag = SQ_SUCCEEDED(sq_call(this->vm, action.a, SQTrue, SQFalse)); break;
			case LC_CHARGE: Squirrel::DecreaseOps(this->vm, static_cast<int>(action.a)); break;
			case LC_DISABLE: this->disabler.emplace(); break;
			case LC_LIMIT: this->limiter.emplace(this->vm, MAX_VALUATE_OPS, action.a == 0 ? "valuator function" : "list filter function"); break;
			case LC_THROW_RESULT: throw static_cast<SQInteger>(action.a);
			case LC_ERROR:
			case LC_THROW_ERROR: {
				const char *message;
				switch (action.a) {
					case 1: message = "You need to give at least a Valuator as parameter to ScriptList::Valuate"; break;
					case 2: message = "parameter 1 has an invalid type (expected function)"; break;
					case 3: message = "return value of valuator is not valid (not integer/bool)"; break;
					case 4: message = "modifying valuated list outside of valuator function"; break;
					case 5: message = "return value of filter is not valid (not bool)"; break;
					case 6: message = "you can only assign integers to this list"; break;
					default: NOT_REACHED();
				}
				SQInteger result = sq_throwerror(this->vm, message);
				if (action.kind == LC_THROW_ERROR) throw result;
				return result;
			}
			case LC_LOG_NEXT: Debug(script, 0, "Next() is invalid as Begin() is never called"); break;
			case LC_LOG_END: Debug(script, 0, "IsEnd() is invalid as Begin() is never called"); break;
			default: NOT_REACHED();
		}
	}
}

void ScriptList::VMControl::Item(bool valid)
{
	this->input.flag = valid ? 2 : 1;
	this->Drive();
}

void ScriptList::VMControl::Index(SQInteger index)
{
	this->input.a = index;
	this->Drive();
}

void ScriptList::VMControl::Finish()
{
	this->input.flag = 0;
	this->Drive();
}
#endif

bool ScriptList::SaveObject(HSQUIRRELVM vm)
{
#ifdef WITH_RUST
	return VMControl(vm, this, 2).Drive() != 0;
#else
	sq_pushstring(vm, "List");
	sq_newarray(vm, 0);
	sq_pushinteger(vm, this->sorter_type);
	sq_arrayappend(vm, -2);
	sq_pushbool(vm, this->sort_ascending ? SQTrue : SQFalse);
	sq_arrayappend(vm, -2);
	sq_newtable(vm);
	for (const auto &item : this->items) {
		sq_pushinteger(vm, item.first);
		sq_pushinteger(vm, item.second);
		sq_rawset(vm, -3);
	}
	sq_arrayappend(vm, -2);
	return true;
#endif
}

bool ScriptList::LoadObject(HSQUIRRELVM vm)
{
#ifdef WITH_RUST
	return VMControl(vm, this, 3).Drive() != 0;
#else
	if (sq_gettype(vm, -1) != OT_ARRAY) return false;
	sq_pushnull(vm);
	if (SQ_FAILED(sq_next(vm, -2))) return false;
	if (sq_gettype(vm, -1) != OT_INTEGER) return false;
	SQInteger type;
	sq_getinteger(vm, -1, &type);
	sq_pop(vm, 2);
	if (SQ_FAILED(sq_next(vm, -2))) return false;
	if (sq_gettype(vm, -1) != OT_BOOL) return false;
	SQBool order;
	sq_getbool(vm, -1, &order);
	sq_pop(vm, 2);
	if (SQ_FAILED(sq_next(vm, -2))) return false;
	if (sq_gettype(vm, -1) != OT_TABLE) return false;
	sq_pushnull(vm);
	while (SQ_SUCCEEDED(sq_next(vm, -2))) {
		if (sq_gettype(vm, -2) != OT_INTEGER && sq_gettype(vm, -1) != OT_INTEGER) return false;
		SQInteger key, value;
		sq_getinteger(vm, -2, &key);
		sq_getinteger(vm, -1, &value);
		this->AddItem(key, value);
		sq_pop(vm, 2);
	}
	sq_pop(vm, 3);
	if (SQ_SUCCEEDED(sq_next(vm, -2))) return false;
	sq_pop(vm, 1);
	this->Sort(static_cast<SorterType>(type), order == SQTrue);
	return true;
#endif
}

ScriptObject *ScriptList::CloneObject()
{
	ScriptList *clone = new ScriptList();
	clone->CopyList(this);
	return clone;
}

void ScriptList::CopyList(const ScriptList *list)
{
#ifdef WITH_RUST
	openttd_rust_list_combine(this->owner.get(), list->owner.get(), 4);
#else
	this->Sort(list->sorter_type, list->sort_ascending);
	this->items = list->items;
	this->values = list->values;
#endif
}

#ifdef WITH_RUST
ScriptList::ScriptList() : owner(openttd_rust_list_new(), openttd_rust_list_destroy) {}
#else
ScriptList::ScriptList()
{
	/* Default sorter */
	this->sorter         = std::make_unique<ScriptListSorterValueDescending>(this);
	this->sorter_type    = SORT_BY_VALUE;
	this->sort_ascending = false;
	this->initialized    = false;
	this->modifications  = 0;
}
#endif

ScriptList::~ScriptList()
{
}

bool ScriptList::HasItem(SQInteger item)
{
#ifdef WITH_RUST
	int64_t value;
	return openttd_rust_list_get(this->owner.get(), item, &value) != 0;
#else
	return this->items.count(item) == 1;
#endif
}

void ScriptList::Clear()
{
#ifdef WITH_RUST
	openttd_rust_list_clear(this->owner.get());
#else
	this->modifications++;

	this->items.clear();
	this->values.clear();
	this->sorter->End();
#endif
}

void ScriptList::AddItem(SQInteger item, SQInteger value)
{
#ifdef WITH_RUST
	openttd_rust_list_add(this->owner.get(), item, value);
#else
	this->modifications++;

	if (this->HasItem(item)) return;

	this->items[item] = value;
	this->values.emplace(value, item);
#endif
}

void ScriptList::RemoveItem(SQInteger item)
{
#ifdef WITH_RUST
	openttd_rust_list_remove(this->owner.get(), item);
#else
	this->modifications++;

	auto item_iter = this->items.find(item);
	if (item_iter == this->items.end()) return;

	SQInteger value = item_iter->second;

	this->sorter->Remove(item);
	auto value_iter = this->values.find({value, item});
	assert(value_iter != this->values.end());
	this->values.erase(value_iter);
	this->items.erase(item_iter);
#endif
}

SQInteger ScriptList::Begin()
{
#ifdef WITH_RUST
	int64_t value;
	openttd_rust_list_iter(this->owner.get(), 0, &value);
	return value;
#else
	this->initialized = true;
	return this->sorter->Begin().value_or(0);
#endif
}

SQInteger ScriptList::Next()
{
#ifdef WITH_RUST
	int64_t value;
	if (openttd_rust_list_iter(this->owner.get(), 1, &value) == 0) Debug(script, 0, "Next() is invalid as Begin() is never called");
	return value;
#else
	if (!this->initialized) {
		Debug(script, 0, "Next() is invalid as Begin() is never called");
		return 0;
	}
	return this->sorter->Next().value_or(0);
#endif
}

bool ScriptList::IsEmpty()
{
#ifdef WITH_RUST
	return openttd_rust_list_count(this->owner.get()) == 0;
#else
	return this->items.empty();
#endif
}

bool ScriptList::IsEnd()
{
#ifdef WITH_RUST
	int64_t value;
	if (openttd_rust_list_iter(this->owner.get(), 2, &value) == 0) {
		Debug(script, 0, "IsEnd() is invalid as Begin() is never called");
		return true;
	}
	return value != 0;
#else
	if (!this->initialized) {
		Debug(script, 0, "IsEnd() is invalid as Begin() is never called");
		return true;
	}
	return this->sorter->IsEnd();
#endif
}

SQInteger ScriptList::Count()
{
#ifdef WITH_RUST
	return openttd_rust_list_count(this->owner.get());
#else
	return this->items.size();
#endif
}

SQInteger ScriptList::GetValue(SQInteger item)
{
#ifdef WITH_RUST
	int64_t value = 0;
	openttd_rust_list_get(this->owner.get(), item, &value);
	return value;
#else
	auto item_iter = this->items.find(item);
	return item_iter == this->items.end() ? 0 : item_iter->second;
#endif
}

bool ScriptList::SetValue(SQInteger item, SQInteger value)
{
#ifdef WITH_RUST
	return openttd_rust_list_set(this->owner.get(), item, value) != 0;
#else
	this->modifications++;

	auto item_iter = this->items.find(item);
	if (item_iter == this->items.end()) return false;

	SQInteger value_old = item_iter->second;
	if (value_old == value) return true;

	this->sorter->Remove(item);
	auto value_iter = this->values.find({value_old, item});
	assert(value_iter != this->values.end());
	item_iter->second = value;
	auto node_handle = this->values.extract(value_iter);
	node_handle.value().first = value;
	this->values.insert(std::move(node_handle));

	return true;
#endif
}

void ScriptList::Sort(SorterType sorter, bool ascending)
{
#ifdef WITH_RUST
	openttd_rust_list_sort(this->owner.get(), sorter, ascending);
#else
	this->modifications++;

	if (sorter != SORT_BY_VALUE && sorter != SORT_BY_ITEM) return;
	if (sorter == this->sorter_type && ascending == this->sort_ascending) return;

	switch (sorter) {
		case SORT_BY_ITEM:
			if (ascending) {
				this->sorter = std::make_unique<ScriptListSorterItemAscending>(this);
			} else {
				this->sorter = std::make_unique<ScriptListSorterItemDescending>(this);
			}
			break;

		case SORT_BY_VALUE:
			if (ascending) {
				this->sorter = std::make_unique<ScriptListSorterValueAscending>(this);
			} else {
				this->sorter = std::make_unique<ScriptListSorterValueDescending>(this);
			}
			break;

		default: NOT_REACHED();
	}
	this->sorter_type    = sorter;
	this->sort_ascending = ascending;
	this->initialized    = false;
#endif
}

void ScriptList::AddList(ScriptList *list)
{
#ifdef WITH_RUST
	openttd_rust_list_combine(this->owner.get(), list->owner.get(), 0);
#else
	if (list == this) return;

	if (this->IsEmpty()) {
		/* If this is empty, we can just take the items of the other list as is. */
		this->items = list->items;
		this->values = list->values;
		this->modifications++;
	} else {
		for (const auto &item : list->items) {
			this->AddItem(item.first);
			this->SetValue(item.first, item.second);
		}
	}
#endif
}

void ScriptList::SwapList(ScriptList *list)
{
#ifdef WITH_RUST
	openttd_rust_list_combine(this->owner.get(), list->owner.get(), 1);
#else
	if (list == this) return;

	this->items.swap(list->items);
	this->values.swap(list->values);
	std::swap(this->sorter, list->sorter);
	std::swap(this->sorter_type, list->sorter_type);
	std::swap(this->sort_ascending, list->sort_ascending);
	std::swap(this->initialized, list->initialized);
	std::swap(this->modifications, list->modifications);
	this->sorter->Retarget(this);
	list->sorter->Retarget(list);
#endif
}

void ScriptList::RemoveAboveValue(SQInteger value)
{
#ifdef WITH_RUST
	openttd_rust_list_filter(this->owner.get(), 0, value, 0);
#else
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = std::next(iter);
		if (iter->second > value) this->RemoveItem(iter->first);
	}
#endif
}

void ScriptList::RemoveBelowValue(SQInteger value)
{
#ifdef WITH_RUST
	openttd_rust_list_filter(this->owner.get(), 1, value, 0);
#else
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = std::next(iter);
		if (iter->second < value) this->RemoveItem(iter->first);
	}
#endif
}

void ScriptList::RemoveBetweenValue(SQInteger start, SQInteger end)
{
#ifdef WITH_RUST
	openttd_rust_list_filter(this->owner.get(), 2, start, end);
#else
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = std::next(iter);
		if (iter->second > start && iter->second < end) this->RemoveItem(iter->first);
	}
#endif
}

void ScriptList::RemoveValue(SQInteger value)
{
#ifdef WITH_RUST
	openttd_rust_list_filter(this->owner.get(), 3, value, 0);
#else
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = std::next(iter);
		if (iter->second == value) this->RemoveItem(iter->first);
	}
#endif
}

void ScriptList::RemoveTop(SQInteger count)
{
#ifdef WITH_RUST
	openttd_rust_list_rank(this->owner.get(), 0, count);
#else
	this->modifications++;

	if (!this->sort_ascending) {
		this->Sort(this->sorter_type, !this->sort_ascending);
		this->RemoveBottom(count);
		this->Sort(this->sorter_type, !this->sort_ascending);
		return;
	}

	switch (this->sorter_type) {
		default: NOT_REACHED();
		case SORT_BY_VALUE:
			for (auto iter = this->values.begin(); iter != this->values.end(); iter = this->values.begin()) {
				if (--count < 0) return;
				this->RemoveItem(iter->second);
			}
			break;

		case SORT_BY_ITEM:
			for (auto iter = this->items.begin(); iter != this->items.end(); iter = this->items.begin()) {
				if (--count < 0) return;
				this->RemoveItem(iter->first);
			}
			break;
	}
#endif
}

void ScriptList::RemoveBottom(SQInteger count)
{
#ifdef WITH_RUST
	openttd_rust_list_rank(this->owner.get(), 1, count);
#else
	this->modifications++;

	if (!this->sort_ascending) {
		this->Sort(this->sorter_type, !this->sort_ascending);
		this->RemoveTop(count);
		this->Sort(this->sorter_type, !this->sort_ascending);
		return;
	}

	switch (this->sorter_type) {
		default: NOT_REACHED();
		case SORT_BY_VALUE:
			for (auto iter = this->values.rbegin(); iter != this->values.rend(); iter = this->values.rbegin()) {
				if (--count < 0) return;
				this->RemoveItem(iter->second);
			}
			break;

		case SORT_BY_ITEM:
			for (auto iter = this->items.rbegin(); iter != this->items.rend(); iter = this->items.rbegin()) {
				if (--count < 0) return;
				this->RemoveItem(iter->first);
			}
			break;
	}
#endif
}

void ScriptList::RemoveList(ScriptList *list)
{
#ifdef WITH_RUST
	openttd_rust_list_combine(this->owner.get(), list->owner.get(), 2);
#else
	this->modifications++;

	if (list == this) {
		this->Clear();
	} else {
		for (const auto &item : list->items) {
			this->RemoveItem(item.first);
		}
	}
#endif
}

void ScriptList::KeepAboveValue(SQInteger value)
{
#ifdef WITH_RUST
	openttd_rust_list_filter(this->owner.get(), 4, value, 0);
#else
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = std::next(iter);
		if (iter->second <= value) this->RemoveItem(iter->first);
	}
#endif
}

void ScriptList::KeepBelowValue(SQInteger value)
{
#ifdef WITH_RUST
	openttd_rust_list_filter(this->owner.get(), 5, value, 0);
#else
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = std::next(iter);
		if (iter->second >= value) this->RemoveItem(iter->first);
	}
#endif
}

void ScriptList::KeepBetweenValue(SQInteger start, SQInteger end)
{
#ifdef WITH_RUST
	openttd_rust_list_filter(this->owner.get(), 6, start, end);
#else
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = std::next(iter);
		if (iter->second <= start || iter->second >= end) this->RemoveItem(iter->first);
	}
#endif
}

void ScriptList::KeepValue(SQInteger value)
{
#ifdef WITH_RUST
	openttd_rust_list_filter(this->owner.get(), 7, value, 0);
#else
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = std::next(iter);
		if (iter->second != value) this->RemoveItem(iter->first);
	}
#endif
}

void ScriptList::KeepTop(SQInteger count)
{
#ifdef WITH_RUST
	openttd_rust_list_rank(this->owner.get(), 2, count);
#else
	this->modifications++;

	this->RemoveBottom(this->Count() - count);
#endif
}

void ScriptList::KeepBottom(SQInteger count)
{
#ifdef WITH_RUST
	openttd_rust_list_rank(this->owner.get(), 3, count);
#else
	this->modifications++;

	this->RemoveTop(this->Count() - count);
#endif
}

void ScriptList::KeepList(ScriptList *list)
{
#ifdef WITH_RUST
	openttd_rust_list_combine(this->owner.get(), list->owner.get(), 3);
#else
	if (list == this) return;

	this->modifications++;

	ScriptList tmp;
	tmp.AddList(this);
	tmp.RemoveList(list);
	this->RemoveList(&tmp);
#endif
}

SQInteger ScriptList::_get(HSQUIRRELVM vm)
{
#ifdef WITH_RUST
	return VMControl(vm, this, 4).Drive();
#else
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	auto item_iter = this->items.find(idx);
	if (item_iter == this->items.end()) return SQ_ERROR;

	sq_pushinteger(vm, item_iter->second);
	return 1;
#endif
}

SQInteger ScriptList::_set(HSQUIRRELVM vm)
{
#ifdef WITH_RUST
	return VMControl(vm, this, 5).Drive();
#else
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	/* Retrieve the return value */
	SQInteger val;
	switch (sq_gettype(vm, 3)) {
		case OT_NULL:
			this->RemoveItem(idx);
			return 0;

		case OT_BOOL: {
			SQBool v;
			sq_getbool(vm, 3, &v);
			val = v ? 1 : 0;
			break;
		}

		case OT_INTEGER:
			sq_getinteger(vm, 3, &val);
			break;

		default:
			return sq_throwerror(vm, "you can only assign integers to this list");
	}

	if (!this->HasItem(idx)) {
		this->AddItem(idx, val);
		return 0;
	}

	this->SetValue(idx, val);
	return 0;
#endif
}

SQInteger ScriptList::_nexti(HSQUIRRELVM vm)
{
#ifdef WITH_RUST
	return VMControl(vm, this, 6).Drive();
#else
	if (sq_gettype(vm, 2) == OT_NULL) {
		if (this->IsEmpty()) {
			sq_pushnull(vm);
			return 1;
		}
		sq_pushinteger(vm, this->Begin());
		return 1;
	}

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	SQInteger val = this->Next();
	if (this->IsEnd()) {
		sq_pushnull(vm);
		return 1;
	}

	sq_pushinteger(vm, val);
	return 1;
#endif
}

SQInteger ScriptList::Valuate(HSQUIRRELVM vm)
{
#ifdef WITH_RUST
	return VMControl(vm, this, 0).Drive();
#else
	this->modifications++;

	/* The first parameter is the instance of ScriptList. */
	int nparam = sq_gettop(vm) - 1;

	if (nparam < 1) {
		return sq_throwerror(vm, "You need to give at least a Valuator as parameter to ScriptList::Valuate");
	}

	/* Make sure the valuator function is really a function, and not any
	 * other type. It's parameter 2 for us, but for the user it's the
	 * first parameter they give. */
	SQObjectType valuator_type = sq_gettype(vm, 2);
	if (valuator_type != OT_CLOSURE && valuator_type != OT_NATIVECLOSURE) {
		return sq_throwerror(vm, "parameter 1 has an invalid type (expected function)");
	}

	/* Don't allow docommand from a Valuator, as we can't resume in
	 * mid C++-code. */
	ScriptObject::DisableDoCommandScope disabler{};

	/* Limit the total number of ops that can be consumed by a valuate operation */
	SQOpsLimiter limiter(vm, MAX_VALUATE_OPS, "valuator function");

	/* Push the function to call */
	sq_push(vm, 2);

	for (const auto &item : this->items) {
		/* Check for changing of items. */
		int previous_modification_count = this->modifications;

		/* Push the root table as instance object, this is what squirrel does for meta-functions. */
		sq_pushroottable(vm);
		/* Push all arguments for the valuator function. */
		sq_pushinteger(vm, item.first);
		for (int i = 0; i < nparam - 1; i++) {
			sq_push(vm, i + 3);
		}

		/* Call the function. Squirrel pops all parameters and pushes the return value. */
		if (SQ_FAILED(sq_call(vm, nparam + 1, SQTrue, SQFalse))) {
			return SQ_ERROR;
		}

		/* Retrieve the return value */
		SQInteger value;
		switch (sq_gettype(vm, -1)) {
			case OT_INTEGER: {
				sq_getinteger(vm, -1, &value);
				break;
			}

			case OT_BOOL: {
				SQBool v;
				sq_getbool(vm, -1, &v);
				value = v ? 1 : 0;
				break;
			}

			default: {
				/* See below for explanation. The extra pop is the return value. */
				sq_pop(vm, nparam + 4);

				return sq_throwerror(vm, "return value of valuator is not valid (not integer/bool)");
			}
		}

		/* Was something changed? */
		if (previous_modification_count != this->modifications) {
			/* See below for explanation. The extra pop is the return value. */
			sq_pop(vm, nparam + 4);

			return sq_throwerror(vm, "modifying valuated list outside of valuator function");
		}

		this->SetValue(item.first, value);

		/* Pop the return value. */
		sq_poptop(vm);

		Squirrel::DecreaseOps(vm, 5);
	}
	/* Pop from the squirrel stack:
	 * 1. The root stable (as instance object).
	 * 2. The valuator function.
	 * 3. The parameters given to this function.
	 * 4. The ScriptList instance object. */
	sq_pop(vm, nparam + 3);

	return 0;
#endif
}
