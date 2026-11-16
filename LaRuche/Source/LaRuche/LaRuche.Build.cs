using UnrealBuildTool;

public class LaRuche : ModuleRules
{
	public LaRuche(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "AIModule", "NetCore", "Sockets", "ApplicationCore"
		});
	}
}
