#pragma once
#include "CoreMinimal.h"
class UCardDataAsset;
enum class ECardRarity : uint8;
namespace ForbiddenPrograms
{
    /** Rarity and starter prices share the same catalog used to configure runtime cards. */
    DECKBUILDER_API ECardRarity Rarity(FName ID);
    DECKBUILDER_API FString RarityName(FName ID);
    DECKBUILDER_API int32 CostForRarity(ECardRarity Tier);
    DECKBUILDER_API int32 UnlockCost(FName ID);
    DECKBUILDER_API void Configure(UCardDataAsset* Card);
}
