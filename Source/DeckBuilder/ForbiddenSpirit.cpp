#include "ForbiddenSpirit.h"
#include "MVPGame.h"
#include "NecromancyVisualComponent.h"
#include "MortisTerrain.h"
#include "NeonArsenal.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "MortisTestingDirector.h"
#include "Components/SphereComponent.h"

AForbiddenSpirit::AForbiddenSpirit()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=0.05f;
    auto* Body=CreateDefaultSubobject<USphereComponent>(TEXT("Body"));SetRootComponent(Body);Body->InitSphereRadius(24);
    Body->SetCollisionObjectType(ECC_WorldDynamic);Body->SetCollisionResponseToAllChannels(ECR_Ignore);Body->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Body->SetCanEverAffectNavigation(false);
    Art=CreateDefaultSubobject<UNecromancyVisualComponent>(TEXT("DigitalSpirit"));Art->SetupAttachment(Body);Art->SetCastShadow(false);
    Terrain=CreateDefaultSubobject<UMortisTerrain>(TEXT("Terrain"));SetActorHiddenInGame(true);
}
void AForbiddenSpirit::BeginPlay() {Super::BeginPlay();Art->LoadCharacter(TEXT("DigitalSpirit"),155.f);Art->SetDormant(true);}
void AForbiddenSpirit::ActivateSupport(AMVPPlayer* Player,float Seconds)
{
    Rider=Player;Remaining=Seconds;Pedestrian=Decoy=false;AttackCooldown=0;SetActorLocation(Player->GetActorLocation()+FVector(80,80,0));Terrain->ResetPath();SetActorHiddenInGame(false);Art->SetDormant(false);
}
void AForbiddenSpirit::ActivateDecoy(FVector Position,float Seconds)
{
    Remaining=Seconds;Pedestrian=false;Decoy=true;SetActorLocation(Position);SetActorHiddenInGame(false);Art->SetDormant(false);Art->UpdateLocomotion(FVector::ForwardVector,0);
}
void AForbiddenSpirit::SetPedestrianRoute(FVector A,FVector B)
{
    RouteA=A;RouteB=B;Pedestrian=true;Decoy=false;SetActorLocation(A);Terrain->ResetPath();SetActorHiddenInGame(false);Art->SetDormant(false);
}
void AForbiddenSpirit::Tick(float Dt)
{
    Super::Tick(Dt);auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());if(!G || (G->TestingDirector && G->TestingDirector->IsActive())) return;
    if(Pedestrian)
    {
        const FVector Destination=RouteForward ? RouteB : RouteA;const FVector Direction=Destination-GetActorLocation();
        if(Direction.SizeSquared2D()<FMath::Square(90.f)) RouteForward=!RouteForward;
        Terrain->MoveTowards(Destination,100.f,Dt);Art->UpdateLocomotion(Direction,100.f);return;
    }
    if(Remaining<=0 || G->bRunEnded) {SetActorHiddenInGame(true);Art->SetDormant(true);return;}
    Remaining-=Dt;if(Decoy) return;
    if(!Rider.IsValid()) return;
    const FVector Destination=Rider->GetActorLocation()+FVector(95,65,0);const FVector Direction=Destination-GetActorLocation();
    if(Direction.SizeSquared2D()>FMath::Square(100.f)) Terrain->MoveTowards(Destination,510.f,Dt);
    Art->UpdateLocomotion(Direction,Direction.SizeSquared2D()>FMath::Square(100.f) ? 300.f : 0.f);
    if((AttackCooldown-=Dt)<=0 && G->ArsenalDirector && Rider->Deck && Rider->Deck->EnemySpawner)
    {
        if(auto* E=Cast<AMVPEnemy>(Rider->Deck->EnemySpawner->FindNearestEnemy(GetActorLocation(),900,true)))
        {
            FSpellModifiers Snapshot;Snapshot.ProgramID="CapturedRoutine";
            const bool Disruption=G->CapturedRoutine==3 || G->CapturedRoutine==6 || G->CapturedRoutine==8;
            Snapshot.Evolution=Disruption ? 1 : 0;
            G->ArsenalDirector->FirePattern(Disruption ? 1 : 0,Rider.Get(),E,Disruption ? 8.f : 12.f,Snapshot);AttackCooldown=Disruption ? 1.8f : 1.4f;
        }
    }
}
