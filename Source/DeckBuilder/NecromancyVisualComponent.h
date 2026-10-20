#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "NecromancyVisualComponent.generated.h"

class UAnimSequence;

/** Shared visual-only Meshy integration. Gameplay collision and camera never rotate with the mesh. */
UCLASS(ClassGroup=(Art), meta=(BlueprintSpawnableComponent))
class DECKBUILDER_API UNecromancyVisualComponent : public USkeletalMeshComponent
{
    GENERATED_BODY()
public:
    UNecromancyVisualComponent();
    /** Canonical import layout: /Game/Art/Characters/<ID>/{SK_<ID>,A_Idle,A_Walk,A_Run}. */
    bool LoadCharacter(const FString& ID, float HeightCentimeters);
    void UpdateLocomotion(FVector WorldDirection, float Speed);
    void SetDormant(bool bDormant);
    bool IsArtReady() const { return bArtReady; }
    bool IsHoverRider() const { return bHoverRider; }
    void UpdateRider(float Dt, FVector Velocity, bool bFalling, int32 LandingCount, bool bDead);
    void TriggerRiderCast(FName Style=NAME_None);
    void TriggerRiderHit();
    FName GetRiderState() const { return RiderState; }
    float GetRiderPlayRate() const {return RiderPlayRate;}
    bool UsesBlendedRider() const {return bBlendedRider;}
    UPROPERTY(EditAnywhere, Category="Art")
    float FacingYawOffset = 0.f;
private:
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> Run;
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CurrentAnimation;
    bool bArtReady = false;
    bool bHoverRider = false;
    UPROPERTY(Transient) TMap<FName,TObjectPtr<UAnimSequence>> RiderClips;
    FName RiderState=NAME_None;
    float RiderTimer=0.f;
    float CastCooldown=0.f;
    int32 AttackRotation=0;
    int32 RiderAttackVariants=4;
    float LastRiderSpeed=0.f;
    float RiderIdleTime=0.f;
    float RiderYaw=0.f;
    int32 LastLandingCount=0;
    bool bWasFalling=false;
    bool bBlendedRider=false;
    float RiderPlayRate=1.f;
    void SetRiderState(FName State, float Duration=0.f);
};
