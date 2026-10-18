#pragma once
#include "CoreMinimal.h"
struct FCyberEnemyProfile
{
    const TCHAR* ID;
    const TCHAR* Name;
    int32 Tier;
    float Health,Height,Speed,UnlockSeconds,ContactDamage,CollisionRadius;
    int32 Essence;
};
namespace CyberEnemies
{
    DECKBUILDER_API const TArray<FCyberEnemyProfile>& Roster();
    /** Bosses are admitted separately once per session; only unlocked units enter the weighted mix. */
    DECKBUILDER_API int32 Select(float Seconds,float Roll);
}
