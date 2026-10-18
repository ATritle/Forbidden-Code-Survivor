#include "MVPGame.h"
#include "MortisTestingDirector.h"
#include "NeonTestingWidget.h"
#include "NeonSettingsWidget.h"
#include "TestingCardInfo.h"
#include "NeonSpellImpactDirector.h"
#include "NeonArsenal.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

void RunTestingMultiSmoke(AMVPController* C)
{
#if !UE_BUILD_SHIPPING
    static int32 Phase=0;static double Since=FPlatformTime::Seconds();static TWeakObjectPtr<AMVPPlayer> Original;
    static int32 BeforePulse=0;
    auto* G=Cast<AMVPArenaGameMode>(C->GetWorld()->GetAuthGameMode());auto* P=Cast<AMVPPlayer>(C->GetPawn());if(!G || !P || !G->Spawner || !G->SpellImpactDirector) return;
    auto* Range=G->TestingDirector.Get();const double Age=FPlatformTime::Seconds()-Since;
    auto Next=[&](int32 Step){Phase=Step;Since=FPlatformTime::Seconds();};
    auto Fail=[&](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("TESTING_MULTI_FAIL phase=%d: %s"),Phase,Why);FPlatformMisc::RequestExitWithStatus(false,1);};
    auto Find=[&](FName Name)->UWidget* {return C->TestingWidget ? C->TestingWidget->WidgetTree->FindWidget(Name) : nullptr;};
    auto Click=[&](FName Name){auto* B=Cast<UButton>(Find(Name));if(!B) return false;B->OnClicked.Broadcast();return true;};
    auto SetAttacks=[&](const TArray<FName>& IDs)
    {
        for(int32 I=0;I<P->Deck->UnlockedCards.Num();++I) if(auto* Toggle=Cast<UCheckBox>(Find(*FString::Printf(TEXT("AttackToggle%d"),I)))) Toggle->SetIsChecked(IDs.Contains(P->Deck->UnlockedCards[I]->CardID));
    };
    auto Modal=[&](){auto* W=Find(TEXT("ChainSpacingWarning"));return W && W->GetVisibility()==ESlateVisibility::Visible;};
    auto Capture=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false);};
    if(Age>20) {Fail(TEXT("Timeout"));return;}
    if(Phase==0)
    {
        Original=P;C->ToggleSettings();C->OpenTestingArea();if(!C->TestingWidget) {Fail(TEXT("Missing multi-attack panel"));return;}
        for(int32 I=0;I<16;++I)
        {
            auto* Toggle=Cast<UCheckBox>(Find(*FString::Printf(TEXT("AttackToggle%d"),I)));
            if(!Toggle || !Toggle->GetToolTip() || !TestingCardInfo::Describe(P->Deck->UnlockedCards[I]).Contains(P->Deck->UnlockedCards[I]->Name.ToString())) {Fail(TEXT("Missing checkbox hover description"));return;}
        }
        const TArray<FName> Chains={TEXT("ArcRelay"),TEXT("GhostLink"),TEXT("RicochetSeal")};
        const FString Warning=TestingCardInfo::SpacingWarning(Chains,300);
        if(!Warning.Contains(TEXT("GHOST LINK")) || Warning.Contains(TEXT("ARC RELAY")) || Warning.Contains(TEXT("RICOCHET SEAL")) || !TestingCardInfo::SpacingWarning(Chains,220).IsEmpty() || TestingCardInfo::SpacingWarning({TEXT("ArcRelay")},320).IsEmpty() || !TestingCardInfo::SpacingWarning({TEXT("KineticCards")},500).IsEmpty()) {Fail(TEXT("Warning did not match actual chain ranges"));return;}
        Click(TEXT("ClearAllAttacks"));Click(TEXT("StartTesting"));
        if(G->TestingDirector || !C->TestingWidget || !C->IsPaused()) {Fail(TEXT("Empty attack selection was accepted"));return;}
        SetAttacks({TEXT("PulseDart"),TEXT("ArcRelay"),TEXT("GhostLink"),TEXT("RicochetSeal")});
        for(int32 I=0;I<10;++I) Cast<UCheckBox>(Find(*FString::Printf(TEXT("EnemyToggle%d"),I)))->SetIsChecked(I==9);
        Cast<USpinBox>(Find(TEXT("GridSpacing")))->SetValue(400);Click(TEXT("StartTesting"));
        if(!Modal() || G->TestingDirector || C->GetPawn()!=Original.Get() || !G->bInCodex || !C->IsPaused()) {Fail(TEXT("Distant chains were applied before confirmation"));return;}
        Next(1);
    }
    else if(Phase==1 && Age>0.5)
    {
        Capture(TEXT("TestingMulti-ChainWarning.png"));Next(2);
    }
    else if(Phase==2 && Age>0.25)
    {
        Click(TEXT("CancelChainSpacing"));if(Modal() || G->TestingDirector) {Fail(TEXT("Cancel changed the session"));return;}
        Cast<USpinBox>(Find(TEXT("GridSpacing")))->SetValue(300);Click(TEXT("StartTesting"));
        auto* Text=Cast<UTextBlock>(Find(TEXT("ChainWarningText")));
        if(!Modal() || !Text || !Text->GetText().ToString().Contains(TEXT("GHOST LINK")) || Text->GetText().ToString().Contains(TEXT("ARC RELAY"))) {Fail(TEXT("Edited spacing reused a stale confirmation"));return;}
        Click(TEXT("ConfirmChainSpacing"));Range=G->TestingDirector.Get();P=Cast<AMVPPlayer>(C->GetPawn());
        if(!Range || !Range->IsActive() || Range->SelectedCards.Num()!=4 || P->Deck->ActiveWeapons.Num()!=4 || Range->GridSpacing!=300 || C->IsSettingsOpen() || C->IsPaused()) {Fail(TEXT("Confirmed combination did not start"));return;}
        Next(3);
    }
    else if(Phase==3 && Age>3.5)
    {
        auto* FX=G->SpellImpactDirector.Get();
        for(int32 Style:{0,6,7,10}) if(FX->ConfirmedHits[Style]<=0) {Fail(TEXT("A selected simultaneous attack dealt no damage"));return;}
        C->SetMouseLocation(100,766);Next(4);
    }
    else if(Phase==4 && Age>0.4)
    {
        auto* HUD=Cast<AMVPHUD>(C->GetHUD());if(!HUD || HUD->GetHoveredTestingCard()!=TEXT("PulseDart")) {Fail(TEXT("In-range name hover did not show its stats"));return;}
        Capture(TEXT("TestingMulti-HoverStats.png"));Next(5);
    }
    else if(Phase==5 && Age>0.25)
    {
        C->ToggleSettings();C->OpenTestingArea();int32 Checked=0;
        for(int32 I=0;I<16;++I) if(Cast<UCheckBox>(Find(*FString::Printf(TEXT("AttackToggle%d"),I)))->IsChecked()) ++Checked;
        if(Checked!=4) {Fail(TEXT("Reopening lost the selected combination"));return;}
        Click(TEXT("SelectAllAttacks"));Cast<USpinBox>(Find(TEXT("GridSpacing")))->SetValue(400);Click(TEXT("StartTesting"));
        if(!Modal() || Range->SelectedCards.Num()!=4 || P->Deck->ActiveWeapons.Num()!=4) {Fail(TEXT("Pending all-attacks setup changed the running loadout"));return;}
        Click(TEXT("CancelChainSpacing"));Cast<USpinBox>(Find(TEXT("GridSpacing")))->SetValue(220);Click(TEXT("StartTesting"));
        if(!Range->IsActive() || Range->SelectedCards.Num()!=16 || P->Deck->ActiveWeapons.Num()!=16 || C->IsPaused()) {Fail(TEXT("Select All did not equip sixteen attacks at safe spacing"));return;}
        Next(6);
    }
    else if(Phase==6 && Age>1.0)
    {
        for(UCardDataAsset* Card:P->Deck->EquippedDeck)
        {
            auto* Spell=P->Deck->FindOwnedSpell(Card->CardID);
            if(!Spell || Spell->Damage!=Card->BaseDamage || Spell->Cooldown!=Card->Cooldown) {Fail(TEXT("Combined testing changed card base tuning"));return;}
        }
        Capture(TEXT("TestingMulti-AllAttacks.png"));Next(7);
    }
    else if(Phase==7 && Age>0.25)
    {
        C->ExitTestingArea();if(C->GetPawn()!=Original.Get() || !G->bInCodex || !C->IsPaused()) {Fail(TEXT("Combined testing did not restore the original menu"));return;}
        for(int32 I=0;I<256;++I) G->EmitProjectile(Original->GetActorLocation(),FVector::ForwardVector,11,FSpellModifiers(),FLinearColor::Green,EProjectileType::ConcentratedElemental,false,ENeonSpellStyle::Pulse);
        int32 Active=0;for(TActorIterator<AMVPProjectile> It(C->GetWorld());It;++It) if(It->bActive) ++Active;
        if(Active!=256) {Fail(TEXT("Could not saturate the original projectile pool"));return;}
        C->ToggleSettings();TArray<int32> Counts;Counts.Init(0,10);Counts[9]=5;
        BeforePulse=G->SpellImpactDirector->ConfirmedHits[0];
        if(!C->StartTestingSession(TEXT("PulseDart"),Counts,true)) {Fail(TEXT("Isolated projectile test did not start"));return;}Next(8);
    }
    else if(Phase==8 && Age>1.0)
    {
        if(Range->TotalDamage<=0 || G->SpellImpactDirector->ConfirmedHits[0]<=BeforePulse) {Fail(TEXT("Original projectiles starved the practice pool"));return;}
        C->ExitTestingArea();int32 Active=0;for(TActorIterator<AMVPProjectile> It(C->GetWorld());It;++It) if(It->bActive) {++Active;It->Deactivate();}
        if(Active!=256 || C->GetPawn()!=Original.Get()) {Fail(TEXT("Practice consumed original paused projectiles"));return;}
        UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);
        UE_LOG(LogTemp,Display,TEXT("TESTING_MULTI_PASS: sixteen attack checkboxes/tooltips; four simultaneous damaging attacks; correct per-card ranges and strict boundary warning; explicit confirm/cancel; changed spacing revalidation; safe Select All with sixteen base weapons; HUD hover stats; original session restored; independent projectile pool under original 256-shot saturation"));FPlatformMisc::RequestExit(false);Next(99);
    }
#endif
}
