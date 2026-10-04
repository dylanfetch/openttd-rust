require("parameters.nut");
class MigrationDisasters extends AIController {
	function Start() {
		print("DISASTER-OBSERVER " + DISASTER_SETUP);
		if (DISASTER_SETUP) {
			AIController.SetCommandDelay(1);
			AIRail.SetCurrentRailType(0);
			for (local tile = 10006; tile <= 10007; tile++) {
				print("DISASTER-SETUP clear " + tile + "=" + AITile.DemolishTile(tile));
				print("DISASTER-SETUP rail " + tile + "=" + AIRail.BuildRailTrack(tile, AIRail.RAILTRACK_NE_SW));
			}
			print("DISASTER-SETUP start=" + AIVehicle.StartStopVehicle(17));
			for (local i = 0; i < 2000 && AIVehicle.GetLocation(17) == 10008; i++) this.Sleep(1);
			print("DISASTER-SETUP location=" + AIVehicle.GetLocation(17));
			print("DISASTER-SETUP stop=" + AIVehicle.StartStopVehicle(17));
			print("DISASTER-SETUP-END");
		}
		if (DISASTER_ACTION == "industry") {
			print("DISASTER-ACTION industry=" + AITile.DemolishTile(DISASTER_TARGET));
		} else if (DISASTER_ACTION == "road-sale") {
			print("DISASTER-ACTION road-sale=" + AIVehicle.SellVehicle(DISASTER_TARGET));
		} else if (DISASTER_ACTION == "large-airport") {
			AIController.SetCommandDelay(1);
			print("DISASTER-ACTION sell-plane=" + AIVehicle.SellVehicle(14));
			print("DISASTER-ACTION remove-airport=" + AIAirport.RemoveAirport(32116));
			print("DISASTER-ACTION large-airport=" + AIAirport.BuildAirport(32116, AIAirport.AT_LARGE, AIStation.STATION_NEW));
		}
		while (true) {
			while (AIEventController.IsEventWaiting()) {
				local event = AIEventController.GetNextEvent();
				if (event.GetEventType() == AIEvent.ET_DISASTER_ZEPPELINER_CRASHED) {
					print("DISASTER-EVENT zeppelin-crashed " + AIEventDisasterZeppelinerCrashed.Convert(event).GetStationID());
				} else if (event.GetEventType() == AIEvent.ET_DISASTER_ZEPPELINER_CLEARED) {
					print("DISASTER-EVENT zeppelin-cleared " + AIEventDisasterZeppelinerCleared.Convert(event).GetStationID());
				} else if (event.GetEventType() == AIEvent.ET_VEHICLE_CRASHED) {
					local crash = AIEventVehicleCrashed.Convert(event);
					print("DISASTER-EVENT vehicle-crashed " + crash.GetVehicleID() + " " + crash.GetCrashReason() + " " + crash.GetCrashSite() + " " + crash.GetVictims());
				} else if (event.GetEventType() == AIEvent.ET_INDUSTRY_CLOSE) {
					print("DISASTER-EVENT industry-close " + AIEventIndustryClose.Convert(event).GetIndustryID());
				}
			}
			this.Sleep(1);
		}
	}
}
