require("parameters.nut");
class MigrationTrees extends AIController {
	count = 0;
	function Start();
	function Check(label, task);
}
function MigrationTrees::Check(label, task)
{
	local town = AITile.GetTownAuthority(TREE_ANCHOR);
	foreach (mode in ["test", "execute"]) {
		local costs = AIAccounting();
		local before = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
		local rating = AITown.GetRating(town, AICompany.COMPANY_SELF);
		local result;
		if (mode == "test") {
			local test = AITestMode();
			result = task();
		} else {
			result = task();
		}
		local after = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
		local new_rating = AITown.GetRating(town, AICompany.COMPANY_SELF);
		print("TREE-COMMAND " + label + " " + mode + " " + result + " " + costs.GetCosts() + " " + AIError.GetLastError() + " " + before + " " + after + " " + rating + " " + new_rating);
		if (mode == "test" && (before != after || rating != new_rating)) print("TREE-TEST-MUTATED");
		this.count++;
	}
}
function MigrationTrees::Start()
{
	AIController.SetCommandDelay(1);
	local a = TREE_ANCHOR;
	this.Check("rectangle", function() : (a) { return AITile.PlantTreeRectangle(a, 3, 3); });
	for (local i = 0; i < 4; i++) this.Check("add" + i, function() : (a) { return AITile.PlantTree(a + 1); });
	this.Check("full", function() : (a) { return AITile.PlantTree(a); });
	this.Check("water", function() { return AITile.PlantTree(TREE_WATER); });
	this.Check("invalid", function() { return AITile.PlantTree(AIMap.GetMapSize()); });
	this.Check("clear-full", function() : (a) { return AITile.DemolishTile(a); });
	this.Check("clear-added", function() : (a) { return AITile.DemolishTile(a + 1); });
	this.Check("clear-field", function() : (a) { return AITile.DemolishTile(a + 2); });
	this.Check("replant", function() : (a) { return AITile.PlantTree(a); });
	print("TREE-COMMAND-END " + this.count);
	while (true) this.Sleep(1000);
}
