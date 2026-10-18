using UnrealBuildTool;
public class DeckBuilder : ModuleRules
{
    public DeckBuilder(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "GameplayTags", "CodexRuntime", "Niagara", "NavigationSystem", "UMG", "Slate", "SlateCore" });
        PrivateDependencyModuleNames.Add("AudioMixer");
    }
}
