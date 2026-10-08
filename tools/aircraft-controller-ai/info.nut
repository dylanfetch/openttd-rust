/* OpenTTD-Rust migration evidence, licensed under GPLv2. */
class MigrationAircraftController extends AIInfo {
	function GetAuthor() { return "OpenTTD-Rust"; }
	function GetName() { return "MigrationAircraftController"; }
	function GetShortName() { return "AIRF"; }
	function GetDescription() { return "Build grouped terminals, dedicated pads and exercise airport commands."; }
	function GetVersion() { return 1; }
	function GetAPIVersion() { return "15"; }
	function GetDate() { return "2026-10-04"; }
	function CreateInstance() { return "MigrationAircraftController"; }
	function UseAsRandomAI() { return false; }
}
RegisterAI(MigrationAircraftController());
