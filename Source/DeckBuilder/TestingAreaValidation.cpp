#include "MVPGame.h"
#include "MortisTestingDirector.h"
#include "NeonSettingsWidget.h"
#include "NeonTestingWidget.h"
#include "NeonSpellImpactDirector.h"
#include "KineticCardVFXDirector.h"
#include "CinematicExplosionDirector.h"
#include "CyberEnemyRoster.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "CodexCardSelectionWidget.h"
#include "EnemySpawner.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/SpinBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "NiagaraComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

void RunTestingAreaSmoke(AMVPController* C)
{
#if !UE_BUILD_SHIPPING
    static int32 Phase=0,CardIndex=0;static double Since=FPlatformTime::Seconds();static bool Restarted=false;
    static TWeakObjectPtr<AMVPPlayer> Original;static TWeakObjectPtr<AEnemySpawner> OriginalPool;static TWeakObjectPtr<AMVPEnemy> RunEnemy;
    static FVector EnemyPosition;static float RunClock=0;static TArray<FName> SavedHand;
    auto* G=Cast<AMVPArenaGameMode>(C->GetWorld()->GetAuthGameMode());auto* P=Cast<AMVPPlayer>(C->GetPawn());if(!G || !P || !G->Spawner || !G->SpellImpactDirector) return;
    auto* FX=G->SpellImpactDirector.Get();auto* Range=G->TestingDirector.Get();const double Age=FPlatformTime::Seconds()-Since;
    auto Next=[&](int32 Step){Phase=Step;Since=FPlatformTime::Seconds();};
    auto Fail=[&](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("TESTING_AREA_FAIL phase=%d card=%d: %s"),Phase,CardIndex,Why);FPlatformMisc::RequestExitWithStatus(false,1);};
    auto Capture=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false);};
    auto Click=[&](UUserWidget* W,FName Name){auto* B=W ? Cast<UButton>(W->WidgetTree->FindWidget(Name)) : nullptr;if(!B) return false;B->OnClicked.Broadcast();return true;};
    if(Age>20) {Fail(TEXT("Timeout"));return;}
    if(Phase==0)
    {
        if(Restarted)
        {
            if(!FMath::IsNearlyEqual(FX->GetImpactIntensity(),0.63f) || !G->bInCodex || !C->IsPaused()) {Fail(TEXT("Intensity did not survive world restart"));return;}
            UGameplayStatics::DeleteGameInSlot(TEXT("CodexVFX_Automation"),0);UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);UGameplayStatics::DeleteGameInSlot(TEXT("CodexAudio_Automation"),0);
            UE_LOG(LogTemp,Display,TEXT("TESTING_AREA_PASS: live persisted 0–150%% intensity; shared/card/cinematic spark controls; zero-density hit cues; actual settings UI; ten static tier grids; sixteen individual damaging cards; real HP and damage numbers; bounded 200 targets; reset/reboot; original run, waves, deck mutations and draft pause restored"));FPlatformMisc::RequestExit(false);Next(99);return;
        }
        Original=P;OriginalPool=G->Spawner;C->ToggleSettings();
        auto* Slider=C->SettingsWidget ? Cast<USlider>(C->SettingsWidget->WidgetTree->FindWidget(TEXT("ImpactIntensitySlider"))) : nullptr;
        if(!Slider || Slider->GetMaxValue()!=1.5f) {Fail(TEXT("Missing intensity slider"));return;}
        Slider->SetValue(0.63f);Slider->OnValueChanged.Broadcast(0.63f);if(!FMath::IsNearlyEqual(FX->GetImpactIntensity(),0.63f)) {Fail(TEXT("Slider not bound"));return;}
        Next(1);
    }
    else if(Phase==1 && Age>0.7)
    {
        FX->LoadVFXSettings();if(!FMath::IsNearlyEqual(FX->GetImpactIntensity(),0.63f)) {Fail(TEXT("Paused automatic save failed"));return;}
        Capture(TEXT("Settings-ImpactIntensity.png"));Next(2);
    }
    else if(Phase==2 && Age>0.25)
    {
        if(!Click(C->SettingsWidget,TEXT("TestingArea")) || !C->TestingWidget) {Fail(TEXT("Testing settings button failed"));return;}
        for(int32 I=0;I<16;++I) if(!Cast<UCheckBox>(C->TestingWidget->WidgetTree->FindWidget(*FString::Printf(TEXT("AttackToggle%d"),I)))) {Fail(TEXT("Attack checkboxes do not contain all sixteen cards"));return;}
        for(int32 I=0;I<10;++I)
        {
            auto* Toggle=Cast<UCheckBox>(C->TestingWidget->WidgetTree->FindWidget(*FString::Printf(TEXT("EnemyToggle%d"),I)));
            auto* Count=Cast<USpinBox>(C->TestingWidget->WidgetTree->FindWidget(*FString::Printf(TEXT("EnemyCount%d"),I)));
            if(!Toggle || !Count || !Toggle->IsChecked() || Count->GetDelta()!=5 || !Count->GetAlwaysUsesDeltaSnap()) {Fail(TEXT("Enemy toggle/count control missing"));return;}
        }
        Capture(TEXT("Settings-TestingArea.png"));Next(3);
    }
    else if(Phase==3 && Age>0.25)
    {
        if(!Click(C->TestingWidget,TEXT("StartTesting")) || !G->TestingDirector || !G->TestingDirector->IsActive() || C->IsPaused() || C->IsSettingsOpen()) {Fail(TEXT("Could not enter from main menu"));return;}
        Next(4);
    }
    else if(Phase==4 && Age>0.8)
    {
        if(!Range || Range->GetTargetCount()!=50 || G->Kills!=0 || G->XP!=0 || G->RunSeconds!=0 || P==Original.Get()) {Fail(TEXT("Main session was changed"));return;}
        int32 Types[10]={};for(const auto& T:Range->GetTargets())
        {
            ++Types[T.Archetype];if(T.bAlive && (!T.Enemy.IsValid() || T.Enemy->IsActorTickEnabled() || FVector::DistSquared(T.Position,T.Enemy->GetActorLocation())>1 || T.Enemy->MaxHealth!=CyberEnemies::Roster()[T.Archetype].Health)) {Fail(TEXT("Target moved or tier HP was wrong"));return;}
        }
        for(int32 Count:Types) if(Count!=5) {Fail(TEXT("Roster distribution incorrect"));return;}
        Capture(TEXT("TestingArea-AllTiers.png"));Next(5);
    }
    else if(Phase==5 && Age>0.25)
    {
        for(ASpellBase* Weapon:P->Deck->ActiveWeapons) Weapon->StopCasting();
        FX->SetImpactIntensity(0);const int32 Before=FX->Accepted,Pulses=FX->VisiblePulses;FX->PlayImpact(ENeonSpellStyle::Pulse,P->GetActorLocation());
        if(FX->Accepted!=Before || FX->VisiblePulses!=Pulses+1) {Fail(TEXT("Zero density suppressed hit cues or still spawned sparks"));return;}
        FX->SetImpactIntensity(0.5f);FX->Tick(2.f);FX->PlayImpact(ENeonSpellStyle::Pulse,P->GetActorLocation());
        auto HasCount=[](AActor* Director,int32 Count)
        {
            TArray<UNiagaraComponent*> Components;Director->GetComponents(Components);
            // Niagara can compile asynchronously on its first activation; inspect the submitted override.
            for(auto* Component:Components) {bool Valid=false;const int32 Actual=Component->GetVariableInt(TEXT("User.SparkCount"),Valid);if(Valid && Actual==Count) return true;}return false;
        };
        if(!HasCount(FX,162)) {Fail(TEXT("Shared burst did not scale to fifty percent"));return;}
        if(!G->CardVFXDirector->PlayImpact(P->GetActorLocation()) || !HasCount(G->CardVFXDirector,18)) {Fail(TEXT("Card shards did not scale"));return;}
        if(!G->ExplosionDirector->TriggerExplosion(P->GetActorLocation(),100,true) || !HasCount(G->ExplosionDirector,60)) {Fail(TEXT("Cinematic sparks did not scale"));return;}
        FX->SetImpactIntensity(0.63f);
        FString Error;TArray<int32> Invalid;Invalid.Init(0,10);Invalid[0]=7;
        if(Range->Configure(TEXT("PulseDart"),Invalid,true,Error)) {Fail(TEXT("Accepted a non-five count"));return;}
        Invalid.Init(50,10);if(Range->Configure(TEXT("PulseDart"),Invalid,true,Error)) {Fail(TEXT("Accepted more than 200 enemies"));return;}
        CardIndex=0;Next(6);
    }
    else if(Phase==6)
    {
        C->ToggleSettings();TArray<int32> Counts;Counts.Init(0,10);Counts[0]=5;
        UCardDataAsset* Card=P->Deck->UnlockedCards[CardIndex];
        if(!C->StartTestingSession(Card->CardID,Counts,true)) {Fail(TEXT("Individual card setup failed"));return;}
        P=Cast<AMVPPlayer>(C->GetPawn());
        if(P->Deck->ActiveWeapons.Num()!=1 || P->Deck->EquippedDeck.Num()!=1 || P->Deck->ActiveWeapons[0]->Damage!=Card->BaseDamage) {Fail(TEXT("Test did not equip exactly one base card"));return;}
        auto* Weapon=P->Deck->ActiveWeapons[0].Get();Weapon->StopCasting();Weapon->Cooldown=0.18f;Weapon->StartCasting();
        if(Card->CardID==TEXT("TalismanGuard"))
        {
            const FVector Offset=P->GetTalismanLocation(0)-P->GetActorLocation();auto Position=Range->GetTargets()[0].Position-Offset;Position.Z=20055;P->SetActorLocation(Position);
        }
        Next(7);
    }
    else if(Phase==7 && Range->HitCount>0)
    {
        if(Range->TotalDamage<=0 || !Range->GetDamageNumbers().ContainsByPredicate([](const auto& N){return N.Age>=0 && N.Amount>0;})) {Fail(TEXT("Missing real damage numbers"));return;}
        UE_LOG(LogTemp,Display,TEXT("TESTING_CARD %s hits=%d actual_damage=%.2f"),*Range->CardID.ToString(),Range->HitCount,Range->TotalDamage);
        for(ASpellBase* Weapon:P->Deck->ActiveWeapons) Weapon->StopCasting();
        if(++CardIndex<16) Next(6);else Next(8);
    }
    else if(Phase==8)
    {
        C->ToggleSettings();TArray<int32> Counts;Counts.Init(0,10);Counts[0]=Counts[4]=Counts[6]=Counts[9]=50;
        if(!C->StartTestingSession(TEXT("ArcRelay"),Counts,false) || Range->GetTargetCount()!=200) {Fail(TEXT("200-target limit failed"));return;}
        for(ASpellBase* Weapon:P->Deck->ActiveWeapons) Weapon->StopCasting();
        C->ToggleSettings();Counts.Init(0,10);Counts[9]=5;
        if(!C->StartTestingSession(TEXT("ArcRelay"),Counts,true)) {Fail(TEXT("Boss-only toggle selection failed"));return;}
        for(const auto& T:Range->GetTargets()) if(T.Archetype!=9 || T.Enemy->Health!=6000) {Fail(TEXT("Boss-only target stats incorrect"));return;}
        for(ASpellBase* Weapon:P->Deck->ActiveWeapons) Weapon->StopCasting();Range->GetTargets()[0].Enemy->TakeSpellDamage(6000,FVector::ZeroVector);Next(9);
    }
    else if(Phase==9 && Age>1.8)
    {
        const auto& T=Range->GetTargets()[0];if(!T.bAlive || !T.Enemy.IsValid() || T.Enemy->Health!=6000) {Fail(TEXT("Target did not reboot at real max HP"));return;}
        C->ExitTestingArea();if(!G->bInCodex || !C->IsPaused() || C->GetPawn()!=Original.Get() || G->Spawner!=OriginalPool.Get()) {Fail(TEXT("Original main menu was not restored"));return;}
        G->StartingCards={TEXT("PulseDart"),TEXT("PrismBurst")};if(!G->StartSelectedRun()) {Fail(TEXT("Could not start original run"));return;}
        P=Cast<AMVPPlayer>(C->GetPawn());P->Health=64;P->bBenchmarkInvulnerable=true;P->Deck->ActiveWeapons[0]->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.Program.Rank")),1);
        RunEnemy=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(FVector(600,0,55)));RunEnemy->Health=137;EnemyPosition=RunEnemy->GetActorLocation();RunClock=G->RunSeconds;
        C->ToggleSettings();TArray<int32> Counts;Counts.Init(0,10);Counts[0]=5;if(!C->StartTestingSession(TEXT("NeonCataclysm"),Counts,true)) {Fail(TEXT("Run entry failed"));return;}Next(10);
    }
    else if(Phase==10 && Age>1.8)
    {
        if(G->RunSeconds!=RunClock || RunEnemy->Health!=137 || FVector::DistSquared(RunEnemy->GetActorLocation(),EnemyPosition)>1 || OriginalPool->GetActiveEnemyCount()!=1) {Fail(TEXT("Original run advanced inside testing"));return;}
        C->ExitTestingArea();P=Cast<AMVPPlayer>(C->GetPawn());
        if(C->IsPaused() || P!=Original.Get() || P->Health!=64 || P->Deck->ActiveWeapons.Num()!=2 || P->Deck->ActiveWeapons[0]->Modifiers.Rank!=2 || !RunEnemy->IsActorTickEnabled() || RunEnemy->IsHidden()) {Fail(TEXT("Run state or mutation was not restored"));return;}
        for(ASpellBase* Weapon:P->Deck->ActiveWeapons) Weapon->StopCasting();Next(11);
    }
    else if(Phase==11 && Age>1.6)
    {
        if(OriginalPool->GetActiveEnemyCount()<=1 || G->RunSeconds<=RunClock) {Fail(TEXT("Waves or clock did not resume"));return;}
        G->Spawner->StopWaves();G->AddXP(G->GetXPRequirement());if(!G->bChoosingCard) {Fail(TEXT("Could not open original draft"));return;}
        SavedHand.Reset();for(UCardDataAsset* Card:P->Deck->CurrentHand) SavedHand.Add(Card->CardID);
        C->ToggleSettings();TArray<int32> Counts;Counts.Init(0,10);Counts[0]=5;if(!C->StartTestingSession(TEXT("PulseDart"),Counts,true)) {Fail(TEXT("Draft entry failed"));return;}Next(12);
    }
    else if(Phase==12 && Age>0.4)
    {
        C->ExitTestingArea();P=Cast<AMVPPlayer>(C->GetPawn());TArray<FName> Hand;for(UCardDataAsset* Card:P->Deck->CurrentHand) Hand.Add(Card->CardID);
        if(!C->IsPaused() || !G->bChoosingCard || Hand!=SavedHand || !G->SelectionWidget->GetIsEnabled() || G->SelectionWidget->GetVisibility()==ESlateVisibility::Collapsed) {Fail(TEXT("Draft pause or choices were not restored"));return;}
        FX->SetImpactIntensity(0.63f);if(!FX->SaveVFXSettings()) {Fail(TEXT("Could not save VFX preference"));return;}
        Restarted=true;Phase=0;Since=FPlatformTime::Seconds();C->SetPause(false);UGameplayStatics::OpenLevel(C,FName(*UGameplayStatics::GetCurrentLevelName(C)));
    }
#endif
}
