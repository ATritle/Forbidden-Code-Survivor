#include "MVPGame.h"
#include "NeonAudioDirector.h"
#include "EnemySpawner.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Containers/Ticker.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Audio.h"
#include "UnrealClient.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

/** Exercises the real canvas controls, stream completion, mixer and isolated save slot. */
void InstallMusicPlayerValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("MusicPlayerSmoke")) || !FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA"))) return;
    struct FState {int32 Phase=0;double Since=FPlatformTime::Seconds();float Position=0,Peak=0;};
    const auto S=MakeShared<FState>();const TWeakObjectPtr<AMVPController> Weak(PC);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak](float)
    {
        auto* C=Weak.Get();if(!C) return false;
        auto* G=Cast<AMVPArenaGameMode>(C->GetWorld()->GetAuthGameMode());auto* A=G ? G->AudioDirector.Get() : nullptr;auto* H=Cast<AMVPHUD>(C->GetHUD());
        if(!A || !H) return true;
        const FString Directory=FPaths::ProjectSavedDir()/TEXT("Review/MusicPlayer");IFileManager::Get().MakeDirectory(*Directory,true);
        const bool Reload=FParse::Param(FCommandLine::Get(),TEXT("MusicPlayerReload"));
        auto End=[&](bool Pass,const TCHAR* Detail)
        {
            const FString Result=Pass ? (Reload ? TEXT("MUSIC_PLAYER_RELOAD_PASS") : TEXT("MUSIC_PLAYER_PASS")) : TEXT("MUSIC_PLAYER_FAIL");
            const FString Json=FString::Printf(TEXT("{\"result\":\"%s\",\"detail\":\"%s\",\"tracks\":%d,\"peak\":%.6f,\"effectVoices\":%d,\"musicTransitions\":%d}"),*Result,Detail,ANeonAudioDirector::MusicLibrary().Num(),S->Peak,A->GetVoiceCapacity(),A->MusicTransitions);
            FFileHelper::SaveStringToFile(Json,*(Directory/(Reload ? TEXT("reload.json") : TEXT("report.json"))));FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
        };
        auto Next=[&](int32 Phase){S->Phase=Phase;S->Since=FPlatformTime::Seconds();};
        const double Age=FPlatformTime::Seconds()-S->Since;if(Age>35) return End(false,TEXT("Phase timeout"));
        if(S->Phase==0 && Age>3)
        {
            if(!A->IsReady() || !A->IsMenuMusic() || A->GetMusicTrackIndex()!=0 || A->GetMusicDuration()<167 || A->GetMusicDuration()>168) return End(false,TEXT("New default menu music not loaded"));
            if(Reload)
            {
                if(!FMath::IsNearlyEqual(A->GetMusicVolume(),50.f/135,.001f) || !A->IsMusicMuted()) return End(false,TEXT("Volume/mute did not survive process restart"));
                A->SetMusicVolume(1);A->SetSFXVolume(1);A->SaveAudioSettings();return End(true,TEXT("Saved music volume and independent mute restored in a fresh process"));
            }
            A->SetMusicVolume(1);A->SetSFXVolume(0);FApp::SetUnfocusedVolumeMultiplier(1);FApp::SetVolumeMultiplier(1);
            UAudioMixerBlueprintLibrary::StartRecordingOutput(C,20.f);Next(1);
        }
        else if(S->Phase==1 && Age>1)
        {
            FScreenshotRequest::RequestScreenshot(Directory/TEXT("SignalDock.png"),true,false);
            Next(11);
        }
        else if(S->Phase==11 && Age>.4)
        {H->ClickAt(725,870);S->Position=A->GetMusicElapsed();if(!A->IsMusicPaused()) return End(false,TEXT("Pause button failed"));Next(2);}
        else if(S->Phase==2 && Age>.8)
        {
            if(!FMath::IsNearlyEqual(A->GetMusicElapsed(),S->Position,.03f)) return End(false,TEXT("Pause lost the playback position"));
            H->ClickAt(650,870);if(A->IsMusicPaused()) return End(false,TEXT("Play button did not resume"));Next(3);
        }
        else if(S->Phase==3 && Age>.65)
        {
            if(A->GetMusicElapsed()<S->Position+.4f) return End(false,TEXT("Resume restarted or froze the stream"));
            H->ClickAt(790,870);if(A->GetMusicTrackIndex()!=1) return End(false,TEXT("Next button failed"));
            H->ClickAt(590,870);if(A->GetMusicTrackIndex()!=0) return End(false,TEXT("Previous button failed"));
            A->PreviousMusic();if(A->GetMusicTrackIndex()!=3) return End(false,TEXT("Previous wrap failed"));
            A->NextMusic();if(A->GetMusicTrackIndex()!=0) return End(false,TEXT("Next wrap failed"));
            H->ClickAt(1440,870);C->SetMouseLocation(1100,750);Next(4);
        }
        else if(S->Phase==4 && Age>.5)
        {
            if(!H->IsMusicPlaylistOpen()) return End(false,TEXT("Track list did not open"));H->ScrollMusicPlaylist(1);Next(5);
        }
        else if(S->Phase==5 && Age>.4)
        {
            FScreenshotRequest::RequestScreenshot(Directory/TEXT("SignalPlaylist.png"),true,false);Next(6);
        }
        else if(S->Phase==6 && Age>.5)
        {
            H->ClickAt(1100,790);if(A->GetMusicTrackIndex()!=3) return End(false,TEXT("Scrolled track selection failed"));
            H->ClickAt(1440,870);H->ClickAt(1260,870);H->ClickAt(1140,870);
            if(!A->IsMusicMuted() || !FMath::IsNearlyEqual(A->GetMusicVolume(),50.f/135,.001f)) return End(false,TEXT("Volume or mute control failed"));
            if(!A->SaveAudioSettings()) return End(false,TEXT("Could not save music preferences"));
            UAudioMixerBlueprintLibrary::StopRecordingOutput(C,EAudioRecordingExportType::WavFile,TEXT("MusicPlayback"),Directory);Next(7);
        }
        else if(S->Phase==7 && Age>.6)
        {
            TArray<uint8> Bytes;FFileHelper::LoadFileToArray(Bytes,*(Directory/TEXT("MusicPlayback.wav")));FWaveModInfo Wave;
            if(!Wave.ReadWaveInfo(Bytes.GetData(),Bytes.Num()) || !Wave.pBitsPerSample || *Wave.pBitsPerSample!=16) return End(false,TEXT("Music mixer recording missing"));
            for(uint32 Offset=0;Offset+2<=Wave.SampleDataSize;Offset+=2) {int16 Sample;FMemory::Memcpy(&Sample,Wave.SampleDataStart+Offset,2);S->Peak=FMath::Max(S->Peak,FMath::Abs(float(Sample)/32768));}
            if(S->Peak<.003f) return End(false,TEXT("Music mixer was silent"));
            G->StartingCards={TEXT("PulseDart"),TEXT("PrismBurst")};if(!G->StartSelectedRun()) return End(false,TEXT("Run did not start"));Next(8);
        }
        else if(S->Phase==8 && Age>.9)
        {
            if(A->IsMenuMusic() || A->GetMusicTrackIndex()!=1) return End(false,TEXT("Gameplay did not start with the run playlist"));
            // Let a real stream finish to verify the audio completion delegate.
            auto* Voice=FindObject<UAudioComponent>(A,TEXT("GameplayMusic"));if(!Voice) return End(false,TEXT("Gameplay stream missing"));
            Voice->Play(A->GetMusicDuration()-.5f);Next(9);
        }
        else if(S->Phase==9 && Age>2)
        {
            if(A->GetMusicTrackIndex()!=2) return End(false,TEXT("Previous menu track was not next in gameplay rotation"));
            S->Position=A->GetMusicElapsed();G->AddXP(G->GetXPRequirement());Next(10);
        }
        else if(S->Phase==10 && Age>.9)
        {
            if(!G->bChoosingCard || A->IsMenuMusic() || A->GetMusicTrackIndex()!=2 || A->GetMusicElapsed()<S->Position+.5f || A->MusicTransitions!=1) return End(false,TEXT("Card drafting interrupted gameplay music"));
            if(!A->IsMusicMuted() || A->GetVoiceCapacity()!=18) return End(false,TEXT("Mute or voice budget changed during gameplay"));
            return End(true,TEXT("All four tracks, default menu, mixer audio, pause/resume, previous/next wrap, scrolling, selection, saved volume/mute, natural gameplay rotation and draft continuity verified"));
        }
        return true;
    }));
}
