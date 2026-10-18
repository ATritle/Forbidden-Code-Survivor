using UnrealBuildTool;
public class DeckBuilderTarget : TargetRules
{
    public DeckBuilderTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("DeckBuilder");
    }
}
