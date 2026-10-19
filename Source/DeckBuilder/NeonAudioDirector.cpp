#include "NeonAudioDirector.h"
#include "MVPGame.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CodexAudioSettingsSave.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ANeonAudioDirector::ANeonAudioDirector()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AudioRoot")));
}

UAudioComponent* ANeonAudioDirector::MakeAudio(FName Name, bool bUI, bool bSpatial)
{
    auto* Audio=NewObject<UAudioComponent>(this,Name);
    AddInstanceComponent(Audio);
    Audio->SetupAttachment(RootComponent);
    Audio->bAutoActivate=false;
    Audio->bAutoDestroy=false;
    Audio->bStopWhenOwnerDestroyed=true;
    Audio->bIsUISound=bUI;
    Audio->bAllowSpatialization=bSpatial;
    if(bSpatial)
    {
        Audio->bOverrideAttenuation=true;
        Audio->AttenuationOverrides.bAttenuate=true;
        Audio->AttenuationOverrides.bSpatialize=true;
        Audio->AttenuationOverrides.bAttenuateWithLPF=false;
        Audio->AttenuationOverrides.AttenuationShapeExtents=FVector(280.f);
        Audio->AttenuationOverrides.FalloffDistance=2400.f;
        Audio->AttenuationOverrides.bEnableOcclusion=false;
    }
    Audio->RegisterComponent();
    return Audio;
}

void ANeonAudioDirector::BeginPlay()
{
    Super::BeginPlay();
    LoadAudioSettings();
    for(const TCHAR* Name:{TEXT("Music_Gameplay"),TEXT("Music_Menu"),TEXT("Pulse"),TEXT("Plasma"),TEXT("Laser"),TEXT("Blaster"),
        TEXT("Impact"),TEXT("Enemy_Metal"),TEXT("Enemy_Glitch"),TEXT("UI_Deny"),TEXT("UI_Confirm"),TEXT("UI_Select"),TEXT("UI_Click"),TEXT("LowHealth")})
    {
        auto* Sound=LoadObject<USoundWave>(nullptr,*FString::Printf(TEXT("/Game/Audio/SW_%s.SW_%s"),Name,Name));
        if(Sound) Sounds.Add(FName(Name),Sound);
        else UE_LOG(LogTemp,Error,TEXT("Missing audio asset: %s"),Name);
    }
    GameplayMusic=MakeAudio(TEXT("GameplayMusic"),true,false);
    MenuMusic=MakeAudio(TEXT("MenuMusic"),true,false);
    GameplayMusic->SetSound(Sounds.FindRef(TEXT("Music_Gameplay")));
    MenuMusic->SetSound(Sounds.FindRef(TEXT("Music_Menu")));
    CriticalAlarm=MakeAudio(TEXT("CriticalHealthAlarm"),false,false);
    CriticalAlarm->SetSound(Sounds.FindRef(TEXT("LowHealth")));CriticalAlarm->SetVolumeMultiplier(0.65f*SFXVolume);
    DangerWarning=MakeAudio(TEXT("PriorityDangerWarning"),false,true);
    // Prewarm all components; mutations/crowds can never allocate more voices.
    const int32 Counts[]={4,8,4,2};
    for(int32 Group=0;Group<4;++Group)
        for(int32 I=0;I<Counts[Group];++I)
        {
            Voices.Add(MakeAudio(*FString::Printf(TEXT("Voice_%d_%d"),Group,I),Group==3,Group!=3));
            Groups.Add(static_cast<EVoiceGroup>(Group));
            VoiceGains.Add(0.f);
        }
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0); PC && PC->GetPawn())
        PC->SetAudioListenerAttenuationOverride(PC->GetPawn()->GetRootComponent(),FVector::ZeroVector);
    LastMusicUpdate=FPlatformTime::Seconds();
    UpdateMusicState();
    UE_LOG(LogTemp,Display,TEXT("NEON_AUDIO_READY: %d assets, %d effect voices + 2 music voices"),Sounds.Num(),Voices.Num());
}

void ANeonAudioDirector::UpdateMusicState()
{
    const auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    // Only the initial Codex menu owns the menu track. Drafts, settings and
    // defeat keep the gameplay loop and its playhead, even while paused.
    const bool bMenu=!Game || Game->bInCodex;
    if(!bMusicStarted)
    {
        bMenuMusic=bMenu;
        MusicBlend=bMenu ? 1.f : 0.f;
        ApplyVolumes();
        // Both loops retain their playhead across brief card drafts. Real-time
        // equal-power blending works even when gameplay DeltaSeconds is zero.
        MenuMusic->Play();GameplayMusic->Play();bMusicStarted=true;
    }
    if(bMenuMusic!=bMenu)
    {
        bMenuMusic=bMenu;++MusicTransitions;
        UE_LOG(LogTemp,Display,TEXT("NEON_MUSIC: %s"),bMenu ? TEXT("Monume / menu") : TEXT("Humanstudioed / gameplay"));
    }
}

void ANeonAudioDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    UpdateMusicState();
    const double Now=FPlatformTime::Seconds();
    const float RealDt=FMath::Clamp(float(Now-LastMusicUpdate),0.f,0.1f);LastMusicUpdate=Now;
    MusicBlend=FMath::FInterpConstantTo(MusicBlend,bMenuMusic ? 1.f : 0.f,RealDt,1.f/0.65f);
    ApplyVolumes(false);
    const bool Duck=DangerWarning && DangerWarning->IsPlaying();if(Duck!=bDangerDucking) {bDangerDucking=Duck;ApplyVolumes(true);}
    UpdateHealthAlarm(Now);
    if(bSettingsDirty && Now>=SaveAfter) SaveAudioSettings();
}

bool ANeonAudioDirector::PlayEffect(FName Cue,EVoiceGroup Group,FVector Location,float Volume,float Pitch)
{
    USoundWave* Sound=Sounds.FindRef(Cue);
    if(!Sound || Location.ContainsNaN()) return false;
    if(Group!=EVoiceGroup::UI)
    {
        const auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
        const auto* Player=Game ? Game->GetArenaPlayer() : nullptr;
        if(!Player || !Game || Game->bInCodex || Game->bChoosingCard || Game->bRunEnded || UGameplayStatics::IsGamePaused(this)
            || FVector::DistSquared(Player->GetActorLocation(),Location)>FMath::Square(2600.f)) return false;
    }
    // Drop excess effects instead of clipping long tails or stealing UI voices.
    for(int32 I=0;I<Voices.Num();++I)
        if(Groups[I]==Group && !Voices[I]->IsPlaying())
        {
            UAudioComponent* Voice=Voices[I];Voice->SetWorldLocation(Location);Voice->SetSound(Sound);
            VoiceGains[I]=Volume;Voice->SetVolumeMultiplier(Volume*SFXVolume*(bDangerDucking && (Group==EVoiceGroup::Attack || Group==EVoiceGroup::Impact) ? 0.35f : 1.f));Voice->SetPitchMultiplier(Pitch);Voice->Play();return true;
        }
    return false;
}

