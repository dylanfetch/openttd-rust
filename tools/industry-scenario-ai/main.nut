require("parameters.nut");
require("water-check.nut");
class MigrationIndustries extends AIController {
	function Start();
}
function MigrationIndustries::Start()
{
	AIController.SetCommandDelay(1);
	if (INDUSTRY_MODE != "lumber") AICompany.SetLoanAmount(AICompany.GetMaxLoanAmount());
	if (INDUSTRY_MODE == "lumber") {
		local found = false;
		foreach (industry, value in AIIndustryList()) {
			if (AIIndustry.GetIndustryType(industry) == 25) found = true;
		}
		if (!found) {
			AICompany.SetLoanAmount(AICompany.GetMaxLoanAmount());
			for (local tile = 0; tile < AIMap.GetMapSize(); tile++) {
				if (AITile.GetTerrainType(tile) != AITile.TERRAIN_RAINFOREST) continue;
				if (AIIndustryType.BuildIndustry(25, tile)) {
					print("INDUSTRY-LUMBER built " + tile);
					found = true;
					break;
				}
			}
		}
		print("INDUSTRY-LUMBER ready " + found);
	} else if (INDUSTRY_MODE == "water-check") {
		IndustryWaterCheck();
	} else if (INDUSTRY_MODE == "fund") {
		local rejected;
		{ local test = AITestMode(); rejected = !AIIndustryType.BuildIndustry(0, 0); }
		local built = false;
		for (local tile = 1; tile < AIMap.GetMapSize(); tile++) {
			local possible;
			{ local test = AITestMode(); possible = AIIndustryType.BuildIndustry(0, tile); }
			if (!possible) continue;
			built = AIIndustryType.BuildIndustry(0, tile);
			if (built) break;
		}
		print("INDUSTRY-COMMAND fund ready " + (rejected && built));
	} else if (INDUSTRY_MODE == "prospect") {
		local test_ok;
		{ local test = AITestMode(); test_ok = AIIndustryType.ProspectIndustry(0); }
		local built = false;
		for (local attempt = 0; attempt < 4; attempt++) {
			local before = AIIndustryList().Count();
			AIIndustryType.ProspectIndustry(0);
			if (AIIndustryList().Count() > before) { built = true; break; }
		}
		print("INDUSTRY-COMMAND prospect ready " + (test_ok && built));
	} else if (INDUSTRY_MODE == "clear") {
		local type = AIIndustry.GetIndustryType(AIIndustry.GetIndustryID(INDUSTRY_TARGET));
		local test_ok;
		{ local test = AITestMode(); test_ok = AITile.DemolishTile(INDUSTRY_TARGET); }
		local cleared = AITile.DemolishTile(INDUSTRY_TARGET);
		local built = cleared && AIIndustryType.BuildIndustry(type, INDUSTRY_TARGET);
		print("INDUSTRY-COMMAND clear ready " + (test_ok && cleared && built));
	} else if (INDUSTRY_MODE == "terraform") {
		local changed = false;
		for (local tile = 1; tile < AIMap.GetMapSize(); tile++) {
			if (!AIIndustry.IsValidIndustry(AIIndustry.GetIndustryID(tile))) continue;
			if (AITile.GetSlope(tile) != AITile.SLOPE_FLAT || AITile.GetMinHeight(tile) == 0) continue;
			local possible;
			{ local test = AITestMode(); possible = AITile.LowerTile(tile, AITile.SLOPE_N); }
			if (!possible) continue;
			changed = AITile.LowerTile(tile, AITile.SLOPE_N);
			if (changed) break;
		}
		print("INDUSTRY-COMMAND terraform ready " + changed);
	}
	while (true) this.Sleep(10000);
}
