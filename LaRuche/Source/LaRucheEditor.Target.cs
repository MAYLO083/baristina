using UnrealBuildTool;

public class LaRucheEditorTarget : TargetRules
{
	public LaRucheEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LaRuche");
	}
}
