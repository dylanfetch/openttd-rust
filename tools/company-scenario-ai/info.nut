/* OpenTTD-Rust migration evidence, licensed under GPLv2. */
class MigrationCompanies extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "MigrationCompanies"; }
	function GetShortName() { return "CMPF"; }
	function GetDescription() { return "Company lifecycle simulation witnesses."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-04"; }
	function CreateInstance() { return "MigrationCompanies"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(MigrationCompanies());
