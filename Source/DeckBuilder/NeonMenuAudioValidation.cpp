#include "ForbiddenWorkbenchLayout.h"
#include "MVPGame.h"
#include "NeonAudioDirector.h"
#include "NeonMenuAudio.h"
#include "NeonSettingsWidget.h"
#include "ForbiddenHelpWidget.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Audio.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"

void InstallNeonMenuAudioValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("MenuAudioSmoke")) || !FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA"))) return;
    struct FState {int32 Phase=0,Inputs=0,Closes=0,Navigations=0;float CapturePeak=0;double Since=FPlatformTime::Seconds();};
    const auto State=MakeShared<FState>();const TWeakObjectPtr<AMVPController> Weak(PC);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State,Weak](float)
    {
        auto* C=Weak.Get();if(!C) return false;auto* G=Cast<AMVPArenaGameMode>(C->GetWorld()->GetAuthGameMode());auto* A=G ? G->AudioDirector.Get() : nullptr;auto* HUD=Cast<AMVPHUD>(C->GetHUD());if(!A || !HUD || !A->IsReady()) return true;
        const FString Directory=FPaths::ProjectSavedDir()/TEXT("Review/MenuAudio");IFileManager::Get().MakeDirectory(*Directory,true);
        auto End=[&](bool Pass,const FString& Detail)
        {
            FString Report=FString::Printf(TEXT("{\"result\":\"%s\",\"detail\":\"%s\",\"navigationPlays\":%d,\"inputPlays\":%d,\"closePlays\":%d,\"effectVoices\":%d,\"capturePeak\":%.6f}"),Pass ? TEXT("MENU_AUDIO_PASS") : TEXT("MENU_AUDIO_FAIL"),*Detail,A->MenuNavigationPlays,A->MenuInputPlays,A->MenuClosePlays,A->GetVoiceCapacity(),State->CapturePeak);
            FFileHelper::SaveStringToFile(Report,*(Directory/TEXT("report.json")));FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
        };
        auto Next=[&](int32 Phase){State->Phase=Phase;State->Since=FPlatformTime::Seconds();};
        auto ActionVoice=[&]()->UAudioComponent* {return FindObject<UAudioComponent>(A,TEXT("Voice_3_1"));};
        auto Cue=[&](const TCHAR* Name){auto* V=ActionVoice();return V && V->IsPlaying() && V->Sound && V->Sound->GetName()==Name && V->bIsUISound;};
        const double Age=FPlatformTime::Seconds()-State->Since;if(Age>40) return End(false,TEXT("Phase timeout"));
        if(State->Phase==0 && Age>3)
        {
            A->SetMusicVolume(0);A->SetSFXVolume(1);C->SetMouseLocation(200,160);
            // Hidden/offscreen validation otherwise inherits the app's unfocused mute.
            // This process-only override never changes the player's saved audio settings.
            FApp::SetUnfocusedVolumeMultiplier(1.f);FApp::SetVolumeMultiplier(1.f);
            UAudioMixerBlueprintLibrary::StartRecordingOutput(C,12.f);Next(1);
        }
        else if(State->Phase==1 && Age>.4)
        {
            if(A->MenuNavigationPlays!=0) return End(false,TEXT("Pointer hover played an unwanted cue"));
            State->Navigations=A->MenuNavigationPlays;State->Inputs=A->MenuInputPlays;
            HUD->ClickAt(ForbiddenWorkbench::SettingsX+80,ForbiddenWorkbench::NavY+17);
            if(!C->SettingsWidget || A->MenuInputPlays!=State->Inputs+1 || !Cue(TEXT("SW_UI_Select"))) return End(false,TEXT("Relocated Settings button did not open with the medium click"));Next(2);
        }
        else if(State->Phase==2 && Age>.8)
        {
            if(A->MenuNavigationPlays!=State->Navigations) return End(false,TEXT("Underlying menu hovered behind Settings"));
            State->Closes=A->MenuClosePlays;C->CloseSettings();
            if(A->MenuClosePlays!=State->Closes+1 || !Cue(TEXT("SW_UI_Click"))) return End(false,TEXT("Closing Settings did not play the small click"));Next(3);
        }
        else if(State->Phase==3 && Age>.55)
        {
            A->SetSFXVolume(.25f);State->Inputs=A->MenuInputPlays;
            NeonMenuAudio::Navigate(C);NeonMenuAudio::Input(C);
            if(A->MenuNavigationPlays!=0) return End(false,TEXT("Navigation played an unwanted cue"));
            if(A->MenuInputPlays!=State->Inputs+1 || !Cue(TEXT("SW_UI_Select")) || !FMath::IsNearlyEqual(ActionVoice()->VolumeMultiplier,.25f)) return End(false,TEXT("Navigation swallowed a click or SFX gain was ignored"));
            Next(4);
        }
        else if(State->Phase==4 && Age>.8)
        {
            A->SetSFXVolume(1);HUD->ClickAt(ForbiddenWorkbench::HowToX+80,ForbiddenWorkbench::NavY+17);if(!C->HelpWidget || !Cue(TEXT("SW_UI_Select"))) return End(false,TEXT("How To button did not select with requested sound"));Next(5);
        }
        else if(State->Phase==5 && Age>.8)
        {
            C->CloseHelp();if(!Cue(TEXT("SW_UI_Click")) || !C->IsPaused()) return End(false,TEXT("Help close cue or initial-menu pause restoration failed"));
            Next(6);
        }
        else if(State->Phase==6 && Age>.65)
        {
            UAudioMixerBlueprintLibrary::StopRecordingOutput(C,EAudioRecordingExportType::WavFile,TEXT("MenuFeedback"),Directory);
            FScreenshotRequest::RequestScreenshot(Directory/TEXT("MainMenu.png"),true,false);Next(7);
        }
        else if(State->Phase==7 && Age>1)
        {
            if(IFileManager::Get().FileSize(*(Directory/TEXT("MenuFeedback.wav")))<10000) return End(false,TEXT("Audio mixer did not export a recording"));
            TArray<uint8> Bytes;FFileHelper::LoadFileToArray(Bytes,*(Directory/TEXT("MenuFeedback.wav")));FWaveModInfo Wave;
            if(!Wave.ReadWaveInfo(Bytes.GetData(),Bytes.Num()) || !Wave.pBitsPerSample || *Wave.pBitsPerSample!=16) return End(false,TEXT("Invalid audio mixer PCM capture"));
            for(uint32 Offset=0;Offset+2<=Wave.SampleDataSize;Offset+=2)
            {int16 Sample;FMemory::Memcpy(&Sample,Wave.SampleDataStart+Offset,2);State->CapturePeak=FMath::Max(State->CapturePeak,FMath::Abs(float(Sample)/32768.f));}
            if(State->CapturePeak<.003f) return End(false,TEXT("Mixer capture was silent"));
            if(A->GetVoiceCapacity()!=18 || A->GetMenuCloseDuration()<.3f || A->GetMenuCloseDuration()>.5f) return End(false,TEXT("Unexpected voice budget or exit recording duration"));
            return End(true,TEXT("Silent hover/navigation, Settings and Help clicks, closing cues, click priority, SFX volume, paused UI playback and audible mixer recording verified"));
        }
        return true;
    }));
}
