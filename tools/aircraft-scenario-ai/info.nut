/* OpenTTD-Rust migration evidence, licensed under GPLv2. */
class MigrationAircraft extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "MigrationAircraft"; }
	function GetShortName() { return "AIRF"; }
	function GetDescription() { return "Build the supplemental aircraft fixture once."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-04"; }
	function CreateInstance() { return "MigrationAircraft"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(MigrationAircraft());
