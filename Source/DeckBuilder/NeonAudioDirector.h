#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonAudioDirector.generated.h"

class UAudioComponent;
class USoundWave;

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
    void PlayWarning(FName Cue,FVector Location);
    /** Call after changing game state; Tick also reconciles external pause changes. */
    void UpdateMusicState();
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
    bool bSettingsDirty=false;
    double SaveAfter=0;
    FString SettingsSlot() const;
    void ApplyVolumes(bool bEffects=true);
    bool bMusicStarted=false;
    bool bMenuMusic=true;
    double LastAttack=-100, LastImpact=-100, LastEnemy=-100, LastUI=-100;
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
