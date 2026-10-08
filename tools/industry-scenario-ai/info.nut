class MigrationIndustries extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "MigrationIndustries"; }
	function GetShortName() { return "MIND"; }
	function GetDescription() { return "Build the absent tropical lumber mill for industry evidence."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-04"; }
	function CreateInstance() { return "MigrationIndustries"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(MigrationIndustries());
