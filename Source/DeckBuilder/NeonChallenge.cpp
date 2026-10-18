#include "NeonChallenge.h"
#include "NeonChallengeWidget.h"
#include "MVPGame.h"
#include "EnemySpawner.h"
#include "MortisTestingDirector.h"
#include "MortisTerrain.h"
#include "NeonSpellImpactDirector.h"
#include "NeonAudioDirector.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"

UNeonChallengeComponent::UNeonChallengeComponent() {PrimaryComponentTick.bCanEverTick=true;}
void UNeonChallengeComponent::BeginPlay()
{
    Super::BeginPlay();Player=Cast<AMVPPlayer>(GetOwner());CreateDefaultDefinitions();
}
void UNeonChallengeComponent::CreateDefaultDefinitions()
{
    if(!Definitions.IsEmpty()) return;
    auto Make=[&](ENeonChallenge Type,const TCHAR* Title,const TCHAR* Reward,const TCHAR* Risk,float Duration,FLinearColor Color)
    {
        auto* D=NewObject<UNeonChallengeDefinition>(this);D->Contract=Type;D->Title=FText::FromString(Title);
        D->Reward=FText::FromString(Reward);D->Risk=FText::FromString(Risk);D->Duration=Duration;D->Accent=Color;Definitions.Add(D);return D;
    };
    auto* Orb=Make(ENeonChallenge::OverloadOrb,TEXT("OVERLOAD ORB"),TEXT("TRIPLE outgoing spell damage for 45 seconds."),
        TEXT("A vulnerable neon orb circles you. Enemy contact ruptures it: lose 50% of MAXIMUM health and the damage bonus. This CAN KILL you at half health or below. Orb arms after 1 second."),45,FLinearColor(1,0.025f,0.45f));
    Orb->DamageMultiplier=3;Orb->PenaltyHealthFraction=0.5f;
    auto* Echo=Make(ENeonChallenge::UnstableEcho,TEXT("UNSTABLE ECHO"),TEXT("Every third spell hit creates a blast dealing TWICE that hit's damage within 180 cm. Active for 40 seconds."),
        TEXT("The marked blast detonates after 1 second. Standing inside costs 20% of MAXIMUM health, even during ordinary hit immunity. Keep moving! At most one echo every 0.5 seconds."),40,FLinearColor(1,0.35f,0.04f));
    Echo->PenaltyHealthFraction=0.2f;
    auto* Hunt=Make(ENeonChallenge::HostileBroadcast,TEXT("HOSTILE BROADCAST"),TEXT("DOUBLE essence dropped by enemies defeated during this 60-second contract."),
        TEXT("Your location is broadcast. TWO ELITE HUNTERS arrive at 15, 30 and 45 seconds. Watch the reinforcement countdown. Hunters remain until defeated; normal enemy limits still apply."),60,FLinearColor(0.05f,0.8f,1));
    Hunt->EssenceMultiplier=2;
    auto* Battery=Make(ENeonChallenge::DeadmanBattery,TEXT("DEADMAN BATTERY"),TEXT("DOUBLE outgoing spell damage for 30 seconds. Every confirmed enemy kill restores 4% of maximum health."),
        TEXT("The battery drains 2% of MAXIMUM health per second, even during hit immunity. The drain CAN KILL you. Keep defeating enemies to stay charged."),30,FLinearColor(0.25f,1,0.35f));
    Battery->DamageMultiplier=2;Battery->HealthDrainPerSecond=0.02f;Battery->HealthRestoredPerKill=0.04f;
    auto* Glass=Make(ENeonChallenge::GlassProtocol,TEXT("GLASS PROTOCOL"),TEXT("75% more outgoing damage and TWO extra penetrations for attacks that support piercing. Active for 45 seconds."),
        TEXT("ALL incoming damage is DOUBLED for the same 45 seconds. A fractured holographic shell marks your vulnerability. Previously fired shots keep their launch-time penetration count."),45,FLinearColor(0.4f,0.8f,1));
    Glass->DamageMultiplier=1.75f;Glass->IncomingDamageMultiplier=2;Glass->ExtraPiercing=2;
    auto* Courier=Make(ENeonChallenge::JackpotCourier,TEXT("JACKPOT COURIER"),TEXT("Collect the marked volatile data cache and return to THIS sign within 35 seconds to earn ONE bonus three-card upgrade selection."),
        TEXT("While carrying the cache, pursuing enemies move 25% FASTER. Missing the delivery deadline causes 35% MAXIMUM-HP backlash. Follow the objective marker back to this sign."),35,FLinearColor(1,0.55f,0.04f));
    Courier->PenaltyHealthFraction=0.35f;Courier->PursuerSpeedMultiplier=1.25f;
    auto* Duel=Make(ENeonChallenge::LockdownDuel,TEXT("LOCKDOWN DUEL"),TEXT("Defeat the marked elite INSIDE the 9-meter-radius ring within 40 seconds. Earn ONE bonus three-card draft and 24 essence."),
        TEXT("Stay inside the visible ring. More than 2 seconds outside, leaving its elevation, or running out of time fails the contract and costs 35% MAXIMUM health. Normal enemies can enter. No invisible walls."),40,FLinearColor(0.85f,0.15f,1));
    Duel->PenaltyHealthFraction=0.35f;Duel->RewardEssence=24;
}
bool UNeonChallengeComponent::IsGameplayRunning() const
{
    const auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    return !bSuspended && Player.IsValid() && G && G->GetArenaPlayer()==Player.Get() && !G->bInCodex && !G->bRunEnded && !G->bChoosingCard
        && !UGameplayStatics::IsGamePaused(this) && !(G->TestingDirector && G->TestingDirector->IsActive());
}
void UNeonChallengeComponent::BeginRun()
{
    if(bRunStarted || !Player.IsValid()) return;
    bRunStarted=true;PlacementSeed=FMath::Rand();
    FActorSpawnParameters Params;Params.Owner=GetOwner();Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    OrbVisual=GetWorld()->SpawnActor<ANeonChallengeEffect>(ANeonChallengeEffect::StaticClass(),FTransform::Identity,Params);
    Echoes.SetNum(EchoCapacity);
    for(int32 I=0;I<EchoCapacity;++I) EchoVisuals.Add(GetWorld()->SpawnActor<ANeonChallengeEffect>(ANeonChallengeEffect::StaticClass(),FTransform::Identity,Params));
    GoalVisual=GetWorld()->SpawnActor<ANeonChallengeEffect>(ANeonChallengeEffect::StaticClass(),FTransform::Identity,Params);
    TrialVisual=GetWorld()->SpawnActor<ANeonChallengeEffect>(ANeonChallengeEffect::StaticClass(),FTransform::Identity,Params);
    for(auto* Offer:DrawOffers(Definitions,PlacementSeed)) RunOffers.Add(Offer);
    TryPlaceStations();
}
TArray<UNeonChallengeDefinition*> UNeonChallengeComponent::DrawOffers(const TArray<TObjectPtr<UNeonChallengeDefinition>>& Library,int32 Seed,int32 Count)
{
    TArray<UNeonChallengeDefinition*> Bag;TSet<ENeonChallenge> IDs;
    for(UNeonChallengeDefinition* D:Library) if(D && int32(D->Contract)<=int32(ENeonChallenge::LockdownDuel) && !IDs.Contains(D->Contract)) {IDs.Add(D->Contract);Bag.Add(D);}
    FRandomStream Random(Seed);
    for(int32 I=Bag.Num()-1;I>0;--I) Bag.Swap(I,Random.RandRange(0,I));
    Bag.SetNum(FMath::Clamp(Count,0,Bag.Num()));return Bag;
}
bool UNeonChallengeComponent::FindStationGround(FRandomStream& Random,FVector& Position) const
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav || !Player.IsValid()) return false;
    FNavLocation Start;if(!Nav->ProjectPointToNavigation(Player->GetActorLocation(),Start,FVector(100,100,250))) return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(StationPlacement),false,GetOwner());
    for(int32 Attempt=0;Attempt<48;++Attempt)
    {
        const float A=Random.FRandRange(0,2*PI),R=Random.FRandRange(1800,4900);
        FVector Candidate=FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0);
        FHitResult Ground;
        if(!GetWorld()->LineTraceSingleByObjectType(Ground,Candidate+FVector(0,0,1700),Candidate-FVector(0,0,1200),FCollisionObjectQueryParams(ECC_WorldStatic),Q) || Ground.ImpactNormal.Z<0.96f) continue;
        Candidate=Ground.ImpactPoint;
        bool bClose=false;for(ANeonChallengeStation* Station:Stations) if(IsValid(Station) && FVector::DistSquared2D(Station->GetActorLocation(),Candidate)<FMath::Square(1500.f)) bClose=true;
        if(bClose || FVector::DistSquared2D(Player->GetActorLocation(),Candidate)<FMath::Square(1400.f)) continue;
        if(GetWorld()->OverlapAnyTestByObjectType(Candidate+FVector(0,0,245),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeBox(FVector(90,210,235)),Q)) continue;
        // The front faces the gameplay camera (-X). Verify floor under the stand and its approach.
        bool bFlat=true;
        for(FVector Offset:{FVector(-55,-170,0),FVector(-55,170,0),FVector(55,-170,0),FVector(55,170,0),FVector(-230,0,0)})
        {
            FHitResult Floor;
            if(!GetWorld()->LineTraceSingleByObjectType(Floor,Candidate+Offset+FVector(0,0,35),Candidate+Offset-FVector(0,0,50),FCollisionObjectQueryParams(ECC_WorldStatic),Q) || FMath::Abs(Floor.ImpactPoint.Z-Candidate.Z)>12) {bFlat=false;break;}
        }
        if(!bFlat) continue;
        FNavLocation Approach;
        if(!Nav->ProjectPointToNavigation(Candidate+FVector(-230,0,0),Approach,FVector(50,50,100))) continue;
        UNavigationPath* Path=Nav->FindPathToLocationSynchronously(GetWorld(),Start.Location,Approach.Location);
        if(!Path || !Path->IsValid() || Path->IsPartial()) continue;
        Position=Candidate;return true;
    }
    return false;
}
void UNeonChallengeComponent::TryPlaceStations()
{
    if(!bRunStarted || !Player.IsValid()) return;
    FRandomStream Random(PlacementSeed+Stations.Num()*997+FMath::FloorToInt(GetWorld()->GetTimeSeconds()));
    for(UNeonChallengeDefinition* D:RunOffers)
    {
        if(!D || Stations.ContainsByPredicate([D](const auto& S){return S && S->Definition==D;})) continue;
        FVector Position;if(!FindStationGround(Random,Position)) continue;
        FActorSpawnParameters Params;Params.Owner=GetOwner();Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if(auto* S=GetWorld()->SpawnActor<ANeonChallengeStation>(ANeonChallengeStation::StaticClass(),Position,FRotator(0,180,0),Params))
        {S->Configure(D);Stations.Add(S);UE_LOG(LogTemp,Display,TEXT("CHALLENGE_STATION %s at %s seed=%d"),*D->Title.ToString(),*Position.ToString(),PlacementSeed);}
    }
}
ANeonChallengeStation* UNeonChallengeComponent::GetNearbyStation() const
{
    if(!IsGameplayRunning() || ActiveStation || OfferWidget) return nullptr;
    ANeonChallengeStation* Best=nullptr;double Distance=DBL_MAX;
    for(ANeonChallengeStation* S:Stations) if(IsValid(S) && S->State==ENeonContractState::Ready && S->CanInteract(Player.Get()))
    {const double D=FVector::DistSquared(S->GetActorLocation(),Player->GetActorLocation());if(D<Distance) {Best=S;Distance=D;}}
    return Best;
}
bool UNeonChallengeComponent::Interact()
{
    auto* S=GetNearbyStation();auto* C=Player.IsValid() ? Cast<AMVPController>(Player->GetController()) : nullptr;
    if(!S || !C || C->IsSettingsOpen() || C->TestingWidget || OfferedStation) return false;
    OfferedStation=S;S->SetContractState(ENeonContractState::Offered);
    OfferWidget=CreateWidget<UNeonChallengeWidget>(C,UNeonChallengeWidget::StaticClass());
    if(!OfferWidget) {S->SetContractState(ENeonContractState::Ready);OfferedStation=nullptr;return false;}
    bOwnPause=!C->IsPaused();if(bOwnPause) C->SetPause(true);C->SetMenuInput(true);
    OfferWidget->InitializeOffer(this,S->Definition);OfferWidget->AddToViewport(90);
    FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(OfferWidget->TakeWidget());C->SetInputMode(Input);C->bShowMouseCursor=true;
    return true;
}
bool UNeonChallengeComponent::AcceptOffer()
{
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    if(!OfferedStation || ActiveStation || !Player.IsValid() || !G || G->bRunEnded || G->bChoosingCard || G->bInCodex || bSuspended
        || OfferedStation->State!=ENeonContractState::Offered || !OfferedStation->CanInteract(Player.Get()) || !OfferedStation->Definition) return false;
    if(!PrepareTrial(OfferedStation)) return false;
    ActiveStation=OfferedStation;ActiveStation->SetContractState(ENeonContractState::Active);
    Elapsed=0;RemainingTime=FMath::Max(1.f,ActiveStation->Definition->Duration);ConfirmedHitCount=0;HitCooldown=0;PendingHunters=0;bOrbPositionValid=false;
    NextHunterWave=FMath::Max(1.f,ActiveStation->Definition->HunterInterval);HuntersSpawned=0;HuntWarnings=0;HunterRetry=0;LastResult.Reset();
    OutsideTime=0;bCarryingCache=false;KillFlash=0;
    CloseOffer();
    if(G->AudioDirector) G->AudioDirector->PlayUI(TEXT("UI_Confirm"));
    UE_LOG(LogTemp,Display,TEXT("CHALLENGE_ACCEPT %s"),*ActiveStation->Definition->Title.ToString());return true;
}
void UNeonChallengeComponent::DeclineOffer()
{
    if(!OfferedStation && !OfferWidget) return;
    if(OfferedStation && OfferedStation->State==ENeonContractState::Offered) OfferedStation->SetContractState(ENeonContractState::Ready);
    CloseOffer();
}
void UNeonChallengeComponent::CloseOffer()
{
    if(OfferWidget) {OfferWidget->RemoveFromParent();OfferWidget=nullptr;}OfferedStation=nullptr;
    auto* C=Player.IsValid() ? Cast<AMVPController>(Player->GetController()) : nullptr;
    const auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    if(C && G)
    {
        C->SetMenuInput(G->bInCodex || G->bChoosingCard || G->bRunEnded);
        if(bOwnPause && !G->bInCodex && !G->bChoosingCard && !G->bRunEnded) C->SetPause(false);
    }
    bOwnPause=false;
}
float UNeonChallengeComponent::GetDamageMultiplier() const
{
    return IsGameplayRunning() && ActiveStation && ActiveStation->Definition && !bResolvingEcho
        ? FMath::Clamp(ActiveStation->Definition->DamageMultiplier,1.f,20.f) : 1.f;
}
int32 UNeonChallengeComponent::ModifyEssenceReward(int32 BaseReward) const
{
    const int32 Multiplier=IsGameplayRunning() && ActiveStation && ActiveStation->Definition && ActiveStation->Definition->Contract==ENeonChallenge::HostileBroadcast ? FMath::Clamp(ActiveStation->Definition->EssenceMultiplier,1,10) : 1;
    return FMath::Clamp(BaseReward,1,100000)*Multiplier;
}
FVector UNeonChallengeComponent::GetOrbLocation() const
{
    const float Angle=Elapsed*1.6f;
    return Player.IsValid() ? Player->GetActorLocation()+FVector(FMath::Cos(Angle)*180,FMath::Sin(Angle)*180,45+12*FMath::Sin(Elapsed*3)) : FVector::ZeroVector;
}
void UNeonChallengeComponent::ResolveOrbContact(AMVPEnemy* Enemy,bool bSweepConfirmed)
{
    if(!IsGameplayRunning() || !ActiveStation || !ActiveStation->Definition || ActiveStation->Definition->Contract!=ENeonChallenge::OverloadOrb || Elapsed<1 || !IsValid(Enemy) || !Enemy->IsPoolActive() || Enemy->bTrainingTarget) return;
    const auto* Sphere=Cast<USphereComponent>(Enemy->GetRootComponent());const FVector Orb=GetOrbLocation();
    if(!bSweepConfirmed && FVector::DistSquared(Enemy->GetActorLocation(),Orb)>FMath::Square(32+(Sphere ? Sphere->GetScaledSphereRadius() : 34))) return;
    const float Damage=Player->MaxHealth*FMath::Clamp(ActiveStation->Definition->PenaltyHealthFraction,0.f,1.f);
    ++OrbRuptures;FinishContract(ENeonContractState::Failed,TEXT("ORB RUPTURED / LOST 50% MAX HP"));
    if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector) G->SpellImpactDirector->PlayImpact(ENeonSpellStyle::Prism,Orb);
    Player->ReceiveChallengeDamage(Damage);
}
void UNeonChallengeComponent::OnConfirmedSpellHit(FVector Position,float Damage)
{
    if(!IsGameplayRunning() || !ActiveStation || !ActiveStation->Definition || bResolvingEcho || ActiveStation->Definition->Contract!=ENeonChallenge::UnstableEcho || !FMath::IsFinite(Damage) || Damage<=0) return;
    auto* D=ActiveStation->Definition.Get();
    ConfirmedHitCount=FMath::Min(ConfirmedHitCount+1,FMath::Max(1,D->HitsPerEcho));
    if(ConfirmedHitCount<FMath::Max(1,D->HitsPerEcho) || HitCooldown>0) return;
    for(auto& Echo:Echoes) if(Echo.Age<0)
    {
        FHitResult Ground;
        if(GetWorld()->LineTraceSingleByObjectType(Ground,Position+FVector(0,0,60),Position-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic))) Position=Ground.ImpactPoint;
        Echo.Age=0;Echo.Position=Position;Echo.Damage=Damage*FMath::Clamp(D->EchoDamageMultiplier,0.f,10.f);Echo.Radius=FMath::Clamp(D->EchoRadius,50.f,500.f);
        Echo.Delay=FMath::Max(0.1f,D->EchoDelay);Echo.PlayerPenalty=FMath::Clamp(D->PenaltyHealthFraction,0.f,1.f);Echo.bDetonated=false;
        ConfirmedHitCount=0;HitCooldown=FMath::Max(0.05f,D->EchoCooldown);break;
    }
}
void UNeonChallengeComponent::DetonateEcho(FEcho& Echo)
{
    Echo.bDetonated=true;++EchoesDetonated;TGuardValue<bool> Guard(bResolvingEcho,true);
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    if(G && G->SpellImpactDirector) G->SpellImpactDirector->PlayImpact(ENeonSpellStyle::Prism,Echo.Position+FVector(0,0,35));
    if(G && G->AudioDirector) G->AudioDirector->PlayImpact(Echo.Position);
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits,Echo.Position+FVector(0,0,70),FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(Echo.Radius));
    TSet<AMVPEnemy*> Seen;
    for(const auto& H:Hits) if(auto* E=Cast<AMVPEnemy>(H.GetActor());E && E->IsPoolActive() && !E->bTrainingTarget && !Seen.Contains(E))
    {
        Seen.Add(E);FHitResult Wall;
        if(GetWorld()->LineTraceSingleByObjectType(Wall,Echo.Position+FVector(0,0,25),E->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic))) continue;
        const float Before=E->Health;E->TakeSpellDamage(Echo.Damage,FVector::UpVector*350);
        if(G && G->SpellImpactDirector) G->SpellImpactDirector->ConfirmDamage(ENeonSpellStyle::Prism,E,FMath::Min(Before,Echo.Damage));
    }
    if(Player.IsValid() && FVector::DistSquared2D(Player->GetActorLocation(),Echo.Position)<=FMath::Square(Echo.Radius) && FMath::Abs(Player->GetActorLocation().Z-Echo.Position.Z)<160)
    {
        FHitResult Wall;
        if(!GetWorld()->LineTraceSingleByObjectType(Wall,Echo.Position+FVector(0,0,25),Player->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic))) Player->ReceiveChallengeDamage(Player->MaxHealth*Echo.PlayerPenalty);
    }
}
bool UNeonChallengeComponent::TrySpawnHunter()
{
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());auto* C=Player.IsValid() ? Cast<AMVPController>(Player->GetController()) : nullptr;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if(!G || !G->Spawner || !C || !Nav) return false;
    auto* S=G->Spawner.Get();FNavLocation Goal;
    if(!Nav->ProjectPointToNavigation(Player->GetActorLocation(),Goal,FVector(120,120,250))) return false;
    // Convert an existing pooled unit when the cap is full; never raise the cap.
    TArray<FOverlapResult> Found;FVector SpawnPoint=FVector::ZeroVector;bool bPoint=false;
    for(int32 I=0;I<12;++I)
    {
        const float A=FMath::FRandRange(0,2*PI);FNavLocation Ground;
        const float R=FMath::Clamp(Player->PreferredCameraDistance*1.3f,1900.f,4800.f);
        const FVector P=Player->GetActorLocation()+FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0);
        FHitResult Floor;
        if(!GetWorld()->LineTraceSingleByObjectType(Floor,P+FVector(0,0,1600),P-FVector(0,0,1600),FCollisionObjectQueryParams(ECC_WorldStatic)) || !Nav->ProjectPointToNavigation(Floor.ImpactPoint,Ground,FVector(100,100,150))) continue;
        if(GetWorld()->OverlapAnyTestByObjectType(Ground.Location+FVector(0,0,91),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(69))) continue;
        FVector2D Screen;int32 W=0,H=0;C->GetViewportSize(W,H);
        if(C->ProjectWorldLocationToScreen(Ground.Location+FVector(0,0,80),Screen) && Screen.X>=-100 && Screen.X<=W+100 && Screen.Y>=-100 && Screen.Y<=H+100) continue;
        UNavigationPath* Path=Nav->FindPathToLocationSynchronously(GetWorld(),Ground.Location,Goal.Location);
        if(!Path || !Path->IsValid() || Path->IsPartial()) continue;
        SpawnPoint=Ground.Location+FVector(0,0,90);bPoint=true;break;
    }
    if(!bPoint) return false;
    const bool bBossAlreadySpawned=G->bBroadcastBossSpawned;
    AMVPEnemy* Hunter=Cast<AMVPEnemy>(S->SpawnEnemyFromPool(SpawnPoint));
    G->bBroadcastBossSpawned=bBossAlreadySpawned;
    if(!Hunter)
    {
        // Prefer a normal unit already outside the viewport; retain its active pool slot.
        GetWorld()->OverlapMultiByObjectType(Found,Player->GetActorLocation(),FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(10000));
        for(const auto& H:Found) if(auto* E=Cast<AMVPEnemy>(H.GetActor());E && E->IsPoolActive() && !E->bTrainingTarget && E->EnemyTier<2)
        {
            FVector2D Screen;int32 W=0,Height=0;C->GetViewportSize(W,Height);
            if(C->ProjectWorldLocationToScreen(E->GetActorLocation(),Screen) && Screen.X>=-80 && Screen.X<=W+80 && Screen.Y>=-80 && Screen.Y<=Height+80) continue;
            Hunter=E;++Hunter->Generation;break;
        }
    }
    if(!Hunter) return false;
    Hunter->SetActorLocation(SpawnPoint);Hunter->ApplyEnemyArchetype(6+(HuntersSpawned%3));
    Hunter->SetCombatHitFlash(FLinearColor::Black,0);Hunter->SetActorTickEnabled(true);
    if(auto* Terrain=Hunter->FindComponentByClass<UMortisTerrain>()) Terrain->ResetPath();
    // Existing timed-boss state belongs to the normal wave spawner, not this contract.
    return true;
}
void UNeonChallengeComponent::UpdateHunters(float Dt)
{
    if(!ActiveStation || !ActiveStation->Definition) return;
    auto* D=ActiveStation->Definition.Get();
    const float Duration=FMath::Max(1.f,D->Duration);
    if(NextHunterWave<Duration && Elapsed>=NextHunterWave-3 && HuntWarnings<FMath::RoundToInt(NextHunterWave/FMath::Max(1.f,D->HunterInterval)))
    {++HuntWarnings;if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->AudioDirector) G->AudioDirector->PlayUI(TEXT("UI_Deny"));}
    if(Elapsed>=NextHunterWave && NextHunterWave<Duration)
    {PendingHunters+=FMath::Clamp(D->HuntersPerWave,1,8);NextHunterWave+=FMath::Max(1.f,D->HunterInterval);}
    // Bounded retries; accumulated work waits for pool space rather than losing reinforcements.
    HunterRetry-=Dt;
    if(PendingHunters>0 && HunterRetry<=0)
    {
        HunterRetry=0.5f;
        for(int32 I=0,Count=FMath::Min(2,PendingHunters);I<Count;++I)
            if(TrySpawnHunter()) {--PendingHunters;++HuntersSpawned;} else break;
    }
}
void UNeonChallengeComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Dt,Type,Function);if(!IsGameplayRunning()) return;
    if(bRunStarted && Stations.Num()<RunOffers.Num() && (StationRetry-=Dt)<=0) {StationRetry=2;TryPlaceStations();}
    LastResultTime=FMath::Max(0.f,LastResultTime-Dt);HitCooldown=FMath::Max(0.f,HitCooldown-Dt);
    for(int32 I=0;I<Echoes.Num();++I)
    {
        auto& Echo=Echoes[I];if(Echo.Age<0) continue;
        Echo.Age+=Dt;
        if(!Echo.bDetonated && Echo.Age>=Echo.Delay) DetonateEcho(Echo);
        if(!IsGameplayRunning()) return;
        if(Echo.Age>=Echo.Delay+0.25f) {Echo.Age=-1;if(EchoVisuals[I]) EchoVisuals[I]->HideEffect();}
        else if(EchoVisuals[I]) EchoVisuals[I]->ShowEcho(Echo.Position,Echo.Radius,Echo.Age/Echo.Delay,Echo.bDetonated);
    }
    if(!ActiveStation || !ActiveStation->Definition) return;
    const float ActiveDt=FMath::Min(Dt,FMath::Max(0.f,ActiveStation->Definition->Duration-Elapsed));
    Elapsed+=Dt;RemainingTime=FMath::Max(0.f,ActiveStation->Definition->Duration-Elapsed);
    const auto Kind=ActiveStation->Definition->Contract;
    if(Kind==ENeonChallenge::OverloadOrb)
    {
        const FVector Position=GetOrbLocation();if(OrbVisual) OrbVisual->ShowOrb(Position,Player->GetActorLocation()+FVector(0,0,45),Elapsed);
        TArray<FHitResult> Found;
        GetWorld()->SweepMultiByObjectType(Found,bOrbPositionValid ? PreviousOrb : Position,Position,FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(32));
        PreviousOrb=Position;bOrbPositionValid=true;
        for(const auto& H:Found) if(auto* E=Cast<AMVPEnemy>(H.GetActor())) {ResolveOrbContact(E,true);if(!ActiveStation) break;}
    }
    else if(Kind==ENeonChallenge::HostileBroadcast) UpdateHunters(Dt);
    else UpdateTrial(ActiveDt);
    if(ActiveStation && RemainingTime<=0)
    {
        if(Kind==ENeonChallenge::JackpotCourier || Kind==ENeonChallenge::LockdownDuel) ResolveTrialFailure(TEXT("CONTRACT FAILED / DEADLINE EXPIRED"));
        else FinishContract(ENeonContractState::Succeeded,TEXT("CONTRACT COMPLETE / BONUS ENDED"));
    }
}
void UNeonChallengeComponent::FinishContract(ENeonContractState State,const FString& Message)
{
    if(!ActiveStation) return;
    ActiveStation->SetContractState(State);ActiveStation=nullptr;RemainingTime=0;PendingHunters=0;
    bCarryingCache=false;OutsideTime=0;DuelTarget.Reset();DuelTargetGeneration=0;
    if(GoalVisual) GoalVisual->HideEffect();if(TrialVisual) TrialVisual->HideEffect();
    if(OrbVisual) OrbVisual->HideEffect();bOrbPositionValid=false;LastResult=Message;LastResultTime=7;
    UE_LOG(LogTemp,Display,TEXT("CHALLENGE_RESULT %s"),*Message);
}
void UNeonChallengeComponent::ClearEffects()
{
    if(OrbVisual) OrbVisual->HideEffect();for(int32 I=0;I<Echoes.Num();++I) {Echoes[I].Age=-1;if(EchoVisuals.IsValidIndex(I) && EchoVisuals[I]) EchoVisuals[I]->HideEffect();}
}
void UNeonChallengeComponent::AbortContract()
{
    DeclineOffer();FinishContract(ENeonContractState::Aborted,TEXT("CONTRACT CLOSED"));ClearEffects();
}
void UNeonChallengeComponent::SetTestingSuspended(bool bInSuspended)
{
    bSuspended=bInSuspended;
    for(ANeonChallengeStation* S:Stations) if(S) {S->SetActorHiddenInGame(bSuspended);S->SetActorEnableCollision(!bSuspended);S->SetActorTickEnabled(!bSuspended);}
    if(bSuspended) {if(OrbVisual) OrbVisual->HideEffect();for(ANeonChallengeEffect* E:EchoVisuals) if(E) E->HideEffect();}
    if(bSuspended) {if(GoalVisual) GoalVisual->HideEffect();if(TrialVisual) TrialVisual->HideEffect();}
}
int32 UNeonChallengeComponent::GetPendingEchoCount() const {return Echoes.FilterByPredicate([](const FEcho& E){return E.Age>=0 && !E.bDetonated;}).Num();}
FString UNeonChallengeComponent::GetTrackerText() const
{
    if(ActiveStation && ActiveStation->Definition)
    {
        const auto* D=ActiveStation->Definition.Get();
        const FString Broadcast=PendingHunters>0 ? FString::Printf(TEXT("2x ESSENCE / %d ELITE HUNTERS INBOUND"),PendingHunters)
            : NextHunterWave<D->Duration ? FString::Printf(TEXT("2x ESSENCE / HUNTERS IN %ds"),FMath::Max(0,FMath::CeilToInt(NextHunterWave-Elapsed)))
            : TEXT("2x ESSENCE / SURVIVE THE REMAINING HUNTERS");
        FString Detail;
        switch(D->Contract)
        {
        case ENeonChallenge::OverloadOrb: Detail=Elapsed<1 ? TEXT("ORB ARMING") : TEXT("3x DAMAGE / PROTECT THE ORB");break;
        case ENeonChallenge::UnstableEcho: Detail=TEXT("MOVE CLEAR OF MARKED BLASTS");break;
        case ENeonChallenge::HostileBroadcast: Detail=Broadcast;break;
        case ENeonChallenge::DeadmanBattery: Detail=TEXT("2x DAMAGE / DRAIN 2% HP/s / KILLS RESTORE 4%");break;
        case ENeonChallenge::GlassProtocol: Detail=TEXT("+75% DAMAGE / +2 PIERCING / TAKE DOUBLE DAMAGE");break;
        case ENeonChallenge::JackpotCourier: Detail=bCarryingCache ? TEXT("RETURN CACHE TO SIGN / PURSUERS +25% SPEED") : TEXT("COLLECT THE MARKED DATA CACHE");break;
        case ENeonChallenge::LockdownDuel: Detail=OutsideTime>0 ? FString::Printf(TEXT("RETURN TO RING! / %.1fs GRACE LEFT"),FMath::Max(0.f,D->OutsideGrace-OutsideTime)) : FString::Printf(TEXT("DEFEAT MARKED ELITE / %.0f HP"),DuelTarget.IsValid() ? FMath::Max(0.f,DuelTarget->Health) : 0);break;
        }
        return FString::Printf(TEXT("%s / %ds\n%s"),*D->Title.ToString(),FMath::CeilToInt(RemainingTime),*Detail);
    }
    return LastResultTime>0 ? LastResult : FString();
}
void UNeonChallengeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(OfferWidget) {OfferWidget->RemoveFromParent();OfferWidget=nullptr;}
    for(ANeonChallengeStation* S:Stations) if(IsValid(S)) S->Destroy();for(ANeonChallengeEffect* E:EchoVisuals) if(IsValid(E)) E->Destroy();if(IsValid(OrbVisual)) OrbVisual->Destroy();
    if(IsValid(GoalVisual)) GoalVisual->Destroy();if(IsValid(TrialVisual)) TrialVisual->Destroy();
    Super::EndPlay(Reason);
}
