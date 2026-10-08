/* OpenTTD-Rust migration evidence, licensed under GPLv2. */
/** @file aircraft-range.cpp Finite NewGRF range gap against unchanged event bodies. */
#include "rust/aircraft_ffi.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>

static constexpr uint32_t INVALID_TILE = UINT32_MAX;
static constexpr uint8_t ENDTAKEOFF = 12, HELIENDLANDING = 18;
static constexpr uint8_t VAF_DEST_TOO_FAR = 0;
static constexpr int OT_GOTO_STATION = 1, OT_GOTO_DEPOT = 2, OT_LOADING = 3, OT_LEAVESTATION = 4;
static constexpr int WC_VEHICLE_VIEW = 0, WID_VV_START_STOP = 0, STR_NEWS_AIRCRAFT_DEST_TOO_FAR = 0;
static constexpr uint8_t _local_company = 0;
enum class VehState { Stopped = 1, Crashed = 7 };
enum class AdviceType { AircraftDestinationTooFar };
struct Status { uint8_t value = 0; bool Test(VehState bit) const { return (this->value & (1 << static_cast<int>(bit))) != 0; } };
struct Destination { uint16_t value; uint16_t ToStationID() const { return this->value; } };
struct Order { int type = OT_GOTO_STATION; uint16_t destination = 1; bool IsType(int type) const { return this->type == type; } Destination GetDestination() const { return {this->destination}; } };
struct Station { struct { uint32_t tile; } airport; static Station *GetIfValid(uint16_t id); };
struct Aircraft {
	Status vehstatus; Order current_order; struct { uint32_t cached_max_range_sqr; } acache;
	uint16_t targetairport = 0; uint8_t state = 1, flags = 0, owner = 0; uint32_t index = 17;
	void HandleBreakdown() {} void HandleLoading(bool) {}
};
static std::array<Station, 2> stations;
static std::array<bool, 2> valid;
static std::array<unsigned, 4> messages{};
Station *Station::GetIfValid(uint16_t id) { return id < 2 && valid[id] ? &stations[id] : nullptr; }
static uint32_t DistanceSquare(uint32_t a, uint32_t b) { int dx = (a & 63) - (b & 63), dy = (a >> 6) - (b >> 6); return dx * dx + dy * dy; }
static bool HasBit(uint8_t value, uint8_t bit) { return (value & (1 << bit)) != 0; }
static void SetBit(uint8_t &value, uint8_t bit) { value |= 1 << bit; }
static void ClrBit(uint8_t &value, uint8_t bit) { value &= ~(1 << bit); }
static void SetWindowWidgetDirty(int, uint32_t, int) { messages[0]++; }
struct ScriptEventAircraftDestTooFar { explicit ScriptEventAircraftDestTooFar(uint32_t) {} };
struct AI { static void NewEvent(uint8_t, ScriptEventAircraftDestTooFar *event) { delete event; messages[1]++; } };
static int GetEncodedString(int, uint32_t) { return 0; }
static void AddVehicleAdviceNewsItem(AdviceType, int, uint32_t) { messages[2]++; }
static void DeleteVehicleNews(uint32_t, AdviceType) { messages[3]++; }
static bool HandleCrashedAircraft(Aircraft *) { return true; }
static void HandleAircraftSmoke(Aircraft *, bool) {}
static void ProcessOrders(Aircraft *) {}
static void AirportGoToNextPosition(Aircraft *) {}
/* Generated solely from the pinned, unchanged src/aircraft_cmd.cpp. */
#include "aircraft-range-reference.inc"

