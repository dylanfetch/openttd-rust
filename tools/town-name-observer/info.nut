class MigrationTownNames extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "MigrationTownNames"; }
	function GetShortName() { return "MTNO"; }
	function GetDescription() { return "Read-only migration town-name observer."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-04"; }
	function CreateInstance() { return "MigrationTownNames"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(MigrationTownNames());
