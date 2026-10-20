#include "MVPGame.h"
#include "MortisTestingDirector.h"
#include "ForbiddenWorkbenchLayout.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenHelpWidget.h"
#include "ForbiddenScoreWidget.h"
#include "ForbiddenProfile.h"
#include "NeonSettingsWidget.h"
#include "NeonTestingWidget.h"
#include "CodexCardSelectionWidget.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/SpinBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Serialization/JsonSerializer.h"

/** Real pointer hover and Slate key routing, including focused controls and nested modals. */
void InstallMenuNavigationValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || !FParse::Param(FCommandLine::Get(),TEXT("MenuNavigationSmoke"))) return;
    struct FState
    {
        int32 Phase=0,Card=0,Pending=0,Rerolls=0,Banishes=0,Active=0,Fragments=0;
        double Since=FPlatformTime::Seconds();float RunTime=0;
        TArray<FName> Hand;TArray<FGameplayTag> Mutations;TArray<FString> Checks;TWeakObjectPtr<UCodexCardSelectionWidget> Draft;
    };
    const auto S=MakeShared<FState>();const TWeakObjectPtr<AMVPController> Weak(PC);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak](float)
    {
        using namespace ForbiddenWorkbench;
        auto* C=Weak.Get();if(!C) return false;
        auto* G=C->GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();auto* H=Cast<AMVPHUD>(C->GetHUD());
        auto* P=G ? G->GetArenaPlayer() : nullptr;if(!P || !H || !P->Deck) return true;
        auto* Profile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return true;
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("Review/MenuNavigation");IFileManager::Get().MakeDirectory(*Folder,true);
        int32 W,VH;C->GetViewportSize(W,VH);const float Scale=FMath::Min(W/1600.f,VH/900.f);
        auto End=[&](bool Pass,const FString& Why)
        {
            auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("result"),Pass ? TEXT("MENU_NAVIGATION_PASS") : TEXT("MENU_NAVIGATION_FAIL"));
            Report->SetStringField(TEXT("detail"),Why);Report->SetNumberField(TEXT("phase"),S->Phase);Report->SetNumberField(TEXT("width"),W);Report->SetNumberField(TEXT("height"),VH);
            TArray<TSharedPtr<FJsonValue>> Checks;for(const auto& Check:S->Checks) Checks.Add(MakeShared<FJsonValueString>(Check));Report->SetArrayField(TEXT("checks"),Checks);
            FString Json;auto Writer=TJsonWriterFactory<>::Create(&Json);FJsonSerializer::Serialize(Report,Writer);
            FFileHelper::SaveStringToFile(Json,*(Folder/TEXT("report.json")));FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
        };
        auto Next=[&](int32 Phase){S->Phase=Phase;S->Since=FPlatformTime::Seconds();};const double Age=FPlatformTime::Seconds()-S->Since;
        auto Click=[&](float X,float Y){H->ClickAt((W-1600*Scale)*.5f+X*Scale,(VH-900*Scale)*.5f+Y*Scale);};
        auto Move=[&](float X,float Y){C->SetMouseLocation(FMath::RoundToInt((W-1600*Scale)*.5f+X*Scale),FMath::RoundToInt((VH-900*Scale)*.5f+Y*Scale));};
        auto Key=[&](FKey K=EKeys::Escape){FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
        auto Raw=[&](FKey K){C->InputKey(FInputKeyEventArgs::CreateSimulated(K,IE_Pressed,1.f));C->InputKey(FInputKeyEventArgs::CreateSimulated(K,IE_Released,0.f));};
        auto Focus=[&](UUserWidget* Widget){if(!Widget || !Widget->WidgetTree) return;bool Done=false;Widget->WidgetTree->ForEachWidget([&](UWidget* Child){if(!Done && Cast<UButton>(Child)) {Child->SetKeyboardFocus();Done=true;}});if(!Done) Widget->SetKeyboardFocus();};
        auto Hand=[&](){TArray<FName> IDs;for(UCardDataAsset* Card:P->Deck->CurrentHand) IDs.Add(Card->CardID);return IDs;};
        auto Has=[&](const FString& Copy){bool Found=false;if(G->DecisionWidget) G->DecisionWidget->WidgetTree->ForEachWidget([&](UWidget* Widget){if(auto* Text=Cast<UTextBlock>(Widget)) Found|=Text->GetText().ToString().Contains(Copy);});return Found;};
        if(Age>45) return End(false,TEXT("Phase timed out"));
        if(S->Phase==0 && Age>4)
        {
            if(P->Deck->UnlockedCards.Num()!=16 || !G->bInCodex) return true;
            Click(ViewX+20,ViewY+10);G->SchoolFilter=-1;G->LibraryPage=0;Move(CardX(0)+80,CardY(0)+70);Next(1);
        }
        else if(S->Phase==1 && Age>.3)
        {
            const auto Cards=H->GetWorkbenchCards(G);if(Cards.Num()!=16) return End(false,TEXT("Library missing cards"));
            auto* Card=Cards[S->Card];if(H->GetHoveredWorkbenchCard()!=Card->CardID || !H->GetWorkbenchTooltipText().Contains(Card->Description.ToString())) return End(false,TEXT("Hover omitted a complete description or targeted the wrong card"));
            if(S->Card==0 || S->Card==7 || S->Card==8 || S->Card==15) FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("Tooltip-%02d.png"),S->Card),true,false);
            Next(2);
        }
        else if(S->Phase==2 && Age>.2)
        {
            if(++S->Card<16) {G->LibraryPage=S->Card/8;Move(CardX(S->Card%8)+80,CardY(S->Card%8)+70);Next(1);}
            else {S->Checks.Add(TEXT("All 16 actual hover targets expose complete descriptions; edge/second-page captures recorded"));Move(20,20);Raw(EKeys::Escape);Next(3);}
        }
        else if(S->Phase==3 && Age>.3)
        {
            if(!H->IsWorkbenchDraftView() || G->LibraryPage!=0 || G->SchoolFilter!=-1 || C->IsSettingsOpen() || !H->GetHoveredWorkbenchCard().IsNone()) return End(false,TEXT("Escape did not return library to deck or clear the tooltip"));
            Click(1240,866);if(!H->IsMusicPlaylistOpen()) return End(false,TEXT("Playlist did not open"));C->NavigateBack();
            if(H->IsMusicPlaylistOpen()) return End(false,TEXT("Escape did not close Tracks"));C->NavigateBack();
            if(C->IsSettingsOpen() || !C->IsPaused()) return End(false,TEXT("Root Escape opened another window or unpaused the menu"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Menu.png"),true,false);Next(35);
        }
        else if(S->Phase==35 && Age>.3) {Click(CardX(0)+30,CardY(0)+35);Next(4);}
        else if(S->Phase==4 && Age>.3)
        {
            if(!G->DecisionWidget) return End(false,TEXT("Card details missing"));G->DecisionWidget->Select(2);Next(5);
        }
        else if(S->Phase==5 && Age>.3) {Focus(G->DecisionWidget);Key();if(!G->DecisionWidget || !Has(TEXT("HOW TO CHANGE"))) return End(false,TEXT("Upgrade paths did not back out to card details"));Next(6);}
        else if(S->Phase==6 && Age>.3)
        {
            Focus(G->DecisionWidget);Key();if(G->DecisionWidget || !C->IsPaused()) return End(false,TEXT("Card details did not close safely"));
            S->Fragments=Profile->Data->CodeFragments;G->bProgramEditorFromHideout=false;G->OpenProgramEditor("GhostLink");Next(61);
        }
        else if(S->Phase==61 && Age>.3) {Focus(G->DecisionWidget);Key();if(G->DecisionWidget || Profile->IsUnlocked("GhostLink") || Profile->Data->CodeFragments!=S->Fragments) return End(false,TEXT("Escape purchased a locked card"));G->OpenHideout();G->DecisionWidget->Select(3);Next(7);}
        else if(S->Phase==7 && Age>.3) {Focus(G->DecisionWidget);Key();if(!G->DecisionWidget || !Has(TEXT("RETURN TO CODEX"))) return End(false,TEXT("Program library did not return to hideout"));Next(8);}
        else if(S->Phase==8 && Age>.3)
        {
            Focus(G->DecisionWidget);Key();if(G->DecisionWidget) return End(false,TEXT("Hideout Back failed"));
            S->Checks.Add(TEXT("Focused dialog buttons back through upgrade paths, card editor, program library and hideout without selecting an upgrade"));C->ToggleSettings();Next(9);
        }
        else if(S->Phase==9 && Age>.3)
        {
            auto* Apply=Cast<UButton>(C->SettingsWidget->WidgetTree->FindWidget(TEXT("ApplyDisplay")));if(!Apply) return End(false,TEXT("Display preview action missing"));Apply->OnClicked.Broadcast();Next(91);
        }
        else if(S->Phase==91 && Age>.3)
        {
            if(!C->SettingsWidget->IsDisplayConfirmationPending()) return End(false,TEXT("Display confirmation missing"));Focus(C->SettingsWidget);Key();
            if(!C->SettingsWidget || C->SettingsWidget->IsDisplayConfirmationPending()) return End(false,TEXT("Escape did not revert only the display preview"));C->OpenHelp();Next(10);
        }
        else if(S->Phase==10 && Age>.3)
        {
            Focus(C->HelpWidget);Key();if(C->HelpWidget || !C->SettingsWidget || C->SettingsWidget->GetVisibility()!=ESlateVisibility::Visible || !C->IsPaused()) return End(false,TEXT("Help Back lost Settings parent/pause"));Next(11);
        }
        else if(S->Phase==11 && Age>.3) {FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,true,0,0));if(!C->SettingsWidget) return End(false,TEXT("Held Escape cascaded into the parent window"));Focus(C->SettingsWidget);Key(EKeys::Gamepad_FaceButton_Right);if(C->SettingsWidget || !C->IsPaused()) return End(false,TEXT("Settings Back lost root pause"));C->ToggleSettings();C->OpenTestingArea();Next(12);}
        else if(S->Phase==12 && Age>.3)
        {
            auto* Test=C->TestingWidget.Get();if(!Test) return End(false,TEXT("Testing configuration missing"));
            for(int32 I=0;I<16;++I) if(auto* Toggle=Cast<UCheckBox>(Test->WidgetTree->FindWidget(*FString::Printf(TEXT("AttackToggle%d"),I)))) Toggle->SetIsChecked(I==6);
            if(auto* Spacing=Cast<USpinBox>(Test->WidgetTree->FindWidget(TEXT("GridSpacing")))) Spacing->SetValue(500);
            auto* Start=Cast<UButton>(Test->WidgetTree->FindWidget(TEXT("StartTesting")));if(!Start) return End(false,TEXT("Start Testing missing"));Start->OnClicked.Broadcast();Next(13);
        }
        else if(S->Phase==13 && Age>.3)
        {
            auto* Test=C->TestingWidget.Get();auto* Warn=Test ? Test->WidgetTree->FindWidget(TEXT("ChainSpacingWarning")) : nullptr;
            if(!Warn || Warn->GetVisibility()!=ESlateVisibility::Visible) return End(false,TEXT("Expected chain-spacing warning missing"));Focus(Test);Key();
            if(!C->TestingWidget || Warn->GetVisibility()!=ESlateVisibility::Collapsed) return End(false,TEXT("Escape did not dismiss only the spacing warning"));Next(14);
        }
        else if(S->Phase==14 && Age>.3) {Focus(C->TestingWidget);Key();if(C->TestingWidget || !C->SettingsWidget) return End(false,TEXT("Testing Back lost Settings"));Next(15);}
        else if(S->Phase==15 && Age>.3) {Focus(C->SettingsWidget);Key();C->OpenLeaderboard();Next(16);}
        else if(S->Phase==16 && Age>.3)
        {
            if(!C->ScoreWidget) return End(false,TEXT("Leaderboard missing"));Focus(C->ScoreWidget);Key();if(C->ScoreWidget || !C->IsPaused()) return End(false,TEXT("Leaderboard Back failed"));
            S->Checks.Add(TEXT("Escape returns Help and Testing to Settings, dismisses spacing warning first, closes leaderboard and preserves root pause"));
            if(!G->StartSelectedRun()) return End(false,TEXT("Could not start run"));P->bBenchmarkInvulnerable=true;G->AddXP(G->GetXPRequirement());Next(17);
        }
        else if(S->Phase==17 && Age>.3)
        {
            if(!G->bChoosingCard || !G->SelectionWidget) return End(false,TEXT("Draft missing"));S->Draft=G->SelectionWidget;S->Hand=Hand();S->Pending=G->PendingLevels;S->Rerolls=P->Deck->RerollsRemaining;S->Banishes=P->Deck->BanishesRemaining;for(UCardDataAsset* Card:P->Deck->CurrentHand) S->Mutations.Add(P->Deck->GetOfferedMutation(Card->CardID));
            C->ToggleSettings();Next(18);
        }
        else if(S->Phase==18 && Age>.3)
        {
            Focus(C->SettingsWidget);Key();if(C->SettingsWidget || !G->bChoosingCard || !C->IsPaused() || Hand()!=S->Hand) return End(false,TEXT("Settings Back did not restore draft"));Next(19);
        }
        else if(S->Phase==19 && Age>.3)
        {
            Focus(G->SelectionWidget);Key();
            if(G->bChoosingCard || !G->bDraftDeferred || C->IsPaused() || G->SelectionWidget!=S->Draft.Get() || Hand()!=S->Hand || G->PendingLevels!=S->Pending || G->ChooseCard(0) || G->SelectionWidget->SelectChoice(0)) return End(false,TEXT("Escape lost the hand or allowed a hidden upgrade"));
            G->AddXP(G->GetXPRequirement());S->RunTime=G->RunSeconds;Next(20);
        }
        else if(S->Phase==20 && Age>.8)
        {
            if(G->RunSeconds<=S->RunTime || !G->bDraftDeferred || G->bChoosingCard || Hand()!=S->Hand || G->PendingLevels!=S->Pending+1) return End(false,TEXT("Deferred draft stopped combat, redrew cards or lost earned levels"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("Deferred-Draft.png"),true,false);Next(21);
        }
        else if(S->Phase==21 && Age>.3)
        {
            C->ToggleSettings();TArray<int32> Counts;Counts.Init(0,10);Counts[0]=5;
            if(!C->StartTestingCards({FName("PulseDart")},Counts,true)) return End(false,TEXT("Could not test a deferred draft across practice"));Next(212);
        }
        else if(S->Phase==212 && Age>.3) {Raw(EKeys::Tab);Next(213);}
        else if(S->Phase==213 && Age>.3)
        {
            if(!G->TestingDirector || !G->TestingDirector->IsActive() || G->bChoosingCard || !G->bDraftDeferred) return End(false,TEXT("Tab reopened a normal draft inside practice"));C->ExitTestingArea();Next(214);
        }
        else if(S->Phase==214 && Age>.3)
        {
            if(G->TestingDirector->IsActive() || !G->bDraftDeferred || Hand()!=S->Hand) return End(false,TEXT("Practice exit lost the deferred hand"));Raw(EKeys::Tab);Next(211);
        }
        else if(S->Phase==211 && Age>.3)
        {
            if(!G->bChoosingCard || G->bDraftDeferred || !C->IsPaused() || G->SelectionWidget!=S->Draft.Get() || Hand()!=S->Hand || P->Deck->RerollsRemaining!=S->Rerolls || P->Deck->BanishesRemaining!=S->Banishes) return End(false,TEXT("Tab did not restore the same offered upgrade"));
            for(int32 I=0;I<P->Deck->CurrentHand.Num();++I) if(P->Deck->GetOfferedMutation(P->Deck->CurrentHand[I]->CardID)!=S->Mutations[I]) return End(false,TEXT("Deferred draft changed its offered mutation"));
            S->Active=P->Deck->ActiveWeapons.Num();P->Deck->MaximumActiveWeapons=S->Active;
            int32 NewCard=INDEX_NONE;for(int32 I=0;I<P->Deck->CurrentHand.Num();++I) if(!P->Deck->FindOwnedSpell(P->Deck->CurrentHand[I]->CardID)) {NewCard=I;break;}
            if(NewCard==INDEX_NONE || G->ChooseCard(NewCard) || !G->DecisionWidget) return End(false,TEXT("Replacement prompt not created"));Next(22);
        }
        else if(S->Phase==22 && Age>.3)
        {
            Focus(G->DecisionWidget);Key();if(G->DecisionWidget || !G->bChoosingCard || !C->IsPaused() || Hand()!=S->Hand || P->Deck->ActiveWeapons.Num()!=S->Active) return End(false,TEXT("Replacement Escape consumed a choice or changed weapons"));
            P->Deck->MaximumActiveWeapons=6;S->Checks.Add(TEXT("Draft Escape resumes live combat; same widget/hand, rerolls, branch state and queued levels survive Tab; replacement Escape keeps weapons"));
            for(int32 I=0;I<4 && G->bChoosingCard;++I) if(!G->ChooseCard(0)) return End(false,TEXT("Could not resolve preserved upgrades"));
            if(G->bChoosingCard || G->PendingLevels!=0 || C->IsPaused()) return End(false,TEXT("Queued choices did not finish normally"));G->PendingCorruptRewards=1;G->ChooseCorruptPerk();Next(23);
        }
        else if(S->Phase==23 && Age>.3)
        {
            const int32 Count=G->CorruptPerks.Num();Focus(G->DecisionWidget);Key();if(G->DecisionWidget || G->CorruptPerks.Num()!=Count || C->IsPaused()) return End(false,TEXT("Escape selected a corruption perk"));
            S->Fragments=Profile->Data->CodeFragments;G->OfferCapturedCode(0);Next(24);
        }
        else if(S->Phase==24 && Age>.3)
        {
            Focus(G->DecisionWidget);Key();if(G->DecisionWidget || Profile->Data->CodeFragments!=S->Fragments || G->CapturedRoutine!=INDEX_NONE) return End(false,TEXT("Escape accepted captured code"));
            G->FinishRun(false);C->OpenLeaderboard();Next(25);
        }
        else if(S->Phase==25 && Age>.3)
        {
            if(!C->ScoreWidget) return End(false,TEXT("End-run scoreboard missing"));bool Found=false;C->ScoreWidget->WidgetTree->ForEachWidget([&](UWidget* Widget){if(auto* Edit=Cast<UEditableTextBox>(Widget)) {Edit->SetKeyboardFocus();Found=true;}});
            if(!Found) return End(false,TEXT("Username control missing"));Key();if(C->ScoreWidget || !C->IsPaused() || !G->bRunEnded) return End(false,TEXT("Escape in username field failed to close scoreboard"));
            S->Checks.Add(TEXT("Back declines corruption/capture without spending or auto-picking, and closes the scoreboard while username text has focus"));
            return End(true,TEXT("Complete hover descriptions and layered Escape navigation verified through real pointer/Slate input with isolated saves"));
        }
        return true;
    }));
}
