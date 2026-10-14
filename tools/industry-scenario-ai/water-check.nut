/* Builtin oil-rig layout: the outer x=-4 ring consists of 0xFF checks. */
function IndustryWaterCheck()
{
	local anchor = -1;
	local width = AIMap.GetMapSizeX();
	for (local y = 6; y < AIMap.GetMapSizeY() - 8 && anchor < 0; y++) {
		for (local x = 8; x < width - 8; x++) {
			local tile = AIMap.GetTileIndex(x, y);
			local clear = true;
			for (local dy = -4; dy <= 6 && clear; dy++) {
				for (local dx = -7; dx <= 5; dx++) {
					local part = tile + dx + dy * width;
					if (!AITile.IsWaterTile(part) || AITile.GetSlope(part) != AITile.SLOPE_FLAT) { clear = false; break; }
				}
			}
			if (!clear) continue;
			local possible;
			{ local test = AITestMode(); possible = AIIndustryType.BuildIndustry(5, tile); }
			if (possible) { anchor = tile; break; }
		}
	}
	if (anchor < 0) throw "No plain-water oil-rig site";
	print("INDUSTRY-WATER plain ready true");
	if (INDUSTRY_WATER == "depot") {
		local depot = anchor - 4 - 4 * width;
		if (!AIMarine.BuildWaterDepot(depot, depot + 1)) throw AIError.GetLastErrorString();
		IndustryWaterRefused(anchor, "depot");
		if (!AIMarine.RemoveWaterDepot(depot)) throw AIError.GetLastErrorString();
	} else {
		/* Raise the upper tile outside the ring; only the flat lower lock part
		 * intersects it. All other 0xFF tiles remain plain flat water. */
		local upper = anchor - 6;
		local middle = anchor - 5;
		if (!AITile.RaiseTile(upper, AITile.SLOPE_ELEVATED)) throw AIError.GetLastErrorString();
		if (!AIMarine.BuildLock(middle)) throw AIError.GetLastErrorString();
		IndustryWaterRefused(anchor, "lock");
		if (!AIMarine.RemoveLock(middle)) throw AIError.GetLastErrorString();
	}
	local built = AIIndustryType.BuildIndustry(5, anchor);
	if (!built) throw "Oil-rig funding: " + AIError.GetLastErrorString();
	print("INDUSTRY-WATER fund ready true");
}
function IndustryWaterRefused(anchor, label)
{
	local before = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
	local count = AIIndustryList().Count();
	local test_refused;
	{ local test = AITestMode(); test_refused = !AIIndustryType.BuildIndustry(5, anchor) && AIError.GetLastError() == AIError.ERR_SITE_UNSUITABLE; }
	local refused = !AIIndustryType.BuildIndustry(5, anchor) && AIError.GetLastError() == AIError.ERR_SITE_UNSUITABLE;
	print("INDUSTRY-WATER " + label + " ready " + (test_refused && refused && count == AIIndustryList().Count() && before == AICompany.GetBankBalance(AICompany.COMPANY_SELF)));
}
