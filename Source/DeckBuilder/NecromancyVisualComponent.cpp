#include "NecromancyVisualComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"

UNecromancyVisualComponent::UNecromancyVisualComponent()
{
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
    bEnableUpdateRateOptimizations = true;
    VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    SetVisibility(false);
}

bool UNecromancyVisualComponent::LoadCharacter(const FString& ID, float HeightCentimeters)
{
    bArtReady=false;
    bHoverRider=ID==TEXT("NeonExorcist");RiderClips.Reset();RiderState=NAME_None;RiderTimer=0.f;
    SetVisibility(false);
    const FString Folder = TEXT("/Game/Art/Characters/") + ID + TEXT("/");
    const FString MeshPackage = Folder + TEXT("SK_") + ID;
    // Missing art is a supported state while generation/import is pending.
    if (!FPackageName::DoesPackageExist(MeshPackage)) return false;
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *(MeshPackage + TEXT(".SK_") + ID));
    if (!Mesh || !Mesh->GetSkeleton()) return false;
    SetSkeletalMesh(Mesh);
    const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
    const float Scale = HeightCentimeters / FMath::Max(1.f, static_cast<float>(MeshBounds.BoxExtent.Z * 2.f));
    SetRelativeScale3D(FVector(Scale));
    SetRelativeLocation(FVector(0.f, 0.f, (bHoverRider ? -40.f : -55.f) - (MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z) * Scale));
    auto Animation = [&Folder](const TCHAR* Name) -> UAnimSequence*
    {
        const FString Package = Folder + Name;
        return FPackageName::DoesPackageExist(Package)
            ? LoadObject<UAnimSequence>(nullptr, *(Package + TEXT(".") + Name)) : nullptr;
    };
    Idle = Animation(TEXT("A_Idle"));
    Walk = Animation(TEXT("A_Walk"));
    Run = Animation(TEXT("A_Run"));
    if(bHoverRider)
    {
        for(const TCHAR* State:{TEXT("HoverIdle"),TEXT("Cruise"),TEXT("Accelerate"),TEXT("Brake"),TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("Cast"),TEXT("Hit"),TEXT("Fall"),TEXT("Land"),TEXT("Defeat"),TEXT("CastBlood"),TEXT("CastStorm"),TEXT("HoverIdleAlt")})
        {
            auto* Clip=Animation(*(FString(TEXT("A_"))+State));
            if(!Clip || Clip->GetSkeleton()!=Mesh->GetSkeleton())
            { UE_LOG(LogTemp,Error,TEXT("Rider clip %s missing or skeleton mismatch"),State);return false; }
            RiderClips.Add(FName(State),Clip);
        }
        Idle=RiderClips.FindRef(TEXT("HoverIdle"));Walk=RiderClips.FindRef(TEXT("Cruise"));Run=Walk;
    }
    if (!Walk || !Run) return false;
    CurrentAnimation=nullptr;
    for (UAnimSequence* Sequence : {Idle.Get(), Walk.Get(), Run.Get()})
    {
        if (Sequence && Sequence->GetSkeleton() != Mesh->GetSkeleton())
        {
            UE_LOG(LogTemp, Error, TEXT("Art animation %s does not match %s"), *Sequence->GetName(), *Mesh->GetName());
            return false;
        }
    }
    bArtReady = true;
    SetVisibility(true);
    SetAnimationMode(EAnimationMode::AnimationSingleNode);
    if(bHoverRider) SetRiderState(TEXT("HoverIdle"));
    else UpdateLocomotion(FVector::ZeroVector, 0.f);
    return true;
}

void UNecromancyVisualComponent::UpdateLocomotion(FVector WorldDirection, float Speed)
{
    if (!bArtReady || bHoverRider) return;
    if (!WorldDirection.IsNearlyZero())
        SetWorldRotation(FRotator(0.f, WorldDirection.Rotation().Yaw + FacingYawOffset, 0.f));
    UAnimSequence* Wanted = Speed > 260.f && Run ? Run.Get() : (Speed > 8.f && Walk ? Walk.Get() : Idle.Get());
    if (!Wanted) Wanted = Walk ? Walk.Get() : Run.Get();
    if (Wanted != CurrentAnimation)
    {
        CurrentAnimation = Wanted;
        if (Wanted) PlayAnimation(Wanted, true);
    }
    if (CurrentAnimation)
        SetPlayRate(Speed > 8.f ? FMath::Clamp(Speed / (CurrentAnimation == Run ? 470.f : 155.f), 0.7f, 1.4f) : (Idle ? 1.f : 0.f));
}

void UNecromancyVisualComponent::SetDormant(bool bDormant)
{
    if (!bArtReady) return;
    SetComponentTickEnabled(!bDormant);
    SetVisibility(!bDormant);
    if (bDormant) Stop();
    else
    {
        CurrentAnimation = nullptr;
        UpdateLocomotion(FVector::ZeroVector, 0.f);
        // Desynchronize the shared crowd animation; no extra actor or animation instance allocation.
        if (CurrentAnimation) SetPosition(FMath::FRand() * CurrentAnimation->GetPlayLength(), false);
    }
}
