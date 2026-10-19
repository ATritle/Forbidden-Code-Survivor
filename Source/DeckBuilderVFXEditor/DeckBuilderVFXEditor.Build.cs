using UnrealBuildTool;
using System.IO;
public class DeckBuilderVFXEditor : ModuleRules
{
    public DeckBuilderVFXEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory,"../DeckBuilder")));
        PrivateDependencyModuleNames.AddRange(new[] {"Core", "CoreUObject", "Engine", "UnrealEd", "NavigationSystem", "Niagara", "NiagaraCore", "NiagaraEditor", "AssetRegistry", "DeckBuilder", "AnimGraph", "AnimGraphRuntime", "BlueprintGraph", "KismetCompiler", "AssetTools"});
    }
}
