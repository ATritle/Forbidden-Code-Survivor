#include "NecromancyVisualComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"

void UNecromancyVisualComponent::SetRiderState(FName State,float Duration)
{
    UAnimSequence* Clip=RiderClips.FindRef(State);
    if(!Clip) return;
    if(RiderState!=State) { RiderState=State;PlayAnimation(Clip,Duration<=0.f);SetPlayRate(1.f); }
    RiderTimer=Duration;
}
void UNecromancyVisualComponent::TriggerRiderCast(FName Style)
{
    if(bHoverRider && CastCooldown<=0.f && RiderState!=TEXT("Hit") && RiderState!=TEXT("Defeat"))
    {
        const FName Rotation[]={TEXT("Cast"),TEXT("CastBlood"),TEXT("CastStorm"),TEXT("HoverIdleAlt")};
        if(Style.IsNone()) Style=Rotation[AttackRotation++ % UE_ARRAY_COUNT(Rotation)];
        const UAnimSequence* Clip=RiderClips.FindRef(Style);
        if(!Clip) return;
        const float Duration=Style==TEXT("HoverIdleAlt") ? 1.2f : 0.8f;
        SetRiderState(Style,Duration);
        SetPlayRate(Clip->GetPlayLength()/Duration);
        CastCooldown=Duration;RiderIdleTime=0.f;
    }
}
void UNecromancyVisualComponent::TriggerRiderHit()
{
    if(bHoverRider) {SetRiderState(TEXT("Hit"),0.4f);RiderIdleTime=0.f;}
}
void UNecromancyVisualComponent::UpdateRider(float Dt,FVector Velocity,bool bFalling,int32 LandingCount,bool bDead)
{
    if(!bHoverRider || !bArtReady || Dt<=0.f) return;
    CastCooldown=FMath::Max(0.f,CastCooldown-Dt);
    RiderTimer=FMath::Max(0.f,RiderTimer-Dt);
    const float Speed=Velocity.Size2D();
    const bool bResting=Speed<=25.f && !bFalling && !bDead && RiderTimer<=0.f;
    RiderIdleTime=bResting ? RiderIdleTime+Dt : 0.f;
    if(!bDead && RiderState==TEXT("Defeat")) SetRiderState(TEXT("HoverIdle"));
    const float Acceleration=(Speed-LastRiderSpeed)/Dt;
    float Turn=0.f;
    if(Speed>12.f)
    {
        const float Desired=Velocity.Rotation().Yaw;
        Turn=FMath::FindDeltaAngleDegrees(RiderYaw,Desired);
        RiderYaw=FMath::FixedTurn(RiderYaw,Desired,360.f*Dt);
        SetWorldRotation(FRotator(0,RiderYaw+FacingYawOffset,0));
    }
    if(bDead) SetRiderState(TEXT("Defeat"),1000.f);
    else if(bFalling) SetRiderState(TEXT("Fall"));
    else if(LandingCount!=LastLandingCount) SetRiderState(TEXT("Land"),0.4f);
    else if(RiderTimer<=0.f)
    {
        if(Speed>25.f && FMath::Abs(Turn)>8.f) SetRiderState(Turn>0 ? TEXT("TurnRight") : TEXT("TurnLeft"));
        else if(Acceleration>400.f && Speed>25.f) SetRiderState(TEXT("Accelerate"),0.22f);
        else if(Acceleration<-400.f && LastRiderSpeed>50.f) SetRiderState(TEXT("Brake"),0.3f);
        else SetRiderState(Speed>25.f ? TEXT("Cruise") : TEXT("HoverIdle"));
    }
    LastLandingCount=LandingCount;LastRiderSpeed=Speed;bWasFalling=bFalling;
}
