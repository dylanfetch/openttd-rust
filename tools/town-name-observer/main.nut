class MigrationTownNames extends AIController {
	function Start();
}
function MigrationTownNames::Start()
{
	local towns = AITownList();
	towns.Sort(AIList.SORT_BY_ITEM, AIList.SORT_ASCENDING);
	local count = 0;
	foreach (id, _ in towns) {
		print("TOWN-NAME " + id + " " + AITown.GetName(id));
		count++;
	}
	print("TOWN-NAME-END " + count);
	while (true) this.Sleep(1000);
}