struct Fixture {
	Aircraft aircraft;
	std::unique_ptr<OpenTTDAircraftState, decltype(&openttd_rust_aircraft_state_destroy)> state{openttd_rust_aircraft_state_new(), openttd_rust_aircraft_state_destroy};
	uint64_t blocks = 0;
	std::array<unsigned, 4> messages{};
};
static void OPENTTD_AIRCRAFT_CALL Read(void *context, uint32_t kind, uint32_t id, int64_t, int64_t, int64_t *out) noexcept
{
	auto &f = *static_cast<Fixture *>(context);
	switch (kind) {
		case 0: out[0] = out[1] = 64; out[2] = out[3] = 63; out[4] = 1; break;
		case 1:
			out[0] = id == 17 ? 2 : 4; out[3] = 1; out[8] = f.aircraft.vehstatus.value; out[9] = id == 17 ? 18 : 1048575;
			out[13] = 8; out[14] = 100; out[16] = f.aircraft.current_order.type; out[17] = f.aircraft.current_order.destination; out[34] = 3;
			break;
		case 2:
			if (id >= 2 || !valid[id]) break;
			out[0] = 1; out[1] = stations[id].airport.tile; out[2] = stations[id].airport.tile; out[4] = out[5] = 7; out[6] = 4; out[8] = out[9] = out[10] = 1;
			out[11] = reinterpret_cast<intptr_t>(&f.blocks); break;
		case 3: out[0] = out[1] = 1; out[3] = 1; break;
		case 4: out[0] = 1; break;
		case 5: out[3] = 1 << 30; break;
		case 6: out[2] = 16; break;
		case 8: out[0] = reinterpret_cast<intptr_t>(f.state.get()); break;
		case 11: out[0] = 100; out[1] = 1; break;
		case 18: out[0] = 1; break;
	}
}
static void OPENTTD_AIRCRAFT_CALL Write(void *, uint32_t, uint32_t, int64_t) noexcept {}
static int64_t OPENTTD_AIRCRAFT_CALL Service(void *context, const OpenTTDAircraftAction *action) noexcept
{
	auto &f = *static_cast<Fixture *>(context);
	if (action->kind == 8) f.messages[0]++;
	if (action->kind == 24) { f.messages[1]++; f.messages[2]++; }
	if (action->kind == 25) f.messages[3]++;
	return 0;
}
static uint32_t Random(void *) noexcept { std::abort(); }
static void Observe(void *, uint32_t, uint32_t *) noexcept {}
static void TileWrite(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept {}
static float Trig(uint32_t, float value) noexcept { return value; }
static uint32_t Industry(int32_t, int32_t, uint32_t *) noexcept { return 0; }
int main()
{
	unsigned cases = 0;
	for (uint32_t range : {0U, 1U, 16U, 64U, UINT32_MAX}) for (uint32_t destination : {0U, 1U, 4U, 8U})
	for (uint8_t state : {uint8_t{1}, uint8_t{10}, uint8_t{12}, uint8_t{14}, uint8_t{18}})
	for (uint8_t flag : {uint8_t{0}, uint8_t{1}}) for (unsigned validity = 0; validity < 4; validity++) {
		Fixture f; stations = {{{0}, {destination}}}; valid = {{(validity & 1) != 0, (validity & 2) != 0}};
		f.aircraft.acache.cached_max_range_sqr = range; f.aircraft.state = state; f.aircraft.flags = flag;
		f.state->cached_max_range_sqr = range; f.state->state = state; f.state->flags = flag; f.state->targetairport = 0;
		messages = {}; AircraftEventHandler(&f.aircraft, 0); auto expected = messages;
		const OpenTTDSharedServices services{nullptr, Random, Observe, TileWrite, Trig, Industry};
		std::unique_ptr<OpenTTDAircraftRun, decltype(&openttd_rust_aircraft_destroy)> run{openttd_rust_aircraft_create(18, 17, 0, 0, 0, 0, &f, Read, Write, &services, Service), openttd_rust_aircraft_destroy};
		for (;;) { auto action = openttd_rust_aircraft_advance(run.get(), 0); if (action.kind == 0) break; if (action.kind != 100) std::abort(); }
		if ((f.state->flags & 1) != (f.aircraft.flags & 1) || f.messages != expected) { std::fprintf(stderr, "range mismatch range=%u dest=%u state=%u flags=%u validity=%u\n", range, destination, state, flag, validity); return 1; }
		cases++;
	}
	std::printf("finite range transition: %u unchanged-reference cases passed\n", cases);
}
