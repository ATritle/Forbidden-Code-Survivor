#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace ForbiddenWorldQueries
{
    /** Find a nearby flat, connected duel space while keeping the approach inside the ring. */
    bool FindDuelSite(UWorld* World,FVector Approach,FVector& Center);
}
