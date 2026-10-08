/* OpenTTD-Rust migration evidence, licensed under GPLv2. */
require("parameters.nut");
class MigrationCompanies extends AIController {
	function Check(label, ok) {
		print("COMPANY-CHECK " + label + " " + ok);
		if (!ok) throw label + ": " + AIError.GetLastErrorString();
	}
	function Start() {
		AIController.SetCommandDelay(1);
		AIRoad.SetCurrentRoadType(AIRoad.ROADTYPE_ROAD);
		local self = AICompany.ResolveCompanyID(AICompany.COMPANY_SELF);
		print("COMPANY-START " + self);
		if (COMPANY_SETUP && self == 1) {
			local depot = -1, station = -1;
			for (local tile = 130; tile < AIMap.GetMapSize() - 130; tile++) {
				if (AITile.GetOwner(tile) != AICompany.COMPANY_INVALID || AITile.GetOwner(tile + 1) != AICompany.COMPANY_INVALID) continue;
				local possible;
				{ local test = AITestMode(); possible = AIRoad.BuildRoadDepot(tile, tile + 1); }
				if (!possible) continue;
				this.Check("depot", AIRoad.BuildRoadDepot(tile, tile + 1));
				depot = tile;
				break;
			}
			this.Check("depot-found", depot != -1);
			for (local tile = 130; tile < AIMap.GetMapSize() - 130; tile++) {
				if (AITile.GetOwner(tile) != AICompany.COMPANY_INVALID || AITile.GetOwner(tile + 1) != AICompany.COMPANY_INVALID) continue;
				local possible;
				{ local test = AITestMode(); possible = AIRoad.BuildRoadStation(tile, tile + 1, AIRoad.ROADVEHTYPE_BUS, AIStation.STATION_NEW); }
				if (!possible) continue;
				this.Check("station", AIRoad.BuildRoadStation(tile, tile + 1, AIRoad.ROADVEHTYPE_BUS, AIStation.STATION_NEW));
				station = AIStation.GetStationID(tile);
				break;
			}
			this.Check("station-found", station != -1);
			local engines = AIEngineList(AIVehicle.VT_ROAD), engine = -1;
			foreach (id, unused in engines) if (AIEngine.IsBuildable(id) && AIEngine.GetCargoType(id) == 0) { engine = id; break; }
			this.Check("engine", engine != -1);
			local vehicle = AIVehicle.BuildVehicle(depot, engine);
			this.Check("vehicle", AIVehicle.IsValidVehicle(vehicle));
			local group = AIGroup.CreateGroup(AIVehicle.VT_ROAD, AIGroup.GROUP_INVALID);
			this.Check("group", AIGroup.IsValidGroup(group));
			this.Check("group-vehicle", AIGroup.MoveVehicle(group, vehicle));
			print("COMPANY-ASSETS " + depot + " " + station + " " + vehicle + " " + group);
		}
		if (!COMPANY_SETUP && COMPANY_ACTION == "finance" && self == 2) {
			local before = AICompany.GetLoanAmount(), amount = before + AICompany.GetLoanInterval();
			{ local test = AITestMode(); this.Check("loan-test", AICompany.SetLoanAmount(amount)); }
			this.Check("loan-test-unchanged", AICompany.GetLoanAmount() == before);
			this.Check("loan-execute", AICompany.SetLoanAmount(amount));
			this.Check("loan-changed", AICompany.GetLoanAmount() == amount);
			this.Check("loan-repay", AICompany.SetLoanAmount(before));
			print("COMPANY-FINANCE-END");
		}
		while (true) {
			while (AIEventController.IsEventWaiting()) {
				local event = AIEventController.GetNextEvent(), type = event.GetEventType();
				if (type == AIEvent.ET_COMPANY_IN_TROUBLE) print("COMPANY-TROUBLE " + self + " " + AIEventCompanyInTrouble.Convert(event).GetCompanyID());
				if (type == AIEvent.ET_COMPANY_ASK_MERGER) {
					local offer = AIEventCompanyAskMerger.Convert(event);
					print("COMPANY-OFFER " + self + " " + offer.GetCompanyID() + " " + offer.GetValue());
					if (COMPANY_ACTION == "acquire") {
						{ local test = AITestMode(); this.Check("merger-test", offer.AcceptMerger()); }
						this.Check("merger-test-unchanged", AICompany.ResolveCompanyID(offer.GetCompanyID()) != AICompany.COMPANY_INVALID);
						this.Check("merger-execute", offer.AcceptMerger());
						print("COMPANY-MERGER-END");
					}
				}
			}
			this.Sleep(1);
		}
	}
}
