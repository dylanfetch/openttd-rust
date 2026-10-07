/* OpenTTD-Rust migration evidence, licensed under GPLv2. */
class RoadCrossing extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "WaterScenes"; }
	function GetShortName() { return "RCRS"; }
	function GetDescription() { return "Build one crossing on the existing multimodal map."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-07"; }
	function CreateInstance() { return "RoadCrossing"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(RoadCrossing());
