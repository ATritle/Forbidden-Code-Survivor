#pragma once
#include "CoreMinimal.h"
class UCardDataAsset;

/** Read the same base tuning used by practice weapons; no legacy school-name guesses. */
namespace TestingCardInfo
{
    FString Describe(const UCardDataAsset* Card);
    float ChainReach(FName CardID);
    FString SpacingWarning(const TArray<FName>& Cards,float Spacing);
}
