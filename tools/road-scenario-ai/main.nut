/* OpenTTD-Rust migration evidence, licensed under GPLv2.
 * Observe crash events; flooding removes one real canal before observation. */
require("parameters.nut");
class RoadCrossing extends AIController {
	function Start() {
		if (ROAD_FLOOD) {
			AIController.SetCommandDelay(1);
			local ok = AIMarine.RemoveCanal(945);
			print("ROAD-FLOOD-BREACH " + ok);
			if (!ok) throw AIError.GetLastErrorString();
		}
		while (true) {
			while (AIEventController.IsEventWaiting()) {
				local event = AIEventController.GetNextEvent();
				if (event.GetEventType() != AIEvent.ET_VEHICLE_CRASHED) continue;
				local crash = AIEventVehicleCrashed.Convert(event);
				local prefix = ROAD_FLOOD ? "ROAD-FLOOD-EVENT " : "ROAD-CROSSING-EVENT ";
				print(prefix + crash.GetVehicleID() + " " + crash.GetCrashSite() + " " + crash.GetCrashReason() + " " + crash.GetVictims());
			}
			this.Sleep(1);
		}
	}
}
