#include "MVPGame.h"
#include "ForbiddenWorkbenchLayout.h"
#include "ForbiddenRiderRoster.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenProfile.h"
#include "CardDataAsset.h"
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
        auto Next=[&](int32 P){S->Phase=P;S->Since=FPlatformTime::Seconds();};const double Age=FPlatformTime::Seconds()-S->Since;
        if(Age>45) return End(false,TEXT("Phase timed out"));
        if(S->Phase==0 && Age>4)
        {
            if(G->GetLibraryCards().Num()<16 || G->DraftPool.IsEmpty()) return true;
            if(!G->bInCodex || G->bCharacterSelectionOpen || G->RiderPreview || !C->IsPaused()) return End(false,TEXT("Initial unified menu state failed"));
            if(Reload) return End(ForbiddenRiders::Selected(C)=="Mira" && Profile->Data->SelectedDeck==2 && G->StartingCards.Contains("TalismanGuard"),TEXT("Rider, preset and modified starters restored in fresh process"));
            if(H->GetWorkbenchCards(G).Num()!=8 || G->StartingCards.Num()!=2) return End(false,TEXT("Initial draft count invalid"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Workbench-Deck.png"),true,false);Next(1);
        }
        else if(S->Phase==1 && Age>.5)
        {
            Click(ViewX+20,ViewY+12);if(H->IsWorkbenchDraftView() || H->GetWorkbenchCards(G).Num()!=16) return End(false,TEXT("Browse library did not expose all programs"));
            Click(NextX+20,PageY+12);if(G->LibraryPage!=1) return End(false,TEXT("Library page navigation failed"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Workbench-Library.png"),true,false);Next(2);
        }
        else if(S->Phase==2 && Age>.5)
        {
            Click(TabsX+2*TabPitch+30,TabsY+15);if(G->SchoolFilter!=7 || G->LibraryPage!=0) return End(false,TEXT("Prism school selection failed"));
            for(UCardDataAsset* Card:H->GetWorkbenchCards(G)) if(Card->MagicSchool!=EMagicSchool::Prism) return End(false,TEXT("School filter leaked other programs"));
            Click(TabsX+30,TabsY+15);Click(ViewX+20,ViewY+12);
            Click(CardX(0)+30,CardY(0)+35);if(!G->DecisionWidget) return End(false,TEXT("Program inspection did not open"));
            const FName Before=ForbiddenRiders::Selected(C);Click(RiderX+50,RiderY+4*RiderPitch+20);
            if(ForbiddenRiders::Selected(C)!=Before) return End(false,TEXT("Modal click leaked into rider selection"));
            G->DecisionWidget->Select(3);if(G->DecisionWidget || !G->bInCodex || !C->IsPaused()) return End(false,TEXT("Program BACK failed to restore combined screen"));
            Click(PresetX+PresetPitch+40,PresetY+15);if(Profile->Data->SelectedDeck!=1) return End(false,TEXT("Preset two click failed"));
            Click(PresetX+2*PresetPitch+40,PresetY+15);if(Profile->Data->SelectedDeck!=2) return End(false,TEXT("Preset three click failed"));
            // Choose the sixth selected program (Talisman Guard) through its real modal.
            const auto Cards=H->GetWorkbenchCards(G);const int32 Guard=Cards.IndexOfByPredicate([](const UCardDataAsset* Card){return Card->CardID=="TalismanGuard";});
            if(Guard==INDEX_NONE) return End(false,TEXT("Default draft omitted Talisman Guard"));
            Click(CardX(Guard)+30,CardY(Guard)+35);if(!G->DecisionWidget) return End(false,TEXT("Starter editor missing"));
            G->DecisionWidget->Select(1);if(!G->DecisionWidget) return End(false,TEXT("Starter replacement missing"));G->DecisionWidget->Select(0);
            if(!G->StartingCards.Contains("TalismanGuard") || !G->DecisionWidget) return End(false,TEXT("Starter replacement failed"));G->DecisionWidget->Select(3);
            Click(PresetX+30,PresetY+15);if(Profile->Data->SelectedDeck!=0 || G->StartingCards.Contains("TalismanGuard")) return End(false,TEXT("Presets leaked modified starters"));
            Click(PresetX+2*PresetPitch+30,PresetY+15);if(!G->StartingCards.Contains("TalismanGuard")) return End(false,TEXT("Preset three did not restore saved starter"));
            Click(RiderX+50,RiderY+4*RiderPitch+20);if(ForbiddenRiders::Selected(C)!="Mira") return End(false,TEXT("Mira row click failed"));
            Click(SaveX+40,ActionY+17);FScreenshotRequest::RequestScreenshot(Folder/TEXT("Workbench-Saved.png"),true,false);Next(3);
        }
        else if(S->Phase==3 && Age>.6)
        {
            Click(EnterX+70,ActionY+17);if(G->bInCodex || C->IsPaused()) return End(false,TEXT("Enter Arena did not start the selected loadout in one click"));
            return End(true,TEXT("Actual rider, modal, library paging, school filter, starter editor, three presets, persistence and one-click run verified"));
        }
        return true;
    }));
}
