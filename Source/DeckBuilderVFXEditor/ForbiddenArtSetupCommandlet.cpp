#include "ForbiddenArtSetupCommandlet.h"
#include "ForbiddenRiderAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "AssetToolsModule.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraphNode.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

UForbiddenArtSetupCommandlet::UForbiddenArtSetupCommandlet() {IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UForbiddenArtSetupCommandlet::Main(const FString& Params)
{
    const FString Folder=TEXT("/Game/Art/Characters/NeonExorcist");
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*(Folder/TEXT("SK_NeonExorcist.SK_NeonExorcist")));if(!Mesh) return 1;
    auto* BP=LoadObject<UAnimBlueprint>(nullptr,*(Folder/TEXT("ABP_NeonRider.ABP_NeonRider")));
    if(!BP)
    {
        auto* Factory=NewObject<UAnimBlueprintFactory>();Factory->ParentClass=UForbiddenRiderAnimInstance::StaticClass();Factory->TargetSkeleton=Mesh->GetSkeleton();Factory->PreviewSkeletalMesh=Mesh;
        BP=Cast<UAnimBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(TEXT("ABP_NeonRider"),Folder,UAnimBlueprint::StaticClass(),Factory));
    }
    if(!BP) return 2;
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UEdGraph* Graph=nullptr;
    for(auto* G:Graphs) if(G->GetFName()==TEXT("AnimGraph")) Graph=G;
    if(!Graph) return 3;
    UAnimGraphNode_Root* Root=nullptr;const auto OldNodes=Graph->Nodes;
    for(UEdGraphNode* Node:OldNodes) {if(auto* R=Cast<UAnimGraphNode_Root>(Node)) Root=R;else Graph->RemoveNode(Node);}
    if(!Root) return 4;Root->BreakAllNodeLinks();
    auto* Blend=NewObject<UAnimGraphNode_BlendListByInt>(Graph);Graph->AddNode(Blend);Blend->CreateNewGuid();Blend->PostPlacedNewNode();Blend->AllocateDefaultPins();
    while(Blend->Node.GetBlendTimes().Num()<14) Blend->AddPinToBlendList();
    auto* BlendTimes=FindFProperty<FArrayProperty>(FAnimNode_BlendListBase::StaticStruct(),TEXT("BlendTime"));
    if(!BlendTimes) return 5;
    FScriptArrayHelper Times(BlendTimes,BlendTimes->ContainerPtrToValuePtr<void>(&Blend->Node));
    for(int32 I=0;I<Times.Num();++I) CastFieldChecked<FFloatProperty>(BlendTimes->Inner)->SetPropertyValue(Times.GetRawPtr(I),0.12f);
    if(auto* Mode=FindFProperty<FEnumProperty>(FAnimNode_BlendListBase::StaticStruct(),TEXT("ChildUpateMode"))) Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(&Blend->Node),int64(EBlendListChildUpdateMode::ResetChildOnActivate));
    Blend->ReconstructNode();
    const UEdGraphSchema* Schema=Graph->GetSchema();
    auto Output=[](UEdGraphNode* Node)->UEdGraphPin* {for(auto* Pin:Node->Pins) if(Pin->Direction==EGPD_Output) return Pin;return nullptr;};
    auto Connect=[&](UEdGraphPin* A,UEdGraphPin* B)
    {if(!A || !B || !Schema->TryCreateConnection(A,B)) {UE_LOG(LogTemp,Error,TEXT("ANIM_GRAPH_LINK_FAILED %s -> %s"),A ? *A->PinName.ToString() : TEXT("null"),B ? *B->PinName.ToString() : TEXT("null"));return false;}return true;};
    if(!Connect(Output(Blend),Root->FindPin(TEXT("Result")))) return 6;
    auto Variable=[&](FName Name)
    {
        auto* Node=NewObject<UK2Node_VariableGet>(Graph);Node->VariableReference.SetSelfMember(Name);Graph->AddNode(Node);Node->CreateNewGuid();Node->PostPlacedNewNode();Node->AllocateDefaultPins();return Node;
    };
    auto* Index=Variable(GET_MEMBER_NAME_CHECKED(UForbiddenRiderAnimInstance,PoseIndex));
    auto* Rate=Variable(GET_MEMBER_NAME_CHECKED(UForbiddenRiderAnimInstance,SequenceRate));
    if(!Connect(Output(Index),Blend->FindPin(TEXT("ActiveChildIndex")))) return 7;
    const TCHAR* States[]={TEXT("HoverIdle"),TEXT("Cruise"),TEXT("Accelerate"),TEXT("Brake"),TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("Cast"),TEXT("Hit"),TEXT("Fall"),TEXT("Land"),TEXT("Defeat"),TEXT("CastBlood"),TEXT("CastStorm"),TEXT("HoverIdleAlt")};
    for(int32 I=0;I<14;++I)
    {
        const FString Path=Folder/TEXT("A_")+States[I];auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Path+TEXT(".A_")+States[I]));
        if(!Clip || Clip->GetSkeleton()!=Mesh->GetSkeleton()) return 8;
        auto* Node=NewObject<UAnimGraphNode_SequencePlayer>(Graph);Graph->AddNode(Node);Node->CreateNewGuid();Node->Node.SetSequence(Clip);
        Node->Node.SetLoopAnimation(I==0 || I==1 || I==4 || I==5 || I==8);Node->PostPlacedNewNode();Node->AllocateDefaultPins();
        for(auto& Pin:Node->ShowPinForProperties) if(Pin.PropertyName==TEXT("PlayRate")) Pin.bShowPin=true;
        Node->ReconstructNode();
        if(!Connect(Output(Node),Blend->FindPin(*FString::Printf(TEXT("BlendPose_%d"),I)))) return 9;
        if(!Connect(Output(Rate),Node->FindPin(TEXT("PlayRate")))) return 10;
        Node->NodePosX=-600;Node->NodePosY=I*150;
    }
    Blend->NodePosX=0;Root->NodePosX=500;Index->NodePosX=-300;Index->NodePosY=-200;Rate->NodePosX=-950;Rate->NodePosY=-250;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    if(BP->Status==BS_Error || !BP->GeneratedClass) return 11;
    BP->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    const FString Filename=FPackageName::LongPackageNameToFilename(BP->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension());
    if(!UPackage::SavePackage(BP->GetPackage(),BP,*Filename,Args)) return 12;
    UE_LOG(LogTemp,Display,TEXT("FORBIDDEN_RIDER_ANIMBP_PASS: 14 board-safe clips, 0.12s blends, inactive children disabled, native state/rate input"));return 0;
}
