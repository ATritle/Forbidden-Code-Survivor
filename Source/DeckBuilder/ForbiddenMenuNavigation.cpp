#include "MVPGame.h"
#include "MortisTestingDirector.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenHelpWidget.h"
#include "ForbiddenScoreWidget.h"
#include "NeonSettingsWidget.h"
#include "NeonTestingWidget.h"
#include "NeonChallenge.h"
#include "CodexCardSelectionWidget.h"
#include "CodexDeckComponent.h"
#include "NeonMenuAudio.h"
#include "Kismet/GameplayStatics.h"

void AMVPController::NavigateBack()
{
    auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));if(!G) return;
    if(HelpWidget) {CloseHelp();return;}
    if(TestingWidget) {TestingWidget->RequestBack();return;}
    if(SettingsWidget) {SettingsWidget->RequestBack();return;}
    if(ScoreWidget) {ScoreWidget->Close();return;}
    if(IsChallengeOpen()) {if(auto* P=Cast<AMVPPlayer>(GetPawn())) P->Challenges->DeclineOffer();return;}
    if(G->DecisionWidget) {G->DecisionWidget->Back();return;}
    if(G->bInCodex) {if(auto* H=Cast<AMVPHUD>(GetHUD())) H->BackFromWorkbench();return;}
    if(G->bChoosingCard) {G->DeferCardSelection();return;}
    if(!G->bRunEnded) ToggleSettings();
}
void AMVPController::ResumeDraft()
{
    auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(!G || HelpWidget || TestingWidget || SettingsWidget || ScoreWidget || IsChallengeOpen() || G->DecisionWidget) return;
    G->ResumeCardSelection();
}
void AMVPArenaGameMode::DeferCardSelection()
{
    if(!bChoosingCard || bRunEnded || DecisionWidget || !SelectionWidget) return;
    auto* PC=Cast<AMVPController>(UGameplayStatics::GetPlayerController(this,0));if(!PC) return;
    // Retain the actual widget and hand: branch choices, rerolls and earned levels survive Back.
    SelectionWidget->SetIsEnabled(false);SelectionWidget->SetVisibility(ESlateVisibility::Collapsed);
    bChoosingCard=false;bDraftDeferred=true;PC->SetMenuInput(false);PC->SetPause(false);NeonMenuAudio::Exit(this);
}
void AMVPArenaGameMode::ResumeCardSelection()
{
    if(!bDraftDeferred || bRunEnded || bInCodex || DecisionWidget || PendingLevels<=0 || !SelectionWidget || (TestingDirector && TestingDirector->IsActive())) return;
    auto* PC=Cast<AMVPController>(UGameplayStatics::GetPlayerController(this,0));if(!PC) return;
    bDraftDeferred=false;bChoosingCard=true;PC->SetPause(true);PC->SetMenuInput(true);
    SelectionWidget->SetVisibility(ESlateVisibility::Visible);SelectionWidget->SetIsEnabled(true);
    FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(SelectionWidget->TakeWidget());PC->SetInputMode(Input);SelectionWidget->SetKeyboardFocus();NeonMenuAudio::Input(this);
}
