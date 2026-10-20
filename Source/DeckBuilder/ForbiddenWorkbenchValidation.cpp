#include "MVPGame.h"
#include "ForbiddenWorkbenchLayout.h"
#include "ForbiddenRiderRoster.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenProfile.h"
#include "ForbiddenPrograms.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

void InstallWorkbenchValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || !FParse::Param(FCommandLine::Get(),TEXT("WorkbenchSmoke"))) return;
    struct FState {int32 Phase=0;double Since=FPlatformTime::Seconds();};auto S=MakeShared<FState>();
    const bool Reload=FParse::Param(FCommandLine::Get(),TEXT("WorkbenchReload"));const TWeakObjectPtr<AMVPController> Weak(PC);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak,Reload](float)
    {
        using namespace ForbiddenWorkbench;
        auto* C=Weak.Get();if(!C) return false;auto* G=C->GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();auto* H=Cast<AMVPHUD>(C->GetHUD());
        if(!G || !H || !G->GetArenaPlayer()) return true;
        auto* Profile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return true;
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("Review/Workbench");IFileManager::Get().MakeDirectory(*Folder,true);
        auto End=[&](bool Pass,const FString& Detail)
        {
            const FString Json=FString::Printf(TEXT("{\"result\":\"%s\",\"detail\":\"%s\",\"rider\":\"%s\",\"preset\":%d,\"draft\":%d,\"starters\":%d}"),Pass ? TEXT("WORKBENCH_PASS") : TEXT("WORKBENCH_FAIL"),*Detail,*ForbiddenRiders::Selected(C).ToString(),Profile->Data->SelectedDeck,G->DraftPool.Num(),G->StartingCards.Num());
            FFileHelper::SaveStringToFile(Json,*(Folder/(Reload ? TEXT("reload.json") : TEXT("report.json"))));FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
        };
        int32 W,VH;C->GetViewportSize(W,VH);const float Scale=FMath::Min(W/1600.f,VH/900.f);
        auto Click=[&](float X,float Y){H->ClickAt((W-1600*Scale)*.5f+X*Scale,(VH-900*Scale)*.5f+Y*Scale);};
        auto Move=[&](float X,float Y){C->SetMouseLocation(FMath::RoundToInt((W-1600*Scale)*.5f+X*Scale),FMath::RoundToInt((VH-900*Scale)*.5f+Y*Scale));};
        auto HasCopy=[&](const FString& Copy){bool Found=false;if(G->DecisionWidget && G->DecisionWidget->WidgetTree) G->DecisionWidget->WidgetTree->ForEachWidget([&](UWidget* Widget){if(const auto* Text=Cast<UTextBlock>(Widget);Text && Text->GetText().ToString().Contains(Copy)) Found=true;});return Found;};
        auto Next=[&](int32 P){S->Phase=P;S->Since=FPlatformTime::Seconds();};const double Age=FPlatformTime::Seconds()-S->Since;
        if(Age>45) return End(false,TEXT("Phase timed out"));
        if(S->Phase==0 && Age>4)
        {
            if(G->GetLibraryCards().Num()<16 || G->DraftPool.IsEmpty()) return true;
            if(!G->bInCodex || G->bCharacterSelectionOpen || G->RiderPreview || !C->IsPaused()) return End(false,TEXT("Initial unified menu state failed"));
            if(Reload) return End(ForbiddenRiders::Selected(C)=="Mira" && Profile->Data->SelectedDeck==2 && G->StartingCards==TArray<FName>({"SatelliteChoir","TalismanGuard"}) && Profile->Data->CodeFragments==0 && Profile->Data->RiderLoadouts.FindChecked("NeonExorcist").UnlockedStarters.Contains("OrbitRazor") && !Profile->IsUnlocked("OrbitRazor"),TEXT("Rider-specific decks/unlocks and fragment wallet restored in fresh process"));
            if(!Profile->SelectRider("NeonExorcist")) return End(false,TEXT("Could not select QA rider"));
            Profile->Data->UnlockedPrograms={"PulseDart","PrismBurst","KineticCards","HaloNova"};
            for(auto& Deck:Profile->Data->Decks) {Deck.DraftPool=UForbiddenProfile::ProgramIDs();Deck.Starters={"PulseDart","PrismBurst"};}
            Profile->Data->SelectedDeck=0;Profile->Data->CodeFragments=0;
            if(!Profile->Save()) return End(false,TEXT("Could not initialize isolated QA profile"));G->LoadStartingDeck();
            if(H->GetWorkbenchCards(G).Num()!=16 || G->StartingCards.Num()!=2 || G->DraftPool.Num()!=16) return End(false,TEXT("Full draft catalog or starters invalid"));
            Move(20,20);Next(10);
        }
        else if(S->Phase==10 && Age>.5) {FScreenshotRequest::RequestScreenshot(Folder/TEXT("Workbench-Deck.png"),true,false);Next(11);}
        else if(S->Phase==11 && Age>.25) {Move(CardX(0)+100,CardY(0)+75);Next(12);}
        else if(S->Phase==12 && Age>.6) {FScreenshotRequest::RequestScreenshot(Folder/TEXT("Workbench-Card-Hover.png"),true,false);Next(13);}
        else if(S->Phase==13 && Age>.25) {Move(RiderX+90,RiderY+RiderPitch+25);Next(14);}
        else if(S->Phase==14 && Age>.6) {FScreenshotRequest::RequestScreenshot(Folder/TEXT("Workbench-Rider-Hover.png"),true,false);Next(15);}
        else if(S->Phase==15 && Age>.25) {Move(20,20);Next(1);}
        else if(S->Phase==1 && Age>.5)
        {
            // Main menu clicks toggle starters directly and immediately persist incomplete edits.
            Click(CardX(0)+30,CardY(0)+35);
            if(G->DecisionWidget || G->StartingCards.Num()!=1 || Profile->GetDeck().Starters!=G->StartingCards) return End(false,TEXT("Click-to-remove did not autosave"));
            if(G->StartSelectedRun() || !G->bInCodex) return End(false,TEXT("Incomplete preset started a run"));
            Click(PresetX+PresetPitch+40,PresetY+15);if(G->StartingCards.Num()!=2) return End(false,TEXT("Second preset did not retain starters"));
            Click(PresetX+40,PresetY+15);if(G->StartingCards.Num()!=1) return End(false,TEXT("Incomplete preset lost on switching"));
            Click(CardX(0)+30,CardY(0)+35);if(G->StartingCards.Num()!=2) return End(false,TEXT("Click-to-add failed"));
            const auto Before=G->StartingCards;Click(CardX(2)+30,CardY(2)+35);
            if(G->StartingCards!=Before || G->DecisionWidget) return End(false,TEXT("Third starter replaced a card or opened an editor"));
            Click(NextX+20,PageY+12);if(G->LibraryPage!=1) return End(false,TEXT("Library pagination failed"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Workbench-Library.png"),true,false);Next(2);
        }
        else if(S->Phase==2 && Age>.5)
        {
            // Find a known locked card rather than assuming collection ordering.
            const auto All=H->GetWorkbenchCards(G);const int32 Index=All.IndexOfByPredicate([](const UCardDataAsset* Card){return Card->CardID=="OrbitRazor";});
            if(Index==INDEX_NONE) return End(false,TEXT("Locked card missing from collection"));G->LibraryPage=Index/8;
            Click(CardX(Index%8)+30,CardY(Index%8)+35);
            if(!G->DecisionWidget || !HasCopy(TEXT("YOU HAVE")) || !HasCopy(TEXT("already available"))) return End(false,TEXT("Unlock confirmation did not explain starting unlock vs draft"));
            const FName Before=ForbiddenRiders::Selected(C);Click(RiderX+50,RiderY+4*RiderPitch+20);if(ForbiddenRiders::Selected(C)!=Before) return End(false,TEXT("Modal click leaked"));
            Profile->Data->CodeFragments=ForbiddenPrograms::UnlockCost("OrbitRazor");
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Workbench-Unlock.png"),true,false);Next(20);
        }
        else if(S->Phase==20 && Age>.5)
        {
            G->DecisionWidget->Select(0);
            if(!Profile->IsUnlocked("OrbitRazor") || Profile->Data->CodeFragments!=0 || G->StartingCards.Contains("OrbitRazor") || G->DecisionWidget) return End(false,TEXT("Unlock spent incorrectly or auto-equipped the card"));
            if(Profile->Data->RiderLoadouts.FindChecked("Mira").UnlockedStarters.Contains("OrbitRazor")) return End(false,TEXT("Unlock leaked to another rider"));
            Click(TabsX+2*TabPitch+30,TabsY+15);if(G->SchoolFilter!=7 || G->LibraryPage!=0) return End(false,TEXT("School selection failed"));
            for(UCardDataAsset* Card:H->GetWorkbenchCards(G)) if(Card->MagicSchool!=EMagicSchool::Prism) return End(false,TEXT("School filter leaked"));
            Click(TabsX+30,TabsY+15);
            Click(RiderX+50,RiderY+4*RiderPitch+20);if(ForbiddenRiders::Selected(C)!="Mira" || Profile->Data->SelectedDeck!=0) return End(false,TEXT("Rider did not load its own presets"));
            Click(PresetX+2*PresetPitch+30,PresetY+15);
            // Reset only Mira deck 3 in this isolated profile, then use actual toggles.
            auto Deck=Profile->GetDeck();Deck.Starters={"SatelliteChoir","GhostLink"};if(!Profile->SaveDeck(2,Deck)) return End(false,TEXT("Mira setup failed"));G->LoadStartingDeck();
            if(!G->ToggleStartingCard("GhostLink") || !G->ToggleStartingCard("TalismanGuard")) return End(false,TEXT("Mira starter edit failed"));
            Click(PresetX+30,PresetY+15);if(Profile->Data->SelectedDeck!=0) return End(false,TEXT("Preset 1 failed"));
            Click(PresetX+2*PresetPitch+30,PresetY+15);if(G->StartingCards!=TArray<FName>({"SatelliteChoir","TalismanGuard"})) return End(false,TEXT("Mira deck 3 did not persist"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Workbench-Saved.png"),true,false);Next(3);
        }
        else if(S->Phase==3 && Age>.6)
        {
            Click(EnterX+70,ActionY+17);if(G->bInCodex || C->IsPaused()) return End(false,TEXT("Run start failed"));
            UCodexDeckComponent* D=G->GetArenaPlayer()->Deck;if(D->EquippedDeck.Num()!=16 || D->ActiveWeapons.Num()!=2) return End(false,TEXT("Run draft did not include all 16 with two starting weapons"));
            G->Spawner->StopWaves();TSet<FName> Seen;
            for(int32 N=0;N<256;++N)
            {
                D->DiscardHand();const auto Hand=D->DrawLevelUpHand();TSet<FName> Unique;
                for(UCardDataAsset* Card:Hand) {Unique.Add(Card->CardID);Seen.Add(Card->CardID);}
                if(Hand.Num()!=3 || Unique.Num()!=3) return End(false,TEXT("Draft did not offer three unique choices"));
            }
            if(Seen.Num()!=16) return End(false,TEXT("Locked starters were excluded from RNG drafts"));D->DiscardHand();
            G->AddXP(G->GetXPRequirement());
            if(!G->bChoosingCard || !C->IsPaused() || D->CurrentHand.Num()!=3 || !G->ChooseCard(0) || C->IsPaused()) return End(false,TEXT("Essence level-up did not apply a choice and restore combat"));
            return End(true,TEXT("Direct toggle autosave; incomplete preset persistence; two-card cap; rider/preset/unlock isolation; purchase without equip; 256 full-catalog unique RNG hands; actual essence draft verified"));
        }
        return true;
    }));
}
