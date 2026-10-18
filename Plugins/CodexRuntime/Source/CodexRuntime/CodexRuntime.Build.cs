using UnrealBuildTool;

public class CodexRuntime : ModuleRules
{
    public CodexRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GameplayTags", "NavigationSystem" });
    }
}
