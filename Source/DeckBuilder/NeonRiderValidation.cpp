#include "MVPGame.h"
#include "NecromancyVisualComponent.h"
#include "Animation/AnimSequence.h"

// Opt-in smoke validation samples the imported FBX clips, not just the Blender source.
const TCHAR* ValidateNeonRider(AMVPPlayer* Player)
{
    auto* Live=Player->FindComponentByClass<UNecromancyVisualComponent>();
    const FName BoardBone=Live && Live->GetBoneIndex(TEXT("Hoverboard"))!=INDEX_NONE ? TEXT("Hoverboard") : TEXT("Hoverboard1");
    if(!Live || !Live->IsHoverRider() || Live->GetBoneIndex(BoardBone)==INDEX_NONE)
        return TEXT("Main player is not the rigged hoverboard rider");
    if(!Live->UsesBlendedRider() || !Live->GetAnimInstance()) return TEXT("Main player has no blended rider animation blueprint");
    auto* Probe=NewObject<UNecromancyVisualComponent>(Player);Probe->RegisterComponent();
    if(!Probe->LoadCharacter(TEXT("NeonExorcist"),170.f)) {Probe->DestroyComponent();return TEXT("Rider animation assets missing");}
    Probe->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    FVector Anchors[2];bool bFirst=true;float Worst=0.f;
    for(const TCHAR* State:{TEXT("HoverIdle"),TEXT("Cruise"),TEXT("Accelerate"),TEXT("Brake"),TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("Cast"),TEXT("Hit"),TEXT("Fall"),TEXT("Land"),TEXT("Defeat"),TEXT("CastBlood"),TEXT("CastStorm"),TEXT("HoverIdleAlt")})
    {
        const FString Package=FString(TEXT("/Game/Art/Characters/NeonExorcist/A_"))+State;
        auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Package+TEXT(".A_")+State));
        if(!Clip){Probe->DestroyComponent();return TEXT("Rider clip missing");}
        Probe->PlayAnimation(Clip,false);
        FVector HandStart=FVector::ZeroVector;float HandTravel=0.f;
        for(int32 Sample=0;Sample<=4;++Sample)
        {
            Probe->SetPosition(Clip->GetPlayLength()*Sample/4.f,false);
            Probe->TickAnimation(0.f,false);Probe->RefreshBoneTransforms();
            const FTransform Board=Probe->GetSocketTransform(BoardBone);
            const FVector Hand=Board.InverseTransformPosition(Probe->GetSocketLocation(TEXT("RightHand")))*Board.GetScale3D().GetAbs();
            if(Sample==0) HandStart=Hand;
            HandTravel=FMath::Max(HandTravel,float(FVector::Distance(Hand,HandStart)));
            for(int32 I=0;I<2;++I)
            {
                const FVector Contact=Board.InverseTransformPosition(Probe->GetSocketLocation(I==0 ? TEXT("LeftFoot") : TEXT("RightFoot")))*Board.GetScale3D().GetAbs();
                if(bFirst) Anchors[I]=Contact;
                Worst=FMath::Max(Worst,float(FVector::Distance(Contact,Anchors[I])));
            }
            bFirst=false;
        }
        if(FName(State)==TEXT("CastBlood") || FName(State)==TEXT("CastStorm"))
        {
            UE_LOG(LogTemp,Display,TEXT("NEON_ATTACK_MOTION %s handTravel=%.3f cm"),State,HandTravel);
            if(HandTravel<1.f){Probe->DestroyComponent();return TEXT("New attack clip has no visible upper-body motion");}
        }
    }
    if(Worst>1.f){UE_LOG(LogTemp,Error,TEXT("NEON_FOOT_ERROR %.4f cm"),Worst);Probe->DestroyComponent();return TEXT("Imported rider feet slip relative to board");}
    Probe->TriggerRiderCast();if(Probe->GetRiderState()!=TEXT("Cast")){Probe->DestroyComponent();return TEXT("Rider cast state failed");}
    Probe->TriggerRiderHit();if(Probe->GetRiderState()!=TEXT("Hit")){Probe->DestroyComponent();return TEXT("Rider hit state failed");}
    Probe->UpdateRider(0.1f,FVector::ZeroVector,true,0,false);
    if(Probe->GetRiderState()!=TEXT("Fall")){Probe->DestroyComponent();return TEXT("Rider fall state failed");}
    Probe->UpdateRider(0.1f,FVector::ZeroVector,false,1,false);
    if(Probe->GetRiderState()!=TEXT("Land")){Probe->DestroyComponent();return TEXT("Rider landing state failed");}
    Probe->UpdateRider(0.1f,FVector::ZeroVector,false,1,true);
    Probe->UpdateRider(0.1f,FVector::ZeroVector,false,1,false);
    if(Probe->GetRiderState()==TEXT("Defeat")){Probe->DestroyComponent();return TEXT("Rider defeat state did not reset");}
    for(const FName Expected:{FName(TEXT("CastBlood")),FName(TEXT("CastStorm")),FName(TEXT("HoverIdleAlt")),FName(TEXT("Cast"))})
    {
        Probe->UpdateRider(1.3f,FVector::ZeroVector,false,1,false);Probe->TriggerRiderCast();
        if(Probe->GetRiderState()!=Expected){Probe->DestroyComponent();return TEXT("Four-clip energy attack rotation failed (including #250)");}
    }
    Probe->DestroyComponent();
    UE_LOG(LogTemp,Display,TEXT("NEON_RIDER_VALIDATION_PASS: 14 clips, board rigidity/foot contact %.4f cm, cast/hit/fall/land/reset, attacks #135/#133, attack flourish #250"),Worst);
    return nullptr;
}
