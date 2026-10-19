#include "MVPGame.h"
#include "ForbiddenHelpWidget.h"
#include "ForbiddenProfile.h"
#include "NeonSettingsWidget.h"
#include "NeonAudioDirector.h"
#include "NeonSpellImpactDirector.h"
#include "CodexCardSelectionWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void AMVPController::OpenHelp()
{
    auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(!G || HelpWidget || ScoreWidget || G->DecisionWidget || IsChallengeOpen() || TestingWidget) return;
    HelpWidget=CreateWidget<UForbiddenHelpWidget>(this,UForbiddenHelpWidget::StaticClass());if(!HelpWidget) return;
    bHelpWasPaused=IsPaused();SetPause(true);SetMenuInput(true);
    if(SettingsWidget) SettingsWidget->SetVisibility(ESlateVisibility::Collapsed);
    if(G->SelectionWidget) G->SelectionWidget->SetIsEnabled(false);
    HelpWidget->AddToViewport(120);
    FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(HelpWidget->TakeWidget());SetInputMode(Input);HelpWidget->SetKeyboardFocus();
}
void AMVPController::CloseHelp()
{
    if(!HelpWidget) return;HelpWidget->RemoveFromParent();HelpWidget=nullptr;
    auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(SettingsWidget)
    {
        SettingsWidget->SetVisibility(ESlateVisibility::Visible);
        FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(SettingsWidget->TakeWidget());SetInputMode(Input);SettingsWidget->SetKeyboardFocus();
        SetPause(true);return;
    }
    if(G && G->bChoosingCard && G->SelectionWidget)
    {
        G->SelectionWidget->SetIsEnabled(true);FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(G->SelectionWidget->TakeWidget());SetInputMode(Input);
    }
    else SetMenuInput(!G || G->bInCodex || G->bRunEnded);
    SetPause(bHelpWasPaused || !G || G->bInCodex || G->bRunEnded || G->bChoosingCard);
}
void AMVPController::CloseGame()
{
    auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(G && G->AudioDirector) G->AudioDirector->SaveAudioSettings();
    if(G && G->SpellImpactDirector) G->SpellImpactDirector->SaveVFXSettings();
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>()) Profile->Save();
    UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);
}
