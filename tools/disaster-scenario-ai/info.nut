class MigrationDisasters extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "MigrationDisasters"; }
	function GetShortName() { return "MDSR"; }
	function GetDescription() { return "Migration disaster targets and event observations."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-04"; }
	function CreateInstance() { return "MigrationDisasters"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(MigrationDisasters());
