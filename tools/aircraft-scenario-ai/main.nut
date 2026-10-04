/* OpenTTD-Rust migration evidence, licensed under GPLv2.
 * Installed only in an isolated, unchanged pinned-reference runtime. */
class MigrationAircraft extends AIController {
	function Check(label, ok) {
		print("AIRCRAFT-BUILD " + label + " " + ok);
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
		this.Check("loan", AICompany.SetLoanAmount(AICompany.GetMaxLoanAmount()));
		local towns = AITownList();
		towns.Valuate(AITown.GetPopulation);
		towns.Sort(AIList.SORT_BY_VALUE, false);
		local first = -1, last = -1;
		foreach (town, population in towns) {
			if (first == -1) first = this.Airport(town, AIAirport.AT_LARGE);
			else if (AIMap.DistanceManhattan(AITown.GetLocation(town), first) >= 35) {
				last = this.Airport(town, AIAirport.AT_METROPOLITAN);
				if (last != -1) break;
			}
		}
		this.Check("two-airports", first != -1 && last != -1);
		local engines = AIEngineList(AIVehicle.VT_AIR);
		local heads = [];
		foreach (type in [AIAirport.PT_SMALL_PLANE, AIAirport.PT_HELICOPTER]) {
			local engine = -1;
			foreach (id, unused in engines) {
				if (AIEngine.IsBuildable(id) && AIEngine.GetCargoType(id) == 0 && AIEngine.GetPlaneType(id) == type) { engine = id; break; }
			}
			this.Check("engine-" + type, engine != -1);
			local vehicle = AIVehicle.BuildVehicle(AIAirport.GetHangarOfAirport(first), engine);
			this.Check("vehicle-" + type, AIVehicle.IsValidVehicle(vehicle));
			this.Check("order-a", AIOrder.AppendOrder(vehicle, first, AIOrder.OF_NONE));
			this.Check("order-b", AIOrder.AppendOrder(vehicle, last, AIOrder.OF_NONE));
			this.Check("start", AIVehicle.StartStopVehicle(vehicle));
			heads.append(vehicle);
		}
		print("AIRCRAFT-SETUP-END " + first + " " + last + " " + heads[0] + " " + heads[1]);
		while (true) this.Sleep(1000);
	}
}
