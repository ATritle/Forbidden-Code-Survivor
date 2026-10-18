#include "MVPGame.h"
#include "CodexPreferencesSave.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

FString AMVPPlayer::CameraSaveSlot() const
{
    return FParse::Param(FCommandLine::Get(), TEXT("MVPSmoke")) ? TEXT("CodexPreferences_Automation") : TEXT("CodexPreferences");
}
void AMVPPlayer::LoadCameraPreference()
{
    PreferredCameraDistance = 2100.f;
    bool bMigrate=false;
    if (!FParse::Param(FCommandLine::Get(), TEXT("ExplosionBenchmark")) && UGameplayStatics::DoesSaveGameExist(CameraSaveSlot(), 0))
        if (const auto* Save = Cast<UCodexPreferencesSave>(UGameplayStatics::LoadGameFromSlot(CameraSaveSlot(), 0));
            Save && (Save->Version == 1 || Save->Version == 2) && FMath::IsFinite(Save->CameraDistance))
        {
            bMigrate=Save->Version==1;
            // Widen existing close saves once; subsequent mouse-wheel choices
            // keep their exact distance across launches.
            PreferredCameraDistance=FMath::Clamp(bMigrate ? FMath::Max(2100.f,Save->CameraDistance+650.f) : Save->CameraDistance,850.f,3600.f);
        }
    CameraBoom->TargetArmLength = PreferredCameraDistance;
    bZoomDirty = false;
    if(bMigrate) {bZoomDirty=true;ZoomSaveDelay=0.5f;}
}
bool AMVPPlayer::SaveCameraPreference()
{
    if (FParse::Param(FCommandLine::Get(), TEXT("ExplosionBenchmark"))) return false;
    auto* Save = Cast<UCodexPreferencesSave>(UGameplayStatics::CreateSaveGameObject(UCodexPreferencesSave::StaticClass()));
    Save->Version = 2;
    Save->CameraDistance = FMath::Clamp(PreferredCameraDistance, 850.f, 3600.f);
    const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, CameraSaveSlot(), 0);
    if (bSaved) bZoomDirty = false;
    else UE_LOG(LogTemp, Warning, TEXT("Could not save camera preference"));
    return bSaved;
}
void AMVPPlayer::ZoomCamera(float Value)
{
    const auto* Game = Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Game || Game->bInCodex || Game->bChoosingCard || Game->bRunEnded || !FMath::IsFinite(Value) || FMath::IsNearlyZero(Value)) return;
    const float Distance = FMath::Clamp(PreferredCameraDistance - Value * 100.f, 850.f, 3600.f);
    if (Distance == PreferredCameraDistance) return;
    PreferredCameraDistance = Distance;
    bZoomDirty = true;
    ZoomSaveDelay = 0.5f;
}
void AMVPPlayer::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bZoomDirty) SaveCameraPreference();
    Super::EndPlay(Reason);
}
