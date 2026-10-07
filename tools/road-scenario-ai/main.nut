/* OpenTTD-Rust migration evidence, licensed under GPLv2.
 * Read-only observer; never constructs or changes simulation state. */
class RoadCrossing extends AIController {
	function Start() {
		while (true) {
			while (AIEventController.IsEventWaiting()) {
				local event = AIEventController.GetNextEvent();
				if (event.GetEventType() != AIEvent.ET_VEHICLE_CRASHED) continue;
				local crash = AIEventVehicleCrashed.Convert(event);
				print("ROAD-CROSSING-EVENT " + crash.GetVehicleID() + " " + crash.GetCrashSite() + " " + crash.GetCrashReason() + " " + crash.GetVictims());
			}
			this.Sleep(1);
		}
	}
}
