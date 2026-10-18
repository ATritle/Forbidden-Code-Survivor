#include "SpellAuthoringCommandlet.h"
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

#include "NiagaraRibbonRendererProperties.h"

USpellAuthoringCommandlet::USpellAuthoringCommandlet() { IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }
int32 USpellAuthoringCommandlet::Main(const FString& Params)
{
    auto Set=[](UNiagaraNodeFunctionCall* Fn,const TCHAR* Input,FNiagaraTypeDefinition Type,const TCHAR* Value)
    {
        auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Fn,FNiagaraParameterHandle(FName(*(Fn->GetFunctionName()+TEXT(".")+Input))),Type,FGuid(),FGuid());
        Pin.BreakAllPinLinks(); Pin.DefaultValue=Value;
    };
    auto Switch=[](UNiagaraNodeFunctionCall* Fn,const TCHAR* Name,const TCHAR* Value)
    { for (auto* Pin:Fn->Pins) if(Pin->PinName==Name) Pin->DefaultValue=Value; };

    auto* Base=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Cinematic/NS_ExplosionSparks.NS_ExplosionSparks"));
    if(!Base) return 1;
    if(Params.Contains(TEXT("FeedbackOnly")))
    {
        UPackage* Package=CreatePackage(TEXT("/Game/Art/Feedback/NS_PhysicalHitSparks"));Package->FullyLoad();
        auto* FX=CastChecked<UNiagaraSystem>(StaticDuplicateObject(Base,Package,TEXT("NS_PhysicalHitSparks")));
        auto* Data=FX->GetEmitterHandles().Last().GetInstance().GetEmitterData();
        Data->bLocalSpace=false;Data->CalculateBoundsMode=ENiagaraEmitterCalculateBoundMode::Fixed;
        Data->FixedBounds=FBox(FVector(-1800,-1800,-1500),FVector(1800,1800,2200));FX->bFixedBounds=true;FX->SetFixedBounds(Data->FixedBounds);
        auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Feedback/M_PhysicalSpark.M_PhysicalSpark"));if(!Material) return 4;
        for(auto* Renderer:Data->GetRenderers()) if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(Renderer)) Sprite->Material=Material;
        FX->GetExposedParameters().SetParameterValue<int32>(360,FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.SparkCount")),true);
        FX->GetExposedParameters().SetParameterValue<FLinearColor>(FLinearColor(8,0.08f,3,1),FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(),TEXT("User.EnergyColor")),true);
        for(const auto& Pair:TArray<TPair<FName,float>>{{TEXT("User.Speed"),750},{TEXT("User.LifeMin"),0.75f},{TEXT("User.LifeMax"),1.5f},{TEXT("User.SizeMin"),3.5f},{TEXT("User.SizeMax"),8},{TEXT("User.SourceRadius"),14}})
            FX->GetExposedParameters().SetParameterValue<float>(Pair.Value,FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),Pair.Key),true);
        auto Link=[&](UNiagaraNodeFunctionCall* Fn,const TCHAR* Input,FNiagaraTypeDefinition Type,const TCHAR* Parameter)
        {
            FNiagaraVariable V(Type,Parameter);auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Fn,FNiagaraParameterHandle(FName(*(Fn->GetFunctionName()+TEXT(".")+Input))),Type,FGuid(),FGuid());
            TSet<FNiagaraVariableBase> Known;Known.Add(V);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin,V,Known);
        };
        UNiagaraGraph* Graph=CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph;bool Collision=false;int32 Links=0;
        const auto Nodes=Graph->Nodes;
        for(UEdGraphNode* Node:Nodes) if(auto* Fn=Cast<UNiagaraNodeFunctionCall>(Node))
        {
            const FString Name=Fn->GetFunctionName();
            if(Name==TEXT("WorldCollision") || Name==TEXT("Collision")) Collision=true;
            if(Name==TEXT("EmitterState")) {Switch(Fn,TEXT("Loop Behavior"),TEXT("NewEnumerator1"));Set(Fn,TEXT("Loop Duration"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("2.0"));}
            if(Name==TEXT("InitializeParticle"))
            {
                Switch(Fn,TEXT("Color Mode"),TEXT("NewEnumerator1"));Link(Fn,TEXT("Color"),FNiagaraTypeDefinition::GetColorDef(),TEXT("User.EnergyColor"));
                Link(Fn,TEXT("Lifetime Min"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.LifeMin"));Link(Fn,TEXT("Lifetime Max"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.LifeMax"));
                Link(Fn,TEXT("Uniform Sprite Size Min"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SizeMin"));Link(Fn,TEXT("Uniform Sprite Size Max"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SizeMax"));Links+=5;
            }
            if(Name==TEXT("AddVelocity")) {Link(Fn,TEXT("Velocity Speed"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Speed"));++Links;}
            if(Name==TEXT("ShapeLocation")) {Link(Fn,TEXT("Sphere Radius"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SourceRadius"));++Links;}
        }
        if(!Collision || Links!=7 || Data->SimTarget!=ENiagaraSimTarget::GPUComputeSim) {UE_LOG(LogTemp,Error,TEXT("FEEDBACK_GRAPH_INVALID collision=%d links=%d"),Collision,Links);return 5;}
        Graph->NotifyGraphChanged();FX->RequestCompile(true);FX->WaitForCompilationComplete(true,false);FX->MarkPackageDirty();
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(Package,FX,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args)) return 3;
        UE_LOG(LogTemp,Display,TEXT("FEEDBACK_NIAGARA_READY GPU distance-field collision preserved; 7 linked controls; shared original fireball solver"));return 0;
    }
    FString Report;
    const bool bCardsOnly=Params.Contains(TEXT("CardsOnly"));
    const bool bArsenal=Params.Contains(TEXT("ArsenalOnly"));
    const TCHAR* ArsenalIDs[]={TEXT("ArcRelay"),TEXT("GhostLink"),TEXT("NeonCataclysm"),TEXT("OrbitRazor"),TEXT("RicochetSeal"),TEXT("StrobeRail"),TEXT("EchoPetals"),TEXT("EventHorizon"),TEXT("ShatterSigil"),TEXT("SatelliteChoir")};
    for(int32 Kind=bArsenal ? 17 : Params.Contains(TEXT("IdentitiesOnly")) ? 10 : bCardsOnly ? 8 : 0;Kind<(bArsenal ? 27 : bCardsOnly ? 10 : 17);++Kind)
    {
        const bool bBeam=Kind==3 || Kind==4 || Kind==6 || Kind==8 || (Kind>=10 && Kind<=12);
        const TCHAR* Names[]={TEXT("NS_Fireball"),TEXT("NS_BloodOrb"),TEXT("NS_BloodImpact"),TEXT("NS_Lightning"),TEXT("NS_FireTail"),TEXT("NS_LandingDust"),TEXT("NS_HoverWake"),TEXT("NS_EnergyAura"),TEXT("NS_KineticTrail"),TEXT("NS_KineticImpact"),TEXT("NS_PulseTrail"),TEXT("NS_HaloTrail"),TEXT("NS_PhotonTrail"),TEXT("NS_PulseImpact"),TEXT("NS_HaloImpact"),TEXT("NS_PhotonImpact"),TEXT("NS_BarrierImpact")};
        const TCHAR* Materials[]={TEXT("M_Fireball"),TEXT("M_BloodDroplets"),TEXT("M_BloodDroplets"),TEXT("M_Lightning"),TEXT("M_FireTail"),TEXT("M_LandingDust"),TEXT("M_HoverWake"),TEXT("M_EnergyAura"),TEXT("M_KineticTrail"),TEXT("M_KineticImpact"),TEXT("M_PulseTrail"),TEXT("M_HaloTrail"),TEXT("M_PhotonTrail"),TEXT("M_PulseImpact"),TEXT("M_HaloImpact"),TEXT("M_PhotonImpact"),TEXT("M_BarrierImpact")};
        const FString SystemName=bArsenal ? FString(TEXT("NS_"))+ArsenalIDs[Kind-17] : Names[Kind];
        const FString MaterialName=bArsenal ? FString(TEXT("M_Sparks_"))+ArsenalIDs[Kind-17] : Materials[Kind];
        const FString Folder=bArsenal ? TEXT("/Game/Art/Arsenal/") : TEXT("/Game/Art/Spells/");
        UPackage* P=CreatePackage(*(Folder+SystemName));P->FullyLoad();
        auto* FX=CastChecked<UNiagaraSystem>(StaticDuplicateObject(Base,P,*SystemName));
        if(Kind!=2 && Kind!=5 && Kind!=9 && Kind<13)
        {
            TSet<FGuid> IDs;for(const auto& H:FX->GetEmitterHandles()) IDs.Add(H.GetId());FX->RemoveEmitterHandlesById(IDs);
            auto* Template=LoadObject<UNiagaraEmitter>(nullptr,bBeam ? TEXT("/Niagara/DefaultAssets/Templates/Emitters/DynamicBeam.DynamicBeam") : TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));
            if(!Template) return 2;
            FNiagaraEditorUtilities::AddEmitterToSystem(*FX,*Template,Template->GetExposedVersion().VersionGuid);
        }
        auto* Data=FX->GetEmitterHandles().Last().GetInstance().GetEmitterData();
        Data->bLocalSpace=Kind<2 || Kind==7;
        Data->CalculateBoundsMode=ENiagaraEmitterCalculateBoundMode::Fixed;
        Data->FixedBounds=FBox(FVector(Kind>=10 ? -650 : -1500),FVector(Kind>=10 ? 650 : 1500));FX->bFixedBounds=true;FX->SetFixedBounds(Data->FixedBounds);
        auto* Material=LoadObject<UMaterialInterface>(nullptr,*(Folder+MaterialName+TEXT(".")+MaterialName));
        if(!Material) return 4;
        for(auto* Renderer:Data->GetRenderers())
        {
            if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(Renderer)) Sprite->Material=Material;
            if(auto* Ribbon=Cast<UNiagaraRibbonRendererProperties>(Renderer)) Ribbon->Material=Material;
        }
        UNiagaraGraph* Graph=CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph;
        const auto Nodes=Graph->Nodes;
        for(UEdGraphNode* Node:Nodes)
        {
            auto* Fn=Cast<UNiagaraNodeFunctionCall>(Node);if(!Fn) continue;
            const FString Name=Fn->GetFunctionName();
            Report+=FString::Printf(TEXT("%d FUNCTION %s\n"),Kind,*Name);
            for(auto* Pin:Fn->Pins) Report+=Pin->PinName.ToString()+TEXT(" = ")+Pin->DefaultValue+TEXT("\n");
            if(Fn->FunctionScript) for(UEdGraphNode* Inner:Fn->GetFunctionScriptSource()->NodeGraph->Nodes) for(auto* Pin:Inner->Pins)
                if(Pin->PinName.ToString().StartsWith(TEXT("Module."))) Report+=TEXT("INPUT ")+Pin->PinName.ToString()+TEXT("\n");
            if(Name==TEXT("SpawnBurst_Instantaneous")) Set(Fn,TEXT("Spawn Count"),FNiagaraTypeDefinition::GetIntDef(),Kind==9 ? TEXT("36") : Kind==2 ? TEXT("80") : Kind==5 ? TEXT("28") : bBeam ? TEXT("32") : TEXT("1"));
            if(Name==TEXT("EmitterState")) { Switch(Fn,TEXT("Loop Behavior"),TEXT("NewEnumerator1"));Set(Fn,TEXT("Loop Duration"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("2.0")); }
            if(bBeam && Name==TEXT("BeamWidth")) Set(Fn,TEXT("Beam Width"),FNiagaraTypeDefinition::GetFloatDef(),Kind==8 ? TEXT("40") : Kind==6 ? TEXT("18") : Kind==4 ? TEXT("66") : TEXT("35"));
            if(Kind>=10 && Kind<=12 && Name==TEXT("BeamWidth")) Set(Fn,TEXT("Beam Width"),FNiagaraTypeDefinition::GetFloatDef(),Kind==10 ? TEXT("30") : Kind==11 ? TEXT("34") : TEXT("22"));
            if(bBeam && Name==TEXT("Color")) Set(Fn,TEXT("Color"),FNiagaraTypeDefinition::GetColorDef(),Kind==8 ? TEXT("(R=3,G=1.6,B=0.05,A=1)") : TEXT("(R=4,G=4,B=4,A=1)"));
            if(Name==TEXT("InitializeParticle"))
            {
                Set(Fn,TEXT("Lifetime"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("2.0"));
                Set(Fn,TEXT("Uniform Sprite Size"),FNiagaraTypeDefinition::GetFloatDef(),(Kind==0 ? TEXT("38") : TEXT("85")));
                Switch(Fn,TEXT("Color Mode"),TEXT("NewEnumerator1"));
                Set(Fn,TEXT("Color"),FNiagaraTypeDefinition::GetColorDef(),Kind==1 || Kind==2 ? TEXT("(R=1,G=0.35,B=0.35,A=1)") : TEXT("(R=3,G=3,B=3,A=1)"));
                if(Kind==0)
                {
                    FNiagaraVariable Color(FNiagaraTypeDefinition::GetColorDef(),TEXT("User.EnergyColor"));
                    FX->GetExposedParameters().SetParameterValue<FLinearColor>(FLinearColor(1.f,0.02f,0.5f),Color,true);
                    auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Fn,FNiagaraParameterHandle(FName(*(Name+TEXT(".Color")))),FNiagaraTypeDefinition::GetColorDef(),FGuid(),FGuid());
                    TSet<FNiagaraVariableBase> Known;Known.Add(Color);
                    FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin,Color,Known);
                }
                Set(Fn,TEXT("Lifetime Min"),FNiagaraTypeDefinition::GetFloatDef(),Kind==2 ? TEXT("0.4") : TEXT("2.0"));
                Set(Fn,TEXT("Lifetime Max"),FNiagaraTypeDefinition::GetFloatDef(),Kind==2 ? TEXT("1.2") : TEXT("2.0"));
                Set(Fn,TEXT("Uniform Sprite Size Min"),FNiagaraTypeDefinition::GetFloatDef(),Kind==2 ? TEXT("12") : (Kind==0 ? TEXT("38") : TEXT("85")));
                Set(Fn,TEXT("Uniform Sprite Size Max"),FNiagaraTypeDefinition::GetFloatDef(),Kind==2 ? TEXT("26") : (Kind==0 ? TEXT("38") : TEXT("85")));
            }
            if(Kind==5 && Name==TEXT("AddVelocity")) Set(Fn,TEXT("Velocity Speed"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("130"));
            if(Kind==7 && Name==TEXT("InitializeParticle"))
            {
                for(const TCHAR* Input:{TEXT("Lifetime"),TEXT("Lifetime Min"),TEXT("Lifetime Max")}) Set(Fn,Input,FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.45"));
                for(const TCHAR* Input:{TEXT("Uniform Sprite Size"),TEXT("Uniform Sprite Size Min"),TEXT("Uniform Sprite Size Max")}) Set(Fn,Input,FNiagaraTypeDefinition::GetFloatDef(),TEXT("140"));
            }
            if(Kind==5 && Name==TEXT("InitializeParticle"))
            {
                Set(Fn,TEXT("Color"),FNiagaraTypeDefinition::GetColorDef(),TEXT("(R=0.18,G=0.22,B=0.2,A=0.4)"));
                Set(Fn,TEXT("Lifetime"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.45"));
                Set(Fn,TEXT("Lifetime Min"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.25"));
                Set(Fn,TEXT("Lifetime Max"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.5"));
                Set(Fn,TEXT("Uniform Sprite Size Min"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("12"));
                Set(Fn,TEXT("Uniform Sprite Size Max"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("25"));
            }
            if(Kind==2 && Name==TEXT("AddVelocity")) Set(Fn,TEXT("Velocity Speed"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("260"));
            if(Kind==9 && Name==TEXT("InitializeParticle"))
            {
                Set(Fn,TEXT("Lifetime"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.55"));
                Set(Fn,TEXT("Lifetime Min"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.3"));
                Set(Fn,TEXT("Lifetime Max"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.65"));
                for(const TCHAR* Input:{TEXT("Uniform Sprite Size"),TEXT("Uniform Sprite Size Min"),TEXT("Uniform Sprite Size Max")}) Set(Fn,Input,FNiagaraTypeDefinition::GetFloatDef(),TEXT("18"));
                Set(Fn,TEXT("Color"),FNiagaraTypeDefinition::GetColorDef(),TEXT("(R=3,G=1.6,B=0.05,A=1)"));
            }
            if(Kind==9 && Name==TEXT("AddVelocity")) Set(Fn,TEXT("Velocity Speed"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("210"));
            if(Kind>=13 && Kind<17)
            {
                const int32 Counts[]={18,24,14,20};
                if(Name==TEXT("SpawnBurst_Instantaneous")) Set(Fn,TEXT("Spawn Count"),FNiagaraTypeDefinition::GetIntDef(),*FString::FromInt(Counts[Kind-13]));
                if(Name==TEXT("InitializeParticle"))
                {
                    Set(Fn,TEXT("Lifetime"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.5"));
                    Set(Fn,TEXT("Lifetime Min"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.3"));
                    Set(Fn,TEXT("Lifetime Max"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.65"));
                    for(const TCHAR* Input:{TEXT("Uniform Sprite Size"),TEXT("Uniform Sprite Size Min"),TEXT("Uniform Sprite Size Max")}) Set(Fn,Input,FNiagaraTypeDefinition::GetFloatDef(),Kind==14 ? TEXT("14") : TEXT("20"));
                }
                if(Name==TEXT("AddVelocity")) Set(Fn,TEXT("Velocity Speed"),FNiagaraTypeDefinition::GetFloatDef(),Kind==14 ? TEXT("190") : Kind==15 ? TEXT("90") : TEXT("125"));
            }
            if(bArsenal)
            {
                if(Name==TEXT("SpawnBurst_Instantaneous")) Set(Fn,TEXT("Spawn Count"),FNiagaraTypeDefinition::GetIntDef(),Kind==19 ? TEXT("96") : Kind==24 ? TEXT("40") : TEXT("28"));
                if(Name==TEXT("InitializeParticle"))
                {
                    Set(Fn,TEXT("Lifetime"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.5"));
                    Set(Fn,TEXT("Lifetime Min"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.25"));
                    Set(Fn,TEXT("Lifetime Max"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("0.65"));
                    for(const TCHAR* Input:{TEXT("Uniform Sprite Size"),TEXT("Uniform Sprite Size Min"),TEXT("Uniform Sprite Size Max")}) Set(Fn,Input,FNiagaraTypeDefinition::GetFloatDef(),Kind==19 ? TEXT("24") : TEXT("15"));
                    Set(Fn,TEXT("Color"),FNiagaraTypeDefinition::GetColorDef(),TEXT("(R=1,G=1,B=1,A=1)"));
                }
                if(Name==TEXT("AddVelocity")) Set(Fn,TEXT("Velocity Speed"),FNiagaraTypeDefinition::GetFloatDef(),Kind==19 ? TEXT("360") : TEXT("160"));
            }
            if(bBeam && Name==TEXT("BeamEmitterSetup"))
            {
                Switch(Fn,TEXT("Absolute Beam End"),TEXT("true"));
                Switch(Fn,TEXT("Absolute Beam Start"),TEXT("true"));
                for(const TCHAR* End:{TEXT("Start"),TEXT("End")})
                {
                    FNiagaraVariable V(FNiagaraTypeDefinition::GetVec3Def(),FName(*(FString(TEXT("User.Beam"))+End)));
                    FX->GetExposedParameters().SetParameterValue<FVector3f>(FVector3f::ZeroVector,V,true);
                    auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Fn,FNiagaraParameterHandle(FName(*(Name+TEXT(".Beam ")+End))),FNiagaraTypeDefinition::GetVec3Def(),FGuid(),FGuid());
                    TSet<FNiagaraVariableBase> Known;Known.Add(V);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin,V,Known);
                }
            }
        }
        // Runtime density control applies to the small signature bursts as well as the shared sparks.
        if(Kind==9 || bArsenal)
        {
            const int32 Count=Kind==9 ? 36 : Kind==19 ? 96 : Kind==24 ? 40 : 28;
            FNiagaraVariable V(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.SparkCount"));FX->GetExposedParameters().SetParameterValue<int32>(Count,V,true);
            for(UEdGraphNode* Node:Nodes) if(auto* Fn=Cast<UNiagaraNodeFunctionCall>(Node);Fn && Fn->GetFunctionName()==TEXT("SpawnBurst_Instantaneous"))
            {
                auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Fn,FNiagaraParameterHandle(FName(*(Fn->GetFunctionName()+TEXT(".Spawn Count")))),FNiagaraTypeDefinition::GetIntDef(),FGuid(),FGuid());
                TSet<FNiagaraVariableBase> Known;Known.Add(V);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin,V,Known);
            }
        }
        if(Kind==3)
        {
            auto* Script=LoadObject<UNiagaraScript>(nullptr,TEXT("/Niagara/Modules/Update/Position/JitterPosition.JitterPosition"));
            auto* Output=Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);
            if(Script && Output)
            {
                auto* Fn=FNiagaraStackGraphUtilities::AddScriptModuleToStack(Script,*Output,INDEX_NONE,TEXT("ArcIrregularity"));
                for(UEdGraphNode* N:Fn->GetFunctionScriptSource()->NodeGraph->Nodes) for(auto* Pin:N->Pins)
                    if(Pin->PinName.ToString().StartsWith(TEXT("Module."))) Report+=TEXT("JITTER ")+Pin->PinName.ToString()+TEXT(" = ")+Pin->DefaultValue+TEXT("\n");
                Set(Fn,TEXT("Jitter Amount"),FNiagaraTypeDefinition::GetVec3Def(),TEXT("(X=8,Y=8,Z=8)"));
            }
        }
        Graph->NotifyGraphChanged();FX->RequestCompile(true);FX->WaitForCompilationComplete(true,false);FX->MarkPackageDirty();
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(P,FX,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),Args)) return 3;
    }
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("SpellNiagaraStructure.txt")));
    UE_LOG(LogTemp,Display,TEXT("SPELL_NIAGARA_READY"));return 0;
}
