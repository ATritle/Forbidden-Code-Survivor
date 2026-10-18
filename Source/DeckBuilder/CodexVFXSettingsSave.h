#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CodexVFXSettingsSave.generated.h"

UCLASS()
class DECKBUILDER_API UCodexVFXSettingsSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version=1;
    UPROPERTY(SaveGame) float ImpactIntensity=1.f;
};
