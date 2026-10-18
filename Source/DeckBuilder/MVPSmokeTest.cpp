#include "MVPGame.h"
#include <limits>
#include "CodexCardSelectionWidget.h"
#include "MortisProgressionManager.h"
#include "BloodSurfaceDirector.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/Button.h"
#include "NecromancyVisualComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "CodexLoadoutSave.h"
#include "SkeletonMinion.h"
#include "CinematicExplosionDirector.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const TCHAR* ValidateTerrainSpells(AMVPArenaGameMode* Game);
const TCHAR* ValidateNeonRider(AMVPPlayer* Player);
void RunArsenalSmoke(AMVPController* Controller);
void RunCombatFeedbackSmoke(AMVPController* Controller);
void RunHealthAlarmSmoke(AMVPController* Controller);
void RunTestingAreaSmoke(AMVPController* Controller);
void RunTestingMultiSmoke(AMVPController* Controller);
void RunChallengeSmoke(AMVPController* Controller);
void RunSevenChallengeSmoke(AMVPController* Controller);
void RunStreetLightingSmoke(AMVPController* Controller);

// Opt-in end-to-end harness in a real standalone world. No player runs execute this.
// It uses normal input bindings, spell timers, collisions, gem pickup and selection.
void AMVPController::TickSmokeTest()
{
#if !UE_BUILD_SHIPPING
    static bool bAwaitingRestart = false;
    static TWeakObjectPtr<AMVPEnemy> NavigationProbe;
    auto Fail = [this](const TCHAR* Reason)
    {
        UE_LOG(LogTemp, Error, TEXT("MVP_SMOKE_FAIL phase=%d: %s"), SmokePhase, Reason);
        bSmokeTest = false;
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    auto Advance = [this](int32 Phase)
    {
        SmokePhase = Phase;
        SmokePhaseTime = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("MVP_SMOKE_PHASE %d"), Phase);
    };
    auto Capture = [](const TCHAR* Name)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name, true, false);
    };
    AMVPArenaGameMode* Game = Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    AMVPPlayer* ArenaPlayer = Cast<AMVPPlayer>(GetPawn());
    if (!Game || !ArenaPlayer || !Game->Spawner) return;
    if(FParse::Param(FCommandLine::Get(),TEXT("StreetLightingSmoke"))) {RunStreetLightingSmoke(this);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("SevenChallengeSmoke"))) {RunSevenChallengeSmoke(this);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("ChallengeSmoke"))) {RunChallengeSmoke(this);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("TestingMultiSmoke"))) {RunTestingMultiSmoke(this);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("TestingAreaSmoke"))) {RunTestingAreaSmoke(this);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("HealthAlarmSmoke"))) {RunHealthAlarmSmoke(this);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("CombatFeedbackSmoke"))) {RunCombatFeedbackSmoke(this);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("ArsenalSmoke"))) {RunArsenalSmoke(this);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonPolishSmoke"))) {TickNeonPolishSmokeTest();return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("SettingsSmoke"))) {TickSettingsSmokeTest();return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("CitySmoke"))) {TickCitySmokeTest();return;}
    if (SmokePhase > 0 && FPlatformTime::Seconds() - SmokePhaseTime > 40.0) { Fail(TEXT("Timed out")); return; }
    const double Elapsed = FPlatformTime::Seconds() - SmokePhaseTime;
    if(FParse::Param(FCommandLine::Get(),TEXT("EnergyPreview")))
    {
        if(SmokePhase==0)
        {
            Game->StartingCards={TEXT("PrismBurst"),TEXT("KineticCards")};
            if(!Game->StartSelectedRun()) {Fail(TEXT("Energy preview deck failed"));return;}
            Game->Spawner->StopWaves();ArenaPlayer->bBenchmarkInvulnerable=true;
            UCardDataAsset* Guard=nullptr;
            for(UCardDataAsset* Card:ArenaPlayer->Deck->UnlockedCards) if(Card->CardID==TEXT("TalismanGuard")) Guard=Card;
            ArenaPlayer->Deck->EquippedDeck={Guard};ArenaPlayer->Deck->DiscardHand();ArenaPlayer->Deck->DrawLevelUpHand();
            ArenaPlayer->Deck->AddSelectedCardSpell(Guard);ArenaPlayer->Deck->EquippedDeck=ArenaPlayer->Deck->UnlockedCards;
            for(int32 I=0;I<8;++I)
                if(auto* Enemy=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(FVector(380+I*45,(I-4)*80,55)))))
                {Enemy->Health=1000000.f;Enemy->SetActorTickEnabled(false);}
            Advance(96);return;
        }
        if(SmokePhase==96 && Elapsed>6.2f) {Capture(TEXT("NeonEnergy-Combat.png"));Advance(97);}
        else if(SmokePhase==97 && Elapsed>1.5f) {UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);bSmokeTest=false;FPlatformMisc::RequestExit(false);}
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonPreview")))
    {
        if(SmokePhase==0)
        {
            Game->bInCodex=false;SetPause(false);SetMenuInput(false);Game->Spawner->StopWaves();
            if(GetHUD()) GetHUD()->bShowHUD=false;
            ArenaPlayer->SetActorLocation(FVector(0,0,40));
            ArenaPlayer->PreferredCameraDistance=550.f;ArenaPlayer->CameraBoom->TargetArmLength=550.f;
            ArenaPlayer->CameraBoom->SetRelativeRotation(FRotator(-15.f,-55.f,0.f));
            ArenaPlayer->CameraBoom->TargetOffset.Z=50.f;ArenaPlayer->FollowCamera->SetFieldOfView(45.f);
            Advance(90);
            return;
        }
        ArenaPlayer->Movement->StopMovementImmediately();
        ArenaPlayer->FindComponentByClass<UNecromancyVisualComponent>()->SetWorldRotation(FRotator::ZeroRotator);
        if(FParse::Param(FCommandLine::Get(),TEXT("NeonAttackPreview")))
        {
            auto* Art=ArenaPlayer->FindComponentByClass<UNecromancyVisualComponent>();
            if(SmokePhase==90 && Elapsed>3.f) {Art->TriggerRiderCast(TEXT("CastBlood"));Advance(92);}
            else if(SmokePhase==92 && Elapsed>0.32f) {Capture(TEXT("NeonExorcist-BloodCast.png"));Advance(93);}
            else if(SmokePhase==93 && Elapsed>1.2f) {Art->TriggerRiderCast(TEXT("CastStorm"));Advance(94);}
            else if(SmokePhase==94 && Elapsed>0.32f) {Capture(TEXT("NeonExorcist-StormCast.png"));Advance(95);}
            else if(SmokePhase==95 && Elapsed>6.f) {Capture(TEXT("NeonExorcist-Idle10.png"));Advance(91);return;}
        }
        if(SmokePhase==90 && Elapsed>5.f) {Capture(TEXT("NeonExorcist-InGame.png"));Advance(91);}
        else if(SmokePhase==91 && Elapsed>0.5f) {bSmokeTest=false;FPlatformMisc::RequestExit(false);}
        return;
    }
    if (SmokePhase == 3 && Elapsed > 0.52 && Elapsed < 0.6) Capture(TEXT("MVP-CodexPolished.png"));
    if (SmokePhase == 0)
    {
        if (bAwaitingRestart)
        {
            if (ArenaPlayer->PreferredCameraDistance != 1400.f) { Fail(TEXT("Zoom preference did not survive restart")); return; }
            if (Game->Level != 1 || Game->XP != 0 || Game->Kills != 0 || ArenaPlayer->Health != 100.f
                || Game->bRunEnded || Game->bChoosingCard || !IsPaused() || !Game->bInCodex || !ArenaPlayer->Deck->ActiveWeapons.IsEmpty()
                || Game->StartingCards != TArray<FName>{FName(TEXT("PulseDart"))})
            { Fail(TEXT("Restart did not reset run state")); return; }
            Game->ToggleStartingCard(TEXT("PrismBurst"));
            if (!Game->StartSelectedRun() || ArenaPlayer->Deck->ActiveWeapons.Num() != 2 || !ArenaPlayer->Deck->FindOwnedSpell(TEXT("PrismBurst")))
            { Fail(TEXT("Two-spell starting loadout failed")); return; }
            Game->Spawner->StopWaves();
            ArenaPlayer->Deck->FindOwnedSpell(TEXT("PrismBurst"))->StopCasting();
            auto* Bone = ArenaPlayer->Deck->FindOwnedSpell(TEXT("PulseDart"));
            Bone->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.Nova")),1);
            Bone->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.ExplodeOnImpact")),1);
            Bone->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.PiercingCount")),2);
            auto* Target=Game->Spawner->ActivateEnemyFromPool(FTransform(ArenaPlayer->GetActorLocation()+FVector(900,0,0)));
            Target->SetActorTickEnabled(false);
            Advance(12);
            return;
        }
        if (!Game->bInCodex || !IsPaused() || !ArenaPlayer->Deck->ActiveWeapons.IsEmpty() || Game->Spawner->GetActiveEnemyCount() != 0)
        { Fail(TEXT("Combat started before loadout selection")); return; }
        Game->SchoolFilter = static_cast<int32>(EMagicSchool::Prism);
        const auto Filtered = Game->GetLibraryCards();
        if (Filtered.Num()!=2 || Filtered[0]->CardID!=FName(TEXT("PrismBurst"))) { Fail(TEXT("School filter failed")); return; }
        Game->SchoolFilter = static_cast<int32>(EMagicSchool::Curses);
        if (!Game->GetLibraryCards().IsEmpty()) { Fail(TEXT("Empty school filter failed")); return; }
        Game->SchoolFilter = -1;
        Game->StartingCards.Reset();
        if (Game->StartSelectedRun() || Game->ToggleStartingCard(TEXT("Unknown"))) { Fail(TEXT("Invalid starter accepted")); return; }
        Game->StartingCards={TEXT("TalismanGuard")};
        if(Game->SaveStartingDeck()) {Fail(TEXT("Finite barrier accepted as only starter"));return;}
        Game->StartingCards.Reset();
        Game->ToggleStartingCard(TEXT("PrismBurst"));
        Game->ToggleStartingCard(TEXT("HaloNova"));
        if (Game->ToggleStartingCard(TEXT("PulseDart")) || !Game->SaveStartingDeck()) { Fail(TEXT("Deck capacity/save failed")); return; }
        Game->StartingCards.Reset();
        Game->LoadStartingDeck();
        if (Game->StartingCards != TArray<FName>{FName(TEXT("PrismBurst")),FName(TEXT("HaloNova"))}) { Fail(TEXT("Deck roundtrip failed")); return; }
        auto* DirtySave=Cast<UCodexLoadoutSave>(UGameplayStatics::CreateSaveGameObject(UCodexLoadoutSave::StaticClass()));
        DirtySave->StartingCards={TEXT("RaiseSkeletons")};
        UGameplayStatics::SaveGameToSlot(DirtySave,TEXT("CodexDeck_Automation"),0);Game->LoadStartingDeck();
        if(Game->StartingCards!=TArray<FName>{TEXT("TalismanGuard"),TEXT("PulseDart")}) {Fail(TEXT("Summon-only migration stranded a finite guard loadout"));return;}
        DirtySave->StartingCards={TEXT("Unknown"),TEXT("BoneLance"),TEXT("PulseDart")};
        UGameplayStatics::SaveGameToSlot(DirtySave,TEXT("CodexDeck_Automation"),0);
        Game->LoadStartingDeck();
        if (Game->StartingCards != TArray<FName>{FName(TEXT("PulseDart"))}) { Fail(TEXT("Stale or duplicate saved IDs survived")); return; }
        Capture(TEXT("MVP-Codex.png"));
        Advance(10);
    }
    else if (SmokePhase == 10 && Elapsed > 0.7)
    {
        if (Game->RunSeconds != 0.f || Game->Spawner->GetActiveEnemyCount()!=0) { Fail(TEXT("Combat advanced in Codex")); return; }
        int32 W,H;
        GetViewportSize(W,H);
        const float S=FMath::Min(W/1600.f,H/900.f);
        auto* UI=Cast<AMVPHUD>(GetHUD());
        if (!UI) { Fail(TEXT("Missing Codex HUD")); return; }
        auto ClickUI=[&](float X,float Y) { UI->ClickAt((W-1600*S)*0.5f+X*S,(H-900*S)*0.5f+Y*S); };
        ClickUI(500,205); // Fire tab; first card now maps to Ember Fan.
        ClickUI(200,530);
        if (Game->SchoolFilter!=static_cast<int32>(EMagicSchool::Prism) || !Game->StartingCards.Contains(TEXT("PrismBurst")))
        { Fail(TEXT("Filtered card click failed")); return; }
        ClickUI(600,665); // Remove the second slot while the library is filtered.
        ClickUI(200,205);
        ClickUI(200,800);
        if (Game->SchoolFilter!=-1 || Game->StartingCards!=TArray<FName>{FName(TEXT("PulseDart"))})
        { Fail(TEXT("Loadout slot click failed")); return; }
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Enter,IE_Pressed,1.f));
        Advance(11);
    }
    else if (SmokePhase == 11 && Elapsed > 0.2)
    {
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Enter,IE_Released,0.f));
        if (Game->bInCodex || IsPaused() || ArenaPlayer->Deck->ActiveWeapons.Num()!=1 || !ArenaPlayer->Deck->FindOwnedSpell(TEXT("PulseDart")))
        { Fail(TEXT("Enter did not start selected loadout")); return; }
        Game->Spawner->StopWaves();
        const float BeforeInvalidDamage = ArenaPlayer->Health;
        ArenaPlayer->ZoomCamera(100.f);
        if (ArenaPlayer->PreferredCameraDistance != 850.f) { Fail(TEXT("Zoom minimum clamp failed")); return; }
        ArenaPlayer->ZoomCamera(-100.f);
        if (ArenaPlayer->PreferredCameraDistance != 3600.f) { Fail(TEXT("Zoom maximum clamp failed")); return; }
        ArenaPlayer->ZoomCamera(22.f);
        if (!ArenaPlayer->SaveCameraPreference()) { Fail(TEXT("Zoom save failed")); return; }
        ArenaPlayer->ZoomCamera(1.f);
        ArenaPlayer->LoadCameraPreference();
        if (ArenaPlayer->PreferredCameraDistance != 1400.f || ArenaPlayer->CameraBoom->TargetArmLength != 1400.f)
        { Fail(TEXT("Zoom save/load roundtrip failed")); return; }
        ArenaPlayer->TakeDamageAmount(-10.f);
        ArenaPlayer->TakeDamageAmount(std::numeric_limits<float>::infinity());
        if (ArenaPlayer->Health != BeforeInvalidDamage || ArenaPlayer->GetClosestEnemy(-1.f))
        { Fail(TEXT("Invalid player damage/range was accepted")); return; }
        if (!ArenaPlayer->FindComponentByClass<UNecromancyVisualComponent>()->IsArtReady() || !ArenaPlayer->FindComponentByClass<UNecromancyVisualComponent>()->GetMaterial(0))
        { Fail(TEXT("Player art or material missing")); return; }
        if(const TCHAR* Error=ValidateNeonRider(ArenaPlayer)) {Fail(Error);return;}
        for (TActorIterator<AMVPEnemy> It(GetWorld()); It; ++It)
        {
            if (!It->FindComponentByClass<UNecromancyVisualComponent>()->IsArtReady() || !It->FindComponentByClass<UNecromancyVisualComponent>()->GetMaterial(0))
            { Fail(TEXT("Enemy art or material missing")); return; }
            if (It->FindComponentByClass<UNecromancyVisualComponent>()->IsComponentTickEnabled())
            { Fail(TEXT("Pooled animation still ticking")); return; }
        }
        SmokeStartPosition = ArenaPlayer->GetActorLocation();
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Pressed, 1.f));
        Advance(1);
    }
    else if (SmokePhase == 1 && Elapsed > 0.7)
    {
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Released, 0.f));
        if (!ArenaPlayer->FindComponentByClass<UNecromancyVisualComponent>()->GetSingleNodeInstance() || ArenaPlayer->FindComponentByClass<UNecromancyVisualComponent>()->GetPosition() <= 0.f)
        { Fail(TEXT("Player animation did not advance")); return; }
        if (ArenaPlayer->GetActorLocation().X < SmokeStartPosition.X + 100.f) { Fail(TEXT("W movement did not move pawn")); return; }
        for (int32 Index = 0; Index < 5; ++Index)
            Game->Spawner->ActivateEnemyFromPool(FTransform(ArenaPlayer->GetActorLocation() + FVector(500.f + Index * 60.f, (Index - 2) * 80.f, 0.f)));
        if (ArenaPlayer->GetClosestEnemy(1000.f) != Game->Spawner->FindNearestEnemy(ArenaPlayer->GetActorLocation(), 1000.f))
        { Fail(TEXT("Player targeting disagrees with active enemy pool")); return; }
        Advance(2);
    }
    else if (SmokePhase == 2)
    {
        if (Elapsed > 0.4 && Elapsed < 0.5) Capture(TEXT("MVP-Combat.png"));
        if (Game->bChoosingCard)
        {
            if (Game->Level != 2 || Game->Kills < 5 || ArenaPlayer->Deck->CurrentHand.Num() != 3 || !IsPaused())
            { Fail(TEXT("Kill/XP/three-card pause flow failed")); return; }
            TSet<FName> IDs;
            for (const UCardDataAsset* Card : ArenaPlayer->Deck->CurrentHand) IDs.Add(Card->CardID);
            if (!Game->SelectionWidget || Game->SelectionWidget->DisplayCards.Num() != 3) { Fail(TEXT("UMG hand missing")); return; }
            for (const auto& Display : Game->SelectionWidget->DisplayCards)
                if (Display.Description.IsEmpty() || Display.Description.ToString() == TEXT("None") || !Display.Icon || !Display.SchoolIcon)
                { Fail(TEXT("UMG spell description missing")); return; }
            if (IDs.Num() != 3) { Fail(TEXT("Duplicate card choices")); return; }
            SmokePausedTime = Game->RunSeconds;
            SmokeStartPosition = ArenaPlayer->GetActorLocation();
            Capture(TEXT("MVP-LevelUp.png"));
            Advance(3);
            return;
        }
        AMVPGem* Nearest = nullptr;
        double BestDistance = TNumericLimits<double>::Max();
        for (TActorIterator<AMVPGem> It(GetWorld()); It; ++It)
        {
            if (!It->bActive) continue;
            const double Distance = FVector::DistSquared(It->GetActorLocation(), ArenaPlayer->GetActorLocation());
            if (Distance < BestDistance) { Nearest = *It; BestDistance = Distance; }
        }
        if (Nearest) ArenaPlayer->AddMovementInput((Nearest->GetActorLocation() - ArenaPlayer->GetActorLocation()).GetSafeNormal2D());
    }
    else if (SmokePhase == 3 && Elapsed > 0.6)
    {
        if (Game->RunSeconds != SmokePausedTime || FVector::DistSquared(ArenaPlayer->GetActorLocation(), SmokeStartPosition) > 1.f)
        { Fail(TEXT("Combat moved while card menu paused")); return; }
        int32 Selection = 0;
        for (int32 Index = 0; Index < 3; ++Index)
            if (!ArenaPlayer->Deck->FindOwnedSpell(ArenaPlayer->Deck->CurrentHand[Index]->CardID)) { Selection = Index; break; }
        const FKey Keys[] = {EKeys::One, EKeys::Two, EKeys::Three};
        InputKey(FInputKeyEventArgs::CreateSimulated(Keys[Selection], IE_Pressed, 1.f));
        Advance(4);
    }
    else if (SmokePhase == 4 && Elapsed > 0.4)
    {
        for (const FKey Key : {EKeys::One, EKeys::Two, EKeys::Three}) InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
        if (Game->bChoosingCard || Game->SelectionWidget || IsPaused() || ArenaPlayer->Deck->ActiveWeapons.Num() != 2)
        { Fail(TEXT("Numeric selection while paused did not equip and resume")); return; }
        const int32 Need = Game->GetXPRequirement();
        Game->AddXP(Need * 2 + 3 - Game->XP);
        if (Game->PendingLevels != 2 || !Game->bChoosingCard) { Fail(TEXT("Multiple level-up queue failed")); return; }
        Advance(5);
    }
    else if (SmokePhase == 5 && Elapsed > 0.3)
    {
        int32 ScreenWidth, ScreenHeight;
        GetViewportSize(ScreenWidth, ScreenHeight);
        const float Scale = FMath::Min(ScreenWidth / 1600.f, ScreenHeight / 900.f);
        auto* OldSelection = Game->SelectionWidget.Get();
        auto* Button = OldSelection ? Cast<UButton>(OldSelection->GetWidgetFromName(TEXT("CardOption_A"))) : nullptr;
        if (!Button || OldSelection->SelectChoice(9)) { Fail(TEXT("Invalid UMG selection accepted or button missing")); return; }
        Button->OnClicked.Broadcast();
        if (OldSelection->SelectChoice(0)) { Fail(TEXT("Consumed UMG selection accepted twice")); return; }
        if (!Game->bChoosingCard || Game->PendingLevels != 1 || !IsPaused())
        { Fail(TEXT("Queued offer did not stay paused")); return; }
        if (!Game->ChooseCard(0) || Game->bChoosingCard || Game->PendingLevels != 0 || IsPaused())
        { Fail(TEXT("Final queued offer did not resume")); return; }
        if (Game->ChooseCard(0)) { Fail(TEXT("Stale selection accepted")); return; }
        Game->Spawner->SpawnWave();
        if (Game->Spawner->GetActiveEnemyCount() == 0) { Fail(TEXT("Camera-aware wave did not spawn")); return; }
        int32 Width, Height;
        GetViewportSize(Width, Height);
        for (TActorIterator<AMVPEnemy> It(GetWorld()); It; ++It)
        {
            if (!It->IsPoolActive()) continue;
            FVector2D Screen;
            if (ProjectWorldLocationToScreen(It->GetActorLocation(), Screen) && Screen.X > 0 && Screen.X < Width && Screen.Y > 0 && Screen.Y < Height)
            { Fail(TEXT("Spawned enemy inside camera view")); return; }
        }
        Advance(6);
    }
    else if (SmokePhase == 6 && Elapsed > 1.0)
    {
        ArenaPlayer->ReceiveContactDamage(1000.f);
        if (!Game->bRunEnded || !IsPaused()) { Fail(TEXT("Death did not stop the run")); return; }
        Capture(TEXT("MVP-GameOver.png"));
        Advance(7);
    }
    else if (SmokePhase == 12 && Elapsed > 0.7)
    {
        for (ASpellBase* Spell : ArenaPlayer->Deck->ActiveWeapons) Spell->StopCasting();
        TSet<int32> Directions;
        int32 Shots=0;
        for (TActorIterator<AMVPProjectile> It(GetWorld()); It; ++It)
            if (It->bActive)
            {
                ++Shots;
                const FVector D=It->GetActorLocation()-ArenaPlayer->GetActorLocation();
                Directions.Add(FMath::RoundToInt(D.Rotation().Yaw/45.f));
            }
        if (Shots!=8 || Directions.Num()!=8) { Fail(TEXT("Nova did not fire eight radial projectiles")); return; }
        Capture(TEXT("MVP-MutatedNova.png"));
        Advance(13);
    }
    else if (SmokePhase == 13 && Elapsed > 0.2)
    {
        for (TActorIterator<AMVPEnemy> It(GetWorld()); It; ++It) if (It->IsPoolActive()) It->ReturnToPool();
        for (TActorIterator<AMVPProjectile> It(GetWorld()); It; ++It) It->Deactivate();
        const FVector Origin=ArenaPlayer->GetActorLocation();
        auto SpawnTarget=[&](FVector Offset)
        {
            auto* E=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(Origin+Offset)));
            if (E) E->SetActorTickEnabled(false);
            return E;
        };
        TArray<AMVPEnemy*> Targets;
        for (int32 I=1; I<=4; ++I) Targets.Add(SpawnTarget(FVector(I*180,0,0)));
        for (auto* E : Targets) if (!E) { Fail(TEXT("Mutation test pool exhausted")); return; }
        auto* Bolt=*TActorIterator<AMVPProjectile>(GetWorld());
        FSpellModifiers Snapshot;
        Snapshot.PiercingCount=2;
        Bolt->Launch(Origin,FVector::ForwardVector,10,Snapshot,FLinearColor::White);
        Bolt->Tick(0.7f);
        if (Targets[0]->Health!=18 || Targets[1]->Health!=18 || Targets[2]->Health!=18 || Targets[3]->Health!=28 || Bolt->bActive)
        { Fail(TEXT("Piercing did not hit exactly three enemies")); return; }
        for (auto* E : Targets) E->ReturnToPool();
        auto* Direct=SpawnTarget(FVector(100,0,0));
        auto* Splash=SpawnTarget(FVector(100,90,0));
        Snapshot.PiercingCount=0;
        Snapshot.ExplodeOnImpact=true;
        Bolt->Launch(Origin,FVector::ForwardVector,10,Snapshot,FLinearColor::White);
        Snapshot.ExplodeOnImpact=false; // In-flight projectile must retain its launch snapshot.
        Bolt->Tick(0.15f);
        if (!Direct || !Splash || Direct->Health!=18 || Splash->Health!=22)
        { Fail(TEXT("Explosion splash/snapshot behavior failed")); return; }
        Direct->ReturnToPool(); Splash->ReturnToPool();
        for (UCardDataAsset* Card : ArenaPlayer->Deck->UnlockedCards)
        {
            if (!ArenaPlayer->Deck->FindOwnedSpell(Card->CardID))
            {
                ArenaPlayer->Deck->EquippedDeck={Card};
                ArenaPlayer->Deck->DiscardHand();
                ArenaPlayer->Deck->DrawLevelUpHand();
                ArenaPlayer->Deck->AddSelectedCardSpell(Card);
            }
            auto* Spell=ArenaPlayer->Deck->FindOwnedSpell(Card->CardID);
            for (const auto Tag : Spell->GetAvailableMutationTags()) Spell->ApplyMutation(Tag,2);
        }
        ArenaPlayer->AddTalismans(AMVPPlayer::MaxTalismans);
        ArenaPlayer->Deck->DiscardHand();
        ArenaPlayer->Deck->EquippedDeck=ArenaPlayer->Deck->UnlockedCards;
        ArenaPlayer->Health=50;
        Game->AddXP(Game->GetXPRequirement());
        if (!Game->bChoosingCard || !ArenaPlayer->Deck->CurrentHand.IsEmpty() || !Game->ChooseCard(0) || IsPaused() || ArenaPlayer->Health!=62)
        { Fail(TEXT("Exhausted mutation library did not offer recovery")); return; }
        Advance(14);
    }
    else if (SmokePhase == 14 && Elapsed > 3.3)
    {
        auto* Guard=Cast<ATalismanGuardSpell>(ArenaPlayer->Deck->FindOwnedSpell(TEXT("TalismanGuard")));
        if(!Guard || Game->BloodDirector) {Fail(TEXT("Energy library contains legacy effects or lacks guard"));return;}
        for(ASpellBase* Spell:ArenaPlayer->Deck->ActiveWeapons) Spell->StopCasting();
        for(TActorIterator<AMVPEnemy> It(GetWorld());It;++It) if(It->IsPoolActive()) It->ReturnToPool();
        ArenaPlayer->ClearTalismans();
        if(ArenaPlayer->AddTalismans(3)!=3) {Fail(TEXT("Initial talisman charges failed"));return;}
        const FVector Position=ArenaPlayer->GetTalismanLocation(0);
        auto* First=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(Position)));
        auto* Second=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(Position)));
        if(!First || !Second) {Fail(TEXT("Talisman test pool exhausted"));return;}
        First->Health=100000;Second->Health=100000;First->SetActorTickEnabled(false);Second->SetActorTickEnabled(false);
        const int32 Kills=Game->Kills;
        ArenaPlayer->UpdateNeonVFX(0.f,true);
        if(Game->Kills!=Kills+1 || ArenaPlayer->GetTalismanCount()!=2 || (First->IsPoolActive()==Second->IsPoolActive()))
        {Fail(TEXT("One talisman must instant kill exactly one high-health foe"));return;}
        First->ReturnToPool();Second->ReturnToPool();
        ArenaPlayer->UpdateNeonVFX(0.f,true);
        if(ArenaPlayer->GetTalismanCount()!=2) {Fail(TEXT("Consumed talisman hit a recycled target"));return;}
        Guard->StartCasting();Guard->Tick(10.f);
        if(ArenaPlayer->GetTalismanCount()!=2) {Fail(TEXT("Talisman unexpectedly recharged on a timer"));return;}
        UCardDataAsset* GuardCard=nullptr;
        for(UCardDataAsset* Card:ArenaPlayer->Deck->UnlockedCards) if(Card->CardID==TEXT("TalismanGuard")) GuardCard=Card;
        ArenaPlayer->Deck->EquippedDeck={GuardCard};ArenaPlayer->Deck->DiscardHand();
        const auto Hand=ArenaPlayer->Deck->DrawLevelUpHand();
        if(Hand.Num()!=1 || Hand[0]!=GuardCard || !ArenaPlayer->Deck->AddSelectedCardSpell(GuardCard) || ArenaPlayer->GetTalismanCount()!=4)
        {Fail(TEXT("Repeat guard draft did not grant two charges"));return;}
        ArenaPlayer->AddTalismans(100);
        if(ArenaPlayer->GetTalismanCount()!=16 || ArenaPlayer->AddTalismans(1)!=0 || !Guard->GetAvailableMutationTags().IsEmpty())
        {Fail(TEXT("Talisman pool cap or exhausted offers failed"));return;}
        ArenaPlayer->ClearTalismans();
        ArenaPlayer->Deck->EquippedDeck=ArenaPlayer->Deck->UnlockedCards;
        // A real temporary WorldStatic collider separates an overlapping card/foe
        // from the player, so this assertion cannot pass merely by missing the foe.
        ArenaPlayer->SetActorLocation(FVector(0,0,40));ArenaPlayer->AddTalismans(1);
        const FVector CardPosition=ArenaPlayer->GetTalismanLocation(0);
        auto* Wall=GetWorld()->SpawnActor<AActor>();
        auto* Collider=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Collider);
        Collider->SetBoxExtent(FVector(5,80,100));Collider->SetCollisionObjectType(ECC_WorldStatic);
        Collider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Collider->SetCollisionResponseToAllChannels(ECR_Block);Collider->RegisterComponent();
        Wall->SetActorLocationAndRotation((CardPosition+ArenaPlayer->GetActorLocation())*0.5f,(CardPosition-ArenaPlayer->GetActorLocation()).Rotation());
        auto* WallTarget=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(CardPosition)));
        if(!WallTarget) {Fail(TEXT("Missing talisman wall probe"));return;}
        WallTarget->SetActorTickEnabled(false);ArenaPlayer->UpdateNeonVFX(0.f,true);
        if(!WallTarget->IsPoolActive() || ArenaPlayer->GetTalismanCount()!=1) {Fail(TEXT("Talisman killed through wall"));return;}
        Wall->Destroy();ArenaPlayer->UpdateNeonVFX(0.f,true);
        if(WallTarget->IsPoolActive() || ArenaPlayer->GetTalismanCount()!=0) {Fail(TEXT("Unoccluded talisman contact did not consume charge"));return;}
        ArenaPlayer->ClearTalismans();
        ArenaPlayer->SetActorLocation(FVector(0,0,40));
        UE_LOG(LogTemp,Display,TEXT("ENERGY_TALISMAN_PASS: instant kill, one charge per victim, repeat draft +2, cap16, no automatic recharge, occlusion"));
        Advance(15);
    }
    else if (SmokePhase == 15 && Elapsed>4.3)
    {
        if (auto* Director=Game->ExplosionDirector.Get())
        {
            const int32 Before=Director->Accepted, Drops=Director->Dropped;
            for (int32 I=0; I<32; ++I) Director->TriggerExplosion(ArenaPlayer->GetActorLocation()+FVector(250,I*12,0),450);
            if (Director->Accepted-Before!=8 || Director->Dropped-Drops!=24)
            { Fail(TEXT("Cinematic explosion pool did not enforce admission cap")); return; }
            TInlineComponentArray<UPointLightComponent*> Lights(Director);
            int32 Shadows=0;
            for (const auto* L : Lights) if (L->IsVisible() && L->CastShadows) ++Shadows;
            if (Shadows>2) { Fail(TEXT("Cinematic shadow light budget exceeded")); return; }
            Capture(TEXT("MVP-CinematicFlash.png"));
        }
        Advance(16);
    }
    else if (SmokePhase == 16 && Elapsed>0.3)
    {
        if (auto* Director=Game->ExplosionDirector.Get())
        {
            TInlineComponentArray<UPointLightComponent*> Lights(Director);
            for (const auto* L : Lights) if (!L->ComponentHasTag(TEXT("FireFlight")) && (L->IsVisible() || L->Intensity>0))
            { Fail(TEXT("Explosion flash did not fade within 0.2 seconds")); return; }
        }
        if (const TCHAR* Error=ValidateTerrainSpells(Game)) { Fail(Error); return; }
        ArenaPlayer->SetActorLocation(FVector(1900,0,340));

        auto* DisplayEnemy=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(FVector(2300,0,340))));
        if(DisplayEnemy) { DisplayEnemy->Health=10000;DisplayEnemy->SetActorTickEnabled(false); }
        for(int32 Tier=1;Tier<=3;++Tier)
        {
            auto* Model=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(FVector(2250,(Tier-2)*220,340))));
            if(Model) { Model->ApplyEnemyTier(Tier);Model->Health=10000;Model->SetActorTickEnabled(false); }
        }
        Game->EmitProjectile(ArenaPlayer->GetActorLocation(),FVector::ForwardVector,1,FSpellModifiers(),FLinearColor(1.f,0.02f,0.5f),EProjectileType::ConcentratedElemental);
        Advance(17);
    }
    else if (SmokePhase == 17 && Elapsed>2.0)
    {
        Game->EmitProjectile(ArenaPlayer->GetActorLocation(),FVector::ForwardVector,1,FSpellModifiers(),FLinearColor(1.f,0.02f,0.5f),EProjectileType::ConcentratedElemental);
        Game->EmitProjectile(ArenaPlayer->GetActorLocation()+FVector(0,160,0),FVector::ForwardVector,1,FSpellModifiers(),FLinearColor(0.12f,0.65f,1.f),EProjectileType::ConcentratedElemental);
        Game->EmitProjectile(ArenaPlayer->GetActorLocation()+FVector(0,-160,0),FVector::ForwardVector,1,FSpellModifiers(),FLinearColor::White);
        Advance(19);
    }
    else if (SmokePhase == 19 && Elapsed>0.14)
    {
        Capture(TEXT("MVP-TerrainSpells.png"));
        Advance(18);
    }
    else if (SmokePhase == 18 && Elapsed>0.6)
    {
        Capture(TEXT("MVP-EnergyArena.png"));
        Advance(20);
    }
    else if (SmokePhase == 20 && Elapsed>0.3)
    {
        ArenaPlayer->bBenchmarkInvulnerable=true;
        ArenaPlayer->SetActorLocation(FVector(300,2200,40));
        NavigationProbe=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(FVector(300,1400,40))));
        if(!NavigationProbe.IsValid()) { Fail(TEXT("Missing navigation chase probe")); return; }
        NavigationProbe->BaseMovementSpeed=900; NavigationProbe->Health=100000;
        Advance(21);
    }
    else if (SmokePhase == 21 && Elapsed>4.0)
    {
        if(!NavigationProbe.IsValid() || FVector::Dist(NavigationProbe->GetActorLocation(),ArenaPlayer->GetActorLocation())>100)
        { Fail(TEXT("Enemy could not physically follow path around wall")); return; }
        NavigationProbe->ReturnToPool();
        UE_LOG(LogTemp,Display,TEXT("TERRAIN_CHASE_PASS"));
        Game->FinishRun();
        for (TActorIterator<ASkeletonMinion> It(GetWorld()); It; ++It)
            if (It->IsActive() || It->IsActorTickEnabled()) { Fail(TEXT("Minions survived run shutdown")); return; }
        auto* Progress = GetWorld()->SpawnActor<AMortisProgressionManager>();
        Progress->TargetKillCount=2; Progress->SessionDuration=10;
        Progress->OnEnemyKilled();
        if (Progress->CurrentKillCount!=0 || !Progress->BeginSession(Game->Spawner)) { Fail(TEXT("Progression start failed")); return; }
        Progress->OnEnemyKilled(); Progress->OnEnemyKilled(); Progress->OnEnemyKilled();
        if (Progress->CurrentKillCount!=2 || Progress->MapState!=EMortisMapState::Cleared || Progress->CommitRewardAndTravel(nullptr))
        { Fail(TEXT("Progression clear latch/invalid reward failed")); return; }
        Progress->Destroy();
        Progress=GetWorld()->SpawnActor<AMortisProgressionManager>();
        Progress->SessionDuration=1; Progress->BeginSession(Game->Spawner); Progress->Tick(2);
        if (Progress->MapState!=EMortisMapState::Expired || Progress->RemainingSessionTime!=0) { Fail(TEXT("Progression timeout failed")); return; }
        Progress->Destroy();
        UGameplayStatics::DeleteGameInSlot(TEXT("CodexPreferences_Automation"),0);
        UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);
        UE_LOG(LogTemp, Display, TEXT("MVP_SMOKE_PASS: Codex, gameplay, mutations, energy cards, kinetic projectiles, consumable talisman guards, repeat drafts, pool caps, animation rotation"));
        bSmokeTest=false;
        FPlatformMisc::RequestExit(false);
    }
    else if (SmokePhase == 7 && Elapsed > 0.5)
    {
        bAwaitingRestart = true;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::R, IE_Pressed, 1.f));
        Advance(8);
    }
#endif
}
