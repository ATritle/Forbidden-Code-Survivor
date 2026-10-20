#pragma once
#include "CoreMinimal.h"

/** Stable IDs are saved; descriptions and art may evolve without migrating profiles. */
namespace ForbiddenRiders
{
    struct FDefinition
    {
        FName ID;
        FString Name,Role,Description,PerkA,PerkB,Tradeoff;
        FLinearColor Color;
        float Height=170;
    };
    const TArray<FDefinition>& All();
    const FDefinition* Find(FName ID);
    FName Selected(const UObject* Context);
    bool Select(const UObject* Context,FName ID);
}
