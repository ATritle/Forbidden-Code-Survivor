#pragma once
#include "CoreMinimal.h"
class UCardDataAsset;
class UUserWidget;
class UWidget;

namespace ForbiddenCardInfo
{
    /** Complete authored rules plus base targeting/chain/area stats, without truncation. */
    FString Describe(const UCardDataAsset* Card);
    UWidget* MakeTooltip(UUserWidget* Owner,const FString& Copy);
}
