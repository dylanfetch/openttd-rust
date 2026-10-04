/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
/** @file company-command-reference.cpp Financial command boundaries unavailable to AI scenarios. */
#include "stdafx.h"
#include "core/overflowsafe_type.hpp"
#include "core/bitmath_func.hpp"
#include "rust/company_ffi.h"
#include "rust/abi_ffi.h"
using Money = OverflowSafeInt64;
using CompanyID = uint8_t;
using TileIndex = uint32_t;
enum ExpensesType : uint8_t { EXPENSES_CONSTRUCTION, EXPENSES_NEW_VEHICLES, EXPENSES_TRAIN_RUN, EXPENSES_ROADVEH_RUN, EXPENSES_AIRCRAFT_RUN, EXPENSES_SHIP_RUN, EXPENSES_PROPERTY, EXPENSES_TRAIN_REVENUE, EXPENSES_ROADVEH_REVENUE, EXPENSES_AIRCRAFT_REVENUE, EXPENSES_SHIP_REVENUE, EXPENSES_LOAN_INTEREST, EXPENSES_OTHER, EXPENSES_END, INVALID_EXPENSES = 255 };
enum class LoanCommand : uint8_t { Interval, Max, Amount };
enum class DoCommandFlag { Execute };
struct DoCommandFlags { bool execute; bool Test(DoCommandFlag) const { return this->execute; } };
enum Error { STR_ERROR_MAXIMUM_PERMITTED_LOAN = 2, STR_ERROR_LOAN_ALREADY_REPAID, STR_ERROR_CURRENCY_REQUIRED, STR_ERROR_INSUFFICIENT_FUNDS };
struct CommandCost {
	int error = 0; Money cost = 0; ExpensesType expense = INVALID_EXPENSES; Money param = 0;
	CommandCost() = default;
	CommandCost(ExpensesType expense, Money cost = 0) : cost(cost), expense(expense) {}
	CommandCost(Error error) : error(error) {}
	Money GetCost() const { return this->cost; }
	ExpensesType GetExpensesType() const { return this->expense; }
};
static const CommandCost CMD_ERROR = [] { CommandCost c; c.error = 1; return c; }();
static CommandCost CommandCostWithParam(Error error, Money param) { CommandCost c(error); c.param = param; return c; }
static constexpr int LOAN_INTERVAL = 10000, MAX_LOAN_LIMIT = 2000000000, OWNER_DEITY = 18, TILE_SIZE = 16;
static const Money COMPANY_MAX_LOAN_DEFAULT = INT64_MIN;
static CompanyID _current_company;
static bool _networking;
static struct { struct { bool infinite_money; } difficulty; struct { bool give_money; } economy; } _settings_game;
static struct { Money max_loan = 300000; } _economy;
struct Company {
	Money money = 0, current_loan = 0, max_loan = COMPANY_MAX_LOAN_DEFAULT;
	std::array<std::array<Money, 13>, 3> yearly_expenses{};
	struct { Money income = 0, expenses = 0; } cur_economy;
	CompanyID index;
	Money GetMaxLoan() const;
	static bool IsValidID(CompanyID id) { return id < 3; }
	static Company *Get(CompanyID);
	static Company *GetIfValid(CompanyID id) { return IsValidID(id) ? Get(id) : nullptr; }
};
static std::array<Company, 3> companies;
Company *Company::Get(CompanyID id) { return &companies[id]; }
using Trace = std::tuple<uint32_t, uint32_t, int64_t>;
static std::vector<Trace> trace;
static void InvalidateCompanyWindows(const Company *c) { trace.emplace_back(3, c->index, 0); }
template <typename T> struct Backup {
	T &target; T old;
	Backup(T &target, T value) : target(target), old(target) { target = value; }
	void Restore() { this->target = this->old; }
};
static void SubtractMoneyFromAnyCompany(Company *, const CommandCost &);
static void SubtractMoneyFromCompany(const CommandCost &c) { SubtractMoneyFromAnyCompany(Company::Get(_current_company), c); }
static int TileX(TileIndex tile) { return tile; }
static int TileY(TileIndex) { return 0; }
static int GetTilePixelZ(TileIndex) { return 0; }
static void ShowCostOrIncomeAnimation(int x, int, int, Money cost) { trace.emplace_back(53, x / TILE_SIZE, cost.base()); }
static constexpr int STR_COMPANY_NAME = 0, NETWORK_ACTION_GIVE_MONEY = 0;
static std::string GetString(int, CompanyID id) { return std::to_string(id); }
static int GetDrawStringCompanyColour(CompanyID id) { return id; }
static void NetworkTextMessage(int, int, bool, const std::string &, const std::string &dest, Money cost) { trace.emplace_back(52, std::stoi(dest), cost.base()); }
#include "company-command-reference.inc"

/* The game ABI test checks complete layouts. This probe addresses only fields
 * touched by these unchanged command bodies, through their audited ABI offsets. */
