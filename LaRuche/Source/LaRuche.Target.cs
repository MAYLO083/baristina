using UnrealBuildTool;

public class LaRucheTarget : TargetRules
{
	public LaRucheTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LaRuche");
	}
}
