#include "MVPGame.h"
#include "ForbiddenScoreService.h"
#include "ForbiddenScoreWidget.h"
#include "ForbiddenProfile.h"
#include "CodexDeckComponent.h"
#include "SpellBase.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"

void InstallLeaderboardValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || !FParse::Param(FCommandLine::Get(),TEXT("LeaderboardSmoke"))) return;
    struct FState {int32 Phase=0;double Since=FPlatformTime::Seconds();};auto S=MakeShared<FState>();const TWeakObjectPtr<AMVPController> Weak(PC);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak](float)
    {
        auto* C=Weak.Get();if(!C) return false;auto* G=C->GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();if(!G || !G->GetArenaPlayer() || G->GetLibraryCards().Num()<16) return true;
        auto* Scores=C->GetGameInstance()->GetSubsystem<UForbiddenScoreService>();const FString Folder=FPaths::ProjectSavedDir()/TEXT("Review/ScoreProfiles");IFileManager::Get().MakeDirectory(*Folder,true);
        auto End=[&](bool Pass,const FString& Detail){FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"result\":\"%s\",\"detail\":\"%s\",\"sampleDataOnly\":true}"),Pass ? TEXT("SCORE_PROFILES_PASS") : TEXT("SCORE_PROFILES_FAIL"),*Detail),*(Folder/TEXT("report.json")));FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;};
        auto Next=[&](int32 Phase){S->Phase=Phase;S->Since=FPlatformTime::Seconds();};const double Age=FPlatformTime::Seconds()-S->Since;if(Age>40) return End(false,TEXT("Phase timed out"));
        if(S->Phase==0 && Age>4)
        {
            Scores->Entries.Reset();FForbiddenScoreEntry E;E.ID=TEXT("example-zero");E.Username=TEXT("ZERO_TRACE");E.Rank=1;E.Score=144420;E.Kills=1120;E.Seconds=642;E.Level=24;E.Contracts=3;E.Victory=true;E.RiderID="Zero";
            const TArray<FName> IDs={"PulseDart","PhotonLance","KineticCards","ArcRelay","HaloNova","OrbitRazor"};const int32 Ranks[]={3,3,3,2,2,1};
            for(int32 I=0;I<IDs.Num();++I) {FForbiddenCardSnapshot Card;Card.CardID=IDs[I];Card.Level=Ranks[I];Card.Evolution=I<2 ? I+1 : 0;E.Cards.Add(Card);}Scores->Entries.Add(E);
            E.ID=TEXT("example-mira");E.Username=TEXT("GHOST_CONDUCTOR");E.Rank=2;E.Score=128000;E.RiderID="Mira";E.Cards.Reset();
            for(FName ID:UForbiddenProfile::ProgramIDs()) {FForbiddenCardSnapshot Card;Card.CardID=ID;E.Cards.Add(Card);}Scores->Entries.Add(E);
            E.ID=TEXT("example-vex");E.Username=TEXT("SIGNAL_RUNNER");E.Rank=3;E.Score=110000;E.RiderID="Vex";E.Cards.SetNum(4);Scores->Entries.Add(E);
            Scores->Status=TEXT("EXAMPLE DATA / Preview only. These scores are not published.");++Scores->Revision;C->OpenLeaderboard();Next(1);
        }
        else if(S->Phase==1 && Age>1)
        {
            if(!C->ScoreWidget || C->ScoreWidget->InspectedIndex!=0 || !C->ScoreWidget->DetailSignature.Contains(TEXT("PhotonLance:3:2"))) return End(false,TEXT("Initial rider and level/evolution binding failed"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Example-Zero.png"),true,false);
            Next(2);
        }
        else if(S->Phase==2 && Age>.3)
        {
            int32 Index=0;bool Found=false;C->ScoreWidget->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* Row=Cast<UForbiddenScoreRowButton>(W)){if(Index++==1){FVector2D Pixel,Viewport;const auto Geometry=Row->GetCachedGeometry();USlateBlueprintLibrary::AbsoluteToViewport(C,Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5f),Pixel,Viewport);C->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));Found=true;}}});
            if(!Found) return End(false,TEXT("No second hover target"));Next(3);
        }
        else if(S->Phase==3 && Age>.8)
        {
            if(C->ScoreWidget->InspectedIndex!=1 || !C->ScoreWidget->DetailSignature.Contains(TEXT("SatelliteChoir:1:0"))) return End(false,TEXT("Real pointer hover did not reveal the second rider and all sixteen cards"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Example-Mira-All16.png"),true,false);Next(4);
        }
        else if(S->Phase==4 && Age>.3)
        {
            int32 Index=0;C->ScoreWidget->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* Row=Cast<UForbiddenScoreRowButton>(W);Row && Index++==2) Row->SetKeyboardFocus();});Next(5);
        }
        else if(S->Phase==5 && Age>.3)
        {
            if(C->ScoreWidget->InspectedIndex!=2 || !C->ScoreWidget->DetailSignature.StartsWith(TEXT("Vex"))) return End(false,TEXT("Keyboard focus did not update rider details"));
            while(Scores->Entries.Num()<10) {auto Entry=Scores->Entries[0];Entry.Rank=Scores->Entries.Num()+1;Entry.Username=FString::Printf(TEXT("SAMPLE_%02d"),Entry.Rank);Entry.Score=100000-Entry.Rank*1000;Scores->Entries.Add(Entry);}++Scores->Revision;Next(51);
        }
        else if(S->Phase==51 && Age>.4)
        {
            int32 RowCount=0;C->ScoreWidget->WidgetTree->ForEachWidget([&](UWidget* W){if(Cast<UForbiddenScoreRowButton>(W)) ++RowCount;});if(RowCount!=10) return End(false,TEXT("Top-ten row presentation failed"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Top-Ten-Example.png"),true,false);Next(52);
        }
        else if(S->Phase==52 && Age>.3)
        {
            Scores->Entries.Reset();++Scores->Revision;Next(6);
        }
        else if(S->Phase==6 && Age>.3)
        {
            if(C->ScoreWidget->InspectedIndex!=INDEX_NONE || !C->ScoreWidget->DetailSignature.IsEmpty()) return End(false,TEXT("Empty board retained stale build data"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Empty-Board.png"),true,false);Next(60);
        }
        else if(S->Phase==60 && Age>.3)
        {
            C->ScoreWidget->Close();
            if(C->ScoreWidget || !C->IsPaused()) return End(false,TEXT("Leaderboard close did not preserve main menu pause"));
            if(!G->SelectRider("Zero") || !G->StartSelectedRun()) return End(false,TEXT("Could not start snapshot capture run"));
            auto* Spell=G->GetArenaPlayer()->Deck->ActiveWeapons[0].Get();Spell->Modifiers.Rank=3;Spell->Modifiers.Evolution=1;G->FinishRun(false);
            if(G->RunRecord.RiderID!="Zero" || G->RunRecord.Cards.Num()!=2 || G->RunRecord.Cards[0].Level!=3 || G->RunRecord.Cards[0].Evolution!=1) return End(false,TEXT("Actual end-run snapshot failed"));
            auto* Profile=C->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile->RecordRun(G->RunRecord,false)) return End(false,TEXT("Isolated history save failed"));
            auto* Saved=Cast<UForbiddenProfileSave>(UGameplayStatics::LoadGameFromSlot(Profile->SlotName(),0));
            if(!Saved || Saved->History.IsEmpty() || Saved->History[0].Cards.Num()!=2 || Saved->History[0].Cards[0].Level!=3 || Saved->History[0].Cards[0].Evolution!=1) return End(false,TEXT("Saved history lost final card state"));
            if(Scores->Eligible || Scores->Submit(TEXT("QA Sample"))) return End(false,TEXT("Validation became eligible for a public score"));
            FForbiddenScoreEntry E;E.ID=TEXT("ended-example");E.Rank=1;E.Username=TEXT("ZERO_TRACE");E.RiderID="Zero";E.Level=24;E.Score=144420;E.Cards=G->RunRecord.Cards;Scores->Entries.Add(E);Scores->Status=TEXT("EXAMPLE DATA / Offline validation. No public submission.");++Scores->Revision;C->OpenLeaderboard();Next(7);
        }
        else if(S->Phase==7 && Age>.7)
        {
            if(!C->ScoreWidget) return End(false,TEXT("End-run submit screen absent"));FScreenshotRequest::RequestScreenshot(Folder/TEXT("End-Run-Submit.png"),true,false);
            C->ScoreWidget->WidgetTree->ForEachWidget([](UWidget* W){if(auto* Name=Cast<UEditableTextBox>(W)) Name->SetKeyboardFocus();});Next(8);
        }
        else if(S->Phase==8 && Age>.3)
        {
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));Next(9);
        }
        else if(S->Phase==9 && Age>.3)
        {
            if(C->ScoreWidget || !C->IsPaused()) return End(false,TEXT("Escape from focused username failed"));
            return End(true,TEXT("Actual row hover and keyboard profiles; all 16 cards; empty-state cleanup; captured and persisted spell levels/evolutions; safe username Escape; no online test submissions"));
        }
        return true;
    }),.1f);
}
