using UnrealBuildTool;

public class EmberveilTarget : TargetRules
{
	public EmberveilTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Emberveil");
	}
}
