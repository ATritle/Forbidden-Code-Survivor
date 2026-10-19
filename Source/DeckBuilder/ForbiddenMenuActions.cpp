#include "MVPGame.h"
#include "ForbiddenHelpWidget.h"
#include "ForbiddenProfile.h"
#include "NeonSettingsWidget.h"
#include "NeonAudioDirector.h"
#include "NeonSpellImpactDirector.h"
#include "CodexCardSelectionWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NeonMenuAudio.h"
#include "Containers/Ticker.h"

void AMVPController::OpenHelp()
{
    auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(!G || HelpWidget || ScoreWidget || G->DecisionWidget || IsChallengeOpen() || TestingWidget) return;
    HelpWidget=CreateWidget<UForbiddenHelpWidget>(this,UForbiddenHelpWidget::StaticClass());if(!HelpWidget) return;
    NeonMenuAudio::Input(this);
    bHelpWasPaused=IsPaused();SetPause(true);SetMenuInput(true);
    if(SettingsWidget) SettingsWidget->SetVisibility(ESlateVisibility::Collapsed);
    if(G->SelectionWidget) G->SelectionWidget->SetIsEnabled(false);
    HelpWidget->AddToViewport(120);
    FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(HelpWidget->TakeWidget());SetInputMode(Input);HelpWidget->SetKeyboardFocus();
}
void AMVPController::CloseHelp()
{
    if(!HelpWidget) return;NeonMenuAudio::Exit(this);HelpWidget->RemoveFromParent();HelpWidget=nullptr;
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
    if(G && G->AudioDirector && G->AudioDirector->bMenuQuitPending) return;
    if(G && G->AudioDirector) G->AudioDirector->SaveAudioSettings();
    if(G && G->SpellImpactDirector) G->SpellImpactDirector->SaveVFXSettings();
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>()) Profile->Save();
    if(G && G->AudioDirector) G->AudioDirector->bMenuQuitPending=true;
    NeonMenuAudio::Exit(this);SetPause(true);SetInputMode(FInputModeUIOnly());bShowMouseCursor=false;
    if(SettingsWidget) SettingsWidget->SetIsEnabled(false);
    if(HelpWidget) HelpWidget->SetIsEnabled(false);
    // Core ticker uses real time: the closing cue must finish even in a paused menu.
    const double ExitAt=FPlatformTime::Seconds()+(G && G->AudioDirector ? G->AudioDirector->GetMenuCloseDuration() : 0.f)+0.08;
    const TWeakObjectPtr<AMVPController> WeakThis(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis,ExitAt](float)
    {
        if(!WeakThis.IsValid()) return false;
        if(FPlatformTime::Seconds()<ExitAt) return true;
        UKismetSystemLibrary::QuitGame(WeakThis.Get(),WeakThis.Get(),EQuitPreference::Quit,false);return false;
    }));
}
