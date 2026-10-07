/* OpenTTD-Rust migration evidence, licensed under GPLv2.
 * Installed only in an isolated pinned-reference preparation runtime. */
class RoadCrossing extends AIController {
	function Start() {
		AIController.SetCommandDelay(1);
		AIRoad.SetCurrentRoadType(AIRoad.ROADTYPE_ROAD);
		for (local tile = 1; tile < AIMap.GetMapSize(); tile++) {
			if (!AIRail.IsRailTile(tile) || AITile.GetOwner(tile) != AICompany.ResolveCompanyID(AICompany.COMPANY_SELF)) continue;
			local tracks = AIRail.GetRailTracks(tile);
			if (tracks != AIRail.RAILTRACK_NE_SW && tracks != AIRail.RAILTRACK_NW_SE) continue;
			if (AITile.GetSlope(tile) != AITile.SLOPE_FLAT) continue;
			local offset = tracks == AIRail.RAILTRACK_NE_SW ? AIMap.GetMapSizeX() : 1;
			local possible;
			{ local test = AITestMode(); possible = AIRoad.BuildRoad(tile - offset, tile + offset); }
			if (!possible) continue;
			if (!AIRoad.BuildRoad(tile - offset, tile + offset)) throw AIError.GetLastErrorString();
			print("ROAD-CROSSING-BUILT " + tile + " " + tracks);
			while (true) this.Sleep(1000);
		}
		throw "no legal crossing on owned straight rail";
	}
}
