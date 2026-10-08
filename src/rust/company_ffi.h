/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
/** @file company_ffi.h Canonical company finances and economy control. */
#ifndef RUST_COMPANY_FFI_H
#define RUST_COMPANY_FFI_H
#include <cstdint>
#if defined(_MSC_VER)
#define OPENTTD_COMPANY_CALL __cdecl
#elif defined(__i386__)
#define OPENTTD_COMPANY_CALL __attribute__((cdecl))
#else
#define OPENTTD_COMPANY_CALL
#endif
enum CompanyDirectService : uint32_t {
	RustCompanyLeafSharedRandom = 1,
	RustCompanyLeafSetCurrentCompany = 2,
	RustCompanyLeafInvalidateCompanyWindows = 3,
	RustCompanyLeafPerformanceDirty = 4,
	RustCompanyLeafUpdateHeadquarters = 5,
	RustCompanyLeafAdminUpdate = 6,
	RustCompanyLeafTroubleNews = 7,
	RustCompanyLeafFinancialGraphsDirty = 8,
	RustCompanyLeafRailMaintenance = 9,
	RustCompanyLeafSignalMaintenance = 10,
	RustCompanyLeafRoadMaintenance = 11,
	RustCompanyLeafCanalMaintenance = 12,
	RustCompanyLeafStationMaintenance = 13,
	RustCompanyLeafAirportMaintenance = 14,
	RustCompanyLeafRecessionNews = 15,
	RustCompanyLeafPriceWindowsDirty = 16,
	RustCompanyLeafSetCargoPayment = 17,
	RustCompanyLeafIndustryStartup = 18,
	RustCompanyLeafClearMonitors = 19,
	RustCompanyLeafResetCompetitorTimer = 20,
	RustCompanyLeafAbortCompetitorTimer = 21,
	RustCompanyLeafCompanyIdentity = 22,
	RustCompanyLeafMergerNews = 23,
	RustCompanyLeafAcquisitionWindows = 24,
	RustCompanyLeafBankruptEvents = 25,
	RustCompanyLeafCompanyControlWindows = 26,
	RustCompanyLeafNetworkCompanyNew = 27,
	RustCompanyLeafCloseNetworkProgress = 28,
	RustCompanyLeafNetworkCreationFailed = 29,
	RustCompanyLeafNetworkClientCreated = 30,
	RustCompanyLeafCountGroupVehicle = 31,
	RustCompanyLeafClearReplacementRules = 32,
	RustCompanyLeafTransferGroup = 33,
	RustCompanyLeafCopyServiceDefaults = 34,
	RustCompanyLeafServiceInterval = 35,
	RustCompanyLeafTransferVehicleOwner = 36,
	RustCompanyLeafAssignUnitNumber = 37,
	RustCompanyLeafUpdateAutoreplace = 38,
	RustCompanyLeafAddSignalTrack = 39,
	RustCompanyLeafUpdateCrossing = 40,
	RustCompanyLeafFlushSignals = 41,
	RustCompanyLeafTransferAirportCount = 42,
	RustCompanyLeafStationOwner = 43,
	RustCompanyLeafTownRating = 44,
	RustCompanyLeafTownExclusivity = 45,
	RustCompanyLeafSubsidyOwner = 46,
	RustCompanyLeafWaypointSignOwner = 47,
	RustCompanyLeafTransferWindowOwner = 48,
	RustCompanyLeafScreenDirty = 49,
	RustCompanyLeafSetLocalCompany = 50,
	RustCompanyLeafClientsToSpectators = 51,
	RustCompanyLeafGiveMoneyMessage = 52,
	RustCompanyLeafMoneyAnimation = 53,
	RustCompanyLeafFinancesDirty = 54,
	RustCompanyLeafShowFinances = 55,
	RustCompanyLeafNewYearSound = 56,
	RustCompanyLeafInteractiveCompany = 57,
	RustCompanyLeafShowTakeoverDialog = 58,
	RustCompanyLeafAskMergerEvent = 59,
	RustCompanyLeafScriptRandomNext = 60,
	RustCompanyLeafServicePercent = 61,
	RustCompanyLeafRebuildSubsidyCache = 62,
	RustCompanyLeafBankruptNews = 63,
	RustCompanyLeafNewCompanyEvents = 64,
	RustCompanyLeafFluctuatingEconomy = 65,
	RustCompanyLeafAssertNewAISlot = 66,
};
enum CompanyReentryService : uint32_t {
	RustCompanyLeafPostCompanyControl = 1000,
	RustCompanyLeafStartAI = 1001,
	RustCompanyLeafStopAI = 1002,
	RustCompanyLeafDeleteCompany = 1003,
	RustCompanyLeafChangeTileOwner = 1004,
	RustCompanyLeafDeletePoolObject = 1005,
	RustCompanyLeafChangeServiceInterval = 1007,
	RustCompanyLeafAllocateCompany = 1008,
};
struct OpenTTDCompanyAction { uint32_t kind, id; int64_t a, b, c, d; };
struct OpenTTDCompanyRun;
struct OpenTTDCompanyLeaves {
	uint32_t (OPENTTD_COMPANY_CALL *next)(uint32_t, uint32_t);
	void (OPENTTD_COMPANY_CALL *read)(uint32_t, uint32_t, int64_t, int64_t *);
	void *(OPENTTD_COMPANY_CALL *owner)(uint32_t);
	int64_t (OPENTTD_COMPANY_CALL *service)(const OpenTTDCompanyAction *);
};
/* All entry points run serially on the game thread. Rust owns the allocations;
 * C++ starts the Money/mask/array object lifetimes in their raw storage and keeps
 * stable field addresses for existing readers/writers and save adapters. Rust
 * accesses only raw fields, without references spanning shared services. No
 * mirror or copy-back exists. C++ property copies construct independent owners.
 * Save/load errors run entirely outside Rust; ordinary/indexed cleanup destroys
 * each owner exactly once. Raw addresses expire at the owning object's deletion.
 * Direct leaves are noexcept, environmental failures terminate. Only named VM,
 * command-post, tile-handler, object-deletion and nested-command actions return
 * to C++. Continuations retain copied IDs/scalars, never borrowed world storage.
 * Native widths/layout are checked by ABI210-214. Rust panics/OOM abort. */
extern "C" {
void *OPENTTD_COMPANY_CALL openttd_rust_company_state_create();
void OPENTTD_COMPANY_CALL openttd_rust_company_state_destroy(void *);
void OPENTTD_COMPANY_CALL openttd_rust_company_initialize(void *, uint32_t, uint32_t, uint32_t, uint32_t);
void *OPENTTD_COMPANY_CALL openttd_rust_economy_state();
int64_t *OPENTTD_COMPANY_CALL openttd_rust_economy_prices();
int64_t *OPENTTD_COMPANY_CALL openttd_rust_company_scores();
uint32_t *OPENTTD_COMPANY_CALL openttd_rust_company_tick();
OpenTTDCompanyRun *OPENTTD_COMPANY_CALL openttd_rust_company_create(uint32_t, uint32_t, int64_t, int64_t, int64_t, int64_t, const OpenTTDCompanyLeaves *);
OpenTTDCompanyAction OPENTTD_COMPANY_CALL openttd_rust_company_advance(OpenTTDCompanyRun *, int64_t);
void OPENTTD_COMPANY_CALL openttd_rust_company_destroy(OpenTTDCompanyRun *);
}
#endif /* RUST_COMPANY_FFI_H */
