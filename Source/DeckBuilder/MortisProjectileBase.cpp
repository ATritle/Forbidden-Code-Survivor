#include "MortisProjectileBase.h"
#include "ForbiddenEnemyBrain.h"
#include "ForbiddenCombatSubsystem.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "MVPGame.h"
#include "NeonAudioDirector.h"
#include "KineticCardVFXDirector.h"
#include "NeonSpellImpactDirector.h"
#include "CinematicExplosionDirector.h"
#include "BloodSurfaceDirector.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Materials/MaterialInterface.h"

AMortisProjectileBase::AMortisProjectileBase()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->InitSphereRadius(12.f); SetRootComponent(Collision);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    Visual->SetupAttachment(Collision); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Trail=CreateDefaultSubobject<UNiagaraComponent>(TEXT("ElementalTrail"));
    Trail->SetupAttachment(Collision); Trail->SetAutoActivate(false);
    for(int32 I=0;I<4;++I)
    {
        auto* FX=CreateDefaultSubobject<UNiagaraComponent>(*FString::Printf(TEXT("AuxiliaryFX%d"),I));
        FX->SetupAttachment(Collision);FX->SetAutoActivate(false);Branches.Add(FX);
    }
    ProjectileMovement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->ProjectileGravityScale=0;
    ProjectileMovement->bRotationFollowsVelocity=true;
    ProjectileMovement->bForceSubStepping=true;
    ProjectileMovement->bTickBeforeOwner=false;
    ProjectileMovement->OnProjectileStop.AddDynamic(this,&AMortisProjectileBase::Stopped);
    ProjectileMovement->PrimaryComponentTick.AddPrerequisite(this,PrimaryActorTick);
}
void AMortisProjectileBase::BeginPlay()
{
    Super::BeginPlay();
    FireTailAsset=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_FireTail.NS_FireTail"));
    ChargedCardMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Spells/M_KineticCard.M_KineticCard"));
    CardTrailAsset=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_KineticTrail.NS_KineticTrail"));
    const TCHAR* Cores[]={TEXT("M_PulseDart"),TEXT("M_Fireball"),TEXT("M_HaloShard"),TEXT("M_PhotonLance"),TEXT("M_KineticCard")};
    const TCHAR* Tails[]={TEXT("NS_PulseTrail"),TEXT("NS_FireTail"),TEXT("NS_HaloTrail"),TEXT("NS_PhotonTrail"),TEXT("NS_KineticTrail")};
    for(int32 I=0;I<5;++I)
    {
        CoreMaterials.Add(LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Spells/%s.%s"),Cores[I],Cores[I])));
        StyleTrails.Add(LoadObject<UNiagaraSystem>(nullptr,*FString::Printf(TEXT("/Game/Art/Spells/%s.%s"),Tails[I],Tails[I])));
    }
    Deactivate();
}
void AMortisProjectileBase::SetupProjectileVisuals(EProjectileType Type,UStaticMesh* MeshAsset,UNiagaraSystem* ParticleAsset)
{
    ProjectileType=Type;
    if(Type==EProjectileType::PhysicalMesh) SpellStyle=ENeonSpellStyle::Kinetic;
    Visual->SetStaticMesh(MeshAsset);
    Visual->SetVisibility(Type==EProjectileType::PhysicalMesh && MeshAsset);
    if (MeshAsset)
    {
        const FVector Extent=MeshAsset->GetBounds().BoxExtent;
        const int32 Axis=Extent.GetMax()==Extent.Z ? 2 : Extent.GetMax()==Extent.Y ? 1 : 0;
        Visual->SetRelativeRotation(Axis==2 ? FRotator(90,0,0) : Axis==1 ? FRotator(0,-90,0) : FRotator::ZeroRotator);
        Visual->SetRelativeScale3D(FVector(140.f/FMath::Max(1.f,float(Extent.GetMax()*2))));
        if(Type==EProjectileType::PhysicalMesh && ChargedCardMaterial)
        {
            Visual->SetMaterial(0,ChargedCardMaterial);
            Visual->SetRelativeRotation(FRotator::ZeroRotator);
            Visual->SetRelativeScale3D(FVector(0.42f,0.27f,1.f));
        }
    }
    Trail->DeactivateImmediate(); Trail->SetAsset(ParticleAsset);
    for(UNiagaraComponent* FX:Branches) { FX->DeactivateImmediate();FX->SetAsset(Type==EProjectileType::ContinuousBeam ? ParticleAsset : nullptr); }
    if(Type!=EProjectileType::ContinuousBeam && !bBloodSpell) Branches[0]->SetAsset(Type==EProjectileType::PhysicalMesh ? CardTrailAsset.Get() : FireTailAsset.Get());
    if(!bBloodSpell && Type!=EProjectileType::ContinuousBeam && SpellStyle!=ENeonSpellStyle::Prism)
    {
        const int32 Style=int32(SpellStyle);
        if(CoreMaterials.IsValidIndex(Style) && CoreMaterials[Style] && MeshAsset)
        {
            Visual->SetMaterial(0,CoreMaterials[Style]);Visual->SetVisibility(true);Visual->SetRelativeRotation(FRotator::ZeroRotator);
            BaseVisualScale=SpellStyle==ENeonSpellStyle::Pulse ? FVector(0.55f,0.24f,1) : SpellStyle==ENeonSpellStyle::Photon ? FVector(1.1f,0.26f,1)
                : SpellStyle==ENeonSpellStyle::Halo ? FVector(0.42f,0.42f,1) : FVector(0.55f,0.36f,1);
            Visual->SetRelativeScale3D(BaseVisualScale);
        }
        Trail->SetAsset(nullptr);
        Branches[0]->SetAsset(StyleTrails.IsValidIndex(Style) ? StyleTrails[Style].Get() : nullptr);
    }
}
void AMortisProjectileBase::Launch(FVector Origin,FVector Direction,float Damage,const FSpellModifiers& Snapshot,FLinearColor Color)
{
    if (Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero() || !FMath::IsFinite(Damage) || Damage<=0) return;
    SetActorLocationAndRotation(Origin,Direction.Rotation());
    LaunchOrigin=Origin; BranchUpdate=0.f;
    FlightAge=0.f;SparkTrailAge=0.f;SpinPhase=FMath::FRandRange(0.f,360.f);Visual->SetRelativeLocation(FVector::ZeroVector);
    Visual->SetRelativeRotation(FRotator::ZeroRotator);
    ShotDamage=Damage; ShotModifiers=Snapshot; PiercingCount=FMath::Max(0,Snapshot.PiercingCount);
    HitLives.Reset(); RemainingLife=1.7f; bActive=true;Returning=false;
    SetActorHiddenInGame(false); SetActorTickEnabled(true);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ProjectileMovement->SetUpdatedComponent(Collision);
    const float Speed=SpellStyle==ENeonSpellStyle::Pulse ? 1450.f : SpellStyle==ENeonSpellStyle::Photon ? 1650.f : SpellStyle==ENeonSpellStyle::Halo ? 1150.f : 1250.f;
    ProjectileMovement->Velocity=Direction.GetSafeNormal()*Speed;
    ProjectileMovement->Activate(true); ProjectileMovement->SetComponentTickEnabled(true);
    Trail->SetVariableLinearColor(TEXT("User.Color"),Color);
    Trail->SetVariableLinearColor(TEXT("User.EnergyColor"),Color);
    if (Trail->GetAsset()) Trail->Activate(true);
    if(ProjectileType!=EProjectileType::ContinuousBeam && !bBloodSpell && Branches[0]->GetAsset())
    {
        Branches[0]->SetVariableVec3(TEXT("User.BeamStart"),Origin);
        Branches[0]->SetVariableVec3(TEXT("User.BeamEnd"),Origin-Direction.GetSafeNormal()*2.f);
        Branches[0]->Activate(true);
        if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->ExplosionDirector && SpellStyle==ENeonSpellStyle::Prism)
            Game->ExplosionDirector->TrackFireball(this);
    }
}
void AMortisProjectileBase::Deactivate()
{
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->ExplosionDirector)
        Game->ExplosionDirector->TrackFireball(this,false);
    for(UNiagaraComponent* FX:Branches) FX->DeactivateImmediate();
    bActive=false; SetActorHiddenInGame(true); SetActorTickEnabled(false);
    Visual->SetRelativeLocation(FVector::ZeroVector);Visual->SetRelativeRotation(FRotator::ZeroRotator);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProjectileMovement->StopMovementImmediately(); ProjectileMovement->Deactivate();
    ProjectileMovement->SetComponentTickEnabled(false); Trail->DeactivateImmediate(); HitLives.Reset();
}
void AMortisProjectileBase::Stopped(const FHitResult& Hit)
{
    if (!bActive) return;
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->AudioDirector) Game->AudioDirector->PlayImpact(Hit.ImpactPoint);
    PlayCinematicImpactVFX(Hit.ImpactPoint,Hit.ImpactNormal);
    if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector) G->SpellImpactDirector->PlayImpact(SpellStyle,Hit.ImpactPoint);
    FireImpact(Hit.ImpactPoint);
    Deactivate();
}
void AMortisProjectileBase::OnProjectileImpact(const FHitResult& Hit)
{
    auto* Enemy=Cast<AMVPEnemy>(Hit.GetActor());
    if (!IsValid(Enemy) || !Enemy->IsPoolActive()) return;
    if (const uint32* Prior=HitLives.Find(Enemy); Prior && *Prior==Enemy->Generation) return;
    HitLives.Add(Enemy,Enemy->Generation);
    const FVector Impact=Enemy->GetActorLocation();
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->AudioDirector) Game->AudioDirector->PlayImpact(Impact);
    const float HealthBefore=Enemy->Health;
    Enemy->TakeSpellDamage(ShotDamage,ProjectileMovement->Velocity,ShotModifiers.ProgramID);
    if(Enemy->IsPoolActive())
    {
        if(auto* Brain=Enemy->FindComponentByClass<UForbiddenEnemyBrain>())
        {
            if(ShotModifiers.ProgramID=="KineticCards" && ShotModifiers.Evolution==1) Brain->AddMark(ShotDamage);
            if(ShotModifiers.ProgramID=="HaloNova" && ShotModifiers.Evolution==2) Brain->ApplySlow(0.25f,1.5f);
        }
    }
    if(ShotModifiers.ProgramID=="PrismBurst" && ShotModifiers.Evolution==1)
        if(auto* Fields=GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>()) Fields->QueueField("PrismBurst",ENeonSpellStyle::Prism,Impact,ShotDamage*0.18f,140.f,0.25f,3,0.35f);
    if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector)
        G->SpellImpactDirector->ConfirmDamage(SpellStyle,Enemy,FMath::Max(0.f,HealthBefore-FMath::Max(0.f,Enemy->Health)));
    PlayCinematicImpactVFX(Impact,GetActorForwardVector());
    FireImpact(Impact);
    if (bBloodSpell) if (auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->BloodDirector) Game->BloodDirector->Splatter(Impact);
    if (ShotModifiers.ExplodeOnImpact)
    {
        if (ProjectileType!=EProjectileType::ConcentratedElemental || bBloodSpell)
            if (auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)))
                // Energy projectiles already submitted one compact effect in FireImpact.
                // Do not also allocate a full fluid explosion for charged cards.
                if(bBloodSpell || ProjectileType==EProjectileType::ContinuousBeam || !Game->ExplosionDirector)
                    Game->EmitImpactPulse(Impact,120.f*ShotModifiers.RadiusMultiplier);
        TArray<FOverlapResult> Hits;
        GetWorld()->OverlapMultiByObjectType(Hits,Impact,FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(120.f*ShotModifiers.RadiusMultiplier));
        TSet<AMVPEnemy*> Seen; Seen.Add(Enemy);
        for (const auto& H:Hits)
            if (auto* Other=Cast<AMVPEnemy>(H.GetActor()); Other && Other->IsPoolActive() && !Seen.Contains(Other))
            {
                Seen.Add(Other);FHitResult Wall;
                if(GetWorld()->LineTraceSingleByObjectType(Wall,Impact,Other->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic))) continue;
                const float Before=Other->Health;Other->TakeSpellDamage(ShotDamage*0.6f,FVector::ZeroVector,ShotModifiers.ProgramID,false);
                if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector)
                    G->SpellImpactDirector->ConfirmDamage(SpellStyle,Other,FMath::Max(0.f,Before-FMath::Max(0.f,Other->Health)));
            }
    }
    if(ShotModifiers.ProgramID=="KineticCards" && ShotModifiers.Evolution==2 && PiercingCount>0)
    {
        const auto* Player=Cast<AMVPPlayer>(GetOwner());AMVPEnemy* Next=nullptr;float Best=FMath::Square(350.f);
        if(Player && Player->Deck && Player->Deck->EnemySpawner)
            for(APooledEnemy* A:Player->Deck->EnemySpawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(A);E && E->IsPoolActive())
            {
                const auto* Previous=HitLives.Find(E);const float D=FVector::DistSquared(Impact,E->GetActorLocation());FHitResult Wall;
                if((!Previous || *Previous!=E->Generation) && D<Best && !GetWorld()->LineTraceSingleByObjectType(Wall,Impact,E->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic))) {Next=E;Best=D;}
            }
        if(Next) {ProjectileMovement->Velocity=(Next->GetActorLocation()-Impact).GetSafeNormal()*1250.f;SetActorLocation(Impact+ProjectileMovement->Velocity.GetSafeNormal()*25.f);}
        else {Deactivate();return;}
    }
    if (PiercingCount--<=0 && ProjectileType!=EProjectileType::ContinuousBeam)
    {
        if(ShotModifiers.ProgramID=="HaloNova" && ShotModifiers.Evolution==1 && !Returning) {Returning=true;HitLives.Reset();PiercingCount=ShotModifiers.PiercingCount;ShotDamage*=0.7f;}
        else Deactivate();
    }
}
void AMortisProjectileBase::TraceDamage(FVector Start,FVector End)
{
    FHitResult Wall;
    if (GetWorld()->SweepSingleByObjectType(Wall,Start,End,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(12))) End=Wall.Location;
    TArray<FHitResult> Hits;
    if (ProjectileType==EProjectileType::ContinuousBeam) GetWorld()->LineTraceMultiByObjectType(Hits,Start,End,FCollisionObjectQueryParams(ECC_GameTraceChannel1));
    else GetWorld()->SweepMultiByObjectType(Hits,Start,End,FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(12));
    Hits.Sort([](const FHitResult& A,const FHitResult& B){return A.Time<B.Time;});
    for (const auto& Hit:Hits) { OnProjectileImpact(Hit); if (!bActive || PiercingCount<0 || (ShotModifiers.ProgramID=="KineticCards" && ShotModifiers.Evolution==2)) break; }
}
void AMortisProjectileBase::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!bActive) return;
    if ((RemainingLife-=Dt)<=0) { Deactivate(); return; }
    FlightAge+=Dt;
    if(ShotModifiers.ProgramID=="HaloNova" && ShotModifiers.Evolution==1 && GetOwner())
    {
        if(!Returning && FlightAge>0.65f) {Returning=true;HitLives.Reset();PiercingCount=ShotModifiers.PiercingCount;ShotDamage*=0.7f;}
        if(Returning)
        {
            const FVector Delta=GetOwner()->GetActorLocation()-GetActorLocation();
            if(Delta.SizeSquared()<FMath::Square(50.f)) {Deactivate();return;}
            ProjectileMovement->Velocity=Delta.GetSafeNormal()*1150.f;
        }
    }
    if(ProjectileType!=EProjectileType::ContinuousBeam && !bBloodSpell)
    {
        const FVector Head=GetActorLocation();
        const float TailLength=SpellStyle==ENeonSpellStyle::Kinetic ? 210.f : SpellStyle==ENeonSpellStyle::Pulse ? 180.f : SpellStyle==ENeonSpellStyle::Photon ? 360.f : SpellStyle==ENeonSpellStyle::Halo ? 155.f : 280.f;
        if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector)
        {
            const float Zoom=G->SpellImpactDirector->ReadabilityScale;
            if(SpellStyle!=ENeonSpellStyle::Prism) Visual->SetRelativeScale3D(BaseVisualScale*Zoom);
            if((SparkTrailAge+=Dt)>=0.075f) {G->SpellImpactDirector->PlayFlightTrail(SpellStyle,Head);SparkTrailAge=0;}
        }
        const FVector TailPoint=Head-(Head-LaunchOrigin).GetClampedToMaxSize(TailLength);
        Branches[0]->SetVariableVec3(TEXT("User.BeamStart"),Head);
        Branches[0]->SetVariableVec3(TEXT("User.BeamEnd"),TailPoint);
        if(ProjectileType==EProjectileType::PhysicalMesh)
        {
            Visual->SetRelativeRotation(FRotator(18.f*FMath::Sin(FlightAge*19.f),SpinPhase+FlightAge*900.f,14.f*FMath::Sin(FlightAge*23.f)));
            Visual->SetRelativeLocation(FVector(0,0,5.f*FMath::Sin(FlightAge*22.f)));
        }
        else if(SpellStyle==ENeonSpellStyle::Halo)
        {
            Visual->SetRelativeRotation(FRotator(12.f*FMath::Sin(FlightAge*14),SpinPhase+FlightAge*480,0));
            Visual->SetRelativeLocation(FVector(0,0,6.f*FMath::Sin(FlightAge*18)));
        }
        else if(SpellStyle==ENeonSpellStyle::Pulse) Visual->SetRelativeScale3D(Visual->GetRelativeScale3D()*(1.f+0.07f*FMath::Sin(FlightAge*35)));
        else if(SpellStyle==ENeonSpellStyle::Photon) Visual->SetRelativeRotation(FRotator(0,0,8.f*FMath::Sin(FlightAge*24)));
    }
    if(ProjectileType==EProjectileType::ContinuousBeam && (BranchUpdate-=Dt)<=0.f) {UpdateBranches();BranchUpdate=0.05f;}
    if (ProjectileType!=EProjectileType::ContinuousBeam)
        TraceDamage(GetActorLocation(),GetActorLocation()+ProjectileMovement->Velocity*Dt);
}
void AMortisProjectileBase::FireContinuousBeam(FVector Origin,FVector Target,float Damage,const FSpellModifiers& Snapshot)
{
    if (Origin.ContainsNaN() || Target.ContainsNaN() || !FMath::IsFinite(Damage) || Damage<=0 || FVector::DistSquared(Origin,Target)<1) return;
    Launch(Origin,Target-Origin,Damage,Snapshot,FLinearColor(0.15f,0.5f,1.f));
    ProjectileType=EProjectileType::ContinuousBeam;
    ProjectileMovement->StopMovementImmediately(); ProjectileMovement->SetComponentTickEnabled(false);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByObjectType(Hit,Origin,Target,FCollisionObjectQueryParams(ECC_WorldStatic))) Target=Hit.ImpactPoint;
    TArray<FHitResult> Targets;
    GetWorld()->LineTraceMultiByObjectType(Targets,Origin,Target,FCollisionObjectQueryParams(ECC_GameTraceChannel1));
    Targets.Sort([](const FHitResult& A,const FHitResult& B){return A.Time<B.Time;});
    int32 Through=PiercingCount;
    for(const auto& Candidate:Targets)
        if(const auto* Enemy=Cast<AMVPEnemy>(Candidate.GetActor()); Enemy && Enemy->IsPoolActive() && Through--<=0)
        { Target=Candidate.ImpactPoint+(Target-Origin).GetSafeNormal()*2.f;break; }
    TraceDamage(Origin,Target);
    Trail->SetVariableVec3(TEXT("User.BeamStart"),Origin);
    Trail->SetVariableVec3(TEXT("User.BeamEnd"),Target);
    BeamEnd=Target;UpdateBranches();
    RemainingLife=0.22f;
}

