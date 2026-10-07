class MigrationIndustries extends AIController {
	function Start();
}
function MigrationIndustries::Start()
{
	AIController.SetCommandDelay(1);
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
	while (true) this.Sleep(10000);
}
