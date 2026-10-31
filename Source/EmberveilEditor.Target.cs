using UnrealBuildTool;

public class EmberveilEditorTarget : TargetRules
{
	public EmberveilEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Emberveil");
	}
}
