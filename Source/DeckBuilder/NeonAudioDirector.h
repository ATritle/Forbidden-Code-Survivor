#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonAudioDirector.generated.h"

class UAudioComponent;
class USoundWave;

/** Stable IDs keep UI ordering independent of SoundWave filenames. */
struct FNeonMusicTrack
{
    FName SoundID;
    FString Title;
    FString Artist;
};

/** Fixed voice budgets, player-relative attenuation, and pause-safe music fades. */
UCLASS()
class DECKBUILDER_API ANeonAudioDirector : public AActor
{
    GENERATED_BODY()
public:
    ANeonAudioDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    void PlayAttack(FName Cue, FVector Location);
    void PlayImpact(FVector Location);
    /** Explicit hook for the future enemy that owns these reserved recordings. */
    UFUNCTION(BlueprintCallable,Category="Audio|Reserved Enemy") void PlayEnemy(FVector Location, bool bDeath);
    void PlayUI(FName Cue);
    /** Two fixed UI voices: navigation cannot swallow a click or its closing cue. */
    void PlayMenuFeedback(bool bClosing=false,bool bNavigation=false);
    float GetMenuCloseDuration() const;
    int32 MenuNavigationPlays=0,MenuInputPlays=0,MenuClosePlays=0;
    FName LastMenuCue;
    bool bMenuQuitPending=false;
    void PlayWarning(FName Cue,FVector Location);
    /** Call after changing game state; Tick also reconciles external pause changes. */
    void UpdateMusicState();
    static const TArray<FNeonMusicTrack>& MusicLibrary();
    UFUNCTION(BlueprintCallable,Category="Audio|Music Player") void PlayMusic();
    UFUNCTION(BlueprintCallable,Category="Audio|Music Player") void PauseMusic();
    UFUNCTION(BlueprintCallable,Category="Audio|Music Player") void NextMusic();
    UFUNCTION(BlueprintCallable,Category="Audio|Music Player") void PreviousMusic();
    UFUNCTION(BlueprintCallable,Category="Audio|Music Player") void SelectMusicTrack(int32 Index);
    UFUNCTION(BlueprintCallable,Category="Audio|Music Player") void SeekMusic(float Seconds);
    UFUNCTION(BlueprintCallable,Category="Audio|Music Player") void SetMusicMuted(bool bMuted);
    UFUNCTION(BlueprintPure,Category="Audio|Music Player") bool IsMusicMuted() const {return bMusicMuted;}
    UFUNCTION(BlueprintPure,Category="Audio|Music Player") bool IsMusicPaused() const {return bMusicPaused;}
    int32 GetMusicTrackIndex() const {return bMenuMusic ? MenuTrackIndex : GameplayTrackIndex;}
    float GetMusicElapsed() const;
    float GetMusicDuration() const;
    const TArray<float>& GetMusicSpectrum() const {return MusicSpectrum;}
    bool HasMusicSpectrum() const {return bHasMusicSpectrum;}
    /** Asset bank readiness is independent of intentional pause/mute. */
    UFUNCTION(BlueprintCallable,Category="Audio") void SetMusicVolume(float Value);
    UFUNCTION(BlueprintCallable,Category="Audio") void SetSFXVolume(float Value);
    UFUNCTION(BlueprintPure,Category="Audio") float GetMusicVolume() const { return MusicVolume; }
    UFUNCTION(BlueprintPure,Category="Audio") float GetSFXVolume() const { return SFXVolume; }
    UFUNCTION(BlueprintCallable,Category="Audio") bool SaveAudioSettings();
    UFUNCTION(BlueprintCallable,Category="Audio") void LoadAudioSettings();
    bool IsMenuMusic() const { return bMenuMusic; }
    bool IsReady() const;
    int32 GetPlayingEffectCount() const;
    int32 GetVoiceCapacity() const { return Voices.Num(); }
    int32 AttackPlays=0, ImpactPlays=0, EnemyPlays=0, UIPlays=0;
    int32 MusicTransitions=0;
    int32 LowHealthAlarmPlays=0;
    bool IsCriticalHealth() const { return bCriticalHealth; }
    float GetHealthAlarmGain() const { return HealthAlarmGain; }
    float GetHealthAlarmElapsed() const { return HealthAlarmAge; }
    bool IsHealthAlarmPlaying() const;

private:
    enum class EVoiceGroup : uint8 { Attack, Impact, Enemy, UI };
    UAudioComponent* MakeAudio(FName Name, bool bUI, bool bSpatial);
    bool PlayEffect(FName Cue, EVoiceGroup Group, FVector Location, float Volume, float Pitch=1.f);
    UPROPERTY() TMap<FName,TObjectPtr<USoundWave>> Sounds;
    UPROPERTY() TObjectPtr<UAudioComponent> GameplayMusic;
    UPROPERTY() TObjectPtr<UAudioComponent> MenuMusic;
    UPROPERTY() TObjectPtr<UAudioComponent> CriticalAlarm;
    UPROPERTY() TObjectPtr<UAudioComponent> DangerWarning;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Voices;
    TArray<EVoiceGroup> Groups;
    TArray<float> VoiceGains;
    float MusicVolume=1.f;
    float SFXVolume=1.f;
    bool bMusicMuted=false,bMusicPaused=false,bSwitchingTrack=false;
    int32 MenuTrackIndex=0,GameplayTrackIndex=1;
    double MenuTrackStart=0,GameplayTrackStart=0;
    float MenuPausedPosition=0,GameplayPausedPosition=0;
    TArray<float> MusicSpectrum;
    bool bHasMusicSpectrum=false;
    double LastSpectrumUpdate=0;
    void UpdateMusicSpectrum(float RealDt,double Now);
    void StartTrack(bool bMenu,int32 Index);
    void StepMusic(int32 Direction);
    UFUNCTION() void MenuTrackFinished();
    UFUNCTION() void GameplayTrackFinished();
    bool bSettingsDirty=false;
    double SaveAfter=0;
    FString SettingsSlot() const;
    void ApplyVolumes(bool bEffects=true);
    bool bMusicStarted=false;
    bool bMenuMusic=true;
    double LastAttack=-100, LastImpact=-100, LastEnemy=-100, LastUI=-100;
    double LastMenuNavigation=-100,LastMenuAction=-100;
    double LastMusicUpdate=0;
    float MusicBlend=1.f;
    bool bCriticalHealth=false;
    bool bHealthAlarmStarted=false;
    float HealthAlarmAge=0.f;
    float HealthAlarmGain=0.f;
    double LastHealthAlarmUpdate=0;
    double LastWarning=-100;
    bool bDangerDucking=false;
    void UpdateHealthAlarm(double Now);
};