void ANeonAudioDirector::PlayAttack(FName Cue,FVector Location)
{
    const double Now=FPlatformTime::Seconds();
    if(Now-LastAttack<0.055) return;
    const float Volume=Cue==TEXT("Laser") ? 0.30f : Cue==TEXT("Blaster") ? 0.5f : 0.19f;
    if(PlayEffect(Cue,EVoiceGroup::Attack,Location,Volume,FMath::FRandRange(0.96f,1.04f))) {LastAttack=Now;++AttackPlays;}
}
void ANeonAudioDirector::PlayImpact(FVector Location)
{
    const double Now=FPlatformTime::Seconds();
    if(Now-LastImpact<0.045) return;
    if(PlayEffect(TEXT("Impact"),EVoiceGroup::Impact,Location,0.24f,FMath::FRandRange(1.04f,1.15f))) {LastImpact=Now;++ImpactPlays;}
}
void ANeonAudioDirector::PlayEnemy(FVector Location,bool bDeath)
{
    const double Now=FPlatformTime::Seconds();
    if(Now-LastEnemy<(bDeath ? 0.22 : 1.2)) return;
    if(PlayEffect(bDeath ? TEXT("Enemy_Metal") : TEXT("Enemy_Glitch"),EVoiceGroup::Enemy,Location,bDeath ? 0.26f : 0.22f,FMath::FRandRange(0.88f,1.04f))) {LastEnemy=Now;++EnemyPlays;}
}
void ANeonAudioDirector::PlayUI(FName Cue)
{
    if(Cue==TEXT("UI_Select") || Cue==TEXT("UI_Click")) {PlayMenuFeedback(Cue==TEXT("UI_Click"),false);return;}
    const double Now=FPlatformTime::Seconds();
    if(Now-LastUI<0.05) return;
    if(PlayEffect(Cue,EVoiceGroup::UI,FVector::ZeroVector,0.7f)) {LastUI=Now;++UIPlays;}
}
void ANeonAudioDirector::PlayMenuFeedback(bool bClosing,bool bNavigation)
{
    if(bMenuQuitPending && !bClosing) return;
    const double Now=FPlatformTime::Seconds();const FName Cue=bClosing ? TEXT("UI_Click") : TEXT("UI_Select");
    if(bNavigation && (Now-LastMenuNavigation<0.085 || Now-LastMenuAction<0.1)) return;
    if(!bClosing && !bNavigation && LastMenuCue==TEXT("UI_Click") && Now-LastMenuAction<0.06) return;
    if(!bNavigation && Cue==LastMenuCue && Now-LastMenuAction<0.025) return;
    USoundWave* Sound=Sounds.FindRef(Cue);if(!Sound || Voices.Num()!=18) return;
    // Existing UI slots are separate from the attack, impact and enemy budgets.
    const int32 Index=bNavigation ? 16 : 17;UAudioComponent* Voice=Voices[Index];
    Voice->Stop();Voice->SetSound(Sound);Voice->SetPitchMultiplier(1.f);
    VoiceGains[Index]=bNavigation ? 0.5f : 1.f;Voice->SetVolumeMultiplier(VoiceGains[Index]*SFXVolume);Voice->Play();++UIPlays;
    if(bNavigation) {LastMenuNavigation=Now;++MenuNavigationPlays;}
    else {LastMenuAction=Now;LastMenuCue=Cue;if(bClosing) ++MenuClosePlays;else ++MenuInputPlays;}
}
float ANeonAudioDirector::GetMenuCloseDuration() const
{const USoundWave* Sound=Sounds.FindRef(TEXT("UI_Click"));return Sound ? FMath::Clamp(Sound->Duration,0.08f,1.5f) : 0.f;}
void ANeonAudioDirector::PlayWarning(FName Cue,FVector Location)
{
    const double Now=FPlatformTime::Seconds();if(!DangerWarning || (Now-LastWarning<0.35 && Cue!="Boss")) return;
    DangerWarning->Stop();DangerWarning->SetWorldLocation(Location);DangerWarning->SetSound(Sounds.FindRef(TEXT("UI_Deny")));
    DangerWarning->SetVolumeMultiplier(SFXVolume*0.85f);DangerWarning->SetPitchMultiplier(Cue=="Boss" ? 0.7f : 1.25f);DangerWarning->Play();LastWarning=Now;
}
bool ANeonAudioDirector::IsReady() const
{
    const USoundWave* Game=Sounds.FindRef(TEXT("Music_Gameplay"));const USoundWave* Menu=Sounds.FindRef(TEXT("Music_Menu"));
    return Sounds.Num()==14 && CriticalAlarm && CriticalAlarm->Sound && Game && Menu && Game->bLooping && Menu->bLooping && Voices.Num()==18
        && GameplayMusic && MenuMusic && GameplayMusic->IsPlaying() && MenuMusic->IsPlaying();
}
int32 ANeonAudioDirector::GetPlayingEffectCount() const
{
    int32 Count=0;for(const UAudioComponent* Voice:Voices) if(Voice && Voice->IsPlaying()) ++Count;return Count;
}
void ANeonAudioDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if(bSettingsDirty) SaveAudioSettings();
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->ClearAudioListenerAttenuationOverride();
    for(UAudioComponent* Voice:Voices) if(Voice) Voice->Stop();
    if(GameplayMusic) GameplayMusic->Stop();if(MenuMusic) MenuMusic->Stop();
    if(CriticalAlarm) CriticalAlarm->Stop();
    if(DangerWarning) DangerWarning->Stop();
    Super::EndPlay(Reason);
}

FString ANeonAudioDirector::SettingsSlot() const
{
    return FParse::Param(FCommandLine::Get(),TEXT("MVPSmoke")) || FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark"))
        ? TEXT("ForbiddenAudio_v3_Automation") : TEXT("ForbiddenAudio_v3");
}
void ANeonAudioDirector::ApplyVolumes(bool bEffects)
{
    if(MenuMusic) MenuMusic->SetVolumeMultiplier(MusicVolume*0.38f*FMath::Sin(MusicBlend*HALF_PI));
    if(GameplayMusic) GameplayMusic->SetVolumeMultiplier(MusicVolume*0.18f*FMath::Cos(MusicBlend*HALF_PI));
    if(bEffects) for(int32 I=0;I<Voices.Num();++I) Voices[I]->SetVolumeMultiplier(VoiceGains[I]*SFXVolume*(bDangerDucking && (Groups[I]==EVoiceGroup::Attack || Groups[I]==EVoiceGroup::Impact) ? 0.35f : 1.f));
    if(DangerWarning) DangerWarning->SetVolumeMultiplier(SFXVolume*0.85f);
    if(bEffects && CriticalAlarm) CriticalAlarm->SetVolumeMultiplier(0.65f*SFXVolume*HealthAlarmGain);
}

