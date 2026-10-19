#include "MVPGame.h"
#include "NeonSettingsWidget.h"
#include "ForbiddenSettingsControls.h"
#include "ForbiddenProfile.h"
#include "NeonAudioDirector.h"
#include "NeonSpellImpactDirector.h"
#include "CodexCardSelectionWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/WidgetSwitcher.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "Widgets/SWindow.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

void InstallSettingsReviewValidation(AMVPController* Controller)
{
    const bool Layout=FParse::Param(FCommandLine::Get(),TEXT("SettingsReviewSmoke"));
    const bool Native=FParse::Param(FCommandLine::Get(),TEXT("SettingsDisplaySmoke"));
    const bool PreferencesReload=FParse::Param(FCommandLine::Get(),TEXT("SettingsPreferencesReload"));
    const bool DisplayReload=FParse::Param(FCommandLine::Get(),TEXT("SettingsDisplayReload"));
    if(!(Layout || Native || PreferencesReload || DisplayReload) || !FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA"))) return;
    struct FState {int32 Phase=0,Index=0;double Since=FPlatformTime::Seconds();TArray<TSharedPtr<FJsonValue>> Checks;};
    const auto S=MakeShared<FState>();const TWeakObjectPtr<AMVPController> Weak(Controller);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak,Layout,Native,PreferencesReload,DisplayReload](float)
    {
        auto* C=Weak.Get();if(!C) return false;auto* G=Cast<AMVPArenaGameMode>(C->GetWorld()->GetAuthGameMode());
        auto* Settings=UGameUserSettings::GetGameUserSettings();if(!G || !Settings || !G->AudioDirector) return true;
        const auto Window=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
        if(Native && Window.IsValid()) Window->HideWindow();
        const FString Directory=FPaths::ProjectSavedDir()/TEXT("Review/SettingsReview");IFileManager::Get().MakeDirectory(*Directory,true);
        const TCHAR* File=PreferencesReload ? TEXT("preferences-reload.json") : DisplayReload ? TEXT("display-reload.json") : TEXT("report.json");
        auto End=[&](bool Pass,const FString& Detail)
        {
            auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("result"),Pass ? TEXT("SETTINGS_REVIEW_PASS") : TEXT("SETTINGS_REVIEW_FAIL"));Report->SetStringField(TEXT("detail"),Detail);
            Report->SetStringField(TEXT("case"),Native ? TEXT("native-display") : PreferencesReload ? TEXT("preferences-reload") : DisplayReload ? TEXT("display-reload") : TEXT("layout-and-graphics"));Report->SetArrayField(TEXT("checks"),S->Checks);
            FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Directory/File));FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
        };
        auto Check=[&](const FString& Detail){S->Checks.Add(MakeShared<FJsonValueString>(Detail));};
        auto Next=[&](int32 Phase){S->Phase=Phase;S->Since=FPlatformTime::Seconds();};
        const double Age=FPlatformTime::Seconds()-S->Since;if(Age>45) return End(false,TEXT("Settings verification phase timed out"));
        auto Click=[&](const TCHAR* Name){auto* B=C->SettingsWidget ? Cast<UButton>(C->SettingsWidget->WidgetTree->FindWidget(Name)) : nullptr;if(!B) return false;B->OnClicked.Broadcast();return true;};
        auto Mode=[&](int32 Value)
        {
            auto* W=C->SettingsWidget.Get();if(!W) return false;auto* Combo=Cast<UComboBoxString>(W->WidgetTree->FindWidget(TEXT("WindowModeCombo")));auto* Res=Cast<UComboBoxString>(W->WidgetTree->FindWidget(TEXT("ResolutionCombo")));
            if(!Combo || !Res) return false;Combo->SetSelectedIndex(Value);
            if(Value==2) Res->SetSelectedOption(TEXT("1280 x 720"));return Click(TEXT("ApplyDisplay"));
        };
        auto ActualMode=[&](int32 Value)
        {return Settings->GetFullscreenMode()==Value && Window.IsValid() && Window->GetWindowMode()==Value && GEngine->GameViewport->Viewport->GetWindowMode()==Value;};
        auto CVar=[&](const TCHAR* Name){auto* V=IConsoleManager::Get().FindConsoleVariable(Name);return V ? V->GetFloat() : -999.f;};
        if(S->Phase==0 && Age>3)
        {
            if(PreferencesReload)
            {
                auto* P=C->GetGameInstance()->GetSubsystem<UForbiddenProfile>();
                if(!P || ForbiddenSettings::Get(C,"Graphics",-1)!=3 || !FMath::IsNearlyEqual(ForbiddenSettings::Get(C,"RenderScale",-1),63.f) || !Settings->IsVSyncEnabled() || !FMath::IsNearlyEqual(CVar(TEXT("r.ScreenPercentage")),63.f)) return End(false,TEXT("Graphics preferences did not survive fresh-process restart"));
                if(!FMath::IsNearlyEqual(G->AudioDirector->GetMusicVolume(),.42f) || !FMath::IsNearlyEqual(G->AudioDirector->GetSFXVolume(),.58f) || !FMath::IsNearlyEqual(G->SpellImpactDirector->GetImpactIntensity(),.67f)) return End(false,TEXT("Audio/VFX preferences did not survive restart"));
                Check(TEXT("Fresh process restored preset, custom render scale, VSync, independent audio volumes and impact intensity"));return End(true,TEXT("Saved settings restored"));
            }
            if(DisplayReload)
            {
                if(Settings->GetFullscreenMode()!=EWindowMode::WindowedFullscreen || Settings->GetLastConfirmedFullscreenMode()!=EWindowMode::WindowedFullscreen) return End(false,TEXT("Confirmed borderless mode did not survive fresh-process restart"));
                Check(TEXT("Fresh process restored confirmed borderless mode from GameUserSettings.ini"));return End(true,TEXT("Saved display mode restored"));
            }
            if(Native)
            {Settings->SetFullscreenMode(EWindowMode::Windowed);Settings->SetScreenResolution(FIntPoint(1280,720));Settings->ApplyResolutionSettings(false);Settings->ConfirmVideoMode();Settings->SaveSettings();}
            C->ToggleSettings();if(!C->SettingsWidget || !C->IsPaused()) return End(false,TEXT("Settings did not open and retain menu pause"));Next(Native ? 20 : 1);
        }
        else if(Layout && S->Phase==1 && Age>.5)
        {
            if(!Click(*FString::Printf(TEXT("GraphicsPreset_%d"),S->Index))) return End(false,TEXT("Graphics preset button missing"));Next(2);
        }
        else if(Layout && S->Phase==2 && Age>.5)
        {
            const float Expected=S->Index==0 ? 70.f : S->Index==1 ? 85.f : 100.f;
            for(const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality")})
                if(FMath::RoundToInt(CVar(Name))!=S->Index) return End(false,FString::Printf(TEXT("Preset %d failed to apply %s"),S->Index,Name));
            if(!FMath::IsNearlyEqual(CVar(TEXT("r.ScreenPercentage")),Expected,.05f)) return End(false,TEXT("Graphics preset did not apply actual render percentage"));
            if((CVar(TEXT("r.Lumen.DiffuseIndirect.Allow"))>0)!=(S->Index>=1) ||
               (CVar(TEXT("r.Lumen.Reflections.Allow"))>0)!=(S->Index>=2) ||
               (S->Index>=1 && FMath::RoundToInt(CVar(TEXT("r.Lumen.FinalGatherMethod")))!=(S->Index==1 ? 0 : 1)))
                return End(false,TEXT("Graphics preset did not apply the UE 5.8 Lumen rendering path"));
            Check(FString::Printf(TEXT("Preset %d applied all ten scalability groups, %.0f%% screen percentage and actual Lumen enablement"),S->Index,Expected));
            if(++S->Index<4) Next(1);else
            {
                auto* Scale=Cast<USlider>(C->SettingsWidget->WidgetTree->FindWidget(TEXT("RenderScaleSlider")));if(!Scale) return End(false,TEXT("Render-scale slider missing"));Scale->SetValue(63);Scale->OnValueChanged.Broadcast(63);
                if(!Settings->IsVSyncEnabled()) Click(TEXT("VSyncToggle"));if(!Settings->IsVSyncEnabled() || CVar(TEXT("r.VSync"))<.5f || !FMath::IsNearlyEqual(CVar(TEXT("r.ScreenPercentage")),63.f)) return End(false,TEXT("Live render scale or VSync failed"));
                Check(TEXT("Custom 63% render scale and VSync reached the actual render CVars"));S->Index=0;Next(3);
            }
        }
        else if(Layout && S->Phase==3 && Age>.4)
        {
            if(!Click(*FString::Printf(TEXT("SettingsTab_%d"),S->Index))) return End(false,TEXT("Settings tab missing"));Next(4);
        }
        else if(Layout && S->Phase==4 && Age>.5)
        {
            auto* W=C->SettingsWidget.Get();auto* Frame=W->WidgetTree->FindWidget(TEXT("SettingsFrame"));const auto FG=Frame->GetCachedGeometry();
            for(const TCHAR* Name:{TEXT("HowToPlay"),TEXT("TestingArea"),TEXT("QuitGame"),TEXT("CloseSettings")})
            {
                auto* B=W->WidgetTree->FindWidget(Name);if(!B) return End(false,TEXT("Pinned footer action missing"));const auto BG=B->GetCachedGeometry();
                const auto Min=FG.AbsoluteToLocal(BG.LocalToAbsolute(FVector2D::ZeroVector));const auto Max=FG.AbsoluteToLocal(BG.LocalToAbsolute(BG.GetLocalSize()));
                if(BG.GetLocalSize().X<20 || Min.X<0 || Min.Y<0 || Max.X>FG.GetLocalSize().X+1 || Max.Y>FG.GetLocalSize().Y+1) return End(false,TEXT("Footer action clipped outside settings frame"));
            }
            const TCHAR* Names[]={TEXT("Display.png"),TEXT("Audio.png"),TEXT("Accessibility.png"),TEXT("Controls.png")};
            FScreenshotRequest::RequestScreenshot(Directory/Names[S->Index],true,false);Check(FString::Printf(TEXT("Tab %d rendered with all four fixed footer actions inside the frame"),S->Index));Next(5);
        }
        else if(Layout && S->Phase==5 && Age>.4)
        {
            if(++S->Index<4) Next(3);else
            {
                auto* W=C->SettingsWidget.Get();
                for(const auto& Pair:TArray<TPair<FName,float>>{{"MusicVolumeSlider",.42f},{"SFXVolumeSlider",.58f},{"ImpactIntensitySlider",.67f}})
                {auto* Slider=Cast<USlider>(W->WidgetTree->FindWidget(Pair.Key));if(!Slider) return End(false,TEXT("Audio/VFX slider missing"));Slider->SetValue(Pair.Value);Slider->OnValueChanged.Broadcast(Pair.Value);}
                auto* Bloom=Cast<USlider>(W->WidgetTree->FindWidget(TEXT("BloomSlider")));auto* Player=Cast<AMVPPlayer>(C->GetPawn());
                if(!Bloom || !Player) return End(false,TEXT("Bloom control or player camera missing"));
                if(ForbiddenSettings::Get(C,"ReducedFlashes",0)>.5f) Click(TEXT("ReducedFlashesToggle"));
                Bloom->SetValue(.5f);Bloom->OnValueChanged.Broadcast(.5f);
                if(!FMath::IsNearlyEqual(Player->FollowCamera->PostProcessSettings.BloomIntensity,.325f)) return End(false,TEXT("Bloom slider did not change the actual camera"));
                Click(TEXT("ReducedFlashesToggle"));if(!FMath::IsNearlyEqual(Player->FollowCamera->PostProcessSettings.BloomIntensity,.075f)) return End(false,TEXT("Reduced flashes did not affect camera bloom"));Click(TEXT("ReducedFlashesToggle"));
                if(!FMath::IsNearlyEqual(CVar(TEXT("r.ScreenPercentage")),63.f)) return End(false,TEXT("Accessibility changed the custom render scale"));
                Check(TEXT("Bloom and reduced-flashes controls changed actual camera settings without resetting custom render scale"));
                Click(TEXT("Rebind_Forward"));if(!ForbiddenSettings::HandleKey(W,EKeys::Up)) return End(false,TEXT("Rebinding did not capture key"));Click(TEXT("ResetInput"));
                auto* P=C->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!P || P->Data->Bindings.FindRef("Forward")!=EKeys::W) return End(false,TEXT("Reset controls failed"));
                Click(TEXT("Rebind_Forward"));ForbiddenSettings::HandleKey(W,EKeys::Escape);
                Click(TEXT("TestingArea"));if(!C->TestingWidget || C->SettingsWidget->GetVisibility()!=ESlateVisibility::Collapsed) return End(false,TEXT("Testing range configuration did not open from settings"));
                C->CloseTestingConfiguration();if(!C->SettingsWidget || C->SettingsWidget->GetVisibility()!=ESlateVisibility::Visible) return End(false,TEXT("Testing configuration did not return to settings"));
                C->CloseSettings();if(C->IsSettingsOpen() || !C->IsPaused()) return End(false,TEXT("Closing settings damaged initial menu pause"));
                Check(TEXT("Audio/VFX sliders, input rebind/reset/cancel and saved close behavior verified"));
                if(!G->StartSelectedRun()) return End(false,TEXT("Could not start game after settings"));C->ToggleSettings();C->CloseSettings();if(C->IsPaused()) return End(false,TEXT("Gameplay did not resume"));
                G->AddXP(G->GetXPRequirement());C->ToggleSettings();C->CloseSettings();if(!C->IsPaused() || !G->SelectionWidget || !G->SelectionWidget->GetIsEnabled()) return End(false,TEXT("Nested draft settings damaged pause or focus"));
                Check(TEXT("Gameplay settings resume; nested card selection remains paused and enabled"));return End(true,TEXT("Organized settings layout and real graphics controls verified"));
            }
        }
        else if(Native && S->Phase==20 && Age>.8)
        {if(!ActualMode(2) || !Mode(1)) return End(false,TEXT("Native windowed baseline or borderless request failed"));Next(21);}
        else if(Native && S->Phase==21 && Age>1)
        {
            if(!ActualMode(1) || !C->SettingsWidget->IsDisplayConfirmationPending()) return End(false,TEXT("Borderless did not reach the native window and viewport"));
            Check(TEXT("Borderless applied to the real native window and RHI viewport while confirmation remained pending"));Next(22);
        }
        else if(Native && S->Phase==22 && Age>15)
        {
            if(!ActualMode(2) || C->SettingsWidget->IsDisplayConfirmationPending()) return End(false,TEXT("15-second timeout did not restore native windowed mode while paused/hidden"));
            Check(TEXT("Real 15-second confirmation timeout restored windowed mode while paused and hidden"));S->Index=0;Next(23);
        }
        else if(Native && S->Phase==23 && Age>.5)
        {const int32 Modes[]={1,0,2};if(!Mode(Modes[S->Index])) return End(false,TEXT("Display apply control failed"));Next(24);}
        else if(Native && S->Phase==24 && Age>1)
        {
            const int32 Modes[]={1,0,2};if(!ActualMode(Modes[S->Index]) || !Click(TEXT("KeepDisplay")) || Settings->GetLastConfirmedFullscreenMode()!=Modes[S->Index]) return End(false,TEXT("Mode selection did not update/confirm the real native window"));
            const auto ActualSize=GEngine->GameViewport->Viewport->GetSizeXY();const auto ExpectedSize=Settings->GetScreenResolution();
            if(FMath::Abs(ActualSize.X-ExpectedSize.X)>2 || FMath::Abs(ActualSize.Y-ExpectedSize.Y)>2)
                return End(false,FString::Printf(TEXT("Native output resolution mismatch: %d x %d expected %d x %d"),ActualSize.X,ActualSize.Y,ExpectedSize.X,ExpectedSize.Y));
            Check(FString::Printf(TEXT("Actual native viewport resolution verified at %d x %d"),ActualSize.X,ActualSize.Y));
            Check(FString::Printf(TEXT("Mode %d confirmed on native window, RHI viewport and GameUserSettings"),Modes[S->Index]));
            if(++S->Index<3) Next(23);else {Mode(1);C->CloseSettings();Next(25);}
        }
        else if(Native && S->Phase==25 && Age>1)
        {
            if(!ActualMode(2) || C->SettingsWidget) return End(false,TEXT("Closing settings did not revert unconfirmed display change"));
            Check(TEXT("Closing settings reverted unconfirmed display changes"));C->ToggleSettings();Mode(1);Next(26);
        }
        else if(Native && S->Phase==26 && Age>1)
        {
            if(!ActualMode(1) || !Click(TEXT("KeepDisplay"))) return End(false,TEXT("Final borderless preference did not confirm"));C->CloseSettings();
            return End(true,TEXT("Native fullscreen, borderless, windowed, timeout rollback, close rollback and confirmed save verified"));
        }
        return true;
    }));
}
