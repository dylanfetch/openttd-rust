/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
/** @file company_economy.cpp Native owner layout and network deferred-delete gap. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#ifdef WITH_RUST
#include "../company_func.h"
#include "../company_base.h"
#include "../economy_type.h"
#include "../rust/abi_ffi.h"
#include "../rust/company_ffi.h"
#endif
#include "../safeguards.h"
#ifdef WITH_RUST
TEST_CASE("Company economy - canonical owner layouts and property copy lifetime")
{
	auto layout = [](uint8_t type, std::initializer_list<size_t> fields) {
		uint8_t i = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, i++) == expected);
		CHECK(openttd_rust_abi_layout(type, i) == SIZE_MAX);
	};
	layout(210, {sizeof(CompanyEconomyEntry), alignof(CompanyEconomyEntry), offsetof(CompanyEconomyEntry, income), offsetof(CompanyEconomyEntry, expenses), offsetof(CompanyEconomyEntry, delivered_cargo), offsetof(CompanyEconomyEntry, performance_history), offsetof(CompanyEconomyEntry, company_value)});
	layout(211, {sizeof(CompanyFinances), alignof(CompanyFinances), offsetof(CompanyFinances, money), offsetof(CompanyFinances, money_fraction), offsetof(CompanyFinances, current_loan), offsetof(CompanyFinances, max_loan), offsetof(CompanyFinances, block_preview), offsetof(CompanyFinances, months_empty), offsetof(CompanyFinances, months_of_bankruptcy), offsetof(CompanyFinances, bankrupt_asked), offsetof(CompanyFinances, bankrupt_timeout), offsetof(CompanyFinances, bankrupt_value), offsetof(CompanyFinances, terraform_limit), offsetof(CompanyFinances, clear_limit), offsetof(CompanyFinances, tree_limit), offsetof(CompanyFinances, build_object_limit), offsetof(CompanyFinances, yearly_expenses), offsetof(CompanyFinances, cur_economy), offsetof(CompanyFinances, old_economy), offsetof(CompanyFinances, num_valid_stat_ent)});
	layout(212, {sizeof(Economy), alignof(Economy), offsetof(Economy, max_loan), offsetof(Economy, fluct), offsetof(Economy, interest_rate), offsetof(Economy, infl_amount), offsetof(Economy, infl_amount_pr), offsetof(Economy, inflation_prices), offsetof(Economy, inflation_payment), offsetof(Economy, old_max_loan_unround), offsetof(Economy, old_max_loan_unround_fract)});
	layout(213, {sizeof(OpenTTDCompanyAction), alignof(OpenTTDCompanyAction), offsetof(OpenTTDCompanyAction, kind), offsetof(OpenTTDCompanyAction, id), offsetof(OpenTTDCompanyAction, a), offsetof(OpenTTDCompanyAction, b), offsetof(OpenTTDCompanyAction, c), offsetof(OpenTTDCompanyAction, d)});
	layout(214, {sizeof(OpenTTDCompanyLeaves), alignof(OpenTTDCompanyLeaves), offsetof(OpenTTDCompanyLeaves, next), offsetof(OpenTTDCompanyLeaves, read), offsetof(OpenTTDCompanyLeaves, owner), offsetof(OpenTTDCompanyLeaves, service)});
	CompanyProperties original;
	original.Finances().money = INT64_MIN;
	original.Finances().old_economy[23].delivered_cargo[63] = UINT32_MAX;
	auto *address = &original.Finances().old_economy[23].delivered_cargo[63];
	CompanyProperties copy = original;
	CHECK(&copy.Finances() != &original.Finances());
	CHECK(copy.Finances().money == Money(INT64_MIN));
	CHECK(copy.Finances().old_economy[23].delivered_cargo[63] == UINT32_MAX);
	copy.Finances().money = 17;
	original = copy;
	CHECK(original.Finances().money == Money(17));
	CHECK(address == &original.Finances().old_economy[23].delivered_cargo[63]);
	copy.Finances().money = 19;
	CHECK(original.Finances().money == Money(17));
}

/* Semantic saves cannot witness a client waiting for a server-posted deletion.
 * These fake world leaves exercise the real Rust monthly dispatcher with source
 * network modes. They check posting and post-deletion iteration, not multiplayer
 * transport or an additional oracle for ordinary financial calculations. */
