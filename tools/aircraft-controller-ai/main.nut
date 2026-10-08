/* OpenTTD-Rust migration evidence, licensed under GPLv2.
 * Reference builds the fixture; frozen commands execute in both comparison roles. */
require("parameters.nut");
class MigrationAircraftController extends AIController {
	function Check(label, ok) {
		print("AIRCRAFT-CONTROL-BUILD " + label + " " + ok);
		if (!ok) throw label + ": " + AIError.GetLastErrorString();
	}
	function Airport(town, type) {
		local center = AITown.GetLocation(town);
		local cx = AIMap.GetTileX(center), cy = AIMap.GetTileY(center);
		local width = AIAirport.GetAirportWidth(type), height = AIAirport.GetAirportHeight(type);
		local radius = AIAirport.GetAirportCoverageRadius(type);
		for (local y = cy - 10; y <= cy + 10; y++) for (local x = cx - 10; x <= cx + 10; x++) {
			if (x < 1 || y < 1 || x + width >= AIMap.GetMapSizeX() || y + height >= AIMap.GetMapSizeY()) continue;
			local tile = AIMap.GetTileIndex(x, y);
			if (AITile.GetCargoAcceptance(tile, 0, width, height, radius) < 8) continue;
			local possible;
			{ local test = AITestMode(); possible = AIAirport.BuildAirport(tile, type, AIStation.STATION_NEW); }
			if (!possible) continue;
			this.Check("airport-" + type, AIAirport.BuildAirport(tile, type, AIStation.STATION_NEW));
			return tile;
		}
		return -1;
	}
	function Start() {
		AIController.SetCommandDelay(1);
		if (AIRCRAFT_ACTION == "landing-event") {
			while (true) {
				while (AIEventController.IsEventWaiting()) {
					local event = AIEventController.GetNextEvent();
					if (event.GetEventType() != AIEvent.ET_VEHICLE_CRASHED) continue;
					local crash = AIEventVehicleCrashed.Convert(event);
					print("LANDING-EVENT " + crash.GetVehicleID() + " " + crash.GetCrashSite() + " " + crash.GetCrashReason() + " " + crash.GetVictims());
				}
				this.Sleep(1);
			}
		}
		if (AIRCRAFT_ACTION == "crash") {
			local pending = AIRCRAFT_AIRPORTS;
			while (pending.len() != 0) {
				local remaining = [];
				foreach (tile in pending) {
					if (!AIAirport.RemoveAirport(tile)) remaining.append(tile);
				}
				pending = remaining;
				this.Sleep(1);
			}
			print("AIRCRAFT-ACTION crash true");
			while (true) this.Sleep(1000);
		}
		if (AIRCRAFT_ACTION != "none") {
			local ok = AIRCRAFT_ACTION == "removal" ? AIAirport.RemoveAirport(AIRCRAFT_TARGET) : AIStation.OpenCloseAirport(AIRCRAFT_TARGET);
			this.Check(AIRCRAFT_ACTION, ok);
			print("AIRCRAFT-ACTION " + AIRCRAFT_ACTION + " " + ok);
			while (true) this.Sleep(1000);
		}
		this.Check("loan", AICompany.SetLoanAmount(AICompany.GetMaxLoanAmount()));
		local towns = AITownList();
		towns.Valuate(AITown.GetPopulation);
		towns.Sort(AIList.SORT_BY_VALUE, false);
		local first = -1, last = -1, pad = -1;
		foreach (town, population in towns) {
			if (first == -1) first = this.Airport(town, AIAirport.AT_INTERNATIONAL);
			else if (AIMap.DistanceManhattan(AITown.GetLocation(town), first) >= 35) {
				if (last == -1) last = this.Airport(town, AIAirport.AT_INTERNATIONAL);
				else if (AIMap.DistanceManhattan(AITown.GetLocation(town), last) >= 25) {
					pad = this.Airport(town, AIAirport.AT_HELISTATION);
					if (pad != -1) break;
				}
			}
		}
		this.Check("three-airports", first != -1 && last != -1 && pad != -1);
		local rig = -1;
		if (AIRCRAFT_OILRIG) {
			foreach (industry, unused in AIIndustryList()) {
				if (AIIndustry.HasHeliport(industry)) { rig = AIIndustry.GetHeliportLocation(industry); break; }
			}
			if (rig == -1) foreach (type, unused in AIIndustryTypeList()) {
				if (!AIIndustryType.HasHeliport(type) || !AIIndustryType.CanBuildIndustry(type)) continue;
				for (local y = 16; y < AIMap.GetMapSizeY() - 16 && rig == -1; y++) for (local x = 16; x < AIMap.GetMapSizeX() - 16; x++) {
					local tile = AIMap.GetTileIndex(x, y);
					if (!AITile.IsWaterTile(tile)) continue;
					local possible;
					{ local test = AITestMode(); possible = AIIndustryType.BuildIndustry(type, tile); }
					if (!possible) continue;
					this.Check("oilrig-build", AIIndustryType.BuildIndustry(type, tile));
					/* Its neutral station appears when the industry finishes construction. */
					for (local tick = 0; tick < 1500 && rig == -1; tick++) {
						foreach (industry, unused in AIIndustryList()) {
							if (AIIndustry.HasHeliport(industry)) { rig = AIIndustry.GetHeliportLocation(industry); break; }
						}
						if (rig == -1) this.Sleep(1);
					}
					this.Check("oilrig-complete", rig != -1);
					break;
				}
				if (rig != -1) break;
			}
			this.Check("oilrig", rig != -1);
			print("AIRCRAFT-OILRIG " + rig);
		}
		local engines = AIEngineList(AIVehicle.VT_AIR);
		local heads = [];
		foreach (type in [AIAirport.PT_SMALL_PLANE, AIAirport.PT_HELICOPTER]) {
			local engine = -1;
			foreach (id, unused in engines) {
				if (AIEngine.IsBuildable(id) && AIEngine.GetCargoType(id) == 0 && AIEngine.GetPlaneType(id) == type) { engine = id; break; }
			}
			this.Check("engine-" + type, engine != -1);
			for (local number = 0; number < AIRCRAFT_COUNT; number++) {
				local vehicle = AIVehicle.BuildVehicle(AIAirport.GetHangarOfAirport(first), engine);
				this.Check("vehicle-" + type, AIVehicle.IsValidVehicle(vehicle));
				this.Check("order-a", AIOrder.AppendOrder(vehicle, first, AIOrder.OF_NONE));
				this.Check("order-b", AIOrder.AppendOrder(vehicle, type == AIAirport.PT_HELICOPTER ? (AIRCRAFT_OILRIG ? rig : pad) : last, AIOrder.OF_NONE));
				this.Check("start", AIVehicle.StartStopVehicle(vehicle));
				heads.append(vehicle);
			}
		}
		print("AIRCRAFT-CONTROL-END " + first + " " + last + " " + pad);
		while (true) this.Sleep(1000);
	}
}
