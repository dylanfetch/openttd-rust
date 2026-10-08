/* OpenTTD-Rust migration evidence, licensed under GPLv2. */
require("parameters.nut");
class WaterScenes extends AIController {
	function X() { return AIMap.GetTileX(AIVehicle.GetLocation(WATER_SHIP)); }
	function Mark(label) { print("WATER " + label + " " + this.GetTick() + " " + AIVehicle.GetLocation(WATER_SHIP)); }
	function Check(label, ok) {
		if (!ok) throw label + ": " + AIError.GetLastErrorString();
		this.Mark(label);
	}
	function WaitLeft() { while (this.X() >= 159) this.Sleep(1); }
	function WaitRight() { while (this.X() < 187) this.Sleep(1); }
	function Closed(label) {
		local lost = false;
		for (local i = 0; i < 30; i++) {
			this.Sleep(100);
			while (AIEventController.IsEventWaiting()) {
				local e = AIEventController.GetNextEvent();
				if (e.GetEventType() == AIEvent.ET_VEHICLE_LOST && AIEventVehicleLost.Convert(e).GetVehicleID() == WATER_SHIP) lost = true;
			}
		}
		this.Check(label + "-lost", lost);
	}
	function Start() {
		AIController.SetCommandDelay(1);
		this.Check("ship", AIVehicle.IsValidVehicle(WATER_SHIP));
		this.WaitRight(); this.Mark("warm-right");
		this.WaitLeft(); this.Mark("warm-left");
		this.WaitRight();
		if (WATER_MODE == "lifecycle") {
			local depot = AIMap.GetTileIndex(154, 190);
			local built = AIVehicle.BuildVehicle(depot, 206);
			this.Check("build " + built, AIVehicle.IsValidVehicle(built));
			this.Check("sell " + built, AIVehicle.SellVehicle(built));
			local reused = AIVehicle.BuildVehicle(depot, 206);
			this.Check("reuse " + reused, AIVehicle.IsValidVehicle(reused) && reused == built);
			this.Check("reuse-order-a", AIOrder.AppendOrder(reused, AIMap.GetTileIndex(158, 190), AIOrder.OF_NONE));
			this.Check("reuse-order-b", AIOrder.AppendOrder(reused, AIMap.GetTileIndex(192, 190), AIOrder.OF_NONE));
			this.Check("reuse-start", AIVehicle.StartStopVehicle(reused));
			while (AIVehicle.GetLocation(reused) == depot || AIVehicle.GetLocation(reused) == depot + 1) this.Sleep(1);
			this.Mark("reuse-moved " + reused);
		} else if (WATER_MODE == "depot") {
			this.Check("send-depot", AIVehicle.SendVehicleToDepot(WATER_SHIP));
			while (!AIVehicle.IsStoppedInDepot(WATER_SHIP)) this.Sleep(1);
			this.Mark("depot-arrival");
			this.Check("restart", AIVehicle.StartStopVehicle(WATER_SHIP));
			this.WaitRight(); this.Mark("depot-recovered");
		} else if (WATER_MODE == "mutate") {
			local lock = AIMap.GetTileIndex(160, 190), canal = AIMap.GetTileIndex(165, 190);
			while (AIEventController.IsEventWaiting()) AIEventController.GetNextEvent();
			this.Check("close-lock", AIMarine.RemoveLock(lock));
			this.Closed("lock");
			this.Check("open-lock", AIMarine.BuildLock(lock));
			this.WaitLeft(); this.Mark("lock-recovered");
			this.WaitRight();
			while (AIEventController.IsEventWaiting()) AIEventController.GetNextEvent();
			this.Check("close-canal", AIMarine.RemoveCanal(canal));
			this.Closed("canal");
			this.Check("open-canal", AIMarine.BuildCanal(canal));
			this.WaitLeft(); this.Mark("canal-recovered");
		}
		this.Mark("complete");
		while (true) this.Sleep(1000);
	}
}