static std::array<std::unique_ptr<CompanyProperties>, 3> network_companies;
static bool company_networking, company_server;
static uint8_t company_current;
static std::vector<uint32_t> company_admin_updates;
static uint32_t OPENTTD_COMPANY_CALL CompanyGapNext(uint32_t kind, uint32_t from)
{
	if (kind != 1) return UINT32_MAX;
	for (uint32_t i = from; i < network_companies.size(); i++) if (network_companies[i] != nullptr) return i;
	return UINT32_MAX;
}
static void *OPENTTD_COMPANY_CALL CompanyGapOwner(uint32_t id)
{
	return id < network_companies.size() && network_companies[id] != nullptr ? &network_companies[id]->Finances() : nullptr;
}
static void OPENTTD_COMPANY_CALL CompanyGapRead(uint32_t kind, uint32_t, int64_t, int64_t *v)
{
	std::fill_n(v, 32, 0);
	if (kind == 0) { v[2] = company_networking; v[3] = company_server; v[4] = 0; v[5] = company_current; v[7] = 1; }
}
static int64_t OPENTTD_COMPANY_CALL CompanyGapService(const OpenTTDCompanyAction *x)
{
	if (x->kind == 2) company_current = static_cast<uint8_t>(x->id);
	if (x->kind == 6) company_admin_updates.push_back(x->id);
	return 0;
}
TEST_CASE("Company economy - network deferred deletion and synchronous pool iteration")
{
	OpenTTDCompanyLeaves leaves{CompanyGapNext, CompanyGapRead, CompanyGapOwner, CompanyGapService};
	for (int mode : {0, 1, 2}) {
		company_networking = mode != 0; company_server = mode == 1; company_current = 18; company_admin_updates.clear();
		auto *economy = new (openttd_rust_economy_state()) Economy{}; economy->max_loan = 300000; economy->fluct = 100;
		for (auto &p : network_companies) p = std::make_unique<CompanyProperties>();
		network_companies[0]->Finances().money = 1000000;
		for (uint32_t i : {1U, 2U}) {
			network_companies[i]->Finances().money = -500000;
			network_companies[i]->Finances().current_loan = 100000;
			network_companies[i]->Finances().months_of_bankruptcy = 9;
		}
		std::unique_ptr<OpenTTDCompanyRun, decltype(&openttd_rust_company_destroy)> run(openttd_rust_company_create(7, 0, 0, 0, 0, 0, &leaves), openttd_rust_company_destroy);
		std::vector<uint32_t> posts;
		for (;;) {
			auto action = openttd_rust_company_advance(run.get(), 0);
			if (action.kind == 0) break;
			REQUIRE(action.kind == 1000); CHECK(action.a == 2); CHECK(action.b == 2); CHECK(action.c == UINT32_MAX);
			posts.push_back(action.id);
			if (mode == 0) network_companies[action.id].reset(); // actual singleplayer Post can synchronously delete
		}
		CHECK(company_current == 18);
		if (mode == 2) { CHECK(posts.empty()); CHECK(company_admin_updates == std::vector<uint32_t>{1, 2}); }
		else { CHECK(posts == std::vector<uint32_t>{1, 2}); CHECK(company_admin_updates.empty()); }
		for (uint32_t i : {1U, 2U}) CHECK((network_companies[i] == nullptr) == (mode == 0));
	}
	for (auto &p : network_companies) p.reset();
}
#endif /* WITH_RUST */
