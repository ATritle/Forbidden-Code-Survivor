#include "MVPGame.h"
#include "MortisTerrain.h"
#include "BloodSurfaceDirector.h"
#include "CinematicExplosionDirector.h"
#include "EnemySpawner.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "NiagaraSystem.h"
#include "NecromancyVisualComponent.h"
#include "Engine/World.h"

// Real-world smoke assertions, invoked only by -MVPSmoke.
const TCHAR* ValidateTerrainSpells(AMVPArenaGameMode* Game)
{
    auto* Player=Game->GetArenaPlayer();
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(Game->GetWorld());
    if(!Nav) return TEXT("Missing arena navigation");
    auto* Path=Nav->FindPathToLocationSynchronously(Game,FVector(300,1400,0),FVector(300,2200,0));
    if(!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num()<3) return TEXT("Nav path did not route around wall");
    const FVector Saved=Player->GetActorLocation();
    Player->SetActorLocation(FVector(350,0,55));Player->Movement->StopMovementImmediately();
    for(int32 I=0;I<250;++I)
    {
        Player->AddMovementInput(FVector::ForwardVector);
        Player->Movement->TickComponent(1.f/60.f,LEVELTICK_All,nullptr);
        Player->UpdateGroundPhysics(1.f/60.f);
    }
    const FVector Raised=Player->GetActorLocation();
    UE_LOG(LogTemp,Display,TEXT("TERRAIN_RAMP_RESULT %s"),*Raised.ToString());
    Player->Movement->StopMovementImmediately();Player->SetActorLocation(Saved);
    if(Raised.X<1800 || Raised.Z<300) return TEXT("Player could not climb ramp onto platform");
    Player->SetActorLocation(FVector(0,0,700));
    const int32 Landings=Player->LandingCount;
    Player->UpdateGroundPhysics(0.1f);
    if(!Player->bFalling || Player->GetActorLocation().Z<650 || Player->VerticalSpeed>=0) return TEXT("Gravity did not accelerate a fall");
    for(int32 I=0;I<120;++I) Player->UpdateGroundPhysics(1.f/60.f);
    if(Player->bFalling || Player->LandingCount!=Landings+1 || FMath::Abs(Player->GetActorLocation().Z-40)>3) return TEXT("Weighted landing did not settle on courtyard");
    Player->SetActorLocation(Saved);
    if(AMVPEnemy::SelectTimedTier(74,0)!=0 || AMVPEnemy::SelectTimedTier(75,0)!=1 || AMVPEnemy::SelectTimedTier(180,0)!=2 || AMVPEnemy::SelectTimedTier(360,0)!=3) return TEXT("Enemy tier timing failed");
    for(const FVector Goal : {FVector(0,4200,235),FVector(-4300,1600,570),FVector(3800,-3700,285)})
    {
        auto* Route=Nav->FindPathToLocationSynchronously(Game,FVector(0,0,0),Goal);
        if(!Route || !Route->IsValid() || Route->IsPartial()) return TEXT("New elevated structure is unreachable by navigation");
    }
    auto* Enemy=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(FVector(300,2200,40))));
    if(!Enemy) return TEXT("No projectile validation target");
    for(int32 Tier=1;Tier<=3;++Tier)
    {
        Enemy->ApplyEnemyTier(Tier);
        auto* Art=Enemy->FindComponentByClass<UNecromancyVisualComponent>();
        if(!Art || !Art->IsArtReady() || Enemy->Health!=Enemy->MaxHealth) return TEXT("Tier model/animation or health reset failed");
    }
    Enemy->ApplyEnemyTier(0);
    Enemy->SetActorTickEnabled(false);Enemy->Health=1000;
    auto* Shot=Game->GetWorld()->SpawnActor<AMortisProjectileBase>();
    auto* Plasma=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_Fireball.NS_Fireball"));
    if(!Plasma) {Shot->Destroy();Enemy->ReturnToPool();return TEXT("Missing energy Niagara core");}
    Shot->SetupProjectileVisuals(EProjectileType::ConcentratedElemental,nullptr,Plasma);
    Shot->Launch(FVector(300,1400,40),FVector::RightVector,100,FSpellModifiers(),FLinearColor(1.f,0.02f,0.5f));
    Shot->Tick(0.7f);
    const bool Blocked=Enemy->Health==1000;
    Shot->Deactivate();Shot->Launch(FVector(300,2000,40),FVector::RightVector,100,FSpellModifiers(),FLinearColor(1.f,0.02f,0.5f));
    Shot->Tick(0.2f);
    const bool EnergyHit=Enemy->Health==900;
    Shot->Deactivate();Shot->bBloodSpell=false;
    Shot->SetupProjectileVisuals(EProjectileType::ConcentratedElemental,nullptr,nullptr);
    if(Game->ExplosionDirector)
    {
        Game->ExplosionDirector->Tick(5.f);
        const int32 Accepted=Game->ExplosionDirector->Accepted, Fluids=Game->ExplosionDirector->FluidAccepted;
        Shot->Launch(FVector(300,2000,40),FVector::RightVector,10,FSpellModifiers(),FLinearColor::White);
        Shot->Tick(0.3f);
        if(Game->ExplosionDirector->Accepted!=Accepted+1 || Game->ExplosionDirector->FluidAccepted!=Fluids)
            return TEXT("Compact fire impact failed or allocated a fluid grid");
        UE_LOG(LogTemp,Display,TEXT("COMPACT_FIRE_IMPACT_PASS"));
        Game->ExplosionDirector->Tick(5.f);
        const int32 CardAccepted=Game->ExplosionDirector->Accepted,CardFluids=Game->ExplosionDirector->FluidAccepted;
        Shot->Deactivate();Shot->SetupProjectileVisuals(EProjectileType::PhysicalMesh,nullptr,nullptr);
        FSpellModifiers Charged;Charged.ExplodeOnImpact=true;
        Shot->Launch(FVector(300,2000,40),FVector::RightVector,10,Charged,FLinearColor(1.f,0.02f,0.5f));
        Shot->Tick(0.2f);
        if(Game->ExplosionDirector->Accepted!=CardAccepted+1 || Game->ExplosionDirector->FluidAccepted!=CardFluids)
            return TEXT("Charged card submitted duplicate effects or allocated a full fluid grid");
        UE_LOG(LogTemp,Display,TEXT("COMPACT_KINETIC_IMPACT_PASS"));
    }
    Shot->Destroy();Enemy->ReturnToPool();
    if(!Blocked || !EnergyHit) return TEXT("Energy projectile wall blocking or damage failed");
    if(Game->BloodDirector) return TEXT("Energy run created legacy blood renderer");
    UE_LOG(LogTemp,Display,TEXT("TERRAIN_SPELL_VALIDATION_PASS: ramp, weighted landing, wall path, energy projectile damage/occlusion, compact pink collision FX"));
    return nullptr;
}
