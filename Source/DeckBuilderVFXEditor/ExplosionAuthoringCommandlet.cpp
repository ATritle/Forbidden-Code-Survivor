#include "ExplosionAuthoringCommandlet.h"
#include "Modules/ModuleManager.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraSpriteRendererProperties.h"
#include "Materials/MaterialInterface.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "UObject/UObjectHash.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, DeckBuilderVFXEditor)
UExplosionAuthoringCommandlet::UExplosionAuthoringCommandlet() { IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }
int32 UExplosionAuthoringCommandlet::Main(const FString& Params)
{
    auto* System=LoadObject<UNiagaraSystem>(nullptr,TEXT("/NiagaraFluids/Templates/Gas/3D/Systems/Grid3D_Gas_Explosion.Grid3D_Gas_Explosion"));
    auto* Spark=LoadObject<UNiagaraEmitter>(nullptr,TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst.OmnidirectionalBurst"));
    if (!System || !Spark) return 1;
    FString Report;
    for(const TCHAR* EnumPath : {TEXT("/Niagara/Enums/ENiagara_GPUCollisionType.ENiagara_GPUCollisionType")})
        if(auto* Enum=LoadObject<UEnum>(nullptr,EnumPath))
            for(int32 I=0;I<Enum->NumEnums();++I) Report+=FString::Printf(TEXT("ENUM %s %s %s\n"),EnumPath,*Enum->GetNameStringByIndex(I),*Enum->GetDisplayNameTextByIndex(I).ToString());
    auto Dump=[&Report](FVersionedNiagaraEmitterData* Data)
    {
        auto* Source=Cast<UNiagaraScriptSource>(Data->GraphSource);
        for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
        {
            if (auto* Fn=Cast<UNiagaraNodeFunctionCall>(Node))
            {
                Report+=FString::Printf(TEXT("FUNCTION %s %s\n"),*Fn->GetFunctionName(),*GetPathNameSafe(Fn->FunctionScript));
                for (UEdGraphPin* Pin: Fn->Pins)
                {
                    Report+=FString::Printf(TEXT("  %s = %s\n"),*Pin->PinName.ToString(),*Pin->DefaultValue);
                    if(Pin->PinName==TEXT("Loop Behavior") || Pin->PinName==TEXT("Color Mode"))
                        if(auto* Enum=Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get()))
                            for(int32 I=0;I<Enum->NumEnums();++I) Report+=TEXT("  ENUM ")+Enum->GetNameStringByIndex(I)+TEXT(" ")+Enum->GetDisplayNameTextByIndex(I).ToString()+TEXT("\n");
                }
                if (Fn->FunctionScript && Fn->FunctionScript->GetPathName().StartsWith(TEXT("/Niagara/Modules")))
                    for (UEdGraphNode* Inner : Fn->GetFunctionScriptSource()->NodeGraph->Nodes)
                        for (UEdGraphPin* Pin:Inner->Pins)
                            if (Pin->PinName.ToString().StartsWith(TEXT("Module."))) Report+=TEXT(" INPUT ")+Pin->PinName.ToString()+TEXT(" = ")+Pin->DefaultValue+TEXT("\n");
            }
            if (Node->GetClass()->GetName().Contains(TEXT("ParameterMapSet")))
                for (UEdGraphPin* Pin: Node->Pins) Report+=FString::Printf(TEXT("OVERRIDE %s = %s [links:%d]\n"),*Pin->PinName.ToString(),*Pin->DefaultValue,Pin->LinkedTo.Num());
        }
    };
    for (const auto& Handle:System->GetEmitterHandles()) { Report+=TEXT("EMITTER ")+Handle.GetName().ToString()+TEXT("\n"); Dump(Handle.GetInstance().GetEmitterData()); }
    Report+=TEXT("SPARK TEMPLATE\n"); Dump(Spark->GetLatestEmitterData());
    auto* CollisionScript=LoadObject<UNiagaraScript>(nullptr,TEXT("/Niagara/Modules/Collision/Collision.Collision"));
    auto* CollisionSource=Cast<UNiagaraScriptSource>(CollisionScript->GetLatestSource());
    for (UEdGraphNode* Node:CollisionSource->NodeGraph->Nodes)
        for (UEdGraphPin* Pin:Node->Pins)
            if (Pin->PinName.ToString().StartsWith(TEXT("Module."))) Report+=TEXT("COLLISION INPUT ")+Pin->PinName.ToString()+TEXT("\n");
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("NiagaraStructure.txt")));
    if (Params.Contains(TEXT("InspectOnly"))) return 0;
    auto Duplicate=[](UNiagaraSystem* Source,const TCHAR* Name)
    {
        UPackage* Package=CreatePackage(*(FString(TEXT("/Game/Cinematic/"))+Name));
        Package->FullyLoad();
        return CastChecked<UNiagaraSystem>(StaticDuplicateObject(Source,Package,Name));
    };
    auto Save=[](UNiagaraSystem* Asset)
    {
        Asset->RequestCompile(true); Asset->WaitForCompilationComplete(true,false);
        Asset->MarkPackageDirty();
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        return UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
    };
    auto Set=[](UNiagaraNodeFunctionCall* Fn,const TCHAR* Input,FNiagaraTypeDefinition Type,const TCHAR* Value)
    {
        auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Fn,FNiagaraParameterHandle(FName(*(Fn->GetFunctionName()+TEXT(".")+Input))),Type,FGuid(),FGuid());
        Pin.BreakAllPinLinks(); Pin.DefaultValue=Value;
    };
    auto Switch=[](UNiagaraNodeFunctionCall* Fn,const TCHAR* Name,const TCHAR* Value)
    { for (auto* Pin:Fn->Pins) if(Pin->PinName==Name) Pin->DefaultValue=Value; };
    auto* Fire=Duplicate(System,TEXT("NS_CinematicExplosion"));
    Fire->GetExposedParameters().SetParameterValue<int32>(64,FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.ResolutionMaxAxis")));
    Fire->GetExposedParameters().SetParameterValue<FNiagaraBool>(FNiagaraBool(false),FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(),TEXT("User.DrawBounds")));
    // Recolor the fluid temperature LUT itself, rather than putting a pink light
    // over an orange blackbody render. Density and collision remain unchanged.
    TArray<UObject*> FluidObjects;GetObjectsWithOuter(Fire,FluidObjects,EGetObjectsFlags::IncludeNestedObjects);
    for(UObject* Object:FluidObjects)
        if(auto* Curve=Cast<UNiagaraDataInterfaceColorCurve>(Object))
        {
            Curve->CurveAsset=nullptr;
            Curve->RedCurve.Reset();Curve->GreenCurve.Reset();Curve->BlueCurve.Reset();
            for(int32 I=0;I<=4;++I)
            {
                const float T=I/4.f;
                Curve->RedCurve.AddKey(T,T*8.f);
                Curve->GreenCurve.AddKey(T,T*T*T*0.6f);
                Curve->BlueCurve.AddKey(T,T*4.f);
            }
            Curve->UpdateLUT();
        }
    if (!Save(Fire)) return 2;
    auto* Sparks=Duplicate(System,TEXT("NS_ExplosionSparks"));
    TSet<FGuid> Handles; for (const auto& Handle:Sparks->GetEmitterHandles()) Handles.Add(Handle.GetId());
    Sparks->RemoveEmitterHandlesById(Handles);
    FNiagaraEditorUtilities::AddEmitterToSystem(*Sparks,*Spark,Spark->GetExposedVersion().VersionGuid);
    auto Handle=Sparks->GetEmitterHandles().Last();
    auto* Data=Handle.GetInstance().GetEmitterData();
    Data->SimTarget=ENiagaraSimTarget::GPUComputeSim;
    Data->CalculateBoundsMode=ENiagaraEmitterCalculateBoundMode::Fixed;
    Data->FixedBounds=FBox(FVector(-1800,-1800,-150),FVector(1800,1800,2200));
    Sparks->bFixedBounds=true; Sparks->SetFixedBounds(Data->FixedBounds);
    UNiagaraGraph* Graph=CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph;
    const auto OriginalNodes=Graph->Nodes;
    for (UEdGraphNode* Node:OriginalNodes)
    {
        auto* Fn=Cast<UNiagaraNodeFunctionCall>(Node); if(!Fn) continue;
        const FString Name=Fn->GetFunctionName();
        if(Name==TEXT("SpawnBurst_Instantaneous"))
        {
            Set(Fn,TEXT("Spawn Count"),FNiagaraTypeDefinition::GetIntDef(),TEXT("2000"));
            auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Fn,FNiagaraParameterHandle(FName(*(Name+TEXT(".Spawn Count")))),FNiagaraTypeDefinition::GetIntDef(),FGuid(),FGuid());
            FNiagaraVariable V(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.SparkCount"));
            Sparks->GetExposedParameters().SetParameterValue<int32>(2000,V,true);
            TSet<FNiagaraVariableBase> Known; Known.Add(V);
            FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin,V,Known);
        }
        if(Name==TEXT("EmitterState")) { Switch(Fn,TEXT("Loop Behavior"),TEXT("NewEnumerator1")); Set(Fn,TEXT("Loop Duration"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("4.0")); }
        if(Name==TEXT("InitializeParticle"))
        {
            Switch(Fn,TEXT("Color Mode"),TEXT("NewEnumerator1"));
            Set(Fn,TEXT("Color"),FNiagaraTypeDefinition::GetColorDef(),TEXT("(R=10.0,G=0.1,B=4.5,A=1.0)"));
            Set(Fn,TEXT("Lifetime Min"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("1.0"));
            Set(Fn,TEXT("Lifetime Max"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("2.8"));
            Set(Fn,TEXT("Uniform Sprite Size Min"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("1.2"));
            Set(Fn,TEXT("Uniform Sprite Size Max"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("4.0"));
        }
        if(Name==TEXT("AddVelocity")) Set(Fn,TEXT("Velocity Speed"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("750.0"));
        if(Name==TEXT("ShapeLocation")) Set(Fn,TEXT("Sphere Radius"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("35.0"));
        if(Name==TEXT("Drag")) Set(Fn,TEXT("Drag"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.65"));
    }
    auto* Update=Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);
    // Follow the parameter-map chain (not the unordered graph node array).
    // Collision must run immediately BEFORE the solver that applies its response.
    TArray<UNiagaraNodeFunctionCall*> Ordered;
    UEdGraphNode* Cursor=Update;
    TSet<UEdGraphNode*> Visited;
    while(Cursor && !Visited.Contains(Cursor))
    {
        Visited.Add(Cursor);
        if(auto* Fn=Cast<UNiagaraNodeFunctionCall>(Cursor)) Ordered.Insert(Fn,0);
        UEdGraphNode* Previous=nullptr;
        for(auto* Pin:Cursor->Pins)
            if(Pin->Direction==EGPD_Input && Pin->PinType.PinSubCategoryObject==FNiagaraTypeDefinition::GetParameterMapDef().GetStruct() && Pin->LinkedTo.Num())
            { Previous=Pin->LinkedTo[0]->GetOwningNode(); break; }
        Cursor=Previous;
    }
    const int32 SolverIndex=Ordered.IndexOfByPredicate([](const UNiagaraNodeFunctionCall* Fn) { return Fn->GetFunctionName()==TEXT("SolveForcesAndVelocity"); });
    if(SolverIndex==INDEX_NONE) { UE_LOG(LogTemp,Error,TEXT("Cannot locate spark physics solver")); return 4; }
    auto* Collision=FNiagaraStackGraphUtilities::AddScriptModuleToStack(CollisionScript,*Update,SolverIndex,TEXT("WorldCollision"));
    for(auto* Pin:Collision->Pins) Report+=TEXT("COLLISION SWITCH ")+Pin->PinName.ToString()+TEXT(" = ")+Pin->DefaultValue+TEXT("\n");
    // Global distance fields remain available when sparks leave the camera's depth buffer.
    Switch(Collision,TEXT("GPU Collision Type"),TEXT("NewEnumerator2"));
    Switch(Collision,TEXT("Kill Offscreen Particles"),TEXT("false"));
    Set(Collision,TEXT("Restitution"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.45"));
    Set(Collision,TEXT("Friction"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.3"));
    Graph->NotifyGraphChanged();
    if(!Save(Sparks)) return 3;
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("NiagaraStructure.txt")));
    UE_LOG(LogTemp,Display,TEXT("NIAGARA_AUTHOR_OK"));
    UE_LOG(LogTemp,Display,TEXT("NIAGARA_INSPECT_OK"));
    return 0;
}
