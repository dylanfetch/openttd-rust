/* OpenTTD-Rust migration evidence, licensed under GPLv2. */
class WaterScenes extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "WaterScenes"; }
	function GetShortName() { return "WREG"; }
	function GetDescription() { return "Deterministic water-region command witnesses."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-04"; }
	function CreateInstance() { return "WaterScenes"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(WaterScenes());
