#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CodexPreferencesSave.generated.h"
UCLASS()
class DECKBUILDER_API UCodexPreferencesSave : public USaveGame
{
    GENERATED_BODY()
public:
    // SaveGame omits properties equal to the class default. Keep the original
    // defaults so old files without a Version/distance field still deserialize
    // as v1. The writer explicitly sets Version=2 for every new preference.
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) float CameraDistance = 1300.f;
};
