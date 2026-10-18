#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CodexLoadoutSave.generated.h"

/** Stable IDs only: no runtime assets, actor pointers, or run upgrades. */
UCLASS()
class DECKBUILDER_API UCodexLoadoutSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame)
    int32 Version = 1;
    UPROPERTY(SaveGame)
    TArray<FName> StartingCards;
};
