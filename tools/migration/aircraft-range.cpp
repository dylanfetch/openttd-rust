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
#include "aircraft-boundary.hpp"
static Fixture *fixture;
static OpenTTDAircraftLeaves RangeLeaves()
{
	auto leaves = AircraftTestLeaves();
	leaves.map_size_x = []() noexcept { return uint32_t{64}; };
	leaves.station = [](uint16_t id) noexcept -> const void * { return Station::GetIfValid(id); };
	leaves.airport_tile = [](const void *s) noexcept { return static_cast<const Station *>(s)->airport.tile; };
	leaves.vehicle_status = [](OpenTTDAircraftVehicle) noexcept { return static_cast<uint8_t>(fixture->aircraft.vehstatus.value); };
	leaves.order_type = [](OpenTTDAircraftVehicle) noexcept { return static_cast<uint8_t>(fixture->aircraft.current_order.type); };
	leaves.order_destination = [](OpenTTDAircraftVehicle) noexcept { return fixture->aircraft.current_order.destination; };
	leaves.handle_breakdown = [](OpenTTDAircraftVehicle) noexcept {};
	leaves.process_orders = [](OpenTTDAircraftVehicle) noexcept {};
	leaves.handle_loading = [](OpenTTDAircraftVehicle, bool) noexcept {};
	leaves.dirty_start_stop = [](OpenTTDAircraftVehicle) noexcept { fixture->messages[0]++; };
	leaves.destination_too_far = [](OpenTTDAircraftVehicle) noexcept { fixture->messages[1]++; fixture->messages[2]++; };
	leaves.delete_range_news = [](OpenTTDAircraftVehicle) noexcept { fixture->messages[3]++; };
	leaves.airport_fta = [](const void *) noexcept -> const void * { return &fixture->blocks; };
	leaves.station_tile = [](const void *s) noexcept { return static_cast<const Station *>(s)->airport.tile; };
	leaves.rotation = [](const void *) noexcept { return uint8_t{0}; };
	leaves.airport_width = leaves.airport_height = [](const void *) noexcept { return uint16_t{7}; };
	leaves.x = leaves.y = [](OpenTTDAircraftVehicle) noexcept { return int32_t{0}; };
	leaves.direction = [](OpenTTDAircraftVehicle) noexcept { return uint8_t{0}; };
	leaves.dummy_airport = []() noexcept -> const void * { return &fixture->blocks; };
	leaves.airport_elements = [](const void *) noexcept { return uint8_t{1}; };
	leaves.moving = [](const void *, uint8_t) noexcept { return OpenTTDAircraftMoving{0, 0, 16, 0}; };
	leaves.set_current_speed = [](OpenTTDAircraftVehicle, uint16_t) noexcept {};
	leaves.node = [](const void *ap, uint8_t) noexcept -> const void * { return ap; };
	leaves.fta = [](const void *) noexcept { return OpenTTDAircraftNode{nullptr, uint64_t{1} << 30, 0, 0, 0}; };
	leaves.airport_blocks = [](const void *) noexcept { return &fixture->blocks; };
	leaves.set_subspeed = [](OpenTTDAircraftVehicle, uint8_t) noexcept {};
	leaves.speed_property = [](OpenTTDAircraftVehicle) noexcept { return uint32_t{0}; };
	leaves.engine_speed = [](OpenTTDAircraftVehicle) noexcept { return uint16_t{100}; };
	leaves.set_maximum_speed = [](OpenTTDAircraftVehicle, uint16_t) noexcept {};
	leaves.set_cargo_age = [](OpenTTDAircraftVehicle, uint16_t) noexcept {};
	leaves.cargo_age_property = [](OpenTTDAircraftVehicle) noexcept { return uint32_t{0}; };
	leaves.next = [](OpenTTDAircraftVehicle v) noexcept { return v; };
	leaves.map_max_x = leaves.map_max_y = []() noexcept { return uint32_t{63}; };
	leaves.plane_speed = []() noexcept { return uint8_t{1}; };
	leaves.z = [](OpenTTDAircraftVehicle) noexcept { return int32_t{1}; };
	leaves.subtype = [](OpenTTDAircraftVehicle v) noexcept { return uint8_t(v.id == 17 ? 2 : 4); };
	leaves.vehicle_type = [](OpenTTDAircraftVehicle) noexcept { return uint8_t{3}; };
	leaves.maximum_speed = [](OpenTTDAircraftVehicle) noexcept { return uint16_t{100}; };
	leaves.acceleration = [](OpenTTDAircraftVehicle) noexcept { return uint8_t{8}; };
	leaves.slope = [](int32_t, int32_t) noexcept { return int32_t{0}; };
	leaves.tile_height = [](uint32_t) noexcept { return int32_t{0}; };
	leaves.airport_entry = [](const void *, uint8_t) noexcept { return uint8_t{0}; };
	leaves.set_x = leaves.set_y = leaves.set_z = [](OpenTTDAircraftVehicle, int32_t) noexcept {};
	leaves.update_position = leaves.position_viewport = [](OpenTTDAircraftVehicle) noexcept {};
	leaves.copy_sprite = [](OpenTTDAircraftVehicle, OpenTTDAircraftVehicle) noexcept {};
	leaves.next = [](OpenTTDAircraftVehicle v) noexcept { if (v.id == 17) { v.id = 18; return v; } return OpenTTDAircraftVehicle{nullptr, nullptr, 1048575}; };
	leaves.current_speed = [](OpenTTDAircraftVehicle) noexcept { return uint16_t{0}; };
	leaves.can_use_station = [](OpenTTDAircraftVehicle, uint16_t) noexcept { return true; };
	leaves.station_owner = [](const void *) noexcept { return uint8_t{0}; };
	leaves.owner = [](OpenTTDAircraftVehicle) noexcept { return uint8_t{0}; };
	leaves.helipads = [](const void *) noexcept { return uint8_t{0}; };
	leaves.airport_flags = [](const void *) noexcept { return uint8_t{1}; };
	leaves.waiting_unbunching = [](OpenTTDAircraftVehicle) noexcept { return false; };
	return leaves;
}
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
		fixture = &f;
		const auto leaves = RangeLeaves();
		openttd_rust_aircraft_event(&leaves, {&f, f.state.get(), 17}, false);
		if ((f.state->flags & 1) != (f.aircraft.flags & 1) || f.messages != expected) { std::fprintf(stderr, "range mismatch range=%u dest=%u state=%u flags=%u validity=%u\n", range, destination, state, flag, validity); return 1; }
		cases++;
	}
	std::printf("finite range transition: %u unchanged-reference cases passed\n", cases);
}