void AMortisProjectileBase::UpdateBranches()
{
    const FVector Axis=BeamEnd-LaunchOrigin;
    const FVector Side=FVector::CrossProduct(Axis.GetSafeNormal(),FVector::UpVector).GetSafeNormal();
    for(int32 I=0;I<Branches.Num();++I)
    {
        const FVector Start=LaunchOrigin+Axis*FMath::FRandRange(0.12f,0.85f);
        FVector End=Start+Axis*FMath::FRandRange(0.07f,0.2f)+Side*(I%2 ? -1.f : 1.f)*FMath::FRandRange(50.f,155.f)+FVector(0,0,FMath::FRandRange(-20.f,45.f));
        FHitResult Hit;
        if(GetWorld()->LineTraceSingleByObjectType(Hit,Start,End,FCollisionObjectQueryParams(ECC_WorldStatic))) End=Hit.ImpactPoint;
        Branches[I]->SetVariableVec3(TEXT("User.BeamStart"),Start);
        Branches[I]->SetVariableVec3(TEXT("User.BeamEnd"),End);
        if(!Branches[I]->IsActive()) Branches[I]->Activate(true);
    }
}

void AMortisProjectileBase::FireImpact(FVector Position)
{
    if(ProjectileType==EProjectileType::ContinuousBeam || bBloodSpell) return;
    if(ProjectileType==EProjectileType::PhysicalMesh)
    {
        if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->CardVFXDirector) Game->CardVFXDirector->PlayImpact(Position);
        return;
    }
    if(SpellStyle!=ENeonSpellStyle::Prism)
    {
        return;
    }
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->ExplosionDirector)
        Game->ExplosionDirector->TriggerExplosion(Position,100.f,true);
}