static std::array<void *, 3> owners;
static int64_t &Field(void *p, uint8_t item) { return *reinterpret_cast<int64_t *>(static_cast<char *>(p) + openttd_rust_abi_layout(211, item)); }
static uint32_t OPENTTD_COMPANY_CALL Next(uint32_t, uint32_t) { return UINT32_MAX; }
static void *OPENTTD_COMPANY_CALL Owner(uint32_t id) { return id < owners.size() ? owners[id] : nullptr; }
static void OPENTTD_COMPANY_CALL Read(uint32_t kind, uint32_t, int64_t, int64_t *v)
{
	std::fill_n(v, 32, 0);
	if (kind == 0) { v[2] = _networking; v[5] = _current_company; v[6] = _settings_game.difficulty.infinite_money; v[11] = _settings_game.economy.give_money; }
}
static int64_t OPENTTD_COMPANY_CALL Service(const OpenTTDCompanyAction *a)
{
	if (a->kind == 2) _current_company = a->id;
	else trace.emplace_back(a->kind, a->id, a->a);
	return 0;
}
static OpenTTDCompanyLeaves leaves{Next, Read, Owner, Service};
static void Reset(int64_t money, int64_t loan, int64_t max, bool infinite, bool networking, bool give, CompanyID current)
{
	_current_company = current; _networking = networking; _settings_game = {{infinite}, {give}}; trace.clear();
	for (uint32_t i = 0; i < owners.size(); i++) {
		companies[i] = {}; companies[i].index = i; companies[i].money = money; companies[i].current_loan = loan; companies[i].max_loan = max;
		std::memset(owners[i], 0, openttd_rust_abi_layout(211, 0)); Field(owners[i], 2) = money; Field(owners[i], 4) = loan; Field(owners[i], 5) = max;
	}
	*static_cast<int64_t *>(openttd_rust_economy_state()) = 300000;
}
static bool Equal(const CommandCost &expected, const OpenTTDCompanyAction &actual)
{
	if (actual.kind != 0 || expected.error != actual.a || expected.cost.base() != actual.b || expected.expense != actual.c || expected.param.base() != actual.d) return false;
	for (uint32_t i = 0; i < owners.size(); i++) {
		const auto &c = companies[i]; void *p = owners[i];
		if (c.money.base() != Field(p, 2) || c.current_loan.base() != Field(p, 4) || c.max_loan.base() != Field(p, 5)) return false;
		auto *expense = reinterpret_cast<int64_t *>(static_cast<char *>(p) + openttd_rust_abi_layout(211, 16));
		for (uint32_t year = 0; year < 3; year++) for (uint32_t type = 0; type < 13; type++) if (c.yearly_expenses[year][type].base() != expense[year * 13 + type]) return false;
		auto *entry = static_cast<char *>(p) + openttd_rust_abi_layout(211, 17);
		if (c.cur_economy.income.base() != *reinterpret_cast<int64_t *>(entry + openttd_rust_abi_layout(210, 2)) || c.cur_economy.expenses.base() != *reinterpret_cast<int64_t *>(entry + openttd_rust_abi_layout(210, 3))) return false;
	}
	return true;
}
int main()
{
	for (auto &p : owners) p = openttd_rust_company_state_create();
	uint64_t cases = 0;
	for (uint32_t op : {21, 22, 23, 24, 25}) for (bool execute : {false, true}) {
		for (int64_t money : {INT64_MIN, int64_t(-1), int64_t(0), int64_t(9999), int64_t(10000), int64_t(30000000), INT64_MAX}) {
			for (int64_t loan : {int64_t(0), int64_t(100000), int64_t(300000), INT64_MAX}) for (int64_t amount : {INT64_MIN, int64_t(-1), int64_t(0), int64_t(9999), int64_t(10000), int64_t(10001), int64_t(20000001), int64_t(2000000001), INT64_MAX}) {
				for (uint8_t variant : {0, 1, 2, 3, 12, 13, 255}) {
					bool infinite = variant == 3, networking = variant == 12, give = variant != 13;
					CompanyID current = op >= 24 && variant != 0 ? OWNER_DEITY : 0;
					uint32_t id = (op == 21 || op >= 24) && variant == 255 ? 255 : 1;
					int64_t max = variant == 2 ? 2000000000 : COMPANY_MAX_LOAN_DEFAULT.base();
					Reset(money, loan, max, infinite, networking, give, current);
					CommandCost expected;
					int64_t a = amount, b = execute, c = 0, d = 0;
					if (op == 21) expected = CmdGiveMoney({execute}, amount, id);
					if (op == 22 || op == 23) { a = variant; b = amount; c = execute; expected = op == 22 ? CmdIncreaseLoan({execute}, LoanCommand(variant), amount) : CmdDecreaseLoan({execute}, LoanCommand(variant), amount); }
					if (op == 24) expected = CmdSetCompanyMaxLoan({execute}, id, amount);
					if (op == 25) { b = variant; c = execute; d = variant % 2; expected = CmdChangeBankBalance({execute}, d, amount, id, ExpensesType(variant)); }
					auto expected_trace = trace; auto expected_current = _current_company; trace.clear(); _current_company = current;
					auto *run = openttd_rust_company_create(op, id, a, b, c, d, &leaves);
					auto actual = openttd_rust_company_advance(run, 0); openttd_rust_company_destroy(run);
					if (!Equal(expected, actual) || expected_trace != trace || expected_current != _current_company) {
						std::fprintf(stderr, "financial command gap failed: op=%u execute=%d money=%lld loan=%lld amount=%lld variant=%u result=%lld/%lld/%lld/%lld expected=%d/%lld/%u/%lld\n", op, execute, static_cast<long long>(money), static_cast<long long>(loan), static_cast<long long>(amount), variant, static_cast<long long>(actual.a), static_cast<long long>(actual.b), static_cast<long long>(actual.c), static_cast<long long>(actual.d), expected.error, static_cast<long long>(expected.cost.base()), expected.expense, static_cast<long long>(expected.param.base())); return 1;
					}
					cases++;
				}
			}
		}
	}
	for (auto p : owners) openttd_rust_company_state_destroy(p);
	std::printf("company financial command gap: %llu unchanged-reference cases passed\n", static_cast<unsigned long long>(cases));
}
