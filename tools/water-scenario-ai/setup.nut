/* OpenTTD-Rust migration evidence, licensed under GPLv2.
 * Only installed over StationList in an isolated reference preparation runtime. */
require("parameters.nut");
class StationList extends AIController {
	function T(x, y) { return AIMap.GetTileIndex(x, y); }
	function Check(label, ok) {
		print("WATER-BUILD " + label + " " + ok);
		if (!ok) throw label + ": " + AIError.GetLastErrorString();
	}
	function Start() {
		AIController.SetCommandDelay(1);
		this.Check("loan", AICompany.SetLoanAmount(AICompany.GetMaxLoanAmount()));
		local depot, first, last, middle = -1;
		if (WATER_SETUP == "ferry") {
			first = 4037; last = 5163; depot = 1214; middle = 1400;
			this.Check("dock-a", AIMarine.BuildDock(first, AIStation.STATION_NEW));
			this.Check("dock-b", AIMarine.BuildDock(last, AIStation.STATION_NEW));
			this.Check("depot", AIMarine.BuildWaterDepot(depot, depot + 1));
			this.Check("buoy", AIMarine.BuildBuoy(middle));
		} else {
			for (local x = 170; x <= 178; x++) for (local y = 189; y <= 192; y++)
				this.Check("valley-" + x + "-" + y, AITile.LowerTile(this.T(x, y), AITile.SLOPE_N));
			for (local x = 153; x <= 160; x++) for (local y = 189; y <= 192; y++)
				this.Check("lock-slope-" + x + "-" + y, AITile.LowerTile(this.T(x, y), AITile.SLOPE_N));
			this.Check("aqueduct", AIBridge.BuildBridge(AIVehicle.VT_WATER, 0, this.T(169, 190), this.T(178, 190)));
			for (local x = 154; x <= 193; x++) {
				if ((x >= 169 && x <= 178) || x == 160) continue;
				this.Check("canal-" + x, AIMarine.BuildCanal(this.T(x, 190)));
			}
			this.Check("lock", AIMarine.BuildLock(this.T(160, 190)));
			this.Check("isolated-a", AIMarine.BuildCanal(this.T(164, 187)));
			this.Check("isolated-b", AIMarine.BuildCanal(this.T(167, 187)));
			depot = this.T(154, 190); first = this.T(158, 190); last = this.T(192, 190);
			this.Check("depot", AIMarine.BuildWaterDepot(depot, depot + 1));
			this.Check("buoy-a", AIMarine.BuildBuoy(first));
			this.Check("buoy-b", AIMarine.BuildBuoy(last));
		}
		local ship = AIVehicle.BuildVehicle(depot, 206);
		this.Check("ship", AIVehicle.IsValidVehicle(ship));
		this.Check("order-a", AIOrder.AppendOrder(ship, first, AIOrder.OF_NONE));
		if (middle != -1) this.Check("order-buoy", AIOrder.AppendOrder(ship, middle, AIOrder.OF_NONE));
		this.Check("order-b", AIOrder.AppendOrder(ship, last, AIOrder.OF_NONE));
		this.Check("start", AIVehicle.StartStopVehicle(ship));
		print("WATER-SETUP-END " + WATER_SETUP + " " + ship);
		while (true) this.Sleep(1000);
	}
}
