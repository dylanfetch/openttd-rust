require("parameters.nut");
class MigrationRails extends AIController {
	function Start() {
		AIController.SetCommandDelay(1);
		if (RAIL_ACTION == "reverse") {
			print("RAIL-COMMAND reverse=" + AIVehicle.ReverseVehicle(8));
		} else if (RAIL_ACTION == "service") {
			print("RAIL-COMMAND service=" + AIVehicle.SendVehicleToDepotForServicing(8));
		} else if (RAIL_ACTION == "crossing") {
			AIRoad.SetCurrentRoadType(AIRoad.ROADTYPE_ROAD);
			print("RAIL-COMMAND crossing=" + AIRoad.BuildRoad(4182, 4184));
		} else if (RAIL_ACTION == "reservation") {
			print("RAIL-COMMAND skip=" + AIOrder.SkipToOrder(8, 0));
			print("RAIL-COMMAND remove-signal=" + AIRail.RemoveSignal(3287, 3415));
			print("RAIL-COMMAND pbs=" + AIRail.BuildSignal(3287, 3415, AIRail.SIGNALTYPE_PBS));
			print("RAIL-COMMAND opposing-pbs=" + AIRail.BuildSignal(4183, 4311, AIRail.SIGNALTYPE_PBS));
		}
		local on_crossing = false;
		local in_depot = false;
		while (true) {
			if (RAIL_ACTION == "crossing") {
				local occupied = AIVehicle.GetLocation(14) == 4183;
				if (occupied != on_crossing) print("RAIL-CROSSING occupied=" + occupied);
				on_crossing = occupied;
			} else if (RAIL_ACTION == "service") {
				local occupied = AIVehicle.IsInDepot(8);
				if (occupied != in_depot) print("RAIL-DEPOT occupied=" + occupied);
				in_depot = occupied;
			}
			while (AIEventController.IsEventWaiting()) {
				local event = AIEventController.GetNextEvent();
				if (event.GetEventType() == AIEvent.ET_VEHICLE_CRASHED) {
					local crash = AIEventVehicleCrashed.Convert(event);
					print("RAIL-CRASH " + crash.GetVehicleID() + " " + crash.GetCrashReason() + " " + crash.GetCrashSite());
				}
			}
			this.Sleep(1);
		}
	}
}
