#include "MVPGame.h"
#include "NeonChallenge.h"
#include "NeonSettingsWidget.h"
#include "NeonAudioDirector.h"
#include "NeonSpellImpactDirector.h"
#include "NeonTestingWidget.h"
#include "MortisTestingDirector.h"
#include "CodexCardSelectionWidget.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"

void AMVPController::ToggleSettings()
{
    if(ScoreWidget) return;
    if(auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));G && G->DecisionWidget) return;
    if(IsChallengeOpen()) {if(auto* P=Cast<AMVPPlayer>(GetPawn())) P->Challenges->DeclineOffer();return;}
    if(TestingWidget) {CloseTestingConfiguration();return;}
    if(IsSettingsOpen()) {CloseSettings();return;}
    auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(!Game || !Game->AudioDirector) return;
    SettingsWidget=CreateWidget<UNeonSettingsWidget>(this,UNeonSettingsWidget::StaticClass());
    if(!SettingsWidget) return;
    bSettingsOwnPause=!IsPaused();
    if(bSettingsOwnPause) SetPause(true);
    if(Game->SelectionWidget) Game->SelectionWidget->SetIsEnabled(false);
    if(auto* ArenaPlayer=Cast<AMVPPlayer>(GetPawn())) ArenaPlayer->Movement->StopMovementImmediately();
    SettingsWidget->AddToViewport(100);
    FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(SettingsWidget->TakeWidget());
    SetInputMode(Input);bShowMouseCursor=true;
}
void AMVPController::CloseSettings()
{
    if(!SettingsWidget) return;
    auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(Game && Game->AudioDirector && !Game->AudioDirector->SaveAudioSettings()) return;
    if(Game && Game->SpellImpactDirector && !Game->SpellImpactDirector->SaveVFXSettings()) return;
    SettingsWidget->RemoveFromParent();SettingsWidget=nullptr;
    if(Game && Game->bChoosingCard && Game->SelectionWidget)
    {
        Game->SelectionWidget->SetIsEnabled(true);
        FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(Game->SelectionWidget->TakeWidget());SetInputMode(Input);
    }
    else SetMenuInput(!Game || Game->bInCodex || Game->bRunEnded);
    if(bSettingsOwnPause && Game && !Game->bInCodex && !Game->bChoosingCard && !Game->bRunEnded) SetPause(false);
    bSettingsOwnPause=false;
}
void AMVPController::OpenTestingArea()
{
    if(!SettingsWidget || TestingWidget) return;
    TestingWidget=CreateWidget<UNeonTestingWidget>(this,UNeonTestingWidget::StaticClass());if(!TestingWidget) return;
    SettingsWidget->SetVisibility(ESlateVisibility::Collapsed);TestingWidget->AddToViewport(110);
    FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(TestingWidget->TakeWidget());SetInputMode(Input);
}
void AMVPController::CloseTestingConfiguration()
{
    if(!TestingWidget) return;TestingWidget->RemoveFromParent();TestingWidget=nullptr;
    if(SettingsWidget) {SettingsWidget->SetVisibility(ESlateVisibility::Visible);FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(SettingsWidget->TakeWidget());SetInputMode(Input);}
}
bool AMVPController::StartTestingSession(FName CardID,const TArray<int32>& Counts,bool bAutoReset,float GridSpacing)
{
    return StartTestingCards({CardID},Counts,bAutoReset,GridSpacing);
}
bool AMVPController::StartTestingCards(const TArray<FName>& Cards,const TArray<int32>& Counts,bool bAutoReset,float GridSpacing)
{
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());if(!Game || !SettingsWidget) return false;
    if(!Game->TestingDirector) Game->TestingDirector=GetWorld()->SpawnActor<AMortisTestingDirector>();
    FString Error;
    if(!Game->TestingDirector || !Game->TestingDirector->ConfigureCards(Cards,Counts,bAutoReset,Error,GridSpacing)) return false;
    if(!Game->AudioDirector->SaveAudioSettings() || !Game->SpellImpactDirector->SaveVFXSettings()) return false;
    if(!Game->TestingDirector->Enter(this,IsPaused() && !bSettingsOwnPause,Error)) return false;
    if(TestingWidget) {TestingWidget->RemoveFromParent();TestingWidget=nullptr;}
    SettingsWidget->RemoveFromParent();SettingsWidget=nullptr;bSettingsOwnPause=false;
    SetPause(false);SetMenuInput(false);return true;
}
void AMVPController::ExitTestingArea()
{
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());if(!Game || !Game->TestingDirector || !Game->TestingDirector->IsActive()) return;
    if(!Game->AudioDirector->SaveAudioSettings() || !Game->SpellImpactDirector->SaveVFXSettings()) return;
    if(TestingWidget) {TestingWidget->RemoveFromParent();TestingWidget=nullptr;}
    if(SettingsWidget) {SettingsWidget->RemoveFromParent();SettingsWidget=nullptr;}
    bSettingsOwnPause=false;Game->TestingDirector->Exit(this);
}
