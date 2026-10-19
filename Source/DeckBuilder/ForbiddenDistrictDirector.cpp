#include "ForbiddenDistrictDirector.h"
#include "MVPGame.h"
#include "NeonChallenge.h"
#include "MortisTestingDirector.h"
#include "EnemySpawner.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Camera/CameraComponent.h"

AForbiddenDistrictDirector::AForbiddenDistrictDirector()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void AForbiddenDistrictDirector::BeginPlay()
{
    Super::BeginPlay();Player=Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(this,0));
    Objectives={
        {EForbiddenObjective::Relay,FVector(2410,700,320),TEXT("PROMENADE RELAY")},
        {EForbiddenObjective::Relay,FVector(-3150,1600,560),TEXT("MARKET RELAY")},
        {EForbiddenObjective::Shop,FVector(-2150,-900,0),TEXT("STREET CLINIC / 15 SALVAGE")},
        {EForbiddenObjective::Ward,FVector(0,3400,700),TEXT("ROOFTOP WARD / HOLD 20s")},
        {EForbiddenObjective::Cache,FVector(-1100,500,0),TEXT("SPIRIT CACHE")},
        {EForbiddenObjective::Cache,FVector(2100,-1700,0),TEXT("SPIRIT CACHE")},
        {EForbiddenObjective::Cache,FVector(-3500,-1400,0),TEXT("SPIRIT CACHE")},
        {EForbiddenObjective::Cache,FVector(1050,3600,700),TEXT("SHRINE CACHE")},
        {EForbiddenObjective::Subway,FVector(-1200,-4150,0),TEXT("SUBWAY POWER RAIL")},
        {EForbiddenObjective::Subway,FVector(1200,-4150,0),TEXT("SUBWAY POWER RAIL")}
    };
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    CaptureMarker=GetWorld()->SpawnActor<ANeonChallengeEffect>();CaptureMarker->HideEffect();
    for(auto& O:Objectives)
    {
        FNavLocation Ground;
        if(Nav && Nav->ProjectPointToNavigation(O.Position,Ground,FVector(180,180,180))) O.Position=Ground.Location;
        auto* Marker=GetWorld()->SpawnActor<ANeonChallengeEffect>();Markers.Add(Marker);
        Marker->ShowObjective(O.Position, O.Type==EForbiddenObjective::Subway ? 280.f : 115.f,FLinearColor(0.05f,0.8f,1.f),true);
        auto* Label=NewObject<UTextRenderComponent>(this);AddInstanceComponent(Label);Label->SetupAttachment(RootComponent);
        Label->SetWorldLocation(O.Position+FVector(0,0,180));Label->SetHorizontalAlignment(EHTA_Center);Label->SetWorldSize(27);
        Label->SetText(FText::FromString(O.Name));Label->SetTextRenderColor(FColor(70,220,255));Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);Label->RegisterComponent();Labels.Add(Label);
    }
}
int32 AForbiddenDistrictDirector::GetNearbyIndex() const
{
    if(!Player.IsValid()) return INDEX_NONE;
    int32 Best=INDEX_NONE;double Distance=FMath::Square(240.f);
    for(int32 I=0;I<Objectives.Num();++I)
    {
        const auto& O=Objectives[I];if(O.Complete || O.Type==EForbiddenObjective::Subway || O.Type==EForbiddenObjective::Ward) continue;
        const double D=FVector::DistSquared(Player->GetActorLocation(),O.Position+FVector(0,0,55));
        if(D<Distance) {Distance=D;Best=I;}
    }
    return Best;
}
FString AForbiddenDistrictDirector::GetPrompt() const
{
    if(CaptureRemaining>0 && Player.IsValid() && FVector::DistSquared(Player->GetActorLocation(),CapturePosition+FVector(0,0,55))<FMath::Square(240.f)) return TEXT("[E / X] CAPTURE ENEMY CODE / STEAL OR RESEARCH");
    const int32 I=GetNearbyIndex();if(I==INDEX_NONE) return FString();
    if(Hacking==I) return FString::Printf(TEXT("HACKING %.0f%% / STAY CLOSE"),HackProgress/2.5f*100);
    return TEXT("[E / X] ")+Objectives[I].Name;
}
FString AForbiddenDistrictDirector::GetTracker() const
{
    const auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    return FString::Printf(TEXT("RELAYS %d/2  |  CACHES %d/4  |  SALVAGE %d\n%s"),HackedRelays,FoundCaches,G ? G->RunSalvage : 0,
        G && G->bBroadcastBossSpawned ? BroadcastPhase==2 ? TEXT("SOVEREIGN / SWEEPING LANES / KEEP MOVING") : BroadcastPhase==3 ? TEXT("SOVEREIGN / SPIRIT BREACH / COLLAPSE THE SIGNAL") : TEXT("SOVEREIGN / DESTROY SHIELD NODES") : HackedRelays==2 ? TEXT("RELAYS HACKED / EARLY BOSS SIGNAL AT 03:00") : TEXT("EXPLORE THE ELEVATED ROUTES / BOSS ARRIVES BY 06:00"));
}
bool AForbiddenDistrictDirector::Interact()
{
    auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(!G || G->bInCodex || G->bRunEnded || G->bChoosingCard || (G->TestingDirector && G->TestingDirector->IsActive())) return false;
    if(CaptureRemaining>0 && Player.IsValid() && FVector::DistSquared(Player->GetActorLocation(),CapturePosition+FVector(0,0,55))<FMath::Square(240.f))
    {G->OfferCapturedCode(CaptureArchetype);CaptureRemaining=0;CaptureMarker->HideEffect();return true;}
    const int32 I=GetNearbyIndex();if(I==INDEX_NONE) return false;auto& O=Objectives[I];
    if(O.Type==EForbiddenObjective::Relay) {Hacking=I;HackProgress=0;return true;}
    if(O.Type==EForbiddenObjective::Shop)
    {
        if(G->RunSalvage<15 || Player->Health>=Player->MaxHealth) return false;
        G->RunSalvage-=15;Player->Health=FMath::Min(Player->MaxHealth,Player->Health+25.f);
    }
    else {++FoundCaches;G->RunSalvage+=10;G->AddXP(12);}
    O.Complete=true;Markers[I]->HideEffect();Labels[I]->SetVisibility(false);return true;
}
bool AForbiddenDistrictDirector::IsSubwayLive() const
{
    const auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    return G && FMath::Fmod(G->RunSeconds,21.f)>=18.f;
}
void AForbiddenDistrictDirector::OfferCapture(int32 Archetype,FVector Position)
{
    CaptureArchetype=Archetype;CapturePosition=Position-FVector(0,0,55);CaptureRemaining=15.f;
    if(CaptureMarker) CaptureMarker->ShowObjective(CapturePosition,100,FLinearColor(0.5f,0.15f,1.f),true);
}
void AForbiddenDistrictDirector::Tick(float Dt)
{
    Super::Tick(Dt);auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(!G || !Player.IsValid()) return;
    const bool Running=!G->bInCodex && !G->bRunEnded && !(G->TestingDirector && G->TestingDirector->IsActive());
    for(int32 I=0;I<Labels.Num();++I)
    {
        Labels[I]->SetVisibility(Running && !Objectives[I].Complete);
        Labels[I]->SetTextRenderColor(BroadcastPhase>0 ? FColor(255,50,110) : FColor(70,220,255));
        if(Player->FollowCamera) Labels[I]->SetWorldRotation((Player->FollowCamera->GetComponentLocation()-Labels[I]->GetComponentLocation()).Rotation());
        if(!Running) Markers[I]->HideEffect();
    }
    if(!Running || G->bChoosingCard) return;
    CaptureRemaining=FMath::Max(0.f,CaptureRemaining-Dt);if(CaptureRemaining<=0 && CaptureMarker) CaptureMarker->HideEffect();
    if(Hacking!=INDEX_NONE)
    {
        if(GetNearbyIndex()!=Hacking) {Hacking=INDEX_NONE;HackProgress=0;}
        else if((HackProgress+=Dt)>=2.5f)
        {
            Objectives[Hacking].Complete=true;Markers[Hacking]->HideEffect();Labels[Hacking]->SetVisibility(false);
            ++HackedRelays;Hacking=INDEX_NONE;HackProgress=0;G->RunSalvage+=8;
        }
    }
    HazardDamageCooldown=FMath::Max(0.f,HazardDamageCooldown-Dt);
    for(int32 I=0;I<Objectives.Num();++I)
    {
        const auto& O=Objectives[I];if(O.Complete) continue;
        if(O.Type==EForbiddenObjective::Ward)
        {
            if(FVector::DistSquared(Player->GetActorLocation(),O.Position+FVector(0,0,55))<FMath::Square(320.f))
            {
                WardProgress+=Dt;WardAbsence=0;WardReinforcements-=Dt;
                if(WardReinforcements<=0 && G->Spawner)
                {
                    WardReinforcements=5.f;auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
                    for(int32 J=0;J<3;++J)
                    {
                        FNavLocation Point;const FVector P=O.Position+FVector(FMath::FRandRange(-700.f,700.f),-600.f,0);
                        if(Nav && Nav->ProjectPointToNavigation(P,Point,FVector(100,100,100)))
                        {TGuardValue<bool> Guard(G->bSuppressBossSelection,true);if(auto* E=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(Point.Location+FVector(0,0,55)))) E->ApplyEnemyArchetype(G->RunSeconds>=180 ? 5 : 1);}
                    }
                }
            }
            else if((WardAbsence+=Dt)>2.f) WardProgress=FMath::Max(0.f,WardProgress-Dt*2);
            if(WardProgress>=20.f) {Objectives[I].Complete=true;WardComplete=true;G->RunSalvage+=20;Player->Health=FMath::Min(Player->MaxHealth,Player->Health+20);G->AwardChallengeReward(0);}
        }
        if(O.Type==EForbiddenObjective::Subway)
        {
            const float Phase=FMath::Fmod(G->RunSeconds,21.f);
            const bool Warning=Phase>=16.f;
            Markers[I]->ShowObjective(O.Position,280,Warning ? FLinearColor(1,0.08f,0.2f) : FLinearColor(0.05f,0.5f,0.8f),Warning);
            Labels[I]->SetText(FText::FromString(IsSubwayLive() ? TEXT("LIVE POWER RAIL / MOVE!") : Warning ? TEXT("POWER SURGE / 2s WARNING") : O.Name));
            if(IsSubwayLive() && HazardDamageCooldown<=0 && FVector::DistSquared(Player->GetActorLocation(),O.Position+FVector(0,0,55))<FMath::Square(280.f))
            {G->LastDamageCause=TEXT("Subway power surge");Player->ReceiveContactDamage(12);HazardDamageCooldown=0.9f;}
        }
        else Markers[I]->ShowObjective(O.Position,O.Type==EForbiddenObjective::Ward ? 320.f : 115.f,BroadcastPhase>0 ? FLinearColor(1.f,0.03f,0.3f) : FLinearColor(0.05f,0.8f,1.f),true);
    }
    if((G->RunSeconds>=360.f || (HackedRelays>=2 && G->RunSeconds>=180.f)) && !G->bBroadcastBossSpawned && (BossRetry-=Dt)<=0)
    {
        G->bBossRequested=true;BossRetry=0.5f;
        if(G->Spawner)
        {
            // Objective reinforcements can occupy the wave reserve. Retire one ordinary life
            // without kill rewards so boss admission cannot deadlock at the active cap.
            if(G->Spawner->GetActiveEnemyCount()>=G->Spawner->MaxActiveEnemies)
            {
                AMVPEnemy* Retire=nullptr;double Farthest=-1;
                for(APooledEnemy* A:G->Spawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(A);E && E->EnemyTier<3 && !E->Tags.Contains("BroadcastNode") && (!Player->Challenges || E!=Player->Challenges->DuelTarget.Get()))
                {const double D=FVector::DistSquared(E->GetActorLocation(),Player->GetActorLocation());if(D>Farthest) {Farthest=D;Retire=E;}}
                if(Retire) Retire->ReturnToPool();
            }
            G->Spawner->ReservedSlots=0;G->Spawner->SpawnWave();G->Spawner->ReservedSlots=1;
        }
    }
}
