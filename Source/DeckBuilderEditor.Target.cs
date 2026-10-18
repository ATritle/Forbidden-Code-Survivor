using UnrealBuildTool;
public class DeckBuilderEditorTarget : TargetRules
{
    public DeckBuilderEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("DeckBuilder");
    }
}
