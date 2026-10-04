class MigrationTrees extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "MigrationTrees"; }
	function GetShortName() { return "MTRS"; }
	function GetDescription() { return "Migration tree command scenarios."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-04"; }
	function CreateInstance() { return "MigrationTrees"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(MigrationTrees());
