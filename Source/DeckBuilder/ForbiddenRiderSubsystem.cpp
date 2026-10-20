#include "ForbiddenRiderSubsystem.h"
#include "Engine/StaticMesh.h"
#include "ForbiddenRiderRoster.h"
#include "MVPGame.h"
#include "ForbiddenSpirit.h"
#include "ForbiddenCombatSubsystem.h"
#include "NecromancyVisualComponent.h"
#include "MortisTerrain.h"
#include "EnemySpawner.h"
#include "CyberEnemyRoster.h"
#include "CodexDeckComponent.h"
#include "NeonSpellImpactDirector.h"
#include "NeonArsenal.h"
#include "NeonChallenge.h"
#include "MortisProjectileBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "MortisTestingDirector.h"

namespace
{
    bool InPractice(const UObject* Context)
    {const auto* G=Context->GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();return G && G->TestingDirector && G->TestingDirector->IsActive();}
}

void UForbiddenRiderSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    Decoy=World.SpawnActor<AForbiddenSpirit>();Familiar=World.SpawnActor<AForbiddenSpirit>();Possessed=World.SpawnActor<AForbiddenSpirit>();
    if(Familiar) Familiar->Art->LoadCharacter(TEXT("DigitalSpirit"),75.f);
    for(int32 I=0;I<8;++I)
    {auto* P=World.SpawnActor<AMortisProjectileBase>();P->SetSpellStyle(ENeonSpellStyle::Halo);P->SetupProjectileVisuals(EProjectileType::ConcentratedElemental,nullptr,nullptr);Pulses.Add(P);PulseTargets.Add(nullptr);PulseLives.Add(0);}
}
bool UForbiddenRiderSubsystem::IsSummon(FName Program)
{return Program=="BoundDaemon" || Program=="PossessedAlly" || Program=="CapturedRoutine" || Program=="RaiseSkeletons" || Program=="SkeletonMinion";}
void UForbiddenRiderSubsystem::EndRun()
{
    if(ShieldVisual) ShieldVisual->SetVisibility(false);
    for(AForbiddenSpirit* A:{Decoy.Get(),Familiar.Get(),Possessed.Get()}) if(A) {A->SetActorHiddenInGame(true);A->Art->SetDormant(true);}
    for(AMortisProjectileBase* P:Pulses) if(P) P->Deactivate();
    Started=false;
}
void UForbiddenRiderSubsystem::BeginRun(AMVPPlayer* Player)
{
    if(!Player || Started) return;Started=true;Rider=Player;ID=ForbiddenRiders::Selected(Player);
    Shield=0;Essence=Hits=Casts=SummonHits=0;Marked.Reset();Candidate.Reset();
    MarkRemaining=HitWindow=DecoyCooldown=DecoyRemaining=RepulsorCooldown=PossessionRemaining=PossessionCooldown=0;
    if(ID=="Vex") {Player->MaxHealth*=.8f;Player->Health=Player->MaxHealth;}
    if(ID=="Rook") Player->Movement->MaxSpeed*=.9f;
    auto GhostMaterial=[&](AForbiddenSpirit* Actor,FLinearColor Color)
    {
        if(!Actor) return;
        auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Characters/RiderExtras/M_RiderGhost.M_RiderGhost"));
        if(!Base) return;auto* M=UMaterialInstanceDynamic::Create(Base,Actor);M->SetVectorParameterValue("Tint",Color);
        for(int32 I=0;I<Actor->Art->GetNumMaterials();++I) Actor->Art->SetMaterial(I,M);
    };
    if(ID=="Vex" && Decoy) {Decoy->Art->LoadCharacter(TEXT("Vex"),170.f);GhostMaterial(Decoy,FLinearColor(.02f,1,.65f));}
    if(ID=="Mira" && Familiar) {Familiar->ActivateDecoy(Player->GetActorLocation(),1000000.f);Familiar->Art->LoadCharacter(TEXT("DigitalSpirit"),75.f);GhostMaterial(Familiar,FLinearColor(.85f,.06f,1));}
    if(ID=="Rook" && !ShieldVisual)
    {
        ShieldVisual=NewObject<UStaticMeshComponent>(Player);Player->AddInstanceComponent(ShieldVisual);ShieldVisual->SetupAttachment(Player->GetRootComponent());
        ShieldVisual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        ShieldVisual->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Characters/RiderExtras/M_RiderShield.M_RiderShield")));
        ShieldVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);ShieldVisual->SetCanEverAffectNavigation(false);ShieldVisual->SetCastShadow(false);
        ShieldVisual->SetRelativeLocation(FVector(0,0,40));ShieldVisual->SetRelativeScale3D(FVector(1.2f,1.2f,1.85f));ShieldVisual->SetVisibility(false);ShieldVisual->RegisterComponent();
    }
}
void UForbiddenRiderSubsystem::OnEssence(int32 Points)
{
    if(!Started || InPractice(this) || ID!="Rook" || !Rider.IsValid() || Points<=0) return;
    Essence+=Points;const int32 Charges=Essence/25;Essence%=25;
    Shield=FMath::Min(Rider->MaxHealth*.3f,Shield+Charges*Rider->MaxHealth*.15f);
}
float UForbiddenRiderSubsystem::AbsorbContact(float Damage)
{
    if(!Started || InPractice(this) || ID!="Rook" || Damage<=0) return Damage;
    const float Taken=FMath::Min(Shield,Damage);Shield-=Taken;return Damage-Taken;
}
bool UForbiddenRiderSubsystem::IsLocked(const AMVPEnemy* Enemy) const
{return MarkRemaining>0 && Marked.Get()==Enemy && Enemy && Enemy->IsPoolActive() && Enemy->Generation==MarkedLife;}
float UForbiddenRiderSubsystem::ModifyDamage(AMVPEnemy* Enemy,float Damage,FName Program,bool Direct)
{
    // Echo power already comes from confirmed post-rider damage on the source hit.
    if(!Started || InPractice(this) || IsSummon(Program) || Program=="TalismanGuard" || Program=="RiderRepulsor" || Program=="UnstableEcho") return Damage;
    if(ID=="Mira") Damage*=.85f;
    if(ID=="Zero" && IsLocked(Enemy)) Damage*=1.25f;
    return Damage;
}
void UForbiddenRiderSubsystem::OnDamageConfirmed(AMVPEnemy* Enemy,FName Program,bool Direct,float Actual)
{
    if(!Started || InPractice(this) || !Enemy || Actual<=0) return;
    if(ID=="Mira" && Direct && IsSummon(Program) && Program!="PossessedAlly")
    {SummonHits=FMath::Min(20,SummonHits+1);++ConfirmedSummonHits;}
    if(ID!="Zero" || !Direct || IsSummon(Program) || Program.IsNone() || Program=="TalismanGuard" || Program=="RiderRepulsor") return;
    if(Marked.IsValid() && MarkRemaining>0 && Marked->IsPoolActive() && Marked->Generation==MarkedLife) return;
    if(Candidate.Get()!=Enemy || CandidateLife!=Enemy->Generation || HitWindow<=0) {Candidate=Enemy;CandidateLife=Enemy->Generation;Hits=0;HitWindow=2;}
    if(++Hits>=3 && Enemy->IsPoolActive() && Enemy->Health>0) {Marked=Enemy;MarkedLife=Enemy->Generation;MarkRemaining=4;Candidate.Reset();Hits=0;}
}
FSpellModifiers UForbiddenRiderSubsystem::ModifySnapshot(const FSpellModifiers& Snapshot,bool Chain) const
{
    FSpellModifiers Result=Snapshot;if(!Started || InPractice(this)) return Result;
    if(ID=="Vex" && Chain) {Result.ProjectileCount=FMath::Min(12,Result.ProjectileCount+1);Result.RadiusMultiplier*=1.15f;}
    if(ID=="Zero" && !Chain) Result.RadiusMultiplier*=.8f;
    return Result;
}
int32 UForbiddenRiderSubsystem::ProjectileCast()
{return Started && !InPractice(this) && ID=="Zero" && ++Casts%3==0 ? 2 : 0;}
bool UForbiddenRiderSubsystem::DecoyGoal(const AMVPEnemy* Enemy,FVector& Goal) const
{
    if(!Started || InPractice(this) || ID!="Vex" || !Enemy || Enemy->EnemyTier!=0 || DecoyRemaining<=0 || !Decoy) return false;
    if(FVector::DistSquared(Enemy->GetActorLocation(),Decoy->GetActorLocation())>FMath::Square(600.f)) return false;
    Goal=Decoy->GetActorLocation();return true;
}
void UForbiddenRiderSubsystem::OnBoost()
{
    if(!Started || InPractice(this) || !Rider.IsValid()) return;
    if(ID=="Vex" && Decoy && DecoyCooldown<=0)
    {Decoy->ActivateDecoy(Rider->GetActorLocation(),1.5f);DecoyCooldown=6;DecoyRemaining=1.5f;++Decoys;}
    if(ID=="Rook" && RepulsorCooldown<=0)
    {
        RepulsorCooldown=6;++Repulsors;
        if(auto* Fields=GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>()) Fields->QueueField("RiderRepulsor",ENeonSpellStyle::ShatterSigil,Rider->GetActorLocation(),35,350,0,1,.1f);
        if(auto* G=GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();G && G->Spawner)
        {
            const TArray<TObjectPtr<APooledEnemy>> Enemies=G->Spawner->GetActiveEnemies().Array();
            for(APooledEnemy* A:Enemies) if(auto* E=Cast<AMVPEnemy>(A);E && E->IsPoolActive() && E->EnemyTier==0 && FVector::DistSquared(E->GetActorLocation(),Rider->GetActorLocation())<FMath::Square(350.f) && UMortisTerrain::ClearSight(Rider.Get(),E->GetActorLocation()))
            {E->StaggerRemaining=FMath::Max(E->StaggerRemaining,.35f);E->AddActorWorldOffset((E->GetActorLocation()-Rider->GetActorLocation()).GetSafeNormal2D()*85.f,true);}
        }
    }
}
void UForbiddenRiderSubsystem::TryPossession()
{
    auto* G=GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();if(!G || !G->Spawner || !Rider.IsValid() || !Possessed) return;
    AMVPEnemy* Best=nullptr;float Distance=FMath::Square(900.f);
    for(APooledEnemy* A:G->Spawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(A);E && E->IsPoolActive() && E->Health>0 && !E->bTrainingTarget && E->EnemyTier==0 && E->EnemyArchetype>=0 && E->EnemyArchetype<4)
    {const float D=FVector::DistSquared(E->GetActorLocation(),Rider->GetActorLocation());if(D<Distance && UMortisTerrain::ClearSight(Rider.Get(),E->GetActorLocation())) {Distance=D;Best=E;}}
    if(!Best) return;
    const FVector Position=Best->GetActorLocation();const int32 Archetype=Best->EnemyArchetype;
    if(!Possessed->Art->LoadCharacter(CyberEnemies::Roster()[Archetype].ID,145.f)) return;
    // Retire the hostile without a death callback, XP, kill or objective credit.
    Best->ReturnToPool();Possessed->ActivateDecoy(Position,8.f);PossessionRemaining=8;PossessionCooldown=10;PossessedCooldown=0;
    if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Characters/RiderExtras/M_RiderGhost.M_RiderGhost"))) for(int32 I=0;I<Possessed->Art->GetNumMaterials();++I) Possessed->Art->SetMaterial(I,M);
    SummonHits-=20;++Possessions;
}
void UForbiddenRiderSubsystem::CompanionAttack(AForbiddenSpirit* Actor,float Damage,FName Program)
{
    if(!Actor || !Rider.IsValid() || !Rider->Deck || !Rider->Deck->EnemySpawner) return;
    auto* E=Cast<AMVPEnemy>(Rider->Deck->EnemySpawner->FindNearestEnemy(Actor->GetActorLocation(),850,true));
    if(!E || !UMortisTerrain::ClearSight(Actor,E->GetActorLocation())) return;
    for(int32 I=0;I<Pulses.Num();++I) if(Pulses[I] && !Pulses[I]->bActive)
    {
        auto* P=Pulses[I].Get();FSpellModifiers Snapshot;Snapshot.ProgramID=Program;Snapshot.ProjectileCount=1;Snapshot.PiercingCount=0;
        const FVector Start=Actor->GetActorLocation()+FVector(0,0,25);P->SetOwner(Rider.Get());
        P->Launch(Start,(E->GetCombatVFXLocation()-Start).GetSafeNormal(),Damage,Snapshot,FLinearColor(.85f,.06f,1));
        P->ProjectileMovement->bIsHomingProjectile=true;P->ProjectileMovement->HomingTargetComponent=E->GetRootComponent();P->ProjectileMovement->HomingAccelerationMagnitude=7000;
        PulseTargets[I]=E;PulseLives[I]=E->Generation;break;
    }
}
void UForbiddenRiderSubsystem::Tick(float Dt)
{
    if(!Started || InPractice(this) || !Rider.IsValid()) return;
    auto* G=GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();
    if(!G || G->bInCodex || G->bRunEnded || G->bChoosingCard || UGameplayStatics::IsGamePaused(this))
    {if(G && G->bRunEnded) {if(Familiar) Familiar->SetActorHiddenInGame(true);if(Possessed) Possessed->SetActorHiddenInGame(true);}return;}
    MarkRemaining=FMath::Max(0.f,MarkRemaining-Dt);HitWindow=FMath::Max(0.f,HitWindow-Dt);
    if(ShieldVisual) ShieldVisual->SetVisibility(ID=="Rook" && Shield>0 && Rider->Health>0);
    if(ID=="Zero" && IsLocked(Marked.Get()) && (MarkVisualAge-=Dt)<=0)
    {
        if(G->SpellImpactDirector) for(int32 I=0;I<3;++I)
        {const float A=GetWorld()->GetTimeSeconds()*2+I*2*PI/3;G->SpellImpactDirector->PlayFlightTrail(ENeonSpellStyle::Photon,Marked->GetCombatVFXLocation()+FVector(FMath::Cos(A)*24,FMath::Sin(A)*24,20));}
        MarkVisualAge=.2f;
    }
    DecoyCooldown=FMath::Max(0.f,DecoyCooldown-Dt);DecoyRemaining=FMath::Max(0.f,DecoyRemaining-Dt);RepulsorCooldown=FMath::Max(0.f,RepulsorCooldown-Dt);
    PossessionCooldown=FMath::Max(0.f,PossessionCooldown-Dt);PossessionRemaining=FMath::Max(0.f,PossessionRemaining-Dt);
    for(int32 I=0;I<Pulses.Num();++I) if(Pulses[I] && Pulses[I]->bActive)
        if(!PulseTargets[I].IsValid() || !PulseTargets[I]->IsPoolActive() || PulseTargets[I]->Generation!=PulseLives[I]) {Pulses[I]->ProjectileMovement->bIsHomingProjectile=false;Pulses[I]->ProjectileMovement->HomingTargetComponent.Reset();}
    if(ID=="Mira" && Familiar)
    {
        const float Phase=GetWorld()->GetTimeSeconds();const FVector Goal=Rider->GetActorLocation()+FVector(FMath::Cos(Phase)*95,FMath::Sin(Phase)*95,30);
        Familiar->SetActorLocation(FMath::VInterpTo(Familiar->GetActorLocation(),Goal,Dt,8));
        Familiar->Art->UpdateLocomotion(Goal-Familiar->GetActorLocation(),100.f);
        if((FamiliarCooldown-=Dt)<=0) {CompanionAttack(Familiar,12+2*FMath::Max(0,G->Level-1),"BoundDaemon");FamiliarCooldown=1.2f;}
        if(SummonHits>=20 && PossessionCooldown<=0 && PossessionRemaining<=0) TryPossession();
    }
    if(PossessionRemaining>0 && Possessed && Rider->Deck && Rider->Deck->EnemySpawner)
    {
        auto* Target=Rider->Deck->EnemySpawner->FindNearestEnemy(Possessed->GetActorLocation(),650,true);
        const FVector Goal=Target ? Target->GetActorLocation() : Rider->GetActorLocation()+FVector(-80,-80,0);
        if(auto* Terrain=Possessed->FindComponentByClass<UMortisTerrain>()) Terrain->MoveTowards(Goal,420.f,Dt);
        Possessed->Art->UpdateLocomotion(Goal-Possessed->GetActorLocation(),300);
        if((PossessedCooldown-=Dt)<=0) {CompanionAttack(Possessed,16,"PossessedAlly");PossessedCooldown=1.4f;}
    }
}
