#include "SkeletonMinion.h"
#include "MortisTerrain.h"
#include "Components/SphereComponent.h"
#include "MVPGame.h"
#include "NecromancyVisualComponent.h"
#include "EnemySpawner.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ASkeletonSummonSpell::ASkeletonSummonSpell() { bRequiresTarget = false; }
TArray<FGameplayTag> ASkeletonSummonSpell::GetAvailableMutationTags() const
{
    TArray<FGameplayTag> Options;
    if (!Modifiers.BoneArcher) Options.Add(FGameplayTag::RequestGameplayTag(TEXT("Mutation.Minion.BoneArcher")));
    if (!Modifiers.GraveGuard) Options.Add(FGameplayTag::RequestGameplayTag(TEXT("Mutation.Minion.GraveGuard")));
    if (!Modifiers.GraveBurst) Options.Add(FGameplayTag::RequestGameplayTag(TEXT("Mutation.Minion.GraveBurst")));
    return Options;
}
void ASkeletonSummonSpell::FireSpell_Implementation(APooledEnemy*, const FSpellModifiers&)
{
    if (auto* Game = Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this))) Game->ReplenishMinions(this);
}

ASkeletonMinion::ASkeletonMinion()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    auto* Body=CreateDefaultSubobject<USphereComponent>(TEXT("Root"));
    Body->InitSphereRadius(28);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionObjectType(ECC_Pawn);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    SetRootComponent(Body);
    Terrain=CreateDefaultSubobject<UMortisTerrain>(TEXT("Terrain"));
    Art=CreateDefaultSubobject<UNecromancyVisualComponent>(TEXT("SkeletonArt"));
    Art->SetupAttachment(RootComponent);
    AllyMarker=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AllyMarker"));
    AllyMarker->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    AllyMarker->SetStaticMesh(Sphere.Object);
    AllyMarker->SetRelativeLocation(FVector(0,0,-49));
    AllyMarker->SetRelativeScale3D(FVector(0.65f,0.65f,0.025f));
    AllyMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AllyMarker->SetCastShadow(false);
}
void ASkeletonMinion::BeginPlay()
{
    Super::BeginPlay();
    // Reuse the already approved animated undead asset until dedicated skeleton art is supplied.
    Art->LoadCharacter(TEXT("Gravebound"),105.f);
    if (auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MVP/M_Prototype.M_Prototype")))
    {
        auto* MID=UMaterialInstanceDynamic::Create(Base,this);
        MID->SetVectorParameterValue(TEXT("Color"),FLinearColor(0.15f,0.8f,1.f));
        AllyMarker->SetMaterial(0,MID);
    }
    Deactivate();
}
void ASkeletonMinion::Activate(ASkeletonSummonSpell* Spell,int32 FormationIndex)
{
    if (!IsValid(Spell) || !IsValid(Spell->GetOwner())) return;
    Terrain->ResetPath();
    Summoner=Spell; Slot=FormationIndex; Target.Reset(); TargetGeneration=0;
    SearchRemaining=FormationIndex*0.1f; AttackRemaining=0.3f; AttackPoseRemaining=0; AttackCount=0;
    SetActorLocation(Spell->GetOwner()->GetActorLocation()+FVector(-100,Slot==0 ? -90 : 90,0));
    SetActorHiddenInGame(false); SetActorTickEnabled(true); Art->SetDormant(false);
}
void ASkeletonMinion::Deactivate()
{
    Summoner.Reset(); Target.Reset(); TargetGeneration=0;
    SetActorHiddenInGame(true); SetActorTickEnabled(false); Art->SetDormant(true);
}
void ASkeletonMinion::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    auto* Spell=Summoner.Get();
    if (!Game || Game->bRunEnded || !Spell || !IsValid(Spell->GetOwner())) { Deactivate(); return; }
    const FVector PlayerPosition=Spell->GetOwner()->GetActorLocation();
    const FSpellModifiers M=Spell->Modifiers; // Existing summons react immediately to mutations.
    const float Leash=M.GraveGuard ? 280.f : 750.f;
    if (FVector::DistSquared2D(GetActorLocation(),PlayerPosition)>FMath::Square(1500.f))
    { SetActorLocation(PlayerPosition+FVector(-100,Slot==0 ? -90 : 90,0)); Target.Reset(); }
    SearchRemaining-=Dt; AttackRemaining-=Dt; AttackPoseRemaining=FMath::Max(0.f,AttackPoseRemaining-Dt);
    if (Target.IsValid() && (!Target->IsPoolActive() || Target->Generation!=TargetGeneration
        || FVector::DistSquared2D(Target->GetActorLocation(),PlayerPosition)>FMath::Square(Leash))) Target.Reset();
    if (SearchRemaining<=0)
    {
        SearchRemaining=0.2f;
        Target=Game->Spawner ? Cast<AMVPEnemy>(Game->Spawner->FindNearestEnemy(PlayerPosition,Leash)) : nullptr;
        TargetGeneration=Target.IsValid() ? Target->Generation : 0;
    }
    FVector Destination=PlayerPosition+FVector(-100,Slot==0 ? -110 : 110,0);
    if (M.GraveGuard)
    {
        const float Angle=Game->RunSeconds*1.7f+Slot*PI;
        Destination=PlayerPosition+FVector(FMath::Cos(Angle)*130,FMath::Sin(Angle)*130,0);
    }
    const float Reach=M.BoneArcher ? 460.f : 85.f;
    if (Target.IsValid()) Destination=Target->GetActorLocation();
    FVector Delta=Destination-GetActorLocation();
    const float StopDistance=Target.IsValid() ? Reach*0.85f : 12.f;
    FVector Velocity=FVector::ZeroVector;
    if (Delta.Size()>StopDistance)
    {
        Velocity=Delta.GetSafeNormal2D()*FMath::Min(430.f,(float(Delta.Size())-StopDistance)/FMath::Max(Dt,0.001f));
        Terrain->MoveTowards(Destination,Velocity.Size(),Dt);
    }
    UMortisTerrain::Ground(this);
    Art->UpdateLocomotion(Delta,Velocity.Size2D());
    // Brief visual-only strike lean; a dedicated attack montage can replace this without moving collision.
    FRotator Pose=Art->GetComponentRotation();
    Pose.Pitch=AttackPoseRemaining>0 ? -7.f : 0.f;
    Art->SetWorldRotation(Pose);
    if (!Target.IsValid() || Delta.Size()>Reach || !UMortisTerrain::ClearSight(this,Destination) || AttackRemaining>0) return;
    AttackRemaining=M.BoneArcher ? 1.1f : 0.8f;
    AttackPoseRemaining=0.12f;
    ++AttackCount;
    const FVector Impact=Target->GetActorLocation();
    if (M.BoneArcher)
    {
        FSpellModifiers Shot;
        Shot.ExplodeOnImpact=M.GraveBurst;
        Game->EmitProjectile(GetActorLocation(),Delta,Spell->Damage,Shot,FLinearColor(0.2f,0.8f,1.f));
    }
    else
    {
        Target->ReceiveSpellDamage(Spell->Damage);
        if (M.GraveBurst)
        {
            Game->EmitImpactPulse(Impact,120);
            TArray<FOverlapResult> Hits;
            GetWorld()->OverlapMultiByObjectType(Hits,Impact,FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(120));
            TSet<AMVPEnemy*> Seen; Seen.Add(Target.Get());
            for (const auto& Hit : Hits)
                if (auto* E=Cast<AMVPEnemy>(Hit.GetActor()); E && E->IsPoolActive() && !Seen.Contains(E))
                { Seen.Add(E); E->ReceiveSpellDamage(Spell->Damage*0.6f); }
        }
    }
}