void ANeonAudioDirector::UpdateHealthAlarm(double Now)
{
    const float RealDt=FMath::Max(0.f,float(Now-LastHealthAlarmUpdate));LastHealthAlarmUpdate=Now;
    const auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    const auto* Player=Game ? Game->GetArenaPlayer() : nullptr;
    const bool bLow=Game && Player && !Game->bInCodex && !Game->bRunEnded && Player->Health>0.f && Player->Health<30.f;
    if(!bLow)
    {
        if(CriticalAlarm->IsPlaying()) CriticalAlarm->Stop();
        bCriticalHealth=false;bHealthAlarmStarted=false;HealthAlarmAge=0;HealthAlarmGain=0;return;
    }
    bCriticalHealth=true;
    const bool bPaused=UGameplayStatics::IsGamePaused(this);
    CriticalAlarm->SetPaused(bPaused);
    if(bPaused) return;
    if(!bHealthAlarmStarted)
    {
        bHealthAlarmStarted=true;HealthAlarmAge=0;HealthAlarmGain=1;
        CriticalAlarm->SetVolumeMultiplier(0.65f*SFXVolume);CriticalAlarm->Play();++LowHealthAlarmPlays;
    }
    else HealthAlarmAge+=RealDt;
    // One audible warning per low-health episode: five seconds, then a gentle 0.75s fade.
    // Keep bCriticalHealth latched for the HUD until health actually recovers.
    HealthAlarmGain=1.f-FMath::Clamp((HealthAlarmAge-5.f)/0.75f,0.f,1.f);
    CriticalAlarm->SetVolumeMultiplier(0.65f*SFXVolume*HealthAlarmGain);
    if(HealthAlarmAge>=5.75f && CriticalAlarm->IsPlaying()) CriticalAlarm->Stop();
}
bool ANeonAudioDirector::IsHealthAlarmPlaying() const {return CriticalAlarm && CriticalAlarm->IsPlaying();}
void ANeonAudioDirector::SetMusicVolume(float Value)
{
    if(!FMath::IsFinite(Value)) return;
    MusicVolume=FMath::Clamp(Value,0.f,1.f);ApplyVolumes();
    bSettingsDirty=true;SaveAfter=FPlatformTime::Seconds()+0.35;
}
void ANeonAudioDirector::SetSFXVolume(float Value)
{
    if(!FMath::IsFinite(Value)) return;
    SFXVolume=FMath::Clamp(Value,0.f,1.f);ApplyVolumes();
    bSettingsDirty=true;SaveAfter=FPlatformTime::Seconds()+0.35;
}
bool ANeonAudioDirector::SaveAudioSettings()
{
    if(!bSettingsDirty) return true;
    if(FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark"))) {bSettingsDirty=false;return true;}
    auto* Save=Cast<UCodexAudioSettingsSave>(UGameplayStatics::CreateSaveGameObject(UCodexAudioSettingsSave::StaticClass()));
    Save->MusicVolume=MusicVolume;Save->SFXVolume=SFXVolume;
    const bool bSaved=UGameplayStatics::SaveGameToSlot(Save,SettingsSlot(),0);
    if(bSaved) bSettingsDirty=false;
    else {SaveAfter=FPlatformTime::Seconds()+5;UE_LOG(LogTemp,Warning,TEXT("Could not save audio settings"));}
    return bSaved;
}
void ANeonAudioDirector::LoadAudioSettings()
{
    MusicVolume=SFXVolume=1.f;
    const FString LoadSlot=UGameplayStatics::DoesSaveGameExist(SettingsSlot(),0) || SettingsSlot().EndsWith("Automation") ? SettingsSlot() : TEXT("CodexAudio");
    if(!FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark")) && UGameplayStatics::DoesSaveGameExist(LoadSlot,0))
        if(const auto* Save=Cast<UCodexAudioSettingsSave>(UGameplayStatics::LoadGameFromSlot(LoadSlot,0)); Save && Save->Version==1)
        {
            MusicVolume=FMath::IsFinite(Save->MusicVolume) ? FMath::Clamp(Save->MusicVolume,0.f,1.f) : 1.f;
            SFXVolume=FMath::IsFinite(Save->SFXVolume) ? FMath::Clamp(Save->SFXVolume,0.f,1.f) : 1.f;
        }
    bSettingsDirty=false;ApplyVolumes();
}
