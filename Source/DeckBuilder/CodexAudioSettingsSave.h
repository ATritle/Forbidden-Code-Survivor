#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CodexAudioSettingsSave.generated.h"

/** Separate slot: audio changes cannot overwrite deck or camera preferences. */
UCLASS()
class DECKBUILDER_API UCodexAudioSettingsSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version=1;
    UPROPERTY(SaveGame) float MusicVolume=1.f;
    UPROPERTY(SaveGame) float SFXVolume=1.f;
};
