class MigrationRails extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "MigrationRails"; }
	function GetShortName() { return "MRAL"; }
	function GetDescription() { return "Rail controller commands and crash observations."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-04"; }
	function CreateInstance() { return "MigrationRails"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(MigrationRails());
